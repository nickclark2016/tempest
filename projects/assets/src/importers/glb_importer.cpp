#include "glb_importer.hpp"

#include <stdexcept>
#include <tempest/int.hpp>

namespace tempest::assets
{
    struct glb_header // NOLINT
    {
        uint32_t magic; // Expected Value: 0x46546C67
        uint32_t version;
        uint32_t length;
    };

    enum class glb_chunk_type : uint32_t
    {
        json = 0x4E4F534A,
        bin = 0x004E4942,
    };

    struct glb_chunk_header // NOLINT
    {
        uint32_t chunk_length;
        glb_chunk_type chunk_type;
    };

    struct glb_data_chunk // NOLINT
    {
        glb_chunk_header header;
        span<byte> data;
    };

    glb_importer::glb_importer(core::mesh_registry* mesh_reg, core::texture_registry* texture_reg,
                               core::material_registry* material_reg) noexcept
        : _mesh_reg(mesh_reg), _texture_reg(texture_reg), _material_reg(material_reg)
    {
    }

    auto glb_importer::import([[maybe_unused]] asset_database& asset_db,
                                               [[maybe_unused]] span<const byte> bytes,
                                               [[maybe_unused]] ecs::archetype_registry& registry,
                                               [[maybe_unused]] optional<string_view> path) -> ecs::archetype_entity
    {
        throw std::runtime_error("GLB import not implemented yet.");
    }
} // namespace tempest::assets