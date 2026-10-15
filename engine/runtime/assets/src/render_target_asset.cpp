#include <tempest/render_target_asset.hpp>

#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/utility.hpp>

namespace tempest::assets
{
    namespace
    {
        constexpr auto format_to_string_view(core::texture_format texture_format) noexcept -> string_view
        {
            switch (texture_format)
            {
                case core::texture_format::rgba8_srgb:
                    return "rgba8_srgb";
                case core::texture_format::rgba8_unorm:
                    return "rgba8_unorm";
                case core::texture_format::rgba16_unorm:
                    return "rgba16_unorm";
                case core::texture_format::rgba32_float:
                    return "rgba32_float";
            }
            return "rgba8_unorm";
        }

        constexpr auto string_view_to_format(string_view format_string) noexcept -> core::texture_format
        {
            if (format_string == "rgba8_srgb")
            {
                return core::texture_format::rgba8_srgb;
            }
            if (format_string == "rgba8_unorm")
            {
                return core::texture_format::rgba8_unorm;
            }
            if (format_string == "rgba16_unorm")
            {
                return core::texture_format::rgba16_unorm;
            }
            if (format_string == "rgba32_float")
            {
                return core::texture_format::rgba32_float;
            }
            return core::texture_format::rgba8_unorm;
        }

        constexpr auto mag_filter_to_string_view(core::magnify_texture_filter filter) noexcept -> string_view
        {
            switch (filter)
            {
                case core::magnify_texture_filter::nearest:
                    return "nearest";
                case core::magnify_texture_filter::linear:
                    return "linear";
            }
            return "linear";
        }

        constexpr auto string_view_to_mag_filter(string_view filter_string) noexcept -> core::magnify_texture_filter
        {
            if (filter_string == "nearest")
            {
                return core::magnify_texture_filter::nearest;
            }
            return core::magnify_texture_filter::linear;
        }

        constexpr auto min_filter_to_string_view(core::minify_texture_filter filter) noexcept -> string_view
        {
            switch (filter)
            {
                case core::minify_texture_filter::nearest:
                    return "nearest";
                case core::minify_texture_filter::linear:
                    return "linear";
                case core::minify_texture_filter::nearest_mipmap_nearest:
                    return "nearest_mipmap_nearest";
                case core::minify_texture_filter::linear_mipmap_nearest:
                    return "linear_mipmap_nearest";
                case core::minify_texture_filter::nearest_mipmap_linear:
                    return "nearest_mipmap_linear";
                case core::minify_texture_filter::linear_mipmap_linear:
                    return "linear_mipmap_linear";
            }
            return "linear";
        }

        constexpr auto string_view_to_min_filter(string_view filter_string) noexcept -> core::minify_texture_filter
        {
            if (filter_string == "nearest")
            {
                return core::minify_texture_filter::nearest;
            }
            if (filter_string == "linear")
            {
                return core::minify_texture_filter::linear;
            }
            if (filter_string == "nearest_mipmap_nearest")
            {
                return core::minify_texture_filter::nearest_mipmap_nearest;
            }
            if (filter_string == "linear_mipmap_nearest")
            {
                return core::minify_texture_filter::linear_mipmap_nearest;
            }
            if (filter_string == "nearest_mipmap_linear")
            {
                return core::minify_texture_filter::nearest_mipmap_linear;
            }
            if (filter_string == "linear_mipmap_linear")
            {
                return core::minify_texture_filter::linear_mipmap_linear;
            }
            return core::minify_texture_filter::linear;
        }

        constexpr auto wrap_mode_to_string_view(core::texture_wrap_mode mode) noexcept -> string_view
        {
            switch (mode)
            {
                case core::texture_wrap_mode::clamp_to_edge:
                    return "clamp_to_edge";
                case core::texture_wrap_mode::mirrored_repeat:
                    return "mirrored_repeat";
                case core::texture_wrap_mode::repeat:
                    return "repeat";
            }
            return "repeat";
        }

        constexpr auto string_view_to_wrap_mode(string_view mode_string) noexcept -> core::texture_wrap_mode
        {
            if (mode_string == "clamp_to_edge")
            {
                return core::texture_wrap_mode::clamp_to_edge;
            }
            if (mode_string == "mirrored_repeat")
            {
                return core::texture_wrap_mode::mirrored_repeat;
            }
            return core::texture_wrap_mode::repeat;
        }

        auto parse_guid_from_value(const core::json_value& value) -> optional<guid>
        {
            if (value.is_string())
            {
                const auto string_opt = value.as_string();
                if (string_opt.has_value())
                {
                    return parse_guid(*string_opt);
                }
            }
            return nullopt;
        }

        auto extract_vec4_from_array(const core::json_value& array_value,
                                     const math::vec4<float>& default_value) -> math::vec4<float>
        {
            auto elements = core::json_array{};
            if (array_value.get(elements) && elements.size() >= 4)
            {
                return math::vec4<float>{
                    static_cast<float>(elements[0].as_number().value_or(default_value.x)),
                    static_cast<float>(elements[1].as_number().value_or(default_value.y)),
                    static_cast<float>(elements[2].as_number().value_or(default_value.z)),
                    static_cast<float>(elements[3].as_number().value_or(default_value.w)),
                };
            }
            return default_value;
        }
    } // namespace

