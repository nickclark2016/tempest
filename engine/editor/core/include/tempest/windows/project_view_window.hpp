#ifndef tempest_editor_core_project_view_window_hpp
#define tempest_editor_core_project_view_window_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/checked.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/functional.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/project_tree_model.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/scene/reverse_dependency_index.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>
#include <tempest/windows/editor_window.hpp>

namespace tempest::assets
{
    class asset_database;
} // namespace tempest::assets

namespace tempest::ecs
{
    class basic_archetype_registry;
} // namespace tempest::ecs

namespace tempest::editor
{
    class command_history;
    /// @brief Segment in the breadcrumb navigation bar.
    struct TEMPEST_EDITOR_API breadcrumb_item
    {
        string name;
        string path;
    };

    /// @brief Editor window for navigating and managing project directories, mounts, and asset hierarchies.
    class TEMPEST_EDITOR_API project_view_window final : public editor_window
    {
      public:
        using inspect_asset_callback = function<void(const guid&, assets::asset_type_id, string_view)>;
        using open_external_callback = function<void(string_view)>;
        using reveal_callback = function<void(string_view)>;
        using open_scene_additive_callback = function<void(string_view)>;

        static constexpr bool default_open = true;
        static constexpr assets::asset_database* default_asset_database = nullptr;
        static constexpr command_history* default_command_history = nullptr;
        static constexpr ecs::basic_archetype_registry* default_registry = nullptr;
        static constexpr scene::reverse_dependency_index* default_reverse_dependency_index = nullptr;
        static constexpr size_t search_buffer_capacity = 256;
        static constexpr size_t rename_buffer_capacity = 256;
        static constexpr bool default_is_renaming = false;
        static constexpr bool default_rename_first_frame = false;
        static constexpr tree_node_kind default_selected_kind = tree_node_kind::directory;
        static constexpr bool default_show_dirty_dialog = false;
        static constexpr bool default_show_removal_dialog = false;
        static constexpr bool default_delete_source_file = false;

        static constexpr string_view default_new_folder_name = "New Folder";
        static constexpr string_view default_new_material_base_name = "New Material";
        static constexpr string_view default_new_material_extension = ".tmaterial";
        static constexpr string_view default_new_scene_base_name = "New Scene";
        static constexpr string_view default_new_scene_extension = ".tscene";
        static constexpr string_view default_material_importer = "material";
        static constexpr size_t max_name_collision_attempts = 10000;

        static constexpr string_view default_material_content =
            "{\n"
            "    \"shading_model\": \"openpbr\",\n"
            "    \"base_color\": [1.0, 1.0, 1.0, 1.0],\n"
            "    \"roughness\": 0.5,\n"
            "    \"metallic\": 0.0\n"
            "}\n";

        static constexpr string_view default_scene_content =
            "{\n"
            "    \"version\": 1,\n"
            "    \"entities\": []\n"
            "}\n";

        /// @brief Constructs a project view window with required subsystem dependencies and optional services.
        /// @param in_scene_mgr Reference to the scene manager.
        /// @param in_selection Reference to the editor selection system.
        /// @param asset_db Optional pointer to the asset database.
        /// @param history Optional pointer to the command history system for undoable operations.
        /// @param registry Optional pointer to the live archetype registry.
        /// @param reverse_deps Optional pointer to the reverse dependency index.
        project_view_window(scene::scene_manager& in_scene_mgr,
                            editor_selection& in_selection,
                            assets::asset_database* asset_db = default_asset_database,
                            command_history* history = default_command_history,
                            ecs::basic_archetype_registry* registry = default_registry,
                            scene::reverse_dependency_index* reverse_deps = default_reverse_dependency_index);

        /// @brief Constructs a project view window initialized with a preconfigured tree model and dependencies.
        /// @param model Project tree model providing tree hierarchy data.
        /// @param in_scene_mgr Reference to the scene manager.
        /// @param in_selection Reference to the editor selection system.
        /// @param asset_db Optional pointer to the asset database.
        /// @param history Optional pointer to the command history system for undoable operations.
        /// @param registry Optional pointer to the live archetype registry.
        /// @param reverse_deps Optional pointer to the reverse dependency index.
        project_view_window(project_tree_model model,
                            scene::scene_manager& in_scene_mgr,
                            editor_selection& in_selection,
                            assets::asset_database* asset_db = default_asset_database,
                            command_history* history = default_command_history,
                            ecs::basic_archetype_registry* registry = default_registry,
                            scene::reverse_dependency_index* reverse_deps = default_reverse_dependency_index);

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

