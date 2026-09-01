#include <tempest/algorithm.hpp>
#include <tempest/editor.hpp>

#include <tempest/editor_engine_context.hpp>
#include <tempest/functional.hpp>
#include <tempest/memory.hpp>
#include <tempest/menus/menu_item.hpp>
#include <tempest/move.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/string.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/windows/engine_component_view_providers.hpp>
#include <tempest/windows/scene_hierarchy_window.hpp>

#include <cmath>
#include <format>
#include <imgui.h>
#include <imgui_internal.h>

namespace tempest::editor
{
    namespace
    {
        class exit_menu_item final : public menu_item
        {
          public:
            exit_menu_item(engine_context& ctx) : menu_item("File", "Exit"), _ctx{&ctx}
            {
            }

            auto on_press() noexcept -> void override
            {
                _ctx->request_close(true);
            }

          private:
            engine_context* _ctx;
        };

        class create_empty_entity_menu final : public menu_item
        {
          public:
            create_empty_entity_menu(engine_context& ctx, scene_hierarchy_window& scene_hierarchy)
                : menu_item("Entities", "Create Empty"), _ctx{&ctx}, _hierarchy{&scene_hierarchy}
            {
            }

            auto on_press() noexcept -> void override
            {
                auto& entities = _ctx->get_entities();
                const auto created = entities.create();
                _hierarchy->selected_entity = created;
            }

          private:
            engine_context* _ctx;
            scene_hierarchy_window* _hierarchy;
        };

        class web_profiler_menu_item final : public menu_item
        {
          public:
            web_profiler_menu_item(editor_engine_context& ctx) : menu_item("Tools", "Web Profiler"), _ctx{&ctx}
            {
            }

            auto get_shortcut() const noexcept -> string_view override
            {
                return "Ctrl+Alt+P";
            }

            auto on_press() noexcept -> void override
            {
                _ctx->open_profiler_in_browser();
            }

          private:
            editor_engine_context* _ctx;
        };

        class live_stream_menu_item final : public menu_item
        {
          public:
            live_stream_menu_item(editor_engine_context& ctx)
                : menu_item("Tools", "Enable Live Telemetry Stream"), _ctx{&ctx}
            {
            }

            auto on_press() noexcept -> void override
            {
                _ctx->set_live_stream_enabled(!_ctx->is_live_stream_enabled());
            }

          private:
            editor_engine_context* _ctx;
        };

        class gpu_stats_menu_item final : public menu_item
        {
          public:
            gpu_stats_menu_item(editor_engine_context& ctx)
                : menu_item("Tools", "Capture GPU Pipeline Statistics"), _ctx{&ctx}
            {
            }

            auto on_press() noexcept -> void override
            {
                _ctx->set_gpu_stats_enabled(!_ctx->is_gpu_stats_enabled());
            }

          private:
            editor_engine_context* _ctx;
        };

        class open_captures_folder_menu_item final : public menu_item
        {
          public:
            open_captures_folder_menu_item(editor_engine_context& ctx)
                : menu_item("Tools", "Open Captures Folder..."), _ctx{&ctx}
            {
            }

            auto on_press() noexcept -> void override
            {
                _ctx->open_captures_folder();
            }

          private:
            editor_engine_context* _ctx;
        };
    } // namespace

    editor_context::menu_hierarchy::menu_node::~menu_node() = default;

    void editor_context::menu_hierarchy::draw()
    {
        auto draw_node = [](menu_node& node, auto&& draw) -> void {
            const auto& title = node.name;
            const auto enabled = node.menu ? node.menu->validate() : true;

            if (node.menu)
            {
                const auto shortcut = node.menu->get_shortcut();
                const auto shortcut_str = shortcut.empty() ? nullptr : string(shortcut).c_str();
                const auto pressed = ImGui::MenuItem(title.c_str(), shortcut_str, false, enabled);
                if (pressed)
                {
                    node.menu->on_press();
                }
            }
            else
            {
                if (ImGui::BeginMenu(title.c_str(), enabled))
                {
                    for (auto& child : node.children)
                    {
                        draw(*child, draw);
                    }

                    ImGui::EndMenu();
                }
            }
        };

        for (auto& menu : _root_nodes)
        {
            draw_node(*menu, draw_node);
        }
    }

