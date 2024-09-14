#ifndef tempest_assets_texture_hpp
#define tempest_assets_texture_hpp

#include <tempest/asset.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets
{
    enum class texture_format
    {
        RGBA8_UINT,
        RGBA8_SRGB,
        RGBA16_UINT,
        RGBA16_SRGB,
        D32_FLOAT,
        RGB_FLOAT_BC6,
        RGBA_UNORM_BC7,
        RGBA_SRGB_BC7,
    };

    enum class sampler_filter
    {
        NEAREST,
        LINEAR,
        NEAREST_MIPMAP_NEAREST,
        LINEAR_MIPMAP_NEAREST,
        NEAREST_MIPMAP_LINEAR,
        LINEAR_MIPMAP_LINEAR,
    };

    enum class sampler_wrap
    {
        REPEAT,
        MIRRORED_REPEAT,
        CLAMP_TO_EDGE,
        CLAMP_TO_BORDER,
    };

    class texture : public asset
    {
      public:
        struct sampler_state
        {
            sampler_filter min_filter;
            sampler_filter mag_filter;
            sampler_wrap wrap_s;
            sampler_wrap wrap_t;
        };

        explicit texture(string name);
        texture(const texture&) = delete;
        texture(texture&&) noexcept = delete;
        virtual ~texture() = default;

        texture& operator=(const texture&) = delete;
        texture& operator=(texture&&) noexcept = delete;

        string_view name() const noexcept override;
        guid id() const noexcept override;

        [[nodiscard]] size_t width() const noexcept;
        void width(size_t value) noexcept;

        [[nodiscard]] size_t height() const noexcept;
        void height(size_t value) noexcept;

        [[nodiscard]] texture_format format() const noexcept;
        void format(texture_format value) noexcept;

        [[nodiscard]] span<const byte> data(size_t level = 0) const noexcept;
        void set_mip_data(size_t level, vector<byte> data);

        [[nodiscard]] const sampler_state& sampler() const noexcept;
        void sampler(sampler_state value) noexcept;

      private:
        string _name;
        guid _id;

        size_t _width;
        size_t _height;
        vector<vector<byte>> _data;
        sampler_state _smp;

        texture_format _format;
    };
} // namespace tempest::assets

#endif // tempest_assets_texture_hpp