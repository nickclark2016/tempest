#ifndef tempest_assets_importers_gltf_importer_hpp
#define tempest_assets_importers_gltf_importer_hpp

#include <tempest/api.hpp>
#include <tempest/asset_importer.hpp>
#include <tempest/material.hpp>
#include <tempest/span.hpp>
#include <tempest/texture.hpp>
#include <tempest/vector.hpp>
#include <tempest/vertex.hpp>

namespace tempest::assets
{
    /// @brief Decodes an RFC 4648 standard base64 payload or glTF data URI.
    /// @param data Input byte span containing raw base64 ASCII or a "data:[<mediatype>][;base64],..." URI.
    /// @return Decoded binary data as a vector of bytes, or an empty vector on invalid characters/padding.
    TEMPEST_API auto parse_base64(span<const byte> data) -> vector<byte>;

    class gltf_importer : public asset_importer
    {
      public:
        gltf_importer(core::mesh_registry* mesh_reg, core::texture_registry* texture_reg, core::material_registry* material_reg) noexcept;

        [[nodiscard]] auto importer_name() const noexcept -> string_view override
        {
            return "model";
        }

        auto import(asset_database& asset_db, span<const byte> bytes, ecs::archetype_registry& registry, optional<string_view> path) -> ecs::entity override;

      private:
        core::mesh_registry* _mesh_reg;
        core::texture_registry* _texture_reg;
        core::material_registry* _material_reg;
    };
} // namespace tempest::assets

#endif // tempest_assets_importers_gltf_importer_hpp