    void editor_context::menu_hierarchy::add_menu_item(unique_ptr<menu_item> menu)
    {
        auto* menus_to_search = &_root_nodes;
        auto path_begin_it = menu->get_menu_path().begin();

        for (const auto& path_elem : menu->get_menu_path())
        {
            // Find the matching element
            auto found = false;
            for (auto& elem : *menus_to_search)
            {
                if (elem->name == path_elem)
                {
                    found = true;
                    menus_to_search = &elem->children;
                    break;
                }
            }

            if (!found)
            {
                break;
            }

            path_begin_it++;
        }

        // Insert the elements
        for (auto it = path_begin_it; it != menu->get_menu_path().end(); ++it)
        {
            auto node = make_unique<menu_node>();
            node->name = string(*it);

            menus_to_search->push_back(tempest::move(node));
            menus_to_search = &menus_to_search->back()->children;
        }

        auto menu_elem = make_unique<menu_node>();
        menu_elem->name = string(menu->get_menu_item_name());
        menu_elem->menu = tempest::move(menu);
        menus_to_search->push_back(tempest::move(menu_elem));
    }

    editor_context::~editor_context()
    {
        if (_engine_ctx != nullptr)
        {
            _engine_ctx->clear_editor_callbacks();
        }
    }

    editor_context::editor_context(editor_engine_context& ctx, window_handle win, ui_context& ui_ctx)
        : _engine_ctx{&ctx}, _win{win}, _ui_ctx{&ui_ctx}
    {
        ctx.set_ui_context(&ui_ctx);

        _entity_view = register_window(make_unique<entity_view_window>(ctx.get_entities()));
        _scene_hierarchy_view = register_window(make_unique<scene_hierarchy_window>(ctx.get_entities()));
        _viewport_view = register_window(make_unique<viewport_window>(ctx));

        register_engine_component_view_providers(*this);

        register_on_paint_callback([&, this](engine_context& engine_ctx) {
            _entity_view->target = _scene_hierarchy_view->selected_entity;

            // Sync aspect ratio to editor camera
            const auto current_aspect = _viewport_view->aspect_ratio();
            _engine_ctx->get_editor_camera().set_aspect_ratio(current_aspect);

            // If a game camera exists, sync its aspect ratio too (only on delta to prevent event spam)
            auto& cam_sys = engine_ctx.get_renderer().get_camera_system();
            auto active_cam_opt = cam_sys.get_active_camera_entity();
            if (active_cam_opt.has_value())
            {
                const auto active_ent = active_cam_opt.value();
                if (const auto* cam = engine_ctx.get_entities().try_get<render_system::camera_component>(active_ent))
                {
                    if (std::abs(cam->aspect_ratio - current_aspect) > 1e-4F)
                    {
                        auto camera_copy = *cam;
                        camera_copy.aspect_ratio = current_aspect;
                        engine_ctx.get_entities().assign_or_replace(active_ent, camera_copy);
                    }
                }
            }

            _ui_ctx->begin_ui_commands();
            draw();
            _ui_ctx->finish_ui_commands();
        });

        register_menu_item(make_unique<exit_menu_item>(ctx));
        register_menu_item(make_unique<create_empty_entity_menu>(ctx, *_scene_hierarchy_view));
        register_menu_item(make_unique<web_profiler_menu_item>(ctx));
        register_menu_item(make_unique<live_stream_menu_item>(ctx));
        register_menu_item(make_unique<gpu_stats_menu_item>(ctx));
        register_menu_item(make_unique<open_captures_folder_menu_item>(ctx));
    }

