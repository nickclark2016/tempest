#ifndef tempest_assets_gltf_gltf_hpp
#define tempest_assets_gltf_gltf_hpp

#include "accessor.hpp"
#include "asset.hpp"
#include "buffer.hpp"
#include "buffer_view.hpp"
#include "image.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "node.hpp"
#include "sampler.hpp"
#include "scene.hpp"
#include "texture.hpp"

#include <tempest/int.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets::gltf
{
    struct gltf
    {
        vector<string> extensions_used;
        vector<string> extensions_required;
        
        vector<accessor> accessors;
        asset asset;
        vector<buffer> buffers;
        vector<buffer_view> buffer_views;
        vector<image> images;
        vector<material> materials;
        vector<mesh> meshes;
        vector<node> nodes;
        vector<sampler> samplers;
        vector<scene> scenes;
        size_t default_scene;
        vector<texture> textures;

        // String representation of JSON objects
        string extensions;
        string extras;
    };
}

#endif // tempest_assets_gltf_gltf_hpp