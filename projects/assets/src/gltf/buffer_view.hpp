#ifndef tempest_assets_gltf_buffer_view_hpp
#define tempest_assets_gltf_buffer_view_hpp

#include <tempest/optional.hpp>
#include <tempest/string.hpp>

namespace tempest::assets::gltf
{
    enum class buffer_view_target
    {
        ARRAY_BUFFER = 34962,
        ELEMENT_ARRAY_BUFFER = 34963
    };

    struct buffer_view
    {
        size_t buffer;
        size_t byte_offset = 0;
        size_t byte_length;
        size_t byte_stride;
        optional<buffer_view_target> target;
        string name;

        // String representation of JSON objects
        string extension;
        string extras;
    };
} // namespace tempest::assets::gltf

#endif // tempest_assets_gltf_buffer_view_hpp