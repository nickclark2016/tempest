#ifndef tempest_editor_core_hierarchy_cache_hpp
#define tempest_editor_core_hierarchy_cache_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/ecs_events.hpp>
#include <tempest/event.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/int.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

#ifndef TEMPEST_EDITOR_API
#    define TEMPEST_EDITOR_API
#endif

namespace tempest::editor
{
    class play_session_diff;
    /// @brief Tag component identifying an entity as networked.
    struct networked_tag
    {
    };

    /// @brief Discriminator for flattened hierarchy rows.
    enum class row_type : uint8_t
    {
        scene_header = 0,
        dynamic_header,
        entity_node,
    };

    /// @brief Flattened visible row descriptor for virtualized rendering.
    struct hierarchy_row
    {
        static constexpr row_type default_type = row_type::entity_node;
        static constexpr size_t default_depth = 0;
        static constexpr ecs::entity default_entity = ecs::tombstone;
        static constexpr scene::scene_handle default_scene = {};
        static constexpr bool default_bool = false;

        row_type type = default_type;
        size_t depth = default_depth;
        ecs::entity entity = default_entity;
        scene::scene_handle scene = default_scene;
        string name = {};
        bool is_expanded = default_bool;
        bool has_children = default_bool;
        bool is_dirty = default_bool;
        bool is_active_scene = default_bool;
        bool is_prefab_instance = default_bool;
        bool is_dynamic = default_bool;
        bool is_networked = default_bool;
    };

    /// @brief Incremental event-driven cache maintaining a flattened list of visible hierarchy rows.
    class TEMPEST_EDITOR_API hierarchy_cache
    {
      public:
        static constexpr bool default_dirty = true;
        static constexpr bool default_section_expanded = true;
        static constexpr event::event_registry* default_events = nullptr;
        static constexpr size_t default_scene_count = 0;
        static constexpr scene::scene_handle default_active_scene = {};

        hierarchy_cache();
        ~hierarchy_cache();

        hierarchy_cache(const hierarchy_cache& other);
        hierarchy_cache(hierarchy_cache&& other) noexcept;
        auto operator=(const hierarchy_cache& other) -> hierarchy_cache&;
        auto operator=(hierarchy_cache&& other) noexcept -> hierarchy_cache&;

        /// @brief Rebuilds the flattened visible row model if marked dirty or scene state changed.
        auto rebuild(const ecs::basic_archetype_registry& reg, const scene::scene_manager& scene_mgr,
                     const play_session_diff* session_diff = nullptr) -> void;

        /// @brief Returns the cached flattened visible rows.
        [[nodiscard]] auto get_visible_rows() const noexcept -> span<const hierarchy_row>;

        /// @brief Sets expansion state for a scene node.
        auto set_expanded(scene::scene_handle scene, bool expanded) -> void;

        /// @brief Sets expansion state for an entity node.
        auto set_expanded(ecs::entity ent, bool expanded) -> void;

        /// @brief Sets expansion state for the dynamic entities section.
        auto set_dynamic_section_expanded(bool expanded) -> void;

        /// @brief Queries expansion state for a scene node.
        [[nodiscard]] auto is_expanded(scene::scene_handle scene) const noexcept -> bool;

        /// @brief Queries expansion state for an entity node.
        [[nodiscard]] auto is_expanded(ecs::entity ent) const noexcept -> bool;

        /// @brief Queries expansion state for the dynamic entities section.
        [[nodiscard]] auto is_dynamic_section_expanded() const noexcept -> bool;

        /// @brief Subscribes to ECS events for incremental dirty notification.
        auto attach_event_registry(event::event_registry& events) -> void;

        /// @brief Unsubscribes from ECS events.
        auto detach_event_registry() -> void;

        /// @brief Marks the cache dirty to trigger a rebuild on next frame.
        auto mark_dirty() -> void;

        /// @brief Returns whether the cache is currently dirty.
        [[nodiscard]] auto is_dirty() const noexcept -> bool;

      private:
        vector<hierarchy_row> _visible_rows;
        flat_unordered_map<scene::scene_handle, bool> _scene_expanded;
        flat_unordered_map<ecs::entity, bool> _entity_expanded;
        vector<ecs::entity> _dynamic_roots;
        flat_unordered_map<scene::scene_handle, vector<ecs::entity>> _scene_roots;
        bool _dynamic_section_expanded = default_section_expanded;
        bool _dirty = default_dirty;

        // Change detection for scene manager state without events
        scene::scene_handle _last_active_scene = default_active_scene;
        flat_unordered_map<scene::scene_handle, bool> _last_scene_dirty;
        size_t _last_scene_count = default_scene_count;

        event::event_registry* _events = default_events;
        event::subscription_handle<ecs::entity_created_event<ecs::entity>> _entity_created_sub =
            event::null_subscription<ecs::entity_created_event<ecs::entity>>;
        event::subscription_handle<ecs::entity_destroyed_event<ecs::entity>> _entity_destroyed_sub =
            event::null_subscription<ecs::entity_destroyed_event<ecs::entity>>;
        event::subscription_handle<ecs::entity_renamed_event<ecs::entity>> _entity_renamed_sub =
            event::null_subscription<ecs::entity_renamed_event<ecs::entity>>;
        event::subscription_handle<ecs::component_added_event<ecs::entity, ecs::relationship_component<ecs::entity>>>
            _rel_added_sub =
                event::null_subscription<ecs::component_added_event<ecs::entity, ecs::relationship_component<ecs::entity>>>;
        event::subscription_handle<ecs::component_removed_event<ecs::entity, ecs::relationship_component<ecs::entity>>>
            _rel_removed_sub =
                event::null_subscription<ecs::component_removed_event<ecs::entity, ecs::relationship_component<ecs::entity>>>;
        event::subscription_handle<ecs::component_replaced_event<ecs::entity, ecs::relationship_component<ecs::entity>>>
            _rel_replaced_sub =
                event::null_subscription<ecs::component_replaced_event<ecs::entity, ecs::relationship_component<ecs::entity>>>;
        event::subscription_handle<ecs::component_added_event<ecs::entity, scene::scene_entity_component>>
            _scene_comp_added_sub =
                event::null_subscription<ecs::component_added_event<ecs::entity, scene::scene_entity_component>>;
        event::subscription_handle<ecs::component_removed_event<ecs::entity, scene::scene_entity_component>>
            _scene_comp_removed_sub =
                event::null_subscription<ecs::component_removed_event<ecs::entity, scene::scene_entity_component>>;
        event::subscription_handle<ecs::component_replaced_event<ecs::entity, scene::scene_entity_component>>
            _scene_comp_replaced_sub =
                event::null_subscription<ecs::component_replaced_event<ecs::entity, scene::scene_entity_component>>;
        event::subscription_handle<ecs::world_restored_event> _world_restored_sub =
            event::null_subscription<ecs::world_restored_event>;
        event::subscription_handle<ecs::simulation_started_event> _sim_started_sub =
            event::null_subscription<ecs::simulation_started_event>;
        event::subscription_handle<ecs::simulation_stopped_event> _sim_stopped_sub =
            event::null_subscription<ecs::simulation_stopped_event>;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_hierarchy_cache_hpp
