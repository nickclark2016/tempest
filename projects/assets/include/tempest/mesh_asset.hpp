#ifndef tempest_assets_mesh_asset_hpp
#define tempest_assets_mesh_asset_hpp

#include "texture_asset.hpp"

#include <tempest/vertex.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace tempest::assets
{
    struct mesh_asset
    {
        core::mesh mesh;
        std::uint32_t material_id;
    };

    struct scene_asset_node
    {
        std::uint32_t parent = std::numeric_limits<std::uint32_t>::max();
        std::vector<std::uint32_t> children;
        std::uint32_t mesh_id;
        math::vec3<float> position;
        math::vec3<float> rotation;
        math::vec3<float> scale;
        std::string name;
    };

    enum class material_type
    {
        OPAQUE,
        MASK,
        BLEND,
    };

    struct material_asset
    {
        std::string name;
        material_type type;
        std::uint32_t base_color_texture = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t normal_map_texture = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t metallic_roughness_texture = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t occlusion_map_texture = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t emissive_map_texture = std::numeric_limits<std::uint32_t>::max();
        math::vec4<float> base_color_factor;
    };

    struct scene_asset
    {
        std::vector<scene_asset_node> nodes;
        std::vector<mesh_asset> meshes;
        std::vector<material_asset> materials;
        std::vector<texture_asset> textures;
    };

    std::optional<scene_asset> load_scene(const std::filesystem::path& path);
}

#endif // tempest_assets_mesh_asset_hpp