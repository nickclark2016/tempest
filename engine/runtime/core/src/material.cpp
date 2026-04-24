#include <tempest/material.hpp>

namespace tempest::core
{
    void material::set_name(string name)
    {
        _name = move(name);
    }

    void material::set_texture(string_view name, guid id)
    {
        _textures[string(name)] = id;
    }

    void material::set_scalar(string_view name, float scalar)
    {
        _scalars[string(name)] = scalar;
    }

    void material::set_bool(string_view name, bool value)
    {
        _bools[string(name)] = value;
    }

    void material::set_vec2(string_view name, math::vec2<float> vec)
    {
        _vec2s[string(name)] = vec;
    }

    void material::set_vec3(string_view name, math::vec3<float> vec)
    {
        _vec3s[string(name)] = vec;
    }

    void material::set_vec4(string_view name, math::vec4<float> vec)
    {
        _vec4s[string(name)] = vec;
    }

    void material::set_string(string_view name, string value)
    {
        _strings[string(name)] = move(value);
    }

    string_view material::get_name() const noexcept
    {
        return _name;
    }

    optional<guid> material::get_texture(string_view name) const
    {
        if (auto it = _textures.find(string(name)); it != _textures.end())
        {
            return it->second;
        }
        return none();
    }

    optional<float> material::get_scalar(string_view name) const
    {
        if (auto it = _scalars.find(string(name)); it != _scalars.end())
        {
            return it->second;
        }
        return none();
    }

    optional<bool> material::get_bool(string_view name) const
    {
        if (auto it = _bools.find(string(name)); it != _bools.end())
        {
            return it->second;
        }
        return none();
    }

    optional<math::vec2<float>> material::get_vec2(string_view name) const
    {
        if (auto it = _vec2s.find(string(name)); it != _vec2s.end())
        {
            return it->second;
        }
        return none();
    }

    optional<math::vec3<float>> material::get_vec3(string_view name) const
    {
        if (auto it = _vec3s.find(string(name)); it != _vec3s.end())
        {
            return it->second;
        }
        return none();
    }

    optional<math::vec4<float>> material::get_vec4(string_view name) const
    {
        if (auto it = _vec4s.find(string(name)); it != _vec4s.end())
        {
            return it->second;
        }
        return none();
    }

    optional<string_view> material::get_string(string_view name) const
    {
        if (auto it = _strings.find(string(name)); it != _strings.end())
        {
            return it->second;
        }
        return none();
    }

    guid material_registry::register_material(material&& mat)
    {
        guid g;
        do
        {
            g = guid::generate_random_guid();
        } while (_materials.find(g) != _materials.end());

        _materials.insert({move(g), move(mat)});
        return g;
    }

    bool material_registry::register_material_with_id(const guid& id, material&& mat)
    {
        if (_materials.find(id) != _materials.end())
        {
            return false;
        }

        _materials.insert({id, move(mat)});
        return true;
    }

    optional<const material&> material_registry::find(guid id) const
    {
        if (auto it = _materials.find(id); it != _materials.end())
        {
            return it->second;
        }
        return none();
    }
} // namespace tempest::core