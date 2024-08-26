#include <tempest/asset_database.hpp>

namespace tempest::assets
{
    asset_database::asset_database(string root_path) : _root_path(std::move(root_path))
    {
    }
}