    auto editor_context::_process_shortcuts() -> void
    {
        if (!_engine_ctx)
        {
            return;
        }

        const auto ctrl = ImGui::IsKeyDown(ImGuiMod_Ctrl);
        const auto alt = ImGui::IsKeyDown(ImGuiMod_Alt);
        const auto shift = ImGui::IsKeyDown(ImGuiMod_Shift);

        // Ctrl + Alt + P: Open Web Profiler in default browser
        if (ctrl && alt && !shift && ImGui::IsKeyPressed(ImGuiKey_P, false))
        {
            _engine_ctx->open_profiler_in_browser();
        }

        // Ctrl + Shift + R or F8: Toggle Capture Recording
        if ((ctrl && shift && !alt && ImGui::IsKeyPressed(ImGuiKey_R, false)) ||
            ImGui::IsKeyPressed(ImGuiKey_F8, false))
        {
            _engine_ctx->toggle_recording();
        }

        // Ctrl + Shift + M or F9: Insert Debug Marker
        if ((ctrl && shift && !alt && ImGui::IsKeyPressed(ImGuiKey_M, false)) ||
            ImGui::IsKeyPressed(ImGuiKey_F9, false))
        {
            _engine_ctx->insert_bookmark_marker();
        }
    }

    auto editor_context::_draw_status_bar() -> void
    {
        if (!_engine_ctx)
        {
            return;
        }

        const auto viewport = ImGui::GetMainViewport();
        const auto height = ImGui::GetFrameHeight() * 1.35f;

        if (ImGui::BeginViewportSideBar("##EditorStatusBar", viewport, ImGuiDir_Down, height,
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
        {
            if (ImGui::BeginMenuBar())
            {
                ImGui::TextUnformatted("Status: Ready");

                _draw_profiler_status_pill();

                ImGui::EndMenuBar();
            }
            ImGui::End();
        }
    }

    auto editor_context::_draw_profiler_status_pill() -> void
    {
        const auto* server = _engine_ctx->get_web_server();
        const auto port = server ? server->get_bound_port() : uint16_t{0};
        const auto clients = server ? server->connected_client_count() : size_t{0};
        const auto is_rec = _engine_ctx->is_recording();

        const auto fps = ImGui::GetIO().Framerate > 0.0f ? ImGui::GetIO().Framerate : 60.0f;
        const auto frame_ms = fps > 0.0f ? (1000.0f / fps) : 16.67f;
        const auto cpu_ms = _engine_ctx->get_last_cpu_time_ms();
        const auto gpu_ms = _engine_ctx->get_last_gpu_time_ms();

        const auto pill_text = std::format(" {:.1f} FPS | {:.1f}ms (CPU {:.1f}ms / GPU {:.1f}ms) | :{} ({}) ]", fps,
                                           frame_ms, cpu_ms, gpu_ms, port, clients);

        auto dot_color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); // Gray (idle, 0 clients)
        auto dot_symbol = "○";

        if (is_rec)
        {
            const auto alpha = 0.4f + 0.6f * static_cast<float>(std::sin(ImGui::GetTime() * 6.0));
            dot_color = ImVec4(1.0f, 0.2f, 0.2f, alpha); // Pulsing Red
            dot_symbol = "●";
        }
        else if (clients > 0)
        {
            dot_color = ImVec4(0.2f, 0.9f, 0.2f, 1.0f); // Green
            dot_symbol = "●";
        }

        const auto bracket_left_size = ImGui::CalcTextSize("[ ");
        const auto dot_size = ImGui::CalcTextSize(dot_symbol);
        const auto text_size = ImGui::CalcTextSize(pill_text.c_str());
        const auto total_width = bracket_left_size.x + dot_size.x + text_size.x + 16.0f;
        const auto avail_width = ImGui::GetContentRegionAvail().x;

        if (avail_width > total_width)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail_width - total_width);
        }

        const auto pos = ImGui::GetCursorScreenPos();
        const auto size = ImVec2(total_width, ImGui::GetFrameHeight());

