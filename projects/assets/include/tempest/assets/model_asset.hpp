#ifndef tempest_assets_model_asset_hpp
#define tempest_assets_model_asset_hpp

#include <tempest/assets.hpp>
#include <tempest/assets/asset.hpp>
#include <tempest/assets/mesh_asset.hpp>

#include <tempest/mat4.hpp>

#include <tempest/vertex.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace tempest::assets
{
    struct model_node
    {
        std::string name;
        model_node* parent;

        std::vector<model_node*> children;

        mesh_asset* mesh;

        math::mat4<float> matrix;
    };

    class model_asset : public asset
    {
      public:
        model_asset(std::string_view name) : asset(name), root(nullptr)
        {
        }

        model_node* root;

        std::uint32_t vertex_count = 0;
        std::uint32_t index_count = 0;

        core::vertex* vertices = nullptr;
        std::uint32_t* indices = nullptr;
    };
} // namespace tempest::assets

#endif // tempest_assets_model_asset_hpp