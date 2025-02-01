#include <tempest/asset_database.hpp>

#include "importers/gltf_importer.hpp"

namespace tempest::assets
{
    asset_database::asset_database(core::mesh_registry* mesh_reg, core::texture_registry* texture_reg,
                                   core::material_registry* material_reg) noexcept
        : _mesh_reg{mesh_reg}, _texture_reg{texture_reg}, _material_reg{material_reg}
    {
        register_importer(make_unique<gltf_importer>(_mesh_reg, _texture_reg, _material_reg), ".gltf");
    }

    void asset_database::register_importer(unique_ptr<asset_importer> importer, string_view extension)
    {
        _importers[string(extension)] = move(importer);
    }

    ecs::archetype_entity asset_database::import(string_view path, ecs::archetype_registry& registry)
    {
        auto extension_it = search_last_of(path, '.');
        if (extension_it == path.end())
        {
            return ecs::tombstone;
        }

        auto importer_it = _importers.find(string(extension_it, path.end()));
        if (importer_it != _importers.end())
        {
            auto ent = importer_it->second->import(*this, path, registry);
            if (!registry.has<prefab_tag_t>(ent))
            {
                registry.assign(ent, prefab_tag);
            }
            return ent;
        }

        return ecs::tombstone;
    }

    guid asset_database::register_asset_metadata(asset_metadata meta)
    {
        guid g;
        do
        {
            g = guid::generate_random_guid();
        } while (_metadata.find(g) != _metadata.end());

        _metadata.insert({move(g), move(meta)});
        return g;
    }

    optional<const asset_database::asset_metadata&> asset_database::get_asset_metadata(guid id) const
    {
        if (auto it = _metadata.find(id); it != _metadata.end())
        {
            return it->second;
        }
        return none();
    }
} // namespace tempest::assets