#include <tempest/editor.hpp>

#include <tempest/editor_engine_context.hpp>
#include <tempest/functional.hpp>
#include <tempest/memory.hpp>
#include <tempest/menus/menu_item.hpp>
#include <tempest/move.hpp>
#include <tempest/string.hpp>
#include <tempest/tempest.hpp>
#include <tempest/windows/engine_component_view_providers.hpp>

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

    editor_context::editor_context(editor_engine_context& ctx, rhi::window_surface& win_surface, ui_context& ui_ctx)
        : _engine_ctx{&ctx}, _win_surface{&win_surface}, _ui_ctx{&ui_ctx}
    {
        auto& frame_graph = _engine_ctx->get_renderer().get_frame_graph();
        auto frame_graph_builder = frame_graph.get_builder();

        auto color_target = frame_graph_builder->create_render_target({
            .format = rhi::image_format::rgba8_srgb,
            .type = rhi::image_type::image_2d,
            .width = _win_surface->framebuffer_width(),
            .height = _win_surface->framebuffer_height(),
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::transfer_src),
            .name = "Final Render Target",
        });

        _final_color_target = color_target;
        _win_surface->register_resize_callback(
            [&](auto width, auto height) { frame_graph.resize_render_target(_final_color_target, width, height); });

        auto surface = ctx.get_renderer().get_device().create_render_surface({
            .window = _win_surface,
            .min_image_count = 3,
            .format =
                {
                    .space = rhi::color_space::srgb_nonlinear,
                    .format = rhi::image_format::bgra8_srgb,
                },
            .present_mode = rhi::present_mode::immediate,
            .width = _win_surface->framebuffer_width(),
            .height = _win_surface->framebuffer_height(),
            .layers = 1,
        });

        auto imported_swapchain_handle = frame_graph_builder->import_render_surface("Editor Render Surface", surface);

        frame_graph_builder->create_graphics_pass(
            "Editor UI Pass",
            [&](graphics::graphics_task_builder& task_builder) {
                task_builder.read_write(color_target, rhi::image_layout::color_attachment,
                                        make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                                        make_enum_mask(rhi::memory_access::color_attachment_read,
                                                       rhi::memory_access::color_attachment_write),
                                        make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                                        make_enum_mask(rhi::memory_access::color_attachment_write,
                                                       rhi::memory_access::color_attachment_read));

                auto tonemapped_image_handle = frame_graph.get_tonemapped_color_handle();
                task_builder.read(
                    tonemapped_image_handle, rhi::image_layout::shader_read_only,
                    make_enum_mask(rhi::pipeline_stage::fragment_shader),
                    make_enum_mask(rhi::memory_access::shader_sampled_read, rhi::memory_access::shader_read));
            },
            [](graphics::graphics_task_execution_context& ctx, auto render_target, auto device, auto ui) {
                const auto rt_handle = ctx.find_image(render_target);
                const auto width = static_cast<uint32_t>(device->get_image_width(rt_handle));
                const auto height = static_cast<uint32_t>(device->get_image_height(rt_handle));

                auto rp_begin_info = rhi::work_queue::render_pass_info{};
                rp_begin_info.color_attachments.push_back(rhi::work_queue::color_attachment_info{
                    .image = rt_handle,
                    .layout = rhi::image_layout::color_attachment,
                    .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
                    .load_op = rhi::work_queue::load_op::clear,
                    .store_op = rhi::work_queue::store_op::store,
                });
                rp_begin_info.x = 0;
                rp_begin_info.y = 0;
                rp_begin_info.width = width;
                rp_begin_info.height = height;
                rp_begin_info.name = "UI Render Pass";

                ctx.begin_render_pass(rp_begin_info);
                ui->render_ui_commands(ctx);
                ctx.end_render_pass();
            },
            color_target, &_engine_ctx->get_renderer().get_device(), _ui_ctx);

        frame_graph_builder->create_transfer_pass(
            "Blit to Swapchain Pass",
            [&](graphics::transfer_task_builder& task_builder) {
                task_builder.read(color_target, rhi::image_layout::transfer_src,
                                  make_enum_mask(rhi::pipeline_stage::blit),
                                  make_enum_mask(rhi::memory_access::transfer_read));
                task_builder.write(imported_swapchain_handle, rhi::image_layout::transfer_dst,
                                   make_enum_mask(rhi::pipeline_stage::blit),
                                   make_enum_mask(rhi::memory_access::transfer_write));
            },
            [](graphics::transfer_task_execution_context& ctx, auto color_target, auto swapchain_handle) {
                ctx.blit(color_target, swapchain_handle);
            },
            color_target, imported_swapchain_handle);

        _entity_view = register_window(make_unique<entity_view_window>(ctx.get_entities()));
        _scene_hierarchy_view = register_window(make_unique<scene_hierarchy_window>(ctx.get_entities()));
        _viewport_view = register_window(make_unique<viewport_window>(ctx.get_renderer()));

        register_engine_component_view_providers(*this);

        register_on_paint_callback([&](engine_context& ctx) {
            _entity_view->target = _scene_hierarchy_view->selected_entity;

            for (auto&& [self, camera] : ctx.get_entities().with<ecs::self_component, graphics::camera_component>())
            {
                auto camera_copy = camera;
                camera_copy.aspect_ratio = _viewport_view->aspect_ratio();
                ctx.get_entities().assign_or_replace(self.entity, camera_copy);
            }

            ui_ctx.begin_ui_commands();
            draw();
            ui_ctx.finish_ui_commands();
        });

        register_menu_item(make_unique<exit_menu_item>(ctx));
    }

    auto editor_context::draw() -> void
    {
        if (ImGui::BeginMainMenuBar())
        {
            _menus.draw();
            ImGui::EndMainMenuBar();
        }

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
            window->draw();
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