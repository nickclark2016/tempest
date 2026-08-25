#ifndef tempest_render_system_pbr_opaque_pass_hpp
#define tempest_render_system_pbr_opaque_pass_hpp

#include <tempest/api.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>

namespace tempest::render_system
{
    struct pbr_opaque_pass_data
    {
        render_graph::rg_texture_id hdr_color;
        render_graph::rg_texture_id depth_texture;
        render_graph::rg_texture_id shadow_atlas;
        render_graph::rg_buffer_id scene_constants;
        render_graph::rg_buffer_id object_buffer;
        render_graph::rg_buffer_id instance_buffer;
        render_graph::rg_buffer_id draw_commands;
        uint32_t draw_count{0};
        uint32_t draw_offset{0};
    };

    struct pbr_opaque_push_constants
    {
        uint64_t scene_constants_address{0};
        uint64_t objects_address{0};
        uint64_t instance_indices_address{0};
        uint64_t directional_shadow_address{0};
        int32_t linear_sampler_index{0};
        int32_t shadow_atlas_index{-1};
    };

    TEMPEST_API auto add_pbr_opaque_pass(render_graph::render_graph& graph, resource_pool& pool,
                                         shader_manager& shaders, render_graph::rg_texture_id hdr_color_tex,
                                         render_graph::rg_texture_id depth_tex,
                                         render_graph::rg_texture_id shadow_atlas,
                                         uint32_t draw_count, uint32_t draw_offset = 0)
        -> const pbr_opaque_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_pbr_opaque_pass_hpp
