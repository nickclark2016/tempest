#ifndef tempest_editor_core_hierarchy_window_hpp
#define tempest_editor_core_hierarchy_window_hpp

#include <tempest/archetype.hpp>
#include <tempest/checked.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/hierarchy_cache.hpp>
#include <tempest/logger.hpp>
#include <tempest/memory.hpp>
#include <tempest/payloads/asset_payload.hpp>
#include <tempest/scene/scene_manager.hpp>
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
    class command_history;
    class play_session_diff;

    /// @brief Interactive virtualized scene and entity hierarchy window for the Tempest editor.
    struct TEMPEST_EDITOR_API hierarchy_window final : public editor_window
    {
        static constexpr bool default_open = true;
        static constexpr ecs::entity default_selected_entity = ecs::null;
        static constexpr int default_anchor_index = -1;
        static constexpr command_history* default_history = nullptr;
        static constexpr const play_session_diff* default_session_diff = nullptr;
        static constexpr scene::prefab_cache* default_prefab_cache = nullptr;
        static constexpr assets::asset_database* default_asset_database = nullptr;

        /// @brief Constructs a hierarchy window for the Tempest editor.
        /// @param registry Reference to the archetype registry.
        /// @param scene_mgr Reference to the scene manager.
        /// @param selection Reference to the editor selection.
        /// @param log Reference to the logger.
        /// @param history Pointer to the command history (optional).
        /// @param session_diff Pointer to the play session diff (optional).
        /// @param prefab_cache Pointer to the prefab cache (optional).
        /// @param asset_db Pointer to the asset database (optional).
        explicit hierarchy_window(ecs::archetype_registry& registry, scene::scene_manager& scene_mgr,
                                  editor_selection& selection, logger& log, command_history* history = default_history,
                                  const play_session_diff* session_diff = default_session_diff,
                                  scene::prefab_cache* prefab_cache = default_prefab_cache,
                                  assets::asset_database* asset_db = default_asset_database);
        ~hierarchy_window() override;

        hierarchy_window(const hierarchy_window&) = delete;
        hierarchy_window(hierarchy_window&&) noexcept = delete;
        auto operator=(const hierarchy_window&) -> hierarchy_window& = delete;
        auto operator=(hierarchy_window&&) noexcept -> hierarchy_window& = delete;

        /// @brief Obtains the desired initial dock location for the hierarchy window.
        /// @return The preferred dock location.
        [[nodiscard]] auto desired_initial_dock() const -> editor_window::dock_location override;

        /// @brief Obtains the display name of the hierarchy window.
        /// @return The window name as a string view.
        [[nodiscard]] auto window_name() const -> string_view override;

        /// @brief Renders the contents of the hierarchy window.
        auto draw() -> void override;

        /// @brief Obtains the hierarchy cache associated with the hierarchy window.
        /// @return Reference to the hierarchy cache.
        [[nodiscard]] auto get_cache() noexcept -> hierarchy_cache&
        {
            return _cache;
        }

        /// @brief Obtains the hierarchy cache associated with the hierarchy window (const version).
        /// @return Const reference to the hierarchy cache.
        [[nodiscard]] auto get_cache() const noexcept -> const hierarchy_cache&
        {
            return _cache;
        }

        /// @brief Handles dropping an asset payload directly onto a live entity row.
        /// @pre target_entity must be a valid entity within entity_registry.
        /// @post If payload represents a valid prefab or material, executes corresponding command and marks hierarchy
        /// dirty.
        /// @param target_entity Entity handle receiving the asset drop.
        /// @param payload Drag-and-drop asset payload containing GUID, metadata, and type info.
        /// @return True if the drop resulted in command execution, false otherwise.
        auto handle_asset_drop_on_entity(ecs::entity target_entity, const asset_payload& payload) -> bool;

        /// @brief Handles dropping an asset payload onto a scene root header.
        /// @pre target_scene must be a valid scene handle (or empty handle for dynamic section).
        /// @post If payload represents a valid prefab, executes instantiate_prefab_command and marks hierarchy dirty.
        /// @param target_scene Scene handle receiving the asset drop.
        /// @param payload Drag-and-drop asset payload containing GUID, metadata, and type info.
        /// @return True if the drop resulted in command execution, false otherwise.
        auto handle_asset_drop_on_scene(scene::scene_handle target_scene, const asset_payload& payload) -> bool;

        /// @brief Handles dropping an asset payload onto the hierarchy window empty space or background.
        /// @post If payload represents a valid prefab, executes instantiate_prefab_command targeting active scene and
        /// marks hierarchy dirty.
        /// @param payload Drag-and-drop asset payload containing GUID, metadata, and type info.
        /// @return True if the drop resulted in command execution, false otherwise.
        auto handle_asset_drop_on_empty_space(const asset_payload& payload) -> bool;

        /// @brief Checks if the hierarchy window is currently open.
        /// @return True if the window is open, false otherwise.
        [[nodiscard]] auto is_open() const noexcept -> bool
        {
            return _open;
        }

        /// @brief Sets the open state of the hierarchy window.
        /// @param in_open True to open the window, false to close it.
        auto set_open(bool in_open) noexcept -> void
        {
            _open = in_open;
        }

        /// @brief Obtains the currently selected entity in the hierarchy window.
        /// @return Handle to the selected entity.
        [[nodiscard]] auto get_selected_entity() const noexcept -> ecs::entity
        {
            return _selected_entity;
        }

        /// @brief Sets the currently selected entity in the hierarchy window.
        /// @param entity Handle to the entity to select.
        auto set_selected_entity(ecs::entity entity) noexcept -> void
        {
            _selected_entity = entity;
        }

        /// @brief Obtains the hierarchy cache associated with the hierarchy window.
        /// @return Reference to the hierarchy cache.
        [[nodiscard]] auto get_registry() noexcept -> ecs::archetype_registry&
        {
            return *_entity_registry;
        }

        /// @brief Obtains the hierarchy cache associated with the hierarchy window (const version).
        /// @return Const reference to the hierarchy cache.
        [[nodiscard]] auto get_registry() const noexcept -> const ecs::archetype_registry&
        {
            return *_entity_registry;
        }

        /// @brief Obtains the scene manager associated with the hierarchy window.
        /// @return Reference to the scene manager.
        [[nodiscard]] auto get_scene_manager() noexcept -> scene::scene_manager&
        {
            return *_scene_mgr;
        }

        /// @brief Obtains the scene manager associated with the hierarchy window (const version).
        /// @return Const reference to the scene manager.
        [[nodiscard]] auto get_scene_manager() const noexcept -> const scene::scene_manager&
        {
            return *_scene_mgr;
        }

        /// @brief Obtains the current editor selection associated with the hierarchy window.
        /// @return Reference to the editor selection.
        [[nodiscard]] auto get_selection() noexcept -> editor_selection&
        {
            return *_selection;
        }

        /// @brief Obtains the current editor selection associated with the hierarchy window (const version).
        /// @return Const reference to the editor selection.
        [[nodiscard]] auto get_selection() const noexcept -> const editor_selection&
        {
            return *_selection;
        }

        static constexpr size_t default_rename_buffer_size = 256;
        static constexpr bool default_focus_rename = false;

      private:
        auto _draw_row(size_t index, const hierarchy_row& row) -> void;
        auto _handle_selection(size_t index, const hierarchy_row& row) -> void;
        auto _draw_context_menu(const hierarchy_row& row) -> void;
        auto _handle_shortcuts() -> void;

        [[nodiscard]] auto _resolve_prefab_cache() noexcept -> scene::prefab_cache*;
        [[nodiscard]] auto _resolve_asset_database() const noexcept -> assets::asset_database*;
        [[nodiscard]] auto _deduce_asset_kind(const asset_payload& payload) const noexcept -> dropped_asset_kind;

        non_null<ecs::archetype_registry> _entity_registry;
        non_null<scene::scene_manager> _scene_mgr;
        non_null<editor_selection> _selection;
        non_null<logger> _log;
        command_history* _history = default_history;
        const play_session_diff* _session_diff = default_session_diff;
        scene::prefab_cache* _prefab_cache = default_prefab_cache;
        assets::asset_database* _asset_db = default_asset_database;
        ecs::entity _selected_entity = default_selected_entity;
        bool _open = default_open;

        hierarchy_cache _cache;
        unique_ptr<scene::prefab_cache> _owned_prefab_cache;
        ecs::entity _anchor_entity = default_selected_entity;
        int _anchor_index = default_anchor_index;

        ecs::entity _renaming_entity = default_selected_entity;
        array<char, default_rename_buffer_size> _rename_buffer;
        bool _focus_rename = default_focus_rename;
        event::subscription_handle<ecs::entity_destroyed_event<ecs::entity>> _entity_destroyed_sub = {};
    };
} // namespace tempest::editor

#endif // tempest_editor_core_hierarchy_window_hpp