        /// @brief Checks whether the project view window is open and visible.
        /// @return True if open, false if hidden.
        [[nodiscard]] auto is_open() const noexcept -> bool
        {
            return _open;
        }

        /// @brief Sets whether the project view window is open and visible.
        /// @param in_open Visibility state.
        auto set_open(bool in_open) noexcept -> void
        {
            _open = in_open;
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
        /// @post `_search_buffer` and backing tree model reflect `query`.
        auto set_search_query(string_view query) -> void;

        /// @brief Clears the active search filter query.
        /// @post `_search_buffer` is emptied and backing model search query cleared.
        auto clear_search_query() -> void;

        /// @brief Retrieves the active search filter query.
        /// @return Current search query as a string view.
        [[nodiscard]] auto search_query() const noexcept -> string_view
        {
            return string_view{_search_buffer.data()};
        }

        /// @brief Retrieves the active directory path being navigated.
        /// @return Current directory path string view.
        [[nodiscard]] auto current_directory() const noexcept -> string_view
        {
            return _current_directory;
        }

        /// @brief Sets the active directory path.
        /// @param dir_path Directory path to set.
        auto set_current_directory(string_view dir_path) -> void;

        /// @brief Navigates to a specific directory path, expanding all ancestor nodes.
        /// @param dir_path Path of directory to navigate to.
        /// @post `_current_directory` is set to `dir_path` and matching ancestors expanded.
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

        /// @brief Sets the callback invoked when revealing an item in the platform file explorer.
        /// @param callback Callback receiving disk path string view.
        /// @post The callback is installed and will be invoked during explorer reveal operations.
        auto set_on_reveal_in_explorer(reveal_callback callback) -> void
        {
            _on_reveal_in_explorer = tempest::move(callback);
        }

        /// @brief Retrieves the callback invoked when revealing in explorer.
        /// @return Const reference to the reveal callback.
        [[nodiscard]] auto get_on_reveal_in_explorer() const noexcept -> const reveal_callback&
        {
            return _on_reveal_in_explorer;
        }

        /// @brief Sets the callback invoked when opening a scene additively.
        /// @param callback Callback receiving scene disk path string view.
        /// @post The callback is installed and will be invoked when scenes are loaded additively.
        auto set_on_open_scene_additive(open_scene_additive_callback callback) -> void
        {
            _on_open_scene_additive = tempest::move(callback);
        }

        /// @brief Retrieves the callback invoked when opening a scene additively.
        /// @return Const reference to the additive scene load callback.
        [[nodiscard]] auto get_on_open_scene_additive() const noexcept -> const open_scene_additive_callback&
        {
            return _on_open_scene_additive;
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

        // Inline Renaming & Validation

        /// @brief Initiates inline renaming for the item at the specified relative or display path.
        /// @param path Relative or display path of the item to rename.
        /// @return True if rename mode was successfully initiated, false if item not found or non-renamable.
        /// @post If successful, is_renaming() is true, renaming_path() equals path, and model invalidation is deferred.
        auto start_rename(string_view path) -> bool;

        /// @brief Commits the pending rename operation, renaming the disk file and updating model and selection.
        /// @return True if rename succeeded or name was unchanged, false if validation failed.
        /// @post If successful, disk files are renamed, model updated, and rename mode deactivated.
        auto commit_rename() -> bool;

        /// @brief Cancels any active inline rename operation and restores model invalidation.
        /// @post is_renaming() returns false and rename state is reset.
        auto cancel_rename() noexcept -> void;

        /// @brief Checks whether an inline rename operation is currently active.
        /// @return True if currently renaming an item, false otherwise.
        [[nodiscard]] auto is_renaming() const noexcept -> bool
        {
            return _is_renaming;
        }

        /// @brief Retrieves the relative path of the item currently being renamed.
        /// @return Relative path string view, or empty if not renaming.
        [[nodiscard]] auto renaming_path() const noexcept -> string_view
        {
            return _renaming_path;
        }

        /// @brief Retrieves the current text in the inline rename buffer.
        /// @return Current rename buffer contents as a string view.
        [[nodiscard]] auto rename_buffer() const noexcept -> string_view
        {
            return string_view{_rename_buffer.data()};
        }

        /// @brief Sets the text in the inline rename buffer.
        /// @param text The new text to copy into the buffer.
        auto set_rename_buffer(string_view text) -> void;

        /// @brief Retrieves the active character validation error message, if any.
        /// @return Validation error string view, or empty if buffer is valid.
        [[nodiscard]] auto rename_error() const noexcept -> string_view
        {
            return _rename_error;
        }

        /// @brief Validates a prospective filename for illegal characters, emptiness, or sibling collisions.
        /// @param new_name Candidate filename to validate.
        /// @return Error message string view if invalid, or nullopt if valid.
        [[nodiscard]] auto validate_rename_name(string_view new_name) const -> optional<string_view>;

        // Asset & Folder Creation

        /// @brief Creates a new folder on disk within the specified or inferred parent directory and activates inline rename.
        /// @param parent_path Target directory or sibling item relative path. If empty, uses active selection or current directory.
        /// @return True if folder was successfully created on disk and inline rename initiated, false on failure.
        /// @pre If parent_path is non-empty, it must reference an existing path in the tree model; if empty, at least one mount root must be loaded.
        /// @post Target folder is expanded, created on disk, added to model, selected, and inline rename active.
        auto create_folder(string_view parent_path = {}) -> bool;

        /// @brief Creates a new default material asset (.tmaterial and .tmeta) and activates inline rename.
        /// @param parent_path Target directory or sibling item relative path. If empty, uses active selection or current directory.
        /// @return True if material and sidecar metadata were successfully created and rename initiated, false on failure.
        /// @pre If parent_path is non-empty, it must reference an existing path in the tree model; if empty, at least one mount root must be loaded.
        /// @post Material and sidecar created on disk, added to model, selected, and inline rename active.
        auto create_material(string_view parent_path = {}) -> bool;

        /// @brief Creates a new default scene asset (.tscene) and activates inline rename.
        /// @param parent_path Target directory or sibling item relative path. If empty, uses active selection or current directory.
        /// @return True if scene document was successfully created on disk and inline rename initiated, false on failure.
        /// @pre If parent_path is non-empty, it must reference an existing path in the tree model; if empty, at least one mount root must be loaded.
        /// @post Scene document created on disk, added to model, selected, and inline rename active.
        auto create_scene(string_view parent_path = {}) -> bool;

        // Asset Import, Reimport, Additive Loading & Explorer Actions

        /// @brief Imports a loose file or recursively imports all loose files in a directory.
        /// @param path Relative or disk path of the loose file or directory. If empty, uses active selection or current directory.
        /// @return True if at least one asset was successfully imported, false on failure or if path is invalid.
        /// @post Generates .tmeta sidecars for loose files, notifies the asset database, and invalidates the model path.
        auto import_asset(string_view path = {}) -> bool;

        /// @brief Reimports an existing imported asset or recursively reimports all assets in a directory.
        /// @param path Relative or disk path of the imported asset or directory. If empty, uses active selection or current directory.
        /// @return True if at least one asset was successfully reimported, false on failure or if path is invalid.
        /// @post Preserves source GUID and sub-asset keys, notifies the asset database, and invalidates the model path.
        auto reimport_asset(string_view path = {}) -> bool;

        /// @brief Loads a scene document additively alongside currently loaded scenes without prompting for save confirmation.
        /// @param disk_path Filesystem path or relative path to the scene document to load additively.
        /// @return True if scene was successfully loaded additively, false on failure.
        /// @pre disk_path must not be empty and must reference an existing scene document.
        /// @post Scene document is loaded into scene manager additively and on_open_scene_additive callback invoked if set.
        auto open_scene_additive(string_view disk_path) -> bool;

        /// @brief Reveals the item in the native platform file explorer (or shell).
        /// @param path Relative or disk path of the item to reveal. If empty, uses active selection or current directory.
        /// @return True if the item was resolved and revealed, false otherwise.
        /// @post Invokes the registered reveal callback or opens the native platform file manager.
        auto reveal_in_explorer(string_view path = {}) -> bool;

        /// @brief Deduces the importer identifier for the specified file or extension.
        /// @param path_or_extension Filename, file path, or extension to inspect.
        /// @return Importer identifier string view if importable, or nullopt if unsupported.
        [[nodiscard]] auto deduce_importer(string_view path_or_extension) const -> optional<string_view>;

        /// @brief Checks whether the specified file or extension is recognized as an importable asset.
        /// @param path_or_extension Filename, file path, or extension to inspect.
        /// @return True if an asset importer is registered or recognized for this file format, false otherwise.
        [[nodiscard]] auto is_importable(string_view path_or_extension) const -> bool;

        // Asset Removal & Dependency Resolution

        /// @brief Requests initiating the removal flow for the asset or folder at target_path.
        /// @param target_path Relative path, display label, or disk path of the item to remove.
        /// @return True if target was resolved and removal modal queued, false if invalid or mount root.
        /// @post If true, is_removal_dialog_open() returns true and referencing_locations() reflects dependencies.
        auto request_remove_asset(string_view target_path) -> bool;

        /// @brief Checks whether the asset removal modal dialog is currently open.
        /// @return True if removal confirmation or resolution dialog is visible, false otherwise.
        [[nodiscard]] auto is_removal_dialog_open() const noexcept -> bool
        {
            return _show_removal_dialog;
        }

        /// @brief Checks whether the pending removal target has any recorded reverse dependencies.
        /// @return True if at least one reference was found, false otherwise.
        [[nodiscard]] auto has_removal_references() const noexcept -> bool
        {
            return !_removal_references.empty();
        }

        /// @brief Retrieves the relative path of the item targeted for removal.
        /// @return Target path string view, or empty if no removal is pending.
        [[nodiscard]] auto removal_target_path() const noexcept -> string_view
        {
            return _removal_target_path;
        }

        /// @brief Retrieves the resolved asset GUID of the item targeted for removal.
        /// @return Asset GUID, or empty GUID if unassigned.
        [[nodiscard]] auto removal_asset_guid() const noexcept -> const guid&
        {
            return _removal_asset_guid;
        }

        /// @brief Returns the collection of locations actively referencing the pending removal asset.
        /// @return Non-owning contiguous span of reference locations.
        [[nodiscard]] auto referencing_locations() const noexcept -> span<const scene::asset_reference_location>
        {
            return span<const scene::asset_reference_location>{_removal_references.data(), _removal_references.size()};
        }

        /// @brief Checks whether the "Delete source file from disk" option is currently checked.
        /// @return True if source file deletion is enabled, false if preserving on disk.
        [[nodiscard]] auto is_delete_source_file_checked() const noexcept -> bool
        {
            return _delete_source_file;
        }

        /// @brief Configures whether the source file on disk will be deleted during clean removal.
        /// @param checked True to delete source file, false to preserve.
        auto set_delete_source_file_checked(bool checked) noexcept -> void
        {
            _delete_source_file = checked;
        }

        /// @brief Confirms clean asset removal without references, deleting metadata and updating model.
        /// @return True if removal succeeded, false on error.
        /// @post Removal dialog is closed, sidecars deleted, source file deleted if checked, and model updated.
        auto confirm_clean_removal() -> bool;

        /// @brief Aborts the pending asset removal operation without making any changes.
        /// @post is_removal_dialog_open() returns false and queued references are cleared.
        auto confirm_removal_abort() -> void;

        /// @brief Confirms removing the asset while leaving referencing locations intact as placeholders.
        /// @return True if asset was cleanly removed, false on failure.
        /// @post Target asset is removed from disk/database, references preserved, and dialog closed.
        auto confirm_removal_leave_dangling() -> bool;

        /// @brief Confirms removing the asset and destroying all live and on-disk referencing entities.
        /// @return True if references were stripped and asset cleanly removed, false on failure.
        /// @post Live referencing entities destroyed, disk scene files stripped, index cleared, and asset deleted.
        auto confirm_removal_remove_referencing_entities() -> bool;

        // Dependency Accessors

        /// @brief Injects the command history instance used for transactional operations.
        /// @param history Pointer to the command history, or nullptr to disable undo/redo tracking.
        auto set_command_history(command_history* history) noexcept -> void
        {
            _history = history;
        }

        /// @brief Retrieves the injected command history instance.
        /// @return Pointer to command history, or nullptr if unset.
        [[nodiscard]] auto get_command_history() const noexcept -> command_history*
        {
            return _history;
        }

        /// @brief Injects the live archetype registry used for resolving active scene entities.
        /// @param registry Pointer to the archetype registry, or nullptr if unset.
        auto set_registry(ecs::basic_archetype_registry* registry) noexcept -> void
        {
            _registry = registry;
        }

        /// @brief Retrieves the injected archetype registry instance.
        /// @return Pointer to archetype registry, or nullptr if unset.
        [[nodiscard]] auto registry() const noexcept -> ecs::basic_archetype_registry*
        {
            return _registry;
        }

        /// @brief Injects an explicit reverse dependency index instance.
        /// @param reverse_deps Pointer to the index, or nullptr to fall back to the scene manager.
        auto set_reverse_dependency_index(scene::reverse_dependency_index* reverse_deps) noexcept -> void
        {
            _reverse_deps = reverse_deps;
        }

        /// @brief Resolves the effective reverse dependency index, falling back to scene_manager if not explicitly set.
        /// @return Pointer to reverse dependency index, or nullptr if unavailable.
        [[nodiscard]] auto reverse_dependency_index() const noexcept -> scene::reverse_dependency_index*;

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
        [[nodiscard]] auto _find_target_node(string_view path) const -> const project_tree_node*;
        [[nodiscard]] auto _resolve_target_folder_node(string_view parent_path) const -> const project_tree_node*;
        [[nodiscard]] auto _resolve_disk_path(string_view path) const -> filesystem::path;
        [[nodiscard]] auto _resolve_asset_database() const noexcept -> const assets::asset_database*;
        [[nodiscard]] auto _resolve_mutable_asset_database() noexcept -> assets::asset_database*;
        auto _finish_creation(const project_tree_node& target_folder_node,
                              const filesystem::path& new_disk_path,
                              string_view unique_name) -> bool;

        project_tree_model _model;
        non_null<scene::scene_manager> _scene_mgr;
        non_null<editor_selection> _selection;

        assets::asset_database* _asset_db = default_asset_database;
        command_history* _history = default_command_history;
        ecs::basic_archetype_registry* _registry = default_registry;
        scene::reverse_dependency_index* _reverse_deps = default_reverse_dependency_index;

        bool _show_removal_dialog = default_show_removal_dialog;
        bool _delete_source_file = default_delete_source_file;
        string _removal_target_path = {};
        string _removal_item_name = {};
        string _removal_disk_path = {};
        guid _removal_asset_guid = {};
        vector<scene::asset_reference_location> _removal_references = {};

        bool _open = default_open;
        string _current_directory;
        array<char, search_buffer_capacity> _search_buffer = {};

        string _selected_path;
        guid _selected_asset_id = {};
        tree_node_kind _selected_kind = default_selected_kind;

        inspect_asset_callback _on_inspect_asset;
        open_external_callback _on_open_external_file;
        reveal_callback _on_reveal_in_explorer;
        open_scene_additive_callback _on_open_scene_additive;

        bool _show_dirty_dialog = default_show_dirty_dialog;
        string _pending_scene_path;
        scene::scene_handle _dirty_scene_to_save;
        string _dirty_scene_name;

        bool _is_renaming = default_is_renaming;
        string _renaming_path;
        array<char, rename_buffer_capacity> _rename_buffer = {};
        bool _rename_first_frame = default_rename_first_frame;
        string _rename_error;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_project_view_window_hpp
