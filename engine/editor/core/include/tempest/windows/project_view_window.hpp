#ifndef tempest_editor_core_project_view_window_hpp
#define tempest_editor_core_project_view_window_hpp

#include <tempest/api.hpp>
#include <tempest/checked.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/functional.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/project_tree_model.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>
#include <tempest/windows/editor_window.hpp>

namespace tempest::assets
{
    class asset_database;
} // namespace tempest::assets

namespace tempest::editor
{
    /// @brief Segment in the breadcrumb navigation bar.
    struct TEMPEST_EDITOR_API breadcrumb_item
    {
        string name = {};
        string path = {};
    };

    /// @brief Editor window for navigating and managing project directories, mounts, and asset hierarchies.
    class TEMPEST_EDITOR_API project_view_window final : public editor_window
    {
      public:
        using inspect_asset_callback = function<void(const guid&, assets::asset_type_id, string_view)>;
        using open_external_callback = function<void(string_view)>;

        static constexpr bool default_open = true;
        static constexpr assets::asset_database* default_asset_database = nullptr;
        static constexpr size_t search_buffer_capacity = 256;
        static constexpr tree_node_kind default_selected_kind = tree_node_kind::directory;
        static constexpr bool default_show_dirty_dialog = false;

        bool open = default_open;
        char search_buffer[search_buffer_capacity] = "";
        string current_directory = {};

        /// @brief Constructs a project view window with required subsystem dependencies and optional asset database.
        /// @param in_scene_mgr Reference to the scene manager.
        /// @param in_selection Reference to the editor selection system.
        /// @param asset_db Optional pointer to the asset database.
        project_view_window(scene::scene_manager& in_scene_mgr,
                            editor_selection& in_selection,
                            assets::asset_database* asset_db = default_asset_database);

        /// @brief Constructs a project view window initialized with a preconfigured tree model and required dependencies.
        /// @param model Project tree model providing tree hierarchy data.
        /// @param in_scene_mgr Reference to the scene manager.
        /// @param in_selection Reference to the editor selection system.
        project_view_window(project_tree_model model,
                            scene::scene_manager& in_scene_mgr,
                            editor_selection& in_selection);

        ~project_view_window() override = default;

        project_view_window(const project_view_window&) = delete;
        project_view_window(project_view_window&&) noexcept = delete;
        auto operator=(const project_view_window&) -> project_view_window& = delete;
        auto operator=(project_view_window&&) noexcept -> project_view_window& = delete;

        /// @brief Returns the desired initial dock location for this window.
        /// @return dock_location::bottom dock location.
        [[nodiscard]] auto desired_initial_dock() const -> dock_location override
        {
            return dock_location::bottom;
        }

        /// @brief Returns the human-readable display title for this window.
        /// @return Window title string.
        [[nodiscard]] auto window_name() const -> string_view override
        {
            return "Project";
        }

        /// @brief Checks whether this window is supported in the given simulation state.
        /// @param state Current simulation state.
        /// @return Always true.
        [[nodiscard]] auto is_mode_supported(simulation_state) const noexcept -> bool override
        {
            return true;
        }

        /// @brief Draws the project view window contents via ImGui.
        auto draw() -> void override;

        /// @brief Retrieves the mutable project tree model backing this view.
        /// @return Reference to the tree model.
        [[nodiscard]] auto get_model() noexcept -> project_tree_model&
        {
            return _model;
        }

        /// @brief Retrieves the read-only project tree model backing this view.
        /// @return Const reference to the tree model.
        [[nodiscard]] auto get_model() const noexcept -> const project_tree_model&
        {
            return _model;
        }

        /// @brief Sets the project tree model backing this view.
        /// @param model The new tree model to adopt.
        /// @post The internal tree model is replaced with the specified model.
        auto set_model(project_tree_model model) noexcept -> void
        {
            _model = tempest::move(model);
        }

        /// @brief Sets the search filter query and updates the backing tree model.
        /// @param query Search query substring.
        /// @post `search_buffer` and backing tree model reflect `query`.
        auto set_search_query(string_view query) -> void;

        /// @brief Clears the active search filter query.
        /// @post `search_buffer` is emptied and backing model search query cleared.
        auto clear_search_query() -> void;

        /// @brief Retrieves the active search filter query.
        /// @return Current search query as a string view.
        [[nodiscard]] auto search_query() const noexcept -> string_view
        {
            return string_view{static_cast<const char*>(search_buffer)};
        }

        /// @brief Navigates to a specific directory path, expanding all ancestor nodes.
        /// @param dir_path Path of directory to navigate to.
        /// @post `current_directory` is set to `dir_path` and matching ancestors expanded.
        auto navigate_to_directory(string_view dir_path) -> void;

        /// @brief Determines the display badge for a visual project tree row.
        /// @param row Visual row to evaluate.
        /// @param asset_db Optional pointer to the asset database for procedural type resolution.
        /// @return Display badge string (e.g. "[Folder]", "[Mesh]", "[GLTF]").
        [[nodiscard]] static auto get_node_badge(const project_tree_row& row,
                                                 const assets::asset_database* asset_db = nullptr) -> string;

        /// @brief Parses a hierarchical path into discrete breadcrumb segments.
        /// @param path Directory path to parse.
        /// @return Vector of breadcrumb items containing segment name and cumulative path.
        [[nodiscard]] static auto split_breadcrumb_path(string_view path) -> vector<breadcrumb_item>;

        // Selection Management

        /// @brief Retrieves the relative path of the actively selected item.
        /// @return Relative path string view, or empty if nothing is selected.
        [[nodiscard]] auto selected_path() const noexcept -> string_view
        {
            return _selected_path;
        }

