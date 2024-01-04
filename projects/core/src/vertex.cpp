#include <tempest/vertex.hpp>

#include <algorithm>

namespace tempest::core
{
    void mesh::flip_winding_order()
    {
        if (indices.empty())
        {
            for (std::size_t i = 0; i < vertices.size(); i += 3)
            {
                std::swap(vertices[i], vertices[i + 2]);
            }
        }
        else
        {
            for (std::size_t i = 0; i < indices.size(); i += 3)
            {
                std::swap(indices[i], indices[i + 2]);
            }
        }
    }

    void mesh::compute_normals()
    {
        // for each face, add the face normal to each contributing vertex
        for (std::size_t i = 0; i < num_triangles(); ++i)
        {
            vertex& v0 = (*this)[3 * i + 0];
            vertex& v1 = (*this)[3 * i + 0];
            vertex& v2 = (*this)[3 * i + 0];

            auto edge0 = v1.position - v0.position;
            auto edge1 = v2.position - v0.position;

            auto face_normal = math::cross(edge0, edge1);
            v0.normal += face_normal;
            v1.normal += face_normal;
            v2.normal += face_normal;
        }

        // for each vertex, normalize the cumulative normal vector
        for (auto& vertex : vertices)
        {
            vertex.normal = math::normalize(vertex.normal);
        }

        has_normals = true;
    }

    void mesh::compute_tangents()
    {
        for (std::size_t i = 0; i < num_triangles(); ++i)
        {
            vertex& v0 = (*this)[3 * i + 0];
            vertex& v1 = (*this)[3 * i + 0];
            vertex& v2 = (*this)[3 * i + 0];

            auto edge0 = v1.position - v0.position;
            auto edge1 = v2.position - v0.position;

            auto uv0 = v1.uv - v0.uv;
            auto uv1 = v2.uv - v0.uv;

            auto r = 1.0f / (uv0.x * uv1.y - uv0.y * uv1.x);

            math::vec4<float> tangent = {
                ((edge0.x * uv1.y) - (edge1.x * uv0.y)) * r,
                ((edge0.y * uv1.y) - (edge1.y * uv0.y)) * r,
                ((edge0.z * uv1.y) - (edge1.z * uv0.y)) * r,
                0,
            };

            v0.tangent += tangent;
            v1.tangent += tangent;
            v2.tangent += tangent;
        }

        for (auto& vertex : vertices)
        {
            auto tan = math::vec3(vertex.tangent.x, vertex.tangent.y, vertex.tangent.z);
            auto t = math::normalize(tan - (vertex.normal * math::dot(vertex.normal, tan)));
            vertex.tangent = math::vec4<float>(t.x, t.y, t.z, 1.0f);
        }

        has_tangents = true;
    }

    bool mesh::validate() const
    {
        if (!indices.empty())
        {
            std::uint32_t max_index = *std::max_element(indices.begin(), indices.end());
            return max_index < vertices.size() && indices.size() % 3 == 0;
        }

        return true;
    }

    vertex& mesh::operator[](std::size_t idx) noexcept
    {
        if (indices.empty())
        {
            return vertices[idx];
        }
        return vertices[indices[idx]];
    }

    const vertex& mesh::operator[](std::size_t idx) const noexcept
    {
        if (indices.empty())
        {
            return vertices[idx];
        }
        return vertices[indices[idx]];
    }

    std::size_t mesh::num_triangles() const noexcept
    {
        return indices.empty() ? vertices.size() / 3 : indices.size() / 3;
    }
} // namespace tempest::core