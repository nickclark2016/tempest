#ifndef tempest_vertex_hpp
#define tempest_vertex_hpp

#include <tempest/memory.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

#include <array>
#include <string>

namespace tempest::core
{
    struct vertex
    {
        math::vec3<float> position;
        math::vec2<float> uv;
        math::vec3<float> normal;
        math::vec4<float> tangent;
        math::vec4<float> color;
    };

    struct mesh_view
    {
        span<const vertex> vertices;
        span<const std::uint32_t> indices;

        bool has_tangents;
        bool has_bitangents;
        bool has_colors;

        [[nodiscard]] std::size_t bytes_per_vertex() const noexcept;
        [[nodiscard]] std::size_t size_bytes() const noexcept;
    };

    struct mesh
    {
        vector<vertex> vertices;
        vector<std::uint32_t> indices;
        std::string name;

        bool has_normals;
        bool has_tangents;
        bool has_colors;

        void flip_winding_order();

        void compute_normals();
        void compute_tangents();

        bool validate() const;

        vertex& operator[](std::size_t idx) noexcept;
        const vertex& operator[](std::size_t idx) const noexcept;

        std::tuple<vertex&, std::uint32_t> get_tri_and_ind(std::size_t idx) noexcept;
        std::tuple<const vertex&, std::uint32_t> get_tri_and_ind(std::size_t idx) const noexcept;

        std::size_t num_triangles() const noexcept;
    };

    inline std::size_t mesh_view::bytes_per_vertex() const noexcept
    {
        std::size_t bpv = (3 + 2 + 3) * sizeof(float);
        bpv += (has_tangents ? 4 : 0) * sizeof(float);
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