#include <tempest/mesh.hpp>

namespace tempest::assets
{
    mesh::mesh(string name) : _name(std::move(name))
    {
        _id = guid::generate_random_guid();
    }

    void mesh::generate_indices() noexcept
    {
        auto vc = vertex_count();
        auto tc = triangle_count();

        _indices.clear();
        _indices.reserve(tc * 3);

        for (uint32_t i = 0; i < vc; i += 3)
        {
            _indices.push_back(i);
            _indices.push_back(i + 1);
            _indices.push_back(i + 2);
        }
    }

    bool mesh::validate() const noexcept
    {
        auto vc = vertex_count();
        auto tc = triangle_count();

        // Check if the number of positions is greater than 0
        if (vc == 0)
        {
            return false;
        }

        // Check if the number of indices is greater than 0
        if (indices().size() == 0)
        {
            return false;
        }

        // Check if the number of indices is a multiple of 3
        if (tc * 3 != indices().size())
        {
            return false;
        }

        // Ensure the number of uvs matches the number of positions
        if (vc != uvs().size())
        {
            return false;
        }

        // Ensure the number of normals matches the number of positions
        if (vc != normals().size())
        {
            return false;
        }

        // Ensure the number of tangents matches the number of positions or is 0
        if (vc != tangents().size() && tangents().size() != 0)
        {
            return false;
        }

        // Ensure the number of colors matches the number of positions or is 0
        if (vc != colors().size() && colors().size() != 0)
        {
            return false;
        }

        // Validate to ensure all indices are within the range of the number of positions
        for (auto i : indices())
        {
            if (i >= vc)
            {
                return false;
            }
        }

        return true;
    }
} // namespace tempest::assets