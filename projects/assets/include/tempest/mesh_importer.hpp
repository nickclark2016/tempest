#ifndef tempest_assets_mesh_importer_hpp
#define tempest_assets_mesh_importer_hpp

#include <tempest/asset_importer.hpp>

namespace tempest::assets
{
    class mesh_importer : public asset_importer
    {
      public:
        mesh_importer() = default;
        mesh_importer(const mesh_importer&) = delete;
        mesh_importer(mesh_importer&&) noexcept = delete;
        virtual ~mesh_importer() = default;

        mesh_importer& operator=(const mesh_importer&) = delete;
        mesh_importer& operator=(mesh_importer&&) noexcept = delete;

        unique_ptr<asset> load(span<const byte> data) const override;
    };
}

#endif // tempest_assets_mesh_importer_hpp