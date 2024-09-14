#ifndef tempest_assets_material_hpp
#define tempest_assets_material_hpp

#include <tempest/asset.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

namespace tempest::assets
{
    class material : public asset
    {
      public:
        static constexpr string_view base_color = "t__base_color";
        static constexpr string_view base_color_factor = "t__base_color_factor";
        static constexpr string_view metallic_roughness = "t__metallic_roughness";
        static constexpr string_view metallic_factor = "t__metallic_factor";
        static constexpr string_view roughness_factor = "t__roughness_factor";
        static constexpr string_view normal = "t__normal";
        static constexpr string_view normal_scale = "t__normal_scale";
        static constexpr string_view occlusion = "t__occlusion";
        static constexpr string_view occlusion_strength = "t__occlusion_strength";
        static constexpr string_view emissive = "t__emissive";
        static constexpr string_view emissive_factor = "t__emissive_factor";
        static constexpr string_view alpha_mode = "t__alpha_mode";
        static constexpr string_view alpha_cutoff = "t__alpha_cutoff";
        static constexpr string_view double_sided = "t__double_sided";

        explicit material(string name);
        material(const material&) = delete;
        material(material&&) noexcept = delete;
        virtual ~material() = default;

        material& operator=(const material&) = delete;
        material& operator=(material&&) noexcept = delete;

        string_view name() const noexcept override;
        guid id() const noexcept override;

        void add_texture(string name, guid texture_id);
        optional<guid> get_texture(string name) const;

        void add_float(string name, float value);
        optional<float> get_float(string name) const;

        void add_vec2(string name, math::vec2<float> value);
        optional<math::vec2<float>> get_vec2(string name) const;

        void add_vec3(string name, math::vec3<float> value);
        optional<math::vec3<float>> get_vec3(string name) const;

        void add_vec4(string name, math::vec4<float> value);
        optional<math::vec4<float>> get_vec4(string name) const;

        void add_bool(string name, bool value);
        optional<bool> get_bool(string name) const;

        void add_string(string name, string value);
        optional<string> get_string(string name) const;

      private:
        guid _id;
        string _name;

        flat_unordered_map<string, guid> _textures;
        flat_unordered_map<string, float> _floats;
        flat_unordered_map<string, math::vec2<float>> _vec2s;
        flat_unordered_map<string, math::vec3<float>> _vec3s;
        flat_unordered_map<string, math::vec4<float>> _vec4s;
        flat_unordered_map<string, bool> _bools;
        flat_unordered_map<string, string> _strings;
    };

    inline material::material(string name) : _id(guid::generate_random_guid()), _name(tempest::move(name))
    {
    }

    inline string_view material::name() const noexcept
    {
        return _name;
    }

    inline guid material::id() const noexcept
    {
        return _id;
    }

    inline void material::add_texture(string name, guid texture_id)
    {
        _textures[tempest::move(name)] = texture_id;
    }

    inline optional<guid> material::get_texture(string name) const
    {
        auto it = _textures.find(name);
        if (it != _textures.end())
        {
            return it->second;
        }

        return nullopt;
    }

    inline void material::add_float(string name, float value)
    {
        _floats[tempest::move(name)] = value;
    }

    inline optional<float> material::get_float(string name) const
    {
        auto it = _floats.find(name);
        if (it != _floats.end())
        {
            return it->second;
        }

        return nullopt;
    }

    inline void material::add_vec2(string name, math::vec2<float> value)
    {
        _vec2s[tempest::move(name)] = value;
    }

    inline optional<math::vec2<float>> material::get_vec2(string name) const
    {
        auto it = _vec2s.find(name);
        if (it != _vec2s.end())
        {
            return it->second;
        }

        return nullopt;
    }

    inline void material::add_vec3(string name, math::vec3<float> value)
    {
        _vec3s[tempest::move(name)] = value;
    }

    inline optional<math::vec3<float>> material::get_vec3(string name) const
    {
        auto it = _vec3s.find(name);
        if (it != _vec3s.end())
        {
            return it->second;
        }

        return nullopt;
    }

    inline void material::add_vec4(string name, math::vec4<float> value)
    {
        _vec4s[tempest::move(name)] = value;
    }

    inline optional<math::vec4<float>> material::get_vec4(string name) const
    {
        auto it = _vec4s.find(name);
        if (it != _vec4s.end())
        {
            return it->second;
        }

        return nullopt;
    }

    inline void material::add_bool(string name, bool value)
    {
        _bools[tempest::move(name)] = value;
    }

    inline optional<bool> material::get_bool(string name) const
    {
        auto it = _bools.find(name);
        if (it != _bools.end())
        {
            return it->second;
        }

        return nullopt;
    }

    inline void material::add_string(string name, string value)
    {
        _strings[tempest::move(name)] = tempest::move(value);
    }

    inline optional<string> material::get_string(string name) const
    {
        auto it = _strings.find(name);
        if (it != _strings.end())
        {
            return it->second;
        }

        return nullopt;
    }
} // namespace tempest::assets

#endif // tempest_assets_material_hpp