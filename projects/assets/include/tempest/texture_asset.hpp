#ifndef tempest_assets_texture_asset_hpp
#define tempest_assets_texture_asset_hpp

#include <tempest/int.hpp>
#include <tempest/vector.hpp>

#include <filesystem>
#include <optional>

namespace tempest::assets
{
    enum class texture_asset_type
    {
        LINEAR,
        SRGB,
        HDRI,
    };

    struct texture_asset
    {
        vector<byte> data;
        uint32_t width;
        uint32_t height;
        uint32_t bit_depth;
        uint32_t channels;
        uint32_t mipmaps;
        texture_asset_type type;
    };

    std::optional<texture_asset> load_texture(const std::filesystem::path& path);
}

#endif // tempest_assets_texture_asset_hpp