#ifndef tempest_core_texture_hpp
#define tempest_core_texture_hpp

#include <tempest/concepts.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::core
{
    enum class texture_format
    {
        rgba8_srgb,
        rgba8_unorm,
        rgba16_unorm,
        rgba32_float,
    };

    enum class texture_compression
    {
        none,
    };

    enum class magnify_texture_filter
    {
        nearest,
        linear,
    };

    enum class minify_texture_filter
    {
        nearest,
        linear,
        nearest_mipmap_nearest,
        linear_mipmap_nearest,
        nearest_mipmap_linear,
        linear_mipmap_linear,
    };

    enum class texture_wrap_mode
    {
        clamp_to_edge,
        mirrored_repeat,
        repeat,
    };

    struct texture_mip_data
    {
        vector<byte> data;
        uint32_t width{};
        uint32_t height{};
    };

    struct sampler_state
    {
        magnify_texture_filter mag_filter = magnify_texture_filter::linear;
        minify_texture_filter min_filter = minify_texture_filter::linear;
        texture_wrap_mode wrap_s = texture_wrap_mode::repeat;
        texture_wrap_mode wrap_t = texture_wrap_mode::repeat;
    };

    struct texture
    {
        vector<texture_mip_data> mips{};
        uint32_t width{};
        uint32_t height{};

        texture_format format{};
        texture_compression compression{};

        sampler_state sampler{};

        string name{};
    };

    class texture_registry
    {
      public:
        texture_registry() = default;
        ~texture_registry() = default;

        texture_registry(const texture_registry&) = delete;
        texture_registry(texture_registry&&) = delete;

        texture_registry& operator=(const texture_registry&) = delete;
        texture_registry& operator=(texture_registry&&) = delete;

        [[nodiscard]] guid register_texture(texture&& tex);
        [[nodiscard]] bool register_texture_with_id(const guid& id, texture&& tex);

        optional<const texture&> get_texture(guid id) const;

        template <invocable<texture&> Fn>
        void update_texture(const guid& id, Fn&& fn);

      private:
        flat_unordered_map<guid, texture> _textures;
    };

    struct texture_component
    {
        guid texture_id;
    };
    
    template <invocable<texture&> Fn>
    inline void texture_registry::update_texture(const guid& id, Fn&& fn)
    {
        auto it = _textures.find(id);
        if (it != _textures.end())
        {
            (void)fn(it->second);
        }
    }
} // namespace tempest::core

#endif // tempest_core_texture_hpp