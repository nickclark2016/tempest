#ifndef tempest_editor_core_play_session_diff_hpp
#define tempest_editor_core_play_session_diff_hpp

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/ecs_events.hpp>
#include <tempest/event.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/int.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

#ifndef TEMPEST_EDITOR_API
#    define TEMPEST_EDITOR_API
#endif

namespace tempest::editor
{
    /// @brief Tracks entity spawning and component additions/removals across play sessions.
    class TEMPEST_EDITOR_API play_session_diff
    {
      public:
        static constexpr event::event_registry* default_events = nullptr;

        play_session_diff() = default;
        ~play_session_diff();

        play_session_diff(const play_session_diff&) = delete;
        auto operator=(const play_session_diff&) -> play_session_diff& = delete;

        play_session_diff(play_session_diff&& other) noexcept;
        auto operator=(play_session_diff&& other) noexcept -> play_session_diff&;

        /// @brief Subscribes to lifecycle events on the given event registry.
        auto attach(event::event_registry& events) -> void;

        /// @brief Unsubscribes from active event listeners.
        auto detach() -> void;

        /// @brief Clears all tracked entities and component differences.
        auto clear() noexcept -> void;

        /// @brief Queries whether an entity was created during the current play session.
        [[nodiscard]] auto is_spawned(ecs::entity entity) const noexcept -> bool;

        /// @brief Returns all entities created during the current play session.
        [[nodiscard]] auto get_spawned_entities() const noexcept -> span<const ecs::entity>;

        /// @brief Queries whether a specific component type was added to an entity during play.
        [[nodiscard]] auto is_component_added(ecs::entity entity, uint32_t type_index) const noexcept -> bool;

        /// @brief Returns the component type indices added to an entity during play.
        [[nodiscard]] auto get_added_components(ecs::entity entity) const -> span<const uint32_t>;

        /// @brief Queries whether a specific component type was removed from an entity during play.
        [[nodiscard]] auto is_component_removed(ecs::entity entity, uint32_t type_index) const noexcept -> bool;

        /// @brief Returns the component type indices removed from an entity during play.
        [[nodiscard]] auto get_removed_components(ecs::entity entity) const -> span<const uint32_t>;

      private:
        vector<ecs::entity> _spawned_entities;
        flat_unordered_map<ecs::entity, vector<uint32_t>> _added_components;
        flat_unordered_map<ecs::entity, vector<uint32_t>> _removed_components;

        event::event_registry* _events = default_events;
        event::subscription_handle<ecs::entity_created_event<ecs::entity>> _entity_created_sub =
            event::null_subscription<ecs::entity_created_event<ecs::entity>>;
        event::subscription_handle<ecs::entity_destroyed_event<ecs::entity>> _entity_destroyed_sub =
            event::null_subscription<ecs::entity_destroyed_event<ecs::entity>>;
        event::subscription_handle<ecs::entity_component_added_event<ecs::entity>> _component_added_sub =
            event::null_subscription<ecs::entity_component_added_event<ecs::entity>>;
        event::subscription_handle<ecs::entity_component_removed_event<ecs::entity>> _component_removed_sub =
            event::null_subscription<ecs::entity_component_removed_event<ecs::entity>>;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_play_session_diff_hpp
