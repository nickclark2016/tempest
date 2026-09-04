#include <tempest/render_system/passes/light_culling_pass.hpp>

#include <tempest/algorithm.hpp>

namespace tempest::render_system
{
    auto add_light_culling_pass(render_graph::render_graph& graph, resource_pool& pool, shader_manager& shaders,
                                render_graph::rg_buffer_id cluster_bounds_buf, render_graph::rg_buffer_id lights_buf,
                                const cluster_grid_create_info& create_info, uint32_t light_count,
                                optional<render_graph::rg_buffer_id> light_bitmask_buf,
                                enum_mask<rhi::pipeline_statistic_flags> pipeline_stats)
        -> const light_culling_pass_data&
    {
        const auto cx = create_info.workgroup_count_tile_size_px.x;
        const auto cy = create_info.workgroup_count_tile_size_px.y;
        const auto cz = create_info.workgroup_count_tile_size_px.z;
        const auto total_clusters = cx * cy * cz;
        const auto words_per_cluster = (light_count + 31U) / 32U;

        auto bitmask_buf = render_graph::rg_buffer_id{};
        if (light_bitmask_buf.has_value() && light_bitmask_buf->is_valid())
        {
            bitmask_buf = *light_bitmask_buf;
        }
        else
        {
            const auto bitmask_buffer_size = tempest::max(1U, total_clusters * words_per_cluster) * sizeof(uint32_t);
            bitmask_buf = graph.create_buffer(render_graph::rg_buffer_desc{
                .size = bitmask_buffer_size,
                .usage = rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address |
                         rhi::buffer_usage::transfer_src,
                .name = "LightBitmaskBuffer",
            });
        }

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
            [cluster_bounds_buf, lights_buf, bitmask_buf, &pool, create_info, light_count, words_per_cluster,
             total_clusters, pipeline_stats](render_graph::pass_builder& builder, light_culling_pass_data& data) {
                if (pipeline_stats != rhi::pipeline_statistic_flags::none)
                {
                    builder.enable_pipeline_statistics(pipeline_stats);
                }

                data.cluster_bounds_buffer =
                    builder.read(cluster_bounds_buf, rhi::pipeline_stage::compute, rhi::resource_access::read);
                data.lights_buffer = builder.read(lights_buf, rhi::pipeline_stage::compute, rhi::resource_access::read);
                data.light_bitmask_buffer =
                    builder.write(bitmask_buf, rhi::pipeline_stage::compute, rhi::resource_access::write);
                data.scene_constants = builder.import_buffer(pool.get_scene_constants_buffer());
                data.scene_constants =
                    builder.read(data.scene_constants, rhi::pipeline_stage::compute, rhi::resource_access::read);
                data.create_info = create_info;
                data.light_count = light_count;
                data.words_per_cluster = words_per_cluster;
                data.total_clusters = total_clusters;
            },
            [&shaders, pipe, &pool](const light_culling_pass_data& data, render_graph::pass_execution_context& ctx,
                                    rhi::command_list& pass_cmd) {
                auto rhi_pipe = shaders.get_rhi_pipeline(pipe);
                if (rhi_pipe.handle == 0)
                {
                    return;
                }

                pass_cmd.bind_pipeline(rhi_pipe);

                const auto cluster_buf = ctx.get_buffer(data.cluster_bounds_buffer);
                const auto lights_buf = ctx.get_buffer(data.lights_buffer);
                const auto bitmask_buf = ctx.get_buffer(data.light_bitmask_buffer);

                const auto constants = light_culling_push_constants{
                    .scene_constants_address = pool.get_scene_constants_address(),
                    .clusters_address = cluster_buf.gpu_address,
                    .lights_address = lights_buf.gpu_address,
                    .light_bitmask_address = bitmask_buf.gpu_address,
                    .cluster_counts_tile_size = data.create_info.workgroup_count_tile_size_px,
                    .light_count = data.light_count,
                    .words_per_cluster = data.words_per_cluster,
                    .total_clusters = data.total_clusters,
                    .padding = 0,
                };

                pass_cmd.push_constants(rhi::shader_stage::compute, 0,
                                        span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

                const auto workgroup_count = (data.total_clusters + 63U) / 64U;
                pass_cmd.dispatch(workgroup_count, 1, 1);
            });
    }
} // namespace tempest::render_system
