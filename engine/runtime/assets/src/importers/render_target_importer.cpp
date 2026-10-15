#include "render_target_importer.hpp"

#include <tempest/asset_database.hpp>
#include <tempest/guid.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_target_asset.hpp>

namespace tempest::assets
{
    render_target_importer::render_target_importer(core::texture_registry* tex_reg)
        : _texture_reg{tex_reg}
    {
    }

    auto render_target_importer::import(asset_database& db, span<const byte> data,
                                        ecs::archetype_registry& registry, optional<string_view> path) -> ecs::entity
    {
        const auto json_str = string_view{reinterpret_cast<const char*>(data.data()), data.size()};
        const auto target_opt = render_target_asset::load_from_string(json_str);
        if (!target_opt.has_value())
        {
            return ecs::tombstone;
        }

        auto asset_id = guid{};
        if (path.has_value())
        {
            asset_id = db.resolve_sub_asset_guid(*path, "root", asset_type_id::of<render_target_asset>());
        }
        else
        {
            asset_id = guid::generate_random_guid();
            db.register_asset_with_guid(asset_id, asset_type_id::of<render_target_asset>(), "memory://render_target");
        }

        db.store_blob(asset_id, data);

        const auto tex_id = target_opt->get_texture_id(asset_id);
        if (_texture_reg != nullptr)
        {
            auto tex = target_opt->to_core_texture();
            [[maybe_unused]] const auto registered = _texture_reg->register_texture_with_id(tex_id, tempest::move(tex));
        }

        const auto ent = registry.create_initialized<core::texture_component>(core::texture_component{
            .texture_id = tex_id,
        });

        const auto meta = asset_database::asset_metadata{
            .path = path.has_value() ? string{*path} : string{"RenderTarget"},
            .metadata = {},
        };
        const auto meta_id = db.register_asset_metadata(meta);

        const auto meta_comp = asset_metadata_component{
            .metadata_id = meta_id,
        };
        registry.assign(ent, meta_comp);

        return ent;
    }
} // namespace tempest::assets
