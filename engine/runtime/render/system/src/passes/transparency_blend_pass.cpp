#include <tempest/render_system/passes/transparency_blend_pass.hpp>

#include <tempest/array.hpp>

namespace tempest::render_system
{
    auto add_transparency_blend_pass(render_graph::render_graph& graph, resource_pool& pool,
                                     shader_manager& shaders, render_graph::rg_texture_id hdr_color_tex,
                                     render_graph::rg_texture_id accum_tex,
                                     render_graph::rg_texture_id zeroth_moment_tex,
                                     rhi::data_format target_format)
        -> const transparency_blend_pass_data&
    {
        auto pipe_h = shaders.find_graphics_pipeline("pbr_oit_blend_pipeline");
        if (!pipe_h.has_value())
        {
            auto vs = shaders.register_shader_module("pbr_oit_blend.vert.spv", rhi::shader_stage::vertex, "VSMain");
            auto fs = shaders.register_shader_module("pbr_oit_blend.frag.spv", rhi::shader_stage::fragment, "FSMain");
            auto stages = array{vs, fs};
            auto color_formats = array{target_format};
            auto blend_states = array{
                rhi::attachment_blend_state{
                    .blend_enable = true,
                    .src_color_blend_factor = rhi::blend_factor::one,
                    .dst_color_blend_factor = rhi::blend_factor::one_minus_src_alpha,
                    .src_alpha_blend_factor = rhi::blend_factor::one,
                    .dst_alpha_blend_factor = rhi::blend_factor::one_minus_src_alpha,
                },
            };

            auto tmpl = graphics_pipeline_template{
                .shader_modules = span<const shader_module_handle>{stages.data(), stages.size()},
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
                .color_attachment_blend_states =
                    span<const rhi::attachment_blend_state>{blend_states.data(), blend_states.size()},
            };
            pipe_h = shaders.register_graphics_pipeline("pbr_oit_blend_pipeline", tmpl);
        }

        const auto pipe = *pipe_h;

        return graph.add_graphics_pass<transparency_blend_pass_data>(
            "TransparencyBlendPass",
            [hdr_color_tex, accum_tex, zeroth_moment_tex](render_graph::pass_builder& builder,
                                                          transparency_blend_pass_data& data) {
                data.hdr_color = builder.set_color_attachment(
                    0, render_graph::rg_color_attachment{
                           .texture = hdr_color_tex,
                           .load_op = rhi::load_op::load,
                           .store_op = rhi::store_op::store,
                       });
                data.accum_texture = builder.read(accum_tex, rhi::pipeline_stage::fragment,
                                                  rhi::resource_access::read, rhi::image_layout::general);
                data.zeroth_moment_texture = builder.read(zeroth_moment_tex, rhi::pipeline_stage::fragment,
                                                          rhi::resource_access::read, rhi::image_layout::general);
            },
            [&pool, &shaders, pipe](const transparency_blend_pass_data& data,
                                    render_graph::pass_execution_context& ctx,
                                    rhi::command_list& pass_cmd) {
                const auto accum_desc_idx = ctx.get_texture_descriptor(data.accum_texture);
                const auto accum_idx = (accum_desc_idx != ~0U) ? static_cast<int32_t>(accum_desc_idx) : -1;

                const auto zeroth_desc_idx = ctx.get_storage_texture_descriptor(data.zeroth_moment_texture);
                const auto zeroth_idx = (zeroth_desc_idx != ~0U) ? static_cast<int32_t>(zeroth_desc_idx) : -1;

                auto rhi_pipe = shaders.get_rhi_pipeline(pipe);
                if (rhi_pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipe);

                const auto constants = transparency_blend_push_constants{
                    .accumulation_texture_index = accum_idx,
                    .zeroth_moment_storage_index = zeroth_idx,
                    .linear_sampler_index = static_cast<int32_t>(pool.get_linear_sampler_descriptor().index),
                    .padding = 0,
                };

                pass_cmd.push_constants(rhi::shader_stage::vertex | rhi::shader_stage::fragment, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.draw(3, 1, 0, 0);
            });
    }
} // namespace tempest::render_system
