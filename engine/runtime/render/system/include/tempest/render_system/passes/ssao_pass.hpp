#ifndef tempest_render_system_ssao_pass_hpp
#define tempest_render_system_ssao_pass_hpp

#include <tempest/api.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>

namespace tempest::render_system
{
    struct ssao_pass_data
    {
        render_graph::rg_texture_id depth_texture;
        render_graph::rg_texture_id ssao_raw;
        render_graph::rg_buffer_id scene_constants;
    };

    struct ssao_push_constants
    {
        uint64_t scene_constants_address{0};
        int32_t depth_texture_index{-1};
        int32_t linear_sampler_index{0};
        int32_t point_sampler_index{0};
        float radius{0.5F};
        float bias{0.025F};
        float power{1.5F};
        float padding{0.0F};
    };

    TEMPEST_API auto add_ssao_pass(render_graph::render_graph& graph, resource_pool& pool,
                                   shader_manager& shaders, render_graph::rg_texture_id depth_tex,
                                   render_graph::rg_texture_id ssao_raw_tex,
                                   float radius = 0.5F, float bias = 0.025F, float power = 1.5F)
        -> const ssao_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_ssao_pass_hpp
