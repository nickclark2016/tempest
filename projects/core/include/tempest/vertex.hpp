#ifndef tempest_vertex_hpp
#define tempest_vertex_hpp

#include "memory.hpp"

#include <array>
#include <span>

namespace tempest::core
{
    struct vertex
    {
        std::array<float, 3> position;
        std::array<float, 2> uv;
        std::array<float, 3> normal;
        std::array<float, 3> tangent;
        std::array<float, 3> bitangent;
        std::array<float, 4> color;
    };

    struct mesh
    {
        void release();
        mesh create_submesh(std::size_t first_index, std::size_t index_count);

        [[nodiscard]] std::size_t vertex_count() const noexcept;
        [[nodiscard]] std::size_t primitive_count() const noexcept;

        core::allocator* owner;

        std::size_t underlying_data_length;
        void* underlying;
        std::span<float> positions;
        std::span<float> uvs;
        std::span<float> normals;
        std::span<float> tangents;
        std::span<float> bitangents;
        std::span<float> colors;
        std::span<std::uint32_t> indices;
    };

    inline std::size_t mesh::vertex_count() const noexcept
    {
        return positions.size() / 3; // 3 floats for one vertex position
    }

    inline std::size_t mesh::primitive_count() const noexcept
    {
        return indices.size() / 3; // 3 vertices per triangle
    }
} // namespace tempest::core

#endif // tempest_vertex_hpp