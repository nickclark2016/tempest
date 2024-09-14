#ifndef tempest_assets_gltf_node_hpp
#define tempest_assets_gltf_node_hpp

#include <tempest/int.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets::gltf
{
    struct node
    {
        vector<size_t> children;
        vector<double> matrix = {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}; // Identity matrix
        optional<size_t> mesh;
        vector<double> rotation = {0.0, 0.0, 0.0, 1.0}; // quaternion
        vector<double> scale = {1.0, 1.0, 1.0};
        vector<double> translation = {0.0, 0.0, 0.0};
        string name;

        // String representation of JSON objects
        string extension;
        string extras;
    };
} // namespace tempest::assets::gltf

#endif // tempest_assets_gltf_node_hpp