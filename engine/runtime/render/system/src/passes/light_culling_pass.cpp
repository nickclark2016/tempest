#include <tempest/render_system/passes/light_culling_pass.hpp>

namespace tempest::render_system
{
    auto add_light_culling_pass(render_graph::render_graph& graph, resource_pool& pool,
                                shader_manager& shaders,
                                render_graph::rg_buffer_id cluster_bounds_buf,
                                render_graph::rg_buffer_id lights_buf,
                                render_graph::rg_buffer_id light_grid_buf,
                                render_graph::rg_buffer_id light_indices_buf,
                                render_graph::rg_buffer_id global_count_buf,
                                const cluster_grid_create_info& create_info,
                                uint32_t light_count)
        -> const light_culling_pass_data&
    {
        auto pipe_h = shaders.find_compute_pipeline("cull_lights_pipeline");
        if (!pipe_h.has_value())
        {
            auto cs = shaders.register_shader_module("cull_lights.comp.spv", rhi::shader_stage::compute, "CSMain");
            auto tmpl = compute_pipeline_template{
                .shader_module = cs,
            };
            pipe_h = shaders.register_compute_pipeline("cull_lights_pipeline", tmpl);
        }

        const auto pipe = *pipe_h;

        return graph.add_compute_pass<light_culling_pass_data>(
            "LightCullingPass",
            [cluster_bounds_buf, lights_buf, light_grid_buf, light_indices_buf, global_count_buf, &pool, create_info, light_count](
                render_graph::pass_builder& builder, light_culling_pass_data& data) {
                data.cluster_bounds_buffer = builder.read(cluster_bounds_buf, rhi::pipeline_stage::compute, rhi::resource_access::read);
                data.lights_buffer = builder.read(lights_buf, rhi::pipeline_stage::compute, rhi::resource_access::read);
                data.light_grid_buffer = builder.write(light_grid_buf, rhi::pipeline_stage::compute, rhi::resource_access::write);
                data.light_indices_buffer = builder.write(light_indices_buf, rhi::pipeline_stage::compute, rhi::resource_access::write);
                data.global_count_buffer = builder.write(global_count_buf, rhi::pipeline_stage::compute, rhi::resource_access::write);
                data.scene_constants = builder.import_buffer(pool.get_scene_constants_buffer());
                data.scene_constants = builder.read(data.scene_constants, rhi::pipeline_stage::compute, rhi::resource_access::read);
                data.create_info = create_info;
                data.light_count = light_count;
            },
            [&pool, &shaders, pipe](
                const light_culling_pass_data& data,
                render_graph::pass_execution_context& ctx,
                rhi::command_list& pass_cmd) {
                auto rhi_pipe = shaders.get_rhi_pipeline(pipe);
                if (rhi_pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipe);

                const auto cluster_buf = ctx.get_buffer(data.cluster_bounds_buffer);
                const auto lights_buf = ctx.get_buffer(data.lights_buffer);
                const auto light_grid_buf = ctx.get_buffer(data.light_grid_buffer);
                const auto light_indices_buf = ctx.get_buffer(data.light_indices_buffer);
                const auto global_count_buf = ctx.get_buffer(data.global_count_buffer);

                const auto constants = light_culling_push_constants{
                    .scene_constants_address = pool.get_scene_constants_address(),
                    .clusters_address = cluster_buf.gpu_address,
                    .lights_address = lights_buf.gpu_address,
                    .light_indices_address = light_indices_buf.gpu_address,
                    .light_grid_address = light_grid_buf.gpu_address,
                    .global_count_address = global_count_buf.gpu_address,
                    .workgroup_count_tile_size_px = data.create_info.workgroup_count_tile_size_px,
                    .light_count = data.light_count,
                    .padding = {0, 0, 0},
                };

                pass_cmd.push_constants(rhi::shader_stage::compute, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.dispatch(1, 1, data.create_info.workgroup_count_tile_size_px.z);
            });
    }
} // namespace tempest::render_system
