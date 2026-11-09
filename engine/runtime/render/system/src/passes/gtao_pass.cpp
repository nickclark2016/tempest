#include "tempest/render_system/shader_manager.hpp"
#include <tempest/array.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/render_system/passes/gtao_pass.hpp>
#include <tempest/rhi.hpp>

namespace tempest::render_system
{
    auto add_gtao_pass(render_graph::render_graph& graph, resource_pool& pool, shader_manager& shaders,
                       render_graph::rg_texture_id depth_tex, uint32_t slice_count, uint32_t steps_per_slice,
                       float radius, float thickness, float power,
                       enum_mask<rhi::pipeline_statistic_flags> pipeline_stats) -> const gtao_pass_data&
    {
        auto gtao_pipeline_handle_res = shaders.find_graphics_pipeline("gtao_pipeline");
        if (!gtao_pipeline_handle_res)
        {
            auto vert_shader = shaders.register_shader_module("gtao.vert.spv", rhi::shader_stage::vertex);
            auto frag_shader = shaders.register_shader_module("gtao.frag.spv", rhi::shader_stage::fragment);
            auto stages = array{vert_shader, frag_shader};
            auto color_formats = array{rhi::data_format::r8_unorm};

            auto tmpl = graphics_pipeline_template{
                .shader_modules = stages,
                .color_attachment_formats = color_formats,
                .primitive_topology = rhi::primitive_topology::triangle_list,
                .rasterization_state =
                    {
                        .polygon_mode = rhi::polygon_mode::fill,
                        .cull_mode = rhi::cull_mode::back,
                        .front_face = rhi::vertex_winding_order::counter_clockwise,
                    },
                .depth_stencil_state =
                    {
                        .depth_test_enable = false,
                        .depth_write_enable = false,
                    },
            };

            gtao_pipeline_handle_res = shaders.register_graphics_pipeline("gtao_pipeline", tmpl);
        }

        const auto gtao_pipeline_handle = *gtao_pipeline_handle_res;

        return graph.add_graphics_pass<gtao_pass_data>(
            "GtaoPass",
            [&depth_tex, &pool, pipeline_stats](render_graph::pass_builder& builder, gtao_pass_data& data) -> void {
                if (pipeline_stats != rhi::pipeline_statistic_flags::none)
                {
                    builder.enable_pipeline_statistics(pipeline_stats);
                }

                data.depth_texture = builder.read(depth_tex, enum_mask(rhi::pipeline_stage::fragment),
                                                  enum_mask(rhi::resource_access::read), rhi::image_layout::general,
                                                  render_graph::rg_subresource_range{
                                                      .base_mip_level = 0,
                                                      .mip_level_count = 1,
                                                      .base_array_layer = 0,
                                                      .array_layer_count = 1,
                                                  });

                auto gtao_raw = builder.create_texture(render_graph::rg_texture_desc{
                    .size = render_graph::rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::r8_unorm,
                    .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
                    .mip_levels = 1,
                    .array_layers = 1,
                    .name = "gtao_raw",
                });

                data.gtao_raw = builder.set_color_attachment(0, {
                                                                    .texture = gtao_raw,
                                                                    .load_op = rhi::load_op::clear,
                                                                    .store_op = rhi::store_op::store,
                                                                    .clear_value =
                                                                        rhi::clear_color_value{
                                                                            .r = 0.0F,
                                                                            .g = 0.0F,
                                                                            .b = 0.0F,
                                                                            .a = 0.0F,
                                                                        },
                                                                    .subresource =
                                                                        render_graph::rg_subresource_range{
                                                                            .base_mip_level = 0,
                                                                            .mip_level_count = 1,
                                                                            .base_array_layer = 0,
                                                                            .array_layer_count = 1,
                                                                        },
                                                                });

                auto scene_constants = builder.import_buffer(pool.get_scene_constants_buffer());

                data.scene_constants = builder.read(scene_constants, enum_mask(rhi::pipeline_stage::fragment),
                                                    enum_mask(rhi::resource_access::read));
            },
            [gtao_pipeline_handle, &shaders, &pool, slice_count, steps_per_slice, radius, thickness,
             power](const gtao_pass_data& data, render_graph::pass_execution_context& ctx,
                    rhi::command_list& pass_cmd) -> void {
                auto rhi_pipeline = shaders.get_rhi_pipeline(gtao_pipeline_handle);
                if (rhi_pipeline.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipeline);

                const auto depth_desc_idx = ctx.get_texture_descriptor(data.depth_texture);

                const auto constants = gtao_push_constants{
                    .scene_constants_address = pool.get_scene_constants_address(),
                    .depth_texture_id = depth_desc_idx,
                    .point_sampler_index = pool.get_point_sampler_descriptor().index,
                    .slice_count = slice_count,
                    .steps_per_slice = steps_per_slice,
                    .radius = radius,
                    .thickness = thickness,
                    .power = power,
                };

                pass_cmd.push_constants(rhi::shader_stage::fragment, 0, as_bytes(span(&constants, 1)));
                pass_cmd.draw(3, 1, 0, 0);
            });
    }
} // namespace tempest::render_system