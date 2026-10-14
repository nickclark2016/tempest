#ifndef tempest_editor_core_editor_selection_hpp
#define tempest_editor_core_editor_selection_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/ecs_events.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/event.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/optional.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

#ifndef TEMPEST_EDITOR_API
#    define TEMPEST_EDITOR_API
#endif

namespace tempest::editor
{
    /// @brief Manages the active editor selection tracking persistent entity addresses alongside cached live handles.
    class TEMPEST_EDITOR_API editor_selection
    {
      public:
        static constexpr ecs::entity default_primary_entity = ecs::tombstone;
        static constexpr event::event_registry* default_events = nullptr;

        editor_selection() = default;
        ~editor_selection();

        editor_selection(const editor_selection& other);
        editor_selection(editor_selection&& other) noexcept;
        auto operator=(const editor_selection& other) -> editor_selection&;
        auto operator=(editor_selection&& other) noexcept -> editor_selection&;

        // Selection Mutations

        /// @brief Replaces current selection with a single entity address and optional live handle.
        auto set_selection(entity_address addr, ecs::entity ent = ecs::tombstone) -> void;

        /// @brief Replaces current selection with an entity handle, computing its persistent address automatically.
        auto set_selection(ecs::entity ent, const ecs::basic_archetype_registry& reg,
                           const scene::scene_manager* scene_mgr = nullptr) -> void;

        /// @brief Toggles selection state of an entity address (Ctrl+Click behavior).
        auto toggle_selection(entity_address addr, ecs::entity ent = ecs::tombstone) -> void;

        /// @brief Selects a contiguous range of entity addresses (Shift+Click behavior).
        auto select_range(span<const entity_address> addrs, span<const ecs::entity> ents = {}) -> void;

        /// @brief Appends an entity address and optional live handle to the selection, designating it as primary.
        auto add_to_selection(entity_address addr, ecs::entity ent = ecs::tombstone) -> void;

        /// @brief Removes a specific entity address and its corresponding live handle from the selection.
        auto remove_from_selection(const entity_address& addr) -> void;

        /// @brief Clears all selections, live handles, and primary target tracking.
        auto clear_selection() -> void;

        // Queries

        [[nodiscard]] auto is_selected(ecs::entity ent) const noexcept -> bool;
        [[nodiscard]] auto is_selected(const entity_address& addr) const noexcept -> bool;

        [[nodiscard]] auto get_primary_entity() const noexcept -> ecs::entity;
        [[nodiscard]] auto get_primary_address() const noexcept -> optional<entity_address>;

        [[nodiscard]] auto get_selected_entities() const noexcept -> span<const ecs::entity>;
        [[nodiscard]] auto get_selected_addresses() const noexcept -> span<const entity_address>;

        [[nodiscard]] auto count() const noexcept -> size_t;
        [[nodiscard]] auto is_empty() const noexcept -> bool;

        // Re-resolution & Resiliency

        /// @brief Re-queries live entity handles from persistent addresses using the active registry and scene manager.
        auto re_resolve(const ecs::basic_archetype_registry& reg, const scene::scene_manager& scene_mgr) -> void;

        /// @brief Evicts destroyed live entity handles while preserving persistent addresses for undo/redo re-resolution.
        auto on_entity_destroyed(ecs::entity ent) -> void;

        /// @brief Purges persistent addresses that fail to resolve in the active scene registry.
        auto purge_destroyed(const ecs::basic_archetype_registry& reg, const scene::scene_manager& scene_mgr) -> void;

        /// @brief Evicts all addresses and live handles belonging to an unloaded scene.
        auto on_scene_unloaded(scene::scene_handle handle) -> void;

        // Event Subscription

        /// @brief Subscribes to entity lifecycle events from the given event registry for automatic eviction.
        auto attach_event_registry(event::event_registry& events) -> void;

        /// @brief Unsubscribes from entity lifecycle events.
        auto detach_event_registry() -> void;

      private:
        vector<entity_address> _selected_addresses;
        vector<ecs::entity> _selected_entities;
        flat_unordered_map<entity_address, ecs::entity> _address_to_entity;
        optional<entity_address> _primary_address = nullopt;
        ecs::entity _primary_entity = default_primary_entity;
        event::event_registry* _events = default_events;
        event::subscription_handle<ecs::entity_destroyed_event<ecs::entity>> _entity_destroyed_sub = {};
    };
} // namespace tempest::editor

#endif // tempest_editor_core_editor_selection_hpp
