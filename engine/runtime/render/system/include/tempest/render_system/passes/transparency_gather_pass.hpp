#ifndef tempest_render_system_transparency_gather_pass_hpp
#define tempest_render_system_transparency_gather_pass_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>

namespace tempest::render_system
{
    struct transparency_gather_pass_data
    {
        render_graph::rg_texture_id moments_texture;
        render_graph::rg_texture_id zeroth_moment_texture;
        render_graph::rg_texture_id depth_texture;
        render_graph::rg_texture_id ao_texture;
        render_graph::rg_buffer_id scene_constants;
        render_graph::rg_buffer_id object_buffer;
        render_graph::rg_buffer_id instance_buffer;
        render_graph::rg_buffer_id draw_commands;
        render_graph::rg_buffer_id vertex_buffer;
        uint32_t draw_count{0};
        uint32_t draw_offset{0};
    };

    struct transparency_gather_inputs
    {
        render_graph::rg_texture_id moments_texture;
        render_graph::rg_texture_id zeroth_moment_texture;
        render_graph::rg_texture_id depth_texture;
        render_graph::rg_texture_id shadow_atlas;
        render_graph::rg_texture_id ao_texture;
        render_graph::rg_buffer_id light_bitmask_buf;
        uint32_t draw_count;
        uint32_t draw_offset;
    };

    TEMPEST_API auto add_transparency_gather_pass(
        render_graph::render_graph& graph, resource_pool& pool, shader_manager& shaders,
        transparency_gather_inputs inputs,
        enum_mask<rhi::pipeline_statistic_flags> pipeline_stats = rhi::pipeline_statistic_flags::none)
        -> const transparency_gather_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_transparency_gather_pass_hpp
