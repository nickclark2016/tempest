#ifndef tempest_render_system_transparency_resolve_pass_hpp
#define tempest_render_system_transparency_resolve_pass_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>

namespace tempest::render_system
{
    struct transparency_resolve_pass_data
    {
        render_graph::rg_texture_id accum_texture;
        render_graph::rg_texture_id moments_texture;
        render_graph::rg_texture_id zeroth_moment_texture;
        render_graph::rg_texture_id depth_texture;
        render_graph::rg_texture_id shadow_atlas;
        render_graph::rg_buffer_id scene_constants;
        render_graph::rg_buffer_id object_buffer;
        render_graph::rg_buffer_id instance_buffer;
        render_graph::rg_buffer_id draw_commands;
        uint32_t draw_count{0};
    };

    struct transparency_resolve_push_constants
    {
        uint64_t scene_constants_address{0};
        uint64_t objects_address{0};
        uint64_t instance_indices_address{0};
        uint64_t directional_shadow_address{0};
        int32_t moments_storage_index{-1};
        int32_t zeroth_moment_storage_index{-1};
        int32_t linear_sampler_index{0};
        int32_t point_sampler_index{-1};
        int32_t ssao_texture_index{-1};
        int32_t shadow_atlas_index{-1};
    };

    TEMPEST_API auto add_transparency_resolve_pass(
        render_graph::render_graph& graph,
        resource_pool& pool,
        shader_manager& shaders,
        render_graph::rg_texture_id accum_tex,
        render_graph::rg_texture_id moments_tex,
        render_graph::rg_texture_id zeroth_moment_tex,
        render_graph::rg_texture_id depth_tex,
        uint32_t draw_count,
        render_graph::rg_texture_id shadow_atlas = {})
        -> const transparency_resolve_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_transparency_resolve_pass_hpp
