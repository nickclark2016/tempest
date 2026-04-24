#ifndef tempest_assets_default_importers_hpp
#define tempest_assets_default_importers_hpp

#include <tempest/material.hpp>
#include <tempest/texture.hpp>
#include <tempest/vertex.hpp>

namespace tempest::assets
{
    class asset_database;

    auto register_default_importers(asset_database& database, core::mesh_registry* mesh_reg,
                                    core::texture_registry* texture_reg, core::material_registry* material_reg) -> void;
} // namespace tempest::assets

#endif // tempest_assets_default_importers_hpp
