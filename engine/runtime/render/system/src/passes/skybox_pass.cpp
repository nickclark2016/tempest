#include <tempest/render_system/passes/skybox_pass.hpp>

#include <iostream>
#include <tempest/array.hpp>

namespace tempest::render_system
{
    auto add_skybox_pass(render_graph::render_graph& graph, resource_pool& pool, shader_manager& shaders,
                         render_graph::rg_texture_id hdr_color_tex, int32_t skybox_tex_idx,
                         enum_mask<rhi::pipeline_statistic_flags> pipeline_stats) -> const skybox_pass_data&
    {
        auto pipe_h = shaders.find_graphics_pipeline("skybox_pipeline");
        if (!pipe_h.has_value())
        {
            auto vs = shaders.register_shader_module("skybox.vert.spv", rhi::shader_stage::vertex, "VSMain");
            auto fs = shaders.register_shader_module("skybox.frag.spv", rhi::shader_stage::fragment, "FSMain");
            auto stages = array{vs, fs};
            auto color_formats = array{rhi::data_format::rgba16_float};

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
            };
            pipe_h = shaders.register_graphics_pipeline("skybox_pipeline", tmpl);
        }

        const auto pipe = *pipe_h;

        return graph.add_graphics_pass<skybox_pass_data>(
            "SkyboxPass",
            [&pool, hdr_color_tex, pipeline_stats](render_graph::pass_builder& builder, skybox_pass_data& data) {
                if (pipeline_stats != rhi::pipeline_statistic_flags::none)
                {
                    builder.enable_pipeline_statistics(pipeline_stats);
                }

                data.hdr_color = builder.set_color_attachment(0, render_graph::rg_color_attachment{
                                                                     .texture = hdr_color_tex,
                                                                     .load_op = rhi::load_op::clear,
                                                                     .store_op = rhi::store_op::store,
                                                                     .clear_value = {0.05F, 0.05F, 0.08F, 1.0F},
                                                                 });

                data.scene_constants = builder.import_buffer(pool.get_scene_constants_buffer());
                data.scene_constants =
                    builder.read(data.scene_constants, rhi::pipeline_stage::vertex, rhi::resource_access::read);
            },
            [&pool, &shaders, skybox_tex_idx, pipe]([[maybe_unused]] const skybox_pass_data& data,
                                                    [[maybe_unused]] render_graph::pass_execution_context& ctx,
                                                    rhi::command_list& pass_cmd) {
                auto rhi_pipe = shaders.get_rhi_pipeline(pipe);
                if (rhi_pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipe);

                const auto constants = skybox_push_constants{
                    .scene_constants_address = pool.get_scene_constants_address(),
                    .skybox_texture_index = skybox_tex_idx,
                    .sampler_index = static_cast<int32_t>(pool.get_linear_sampler_descriptor().index),
                };

                pass_cmd.push_constants(rhi::shader_stage::vertex | rhi::shader_stage::fragment, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.draw(3, 1, 0, 0);
            });
    }
} // namespace tempest::render_system
