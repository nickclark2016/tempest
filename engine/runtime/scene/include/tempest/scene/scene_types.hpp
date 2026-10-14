#ifndef tempest_scene_scene_types_hpp
#define tempest_scene_scene_types_hpp

#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/format.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    /// @brief Strongly typed 64-bit local entity identifier within a scene or prefab.
    struct TEMPEST_API entity_local_id
    {
        static constexpr uint64_t invalid_value = 0;

        uint64_t value = invalid_value;

        [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
        {
            return value != invalid_value;
        }

        constexpr auto operator==(const entity_local_id& rhs) const noexcept -> bool = default;
        constexpr auto operator!=(const entity_local_id& rhs) const noexcept -> bool = default;
        constexpr auto operator<(const entity_local_id& rhs) const noexcept -> bool
        {
            return value < rhs.value;
        }

        constexpr operator uint64_t() const noexcept
        {
            return value;
        }
    };

    /// @brief Error code returned when parsing a hex string fails.
    enum class parse_hex_error : uint8_t
    {
        empty_input = 0,
        invalid_prefix,
        invalid_digit,
        overflow_too_many_digits,
    };

    /// @brief Generational handle identifying a scene instance.
    struct TEMPEST_API scene_handle
    {
        static constexpr uint32_t invalid_index = ~uint32_t{0};
        static constexpr uint32_t default_generation = 0;

        uint32_t index = invalid_index;
        uint32_t generation = default_generation;

        [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
        {
            return index != invalid_index;
        }

        constexpr auto operator==(const scene_handle& rhs) const noexcept -> bool = default;
        constexpr auto operator!=(const scene_handle& rhs) const noexcept -> bool = default;
    };

    /// @brief Preserved payload for a component unrecognized by the active schema.
    struct TEMPEST_API unknown_component
    {
        string name;
        string json_payload;
    };

    /// @brief Preserved payload for an unrecognized field inside a recognized component.
    struct TEMPEST_API unknown_field
    {
        string field_name;
        string json_payload;
    };

    /// @brief Unknown schema data retained for a single entity.
    struct TEMPEST_API entity_unrecognized_data
    {
        vector<unknown_component> unknown_components;
        flat_unordered_map<string, vector<unknown_field>> unknown_fields;

        [[nodiscard]] auto empty() const noexcept -> bool
        {
            return unknown_components.empty() && unknown_fields.empty();
        }
    };

    /// @brief Scene-wide side-table of unrecognized schema data keyed by 64-bit local entity identifier.
    struct TEMPEST_API scene_unrecognized_data
    {
        flat_unordered_map<uint64_t, entity_unrecognized_data> entities;

        [[nodiscard]] auto empty() const noexcept -> bool
        {
            return entities.empty();
        }
    };

    /// @brief Formats a 64-bit local entity identifier as a 0x-prefixed 16-digit lowercase hex string.
    inline auto to_hex_string(uint64_t value) -> string
    {
        return tempest::format("{:#018x}", value);
    }

    /// @brief Formats an entity_local_id as a 0x-prefixed 16-digit lowercase hex string.
    inline auto to_hex_string(entity_local_id id) -> string
    {
        return to_hex_string(id.value);
    }

    /// @brief Parses a 0x-prefixed or plain hex string into a 64-bit local entity identifier.
    inline auto parse_hex_string(string_view str) noexcept -> expected<uint64_t, parse_hex_error>
    {
        if (str.empty())
        {
            return unexpected(parse_hex_error::empty_input);
        }
        auto view = str;
        if (view.size() >= 2 && view[0] == '0' && (view[1] == 'x' || view[1] == 'X'))
        {
            view = string_view{view.data() + 2, view.size() - 2};
        }
        if (view.empty() || view.size() > 16)
        {
            return unexpected(view.empty() ? parse_hex_error::invalid_prefix : parse_hex_error::overflow_too_many_digits);
        }
        auto result = uint64_t{0};
        for (const auto character : view)
        {
            result <<= 4;
            if (character >= '0' && character <= '9')
            {
                result |= static_cast<uint64_t>(character - '0');
            }
            else if (character >= 'a' && character <= 'f')
            {
                result |= static_cast<uint64_t>(character - 'a' + 10);
            }
            else if (character >= 'A' && character <= 'F')
            {
                result |= static_cast<uint64_t>(character - 'A' + 10);
            }
            else
            {
                return unexpected(parse_hex_error::invalid_digit);
            }
        }
        return result;
    }

    /// @brief Parses a 0x-prefixed or plain hex string into a strongly typed entity_local_id.
    inline auto parse_local_id(string_view str) noexcept -> expected<entity_local_id, parse_hex_error>
    {
        const auto res = parse_hex_string(str);
        if (!res.has_value())
        {
            return unexpected(res.error());
        }
        return entity_local_id{*res};
    }
} // namespace tempest::scene

namespace tempest
{
    template <typename CharT>
    struct formatter<scene::entity_local_id, CharT>
    {
        template <typename FormatContext>
        static auto format(const scene::entity_local_id& val, FormatContext& ctx) -> void
        {
            if (ctx.specs().type == '\0')
            {
                auto str = scene::to_hex_string(val);
                ctx.write_padded(string_view{str.data(), str.size()});
            }
            else
            {
                formatter<uint64_t, CharT>::format(val.value, ctx);
            }
        }
    };

    template <>
    struct hash<scene::entity_local_id>
    {
        auto operator()(const scene::entity_local_id& id) const noexcept -> size_t
        {
            return hash<uint64_t>{}(id.value);
        }
    };

    template <>
    struct hash<scene::scene_handle>
    {
        auto operator()(const scene::scene_handle& handle) const noexcept -> size_t
        {
            auto h1 = hash<uint32_t>{}(handle.index);
            auto h2 = hash<uint32_t>{}(handle.generation);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
}

#endif // tempest_scene_scene_types_hpp
