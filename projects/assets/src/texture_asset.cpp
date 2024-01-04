#include <tempest/texture_asset.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace tempest::assets
{
    std::optional<texture_asset> load_texture(const std::filesystem::path& path)
    {
        auto fp = path.string();
        FILE* f = fopen(fp.c_str(), "rb");
        int width, height, channels;
        bool is_16_bit = stbi_is_16_bit(fp.c_str());

        auto bytes = stbi_load_from_file(f, &width, &height, &channels, 4);

        if (bytes)
        {
            texture_asset asset = {
                .data{std::vector<std::byte>(reinterpret_cast<std::byte*>(bytes),
                                             reinterpret_cast<std::byte*>(bytes + (width * height * 4)))},
                .width{static_cast<std::uint32_t>(width)},
                .height{static_cast<std::uint32_t>(height)},
                .bit_depth{is_16_bit ? 16u : 8u},
            };

            stbi_image_free(bytes);

            return asset;
        }

        return std::nullopt;
    }
} // namespace tempest::assets