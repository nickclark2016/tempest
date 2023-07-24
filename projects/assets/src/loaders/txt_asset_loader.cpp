#include <tempest/loaders/txt_asset_loader.hpp>

#include <tempest/assets/txt_asset.hpp>

#include <string>
#include <fstream>
#include <streambuf>

namespace tempest::assets
{
    bool txt_asset_loader::load(const std::filesystem::path& path, void* dest)
    {
        assert(std::filesystem::is_regular_file(path) && "Path does not direct to a regular file.");

        std::ifstream file(path.c_str());
        std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        txt_asset* asset = std::construct_at(reinterpret_cast<txt_asset*>(dest), path.string());
        asset->data = data;

        return true;
    }

    bool txt_asset_loader::release(std::unique_ptr<asset>&& asset)
    {
        return false;
    }
} // namespace tempest::assets