#ifndef tempest_editor_recent_projects_hpp
#define tempest_editor_recent_projects_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::editor
{
    /// @brief Holds the metadata for an opened project recorded in the editor session.
    struct TEMPEST_EDITOR_API recent_project_entry
    {
        static constexpr uint64_t default_last_opened = 0;

        string name = {};
        string path = {};
        uint64_t last_opened = default_last_opened;

        auto operator==(const recent_project_entry& rhs) const noexcept -> bool = default;
    };

    static constexpr size_t default_max_recent_projects = 10;
    static constexpr string_view default_session_file_path = ".cache/editor_session.json";

    /// @brief Loads recent project entries from the editor session configuration file.
    /// @param session_file_path Filesystem path to the editor session JSON document.
    /// @return A vector of recent project entries ordered from most recent to oldest.
    [[nodiscard]] TEMPEST_EDITOR_API auto load_recent_projects(
        string_view session_file_path = default_session_file_path) -> vector<recent_project_entry>;

    /// @brief Saves recent project entries to the editor session file, preserving any other root keys.
    /// @param projects Collection of recent project entries to persist.
    /// @param session_file_path Filesystem path to the editor session JSON document.
    /// @return True if written successfully, false otherwise.
    TEMPEST_EDITOR_API auto save_recent_projects(span<const recent_project_entry> projects,
                                                 string_view session_file_path = default_session_file_path) -> bool;

    /// @brief Adds a recent project path to the session list, deduplicating and clamping to the limit.
    /// @param project_path Filesystem path to the .tproject file.
    /// @param project_name Friendly name for the project (inferred from path stem if empty).
    /// @param session_file_path Filesystem path to the editor session JSON document.
    /// @return True if saved successfully, false otherwise.
    TEMPEST_EDITOR_API auto add_recent_project(string_view project_path, string_view project_name = {},
                                               string_view session_file_path = default_session_file_path) -> bool;

    /// @brief Clears all recorded recent projects from the session file while preserving other root keys.
    /// @param session_file_path Filesystem path to the editor session JSON document.
    /// @return True if saved successfully, false otherwise.
    TEMPEST_EDITOR_API auto clear_recent_projects(string_view session_file_path = default_session_file_path) -> bool;

    /// @brief Removes a single project entry from the session list by path, preserving other root keys.
    /// @param project_path Filesystem path of the project to remove.
    /// @param session_file_path Filesystem path to the editor session JSON document.
    /// @return True if an entry was found and removed, false otherwise.
    TEMPEST_EDITOR_API auto remove_recent_project(string_view project_path,
                                                  string_view session_file_path = default_session_file_path) -> bool;
} // namespace tempest::editor

#endif // tempest_editor_recent_projects_hpp
