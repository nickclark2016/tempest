#ifndef tempest_render_system_transparency_blend_pass_hpp
#define tempest_render_system_transparency_blend_pass_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>

namespace tempest::render_system
{
    struct transparency_blend_pass_data
    {
        render_graph::rg_texture_id hdr_color;
        render_graph::rg_texture_id accum_texture;
        render_graph::rg_texture_id zeroth_moment_texture;
    };

    struct transparency_blend_push_constants
    {
        int32_t accumulation_texture_index{-1};
        int32_t zeroth_moment_storage_index{-1};
        int32_t linear_sampler_index{0};
        int32_t padding{0};
    };

    TEMPEST_API auto add_transparency_blend_pass(
        render_graph::render_graph& graph, resource_pool& pool, shader_manager& shaders,
        render_graph::rg_texture_id hdr_color_tex, render_graph::rg_texture_id accum_tex,
        render_graph::rg_texture_id zeroth_moment_tex, rhi::data_format target_format = rhi::data_format::rgba16_float,
        enum_mask<rhi::pipeline_statistic_flags> pipeline_stats = rhi::pipeline_statistic_flags::none)
        -> const transparency_blend_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_transparency_blend_pass_hpp
