#ifndef tempest_assets_gltf_buffer_hpp
#define tempest_assets_gltf_buffer_hpp

#include <tempest/int.hpp>
#include <tempest/string.hpp>

namespace tempest::assets::gltf
{
    struct buffer
    {
        string uri;
        size_t byte_length;
        string name;

        string extension;
        string extras;
    };
}

#endif // tempest_assets_gltf_buffer_hpp