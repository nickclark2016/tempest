#include <tempest/windows/project_launcher_dialog.hpp>

#include <tempest/filesystem.hpp>
#include <tempest/format.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/string_view.hpp>
#include <tempest/utility.hpp>

#include <imgui.h>

namespace tempest::editor
{
    namespace
    {
        auto copy_string_to_buffer(string_view source, span<char> destination) -> void
        {
            if (destination.empty())
            {
                return;
            }
            const auto copy_len = tempest::min(source.size(), destination.size() - 1);
            if (copy_len > 0)
            {
                tempest::memcpy(destination.data(), source.data(), copy_len);
            }
            destination[copy_len] = '\0';
        }
    } // namespace

    project_launcher_dialog::project_launcher_dialog()
    {
        copy_string_to_buffer("NewProject", span<char>{_new_project_name_buffer.data(), _new_project_name_buffer.size()});
        copy_string_to_buffer(filesystem::current_path().generic_string(),
                              span<char>{_new_project_dir_buffer.data(), _new_project_dir_buffer.size()});
    }

    auto project_launcher_dialog::open(launcher_tab initial_tab) noexcept -> void
    {
        _is_open = true;
        _request_open_popup = true;
        _current_tab = initial_tab;
        _requested_tab = initial_tab;
        _switch_tab_requested = true;
        _last_error.clear();
        refresh_recent_projects();
    }

    auto project_launcher_dialog::close() noexcept -> void
    {
        _is_open = false;
        _request_open_popup = false;
        _last_error.clear();
    }

    auto project_launcher_dialog::refresh_recent_projects() -> void
    {
        _recents = load_recent_projects(_session_file_path);
        if (_recents.empty())
        {
            _selected_recent_index = -1;
        }
        else if (_selected_recent_index < 0 || _selected_recent_index >= static_cast<int32_t>(_recents.size()))
        {
            _selected_recent_index = 0;
        }
    }

    auto project_launcher_dialog::set_open_path(string_view path) -> void
    {
        copy_string_to_buffer(path, span<char>{_open_path_buffer.data(), _open_path_buffer.size()});
    }

    auto project_launcher_dialog::set_new_project_fields(string_view project_name, string_view parent_directory) -> void
    {
        copy_string_to_buffer(project_name,
                              span<char>{_new_project_name_buffer.data(), _new_project_name_buffer.size()});
        copy_string_to_buffer(parent_directory,
                              span<char>{_new_project_dir_buffer.data(), _new_project_dir_buffer.size()});
    }

