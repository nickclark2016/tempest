#include <tempest/texture.hpp>

namespace tempest::core
{
    guid texture_registry::register_texture(texture&& tex)
    {
        guid g;
        do
        {
            g = guid::generate_random_guid();
        } while (_textures.find(g) != _textures.end());

        _textures.insert({move(g), move(tex)});
        return g;
    }

    bool texture_registry::register_texture_with_id(const guid& id, texture&& tex)
    {
        if (_textures.find(id) != _textures.end())
        {
            return false;
        }

        _textures.insert({id, move(tex)});
        return true;
    }

    optional<const texture&> texture_registry::get_texture(guid id) const
    {
        if (auto it = _textures.find(id); it != _textures.end())
        {
            return it->second;
        }
        return none();
    }
} // namespace tempest::core