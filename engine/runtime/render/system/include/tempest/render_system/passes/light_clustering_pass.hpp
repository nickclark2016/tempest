#ifndef tempest_render_system_light_clustering_pass_hpp
#define tempest_render_system_light_clustering_pass_hpp

#include <tempest/api.hpp>
#include <tempest/mat4.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>
#include <tempest/vec4.hpp>

namespace tempest::render_system
{
    struct TEMPEST_API cluster_grid_create_info
    {
        math::mat4<float> inv_projection{1.0F};
        math::vec4<float> screen_bounds{1920.0F, 1080.0F, 0.1F, 1000.0F};
        math::vec4<uint32_t> workgroup_count_tile_size_px{16, 9, 24, 64};
    };

    struct TEMPEST_API cluster_bounds
    {
        math::vec4<float> min_corner{0.0F, 0.0F, 0.0F, 0.0F};
        math::vec4<float> max_corner{0.0F, 0.0F, 0.0F, 0.0F};
    };

    struct light_clustering_pass_data
    {
        render_graph::rg_buffer_id cluster_bounds_buffer;
        cluster_grid_create_info create_info{};
    };

    struct build_cluster_grid_push_constants
    {
        uint64_t clusters_buffer_address{0};
        cluster_grid_create_info create_info{};
    };

    TEMPEST_API auto add_light_clustering_pass(render_graph::render_graph& graph, resource_pool& pool,
                                               shader_manager& shaders,
                                               render_graph::rg_buffer_id cluster_bounds_buf,
                                               const render_camera& cam,
                                               uint32_t screen_width, uint32_t screen_height,
                                               uint32_t cluster_count_x = 16,
                                               uint32_t cluster_count_y = 9,
                                               uint32_t cluster_count_z = 24)
        -> const light_clustering_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_light_clustering_pass_hpp
