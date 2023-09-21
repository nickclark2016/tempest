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

    struct mesh_view
    {
        std::span<vertex> vertices;
        std::span<std::uint32_t> indices;

        bool has_tangents;
        bool has_bitangents;
        bool has_colors;

        [[nodiscard]] std::size_t bytes_per_vertex() const noexcept;
        [[nodiscard]] std::size_t size_bytes() const noexcept;
    };

    inline std::size_t mesh_view::bytes_per_vertex() const noexcept
    {
        std::size_t bpv = (3 + 2 + 3) * sizeof(float);
        bpv += (has_tangents ? 3 : 0) * sizeof(float);
        bpv += (has_bitangents ? 3 : 0) * sizeof(float);
        bpv += (has_colors ? 4 : 0) * sizeof(float);
        return bpv;
    }

    inline std::size_t mesh_view::size_bytes() const noexcept
    {
        std::size_t vertex_size = vertices.size() * bytes_per_vertex();
        std::size_t index_size = indices.size_bytes();
        return vertex_size + index_size;
    }
} // namespace tempest::core

#endif // tempest_vertex_hpp