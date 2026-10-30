#ifndef tempest_editor_windows_project_launcher_dialog_hpp
#define tempest_editor_windows_project_launcher_dialog_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/functional.hpp>
#include <tempest/int.hpp>
#include <tempest/recent_projects.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::editor
{
    /// @brief Enumerates tab views available within the project launcher modal dialog.
    enum class launcher_tab : uint8_t
    {
        recent_projects = 0,
        open_browse,
        create_project,
    };

    /// @brief Blocking modal dialog allowing users to open recent projects, browse existing projects,
    /// or create new projects from scratch before running or from the main menu.
    class TEMPEST_EDITOR_API project_launcher_dialog
    {
      public:
        using open_project_fn = function<bool(string_view)>;
        using create_project_fn = function<bool(string_view, string_view)>;
        using exit_fn = function<void()>;
        using has_active_project_fn = function<bool()>;

        static constexpr size_t path_buffer_capacity = 512;
        static constexpr size_t name_buffer_capacity = 128;
        static constexpr launcher_tab default_tab = launcher_tab::recent_projects;
        static constexpr bool default_is_open = false;
        static constexpr bool default_request_open_popup = false;
        static constexpr bool default_switch_tab_requested = false;
        static constexpr int32_t default_selected_recent_index = -1;

        project_launcher_dialog();
        ~project_launcher_dialog() = default;

        project_launcher_dialog(const project_launcher_dialog&) = delete;
        project_launcher_dialog(project_launcher_dialog&&) noexcept = default;
        auto operator=(const project_launcher_dialog&) -> project_launcher_dialog& = delete;
        auto operator=(project_launcher_dialog&&) noexcept -> project_launcher_dialog& = default;

        /// @brief Checks whether the launcher modal is currently requested to be displayed.
        [[nodiscard]] auto is_open() const noexcept -> bool
        {
            return _is_open;
        }

        /// @brief Opens the modal dialog and selects the designated initial tab.
        /// @param initial_tab The tab view to activate when the dialog appears.
        auto open(launcher_tab initial_tab = default_tab) noexcept -> void;

        /// @brief Dismisses the modal dialog without taking action.
        auto close() noexcept -> void;

        /// @brief Evaluates and draws the Dear ImGui modal dialog contents.
        auto draw() -> void;

        /// @brief Configures the callback invoked when a project open request is committed.
        auto set_on_open_project(open_project_fn callback) -> void
        {
            _on_open_project = tempest::move(callback);
        }

        /// @brief Configures the callback invoked when a new project creation request is committed.
        auto set_on_create_project(create_project_fn callback) -> void
        {
            _on_create_project = tempest::move(callback);
        }

        /// @brief Configures the callback invoked when the user presses Exit to terminate the editor.
        auto set_on_exit(exit_fn callback) -> void
        {
            _on_exit = tempest::move(callback);
        }

        /// @brief Configures the predicate indicating whether a project is currently open in the editor.
        auto set_has_active_project(has_active_project_fn callback) -> void
        {
            _has_active_project = tempest::move(callback);
        }

        /// @brief Retrieves the currently active or requested launcher tab.
        [[nodiscard]] auto get_active_tab() const noexcept -> launcher_tab
        {
            return _current_tab;
        }

        /// @brief Populates the open/browse text buffer for programmatic or test use.
        auto set_open_path(string_view path) -> void;

        /// @brief Populates the new project fields for programmatic or test use.
        auto set_new_project_fields(string_view project_name, string_view parent_directory) -> void;

        /// @brief Overrides the filesystem path used for session persistence.
        auto set_session_file_path(string_view path) -> void
        {
            _session_file_path = string(path);
        }

        /// @brief Retrieves the filesystem path used for session persistence.
        [[nodiscard]] auto get_session_file_path() const noexcept -> string_view
        {
            return _session_file_path;
        }

        /// @brief Retrieves the last error message encountered during project open or create operations.
        [[nodiscard]] auto get_last_error() const noexcept -> string_view
        {
            return _last_error;
        }

        /// @brief Clears the last error message.
        auto clear_last_error() noexcept -> void
        {
            _last_error.clear();
        }

        /// @brief Reloads recent project entries from the session configuration file.
        auto refresh_recent_projects() -> void;

        /// @brief Read-only accessor to the cached recent project entries.
        [[nodiscard]] auto get_recent_projects() const noexcept -> const vector<recent_project_entry>&
        {
            return _recents;
        }

        /// @brief Gets the currently selected index within the recent projects list.
        [[nodiscard]] auto get_selected_recent_index() const noexcept -> int32_t
        {
            return _selected_recent_index;
        }

        /// @brief Sets the selected index within the recent projects list.
        auto set_selected_recent_index(int32_t index) noexcept -> void
        {
            _selected_recent_index = index;
        }

      private:
        auto _draw_recents_tab() -> void;
        auto _draw_open_browse_tab() -> void;
        auto _draw_create_project_tab() -> void;

        bool _is_open = default_is_open;
        bool _request_open_popup = default_request_open_popup;
        launcher_tab _current_tab = default_tab;
        launcher_tab _requested_tab = default_tab;
        bool _switch_tab_requested = default_switch_tab_requested;

        vector<recent_project_entry> _recents = {};
        int32_t _selected_recent_index = default_selected_recent_index;

        array<char, path_buffer_capacity> _open_path_buffer = {};
        array<char, name_buffer_capacity> _new_project_name_buffer = {};
        array<char, path_buffer_capacity> _new_project_dir_buffer = {};

        string _session_file_path = string(default_session_file_path);
        string _last_error = {};

        open_project_fn _on_open_project = {};
        create_project_fn _on_create_project = {};
        exit_fn _on_exit = {};
        has_active_project_fn _has_active_project = {};
    };
} // namespace tempest::editor

#endif // tempest_editor_windows_project_launcher_dialog_hpp
