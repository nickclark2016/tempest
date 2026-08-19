#include <tempest/render_system/passes/light_clustering_pass.hpp>

namespace tempest::render_system
{
    auto add_light_clustering_pass(render_graph::render_graph& graph, [[maybe_unused]] resource_pool& pool,
                                   shader_manager& shaders,
                                   render_graph::rg_buffer_id cluster_bounds_buf,
                                   const render_camera& cam,
                                   uint32_t screen_width, uint32_t screen_height,
                                   uint32_t cluster_count_x,
                                   uint32_t cluster_count_y,
                                   uint32_t cluster_count_z)
        -> const light_clustering_pass_data&
    {
        const auto tile_size_px = (screen_width + cluster_count_x - 1) / cluster_count_x;
        const auto create_info = cluster_grid_create_info{
            .inv_projection = cam.inv_proj,
            .screen_bounds = {static_cast<float>(screen_width), static_cast<float>(screen_height), 0.1F, 1000.0F},
            .workgroup_count_tile_size_px = {cluster_count_x, cluster_count_y, cluster_count_z, tile_size_px},
        };

        auto pipe_h = shaders.find_compute_pipeline("build_cluster_grid_pipeline");
        if (!pipe_h.has_value())
        {
            auto cs = shaders.register_shader_module("build_cluster_grid.comp.spv", rhi::shader_stage::compute, "CSMain");
            auto tmpl = compute_pipeline_template{
                .shader_module = cs,
            };
            pipe_h = shaders.register_compute_pipeline("build_cluster_grid_pipeline", tmpl);
        }

        const auto pipe = *pipe_h;

        return graph.add_compute_pass<light_clustering_pass_data>(
            "LightClusteringPass",
            [cluster_bounds_buf, create_info](render_graph::pass_builder& builder, light_clustering_pass_data& data) {
                data.cluster_bounds_buffer = builder.write(cluster_bounds_buf, rhi::pipeline_stage::compute,
                                                           rhi::resource_access::write);
                data.create_info = create_info;
            },
            [&shaders, cluster_count_x, cluster_count_y, cluster_count_z, pipe](
                const light_clustering_pass_data& data,
                render_graph::pass_execution_context& ctx,
                rhi::command_list& pass_cmd) {
                auto rhi_pipe = shaders.get_rhi_pipeline(pipe);
                if (rhi_pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipe);

                const auto cluster_buf_handle = ctx.get_buffer(data.cluster_bounds_buffer);

                const auto constants = build_cluster_grid_push_constants{
                    .clusters_buffer_address = cluster_buf_handle.gpu_address,
                    .create_info = data.create_info,
                };

                pass_cmd.push_constants(rhi::shader_stage::compute, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                pass_cmd.dispatch(cluster_count_x, cluster_count_y, cluster_count_z);
            });
    }
} // namespace tempest::render_system