    auto project_launcher_dialog::draw() -> void
    {
        if (!_is_open)
        {
            return;
        }

        if (_request_open_popup)
        {
            ImGui::OpenPopup("Tempest Project Launcher##Modal");
            _request_open_popup = false;
        }

        auto* const viewport = ImGui::GetMainViewport();
        if (viewport != nullptr)
        {
            const auto center = viewport->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        }
        ImGui::SetNextWindowSize(ImVec2(660.0F, 440.0F), ImGuiCond_Appearing);

        const auto window_flags = ImGuiWindowFlags_NoCollapse;
        if (ImGui::BeginPopupModal("Tempest Project Launcher##Modal", &_is_open, window_flags))
        {
            if (ImGui::BeginTabBar("LauncherTabBar", ImGuiTabBarFlags_None))
            {
                auto recents_flags = ImGuiTabItemFlags{ImGuiTabItemFlags_None};
                auto open_flags = ImGuiTabItemFlags{ImGuiTabItemFlags_None};
                auto create_flags = ImGuiTabItemFlags{ImGuiTabItemFlags_None};

                if (_switch_tab_requested)
                {
                    if (_requested_tab == launcher_tab::recent_projects)
                    {
                        recents_flags |= ImGuiTabItemFlags_SetSelected;
                    }
                    else if (_requested_tab == launcher_tab::open_browse)
                    {
                        open_flags |= ImGuiTabItemFlags_SetSelected;
                    }
                    else if (_requested_tab == launcher_tab::create_project)
                    {
                        create_flags |= ImGuiTabItemFlags_SetSelected;
                    }
                    _switch_tab_requested = false;
                }

                if (ImGui::BeginTabItem("Recent Projects", nullptr, recents_flags))
                {
                    _current_tab = launcher_tab::recent_projects;
                    _draw_recents_tab();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Open / Browse", nullptr, open_flags))
                {
                    _current_tab = launcher_tab::open_browse;
                    _draw_open_browse_tab();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Create New Project", nullptr, create_flags))
                {
                    _current_tab = launcher_tab::create_project;
                    _draw_create_project_tab();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const auto has_project = _has_active_project ? _has_active_project() : false;

            if (ImGui::Button("Exit", ImVec2(100.0F, 0.0F)))
            {
                if (_on_exit)
                {
                    _on_exit();
                }
                close();
                ImGui::CloseCurrentPopup();
            }

            if (has_project)
            {
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(100.0F, 0.0F)))
                {
                    close();
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }
        else
        {
            _is_open = false;
        }
    }

    auto project_launcher_dialog::_draw_recents_tab() -> void
    {
        ImGui::Spacing();
        if (_recents.empty())
        {
            ImGui::TextDisabled("No recent projects found.");
            return;
        }

        const auto content_height = ImGui::GetContentRegionAvail().y - 65.0F;
        if (ImGui::BeginChild("RecentsListRegion", ImVec2(0.0F, content_height), true))
        {
            for (int32_t index = 0; index < static_cast<int32_t>(_recents.size()); ++index)
            {
                const auto& entry = _recents[static_cast<size_t>(index)];
                const auto is_selected = (_selected_recent_index == index);
                const auto file_exists = filesystem::exists(entry.path);

                if (!file_exists)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85F, 0.4F, 0.4F, 1.0F));
                }

                const auto item_label = file_exists
                                            ? tempest::format("{}##recent_{}", entry.name, index)
                                            : tempest::format("{} [Missing]##recent_{}", entry.name, index);

                if (ImGui::Selectable(item_label.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    _selected_recent_index = index;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        if (!file_exists)
                        {
                            _last_error = tempest::format("Project '{}' no longer exists at: {}", entry.name, entry.path);
                        }
                        else if (_on_open_project && _on_open_project(entry.path))
                        {
                            close();
                            ImGui::CloseCurrentPopup();
                        }
                        else
                        {
                            _last_error = tempest::format("Failed to open project: {}. Check log for details.", entry.path);
                        }
                    }
                }

                if (!file_exists)
                {
                    ImGui::PopStyleColor();
                }

                ImGui::SameLine();
                ImGui::TextDisabled(" - %s", entry.path.c_str());

                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(entry.path.c_str());
                    ImGui::EndTooltip();
                }
            }
            ImGui::EndChild();
        }

        if (!_last_error.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.4F, 1.0F), "%s", _last_error.c_str());
        }

        ImGui::Spacing();
        const auto has_valid_selection =
            _selected_recent_index >= 0 && _selected_recent_index < static_cast<int32_t>(_recents.size());
        const auto selected_file_exists =
            has_valid_selection && filesystem::exists(_recents[static_cast<size_t>(_selected_recent_index)].path);

        ImGui::BeginDisabled(!selected_file_exists);
        if (ImGui::Button("Open Selected", ImVec2(130.0F, 0.0F)))
        {
            if (has_valid_selection)
            {
                const auto& chosen = _recents[static_cast<size_t>(_selected_recent_index)];
                if (_on_open_project && _on_open_project(chosen.path))
                {
                    close();
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    _last_error = tempest::format("Failed to open project: {}. Check log for details.", chosen.path);
                }
            }
        }
        ImGui::EndDisabled();

        if (has_valid_selection && !selected_file_exists)
        {
            ImGui::SameLine();
            if (ImGui::Button("Remove from Recents", ImVec2(160.0F, 0.0F)))
            {
                remove_recent_project(_recents[static_cast<size_t>(_selected_recent_index)].path, _session_file_path);
                refresh_recent_projects();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Recents", ImVec2(120.0F, 0.0F)))
        {
            clear_recent_projects(_session_file_path);
            refresh_recent_projects();
        }
    }

    auto project_launcher_dialog::_draw_open_browse_tab() -> void
    {
        ImGui::Spacing();
        ImGui::TextUnformatted("Specify the absolute or relative path to a .tproject file:");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##OpenProjectPath", _open_path_buffer.data(), _open_path_buffer.size());

        const auto* path_ptr = _open_path_buffer.data();
        const auto path_view = string_view{path_ptr};
        const auto is_tproject = filesystem::path(path_view).extension() == ".tproject";
        const auto file_exists = !path_view.empty() && filesystem::exists(path_view) &&
                                 !filesystem::is_directory(filesystem::path(path_view));
        const auto is_valid_selection = is_tproject && file_exists;

        ImGui::Spacing();
        if (!path_view.empty())
        {
            if (!is_tproject)
            {
                ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.4F, 1.0F), "File path must end with .tproject extension.");
            }
            else if (!file_exists)
            {
                ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.4F, 1.0F), "Project file does not exist on disk.");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.3F, 0.9F, 0.3F, 1.0F), "Valid Tempest project file recognized.");
            }
        }
        else
        {
            ImGui::TextDisabled("Enter path above to enable opening.");
        }

        if (!_last_error.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.4F, 1.0F), "%s", _last_error.c_str());
        }

        ImGui::Spacing();
        ImGui::BeginDisabled(!is_valid_selection);
        if (ImGui::Button("Open Project", ImVec2(140.0F, 0.0F)))
        {
            _last_error.clear();
            if (!_on_open_project)
            {
                _last_error = "Project open handler is not configured.";
            }
            else if (_on_open_project(path_view))
            {
                close();
                ImGui::CloseCurrentPopup();
            }
            else
            {
                _last_error = tempest::format("Failed to open project: {}. Check log for details.", path_view);
            }
        }
        ImGui::EndDisabled();
    }

    auto project_launcher_dialog::_draw_create_project_tab() -> void
    {
        ImGui::Spacing();
        ImGui::TextUnformatted("Create a new Tempest project and directory structure:");
        ImGui::Spacing();

        ImGui::TextUnformatted("Project Name:");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##NewProjectName", _new_project_name_buffer.data(), _new_project_name_buffer.size());

        ImGui::Spacing();
        ImGui::TextUnformatted("Parent Directory:");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##NewProjectParentDir", _new_project_dir_buffer.data(), _new_project_dir_buffer.size());

        const auto* name_ptr = _new_project_name_buffer.data();
        const auto* dir_ptr = _new_project_dir_buffer.data();
        const auto name_view = string_view{name_ptr};
        const auto dir_view = string_view{dir_ptr};
        const auto is_valid_inputs = !name_view.empty() && !dir_view.empty();

        ImGui::Spacing();
        if (is_valid_inputs)
        {
            const auto parent_p = filesystem::path(dir_view);
            const auto target_dir =
                (parent_p.filename() == name_view) ? parent_p : (parent_p / name_view);
            const auto target_file = target_dir / tempest::format("{}.tproject", name_view);
            ImGui::TextDisabled("Target project file: %s", target_file.generic_string().c_str());
        }
        else
        {
            ImGui::TextDisabled("Enter both project name and parent directory to proceed.");
        }

        if (!_last_error.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.4F, 1.0F), "%s", _last_error.c_str());
        }

        ImGui::Spacing();
        ImGui::BeginDisabled(!is_valid_inputs);
        if (ImGui::Button("Create Project", ImVec2(140.0F, 0.0F)))
        {
            _last_error.clear();
            if (!_on_create_project)
            {
                _last_error = "Project creation handler is not configured.";
            }
            else if (_on_create_project(name_view, dir_view))
            {
                close();
                ImGui::CloseCurrentPopup();
            }
            else
            {
                _last_error = tempest::format("Failed to create project '{}' in '{}'. Check directory permissions and logs.",
                                              name_view, dir_view);
            }
        }
        ImGui::EndDisabled();
    }
} // namespace tempest::editor
