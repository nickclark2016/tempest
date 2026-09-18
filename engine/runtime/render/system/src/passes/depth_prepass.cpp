#include <tempest/render_system/passes/depth_prepass.hpp>

#include <tempest/array.hpp>

namespace tempest::render_system
{
    auto add_depth_prepass(render_graph::render_graph& graph, resource_pool& pool, shader_manager& shaders,
                           render_graph::rg_texture_id depth_tex, uint32_t opaque_draw_count,
                           uint32_t opaque_draw_offset, uint32_t alpha_masked_draw_count,
                           uint32_t alpha_masked_draw_offset,
                           enum_mask<rhi::pipeline_statistic_flags> pipeline_stats) -> const depth_prepass_data&
    {
        auto opaque_pipe_h = shaders.find_graphics_pipeline("zprepass_opaque_pipeline");
        if (!opaque_pipe_h.has_value())
        {
            auto vs = shaders.register_shader_module("zprepass_opaque.vert.spv", rhi::shader_stage::vertex, "VSMain");
            auto stages = array{vs};

            auto tmpl = graphics_pipeline_template{
                .shader_modules = span<const shader_module_handle>{stages.data(), stages.size()},
                .color_attachment_formats = {},
                .depth_stencil_attachment_format = rhi::data_format::depth32_float,
                .primitive_topology = rhi::primitive_topology::triangle_list,
                .rasterization_state =
                    {
                        .polygon_mode = rhi::polygon_mode::fill,
                        .cull_mode = rhi::cull_mode::none,
                        .front_face = rhi::vertex_winding_order::clockwise,
                    },
                .depth_stencil_state =
                    {
                        .depth_test_enable = true,
                        .depth_write_enable = true,
                        .depth_compare_op = rhi::compare_op::greater,
                    },
            };
            opaque_pipe_h = shaders.register_graphics_pipeline("zprepass_opaque_pipeline", tmpl);
        }

        auto masked_pipe_h = shaders.find_graphics_pipeline("zprepass_masked_pipeline");
        if (!masked_pipe_h.has_value())
        {
            auto vs = shaders.register_shader_module("zprepass_masked.vert.spv", rhi::shader_stage::vertex, "VSMain");
            auto fs = shaders.register_shader_module("zprepass_masked.frag.spv", rhi::shader_stage::fragment, "FSMain");
            auto stages = array{vs, fs};

            auto tmpl = graphics_pipeline_template{
                .shader_modules = span<const shader_module_handle>{stages.data(), stages.size()},
                .color_attachment_formats = {},
                .depth_stencil_attachment_format = rhi::data_format::depth32_float,
                .primitive_topology = rhi::primitive_topology::triangle_list,
                .rasterization_state =
                    {
                        .polygon_mode = rhi::polygon_mode::fill,
                        .cull_mode = rhi::cull_mode::none,
                        .front_face = rhi::vertex_winding_order::clockwise,
                    },
                .depth_stencil_state =
                    {
                        .depth_test_enable = true,
                        .depth_write_enable = true,
                        .depth_compare_op = rhi::compare_op::greater,
                    },
            };
            masked_pipe_h = shaders.register_graphics_pipeline("zprepass_masked_pipeline", tmpl);
        }

        const auto opaque_pipe = *opaque_pipe_h;
        const auto masked_pipe = *masked_pipe_h;

        return graph.add_graphics_pass<depth_prepass_data>(
            "DepthPrepass",
            [&pool, depth_tex, opaque_draw_count, opaque_draw_offset, alpha_masked_draw_count,
             alpha_masked_draw_offset, pipeline_stats](render_graph::pass_builder& builder,
                                                       depth_prepass_data& data) -> void {
                if (pipeline_stats != rhi::pipeline_statistic_flags::none)
                {
                    builder.enable_pipeline_statistics(pipeline_stats);
                }

                data.depth_texture = builder.set_depth_stencil_attachment(render_graph::rg_depth_stencil_attachment{
                    .texture = depth_tex,
                    .depth_load_op = rhi::load_op::clear,
                    .depth_store_op = rhi::store_op::store,
                    .clear_value = {.depth = 0.0F, .stencil = 0},
                });

                data.scene_constants = builder.import_buffer(pool.get_scene_constants_buffer());
                data.object_buffer = builder.import_buffer(pool.get_object_buffer());
                data.instance_buffer = builder.import_buffer(pool.get_instance_buffer());
                data.draw_commands = builder.import_buffer(pool.get_draw_commands_buffer());
                data.vertex_buffer = builder.import_buffer(pool.get_vertex_buffer());

                data.scene_constants =
                    builder.read(data.scene_constants, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.object_buffer =
                    builder.read(data.object_buffer, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.instance_buffer =
                    builder.read(data.instance_buffer, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.draw_commands = builder.read(data.draw_commands, rhi::pipeline_stage::indirect_commands,
                                                  rhi::resource_access::read);
                data.vertex_buffer =
                    builder.read(data.vertex_buffer, rhi::pipeline_stage::vertex, rhi::resource_access::read);
                data.opaque_draw_count = opaque_draw_count;
                data.opaque_draw_offset = opaque_draw_offset;
                data.alpha_masked_draw_count = alpha_masked_draw_count;
                data.alpha_masked_draw_offset = alpha_masked_draw_offset;
            },
            [&pool, &shaders, opaque_pipe, masked_pipe](const depth_prepass_data& data,
                                                        [[maybe_unused]] render_graph::pass_execution_context& ctx,
                                                        rhi::command_list& pass_cmd) -> void {
                if (data.opaque_draw_count == 0 && data.alpha_masked_draw_count == 0)
                {
                    return;
                }

                auto rhi_opaque_pipe = shaders.get_rhi_pipeline(opaque_pipe);
                auto rhi_masked_pipe = shaders.get_rhi_pipeline(masked_pipe);

                pass_cmd.bind_index_buffer(pool.get_vertex_buffer(), rhi::index_type::uint32, 0);

                const auto constants = depth_prepass_push_constants{
                    .scene_constants_address = pool.get_scene_constants_address(),
                    .objects_address = pool.get_object_buffer_address(),
                    .instance_indices_address = pool.get_instance_buffer_address(),
                    .linear_sampler_index = static_cast<int32_t>(pool.get_linear_sampler_descriptor().index),
                };

                if (data.opaque_draw_count > 0 && rhi_opaque_pipe.handle != 0)
                {
                    pass_cmd.bind_pipeline(rhi_opaque_pipe);
                    pass_cmd.push_constants(
                        rhi::shader_stage::vertex, 0,
                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                    const auto byte_offset =
                        pool.get_draw_commands_buffer_offset() +
                        (static_cast<uint64_t>(data.opaque_draw_offset) * sizeof(indexed_indirect_command));
                    pass_cmd.draw_indexed_indirect(pool.get_draw_commands_buffer(), byte_offset, data.opaque_draw_count,
                                                   sizeof(indexed_indirect_command));
                }

                if (data.alpha_masked_draw_count > 0 && rhi_masked_pipe.handle != 0)
                {
                    pass_cmd.bind_pipeline(rhi_masked_pipe);
                    pass_cmd.push_constants(
                        rhi::shader_stage::vertex | rhi::shader_stage::fragment, 0,
                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                    const auto byte_offset =
                        pool.get_draw_commands_buffer_offset() +
                        (static_cast<uint64_t>(data.alpha_masked_draw_offset) * sizeof(indexed_indirect_command));
                    pass_cmd.draw_indexed_indirect(pool.get_draw_commands_buffer(), byte_offset,
                                                   data.alpha_masked_draw_count, sizeof(indexed_indirect_command));
                }
            });
    }
} // namespace tempest::render_system