        auto* draw_list = ImGui::GetWindowDrawList();
        const auto bg_color = ImGui::GetColorU32(ImGuiCol_FrameBg);
        const auto border_color = ImGui::GetColorU32(ImGuiCol_Border);
        draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg_color, 4.0f);
        draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), border_color, 4.0f);

        ImGui::InvisibleButton("##ProfilerStatusPill", size);

        const auto is_hovered = ImGui::IsItemHovered();
        const auto is_left_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        if (is_hovered)
        {
            draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::GetColorU32(ImGuiCol_ButtonHovered),
                               4.0f);
        }

        const auto text_pos_y = pos.y + (size.y - dot_size.y) * 0.5f;
        auto cur_x = pos.x + 8.0f;
        draw_list->AddText(ImVec2(cur_x, text_pos_y), ImGui::GetColorU32(ImGuiCol_Text), "[ ");
        cur_x += bracket_left_size.x;
        draw_list->AddText(ImVec2(cur_x, text_pos_y), ImGui::GetColorU32(dot_color), dot_symbol);
        cur_x += dot_size.x;
        draw_list->AddText(ImVec2(cur_x, text_pos_y), ImGui::GetColorU32(ImGuiCol_Text), pill_text.c_str());

        if (is_left_clicked)
        {
            _engine_ctx->open_profiler_in_browser();
        }

        if (ImGui::BeginPopupContextItem("##ProfilerStatusContextMenu", ImGuiPopupFlags_MouseButtonRight))
        {
            if (ImGui::MenuItem("Open in Browser", "Ctrl+Alt+P"))
            {
                _engine_ctx->open_profiler_in_browser();
            }

            const auto server_url = server ? server->get_server_url() : string{};
            auto copy_label = std::format("Copy Profiler URL ({})", server_url.c_str());
            if (ImGui::MenuItem(copy_label.c_str(), nullptr, false, !server_url.empty()))
            {
                ImGui::SetClipboardText(server_url.c_str());
            }

            ImGui::Separator();

            const auto rec_text = is_rec ? "Stop Recording" : "Start Recording";
            if (ImGui::MenuItem(rec_text, "Ctrl+Shift+R"))
            {
                _engine_ctx->toggle_recording();
            }

            if (ImGui::MenuItem("Insert Bookmark Marker", "Ctrl+Shift+M"))
            {
                _engine_ctx->insert_bookmark_marker();
            }

            ImGui::Separator();

            auto gpu_stats = _engine_ctx->is_gpu_stats_enabled();
            if (ImGui::MenuItem("Capture GPU Pipeline Statistics", nullptr, &gpu_stats))
            {
                _engine_ctx->set_gpu_stats_enabled(gpu_stats);
            }

            ImGui::EndPopup();
        }

        if (is_hovered)
        {
            _draw_profiler_tooltip();
        }
    }

    auto editor_context::_draw_profiler_tooltip() -> void
    {
        const auto& frame = _engine_ctx->get_last_telemetry_frame();
        const auto* server = _engine_ctx->get_web_server();
        const auto clients = server ? server->connected_client_count() : size_t{0};

        const auto fps = ImGui::GetIO().Framerate > 0.0f ? ImGui::GetIO().Framerate : 60.0f;
        const auto frame_ms = fps > 0.0f ? (1000.0f / fps) : 16.67f;
        const auto cpu_ms = _engine_ctx->get_last_cpu_time_ms();
        const auto gpu_ms = _engine_ctx->get_last_gpu_time_ms();

        ImGui::BeginTooltip();

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Tempest Telemetry (Frame #%llu)",
                           static_cast<unsigned long long>(frame.frame_index));
        ImGui::Separator();

        ImGui::Text("Total Frame Time : %.2f ms (%.1f FPS)", frame_ms, fps);
        ImGui::Text("CPU Time         : %.2f ms", cpu_ms);
        ImGui::Text("GPU Time         : %.2f ms", gpu_ms);
        ImGui::Text("Active Clients   : %zu connected", clients);

        struct zone_summary
        {
            string name;
            double duration_ms;
        };
        auto cpu_zones = vector<zone_summary>{};
        for (const auto& track : frame.cpu_tracks)
        {
            for (const auto& z : track.zones)
            {
                if (z.end_ns >= z.start_ns)
                {
                    const auto dur = static_cast<double>(z.end_ns - z.start_ns) / 1000000.0;
                    cpu_zones.push_back({z.name, dur});
                }
            }
        }

        for (auto i = size_t{0}; i < cpu_zones.size(); ++i)
        {
            for (auto j = i + 1; j < cpu_zones.size(); ++j)
            {
                if (cpu_zones[j].duration_ms > cpu_zones[i].duration_ms)
                {
                    auto tmp = tempest::move(cpu_zones[i]);
                    cpu_zones[i] = tempest::move(cpu_zones[j]);
                    cpu_zones[j] = tempest::move(tmp);
                }
            }
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Top CPU Zones:");
        if (cpu_zones.empty())
        {
            ImGui::TextDisabled("  (No CPU zones recorded)");
        }
        else
        {
            const auto show_count = min(cpu_zones.size(), size_t{5});
            for (auto i = size_t{0}; i < show_count; ++i)
            {
                ImGui::Text("  • %-24s : %.2f ms", cpu_zones[i].name.c_str(), cpu_zones[i].duration_ms);
            }
        }

        auto gpu_zones = vector<zone_summary>{};
        for (const auto& track : frame.gpu_tracks)
        {
            for (const auto& z : track.zones)
            {
                if (z.end_ns >= z.start_ns)
                {
                    const auto dur = static_cast<double>(z.end_ns - z.start_ns) / 1000000.0;
                    gpu_zones.push_back({z.name, dur});
                }
            }
        }

        for (auto i = size_t{0}; i < gpu_zones.size(); ++i)
        {
            for (auto j = i + 1; j < gpu_zones.size(); ++j)
            {
                if (gpu_zones[j].duration_ms > gpu_zones[i].duration_ms)
                {
                    auto tmp = tempest::move(gpu_zones[i]);
                    gpu_zones[i] = tempest::move(gpu_zones[j]);
                    gpu_zones[j] = tempest::move(tmp);
                }
            }
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Top GPU Passes:");
        if (gpu_zones.empty())
        {
            ImGui::TextDisabled("  (No GPU passes recorded)");
        }
        else
        {
            const auto show_count = min(gpu_zones.size(), size_t{5});
            for (auto i = size_t{0}; i < show_count; ++i)
            {
                ImGui::Text("  • %-24s : %.2f ms", gpu_zones[i].name.c_str(), gpu_zones[i].duration_ms);
            }
        }

        ImGui::EndTooltip();
    }

    auto editor_context::draw() -> void
    {
        const auto sim_state = _engine_ctx->get_simulation_state();
        const auto is_play_mode = sim_state == simulation_state::play;

        _process_shortcuts();

        ImGui::BeginDisabled(is_play_mode);

        if (ImGui::BeginMainMenuBar())
        {
            _menus.draw();
            ImGui::EndMainMenuBar();
        }

        ImGui::EndDisabled();

        const auto dockspace_id = ImGui::GetID("Tempest Editor Dockspace");
        const auto viewport = ImGui::GetMainViewport();

        if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
        {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            auto dock_id_main = dockspace_id;

            auto dock_id_bottom = ImGuiID{};
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.2f, &dock_id_bottom, &dock_id_main);

            auto dock_id_left = ImGuiID{};
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.2f, &dock_id_left, &dock_id_main);

            auto dock_id_right = ImGuiID{};
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, &dock_id_right, &dock_id_main);

            for (const auto& window : _windows)
            {
                const auto desired_dock_location = window->desired_initial_dock();
                switch (desired_dock_location)
                {
                case editor_window::dock_location::center: {
                    ImGui::DockBuilderDockWindow(window->window_name().data(), dock_id_main);
                    break;
                }
                case editor_window::dock_location::left: {
                    ImGui::DockBuilderDockWindow(window->window_name().data(), dock_id_left);
                    break;
                }
                case editor_window::dock_location::right: {
                    ImGui::DockBuilderDockWindow(window->window_name().data(), dock_id_right);
                    break;
                }
                case editor_window::dock_location::bottom: {
                    ImGui::DockBuilderDockWindow(window->window_name().data(), dock_id_bottom);
                    break;
                }
                default:
                    break;
                }
            }

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

        for (auto& window : _windows)
        {
            const auto is_available = window->is_mode_supported(sim_state);
            ImGui::BeginDisabled(!is_available);
            window->draw();
            ImGui::EndDisabled();
        }

        _draw_status_bar();
    }

    auto editor_context::register_on_paint_callback(function<void(engine_context&)> callback) -> void
    {
        _engine_ctx->register_on_editor_paint_callback(tempest::move(callback));
    }

    auto editor_context::register_on_update_callback(function<void(engine_context&)> callback) -> void
    {
        _engine_ctx->register_on_editor_update_callback(tempest::move(callback));
    }

    auto editor_context::register_menu_item(unique_ptr<menu_item> menu) -> void
    {
        _menus.add_menu_item(tempest::move(menu));
    }
} // namespace tempest::editor