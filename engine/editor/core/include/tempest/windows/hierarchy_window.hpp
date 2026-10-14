#ifndef tempest_editor_core_hierarchy_window_hpp
#define tempest_editor_core_hierarchy_window_hpp

#include <tempest/archetype.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/hierarchy_cache.hpp>
#include <tempest/memory.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/windows/editor_window.hpp>

namespace tempest::editor
{
    class command_history;

    /// @brief Interactive virtualized scene and entity hierarchy window for the Tempest editor.
    struct TEMPEST_EDITOR_API hierarchy_window final : public editor_window
    {
        static constexpr bool default_open = true;
        static constexpr ecs::entity default_selected_entity = ecs::null;
        static constexpr int default_anchor_index = -1;
        static constexpr ecs::archetype_registry* default_registry = nullptr;
        static constexpr scene::scene_manager* default_scene_mgr = nullptr;
        static constexpr editor_selection* default_selection = nullptr;
        static constexpr command_history* default_history = nullptr;

        explicit hierarchy_window(ecs::archetype_registry& registry, scene::scene_manager* in_scene_mgr = nullptr,
                                  editor_selection* in_selection = nullptr, command_history* in_history = nullptr);
        ~hierarchy_window() override;

        hierarchy_window(const hierarchy_window&) = delete;
        hierarchy_window(hierarchy_window&&) noexcept = delete;
        auto operator=(const hierarchy_window&) -> hierarchy_window& = delete;
        auto operator=(hierarchy_window&&) noexcept -> hierarchy_window& = delete;

        auto desired_initial_dock() const -> editor_window::dock_location override;
        auto window_name() const -> string_view override;
        auto draw() -> void override;

        [[nodiscard]] auto get_cache() noexcept -> hierarchy_cache&
        {
            return _cache;
        }

        [[nodiscard]] auto get_cache() const noexcept -> const hierarchy_cache&
        {
            return _cache;
        }

        ecs::archetype_registry* entity_registry = default_registry;
        scene::scene_manager* scene_mgr = default_scene_mgr;
        editor_selection* selection = default_selection;
        command_history* history = default_history;
        ecs::entity selected_entity = default_selected_entity;
        bool open = default_open;

        static constexpr size_t default_rename_buffer_size = 256;
        static constexpr bool default_focus_rename = false;

      private:
        auto _draw_row(size_t index, const hierarchy_row& row) -> void;
        auto _handle_selection(size_t index, const hierarchy_row& row) -> void;
        auto _draw_context_menu(const hierarchy_row& row) -> void;
        auto _handle_shortcuts() -> void;

        hierarchy_cache _cache;
        unique_ptr<scene::scene_manager> _owned_scene_mgr;
        ecs::entity _anchor_entity = default_selected_entity;
        int _anchor_index = default_anchor_index;

        ecs::entity _renaming_entity = default_selected_entity;
        char _rename_buffer[default_rename_buffer_size]{};
        bool _focus_rename = default_focus_rename;
        event::subscription_handle<ecs::entity_destroyed_event<ecs::entity>> _entity_destroyed_sub = {};
    };
} // namespace tempest::editor

#endif // tempest_editor_core_hierarchy_window_hpp
