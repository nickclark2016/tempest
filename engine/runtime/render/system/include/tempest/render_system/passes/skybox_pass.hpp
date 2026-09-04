#ifndef tempest_render_system_skybox_pass_hpp
#define tempest_render_system_skybox_pass_hpp

#include <tempest/api.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>

namespace tempest::render_system
{
    struct skybox_pass_data
    {
        render_graph::rg_texture_id hdr_color;
        render_graph::rg_buffer_id scene_constants;
    };

    struct skybox_push_constants
    {
        uint64_t scene_constants_address{0};
        int32_t skybox_texture_index{-1};
        int32_t sampler_index{0};
        int32_t _pad{0};
    };

    TEMPEST_API auto add_skybox_pass(render_graph::render_graph& graph, resource_pool& pool, shader_manager& shaders,
                                     render_graph::rg_texture_id hdr_color_tex, int32_t skybox_tex_idx = -1,
                                     enum_mask<rhi::pipeline_statistic_flags> pipeline_stats =
                                         rhi::pipeline_statistic_flags::none) -> const skybox_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_skybox_pass_hpp
