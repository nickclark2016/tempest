#ifndef tempest_graphics_graphics_components_hpp
#define tempest_graphics_graphics_components_hpp

#include <tempest/int.hpp>
#include <tempest/mat4.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

#include <cstdint>
#include <limits>
#include <numeric>
#include <span>

namespace tempest::graphics
{
    struct mesh_layout
    {
        uint32_t mesh_start_offset;
        uint32_t positions_offset;
        uint32_t interleave_offset;
        uint32_t interleave_stride;
        uint32_t uvs_offset;
        uint32_t normals_offset;
        uint32_t tangents_offset = std::numeric_limits<uint32_t>::max();
        uint32_t color_offset = std::numeric_limits<uint32_t>::max();
        uint32_t index_offset;
        uint32_t index_count;
    };

    enum class alpha_behavior : uint32_t
    {
        OPAQUE = 0,
        MASK = 1,
        TRANSPARENT = 2,
    };

    struct material_payload
    {
        alpha_behavior type{alpha_behavior::OPAQUE};

        uint32_t albedo_map_id{std::numeric_limits<uint32_t>::max()};
        uint32_t normal_map_id{std::numeric_limits<uint32_t>::max()};
        uint32_t metallic_map_id{std::numeric_limits<uint32_t>::max()};
        uint32_t roughness_map_id{std::numeric_limits<uint32_t>::max()};
        uint32_t ao_map_id{std::numeric_limits<uint32_t>::max()};
        uint32_t emissive_map_id{std::numeric_limits<uint32_t>::max()};

        float alpha_cutoff;
        float metallic_factor;
        float roughness_factor;
        float reflectance;
        float normal_scale;

        math::vec4<float> base_color_factor;
        math::vec3<float> emissive_factor;
    };

    struct object_payload
    {
        math::mat4<float> transform;
        math::mat4<float> inv_transform;
        uint32_t mesh_id{std::numeric_limits<uint32_t>::max()};
        uint32_t material_id{std::numeric_limits<uint32_t>::max()};
        uint32_t parent_id{std::numeric_limits<uint32_t>::max()};
        uint32_t self_id{std::numeric_limits<uint32_t>::max()};
    };

    struct renderable_component
    {
        uint32_t mesh_id;
        uint32_t material_id;
        uint32_t object_id;
    };

    struct camera_component
    {
        float aspect_ratio;
        float vertical_fov;
        float near_plane{0.1f};
        float far_shadow_plane{256.0f};
    };

    struct directional_light_component
    {
        math::vec3<float> color;
        float intensity;
    };

    struct point_light_component
    {
        math::vec3<float> color;
        float intensity;
        float range;
    };

    struct shadow_map_component
    {
        math::vec2<uint32_t> size;
        uint32_t cascade_count;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_graphics_components_hpp