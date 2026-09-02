#include <tempest/render_system/passes/light_clustering_pass.hpp>

#include <cmath>
#include <tempest/algorithm.hpp>

namespace tempest::render_system
{
    auto compute_cluster_grid_dimensions(uint32_t width, uint32_t height) -> math::vec4<uint32_t>
    {
        if (width == 0 || height == 0)
        {
            return {16, 9, 24, 64};
        }

        const auto aspect = static_cast<float>(width) / static_cast<float>(height);
        auto cx = 16U;
        auto cy = 9U;
        const auto cz = 24U;

        if (aspect >= 3.0F)
        {
            cx = 32U;
            cy = 9U;
        }
        else if (aspect >= 2.1F)
        {
            cx = 21U;
            cy = 9U;
        }
        else if (aspect >= 1.7F)
        {
            cx = 16U;
            cy = 9U;
        }
        else if (aspect >= 1.5F)
        {
            cx = 16U;
            cy = 10U;
        }
        else if (aspect >= 1.2F)
        {
            cx = 16U;
            cy = 12U;
        }
        else
        {
            const auto target_ratio = 16.0F / 9.0F;
            const auto scaled_x = std::round(16.0F * (aspect / target_ratio));
            cx = tempest::max(1U, static_cast<uint32_t>(scaled_x));
            cy = 9U;
        }

        const auto tile_size_px = (width + cx - 1) / cx;
        return {cx, cy, cz, tile_size_px};
    }

    auto add_light_clustering_pass(render_graph::render_graph& graph, [[maybe_unused]] resource_pool& pool,
                                   shader_manager& shaders, render_graph::rg_buffer_id cluster_bounds_buf,
                                   const render_camera& cam, uint32_t screen_width, uint32_t screen_height,
                                   uint32_t cluster_count_x, uint32_t cluster_count_y, uint32_t cluster_count_z,
                                   enum_mask<rhi::pipeline_statistic_flags> pipeline_stats)
        -> const light_clustering_pass_data&
    {
        auto grid_dims = compute_cluster_grid_dimensions(screen_width, screen_height);
        if (cluster_count_x > 0)
        {
            grid_dims.x = cluster_count_x;
        }
        if (cluster_count_y > 0)
        {
            grid_dims.y = cluster_count_y;
        }
        if (cluster_count_z > 0)
        {
            grid_dims.z = cluster_count_z;
        }

        const auto actual_cx = grid_dims.x;
        const auto actual_cy = grid_dims.y;
        const auto actual_cz = grid_dims.z;

        grid_dims.w = (screen_width + actual_cx - 1) / actual_cx;

        const auto create_info = cluster_grid_create_info{
            .inv_projection = cam.inv_proj,
            .screen_bounds = {static_cast<float>(screen_width), static_cast<float>(screen_height), 0.1F, 1000.0F},
            .workgroup_count_tile_size_px = grid_dims,
        };

        auto pipe_h = shaders.find_compute_pipeline("build_cluster_grid_pipeline");
        if (!pipe_h.has_value())
        {
            auto cs =
                shaders.register_shader_module("build_cluster_grid.comp.spv", rhi::shader_stage::compute, "CSMain");
            auto tmpl = compute_pipeline_template{
                .shader_module = cs,
            };
            pipe_h = shaders.register_compute_pipeline("build_cluster_grid_pipeline", tmpl);
        }

        const auto pipe = *pipe_h;

        return graph.add_compute_pass<light_clustering_pass_data>(
            "LightClusteringPass",
            [cluster_bounds_buf, create_info, pipeline_stats](render_graph::pass_builder& builder,
                                                              light_clustering_pass_data& data) {
                if (pipeline_stats != rhi::pipeline_statistic_flags::none)
                {
                    builder.enable_pipeline_statistics(pipeline_stats);
                }

                data.cluster_bounds_buffer =
                    builder.write(cluster_bounds_buf, rhi::pipeline_stage::compute, rhi::resource_access::write);
                data.create_info = create_info;
            },
            [&shaders, actual_cx, actual_cy, actual_cz, pipe](const light_clustering_pass_data& data,
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

                pass_cmd.dispatch(actual_cx, actual_cy, actual_cz);
            });
    }
} // namespace tempest::render_system
