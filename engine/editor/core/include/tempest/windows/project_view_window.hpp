#ifndef tempest_editor_core_project_view_window_hpp
#define tempest_editor_core_project_view_window_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/project_tree_model.hpp>
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
        static constexpr bool default_open = true;
        static constexpr assets::asset_database* default_asset_database = nullptr;
        static constexpr size_t search_buffer_capacity = 256;

        bool open = default_open;
        char search_buffer[search_buffer_capacity] = "";
        string current_directory = {};

        /// @brief Constructs a project view window with an optional asset database reference.
        /// @param asset_db Pointer to the asset database, or nullptr if none.
        explicit project_view_window(assets::asset_database* asset_db = default_asset_database);

        /// @brief Constructs a project view window initialized with a preconfigured tree model.
        /// @param model Project tree model providing tree hierarchy data.
        explicit project_view_window(project_tree_model model);

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

      private:
        project_tree_model _model;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_project_view_window_hpp
