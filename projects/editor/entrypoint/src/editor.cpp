#include "editor.hpp"

#include "ui.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace tempest::editor
{
    editor::editor(engine_context& ctx, rhi::window_surface* win_surface, ui::ui_context* ui_ctx)
        : _ctx{&ctx}, _win_surface{win_surface}, _ui_ctx{ui_ctx}
    {
        auto& pbr_fg = _ctx->get_renderer().get_frame_graph();
        _viewport_state.viewport_image_size = pbr_fg.get_render_target_size();

        auto color_target = pbr_fg.get_builder()->create_render_target({
            .format = rhi::image_format::rgba8_srgb,
            .type = rhi::image_type::image_2d,
            .width = win_surface->framebuffer_width(),
            .height = win_surface->framebuffer_height(),
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

        win_surface->register_resize_callback(
            [&](auto width, auto height) { pbr_fg.resize_render_target(_final_color_target, width, height); });

        auto surface = ctx.get_renderer().get_device().create_render_surface({
            .window = win_surface,
            .min_image_count = 3,
            .format =
                {
                    .space = rhi::color_space::srgb_nonlinear,
                    .format = rhi::image_format::bgra8_srgb,
                },
            .present_mode = rhi::present_mode::immediate,
            .width = win_surface->framebuffer_width(),
            .height = win_surface->framebuffer_height(),
            .layers = 1,
        });

        auto render_graph_builder = pbr_fg.get_builder();
        auto imported_surface_handle = render_graph_builder->import_render_surface("Editor Render Surface", surface);

        render_graph_builder->create_graphics_pass(
            "Editor UI Pass",
            [&](graphics::graphics_task_builder& task) {
                task.read_write(color_target, rhi::image_layout::color_attachment,
                                make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                                make_enum_mask(rhi::memory_access::color_attachment_write,
                                               rhi::memory_access::color_attachment_read),
                                make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                                make_enum_mask(rhi::memory_access::color_attachment_write,
                                               rhi::memory_access::color_attachment_read));

                auto tonemapped_image_handle = pbr_fg.get_tonemapped_color_handle();
                task.read(tonemapped_image_handle, rhi::image_layout::shader_read_only,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_sampled_read, rhi::memory_access::shader_read));
            },
            [](graphics::graphics_task_execution_context& ctx, auto rt, auto device, auto ui) {
                const auto rt_handle = ctx.find_image(rt);
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
            color_target, &ctx.get_renderer().get_device(), _ui_ctx);

        render_graph_builder->create_transfer_pass(
            "Blit to Swapchain Pass",
            [&](graphics::transfer_task_builder& task) {
                task.read(color_target, rhi::image_layout::transfer_src, make_enum_mask(rhi::pipeline_stage::blit),
                          make_enum_mask(rhi::memory_access::transfer_read));
                task.write(imported_surface_handle, rhi::image_layout::transfer_dst,
                           make_enum_mask(rhi::pipeline_stage::blit),
                           make_enum_mask(rhi::memory_access::transfer_write));
            },
            [](graphics::transfer_task_execution_context& ctx, auto color_target, auto swapchain_handle) {
                ctx.blit(color_target, swapchain_handle);
            },
            color_target, imported_surface_handle);
    }

    void editor::draw([[maybe_unused]] const editor::draw_data& data)
    {
        _configure_dockspace();
        _draw_viewport();
        _draw_scene_hierarchy();
        _draw_entity_properties();
    }

    void editor::_configure_dockspace()
    {
        const auto dockspace_id = ImGui::GetID("Tempest Editor Dockspace");
        const auto viewport = ImGui::GetMainViewport();

        if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
        {
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            auto dock_id_main = dockspace_id;

            auto dock_id_bottom = ImGuiID{};
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.2f, &dock_id_bottom, &dock_id_main);

            auto dock_id_left = ImGuiID{};
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.2f, &dock_id_left, &dock_id_main);

            auto dock_id_right = ImGuiID{};
            ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, &dock_id_right, &dock_id_main);

            ImGui::DockBuilderDockWindow("Viewport", dock_id_main);
            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Entity Properties", dock_id_right);
        }

        ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
    }

    void editor::_draw_viewport()
    {
        if (ImGui::Begin("Viewport", &_viewport_state.open))
        {
            const auto window_size = ImGui::GetContentRegionAvail();

            // Set up the camera aspect ratio
            _ctx->get_registry().each([window_size](ecs::self_component self, graphics::camera_component& cam) {
                cam.aspect_ratio = window_size.x / window_size.y;
            });

            // Early exit if the content area is 0 width or height
            if (window_size.x == 0 || window_size.y == 0)
            {
                ImGui::End();
                return;
            }

            if (window_size.x != _viewport_state.viewport_image_size.x ||
                window_size.y != _viewport_state.viewport_image_size.y)
            {
                _ctx->get_renderer().get_frame_graph().resize_render_targets(static_cast<uint32_t>(window_size.x),
                                                                             static_cast<uint32_t>(window_size.y));
                _viewport_state.viewport_texture = _ctx->get_renderer().get_frame_graph().get_tonemapped_color_image();
                _viewport_state.viewport_image_size = {static_cast<uint32_t>(window_size.x),
                                                       static_cast<uint32_t>(window_size.y)};
            }

            if (_viewport_state.viewport_texture)
            {
                _viewport_state.viewport_texture = _ctx->get_renderer().get_frame_graph().get_tonemapped_color_image();
            }

            const auto viewport_tex = _ctx->get_renderer().get_frame_graph().get_tonemapped_color_image();
            ui::image(_viewport_state.viewport_texture, static_cast<uint32_t>(window_size.x),
                      static_cast<uint32_t>(window_size.y));
        }
        ImGui::End();
    }

    void editor::_draw_scene_hierarchy()
    {
        const auto render_entity_node = [](ecs::archetype_entity entity, const ecs::archetype_registry& reg,
                                           ecs::archetype_entity& select, auto render) {
            if (reg.has<assets::prefab_tag_t>(entity))
            {
                return;
            }

            auto rel_comp = reg.try_get<ecs::relationship_component<ecs::archetype_entity>>(entity);
            const bool has_children = rel_comp && rel_comp->first_child != ecs::tombstone;

            auto draw_children = [&]() {
                auto child = rel_comp->first_child;
                while (child != ecs::tombstone)
                {
                    render(child, reg, select, render);
                    auto child_rel = reg.try_get<ecs::relationship_component<ecs::archetype_entity>>(child);
                    if (child_rel)
                    {
                        child = child_rel->next_sibling;
                    }
                    else
                    {
                        break;
                    }
                }
            };

            const void* id = bit_cast<void*>(entity);
            const auto name = reg.name(entity);

            const auto is_selected = select == entity;
            const auto node_flags = is_selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;

            auto node_open = false;

            if (name)
            {
                if (has_children)
                {
                    node_open = ImGui::TreeNodeEx(id, node_flags, "%s", name->data());
                }
                else
                {
                    node_open = ImGui::TreeNodeEx(id, node_flags | ImGuiTreeNodeFlags_Leaf, "%s", name->data());
                }
            }
            else
            {
                if (has_children)
                {
                    node_open = ImGui::TreeNodeEx(id, node_flags, "<Unnamed:%zu>", static_cast<size_t>(entity));
                }
                else
                {
                    node_open = ImGui::TreeNodeEx(id, node_flags | ImGuiTreeNodeFlags_Leaf, "<Unnamed:%zu>",
                                                  static_cast<size_t>(entity));
                }
            }

            if (node_open)
            {
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    select = entity;
                }

                draw_children();

                ImGui::TreePop();
            }
        };

        if (ImGui::Begin("Scene Hierarchy", &_entity_hierarchy_state.open))
        {
            _ctx->get_registry().each(
                [&](ecs::relationship_component<ecs::archetype_entity> rel_comp, ecs::self_component self) {
                    const auto is_root = rel_comp.parent == ecs::tombstone;
                    if (is_root)
                    {
                        render_entity_node(self.entity, _ctx->get_registry(), _entity_hierarchy_state.selected_entity,
                                           render_entity_node);
                    }
                });
        }

        ImGui::End();
    }

    namespace
    {
        struct input_text_callback_user_data
        {
            string* str;
            ImGuiInputTextCallback chained_callback;
            void* chained_callback_user_data;
        };

        int input_text_callback(ImGuiInputTextCallbackData* data)
        {
            auto user_data = static_cast<input_text_callback_user_data*>(data->UserData);
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                // Resize string callback
                // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set
                // them back to what we want.
                auto str = user_data->str;
                IM_ASSERT(data->Buf == str->c_str());
                str->resize(data->BufTextLen);
                data->Buf = str->data();
            }
            else if (user_data->chained_callback)
            {
                // Forward to user callback, if any
                data->UserData = user_data->chained_callback_user_data;
                return user_data->chained_callback(data);
            }
            return 0;
        }
    } // namespace

    void editor::_draw_entity_properties()
    {
        if (ImGui::Begin("Entity Properties", &_entity_properties_state.open))
        {
            const auto selected = _entity_hierarchy_state.selected_entity;
            if (selected == ecs::null)
            {
                ImGui::Text("No Entity Selected");
                ImGui::End();
                return;
            }

            auto selected_entity_name = _ctx->get_registry()
                                            .name(selected)
                                            .transform([](const auto& view) { return string{view}; })
                                            .value_or("");
            auto cb_user_data = input_text_callback_user_data{
                .str = &selected_entity_name,
                .chained_callback = nullptr,
                .chained_callback_user_data = nullptr,
            };

            const auto id = bit_cast<void*>(selected);
            ImGui::PushID(id);
            const auto name_modified = ImGui::InputTextWithHint(
                "Name", "Unnamed", selected_entity_name.data(), selected_entity_name.capacity() + 1,
                ImGuiInputTextFlags_CallbackResize, input_text_callback, &cb_user_data);
            ImGui::PopID();

            if (name_modified)
            {
                _ctx->get_registry().name(selected, selected_entity_name);
            }
        }

        ImGui::End();
    }
} // namespace tempest::editor
