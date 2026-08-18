#include <tempest/render_system/passes/pbr_opaque_pass.hpp>

#include <iostream>
#include <tempest/array.hpp>

namespace tempest::render_system
{
    auto add_pbr_opaque_pass(render_graph::render_graph& graph, resource_pool& pool,
                             shader_manager& shaders, render_graph::rg_texture_id hdr_color_tex,
                             render_graph::rg_texture_id depth_tex, uint32_t draw_count)
        -> const pbr_opaque_pass_data&
    {
        return graph.add_graphics_pass<pbr_opaque_pass_data>(
            "PBROpaquePass",
            [&pool, hdr_color_tex, depth_tex, draw_count](render_graph::pass_builder& builder,
                                                          pbr_opaque_pass_data& data) {
                data.hdr_color = builder.set_color_attachment(
                    0, render_graph::rg_color_attachment{
                           .texture = hdr_color_tex,
                           .load_op = rhi::load_op::load,
                           .store_op = rhi::store_op::store,
                       });

                data.depth_texture = builder.set_depth_stencil_attachment(
                    render_graph::rg_depth_stencil_attachment{
                        .texture = depth_tex,
                        .depth_load_op = rhi::load_op::load,
                        .depth_store_op = rhi::store_op::store,
                    });

                data.scene_constants = builder.import_buffer(pool.get_scene_constants_buffer());
                data.object_buffer = builder.import_buffer(pool.get_object_buffer());
                data.instance_buffer = builder.import_buffer(pool.get_instance_buffer());
                data.draw_commands = builder.import_buffer(pool.get_draw_commands_buffer());

                data.scene_constants = builder.read(data.scene_constants, rhi::pipeline_stage::vertex | rhi::pipeline_stage::fragment, rhi::resource_access::read);
                data.object_buffer = builder.read(data.object_buffer, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.instance_buffer = builder.read(data.instance_buffer, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.draw_commands = builder.read(data.draw_commands, rhi::pipeline_stage::indirect_commands, rhi::resource_access::read);
                data.draw_count = draw_count;
            },
            [&pool, &shaders](const pbr_opaque_pass_data& data,
                             [[maybe_unused]] render_graph::pass_execution_context& ctx,
                             rhi::command_list& pass_cmd) {
                if (data.draw_count == 0)
                {
                    return;
                }

                auto vs = shaders.create_shader_module_desc("pbr.vert.spv", rhi::shader_stage::vertex, "VSMain");
                auto fs = shaders.create_shader_module_desc("pbr.frag.spv", rhi::shader_stage::fragment, "FSMain");
                if (!vs.has_value() || !fs.has_value())
                {
                    return;
                }

                auto stages = array{*vs, *fs};
                auto color_formats = array{rhi::data_format::rgba16_float};

                auto pipe_desc = rhi::graphics_pipeline_desc{
                    .shader_modules = span<const rhi::shader_module_desc>{stages.data(), stages.size()},
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
                };

                auto pipe = shaders.get_or_create_graphics_pipeline("pbr_opaque_pipeline", pipe_desc);
                if (pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(pipe);
                pass_cmd.bind_index_buffer(pool.get_vertex_buffer(), rhi::index_type::uint32, 0);

                const auto constants = pbr_opaque_push_constants{
                    .scene_constants_address = pool.get_scene_constants_address(),
                    .objects_address = pool.get_object_buffer().gpu_address,
                    .instance_indices_address = pool.get_instance_buffer().gpu_address,
                    .linear_sampler_index = static_cast<int32_t>(pool.get_linear_sampler_descriptor().index),
                };

                pass_cmd.push_constants(rhi::shader_stage::vertex | rhi::shader_stage::fragment, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.draw_indexed_indirect(pool.get_draw_commands_buffer(), 0, data.draw_count,
                                               sizeof(indexed_indirect_command));
            });
    }
} // namespace tempest::render_system
