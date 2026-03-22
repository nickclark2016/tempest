#include <tempest/default_importers.hpp>

#include <tempest/asset_database.hpp>
#include <tempest/asset_serializers.hpp>
#include <tempest/transform_component.hpp>

#include "importers/exr_importer.hpp"
#include "importers/glb_importer.hpp"
#include "importers/gltf_importer.hpp"

namespace tempest::assets
{
    auto register_default_importers(asset_database& database, core::mesh_registry* mesh_reg,
                                    core::texture_registry* texture_reg, core::material_registry* material_reg) -> void
    {
        // Register ECS component types for entity hierarchy serialization.
        // These must be registered before open() so _load_from_blobs can
        // reconstruct the entity hierarchy with all component data.
        database.register_component<ecs::transform_component>();
        database.register_component<core::mesh_component>();
        database.register_component<core::material_component>();
        database.register_component<asset_metadata_component>();

        // Register core asset types with their serializer/deserializer callbacks so the database
        // can persist and restore blobs across runs.  These must be registered before open() is
        // called so that _load_from_blobs can find the callbacks and restore into the registries.
        auto* type_reg = database.type_registry();
        if (type_reg != nullptr)
        {
            type_reg->register_type<core::mesh>(
                [mesh_reg](span<const byte> blob, const guid& asset_id, asset_database&) -> bool {
                    serialization::binary_archive archive;
                    archive.write(blob);
                    auto mesh =
                        serialization::serializer<serialization::binary_archive, core::mesh>::deserialize(archive);
                    return mesh_reg->register_mesh_with_id(asset_id, tempest::move(mesh));
                },
                [mesh_reg](const guid& asset_id, const asset_database&, vector<byte>& out) -> bool {
                    auto opt = mesh_reg->find(asset_id);
                    if (!opt.has_value())
                    {
                        return false;
                    }
                    serialization::binary_archive archive;
                    serialization::serializer<serialization::binary_archive, core::mesh>::serialize(
                        archive, opt.value());
                    auto data = archive.read(archive.written_size());
                    out.insert(out.end(), data.begin(), data.end());
                    return true;
                });

            type_reg->register_type<core::texture>(
                [texture_reg](span<const byte> blob, const guid& asset_id, asset_database&) -> bool {
                    serialization::binary_archive archive;
                    archive.write(blob);
                    auto tex =
                        serialization::serializer<serialization::binary_archive, core::texture>::deserialize(archive);
                    return texture_reg->register_texture_with_id(asset_id, tempest::move(tex));
                },
                [texture_reg](const guid& asset_id, const asset_database&, vector<byte>& out) -> bool {
                    auto opt = texture_reg->get_texture(asset_id);
                    if (!opt.has_value())
                    {
                        return false;
                    }
                    serialization::binary_archive archive;
                    serialization::serializer<serialization::binary_archive, core::texture>::serialize(
                        archive, opt.value());
                    auto data = archive.read(archive.written_size());
                    out.insert(out.end(), data.begin(), data.end());
                    return true;
                });

            type_reg->register_type<core::material>(
                [material_reg](span<const byte> blob, const guid& asset_id, asset_database&) -> bool {
                    serialization::binary_archive archive;
                    archive.write(blob);
                    auto mat =
                        serialization::serializer<serialization::binary_archive, core::material>::deserialize(
                            archive);
                    return material_reg->register_material_with_id(asset_id, tempest::move(mat));
                },
                [material_reg](const guid& asset_id, const asset_database&, vector<byte>& out) -> bool {
                    auto opt = material_reg->find(asset_id);
                    if (!opt.has_value())
                    {
                        return false;
                    }
                    serialization::binary_archive archive;
                    serialization::serializer<serialization::binary_archive, core::material>::serialize(
                        archive, opt.value());
                    auto data = archive.read(archive.written_size());
                    out.insert(out.end(), data.begin(), data.end());
                    return true;
                });
        }

        database.register_importer(make_unique<gltf_importer>(mesh_reg, texture_reg, material_reg), ".gltf");
        database.register_importer(make_unique<glb_importer>(mesh_reg, texture_reg, material_reg), ".glb");
        database.register_importer(make_unique<exr_importer>(texture_reg), ".exr");
    }
} // namespace tempest::assets
