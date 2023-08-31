#ifndef tempest_graphics_mesh_component_hpp
#define tempest_graphics_mesh_component_hpp

#include <tempest/mat4.hpp>

#include <cstdint>
#include <numeric>
#include <span>

namespace tempest::graphics
{
    struct mesh_component
    {
        std::uint32_t index_count;
        std::uint32_t first_index;
        std::int32_t vertex_offset;
    };

    struct alignas(16) mesh_layout
    {
        std::uint32_t mesh_start_offset;
        std::uint32_t positions_offset;
        std::uint32_t interleave_offset;
        std::uint32_t interleave_stride;
        std::uint32_t uvs_offset;
        std::uint32_t normals_offset;
        std::uint32_t tangents_offset = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t bitangents_offset = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t color_offset = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t index_offset;
        std::uint32_t index_count;
    };

    struct alignas(16) object_payload
    {
        math::mat4<float> transform;
        std::uint32_t mesh_id;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_mesh_component_hpp