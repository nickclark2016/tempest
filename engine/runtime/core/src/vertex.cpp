#include <tempest/vertex.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/int.hpp>
#include <tempest/tuple.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::core
{
    void mesh::flip_winding_order()
    {
        if (indices.empty())
        {
            for (size_t i = 0; i < vertices.size(); i += 3)
            {
                tempest::swap(vertices[i], vertices[i + 2]);
            }
        }
        else
        {
            for (size_t i = 0; i < indices.size(); i += 3)
            {
                tempest::swap(indices[i], indices[i + 2]);
            }
        }
    }

    void mesh::compute_normals()
    {
        // for each face, add the face normal to each contributing vertex
        for (size_t i = 0; i < num_triangles(); ++i)
        {
            vertex& v0 = (*this)[3 * i + 0];
            vertex& v1 = (*this)[3 * i + 1];
            vertex& v2 = (*this)[3 * i + 2];

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
        // Per-triangle tdir accumulator (bitangent)
        auto tangent_dir = vector<math::vec3<float>>(num_triangles() * 3, math::vec3<float>(0.0F));

        for (size_t i = 0; i < num_triangles(); ++i)
        {
            auto&& [v0, idx0] = get_tri_and_ind(3 * i + 0);
            auto&& [v1, idx1] = get_tri_and_ind(3 * i + 1);
            auto&& [v2, idx2] = get_tri_and_ind(3 * i + 2);

            // Edge vectors
            math::vec3<float> edge1 = v1.position - v0.position;
            math::vec3<float> edge2 = v2.position - v0.position;

            // UV deltas
            float s1 = v1.uv.x - v0.uv.x;
            float s2 = v2.uv.x - v0.uv.x;
            float t1 = v1.uv.y - v0.uv.y;
            float t2 = v2.uv.y - v0.uv.y;

            float denom = s1 * t2 - s2 * t1;

            math::vec3<float> sdir;
            math::vec3<float> tdir;

            if (fabs(denom) > 1e-6f) // valid UV triangle
            {
                float r = 1.0f / denom;
                sdir = (edge1 * t2 - edge2 * t1) * r;
                tdir = (edge2 * s1 - edge1 * s2) * r;
            }
            else // degenerate UVs: fallback tangent/bitangent
            {
                // Pick a vector perpendicular to normal for sdir
                math::vec3<float> n = v0.normal;
                math::vec3<float> up = fabs(n.z) < 0.999f ? math::vec3<float>(0, 0, 1) : math::vec3<float>(0, 1, 0);
                sdir = normalize(cross(up, n));
                tdir = normalize(cross(n, sdir));
            }

            // Accumulate per-vertex tangent (sdir) and bitangent (tdir)
            v0.tangent += math::vec4(sdir.x, sdir.y, sdir.z, 0.0f);
            v1.tangent += math::vec4(sdir.x, sdir.y, sdir.z, 0.0f);
            v2.tangent += math::vec4(sdir.x, sdir.y, sdir.z, 0.0f);

            tangent_dir[idx0] += tdir;
            tangent_dir[idx1] += tdir;
            tangent_dir[idx2] += tdir;
        }

        // Final orthogonalize & compute handedness
        size_t vtx = 0;
        for (auto& vertex : vertices)
        {
            math::vec3<float> n = vertex.normal;
            math::vec3<float> t = math::vec3(vertex.tangent.x, vertex.tangent.y, vertex.tangent.z);

            // Gram-Schmidt orthogonalization
            t = normalize(t - n * math::dot(n, t));

            // Handedness
            float handedness = math::dot(math::cross(n, t), tangent_dir[vtx++]) < 0.0f ? -1.0f : 1.0f;

            vertex.tangent = math::vec4(t.x, t.y, t.z, handedness);
        }

        has_tangents = true;
    }

    bool mesh::validate() const
    {
        if (!indices.empty())
        {
            uint32_t max_index = *tempest::max_element(indices.begin(), indices.end());
            return max_index < vertices.size() && indices.size() % 3 == 0;
        }

        return true;
    }

    vertex& mesh::operator[](size_t idx) noexcept
    {
        if (indices.empty())
        {
            return vertices[idx];
        }
        return vertices[indices[idx]];
    }

    const vertex& mesh::operator[](size_t idx) const noexcept
    {
        if (indices.empty())
        {
            return vertices[idx];
        }
        return vertices[indices[idx]];
    }

    tuple<vertex&, uint32_t> mesh::get_tri_and_ind(size_t idx) noexcept
    {
        if (indices.empty())
        {
            return make_tuple(ref(vertices[idx]), static_cast<uint32_t>(idx));
        }
        auto index = indices[idx];
        return make_tuple(ref(vertices[index]), index);
    }

    tuple<const vertex&, uint32_t> mesh::get_tri_and_ind(size_t idx) const noexcept
    {
        if (indices.empty())
        {
            return make_tuple(cref(vertices[idx]), static_cast<uint32_t>(idx));
        }
        auto index = indices[idx];
        return make_tuple(cref(vertices[index]), index);
    }

    size_t mesh::num_triangles() const noexcept
    {
        return indices.empty() ? vertices.size() / 3 : indices.size() / 3;
    }

    guid mesh_registry::register_mesh(mesh&& m)
    {
        guid g = guid::generate_random_guid();
        _meshes[g] = tempest::move(m);
        return g;
    }

    bool mesh_registry::register_mesh_with_id(const guid& id, mesh&& m)
    {
        if (_meshes.find(id) == _meshes.end())
        {
            _meshes[id] = tempest::move(m);
            return true;
        }
        return false;
    }

    bool mesh_registry::remove_mesh(const guid& g)
    {
        if (auto it = _meshes.find(g); it != _meshes.end())
        {
            _meshes.erase(it);
            return true;
        }
        return false;
    }

    optional<mesh&> mesh_registry::find(const guid& g)
    {
        if (auto it = _meshes.find(g); it != _meshes.end())
        {
            return it->second;
        }
        return none();
    }

    optional<const mesh&> mesh_registry::find(const guid& g) const
    {
        if (auto it = _meshes.find(g); it != _meshes.end())
        {
            return it->second;
        }
        return none();
    }
} // namespace tempest::core