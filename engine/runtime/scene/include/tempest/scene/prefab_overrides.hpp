#ifndef tempest_scene_prefab_overrides_hpp
#define tempest_scene_prefab_overrides_hpp

#include <tempest/api.hpp>
#include <tempest/format.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/int.hpp>
#include <tempest/scene/scene_staging.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    /// @brief Maximum nesting depth supported for hierarchical prefab entity addressing.
    /// Guarantees zero heap allocation for id_path instances while fitting within a single 64-byte cache line.
    inline constexpr size_t max_id_path_depth = 8;

    /// @brief Hierarchical sequence of local entity IDs addressing an entity within nested prefab instances.
    using id_path = inplace_vector<entity_local_id, max_id_path_depth>;

    /// @brief Formats an id_path as slash-separated 16-digit lowercase hex local IDs.
    inline auto to_string(const id_path& path) -> string
    {
        if (path.empty())
        {
            return string{};
        }

        auto result = string{};
        for (auto segment_index = size_t{0}; segment_index < path.size(); ++segment_index)
        {
            if (segment_index > 0)
            {
                result += '/';
            }
            result += to_hex_string(path[segment_index]);
        }
        return result;
    }

    /// @brief Field-level override targeting a component field on an entity in a prefab instance.
    struct TEMPEST_API field_override
    {
        id_path target;
        string component_name;
        string field_name;
        vector<byte> value_bytes;

        auto operator==(const field_override& rhs) const noexcept -> bool = default;
    };

    /// @brief Specification of a component to be removed from an entity in a prefab instance.
    struct TEMPEST_API removed_component
    {
        id_path target;
        string component_name;

        auto operator==(const removed_component& rhs) const noexcept -> bool = default;
    };

    /// @brief Specification of a component to be added to an entity in a prefab instance.
    struct TEMPEST_API added_component
    {
        id_path target;
        staged_component component = {};
    };

    /// @brief Collection of structural and field-level overrides applied to a prefab instance.
    struct TEMPEST_API prefab_overrides
    {
        vector<field_override> field_overrides;
        vector<removed_component> removed_components;
        vector<added_component> added_components;
        vector<entity_local_id> added_children;

        [[nodiscard]] auto empty() const noexcept -> bool
        {
            return field_overrides.empty() && removed_components.empty() && added_components.empty() &&
                   added_children.empty();
        }
    };
} // namespace tempest::scene

namespace tempest
{
    using scene::to_string;

    template <typename CharT>
    struct formatter<scene::id_path, CharT>
    {
        template <typename FormatContext>
        static auto format(const scene::id_path& val, FormatContext& ctx) -> void
        {
            auto str = scene::to_string(val);
            ctx.write_padded(string_view{str.data(), str.size()});
        }
    };
} // namespace tempest

#endif // tempest_scene_prefab_overrides_hpp
