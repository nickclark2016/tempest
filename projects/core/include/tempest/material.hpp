#ifndef tempest_core_material_hpp
#define tempest_core_material_hpp

#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

namespace tempest::core
{
    class material
    {
      public:
        static constexpr string_view base_color_factor_name = "t__base_color_factor";
        static constexpr string_view base_color_texture_name = "t__base_color_texture";
        static constexpr string_view metallic_factor_name = "t__metallic_factor";
        static constexpr string_view roughness_factor_name = "t__roughness_factor";
        static constexpr string_view metallic_roughness_texture_name = "t__mr_texture";
        static constexpr string_view normal_texture_name = "t__normal_texture";
        static constexpr string_view normal_scale_name = "t__normal_scale";
        static constexpr string_view occlusion_texture_name = "t__occlusion_texture";
        static constexpr string_view occlusion_strength_name = "t__occlusion_strength";
        static constexpr string_view emissive_texture_name = "t__emissive_texture";
        static constexpr string_view emissive_factor_name = "t__emissive_factor";
        static constexpr string_view alpha_mode_name = "t__alpha_mode";
        static constexpr string_view alpha_cutoff_name = "t__alpha_cutoff";
        static constexpr string_view double_sided_name = "t__double_sided";
        static constexpr string_view transmissive_factor_name = "t__transmissive_factor";
        static constexpr string_view transmissive_texture_name = "t__transmissive_texture";
        static constexpr string_view volume_attenuation_color_name = "t__volume_attenuation_color";
        static constexpr string_view volume_thickness_factor_name = "t__volume_thickness_factor";
        static constexpr string_view volume_thickness_texture_name = "t__volume_thickness_texture";
        static constexpr string_view volume_attenuation_distance_name = "t__volume_attenuation_distance";


        void set_name(string name);
        void set_texture(string_view name, guid id);
        void set_scalar(string_view name, float scalar);
        void set_bool(string_view name, bool value);
        void set_vec2(string_view name, math::vec2<float> vec);
        void set_vec3(string_view name, math::vec3<float> vec);
        void set_vec4(string_view name, math::vec4<float> vec);
        void set_string(string_view name, string value);

        string_view get_name() const noexcept;
        optional<guid> get_texture(string_view name) const;
        optional<float> get_scalar(string_view name) const;
        optional<bool> get_bool(string_view name) const;
        optional<math::vec2<float>> get_vec2(string_view name) const;
        optional<math::vec3<float>> get_vec3(string_view name) const;
        optional<math::vec4<float>> get_vec4(string_view name) const;
        optional<string_view> get_string(string_view name) const;

      private:
        string _name;
        flat_unordered_map<string, guid> _textures;
        flat_unordered_map<string, float> _scalars;
        flat_unordered_map<string, bool> _bools;
        flat_unordered_map<string, math::vec2<float>> _vec2s;
        flat_unordered_map<string, math::vec3<float>> _vec3s;
        flat_unordered_map<string, math::vec4<float>> _vec4s;
        flat_unordered_map<string, string> _strings;
    };

    class material_registry
    {
      public:
        material_registry() = default;
        ~material_registry() = default;

        material_registry(const material_registry&) = delete;
        material_registry(material_registry&&) = delete;

        material_registry& operator=(const material_registry&) = delete;
        material_registry& operator=(material_registry&&) = delete;

        [[nodiscard]] guid register_material(material&& mat);
        [[nodiscard]] bool register_material_with_id(const guid& id, material&& mat);

        optional<const material&> get_material(guid id) const;

        template <invocable<material&> Fn>
        void update_material(const guid& id, Fn&& fn);

      private:
        flat_unordered_map<guid, material> _materials;
    };

    struct material_component
    {
        guid material_id;
    };

    template <invocable<material&> Fn>
    void material_registry::update_material(const guid& id, Fn&& fn)
    {
        if (auto it = _materials.find(id); it != _materials.end())
        {
            fn(it->second);
        }
    }
} // namespace tempest::core

#endif // tempest_core_material_hpp