#include <tempest/render_system/passes/tonemapping_pass.hpp>

#include <iostream>
#include <tempest/array.hpp>

namespace tempest::render_system
{
    auto add_tonemapping_pass(render_graph::render_graph& graph, resource_pool& pool,
                              shader_manager& shaders, render_graph::rg_texture_id hdr_color_tex,
                              render_graph::rg_texture_id tonemapped_target,
                              rhi::data_format target_format,
                              float exposure) -> const tonemapping_pass_data&
    {
        return graph.add_graphics_pass<tonemapping_pass_data>(
            "TonemappingPass",
            [hdr_color_tex, tonemapped_target](render_graph::pass_builder& builder, tonemapping_pass_data& data) {
                data.hdr_color = builder.read(hdr_color_tex, rhi::pipeline_stage::fragment, rhi::resource_access::read,
                                              rhi::image_layout::general);
                data.tonemapped_output = builder.set_color_attachment(
                    0, render_graph::rg_color_attachment{
                           .texture = tonemapped_target,
                           .load_op = rhi::load_op::clear,
                           .store_op = rhi::store_op::store,
                           .clear_value = {0.0F, 0.0F, 0.0F, 1.0F},
                       });
                builder.mark_sink();
            },
            [&pool, &shaders, target_format, exposure](const tonemapping_pass_data& data,
                                                      render_graph::pass_execution_context& ctx,
                                                      rhi::command_list& pass_cmd) {
                auto vs = shaders.create_shader_module_desc("tonemap.vert.spv", rhi::shader_stage::vertex, "VSMain");
                auto fs = shaders.create_shader_module_desc("tonemap.frag.spv", rhi::shader_stage::fragment, "FSMain");
                if (!vs.has_value() || !fs.has_value())
                {
                    return;
                }

                auto stages = array{*vs, *fs};
                auto color_formats = array{target_format};

                auto pipe_desc = rhi::graphics_pipeline_desc{
                    .shader_modules = span<const rhi::shader_module_desc>{stages.data(), stages.size()},
                    .color_attachment_formats = span<const rhi::data_format>{color_formats.data(), color_formats.size()},
                    .primitive_topology = rhi::primitive_topology::triangle_list,
                    .rasterization_state =
                        {
                            .polygon_mode = rhi::polygon_mode::fill,
                            .cull_mode = rhi::cull_mode::none,
                            .front_face = rhi::vertex_winding_order::counter_clockwise,
                        },
                    .depth_stencil_state =
                        {
                            .depth_test_enable = false,
                            .depth_write_enable = false,
                        },
                };

                const auto desc_idx = ctx.get_texture_descriptor(data.hdr_color);
                const auto hdr_tex_idx = (desc_idx != ~0U) ? static_cast<int32_t>(desc_idx) : -1;

                auto pipe = shaders.get_or_create_graphics_pipeline("tonemap_pipeline", pipe_desc);
                if (pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(pipe);

                const auto constants = tonemapping_push_constants{
                    .hdr_texture_index = hdr_tex_idx,
                    .sampler_index = static_cast<int32_t>(pool.get_linear_sampler_descriptor().index),
                    .exposure = exposure,
                };

                pass_cmd.push_constants(rhi::shader_stage::vertex | rhi::shader_stage::fragment, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.draw(3, 1, 0, 0);
            });
    }
} // namespace tempest::render_system
