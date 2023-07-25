#ifndef tempest_assets_model_asset_hpp
#define tempest_assets_model_asset_hpp

#include <tempest/assets.hpp>
#include <tempest/assets/asset.hpp>
#include <tempest/assets/mesh_asset.hpp>

#include <tempest/math/mat4.hpp>

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
    };
} // namespace tempest::assets

#endif // tempest_assets_model_asset_hpp