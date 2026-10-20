#ifndef tempest_editor_core_inspector_window_hpp
#define tempest_editor_core_inspector_window_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/memory.hpp>
#include <tempest/property_grid.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/vector.hpp>
#include <tempest/windows/editor_window.hpp>

#include <imgui.h>

#ifndef TEMPEST_EDITOR_API
#    define TEMPEST_EDITOR_API
#endif

namespace tempest
{
    class logger;
}

namespace tempest::editor
{
    class command_history;
    class play_session_diff;

    /// @brief Generic reflection-driven entity and component inspector window.
    struct TEMPEST_EDITOR_API inspector_window final : public editor_window
    {
        static constexpr bool default_open = true;
        static constexpr ecs::entity default_target = ecs::null;
        static constexpr ecs::entity default_last_inspected_entity = ecs::tombstone;
        static constexpr ecs::archetype_registry* default_registry = nullptr;
        static constexpr scene::scene_manager* default_scene_mgr = nullptr;
        static constexpr editor_selection* default_selection = nullptr;
        static constexpr command_history* default_history = nullptr;
        static constexpr scene::prefab_cache* default_prefab_cache = nullptr;
        static constexpr event::event_registry* default_events = nullptr;
        static constexpr logger* default_logger = nullptr;
        static constexpr const play_session_diff* default_session_diff = nullptr;

        explicit inspector_window(ecs::archetype_registry& registry, scene::scene_manager* in_scene_mgr = nullptr,
                                  editor_selection* in_selection = nullptr, command_history* in_history = nullptr,
                                  scene::prefab_cache* in_prefab_cache = nullptr,
                                  event::event_registry* in_events = nullptr, logger* in_logger = nullptr,
                                  const play_session_diff* in_session_diff = nullptr);
        ~inspector_window() override;

        inspector_window(const inspector_window&) = delete;
        inspector_window(inspector_window&&) noexcept = delete;
        auto operator=(const inspector_window&) -> inspector_window& = delete;
        auto operator=(inspector_window&&) noexcept -> inspector_window& = delete;

        auto desired_initial_dock() const -> editor_window::dock_location override;
        auto window_name() const -> string_view override;
        auto draw() -> void override;

        ecs::archetype_registry* entity_registry = default_registry;
        scene::scene_manager* scene_mgr = default_scene_mgr;
        editor_selection* selection = default_selection;
        command_history* history = default_history;
        scene::prefab_cache* prefab_cache = default_prefab_cache;
        event::event_registry* events = default_events;
        logger* log = default_logger;
        const play_session_diff* session_diff = default_session_diff;
        ecs::entity target = default_target;
        bool open = default_open;

      private:
        auto _draw_entity_header(ecs::entity ent, const entity_address& addr, command_context& cmd_ctx) -> void;
        auto _draw_component_cards(ecs::entity ent, const entity_address& addr, command_context& cmd_ctx) -> void;
        auto _draw_unknown_components(ecs::entity ent, const entity_address& addr, command_context& cmd_ctx) -> void;
        auto _draw_add_component_button(ecs::entity ent, const entity_address& addr, command_context& cmd_ctx) -> void;

        ImGuiTextFilter _component_search_filter;
        ImGuiTextFilter _asset_search_filter;
        vector<ecs::component_type_info> _cached_comp_types;
        unique_ptr<scene::scene_manager> _owned_scene_mgr;
        ecs::entity _last_inspected_entity = default_last_inspected_entity;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_inspector_window_hpp
