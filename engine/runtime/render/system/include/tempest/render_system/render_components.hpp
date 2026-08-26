#ifndef tempest_render_system_render_components_hpp
#define tempest_render_system_render_components_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/mat4.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

namespace tempest::render_system
{
    struct TEMPEST_API mesh_layout
    {
        uint64_t vertex_buffer_address{0};
        uint32_t mesh_start_offset{0};
        uint32_t positions_offset{0};
        uint32_t interleave_offset{0};
        uint32_t interleave_stride{0};
        uint32_t uvs_offset{0};
        uint32_t normals_offset{0};
        uint32_t tangents_offset{numeric_limits<uint32_t>::max()};
        uint32_t color_offset{numeric_limits<uint32_t>::max()};
        uint32_t index_offset{0};
        uint32_t index_count{0};
    };

    enum class alpha_behavior : uint32_t
    {
        opaque = 0,
        mask = 1,
        transparent = 2,
        transmissive = 3,
    };

    enum class material_type : uint32_t
    {
        opaque = 0,
        mask = 1,
        blend = 2,
        transmissive = 3,
    };

    struct TEMPEST_API material_payload
    {
        static constexpr int32_t invalid_texture_id = -1;

        math::vec4<float> base_color_factor{1.0F, 1.0F, 1.0F, 1.0F};
        math::vec4<float> emissive_factor{0.0F, 0.0F, 0.0F, 0.0F};
        math::vec4<float> attenuation_color{1.0F, 1.0F, 1.0F, 1.0F};

        float normal_scale{1.0F};
        float metallic_factor{0.0F};
        float roughness_factor{1.0F};
        float alpha_cutoff{0.5F};
        float reflectance{0.5F};
        float transmission_factor{0.0F};
        float thickness_factor{0.0F};
        float attenuation_distance{0.0F};

        int32_t base_color_texture_id{invalid_texture_id};
        int32_t normal_texture_id{invalid_texture_id};
        int32_t metallic_roughness_texture_id{invalid_texture_id};
        int32_t emissive_texture_id{invalid_texture_id};
        int32_t occlusion_texture_id{invalid_texture_id};
        int32_t transmission_texture_id{invalid_texture_id};
        int32_t thickness_texture_id{invalid_texture_id};
        material_type type{material_type::opaque};
    };

    struct TEMPEST_API object_payload
    {
        math::mat4<float> model{1.0F};
        math::mat4<float> inv_transpose_model{1.0F};
        uint64_t mesh_gpu_address{0};      // 64-bit BDA pointer to mesh_layout
        uint64_t material_gpu_address{0};  // 64-bit BDA pointer to material_payload
        uint64_t parent_gpu_address{0};    // 64-bit BDA pointer to parent object_payload (0 if root)
        uint32_t self_id{numeric_limits<uint32_t>::max()};
        uint32_t padding{0};
    };

    struct TEMPEST_API indirect_command
    {
        uint32_t vertex_count{0};
        uint32_t instance_count{0};
        uint32_t first_vertex{0};
        uint32_t first_instance{0};
    };

    struct TEMPEST_API indexed_indirect_command
    {
        uint32_t index_count{0};
        uint32_t instance_count{0};
        uint32_t first_index{0};
        int32_t vertex_offset{0};
        uint32_t first_instance{0};
    };

    struct TEMPEST_API renderable_component
    {
        guid mesh_id;
        guid material_id;
        bool double_sided;
    };

    struct TEMPEST_API camera_component
    {
        float aspect_ratio;
        float vertical_fov;
        float near_plane;
    };

    struct TEMPEST_API active_camera_component
    {
    };

    struct TEMPEST_API directional_light_component
    {
        math::vec3<float> color;
        float intensity;
    };

    struct TEMPEST_API point_light_component
    {
        math::vec3<float> color;
        float intensity;
        float range;
    };

    struct TEMPEST_API light_payload
    {
        math::vec4<float> color_intensity{1.0F, 1.0F, 1.0F, 1.0F}; // rgb = color, w = intensity
        math::vec4<float> position_falloff{0.0F, 0.0F, 0.0F, 10.0F}; // xyz = world position, w = range/radius
        math::vec4<float> direction_angle{0.0F, -1.0F, 0.0F, 0.0F}; // xyz = forward direction, w = spot angle
        uint32_t type{1}; // 0 = Directional, 1 = Point, 2 = Spot
        uint32_t enabled{1};
        uint32_t padding[2]{0, 0};
    };

    enum class shadow_debug_mode : uint32_t
    {
        none = 0,
        cascades = 1,
        shadow_factor = 2,
        cascade_and_shadow = 3,
        scene_cascade_tint = 4,
    };

    struct TEMPEST_API shadow_caster_component
    {
        uint32_t resolution{2048};
        uint32_t num_cascades{4};
        float split_lambda{0.5F};
        float max_shadow_distance{200.0F};
        float normal_bias{0.02F};
        float depth_bias{0.005F};
        uint32_t priority{0}; // Lower value = higher priority (0 = directional sun)
        shadow_debug_mode debug_mode{shadow_debug_mode::none};
    };
} // namespace tempest::render_system

#endif // tempest_render_system_render_components_hpp
