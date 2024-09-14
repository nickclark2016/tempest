#ifndef tempest_assets_gltf_image_hpp
#define tempest_assets_gltf_image_hpp

#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>

namespace tempest::assets::gltf
{
    struct image
    {
        string uri;
        string mime_type;
        optional<size_t> buffer_view;
        string name;

        // String representation of JSON objects
        string extension;
        string extras;
    };
} // namespace tempest::assets::gltf

#endif // tempest_assets_gltf_image_hpp