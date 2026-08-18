#ifndef tempest_render_system_ssao_blur_pass_hpp
#define tempest_render_system_ssao_blur_pass_hpp

#include <tempest/api.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>

namespace tempest::render_system
{
    struct ssao_blur_pass_data
    {
        render_graph::rg_texture_id ssao_raw;
        render_graph::rg_texture_id depth_texture;
        render_graph::rg_texture_id ssao_blurred;
    };

    struct ssao_blur_push_constants
    {
        int32_t ssao_texture_index{-1};
        int32_t depth_texture_index{-1};
        int32_t point_sampler_index{0};
        int32_t linear_sampler_index{0};
        math::vec2<float> inv_screen_size{0.0F, 0.0F};
        float depth_threshold{0.05F};
        float padding{0.0F};
    };

    TEMPEST_API auto add_ssao_blur_pass(render_graph::render_graph& graph, resource_pool& pool,
                                        shader_manager& shaders, render_graph::rg_texture_id ssao_raw_tex,
                                        render_graph::rg_texture_id depth_tex,
                                        render_graph::rg_texture_id ssao_blurred_tex,
                                        uint32_t width, uint32_t height,
                                        float depth_threshold = 0.05F)
        -> const ssao_blur_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_ssao_blur_pass_hpp
