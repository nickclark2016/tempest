#include <tempest/asset_database.hpp>

#include "importers/exr_importer.hpp"
#include "importers/gltf_importer.hpp"

namespace tempest::assets
{
    asset_database::asset_database(core::mesh_registry* mesh_reg, core::texture_registry* texture_reg,
                                   core::material_registry* material_reg) noexcept
        : _mesh_reg{mesh_reg}, _texture_reg{texture_reg}, _material_reg{material_reg}
    {
        register_importer(make_unique<gltf_importer>(_mesh_reg, _texture_reg, _material_reg), ".gltf");
        register_importer(make_unique<exr_importer>(_texture_reg), ".exr");
    }

    void asset_database::register_importer(unique_ptr<asset_importer> importer, string_view extension)
    {
        _importers[string(extension)] = move(importer);
    }

    auto asset_database::import(string_view path, ecs::archetype_registry& registry) -> ecs::archetype_entity
    {
        const auto* extension_it = search_last_of(path, '.');
        if (extension_it == path.end())
        {
            return ecs::tombstone;
        }

        auto importer_it = _importers.find(string(extension_it, path.end()));
        if (importer_it != _importers.end())
        {
            const auto ent = importer_it->second->import(*this, path, registry);
            if (!registry.has<prefab_tag_t>(ent))
            {
                registry.assign(ent, prefab_tag);
            }
            return ent;
        }

        return ecs::tombstone;
    }

    auto asset_database::register_asset_metadata(asset_metadata meta) -> guid
    {
        auto unique_id = guid{};
        do
        {
            unique_id = guid::generate_random_guid();
        } while (_metadata.contains(unique_id));

        _metadata.insert({move(unique_id), move(meta)});
        return unique_id;
    }

    auto asset_database::get_asset_metadata(guid asset_id) const -> optional<const asset_database::asset_metadata&>
    {
        if (auto iter = _metadata.find(asset_id); iter != _metadata.end())
        {
            return iter->second;
        }
        return none();
    }
} // namespace tempest::assets