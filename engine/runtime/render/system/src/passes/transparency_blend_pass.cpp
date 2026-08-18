#include <tempest/render_system/passes/transparency_blend_pass.hpp>

#include <tempest/array.hpp>

namespace tempest::render_system
{
    auto add_transparency_blend_pass(render_graph::render_graph& graph, resource_pool& pool,
                                     shader_manager& shaders,
                                     render_graph::rg_texture_id accum_tex,
                                     render_graph::rg_texture_id hdr_color_tex)
        -> const transparency_blend_pass_data&
    {
        return graph.add_graphics_pass<transparency_blend_pass_data>(
            "TransparencyBlendPass",
            [accum_tex, hdr_color_tex](render_graph::pass_builder& builder,
                                       transparency_blend_pass_data& data) {
                data.accum_texture = builder.read(accum_tex, rhi::pipeline_stage::fragment,
                                                  rhi::resource_access::read, rhi::image_layout::general);
                data.hdr_color = builder.set_color_attachment(
                    0, render_graph::rg_color_attachment{
                           .texture = hdr_color_tex,
                           .load_op = rhi::load_op::load,
                           .store_op = rhi::store_op::store,
                       });
            },
            [&pool, &shaders](const transparency_blend_pass_data& data,
                             render_graph::pass_execution_context& ctx,
                             rhi::command_list& pass_cmd) {
                auto vs = shaders.create_shader_module_desc("pbr_oit_blend.vert.spv", rhi::shader_stage::vertex, "VSMain");
                auto fs = shaders.create_shader_module_desc("pbr_oit_blend.frag.spv", rhi::shader_stage::fragment, "FSMain");
                if (!vs.has_value() || !fs.has_value())
                {
                    return;
                }

                auto stages = array{*vs, *fs};
                auto color_formats = array{rhi::data_format::rgba16_float};
                auto blend_states = array{
                    rhi::attachment_blend_state{
                        .blend_enable = true,
                        .src_color_blend_factor = rhi::blend_factor::one,
                        .dst_color_blend_factor = rhi::blend_factor::one_minus_src_alpha,
                        .src_alpha_blend_factor = rhi::blend_factor::zero,
                        .dst_alpha_blend_factor = rhi::blend_factor::one,
                    },
                };

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
                    .color_attachment_blend_states = span<const rhi::attachment_blend_state>{blend_states.data(), blend_states.size()},
                };

                auto pipe = shaders.get_or_create_graphics_pipeline("pbr_oit_blend_pipeline", pipe_desc);
                if (pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(pipe);

                const auto accum_desc_idx = ctx.get_texture_descriptor(data.accum_texture);
                const auto accum_tex_idx = (accum_desc_idx != ~0U) ? static_cast<int32_t>(accum_desc_idx) : -1;

                const auto constants = transparency_blend_push_constants{
                    .accumulation_texture_index = accum_tex_idx,
                    .linear_sampler_index = static_cast<int32_t>(pool.get_linear_sampler_descriptor().index),
                    .padding = {0.0F, 0.0F},
                };

                pass_cmd.push_constants(rhi::shader_stage::vertex | rhi::shader_stage::fragment, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.draw(3, 1, 0, 0);
            });
    }
} // namespace tempest::render_system
