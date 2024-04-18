#ifndef tempest_assets_texture_asset_hpp
#define tempest_assets_texture_asset_hpp

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

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
        std::vector<std::byte> data;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t bit_depth;
        std::uint32_t channels;
        std::uint32_t mipmaps;
        texture_asset_type type;
    };

    std::optional<texture_asset> load_texture(const std::filesystem::path& path);
}

#endif // tempest_assets_texture_asset_hpp