#ifndef tempest_render_system_shadow_map_pass_hpp
#define tempest_render_system_shadow_map_pass_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/mat4.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

namespace tempest::render_system
{
    struct TEMPEST_API csm_cascade
    {
        math::mat4<float> light_view_projection{1.0F};
        math::vec2<float> atlas_offset{0.0F, 0.0F};
        math::vec2<float> atlas_scale{1.0F, 1.0F};
        float split_depth{0.0F};
        float blend_start{0.9F};
        float texel_size_ws{0.0F};
        uint32_t cascade_index{0};
    };

    struct TEMPEST_API directional_shadow_gpu_data
    {
        uint32_t atlas_index{0};
        uint32_t cascade_count{0};
        math::vec2<float> inv_atlas_resolution{0.0F, 0.0F};
        array<csm_cascade, 4> cascades{};
    };

    struct TEMPEST_API shadow_parameters_gpu
    {
        static constexpr size_t max_directional_lights = 4;
        array<directional_shadow_gpu_data, max_directional_lights> directional_shadow_maps{};
        uint32_t directional_light_count{0};
        uint32_t padding[3]{0, 0, 0};
    };

    struct shadow_map_pass_data
    {
        render_graph::rg_texture_id shadow_atlas;
        render_graph::rg_buffer_id object_buffer;
        render_graph::rg_buffer_id instance_buffer;
        render_graph::rg_buffer_id draw_commands;
        uint32_t draw_count{0};
        uint32_t cascade_count{0};
        array<csm_cascade, 4> cascades{};
        math::vec2<uint32_t> atlas_size{2048, 2048};
    };

    struct shadow_map_push_constants
    {
        math::mat4<float> view_projection{1.0F};
        uint64_t objects_address{0};
        uint64_t instance_indices_address{0};
        int32_t linear_sampler_index{0};
        int32_t padding{0};
    };

    TEMPEST_API auto compute_csm_cascades(const math::vec3<float>& light_dir,
                                          const render_camera& cam,
                                          const shadow_map_component& shadow_cfg,
                                          math::vec2<uint32_t> atlas_size)
        -> array<csm_cascade, 4>;

    TEMPEST_API auto add_shadow_map_pass(render_graph::render_graph& graph, resource_pool& pool,
                                         shader_manager& shaders, render_graph::rg_texture_id shadow_atlas_tex,
                                         const math::vec3<float>& light_dir,
                                         const render_camera& cam,
                                         const shadow_map_component& shadow_cfg,
                                         math::vec2<uint32_t> atlas_size,
                                         uint32_t draw_count)
        -> const shadow_map_pass_data&;
} // namespace tempest::render_system

#endif // tempest_render_system_shadow_map_pass_hpp
