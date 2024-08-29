#ifndef tempest_asset_prefab_hpp
#define tempest_asset_prefab_hpp

#include <tempest/asset.hpp>
#include <tempest/memory.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets
{
    struct prefab
    {
        string name;
        vector<unique_ptr<asset>> assets;
    };
}

#endif // tempest_asset_prefab_hpp