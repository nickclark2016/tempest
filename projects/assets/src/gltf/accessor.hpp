#ifndef tempest_assets_gltf_accessor_hpp
#define tempest_assets_gltf_accessor_hpp

#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets::gltf
{
    enum class component_type
    {
        BYTE = 5120,
        UNSIGNED_BYTE = 5121,
        SHORT = 5122,
        UNSIGNED_SHORT = 5123,
        UNSIGNED_INT = 5125,
        FLOAT = 5126
    };

    struct accessor
    {
        optional<size_t> buffer_view;
        size_t byte_offset = 0;
        component_type comp_type;
        bool normalized;
        size_t count;
        string type;
        vector<double> max;
        vector<double> min;
        string name;

        // TODO: Add support for sparse accessors

        // String representation of JSON objects
        string extension;
        string extras;
    };
} // namespace tempest::assets::gltf

#endif // tempest_assets_gltf_accessor_hpp