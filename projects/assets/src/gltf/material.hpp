#ifndef tempest_assets_gltf_material_hpp
#define tempest_assets_gltf_material_hpp

#include <tempest/array.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>

namespace tempest::assets::gltf
{
    struct texture_info
    {
        size_t index;
        size_t tex_coord = 0;

        // String representation of JSON objects
        string extension;
        string extras;
    };

    struct normal_texture_info
    {
        size_t index;
        size_t tex_coord = 0;
        double scale = 1.0;

        // String representation of JSON objects
        string extension;
        string extras;
    };

    struct occlusion_texture_info
    {
        size_t index;
        size_t tex_coord = 0;
        double strength = 1.0;

        // String representation of JSON objects
        string extension;
        string extras;
    };

    struct pbr_metallic_roughness
    {
        array<double, 4> base_color_factor = {1.0, 1.0, 1.0, 1.0};
        optional<texture_info> base_color_texture = none();
        double metallic_factor = 1.0;
        double roughness_factor = 1.0;
        optional<texture_info> metallic_roughness_texture = none();

        // String representation of JSON objects
        string extension;
        string extras;
    };

    struct material
    {
        string name;
        optional<pbr_metallic_roughness> pbr_metallic_roughness = none();
        optional<normal_texture_info> normal_texture = none();
        optional<occlusion_texture_info> occlusion_texture = none();
        optional<texture_info> emissive_texture = none();
        array<double, 3> emissive_factor = {0.0, 0.0, 0.0};
        string alpha_mode = "OPAQUE";
        double alpha_cutoff = 0.5;
        bool double_sided = false;

        // String representation of JSON objects
        string extension;
        string extras;
    };
} // namespace tempest::assets::gltf

#endif // tempest_assets_gltf_material_hpp