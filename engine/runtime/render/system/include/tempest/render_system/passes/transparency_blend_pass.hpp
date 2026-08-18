#ifndef tempest_render_system_transparency_blend_pass_hpp
#define tempest_render_system_transparency_blend_pass_hpp

#include <tempest/api.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>

namespace tempest::render_system
{
    struct transparency_blend_pass_data
    {
        render_graph::rg_texture_id accum_texture;
        render_graph::rg_texture_id hdr_color;
    };

    struct transparency_blend_push_constants
    {
        int32_t accumulation_texture_index{-1};
        int32_t linear_sampler_index{0};
        float padding[2]{0.0F, 0.0F};
    };

    TEMPEST_API auto add_transparency_blend_pass(render_graph::render_graph& graph, resource_pool& pool,
                                                 shader_manager& shaders,
                                                 render_graph::rg_texture_id accum_tex,
                                                 render_graph::rg_texture_id hdr_color_tex)
        -> const transparency_blend_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_transparency_blend_pass_hpp
