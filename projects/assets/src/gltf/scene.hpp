#ifndef tempest_assets_gltf_scene_hpp
#define tempest_assets_gltf_scene_hpp

#include <tempest/int.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets::gltf
{
    struct scene
    {
        vector<size_t> nodes;
        string name;

        string extensions;
        string extras;
    };
}

#endif // tempest_assets_gltf_scene_hpp