#include <tempest/render_system/passes/ssao_pass.hpp>

#include <tempest/array.hpp>

namespace tempest::render_system
{
    auto add_ssao_pass(render_graph::render_graph& graph, resource_pool& pool,
                       shader_manager& shaders, render_graph::rg_texture_id depth_tex,
                       render_graph::rg_texture_id ssao_raw_tex,
                       float radius, float bias, float power)
        -> const ssao_pass_data&
    {
        auto pipe_h = shaders.find_graphics_pipeline("ssao_pipeline");
        if (!pipe_h.has_value())
        {
            auto vs = shaders.register_shader_module("ssao.vert.spv", rhi::shader_stage::vertex, "VSMain");
            auto fs = shaders.register_shader_module("ssao.frag.spv", rhi::shader_stage::fragment, "FSMain");
            auto stages = array{vs, fs};
            auto color_formats = array{rhi::data_format::r8_unorm};

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
            pipe_h = shaders.register_graphics_pipeline("ssao_pipeline", tmpl);
        }

        const auto pipe = *pipe_h;

        return graph.add_graphics_pass<ssao_pass_data>(
            "SSAOPass",
            [depth_tex, ssao_raw_tex, &pool](render_graph::pass_builder& builder, ssao_pass_data& data) {
                data.depth_texture = builder.read(depth_tex, rhi::pipeline_stage::fragment,
                                                  rhi::resource_access::read, rhi::image_layout::general);
                data.ssao_raw = builder.set_color_attachment(
                    0, render_graph::rg_color_attachment{
                           .texture = ssao_raw_tex,
                           .load_op = rhi::load_op::clear,
                           .store_op = rhi::store_op::store,
                           .clear_value = {1.0F, 1.0F, 1.0F, 1.0F},
                       });
                data.scene_constants = builder.import_buffer(pool.get_scene_constants_buffer());
                data.scene_constants = builder.read(data.scene_constants, rhi::pipeline_stage::fragment,
                                                    rhi::resource_access::read);
            },
            [&pool, &shaders, radius, bias, power, pipe](const ssao_pass_data& data,
                                                         render_graph::pass_execution_context& ctx,
                                                         rhi::command_list& pass_cmd) {
                auto rhi_pipe = shaders.get_rhi_pipeline(pipe);
                if (rhi_pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipe);

                const auto depth_desc_idx = ctx.get_texture_descriptor(data.depth_texture);
                const auto depth_tex_idx = (depth_desc_idx != ~0U) ? static_cast<int32_t>(depth_desc_idx) : -1;

                const auto constants = ssao_push_constants{
                    .scene_constants_address = pool.get_scene_constants_address(),
                    .depth_texture_index = depth_tex_idx,
                    .linear_sampler_index = static_cast<int32_t>(pool.get_linear_sampler_descriptor().index),
                    .point_sampler_index = static_cast<int32_t>(pool.get_point_sampler_descriptor().index),
                    .radius = radius,
                    .bias = bias,
                    .power = power,
                    .padding = 0.0F,
                };

                pass_cmd.push_constants(rhi::shader_stage::vertex | rhi::shader_stage::fragment, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.draw(3, 1, 0, 0);
            });
    }
} // namespace tempest::render_system
