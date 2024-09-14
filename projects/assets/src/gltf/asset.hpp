#ifndef tempest_assets_gltf_asset_hpp
#define tempest_assets_gltf_asset_hpp

#include <tempest/string.hpp>

namespace tempest::assets::gltf
{
    struct asset
    {
        string version;
        string generator;
        string copyright;
    };
}

#endif // tempest_assets_gltf_asset_hpp