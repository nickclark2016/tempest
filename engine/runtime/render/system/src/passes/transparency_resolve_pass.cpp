#include <tempest/render_system/passes/transparency_resolve_pass.hpp>

#include <tempest/array.hpp>

namespace tempest::render_system
{
    auto add_transparency_resolve_pass(render_graph::render_graph& graph, resource_pool& pool, shader_manager& shaders,
                                       render_graph::rg_texture_id accum_tex, render_graph::rg_texture_id moments_tex,
                                       render_graph::rg_texture_id zeroth_moment_tex,
                                       render_graph::rg_texture_id depth_tex, uint32_t draw_count, uint32_t draw_offset,
                                       render_graph::rg_texture_id shadow_atlas,
                                       render_graph::rg_buffer_id light_bitmask_buf)
        -> const transparency_resolve_pass_data&
    {
        auto pipe_h = shaders.find_graphics_pipeline("pbr_oit_resolve_pipeline");
        if (!pipe_h.has_value())
        {
            auto vs = shaders.register_shader_module("pbr_oit_resolve.vert.spv", rhi::shader_stage::vertex, "VSMain");
            auto fs = shaders.register_shader_module("pbr_oit_resolve.frag.spv", rhi::shader_stage::fragment, "FSMain");
            auto stages = array{vs, fs};
            auto color_formats = array{rhi::data_format::rgba16_float};
            auto blend_states = array{
                rhi::attachment_blend_state{
                    .blend_enable = true,
                    .src_color_blend_factor = rhi::blend_factor::one,
                    .dst_color_blend_factor = rhi::blend_factor::one,
                    .src_alpha_blend_factor = rhi::blend_factor::one,
                    .dst_alpha_blend_factor = rhi::blend_factor::one,
                },
            };

            auto tmpl = graphics_pipeline_template{
                .shader_modules = span<const shader_module_handle>{stages.data(), stages.size()},
                .color_attachment_formats = span<const rhi::data_format>{color_formats.data(), color_formats.size()},
                .depth_stencil_attachment_format = rhi::data_format::depth32_float,
                .primitive_topology = rhi::primitive_topology::triangle_list,
                .rasterization_state =
                    {
                        .polygon_mode = rhi::polygon_mode::fill,
                        .cull_mode = rhi::cull_mode::none,
                        .front_face = rhi::vertex_winding_order::counter_clockwise,
                    },
                .depth_stencil_state =
                    {
                        .depth_test_enable = true,
                        .depth_write_enable = false,
                        .depth_compare_op = rhi::compare_op::greater_or_equal,
                    },
                .color_attachment_blend_states =
                    span<const rhi::attachment_blend_state>{blend_states.data(), blend_states.size()},
            };
            pipe_h = shaders.register_graphics_pipeline("pbr_oit_resolve_pipeline", tmpl);
        }

        const auto pipe = *pipe_h;

        return graph.add_graphics_pass<transparency_resolve_pass_data>(
            "TransparencyResolvePass",
            [&pool, accum_tex, moments_tex, zeroth_moment_tex, depth_tex, shadow_atlas, draw_count, draw_offset,
             light_bitmask_buf](render_graph::pass_builder& builder, transparency_resolve_pass_data& data) {
                data.accum_texture = builder.set_color_attachment(0, render_graph::rg_color_attachment{
                                                                         .texture = accum_tex,
                                                                         .load_op = rhi::load_op::clear,
                                                                         .store_op = rhi::store_op::store,
                                                                         .clear_value = {0.0F, 0.0F, 0.0F, 0.0F},
                                                                     });

                data.depth_texture = builder.set_depth_stencil_attachment(render_graph::rg_depth_stencil_attachment{
                    .texture = depth_tex,
                    .depth_load_op = rhi::load_op::load,
                    .depth_store_op = rhi::store_op::store,
                });

                data.moments_texture = builder.read(moments_tex, rhi::pipeline_stage::fragment,
                                                    rhi::resource_access::read, rhi::image_layout::general);
                data.zeroth_moment_texture = builder.read(zeroth_moment_tex, rhi::pipeline_stage::fragment,
                                                          rhi::resource_access::read, rhi::image_layout::general);

                if (shadow_atlas.id != 0)
                {
                    data.shadow_atlas = builder.read(shadow_atlas, rhi::pipeline_stage::fragment,
                                                     rhi::resource_access::read, rhi::image_layout::general);
                }

                data.scene_constants = builder.import_buffer(pool.get_scene_constants_buffer());
                data.object_buffer = builder.import_buffer(pool.get_object_buffer());
                data.instance_buffer = builder.import_buffer(pool.get_instance_buffer());
                data.draw_commands = builder.import_buffer(pool.get_draw_commands_buffer());
                data.vertex_buffer = builder.import_buffer(pool.get_vertex_buffer());

                data.scene_constants =
                    builder.read(data.scene_constants, rhi::pipeline_stage::vertex | rhi::pipeline_stage::fragment,
                                 rhi::resource_access::read);
                data.object_buffer =
                    builder.read(data.object_buffer, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.instance_buffer =
                    builder.read(data.instance_buffer, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.draw_commands = builder.read(data.draw_commands, rhi::pipeline_stage::indirect_commands,
                                                  rhi::resource_access::read);
                data.vertex_buffer =
                    builder.read(data.vertex_buffer, rhi::pipeline_stage::vertex, rhi::resource_access::read);

                if (light_bitmask_buf.is_valid())
                {
                    data.light_bitmask_buffer =
                        builder.read(light_bitmask_buf, rhi::pipeline_stage::fragment, rhi::resource_access::read);
                }

                data.draw_count = draw_count;
                data.draw_offset = draw_offset;
            },
            [&pool, &shaders, pipe](const transparency_resolve_pass_data& data,
                                    render_graph::pass_execution_context& ctx, rhi::command_list& pass_cmd) {
                if (data.draw_count == 0)
                {
                    return;
                }

                auto rhi_pipe = shaders.get_rhi_pipeline(pipe);
                if (rhi_pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipe);
                pass_cmd.bind_index_buffer(pool.get_vertex_buffer(), rhi::index_type::uint32, 0);

                const auto moments_desc_idx = ctx.get_storage_texture_descriptor(data.moments_texture);
                const auto moments_idx = (moments_desc_idx != ~0U) ? static_cast<int32_t>(moments_desc_idx) : -1;

                const auto zeroth_desc_idx = ctx.get_storage_texture_descriptor(data.zeroth_moment_texture);
                const auto zeroth_idx = (zeroth_desc_idx != ~0U) ? static_cast<int32_t>(zeroth_desc_idx) : -1;

                auto shadow_atlas_idx = -1;
                if (data.shadow_atlas.id != 0)
                {
                    const auto shadow_desc_idx = ctx.get_texture_descriptor(data.shadow_atlas);
                    shadow_atlas_idx = (shadow_desc_idx != ~0U) ? static_cast<int32_t>(shadow_desc_idx) : -1;
                }

                auto bitmask_gpu_addr = uint64_t{0};
                if (data.light_bitmask_buffer.is_valid())
                {
                    bitmask_gpu_addr = ctx.get_buffer(data.light_bitmask_buffer).gpu_address;
                }

                const auto constants = transparency_resolve_push_constants{
                    .scene_constants_address = pool.get_scene_constants_address(),
                    .objects_address = pool.get_object_buffer_address(),
                    .instance_indices_address = pool.get_instance_buffer_address(),
                    .directional_shadow_address = pool.get_directional_shadow_address(),
                    .light_bitmask_address = bitmask_gpu_addr,
                    .moments_storage_index = moments_idx,
                    .zeroth_moment_storage_index = zeroth_idx,
                    .linear_sampler_index = static_cast<int32_t>(pool.get_linear_sampler_descriptor().index),
                    .point_sampler_index = -1,
                    .ssao_texture_index = -1,
                    .shadow_atlas_index = shadow_atlas_idx,
                };

                pass_cmd.push_constants(rhi::shader_stage::vertex | rhi::shader_stage::fragment, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                const auto byte_offset = pool.get_draw_commands_buffer_offset() +
                                         static_cast<uint64_t>(data.draw_offset) * sizeof(indexed_indirect_command);
                pass_cmd.draw_indexed_indirect(pool.get_draw_commands_buffer(), byte_offset, data.draw_count,
                                               sizeof(indexed_indirect_command));
            });
    }
} // namespace tempest::render_system
