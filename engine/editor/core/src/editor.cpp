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
    } // namespace

    editor_context::menu_hierarchy::menu_node::~menu_node() = default;

    void editor_context::menu_hierarchy::draw()
    {
        auto draw_node = [](menu_node& node, auto&& draw) -> void {
            const auto& title = node.name;
            const auto enabled = node.menu ? node.menu->validate() : true;

            if (node.menu)
            {
                const auto pressed = ImGui::MenuItem(title.c_str(), nullptr, false, enabled);
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

    editor_context::editor_context(editor_engine_context& ctx, window_handle win, ui_context& ui_ctx)
        : _engine_ctx{&ctx}, _win{win}, _ui_ctx{&ui_ctx}
    {
        _entity_view = register_window(make_unique<entity_view_window>(ctx.get_entities()));
        _scene_hierarchy_view = register_window(make_unique<scene_hierarchy_window>(ctx.get_entities()));
        _viewport_view = register_window(make_unique<viewport_window>(ctx));

        register_engine_component_view_providers(*this);

        auto& camera_sys = ctx.get_renderer().get_camera_system();

        auto active_cam_entity = camera_sys.get_active_camera_entity();
        if (!active_cam_entity.has_value())
        {
            auto editor_cam = ctx.get_entities().create();
            ctx.get_entities().name(editor_cam, "Editor Camera");
            ctx.get_entities().assign(editor_cam, ecs::transform_component::identity());
            ctx.get_entities().assign(editor_cam, render_system::camera_component{
                .aspect_ratio = 16.0f / 9.0f,
                .vertical_fov = math::as_radians(60.0f),
                .near_plane = 0.1f,
            });
            camera_sys.set_active_camera(editor_cam);
        }

        register_on_paint_callback([&, this](engine_context& engine_ctx) {
            _entity_view->target = _scene_hierarchy_view->selected_entity;

            auto& cam_sys = engine_ctx.get_renderer().get_camera_system();
            auto active_cam_opt = cam_sys.get_active_camera_entity();
            if (active_cam_opt.has_value())
            {
                const auto active_ent = active_cam_opt.value();
                if (const auto* cam = engine_ctx.get_entities().try_get<render_system::camera_component>(active_ent))
                {
                    auto camera_copy = *cam;
                    camera_copy.aspect_ratio = _viewport_view->aspect_ratio();
                    engine_ctx.get_entities().assign_or_replace(active_ent, camera_copy);
                }
            }

            _ui_ctx->begin_ui_commands();
            draw();
            _ui_ctx->finish_ui_commands();
        });

        register_menu_item(make_unique<exit_menu_item>(ctx));
        register_menu_item(make_unique<create_empty_entity_menu>(ctx, *_scene_hierarchy_view));
    }

    auto editor_context::draw() -> void
    {
        const auto sim_state = _engine_ctx->get_simulation_state();
        const auto is_play_mode = sim_state == simulation_state::play;

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
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

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
    }

    auto editor_context::register_on_paint_callback(function<void(engine_context&)> callback) -> void
    {
        _engine_ctx->register_on_editor_paint_callback(tempest::move(callback));
    }

    auto editor_context::register_on_update_callback(function<void(engine_context&)> callback) -> void
    {
        _engine_ctx->register_on_editor_update_callback(tempest::move(callback));
    }
} // namespace tempest::editor