#include <tempest/render_system/passes/transparency_clear_pass.hpp>

namespace tempest::render_system
{
    auto add_transparency_clear_pass(render_graph::render_graph& graph, shader_manager& shaders,
                                     render_graph::rg_texture_id moments_tex,
                                     render_graph::rg_texture_id zeroth_moment_tex, uint32_t width,
                                     uint32_t height) -> const transparency_clear_pass_data&
    {
        auto pipe_h = shaders.find_compute_pipeline("mboit_clear_pipeline");
        if (!pipe_h.has_value())
        {
            auto cs = shaders.register_shader_module("mboit_clear.comp.spv", rhi::shader_stage::compute, "CSMain");
            auto tmpl = compute_pipeline_template{
                .shader_module = cs,
            };
            pipe_h = shaders.register_compute_pipeline("mboit_clear_pipeline", tmpl);
        }

        const auto pipe = *pipe_h;

        return graph.add_compute_pass<transparency_clear_pass_data>(
            "TransparencyClearPass",
            [moments_tex, zeroth_moment_tex, width, height](render_graph::pass_builder& builder,
                                                            transparency_clear_pass_data& data) {
                data.moments_texture = builder.write(moments_tex, rhi::pipeline_stage::compute,
                                                     rhi::resource_access::write, rhi::image_layout::general);
                data.zeroth_moment_texture = builder.write(zeroth_moment_tex, rhi::pipeline_stage::compute,
                                                           rhi::resource_access::write, rhi::image_layout::general);
                data.width = width;
                data.height = height;
            },
            [&shaders, pipe](const transparency_clear_pass_data& data,
                             render_graph::pass_execution_context& ctx, rhi::command_list& pass_cmd) {
                auto rhi_pipe = shaders.get_rhi_pipeline(pipe);
                if (rhi_pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipe);

                const auto moments_desc_idx = ctx.get_storage_texture_descriptor(data.moments_texture);
                const auto moments_idx = (moments_desc_idx != ~0U) ? static_cast<int32_t>(moments_desc_idx) : -1;

                const auto zeroth_desc_idx = ctx.get_storage_texture_descriptor(data.zeroth_moment_texture);
                const auto zeroth_idx = (zeroth_desc_idx != ~0U) ? static_cast<int32_t>(zeroth_desc_idx) : -1;

                const auto constants = transparency_clear_push_constants{
                    .moments_storage_index = moments_idx,
                    .zeroth_moment_storage_index = zeroth_idx,
                    .width = data.width,
                    .height = data.height,
                };

                pass_cmd.push_constants(rhi::shader_stage::compute, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.dispatch((data.width + 15) / 16, (data.height + 15) / 16, 1);
            });
    }
} // namespace tempest::render_system
