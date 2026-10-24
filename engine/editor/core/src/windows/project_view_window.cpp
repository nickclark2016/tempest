#include <tempest/windows/project_view_window.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/checked.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/entity_hierarchy.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/material.hpp>
#include <tempest/render_target_asset.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/texture.hpp>
#include <tempest/utility.hpp>
#include <tempest/vertex.hpp>

#include <imgui.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace tempest::editor
{
    namespace
    {
#if defined(_WIN32)
        auto open_file_in_shell(string_view file_path) -> void
        {
            if (file_path.empty())
            {
                return;
            }
            const auto path_length =
                MultiByteToWideChar(CP_UTF8, 0, file_path.data(), static_cast<int>(file_path.size()), nullptr, 0);
            if (path_length <= 0)
            {
                return;
            }
            auto wide_path = vector<wchar_t>(static_cast<size_t>(path_length + 1), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, file_path.data(), static_cast<int>(file_path.size()), wide_path.data(),
                                path_length);
            wide_path[path_length] = L'\0';

            auto* shell32 = LoadLibraryA("shell32.dll");
            if (shell32 != nullptr)
            {
                using PFN_ShellExecuteW = HINSTANCE(WINAPI*)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
                auto shell_execute_fn = reinterpret_cast<PFN_ShellExecuteW>(GetProcAddress(shell32, "ShellExecuteW"));
                if (shell_execute_fn != nullptr)
                {
                    shell_execute_fn(nullptr, L"open", wide_path.data(), nullptr, nullptr, SW_SHOWNORMAL);
                }
                FreeLibrary(shell32);
            }
        }
#else
        auto open_file_in_shell(string_view) -> void
        {
        }
#endif

        auto find_last_scope(string_view text) noexcept -> string_view
        {
            if (text.size() < 2)
            {
                return text;
            }
            for (auto index = text.size() - 2; index > 0; --index)
            {
                if (text[index] == ':' && text[index + 1] == ':')
                {
                    return string_view{text.data() + index + 2, text.size() - (index + 2)};
                }
            }
            if (text[0] == ':' && text[1] == ':')
            {
                return string_view{text.data() + 2, text.size() - 2};
            }
            return text;
        }

        auto find_category(string_view text) noexcept -> string_view
        {
            for (auto index = size_t{0}; index < text.size(); ++index)
            {
                if (text[index] == '/')
                {
                    return string_view{text.data(), index};
                }
            }
            return text;
        }

        auto find_extension(string_view text) noexcept -> string_view
        {
            if (text.empty())
            {
                return {};
            }
            for (auto index = text.size(); index > 0; --index)
            {
                if (text[index - 1] == '.')
                {
                    if (index < text.size())
                    {
                        return string_view{text.data() + index, text.size() - index};
                    }
                    return {};
                }
            }
            return {};
        }

        auto format_badge_from_type_name(string_view type_name) -> string
        {
            const auto short_name = find_last_scope(type_name);
            if (short_name.empty())
            {
                return "[Asset]";
            }

            auto result = string{"["};
            result.push_back(static_cast<char>(short_name[0] >= 'a' && short_name[0] <= 'z'
                                                   ? short_name[0] - ('a' - 'A')
                                                   : short_name[0]));
            if (short_name.size() > 1)
            {
                result.append(short_name.data() + 1, short_name.size() - 1);
            }
            result.push_back(']');
            return result;
        }
    } // namespace

    project_view_window::project_view_window(scene::scene_manager& in_scene_mgr,
                                             editor_selection& in_selection,
                                             assets::asset_database* asset_db)
        : _model{asset_db}, _scene_mgr{in_scene_mgr}, _selection{in_selection}
    {
    }

    project_view_window::project_view_window(project_tree_model model,
                                             scene::scene_manager& in_scene_mgr,
                                             editor_selection& in_selection)
        : _model{tempest::move(model)}, _scene_mgr{in_scene_mgr}, _selection{in_selection}
    {
    }

    auto project_view_window::set_search_query(string_view query) -> void
    {
        const auto copy_length = tempest::min(query.size(), search_buffer_capacity - 1);
        tempest::memcpy(search_buffer, query.data(), copy_length);
        search_buffer[copy_length] = '\0';
        _model.set_search_query(search_buffer);
    }

    auto project_view_window::clear_search_query() -> void
    {
        search_buffer[0] = '\0';
        _model.clear_search_query();
    }

    auto project_view_window::navigate_to_directory(string_view dir_path) -> void
    {
        current_directory = string{dir_path};

        auto* target_node = _model.find_node_by_path(dir_path);
        if (target_node != nullptr)
        {
            auto* current = target_node;
            auto any_expanded = false;
            while (current != nullptr)
            {
                if (!current->is_expanded)
                {
                    current->is_expanded = true;
                    any_expanded = true;
                }
                current = current->parent;
            }

            if (any_expanded)
            {
                _model.rebuild_visible_rows();
            }
        }
    }

    auto project_view_window::get_node_badge(const project_tree_row& row,
                                             const assets::asset_database* asset_db) -> string
    {
        if (row.kind == tree_node_kind::mount_root || row.kind == tree_node_kind::directory)
        {
            return "[Folder]";
        }

        if (row.kind == tree_node_kind::loose_file)
        {
            return "[Loose]";
        }

        // Procedurally resolve registered type if present
        if (row.type.hash() != 0)
        {
            if (asset_db != nullptr && asset_db->type_registry() != nullptr)
            {
                const auto registered_name = asset_db->type_registry()->name_of(row.type);
                if (registered_name.has_value())
                {
                    return format_badge_from_type_name(*registered_name);
                }
            }

            // Fast-path / fallback for core engine types
            if (row.type == assets::asset_type_id::of<core::mesh>())
            {
                return "[Mesh]";
            }
            if (row.type == assets::asset_type_id::of<core::material>())
            {
                return "[Material]";
            }
            if (row.type == assets::asset_type_id::of<core::texture>())
            {
                return "[Texture]";
            }
            if (row.type == assets::asset_type_id::of<assets::shader_asset>())
            {
                return "[Shader]";
            }
            if (row.type == assets::asset_type_id::of<assets::render_target_asset>())
            {
                return "[RenderTarget]";
            }
            if (row.type == assets::asset_type_id::of<assets::entity_hierarchy>())
            {
                return "[Hierarchy]";
            }
        }

        if (row.kind == tree_node_kind::sub_asset)
        {
            const auto key = (row.node != nullptr && !row.node->sub_asset_key.empty())
                                 ? string_view{row.node->sub_asset_key}
                                 : row.display_label;

            const auto category = find_category(key);

            if (category == "mesh")
            {
                return "[Mesh]";
            }
            if (category == "material")
            {
                return "[Material]";
            }
            if (category == "texture")
            {
                return "[Texture]";
            }
            if (category == "prefab")
            {
                return "[Prefab]";
            }
            if (category == "shader")
            {
                return "[Shader]";
            }

            return "[SubAsset]";
        }

        if (row.kind == tree_node_kind::imported_file)
        {
            const auto name = (row.node != nullptr && !row.node->name.empty())
                                  ? string_view{row.node->name}
                                  : row.display_label;

            const auto ext_view = find_extension(name);
            if (!ext_view.empty())
            {
                auto result = string{"["};
                for (auto char_index = size_t{0}; char_index < ext_view.size(); ++char_index)
                {
                    const auto ch = ext_view[char_index];
                    result.push_back(static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch));
                }
                result.push_back(']');
                return result;
            }

            return "[Asset]";
        }

        return "[Asset]";
    }

    auto project_view_window::split_breadcrumb_path(string_view path) -> vector<breadcrumb_item>
    {
        auto results = vector<breadcrumb_item>{};
        if (path.empty())
        {
            return results;
        }

        results.reserve(4);

        auto accumulated_path = string{};
        auto cursor = size_t{0};

        while (cursor < path.size())
        {
            while (cursor < path.size() && (path[cursor] == '/' || path[cursor] == '\\'))
            {
                ++cursor;
            }

            if (cursor >= path.size())
            {
                break;
            }

            const auto start_index = cursor;
            while (cursor < path.size() && path[cursor] != '/' && path[cursor] != '\\')
            {
                ++cursor;
            }

            const auto segment_len = cursor - start_index;
            const auto segment = string_view{path.data() + start_index, segment_len};

            if (!accumulated_path.empty())
            {
                accumulated_path.push_back('/');
            }
            accumulated_path.append(segment.data(), segment.size());

            auto item = breadcrumb_item{
                .name = string{segment},
                .path = accumulated_path,
            };
            results.push_back(tempest::move(item));
        }

        return results;
    }

    auto project_view_window::set_selected_path(string_view path) -> void
    {
        if (path.empty())
        {
            clear_selection();
            return;
        }

        _selected_path = string{path};
        const auto* node = _model.find_node_by_path(path);
        if (node != nullptr)
        {
            _selected_asset_id = node->asset_id;
            _selected_kind = node->kind;
            return;
        }

        for (const auto& root : _model.roots())
        {
            if (root && (root->name == path || root->path == path))
            {
                _selected_asset_id = root->asset_id;
                _selected_kind = root->kind;
                return;
            }
        }

        for (const auto& row : _model.visible_rows())
        {
            if (row.relative_path == path || (row.relative_path.empty() && row.display_label == path))
            {
                _selected_asset_id = row.asset_id;
                _selected_kind = row.kind;
                return;
            }
        }

        _selected_asset_id = {};
        _selected_kind = default_selected_kind;
    }

    auto project_view_window::clear_selection() noexcept -> void
    {
        _selected_path.clear();
        _selected_asset_id = {};
        _selected_kind = default_selected_kind;
    }

    auto project_view_window::activate_item(const project_tree_row& row) -> void
    {
        const auto item_path = row.relative_path.empty() ? row.display_label : row.relative_path;
        set_selected_path(item_path);

        if (row.kind == tree_node_kind::mount_root || row.kind == tree_node_kind::directory)
        {
            _model.set_expanded_by_path(item_path, !row.is_expanded);
            current_directory = string{item_path};
            return;
        }

        auto disk_path = string_view{};
        if (row.node != nullptr && !row.node->disk_path.empty())
        {
            disk_path = row.node->disk_path;
        }
        else
        {
            const auto* found_node = _model.find_node_by_path(row.relative_path);
            if (found_node != nullptr && !found_node->disk_path.empty())
            {
                disk_path = found_node->disk_path;
            }
            else
            {
                disk_path = item_path;
            }
        }

        const auto is_scene = tempest::ends_with(row.relative_path, ".tscene") ||
                              tempest::ends_with(row.display_label, ".tscene") ||
                              tempest::ends_with(disk_path, ".tscene");
        if (is_scene)
        {
            open_scene_requested(disk_path);
            return;
        }

        const auto is_prefab = tempest::ends_with(row.relative_path, ".tprefab") ||
                               tempest::ends_with(row.display_label, ".tprefab") ||
                               tempest::ends_with(disk_path, ".tprefab");

        const auto is_material = tempest::ends_with(row.relative_path, ".tmaterial") ||
                                 tempest::ends_with(row.display_label, ".tmaterial") ||
                                 tempest::ends_with(disk_path, ".tmaterial") ||
                                 row.type == assets::asset_type_id::of<core::material>();

        auto is_sub_prefab_or_material = false;
        if (row.kind == tree_node_kind::sub_asset)
        {
            const auto key = (row.node != nullptr && !row.node->sub_asset_key.empty())
                                 ? string_view{row.node->sub_asset_key}
                                 : row.display_label;
            const auto cat = find_category(key);
            if (cat == "prefab" || cat == "material")
            {
                is_sub_prefab_or_material = true;
            }
        }

        if (is_prefab || is_material || is_sub_prefab_or_material)
        {
            if (_on_inspect_asset)
            {
                _on_inspect_asset(row.asset_id, row.type, row.relative_path);
            }
            return;
        }

        if (_on_open_external_file)
        {
            _on_open_external_file(disk_path);
        }
        else
        {
            open_file_in_shell(disk_path);
        }
    }

    auto project_view_window::open_scene_requested(string_view disk_path) -> void
    {
        const auto loaded_scenes = _scene_mgr->get_loaded_scenes();
        auto found_dirty = false;
        auto dirty_handle = scene::scene_handle{};
        for (const auto& handle : loaded_scenes)
        {
            if (_scene_mgr->is_dirty(handle))
            {
                found_dirty = true;
                dirty_handle = handle;
                break;
            }
        }

        if (found_dirty)
        {
            _show_dirty_dialog = true;
            _pending_scene_path = string{disk_path};
            _dirty_scene_to_save = dirty_handle;
            const auto maybe_name = _scene_mgr->get_scene_name(dirty_handle);
            if (maybe_name.has_value() && !maybe_name->empty())
            {
                _dirty_scene_name = string{*maybe_name};
            }
            else
            {
                const auto maybe_path = _scene_mgr->get_scene_path(dirty_handle);
                if (maybe_path.has_value() && !maybe_path->empty())
                {
                    _dirty_scene_name = string{*maybe_path};
                }
                else
                {
                    _dirty_scene_name = "Untitled";
                }
            }
        }
        else
        {
            [[maybe_unused]] const auto load_res = _scene_mgr->load_sync(
                filesystem::path{disk_path}, scene::scene_load_options{.mode = scene::scene_load_mode::single});
        }
    }

    auto project_view_window::confirm_save_and_open() -> void
    {
        auto path_to_save = filesystem::path{};
        const auto maybe_path = _scene_mgr->get_scene_path(_dirty_scene_to_save);
        if (maybe_path.has_value() && !maybe_path->empty())
        {
            path_to_save = filesystem::path{*maybe_path};
        }
        else if (!_dirty_scene_name.empty())
        {
            path_to_save = filesystem::path{_dirty_scene_name};
        }
        else
        {
            path_to_save = filesystem::path{"untitled.tscene"};
        }

        if (!path_to_save.empty())
        {
            const auto save_res = _scene_mgr->save_sync(_dirty_scene_to_save, path_to_save);
            if (!save_res.has_value())
            {
                return;
            }
        }

        if (!_pending_scene_path.empty())
        {
            [[maybe_unused]] const auto load_res = _scene_mgr->load_sync(
                filesystem::path{_pending_scene_path}, scene::scene_load_options{.mode = scene::scene_load_mode::single});
        }

        _show_dirty_dialog = false;
        _pending_scene_path.clear();
        _dirty_scene_to_save = {};
        _dirty_scene_name.clear();
    }

    auto project_view_window::confirm_discard_and_open() -> void
    {
        if (!_pending_scene_path.empty())
        {
            [[maybe_unused]] const auto load_res = _scene_mgr->load_sync(
                filesystem::path{_pending_scene_path}, scene::scene_load_options{.mode = scene::scene_load_mode::single});
        }

        _show_dirty_dialog = false;
        _pending_scene_path.clear();
        _dirty_scene_to_save = {};
        _dirty_scene_name.clear();
    }

    auto project_view_window::cancel_open_scene() -> void
    {
        _show_dirty_dialog = false;
        _pending_scene_path.clear();
        _dirty_scene_to_save = {};
        _dirty_scene_name.clear();
    }

    auto project_view_window::draw() -> void
    {
        if (!open)
        {
            return;
        }

        if (_model.is_dirty())
        {
            _model.rebuild_visible_rows();
        }

        if (ImGui::Begin(window_name().data(), &open))
        {
            // Search text input and synchronization
            const auto text_changed = ImGui::InputTextWithHint("##ProjectSearch", "Search assets...", search_buffer,
                                                               search_buffer_capacity);
            if (text_changed || string_view{static_cast<const char*>(search_buffer)} != _model.search_query())
            {
                _model.set_search_query(search_buffer);
            }

            ImGui::SameLine();
            if (ImGui::Button("Clear"))
            {
                clear_search_query();
            }

            if (_model.is_search_active())
            {
                ImGui::SameLine();
                const auto match_count = _model.visible_row_count();
                ImGui::TextDisabled(match_count == 1 ? "1 match" : "%zu matches", match_count);
            }

            // Breadcrumb navigation strip
            auto effective_dir = string_view{current_directory};
            if (effective_dir.empty() && !_model.roots().empty() && _model.roots()[0] != nullptr)
            {
                effective_dir = _model.roots()[0]->name;
            }

            const auto breadcrumbs = split_breadcrumb_path(effective_dir);
            if (!breadcrumbs.empty())
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0F, 2.0F));
                for (auto segment_index = size_t{0}; segment_index < breadcrumbs.size(); ++segment_index)
                {
                    if (segment_index > 0)
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled(">");
                        ImGui::SameLine();
                    }

                    const auto& item = breadcrumbs[segment_index];
                    ImGui::PushID(static_cast<int>(segment_index));
                    if (ImGui::SmallButton(item.name.c_str()))
                    {
                        navigate_to_directory(item.path);
                    }
                    ImGui::PopID();
                }
                ImGui::PopStyleVar();
                ImGui::Separator();
            }

            // Virtualized tree rows region
            if (ImGui::BeginChild("##ProjectTreeScrollRegion"))
            {
                const auto rows = _model.visible_rows();
                auto clipper = ImGuiListClipper{};
                clipper.Begin(static_cast<int>(rows.size()));

                auto pending_toggle_path = string{};
                auto pending_toggle_open = false;
                auto has_pending_toggle = false;

                while (clipper.Step())
                {
                    for (auto row_index = clipper.DisplayStart; row_index < clipper.DisplayEnd; ++row_index)
                    {
                        if (row_index < 0 || static_cast<size_t>(row_index) >= rows.size())
                        {
                            continue;
                        }

                        const auto& row = rows[static_cast<size_t>(row_index)];
                        ImGui::PushID(row_index);

                        const auto indent_px = static_cast<float>(row.depth) * 16.0F;
                        if (indent_px > 0.0F)
                        {
                            ImGui::Indent(indent_px);
                        }

                        const auto badge = get_node_badge(row, _model.asset_database());

                        auto node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                                          ImGuiTreeNodeFlags_NoTreePushOnOpen;
                        if (!row.has_children)
                        {
                            node_flags |= ImGuiTreeNodeFlags_Leaf;
                        }

                        const auto row_path = row.relative_path.empty() ? row.display_label : row.relative_path;
                        const auto is_selected = (_selected_path == row_path);
                        if (is_selected)
                        {
                            node_flags |= ImGuiTreeNodeFlags_Selected;
                        }

                        ImGui::SetNextItemOpen(row.is_expanded);

                        const auto is_loose = (row.kind == tree_node_kind::loose_file);
                        if (is_loose)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6F, 0.6F, 0.6F, 1.0F));
                        }

                        auto node_open = false;
                        if (_model.is_search_active() && !row.relative_path.empty())
                        {
                            node_open = ImGui::TreeNodeEx("##row", node_flags, "%.*s %.*s (%.*s)",
                                                          static_cast<int>(badge.size()), badge.data(),
                                                          static_cast<int>(row.display_label.size()),
                                                          row.display_label.data(),
                                                          static_cast<int>(row.relative_path.size()),
                                                          row.relative_path.data());
                        }
                        else
                        {
                            node_open = ImGui::TreeNodeEx("##row", node_flags, "%.*s %.*s",
                                                          static_cast<int>(badge.size()), badge.data(),
                                                          static_cast<int>(row.display_label.size()),
                                                          row.display_label.data());
                        }

                        if (is_loose)
                        {
                            ImGui::PopStyleColor();
                        }

                        if (row.has_children && ImGui::IsItemToggledOpen())
                        {
                            has_pending_toggle = true;
                            pending_toggle_path = string{row_path};
                            pending_toggle_open = node_open;
                        }

                        if (ImGui::IsItemClicked())
                        {
                            set_selected_path(row_path);
                            if (row.kind == tree_node_kind::directory || row.kind == tree_node_kind::mount_root)
                            {
                                current_directory = string{row_path};
                            }
                        }

                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            activate_item(row);
                        }

                        if (indent_px > 0.0F)
                        {
                            ImGui::Unindent(indent_px);
                        }

                        ImGui::PopID();
                    }
                }
                clipper.End();

                if (has_pending_toggle)
                {
                    _model.set_expanded_by_path(pending_toggle_path, pending_toggle_open);
                }
            }
            ImGui::EndChild();

            if (_show_dirty_dialog)
            {
                ImGui::OpenPopup("Save Changes?");
            }

            if (ImGui::BeginPopupModal("Save Changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Save changes to '%s' before opening a new scene?",
                            _dirty_scene_name.empty() ? "Untitled" : _dirty_scene_name.c_str());
                ImGui::Separator();

                if (ImGui::Button("Save"))
                {
                    confirm_save_and_open();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Don't Save"))
                {
                    confirm_discard_and_open();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    cancel_open_scene();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
} // namespace tempest::editor
