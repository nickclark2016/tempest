#ifndef tempest_assets_mesh_hpp
#define tempest_assets_mesh_hpp

#include <tempest/asset.hpp>
#include <tempest/guid.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets
{
    /// @brief Represents a mesh asset.
    ///
    /// Represents a collection of vertices and indices defining a mesh.  The mesh asset is used as
    /// an input type to various systems, such as rendering and physics.
    class mesh : public asset
    {
      public:
        /// @brief Represents a position in 3D space.
        using position = math::vec3<float>;

        /// @brief Represents a texture coordinate.
        using uv = math::vec2<float>;

        /// @brief Represents a normal vector.
        using normal = math::vec3<float>;

        /// @brief Represents a tangent vector.
        using tangent = math::vec4<float>;

        /// @brief Represents a color.
        using color = math::vec4<float>;

        /// @brief Represents an index into a vertex buffer.
        using index = uint32_t;

        explicit mesh(string name);
        mesh(const mesh&) = delete;
        mesh(mesh&&) noexcept = delete;
        virtual ~mesh() = default;

        mesh& operator=(const mesh&) = delete;
        mesh& operator=(mesh&&) noexcept = delete;

        string_view name() const noexcept override;
        guid id() const noexcept override;

        vector<position>& positions() noexcept;
        const vector<position>& positions() const noexcept;
        vector<uv>& uvs() noexcept;
        const vector<uv>& uvs() const noexcept;
        vector<normal>& normals() noexcept;
        const vector<normal>& normals() const noexcept;
        vector<tangent>& tangents() noexcept;
        const vector<tangent>& tangents() const noexcept;
        vector<color>& colors() noexcept;
        const vector<color>& colors() const noexcept;
        vector<index>& indices() noexcept;
        const vector<index>& indices() const noexcept;

        math::vec3<float>& min_bounds() noexcept;
        const math::vec3<float>& min_bounds() const noexcept;
        math::vec3<float>& max_bounds() noexcept;
        const math::vec3<float>& max_bounds() const noexcept;

        optional<guid> material() const noexcept;
        void set_material(guid mat) noexcept;

        uint32_t vertex_count() const noexcept;
        uint32_t triangle_count() const noexcept;

        bool validate() const noexcept;
        void generate_indices() noexcept;

        // TODO: Generate normals
        // TODO: Generate tangents

      private:
        guid _id;
        string _name;

        vector<position> _positions;
        vector<uv> _uvs;
        vector<normal> _normals;
        vector<tangent> _tangents;
        vector<color> _colors;
        vector<index> _indices;

        math::vec3<float> _min;
        math::vec3<float> _max;

        optional<guid> _material;
    };

    class mesh_group : public asset
    {
      public:
        explicit mesh_group(string name);
        mesh_group(const mesh_group&) = delete;
        mesh_group(mesh_group&&) noexcept = delete;
        virtual ~mesh_group() = default;

        mesh_group& operator=(const mesh_group&) = delete;
        mesh_group& operator=(mesh_group&&) noexcept = delete;

        string_view name() const noexcept override;
        guid id() const noexcept override;

        vector<guid>& meshes() noexcept;
        span<const guid> meshes() const noexcept;

      private:
        guid _id;
        string _name;
        vector<guid> _meshes;
    };

    inline string_view mesh::name() const noexcept
    {
        return _name;
    }

    inline guid mesh::id() const noexcept
    {
        return _id;
    }

    inline vector<mesh::position>& mesh::positions() noexcept
    {
        return _positions;
    }

    inline const vector<mesh::position>& mesh::positions() const noexcept
    {
        return _positions;
    }

    inline vector<mesh::uv>& mesh::uvs() noexcept
    {
        return _uvs;
    }

    inline const vector<mesh::uv>& mesh::uvs() const noexcept
    {
        return _uvs;
    }

    inline vector<mesh::normal>& mesh::normals() noexcept
    {
        return _normals;
    }

    inline const vector<mesh::normal>& mesh::normals() const noexcept
    {
        return _normals;
    }

    inline vector<mesh::tangent>& mesh::tangents() noexcept
    {
        return _tangents;
    }

    inline const vector<mesh::tangent>& mesh::tangents() const noexcept
    {
        return _tangents;
    }

    inline vector<mesh::color>& mesh::colors() noexcept
    {
        return _colors;
    }

    inline const vector<mesh::color>& mesh::colors() const noexcept
    {
        return _colors;
    }

    inline vector<mesh::index>& mesh::indices() noexcept
    {
        return _indices;
    }

    inline const vector<mesh::index>& mesh::indices() const noexcept
    {
        return _indices;
    }

    inline math::vec3<float>& mesh::min_bounds() noexcept
    {
        return _min;
    }

    inline const math::vec3<float>& mesh::min_bounds() const noexcept
    {
        return _min;
    }

    inline math::vec3<float>& mesh::max_bounds() noexcept
    {
        return _max;
    }

    inline const math::vec3<float>& mesh::max_bounds() const noexcept
    {
        return _max;
    }

    inline optional<guid> mesh::material() const noexcept
    {
        return _material;
    }

    inline void mesh::set_material(guid mat) noexcept
    {
        _material = mat;
    }

    inline uint32_t mesh::vertex_count() const noexcept
    {
        return static_cast<uint32_t>(_positions.size());
    }

    inline uint32_t mesh::triangle_count() const noexcept
    {
        return static_cast<uint32_t>(_indices.size()) / 3;
    }

    inline mesh_group::mesh_group(string name)
        : _id{guid::generate_random_guid()}, _name{tempest::move(name)}
    {
    }

    inline string_view mesh_group::name() const noexcept
    {
        return _name;
    }

    inline guid mesh_group::id() const noexcept
    {
        return _id;
    }

    inline vector<guid>& mesh_group::meshes() noexcept
    {
        return _meshes;
    }

    inline span<const guid> mesh_group::meshes() const noexcept
    {
        return _meshes;
    }
} // namespace tempest::assets

#endif // tempest_assets_mesh_hpp