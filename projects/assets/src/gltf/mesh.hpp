#ifndef tempest_assets_gltf_mesh_hpp
#define tempest_assets_gltf_mesh_hpp

#include <tempest/int.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets::gltf
{
    enum class topology
    {
        POINTS = 0,
        LINES = 1,
        LINE_LOOP = 2,
        LINE_STRIP = 3,
        TRIANGLES = 4,
        TRIANGLE_STRIP = 5,
        TRIANGLE_FAN = 6
    };

    struct mesh_primitive_attribute
    {
        string name;
        size_t accessor;
    };

    struct mesh_primitive
    {
        vector<mesh_primitive_attribute> attributes;
        optional<size_t> indices;
        optional<size_t> material;
        topology mode = topology::TRIANGLES;

        // TODO: Add support for morph targets

        // String representation of JSON objects
        string extension;
        string extras;
    };

    struct mesh
    {
        string name;
        vector<mesh_primitive> primitives;

        // TODO: Add support for weights

        // String representation of JSON objects
        string extension;
        string extras;
    };
} // namespace tempest::assets::gltf

#endif // tempest_assets_gltf_mesh_hpp