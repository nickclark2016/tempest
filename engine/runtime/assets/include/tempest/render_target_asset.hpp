#ifndef tempest_assets_render_target_asset_hpp
#define tempest_assets_render_target_asset_hpp

#include <tempest/api.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/texture.hpp>
#include <tempest/vec4.hpp>

namespace tempest::assets
{
    struct TEMPEST_API render_target_asset
    {
        static constexpr uint32_t default_width = 1024;
        static constexpr uint32_t default_height = 1024;
        static constexpr core::texture_format default_format = core::texture_format::rgba8_unorm;
        static constexpr math::vec4<float> default_clear_color = {0.0F, 0.0F, 0.0F, 1.0F};
        static constexpr guid default_texture_id = {};

        string name = {};
        uint32_t width = default_width;
        uint32_t height = default_height;
        core::texture_format format = default_format;
        math::vec4<float> clear_color = default_clear_color;
        core::sampler_state sampler = {};
        guid texture_id = default_texture_id;

        [[nodiscard]] auto get_texture_id(const guid& asset_guid = {}) const noexcept -> guid;
        [[nodiscard]] auto to_core_texture() const -> core::texture;
        [[nodiscard]] static auto from_core_texture(const core::texture& tex) -> render_target_asset;

        [[nodiscard]] auto to_string() const -> string;
        [[nodiscard]] static auto load_from_string(string_view json_str) -> optional<render_target_asset>;

        [[nodiscard]] auto save_to_file(string_view path) const -> bool;
        [[nodiscard]] static auto load_from_file(string_view path) -> optional<render_target_asset>;

        auto operator==(const render_target_asset& rhs) const noexcept -> bool = default;
    };
} // namespace tempest::assets

#endif // tempest_assets_render_target_asset_hpp
