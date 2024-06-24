#include <tempest/texture_asset.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace tempest::assets
{
    std::optional<texture_asset> load_texture(const std::filesystem::path& path)
    {
        auto fp = path.string();
#ifdef _WIN32
        FILE* f;
        fopen_s(&f, fp.c_str(), "rb");
#else
        FILE* f = fopen(fp.c_str(), "rb");
#endif
        int width, height, channels;
        bool is_16_bit = stbi_is_16_bit(fp.c_str());

        auto bytes = is_16_bit ? reinterpret_cast<byte*>(stbi_load_from_file_16(f, &width, &height, &channels, 4))
                               : reinterpret_cast<byte*>(stbi_load_from_file(f, &width, &height, &channels, 4));

        if (bytes)
        {
            texture_asset asset = {
                .data =
                    vector<byte>(reinterpret_cast<byte*>(bytes), reinterpret_cast<byte*>(bytes + (width * height * 4))),
                .width = static_cast<uint32_t>(width),
                .height = static_cast<uint32_t>(height),
                .bit_depth = is_16_bit ? 16u : 8u,
                .channels = 4u,
                .mipmaps = 1u,
            };

            stbi_image_free(bytes);

            return asset;
        }

        return std::nullopt;
    }
} // namespace tempest::assets