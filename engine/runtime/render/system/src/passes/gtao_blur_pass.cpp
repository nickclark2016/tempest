#include <tempest/render_system/passes/gtao_blur_pass.hpp>

#include <tempest/render_graph/types.hpp>
#include <tempest/rhi.hpp>
#include <tempest/vec2.hpp>

namespace tempest::render_system
{
    struct gtao_blur_push_constants
    {
        uint32_t depth_texture;
        uint32_t gtao_blur_input_texture;
        uint32_t point_sampler_index;
        float depth_discontinuity_threshold;
        math::float2 blur_direction;
    };

    // NOLINTNEXTLINE -- Ensure the push constants struct has the expected size for GPU alignment.
    static_assert(sizeof(gtao_blur_push_constants) == 24U);

    auto add_gtao_blur_pass(render_graph::render_graph& graph, resource_pool& pool, shader_manager& shaders,
                            render_graph::rg_texture_id gtao_raw_tex, render_graph::rg_texture_id depth_tex,
                            float depth_discontinuity_threshold, enum_mask<rhi::pipeline_statistic_flags> statistics)
        -> const gtao_blur_pass_data&
    {
        auto gtao_blur_pipeline_res = shaders.find_graphics_pipeline("gtao_blur_pipeline");
        if (!gtao_blur_pipeline_res)
        {
            auto vert_shader =
                shaders.register_shader_module("gtao_blur.vert.spv", rhi::shader_stage::vertex, "VSMain");
            auto frag_shader =
                shaders.register_shader_module("gtao_blur.frag.spv", rhi::shader_stage::fragment, "FSMain");
            const auto stages = array{vert_shader, frag_shader};
            const auto color_formats = array{rhi::data_format::r16_float};

            auto tmpl = graphics_pipeline_template{
                .shader_modules = stages,
                .color_attachment_formats = color_formats,
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

            gtao_blur_pipeline_res = shaders.register_graphics_pipeline("gtao_blur_pipeline", tmpl);
        }
        TEMPEST_ASSERT(gtao_blur_pipeline_res.has_value() && "Failed to find or register GTAO blur pipeline");

        const auto gtao_blur_pipeline = *gtao_blur_pipeline_res;

        const auto intermediate_pass = graph.add_graphics_pass<gtao_blur_pass_data>(
            "GtaoHorizontalBlurPass",
            [depth_tex, gtao_raw_tex, statistics](render_graph::pass_builder& builder,
                                                  gtao_blur_pass_data& data) -> void {
                if (statistics != rhi::pipeline_statistic_flags::none)
                {
                    builder.enable_pipeline_statistics(statistics);
                }

                auto gtao_blurred = builder.create_texture(render_graph::rg_texture_desc{
                    .size = render_graph::rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::r16_float,
                    .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
                    .mip_levels = 1,
                    .array_layers = 1,
                    .name = "gtao_blur_scratch_texture",
                });

                data.gtao_blurred = builder.set_color_attachment(0, {
                                                                        .texture = gtao_blurred,
                                                                        .load_op = rhi::load_op::clear,
                                                                        .store_op = rhi::store_op::store,
                                                                        .clear_value =
                                                                            rhi::clear_color_value{
                                                                                .r = 1.0F,
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

                data.gtao_raw = builder.read(gtao_raw_tex, make_enum_mask(rhi::pipeline_stage::fragment),
                                             make_enum_mask(rhi::resource_access::read), rhi::image_layout::general);

                data.depth_tex = builder.read(depth_tex, make_enum_mask(rhi::pipeline_stage::fragment),
                                              make_enum_mask(rhi::resource_access::read), rhi::image_layout::general);
            },
            [&shaders, &pool, gtao_blur_pipeline,
             depth_discontinuity_threshold](const gtao_blur_pass_data& data, render_graph::pass_execution_context& ctx,
                                            rhi::command_list& pass_cmd) -> void {
                auto rhi_pipeline = shaders.get_rhi_pipeline(gtao_blur_pipeline);
                if (rhi_pipeline.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipeline);

                const auto depth_desc_idx = ctx.get_texture_descriptor(data.depth_tex);
                const auto gtao_raw_desc_idx = ctx.get_texture_descriptor(data.gtao_raw);
                const auto gtao_raw_size = ctx.get_resolved_size(data.gtao_raw);

                if (depth_desc_idx == ~0U || gtao_raw_desc_idx == ~0U || gtao_raw_size.width == 0 ||
                    gtao_raw_size.height == 0)
                {
                    return;
                }

                const auto constants = gtao_blur_push_constants{
                    .depth_texture = depth_desc_idx,
                    .gtao_blur_input_texture = gtao_raw_desc_idx,
                    .point_sampler_index = pool.get_point_sampler_descriptor().index,
                    .depth_discontinuity_threshold = depth_discontinuity_threshold,
                    .blur_direction = {1.0F / static_cast<float>(ctx.get_resolved_size(data.gtao_raw).width), 0.0F},
                };

                pass_cmd.push_constants(rhi::shader_stage::fragment, 0, as_bytes(span(&constants, 1)));
                pass_cmd.draw(3, 1, 0, 0);
            });

        return graph.add_graphics_pass<gtao_blur_pass_data>(
            "GtaoVerticalBlurPass",
            [statistics, depth_tex, gtao_horizontal_blurred = intermediate_pass.gtao_blurred](
                render_graph::pass_builder& builder, gtao_blur_pass_data& data) -> void {
                if (statistics != rhi::pipeline_statistic_flags::none)
                {
                    builder.enable_pipeline_statistics(statistics);
                }

                const auto gtao_final = builder.create_texture({
                    .size = render_graph::rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::r16_float,
                    .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
                    .mip_levels = 1,
                    .array_layers = 1,
                    .name = "gtao_result",
                });

                data.gtao_blurred = builder.set_color_attachment(0, {
                                                                        .texture = gtao_final,
                                                                        .load_op = rhi::load_op::dont_care,
                                                                        .store_op = rhi::store_op::store,
                                                                        .subresource =
                                                                            {
                                                                                .base_mip_level = 0,
                                                                                .mip_level_count = 1,
                                                                                .base_array_layer = 0,
                                                                                .array_layer_count = 1,
                                                                            },
                                                                    });

                data.gtao_raw = builder.read(gtao_horizontal_blurred, make_enum_mask(rhi::pipeline_stage::fragment),
                                             make_enum_mask(rhi::resource_access::read), rhi::image_layout::general);

                data.depth_tex = builder.read(depth_tex, make_enum_mask(rhi::pipeline_stage::fragment),
                                              make_enum_mask(rhi::resource_access::read), rhi::image_layout::general);
            },
            [&shaders, &pool, gtao_blur_pipeline,
             depth_discontinuity_threshold](const gtao_blur_pass_data& data, render_graph::pass_execution_context& ctx,
                                            rhi::command_list& pass_cmd) -> void {
                auto rhi_pipeline = shaders.get_rhi_pipeline(gtao_blur_pipeline);
                if (rhi_pipeline.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipeline);

                const auto depth_desc_idx = ctx.get_texture_descriptor(data.depth_tex);
                const auto gtao_raw_desc_idx = ctx.get_texture_descriptor(data.gtao_raw);
                const auto gtao_raw_size = ctx.get_resolved_size(data.gtao_raw);

                if (depth_desc_idx == ~0U || gtao_raw_desc_idx == ~0U || gtao_raw_size.width == 0 ||
                    gtao_raw_size.height == 0)
                {
                    return;
                }

                const auto constants = gtao_blur_push_constants{
                    .depth_texture = depth_desc_idx,
                    .gtao_blur_input_texture = gtao_raw_desc_idx,
                    .point_sampler_index = pool.get_point_sampler_descriptor().index,
                    .depth_discontinuity_threshold = depth_discontinuity_threshold,
                    .blur_direction = {0.0F, 1.0F / static_cast<float>(gtao_raw_size.height)},
                };

                pass_cmd.push_constants(rhi::shader_stage::fragment, 0, as_bytes(span(&constants, 1)));
                pass_cmd.draw(3, 1, 0, 0);
            });
    }
} // namespace tempest::render_system