        /// @brief Retrieves the asset GUID of the actively selected item.
        /// @return Asset GUID, or empty GUID if unassigned or nothing is selected.
        [[nodiscard]] auto selected_asset_id() const noexcept -> guid
        {
            return _selected_asset_id;
        }

        /// @brief Retrieves the tree node kind of the actively selected item.
        /// @return Node kind enum value.
        [[nodiscard]] auto selected_kind() const noexcept -> tree_node_kind
        {
            return _selected_kind;
        }

        /// @brief Updates the active item selection by relative path.
        /// @param path Path of the node or row to select.
        /// @post selected_path() reflects path.
        auto set_selected_path(string_view path) -> void;

        /// @brief Clears the active item selection.
        /// @post selected_path() is empty and has_selection() returns false.
        auto clear_selection() noexcept -> void;

        /// @brief Checks whether any item is currently selected.
        /// @return True if a selection is active, false otherwise.
        [[nodiscard]] auto has_selection() const noexcept -> bool
        {
            return !_selected_path.empty();
        }

        // Double-Click Activation & Handlers

        /// @brief Activates an item in the project view hierarchy (e.g. on double-click).
        /// @param row Visual tree row representing the activated item.
        auto activate_item(const project_tree_row& row) -> void;

        /// @brief Sets the callback invoked when inspecting a prefab or material asset.
        /// @param callback Callback receiving asset GUID, type ID, and relative path.
        auto set_on_inspect_asset(inspect_asset_callback callback) -> void
        {
            _on_inspect_asset = tempest::move(callback);
        }

        /// @brief Retrieves the callback invoked when inspecting an asset.
        /// @return Const reference to the inspect callback.
        [[nodiscard]] auto get_on_inspect_asset() const noexcept -> const inspect_asset_callback&
        {
            return _on_inspect_asset;
        }

        /// @brief Sets the callback invoked when opening external source or asset files.
        /// @param callback Callback receiving disk path string view.
        auto set_on_open_external_file(open_external_callback callback) -> void
        {
            _on_open_external_file = tempest::move(callback);
        }

        /// @brief Retrieves the callback invoked when opening external files.
        /// @return Const reference to the open external file callback.
        [[nodiscard]] auto get_on_open_external_file() const noexcept -> const open_external_callback&
        {
            return _on_open_external_file;
        }

        // Dirty Scene Protection Confirmation Dialog

        /// @brief Checks whether the unsaved scene protection dialog is currently open.
        /// @return True if the dialog modal is visible, false otherwise.
        [[nodiscard]] auto is_dirty_dialog_open() const noexcept -> bool
        {
            return _show_dirty_dialog;
        }

        /// @brief Initiates opening a scene document, prompting for save confirmation if active scenes are dirty.
        /// @param disk_path Filesystem path or relative path to the scene document.
        auto open_scene_requested(string_view disk_path) -> void;

        /// @brief Confirms saving the dirty scene before loading the pending scene document.
        /// @post Dirty scene is saved, pending scene loaded, and dirty dialog closed.
        auto confirm_save_and_open() -> void;

        /// @brief Confirms discarding changes to dirty scenes before loading the pending scene document.
        /// @post Pending scene loaded without saving dirty changes, and dialog closed.
        auto confirm_discard_and_open() -> void;

        /// @brief Cancels scene opening and dismisses the dirty scene confirmation dialog.
        /// @post Pending scene load cancelled and dirty dialog closed.
        auto cancel_open_scene() -> void;

        /// @brief Retrieves the pending scene path queued for loading.
        /// @return Queued scene path string view.
        [[nodiscard]] auto pending_scene_path() const noexcept -> string_view
        {
            return _pending_scene_path;
        }

        /// @brief Retrieves the name or path of the dirty scene requiring save confirmation.
        /// @return Dirty scene identifier string view.
        [[nodiscard]] auto dirty_scene_name() const noexcept -> string_view
        {
            return _dirty_scene_name;
        }

        // Dependency Accessors

        /// @brief Retrieves the mutable scene manager instance.
        /// @return Reference to the scene manager.
        [[nodiscard]] auto scene_manager() noexcept -> scene::scene_manager&
        {
            return *_scene_mgr;
        }

        /// @brief Retrieves the read-only scene manager instance.
        /// @return Const reference to the scene manager.
        [[nodiscard]] auto scene_manager() const noexcept -> const scene::scene_manager&
        {
            return *_scene_mgr;
        }

        /// @brief Retrieves the mutable editor selection instance.
        /// @return Reference to editor selection.
        [[nodiscard]] auto selection() noexcept -> editor_selection&
        {
            return *_selection;
        }

        /// @brief Retrieves the read-only editor selection instance.
        /// @return Const reference to editor selection.
        [[nodiscard]] auto selection() const noexcept -> const editor_selection&
        {
            return *_selection;
        }

      private:
        project_tree_model _model;
        non_null<scene::scene_manager> _scene_mgr;
        non_null<editor_selection> _selection;

        string _selected_path = {};
        guid _selected_asset_id = {};
        tree_node_kind _selected_kind = default_selected_kind;

        inspect_asset_callback _on_inspect_asset = {};
        open_external_callback _on_open_external_file = {};

        bool _show_dirty_dialog = default_show_dirty_dialog;
        string _pending_scene_path = {};
        scene::scene_handle _dirty_scene_to_save = {};
        string _dirty_scene_name = {};
    };
} // namespace tempest::editor

#endif // tempest_editor_core_project_view_window_hpp
