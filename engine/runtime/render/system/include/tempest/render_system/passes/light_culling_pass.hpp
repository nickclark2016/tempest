#ifndef tempest_render_system_light_culling_pass_hpp
#define tempest_render_system_light_culling_pass_hpp

#include <tempest/api.hpp>
#include <tempest/optional.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/passes/light_clustering_pass.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>
#include <tempest/vec4.hpp>

namespace tempest::render_system
{
    struct light_culling_pass_data
    {
        render_graph::rg_buffer_id cluster_bounds_buffer;
        render_graph::rg_buffer_id lights_buffer;
        render_graph::rg_buffer_id light_bitmask_buffer;
        render_graph::rg_buffer_id scene_constants;
        cluster_grid_create_info create_info{};
        uint32_t light_count{0};
        uint32_t words_per_cluster{0};
        uint32_t total_clusters{0};
    };

    struct light_culling_push_constants
    {
        uint64_t scene_constants_address{0};
        uint64_t clusters_address{0};
        uint64_t lights_address{0};
        uint64_t light_bitmask_address{0};
        math::vec4<uint32_t> cluster_counts_tile_size{0, 0, 0, 0};
        uint32_t light_count{0};
        uint32_t words_per_cluster{0};
        uint32_t total_clusters{0};
        uint32_t padding{0};
    };

    TEMPEST_API auto add_light_culling_pass(render_graph::render_graph& graph, resource_pool& pool,
                                            shader_manager& shaders,
                                            render_graph::rg_buffer_id cluster_bounds_buf,
                                            render_graph::rg_buffer_id lights_buf,
                                            const cluster_grid_create_info& create_info,
                                            uint32_t light_count,
                                            optional<render_graph::rg_buffer_id> light_bitmask_buf = nullopt)
        -> const light_culling_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_light_culling_pass_hpp