    auto render_target_asset::get_texture_id(const guid& asset_guid) const noexcept -> guid
    {
        if (texture_id != guid{})
        {
            return texture_id;
        }
        return asset_guid;
    }

    auto render_target_asset::to_core_texture() const -> core::texture
    {
        auto tex = core::texture{};
        tex.width = width;
        tex.height = height;
        tex.format = format;
        tex.compression = core::texture_compression::none;
        tex.sampler = sampler;
        tex.name = name;
        return tex;
    }

    auto render_target_asset::from_core_texture(const core::texture& tex) -> render_target_asset
    {
        auto asset = render_target_asset{};
        asset.name = tex.name;
        asset.width = tex.width;
        asset.height = tex.height;
        asset.format = tex.format;
        asset.sampler = tex.sampler;
        return asset;
    }

    auto render_target_asset::to_string() const -> string
    {
        auto writer = core::json_writer{};
        auto root = writer.create_object();

        root.set("name", name);
        root.set("width", static_cast<uint64_t>(width));
        root.set("height", static_cast<uint64_t>(height));
        root.set("format", format_to_string_view(format));

        auto clear_color_array = root.create_child_array("clear_color");
        clear_color_array.push_back(clear_color.x);
        clear_color_array.push_back(clear_color.y);
        clear_color_array.push_back(clear_color.z);
        clear_color_array.push_back(clear_color.w);

        auto sampler_object = root.create_child_object("sampler");
        sampler_object.set("mag_filter", mag_filter_to_string_view(sampler.mag_filter));
        sampler_object.set("min_filter", min_filter_to_string_view(sampler.min_filter));
        sampler_object.set("wrap_s", wrap_mode_to_string_view(sampler.wrap_s));
        sampler_object.set("wrap_t", wrap_mode_to_string_view(sampler.wrap_t));

        if (texture_id != guid{})
        {
            root.set("texture_id", tempest::to_string(texture_id));
        }

        writer.set_root(root);
        return writer.to_string(core::json_format::pretty);
    }

    auto render_target_asset::load_from_string(string_view json_str) -> optional<render_target_asset>
    {
        auto allocator = system_allocator{};
        auto document_result = core::json_document::from_string(json_str, allocator);
        if (!document_result.has_value())
        {
            return nullopt;
        }

        const auto root = document_result->root();
        if (!root.is_object())
        {
            return nullopt;
        }

        auto result = render_target_asset{};

        const auto name_value = root["name"].as_string();
        if (name_value.has_value())
        {
            result.name = string{*name_value};
        }

        const auto width_value = root["width"].as_number();
        if (width_value.has_value())
        {
            result.width = static_cast<uint32_t>(*width_value);
        }

        const auto height_value = root["height"].as_number();
        if (height_value.has_value())
        {
            result.height = static_cast<uint32_t>(*height_value);
        }

        const auto format_value = root["format"].as_string();
        if (format_value.has_value())
        {
            result.format = string_view_to_format(*format_value);
        }

        result.clear_color = extract_vec4_from_array(root["clear_color"], default_clear_color);

        const auto sampler_node = root["sampler"];
        if (sampler_node.is_object())
        {
            const auto mag_filter_value = sampler_node["mag_filter"].as_string();
            if (mag_filter_value.has_value())
            {
                result.sampler.mag_filter = string_view_to_mag_filter(*mag_filter_value);
            }

            const auto min_filter_value = sampler_node["min_filter"].as_string();
            if (min_filter_value.has_value())
            {
                result.sampler.min_filter = string_view_to_min_filter(*min_filter_value);
            }

            const auto wrap_s_value = sampler_node["wrap_s"].as_string();
            if (wrap_s_value.has_value())
            {
                result.sampler.wrap_s = string_view_to_wrap_mode(*wrap_s_value);
            }

            const auto wrap_t_value = sampler_node["wrap_t"].as_string();
            if (wrap_t_value.has_value())
            {
                result.sampler.wrap_t = string_view_to_wrap_mode(*wrap_t_value);
            }
        }

        const auto parsed_texture_id = parse_guid_from_value(root["texture_id"]);
        if (parsed_texture_id.has_value())
        {
            result.texture_id = *parsed_texture_id;
        }

        return result;
    }

    auto render_target_asset::save_to_file(string_view path) const -> bool
    {
        const auto json_str = to_string();
        const auto write_result = write_file_from_bytes(
            filesystem::path(path),
            span<const byte>{reinterpret_cast<const byte*>(json_str.data()), json_str.size()});
        return write_result.has_value();
    }

    auto render_target_asset::load_from_file(string_view path) -> optional<render_target_asset>
    {
        const auto content = read_file_to_string(filesystem::path(path));
        if (!content.has_value())
        {
            return nullopt;
        }
        return load_from_string(*content);
    }
} // namespace tempest::assets
