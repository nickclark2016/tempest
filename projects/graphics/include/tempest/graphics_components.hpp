#ifndef tempest_graphics_graphics_components_hpp
#define tempest_graphics_graphics_components_hpp

#include <tempest/mat4.hpp>
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
        std::uint32_t mesh_start_offset;
        std::uint32_t positions_offset;
        std::uint32_t interleave_offset;
        std::uint32_t interleave_stride;
        std::uint32_t uvs_offset;
        std::uint32_t normals_offset;
        std::uint32_t tangents_offset = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t color_offset = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t index_offset;
        std::uint32_t index_count;
    };

    enum class material_type : std::uint32_t
    {
        OPAQUE = 0,
        TRANSPARENT = 1,
        MASK = 2,
    };

    struct material_payload
    {
        material_type type{material_type::OPAQUE};

        std::uint32_t albedo_map_id{std::numeric_limits<std::uint32_t>::max()};
        std::uint32_t normal_map_id{std::numeric_limits<std::uint32_t>::max()};
        std::uint32_t metallic_map_id{std::numeric_limits<std::uint32_t>::max()};
        std::uint32_t roughness_map_id{std::numeric_limits<std::uint32_t>::max()};
        std::uint32_t ao_map_id{std::numeric_limits<std::uint32_t>::max()};
        std::uint32_t emissive_map_id{std::numeric_limits<std::uint32_t>::max()};

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
        std::uint32_t mesh_id{std::numeric_limits<std::uint32_t>::max()};
        std::uint32_t material_id{std::numeric_limits<std::uint32_t>::max()};
        std::uint32_t parent_id{std::numeric_limits<std::uint32_t>::max()};
        std::uint32_t self_id{std::numeric_limits<std::uint32_t>::max()};
    };

    struct renderable_component
    {
        std::uint32_t mesh_id;
        std::uint32_t material_id;
        std::uint32_t object_id;
    };

    struct camera_component
    {
        math::vec3<float> position;
        math::vec3<float> forward;
        math::vec3<float> up;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_graphics_components_hpp