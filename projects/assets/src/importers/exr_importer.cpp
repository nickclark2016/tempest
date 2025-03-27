#include "exr_importer.hpp"

#include <tempest/asset_database.hpp>
#include <tempest/logger.hpp>

#include <tinyexr/tinyexr.h>

namespace tempest::assets
{
    namespace
    {
        auto log = logger::logger_factory::create({
            .prefix = "tempest::exr_importer",
        });
    }

    exr_importer::exr_importer(core::texture_registry* tex_reg) : _texture_reg(tex_reg)
    {
    }

    ecs::archetype_entity exr_importer::import(asset_database& db, span<const byte> data,
                                               ecs::archetype_registry& registry, optional<string_view> path)
    {
        float* image_data = nullptr;
        int width = 0;
        int height = 0;
        const char* err;

        int result = LoadEXRFromMemory(&image_data, &width, &height,
                                       reinterpret_cast<const unsigned char*>(data.data()), data.size(), &err);

        if (result < 0)
        {
            log->error("Failed to load EXR image: {}", err);
            return ecs::tombstone;
        }

        core::texture_mip_data mip = {
            .data = vector<byte>(reinterpret_cast<byte*>(image_data),
                                 reinterpret_cast<byte*>(image_data) + (width * height * sizeof(float))),
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
        };

        core::texture tex = {
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .format = core::texture_format::RGBA32_FLOAT,
            .compression = core::texture_compression::NONE,
        };

        tex.mips.push_back(tempest::move(mip));

        auto id = _texture_reg->register_texture(tempest::move(tex));

        if (image_data)
        {
            free(image_data);
        }

        auto ent = registry.create<core::texture_component>();
        registry.get<core::texture_component>(ent).texture_id = id;

        asset_database::asset_metadata meta = {
            .path = path ? *path : "EXR of Unknown Origin",
            .metadata = {},
        };

        auto meta_id = db.register_asset_metadata(meta);
        
        asset_metadata_component meta_comp{
            .metadata_id = meta_id,
        };

        registry.assign(ent, meta_comp);

        return ent;
    }
} // namespace tempest::assets