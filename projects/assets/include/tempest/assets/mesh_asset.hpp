#ifndef tempest_assets_mesh_asset_hpp
#define tempest_assets_mesh_asset_hpp

#include <tempest/assets.hpp>
#include <tempest/assets/asset.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace tempest::assets
{
    struct mesh_primitive
    {
        std::uint32_t first_index;
        std::uint32_t index_count;
        std::int32_t material_index;
    };

    class mesh_asset : public asset
    {
      public:
        mesh_asset(std::string_view name) : asset(name)
        {
        }

        std::vector<mesh_primitive> primitives;
        std::uint32_t vertices_handle;
        std::uint32_t indices_handle;
    };
} // namespace tempest::assets

#endif // tempest_assets_mesh_asset_hpp