#ifndef tempest_render_system_material_asset_hpp
#define tempest_render_system_material_asset_hpp

#include <tempest/api.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/material.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

namespace tempest::assets
{
    struct TEMPEST_API material_asset
    {
        static constexpr string_view default_shading_model = "openpbr";
        static constexpr math::vec4<float> default_base_color_factor = {1.0f, 1.0f, 1.0f, 1.0f};
        static constexpr float default_roughness_factor = 0.5f;
        static constexpr float default_metallic_factor = 0.0f;
        static constexpr float default_transmission_factor = 0.0f;
        static constexpr math::vec3<float> default_emissive_factor = {0.0f, 0.0f, 0.0f};
        static constexpr float default_normal_scale = 1.0f;
        static constexpr float default_occlusion_strength = 1.0f;
        static constexpr float default_alpha_cutoff = 0.5f;
        static constexpr string_view default_alpha_mode = "opaque";
        static constexpr bool default_double_sided = false;

        string name = {};
        string shading_model = string{default_shading_model};

        math::vec4<float> base_color_factor = default_base_color_factor;
        float roughness_factor = default_roughness_factor;
        float metallic_factor = default_metallic_factor;
        float transmission_factor = default_transmission_factor;
        math::vec3<float> emissive_factor = default_emissive_factor;
        float normal_scale = default_normal_scale;
        float occlusion_strength = default_occlusion_strength;
        float alpha_cutoff = default_alpha_cutoff;
        string alpha_mode = string{default_alpha_mode};
        bool double_sided = default_double_sided;

        optional<guid> base_color_map = nullopt;
        optional<guid> metallic_roughness_map = nullopt;
        optional<guid> normal_map = nullopt;
        optional<guid> occlusion_map = nullopt;
        optional<guid> emissive_map = nullopt;
        optional<guid> transmission_map = nullopt;

        flat_unordered_map<string, float> custom_scalars = {};
        flat_unordered_map<string, math::vec4<float>> custom_vec4s = {};
        flat_unordered_map<string, guid> custom_textures = {};

        [[nodiscard]] auto to_core_material() const -> core::material;
        [[nodiscard]] static auto from_core_material(const core::material& mat) -> material_asset;

        [[nodiscard]] auto to_string() const -> string;
        [[nodiscard]] static auto load_from_string(string_view json_str) -> optional<material_asset>;

        [[nodiscard]] auto save_to_file(string_view path) const -> bool;
        [[nodiscard]] static auto load_from_file(string_view path) -> optional<material_asset>;

        auto operator==(const material_asset& rhs) const noexcept -> bool = default;
    };
} // namespace tempest::assets

#endif // tempest_render_system_material_asset_hpp
