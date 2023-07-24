#include <gtest/gtest.h>

#include <tempest/assets.hpp>
#include <tempest/loaders/txt_asset_loader.hpp>
#include <tempest/assets/txt_asset.hpp>

using namespace tempest::assets;

TEST(asset_loader, load_asset)
{
    asset_manager man;
    man.register_loader<txt_asset_loader>();

    man.load<txt_asset>("tests/assets/loading_test.txt");
}