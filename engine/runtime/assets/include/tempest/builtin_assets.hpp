#ifndef tempest_assets_builtin_assets_hpp
#define tempest_assets_builtin_assets_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/material.hpp>
#include <tempest/texture.hpp>
#include <tempest/vertex.hpp>

namespace tempest::assets::builtin
{
    // =========================================================================
    // Deterministic Built-in GUID Identifiers
    // =========================================================================

    inline constexpr guid plane_mesh_guid{
        array<byte, 16>{byte{0xB0}, byte{0x01}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x01}}};

    inline constexpr guid box_mesh_guid{
        array<byte, 16>{byte{0xB0}, byte{0x01}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x02}}};

    inline constexpr guid sphere_mesh_guid{
        array<byte, 16>{byte{0xB0}, byte{0x01}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x03}}};

    inline constexpr guid capsule_mesh_guid{
        array<byte, 16>{byte{0xB0}, byte{0x01}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x04}}};

    inline constexpr guid cylinder_mesh_guid{
        array<byte, 16>{byte{0xB0}, byte{0x01}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x05}}};

    inline constexpr guid missing_texture_guid{
        array<byte, 16>{byte{0xB0}, byte{0x02}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x01}}};

    inline constexpr guid white_texture_guid{
        array<byte, 16>{byte{0xB0}, byte{0x02}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x02}}};

    inline constexpr guid normal_flat_texture_guid{
        array<byte, 16>{byte{0xB0}, byte{0x02}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x03}}};

    inline constexpr guid default_material_guid{
        array<byte, 16>{byte{0xB0}, byte{0x03}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x01}}};

    inline constexpr guid missing_material_guid{
        array<byte, 16>{byte{0xB0}, byte{0x03}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00}, byte{0x00},
                        byte{0x00}, byte{0x02}}};

    // =========================================================================
    // Canonical Geometry Generators
    // =========================================================================

    [[nodiscard]] TEMPEST_API auto create_plane_mesh(float size = 1.0f) -> core::mesh;
    [[nodiscard]] TEMPEST_API auto create_box_mesh(float size = 1.0f) -> core::mesh;
    [[nodiscard]] TEMPEST_API auto create_sphere_mesh(float radius = 0.5f, uint32_t rings = 16, uint32_t sectors = 32)
        -> core::mesh;
    [[nodiscard]] TEMPEST_API auto create_capsule_mesh(float radius = 0.25f, float height = 1.0f, uint32_t rings = 8,
                                                       uint32_t sectors = 16) -> core::mesh;
    [[nodiscard]] TEMPEST_API auto create_cylinder_mesh(float radius = 0.5f, float height = 1.0f,
                                                        uint32_t sectors = 32) -> core::mesh;

    // =========================================================================
    // Canonical Texture Generators
    // =========================================================================

    [[nodiscard]] TEMPEST_API auto create_missing_texture() -> core::texture;
    [[nodiscard]] TEMPEST_API auto create_white_texture() -> core::texture;
    [[nodiscard]] TEMPEST_API auto create_normal_flat_texture() -> core::texture;

    // =========================================================================
    // Canonical Material Generators
    // =========================================================================

    [[nodiscard]] TEMPEST_API auto create_default_material() -> core::material;
    [[nodiscard]] TEMPEST_API auto create_missing_material() -> core::material;

    // =========================================================================
    // Registration
    // =========================================================================

    TEMPEST_API auto register_builtin_assets(asset_database& database, core::mesh_registry* mesh_reg = nullptr,
                                             core::texture_registry* texture_reg = nullptr,
                                             core::material_registry* material_reg = nullptr) -> void;
} // namespace tempest::assets::builtin

#endif // tempest_assets_builtin_assets_hpp
