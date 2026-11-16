#ifndef TEMPEST_RENDER_SYSTEM_PASSES_GTAO_BLUR_PASS_HPP
#define TEMPEST_RENDER_SYSTEM_PASSES_GTAO_BLUR_PASS_HPP

#include <tempest/api.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>
#include <tempest/rhi.hpp>

namespace tempest::render_system
{
    struct gtao_blur_pass_data
    {
        render_graph::rg_texture_id depth_tex;
        render_graph::rg_texture_id gtao_raw;
        render_graph::rg_texture_id gtao_blurred;
    };

    static constexpr float default_depth_discontinuity_threshold = 0.05F;

    TEMPEST_API auto add_gtao_blur_pass(render_graph::render_graph& graph, resource_pool& pool, shader_manager& shaders,
                                        render_graph::rg_texture_id gtao_raw_tex, render_graph::rg_texture_id depth_tex,
                                        float depth_discontinuity_threshold = default_depth_discontinuity_threshold,
                                        enum_mask<rhi::pipeline_statistic_flags> statistics =
                                            rhi::pipeline_statistic_flags::none) -> const gtao_blur_pass_data&;
} // namespace tempest::render_system

#endif // TEMPEST_RENDER_SYSTEM_PASSES_GTAO_BLUR_PASS_HPP