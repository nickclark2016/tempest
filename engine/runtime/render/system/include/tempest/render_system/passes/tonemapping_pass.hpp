#ifndef tempest_render_system_tonemapping_pass_hpp
#define tempest_render_system_tonemapping_pass_hpp

#include <tempest/api.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>

namespace tempest::render_system
{
    struct tonemapping_pass_data
    {
        render_graph::rg_texture_id hdr_color;
        render_graph::rg_texture_id tonemapped_output;
    };

    struct tonemapping_push_constants
    {
        int32_t hdr_texture_index{-1};
        int32_t sampler_index{0};
        float exposure{0.0F};
        float _pad{0.0F};
    };

    TEMPEST_API auto add_tonemapping_pass(render_graph::render_graph& graph, resource_pool& pool,
                                          shader_manager& shaders, render_graph::rg_texture_id hdr_color_tex,
                                          render_graph::rg_texture_id tonemapped_target,
                                          rhi::data_format target_format = rhi::data_format::rgba8_srgb,
                                          float exposure = 0.0F) -> const tonemapping_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_tonemapping_pass_hpp
