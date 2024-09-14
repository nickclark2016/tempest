#ifndef tempest_assets_gltf_texture_hpp
#define tempest_assets_gltf_texture_hpp

#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>

namespace tempest::assets::gltf
{
    struct texture
    {
        optional<size_t> sampler;
        size_t source;
        string name;

        // String representation of JSON objects
        string extension;
        string extras;
    };
} // namespace tempest::assets::gltf

#endif // tempest_assets_gltf_texture_hpp