#ifndef tempest_editor_core_asset_payload_hpp
#define tempest_editor_core_asset_payload_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/string_view.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

#ifndef TEMPEST_EDITOR_API
#define TEMPEST_EDITOR_API
#endif

namespace tempest::editor
{
    inline constexpr const char* asset_payload_type = "TEMPEST_ASSET_PAYLOAD";

    /// @brief Trivially-copyable drag-and-drop payload transferred via ImGui drag-and-drop.
    /// @details The first field is guaranteed to be guid asset_guid at offset 0, preserving 100%
    /// binary compatibility with legacy property grid receivers expecting raw guid data.
    struct TEMPEST_EDITOR_API asset_payload
    {
        static constexpr size_t max_path_length = 260;
        static constexpr size_t max_type_name_length = 64;
        static constexpr size_t max_sub_asset_length = 128;
        static constexpr size_t default_length = 0;
        static constexpr size_t default_type_hash = 0;
        static constexpr guid default_asset_guid = {};
        static constexpr array<char, max_type_name_length> default_type_name = {};
        static constexpr array<char, max_path_length> default_path = {};
        static constexpr array<char, max_sub_asset_length> default_sub_asset_key = {};

        guid asset_guid = default_asset_guid;
        size_t type_hash = default_type_hash;
        size_t type_name_length = default_length;
        array<char, max_type_name_length> type_name = default_type_name;
        size_t path_length = default_length;
        array<char, max_path_length> path = default_path;
        size_t sub_asset_key_length = default_length;
        array<char, max_sub_asset_length> sub_asset_key = default_sub_asset_key;

        /// @brief Obtains a string_view of the stored asset path.
        /// @return String view referencing the path buffer.
        [[nodiscard]] auto path_view() const noexcept -> string_view
        {
            return string_view{path.data(), path_length};
        }

        /// @brief Obtains a string_view of the stored asset type name.
        /// @return String view referencing the type name buffer.
        [[nodiscard]] auto type_name_view() const noexcept -> string_view
        {
            return string_view{type_name.data(), type_name_length};
        }

        /// @brief Obtains a string_view of the stored sub-asset key.
        /// @return String view referencing the sub-asset key buffer.
        [[nodiscard]] auto sub_asset_key_view() const noexcept -> string_view
        {
            return string_view{sub_asset_key.data(), sub_asset_key_length};
        }

        /// @brief Copies a path into the internal null-terminated buffer, truncating if necessary.
        /// @param in_path Source path to copy.
        auto set_path(string_view in_path) noexcept -> void
        {
            const auto copy_len = tempest::min(in_path.size(), max_path_length - 1);
            if (copy_len > 0)
            {
                tempest::memcpy(path.data(), in_path.data(), copy_len);
            }
            path[copy_len] = '\0';
            path_length = copy_len;
        }

        /// @brief Copies a type name into the internal null-terminated buffer, truncating if necessary.
        /// @param in_type_name Source type name to copy.
        auto set_type_name(string_view in_type_name) noexcept -> void
        {
            const auto copy_len = tempest::min(in_type_name.size(), max_type_name_length - 1);
            if (copy_len > 0)
            {
                tempest::memcpy(type_name.data(), in_type_name.data(), copy_len);
            }
            type_name[copy_len] = '\0';
            type_name_length = copy_len;
        }

        /// @brief Copies a sub-asset key into the internal null-terminated buffer, truncating if necessary.
        /// @param in_key Source sub-asset key to copy.
        auto set_sub_asset_key(string_view in_key) noexcept -> void
        {
            const auto copy_len = tempest::min(in_key.size(), max_sub_asset_length - 1);
            if (copy_len > 0)
            {
                tempest::memcpy(sub_asset_key.data(), in_key.data(), copy_len);
            }
            sub_asset_key[copy_len] = '\0';
            sub_asset_key_length = copy_len;
        }

        /// @brief Unpacks an asset payload from raw ImGui drag-and-drop payload bytes.
        /// @details Supports both full asset_payload structures and legacy guid-only payloads.
        /// @param data Pointer to raw payload memory.
        /// @param size Size in bytes of the payload memory.
        /// @return Reconstructed asset_payload or empty default payload if data is invalid or undersized.
        static auto from_raw_payload(const void* data, size_t size) noexcept -> asset_payload
        {
            if (data == nullptr || size < sizeof(guid))
            {
                return asset_payload{};
            }

            auto result = asset_payload{};
            if (size >= sizeof(asset_payload))
            {
                tempest::memcpy(&result, data, sizeof(asset_payload));
            }
            else
            {
                tempest::memcpy(&result.asset_guid, data, sizeof(guid));
            }
            return result;
        }
    };

    static_assert(offsetof(asset_payload, asset_guid) == 0,
                  "asset_payload::asset_guid must be at offset 0 for property_grid compatibility");

    static_assert(is_trivially_copyable_v<asset_payload>,
                  "asset_payload must be trivially copyable for ImGui transport");
} // namespace tempest::editor

#endif // tempest_editor_core_asset_payload_hpp
