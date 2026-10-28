#ifndef tempest_editor_core_viewport_window_hpp
#define tempest_editor_core_viewport_window_hpp

#include <tempest/checked.hpp>
#include <tempest/archetype.hpp>
#include <tempest/memory.hpp>
#include <tempest/payloads/asset_payload.hpp>
#include <tempest/render_system/renderer.hpp>
#include <tempest/rhi.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/windows/editor_window.hpp>

namespace tempest::assets
{
    class asset_database;
}

namespace tempest::scene
{
    class prefab_cache;
}

namespace tempest::editor
{
    class editor_engine_context;
    class command_history;

    /// @brief Ray in world space computed from camera and viewport screen coordinates.
    struct camera_ray
    {
        static constexpr math::vec3<float> default_origin = {};
        static constexpr math::vec3<float> default_direction = {0.0F, 0.0F, 1.0F};

        math::vec3<float> origin = default_origin;
        math::vec3<float> direction = default_direction;
    };

    using ray = camera_ray;

    /// @brief Result of a scene raycast or placement calculation in the viewport.
    struct placement_result
    {
        static constexpr math::vec3<float> default_position = {};
        static constexpr ecs::entity default_hit_entity = ecs::tombstone;
        static constexpr bool default_has_hit = false;

        math::vec3<float> position = default_position;
        ecs::entity hit_entity = default_hit_entity;
        bool has_hit = default_has_hit;

        constexpr operator math::vec3<float>() const noexcept
        {
            return position;
        }
    };

    inline bool operator==(const placement_result& lhs, const math::vec3<float>& rhs) noexcept
    {
        return lhs.position == rhs;
    }

    inline bool operator==(const math::vec3<float>& lhs, const placement_result& rhs) noexcept
    {
        return lhs == rhs.position;
    }

    inline bool operator!=(const placement_result& lhs, const math::vec3<float>& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    inline bool operator!=(const math::vec3<float>& lhs, const placement_result& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    class TEMPEST_EDITOR_API viewport_window final : public editor_window
    {
      public:
        static constexpr command_history* default_history = nullptr;
        static constexpr scene::prefab_cache* default_prefab_cache = nullptr;
        static constexpr assets::asset_database* default_asset_database = nullptr;
        static constexpr math::uint2 default_viewport_size = {0, 0};
        static constexpr bool default_open = true;

        /// @brief Constructs a new viewport window managing 3D scene rendering, fly navigation, and asset drops.
        /// @param ctx Non-owning reference to the editor engine context.
        /// @param in_history Optional pointer to command history for undoable operations.
        /// @param in_prefab_cache Optional pointer to prefab cache for template instantiation.
        /// @param in_asset_db Optional pointer to asset database for asset resolution.
        /// @post Viewport window is initialized and ready to render in the docking layout.
        explicit viewport_window(editor_engine_context& ctx,
                                 command_history* in_history = default_history,
                                 scene::prefab_cache* in_prefab_cache = default_prefab_cache,
                                 assets::asset_database* in_asset_db = default_asset_database);
        ~viewport_window() override = default;

        viewport_window(const viewport_window&) = delete;
        viewport_window(viewport_window&&) noexcept = delete;
        auto operator=(const viewport_window&) -> viewport_window& = delete;
        auto operator=(viewport_window&&) noexcept -> viewport_window& = delete;

        auto desired_initial_dock() const -> editor_window::dock_location override;
        auto window_name() const -> string_view override;
        auto draw() -> void override;

        /// @brief Computes the current viewport aspect ratio (width / height).
        /// @return Aspect ratio as floating-point value, or 1.0f if height is zero or uninitialized.
        /// @post Underlying viewport dimensions remain unchanged.
        [[nodiscard]] auto aspect_ratio() const -> float;

        [[nodiscard]] auto is_mode_supported([[maybe_unused]] simulation_state state) const noexcept -> bool override
        {
            return true;
        }

        /// @brief Computes a normalized world-space ray originating from the editor camera through the viewport pixel coordinates.
        /// @param mouse_pos Relative mouse cursor coordinates within the viewport rectangle.
        /// @param vp_size Extents of the viewport area in pixels.
        /// @return Camera ray in world space with origin and normalized direction.
        /// @post Camera position and orientation remain unchanged.
        [[nodiscard]] auto compute_camera_ray(math::vec2<float> mouse_pos, math::vec2<float> vp_size) const
            -> camera_ray;

        /// @brief Calculates the world-space 3D placement point from viewport mouse coordinates via raycasting.
        /// @param mouse_pos Relative mouse cursor coordinates within the viewport rectangle.
        /// @param vp_size Extents of the viewport area in pixels.
        /// @return Placement result containing target world position, hit entity if any, and hit flag.
        /// @post Scene entities and registry state remain unchanged.
        [[nodiscard]] auto calculate_placement_position(math::vec2<float> mouse_pos, math::vec2<float> vp_size) const
            -> placement_result;

        /// @brief Handles dropping an asset payload into the viewport, instantiating prefabs or assigning materials.
        /// @param payload Drag-and-drop asset payload containing GUID, metadata, and type info.
        /// @param mouse_pos Relative mouse cursor coordinates within the viewport rectangle.
        /// @param vp_size Extents of the viewport area in pixels.
        /// @return True if the drop resulted in command execution, false otherwise.
        /// @post If payload is a prefab, an instance is spawned at the placement location; if material and hovering over an entity, the material is assigned.
        auto handle_asset_drop(const asset_payload& payload, math::vec2<float> mouse_pos, math::vec2<float> vp_size)
            -> bool;

        /// @brief Classifies the dropped asset kind (prefab, material, or unknown) from payload metadata, database, and cache.
        /// @param payload Drag-and-drop asset payload containing GUID, type name, sub-asset key, or file path.
        /// @return Classified dropped_asset_kind enum value.
        /// @post No asset or cache state is modified.
        [[nodiscard]] auto deduce_asset_kind(const asset_payload& payload) const noexcept -> dropped_asset_kind;

      private:
        non_null<editor_engine_context> _ctx;
        command_history* _history = default_history;
        scene::prefab_cache* _prefab_cache = default_prefab_cache;
        unique_ptr<scene::prefab_cache> _owned_prefab_cache;
        assets::asset_database* _asset_db = default_asset_database;
        math::uint2 _viewport_size = default_viewport_size;
        bool _open = default_open;

        [[nodiscard]] auto _resolve_prefab_cache() noexcept -> scene::prefab_cache*;
        [[nodiscard]] auto _resolve_asset_database() const noexcept -> assets::asset_database*;
        auto _render_drop_target(math::vec2<float> child_pos, math::vec2<float> child_size) -> void;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_viewport_window_hpp
