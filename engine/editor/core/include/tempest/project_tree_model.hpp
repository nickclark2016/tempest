#ifndef tempest_editor_core_project_tree_model_hpp
#define tempest_editor_core_project_tree_model_hpp

#include <tempest/api.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/file_watcher.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/project_workspace.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

#ifndef TEMPEST_EDITOR_API
#    define TEMPEST_EDITOR_API
#endif

namespace tempest::assets
{
    class asset_database;
} // namespace tempest::assets

namespace tempest::editor
{
    /// @brief Kind of a node in the project hierarchy tree.
    enum class tree_node_kind : uint8_t
    {
        mount_root,
        directory,
        imported_file,
        loose_file,
        sub_asset,
    };

    /// @brief Node in the hierarchical project navigation tree.
    class TEMPEST_EDITOR_API project_tree_node
    {
      public:
        static constexpr tree_node_kind default_kind = tree_node_kind::directory;
        static constexpr bool default_is_expanded = false;
        static constexpr project_tree_node* default_parent = nullptr;

        string name = {};
        string path = {};
        string disk_path = {};
        string sub_asset_key = {};
        guid asset_id = {};
        assets::asset_type_id type = {};
        project_tree_node* parent = default_parent;
        vector<unique_ptr<project_tree_node>> children = {};
        tree_node_kind kind = default_kind;
        bool is_expanded = default_is_expanded;

        project_tree_node() = default;
        ~project_tree_node() = default;

        project_tree_node(const project_tree_node&) = delete;
        auto operator=(const project_tree_node&) -> project_tree_node& = delete;

        /// @brief Move constructs a project tree node, taking ownership and reparenting immediate children.
        /// @param other The rvalue project tree node to move from.
        /// @post `other.children` is empty, scalar fields of `other` reset to defaults, and `children` parent pointers reference `this`.
        project_tree_node(project_tree_node&& other) noexcept;

        /// @brief Move assigns a project tree node, taking ownership and reparenting immediate children.
        /// @param other The rvalue project tree node to move from.
        /// @return Reference to `*this`.
        /// @post `other.children` is empty, scalar fields of `other` reset to defaults, and `children` parent pointers reference `this`.
        auto operator=(project_tree_node&& other) noexcept -> project_tree_node&;

        /// @brief Appends a child node, sets its parent pointer, and returns a raw non-owning pointer.
        /// @param child Unique pointer to the child node to append.
        /// @return Pointer to the inserted child node, or `nullptr` if `child` was null.
        /// @post If `child` was non-null, the child is appended to `children` with its `parent` set to `this`.
        auto add_child(unique_ptr<project_tree_node> child) -> project_tree_node*;

        /// @brief Returns whether this node is a mount root or directory.
        /// @return `true` if `kind` is `mount_root` or `directory`, `false` otherwise.
        [[nodiscard]] auto is_directory_like() const noexcept -> bool
        {
            return kind == tree_node_kind::mount_root || kind == tree_node_kind::directory;
        }

        /// @brief Returns whether this node has child nodes.
        /// @return `true` if `children` is not empty, `false` otherwise.
        [[nodiscard]] auto has_children() const noexcept -> bool
        {
            return !children.empty();
        }
    };

    /// @brief Flattened visual row for virtualization and tree navigation.
    struct TEMPEST_EDITOR_API project_tree_row
    {
        static constexpr size_t default_depth = 0;
        static constexpr bool default_is_expanded = false;
        static constexpr bool default_has_children = false;
        static constexpr const project_tree_node* default_node = nullptr;
        static constexpr tree_node_kind default_kind = tree_node_kind::directory;

        const project_tree_node* node = default_node;
        size_t depth = default_depth;
        string_view display_label = {};
        string_view relative_path = {};
        tree_node_kind kind = default_kind;
        guid asset_id = {};
        assets::asset_type_id type = {};
        bool is_expanded = default_is_expanded;
        bool has_children = default_has_children;
    };

    /// @brief Tree model holding the hierarchical project structure.
    class TEMPEST_EDITOR_API project_tree_model
    {
      public:
        static constexpr assets::asset_database* default_asset_database = nullptr;

        /// @brief Constructs a project tree model with an optional asset database reference.
        /// @param asset_db Pointer to the asset database used for metadata lookups, or `nullptr`.
        explicit project_tree_model(assets::asset_database* asset_db = default_asset_database) noexcept;

        ~project_tree_model();

        project_tree_model(const project_tree_model&) = delete;
        auto operator=(const project_tree_model&) -> project_tree_model& = delete;

        /// @brief Move constructs a project tree model.
        /// @param other The rvalue project tree model to move from.
        /// @post `other` is left empty with a null asset database pointer.
        project_tree_model(project_tree_model&& other) noexcept;

        /// @brief Move assigns a project tree model.
        /// @param other The rvalue project tree model to move from.
        /// @return Reference to `*this`.
        /// @post `other` is left empty with a null asset database pointer.
        auto operator=(project_tree_model&& other) noexcept -> project_tree_model&;

        /// @brief Sets the asset database reference used for metadata lookups.
        /// @param asset_db Pointer to the asset database to associate with this model.
        /// @post `asset_database()` returns `asset_db`.
        auto set_asset_database(assets::asset_database* asset_db) noexcept -> void;

        /// @brief Retrieves the asset database associated with this tree model.
        /// @return Pointer to the associated asset database, or `nullptr` if unset.
        [[nodiscard]] auto asset_database() const noexcept -> assets::asset_database*;

        /// @brief Rebuilds the project tree from the specified mount points.
        /// @param mounts Span of mount points representing root folders to scan.
        /// @post Any existing tree state is cleared and replaced with nodes reflecting `mounts`.
        auto rebuild_from_mounts(span<const assets::mount_point> mounts) -> void;

        /// @brief Rebuilds the project tree from the project workspace.
        /// @param workspace The project workspace containing configured mounts and base directory.
        /// @post Any existing tree state is cleared and replaced with nodes reflecting `workspace`.
        auto rebuild_from_workspace(const assets::project_workspace& workspace) -> void;

        /// @brief Returns the root nodes of all mounts.
        /// @return Read-only span of root node unique pointers.
        [[nodiscard]] auto roots() const noexcept -> span<const unique_ptr<project_tree_node>>
        {
            return _roots;
        }

        /// @brief Returns the flattened list of currently visible rows.
        /// @return Read-only span of visible rows.
        [[nodiscard]] auto visible_rows() const noexcept -> span<const project_tree_row>
        {
            return _visible_rows;
        }

        /// @brief Returns the number of currently visible rows.
        /// @return Count of visible rows.
        [[nodiscard]] auto visible_row_count() const noexcept -> size_t
        {
            return _visible_rows.size();
        }

        /// @brief Retrieves the visible row at the specified visual index.
        /// @param index 0-based visual index of the row.
        /// @return Pointer to the visible row, or nullptr if index is out of bounds.
        [[nodiscard]] auto get_row(size_t index) const noexcept -> const project_tree_row*;

        /// @brief Rebuilds the cached list of visible rows based on expansion states or active search filter.
        /// @post `visible_rows()` reflects the updated hierarchy expansion states or search results.
        auto rebuild_visible_rows() -> void;

        /// @brief Returns whether the cached visible rows are dirty and require rebuilding.
        /// @return True if rows are dirty, false otherwise.
        [[nodiscard]] auto is_dirty() const noexcept -> bool
        {
            return _rows_dirty;
        }

        /// @brief Marks the cached visible rows as dirty, indicating a rebuild is required.
        /// @post `is_dirty()` returns true.
        auto mark_dirty() noexcept -> void
        {
            _rows_dirty = true;
        }

        /// @brief Configures whether path invalidation is deferred.
        /// @param deferred True to queue incoming invalidations without rebuilding visible rows, false to flush.
        /// @post If transitioning from true to false with pending invalidations, deferred invalidations are flushed.
        auto set_invalidation_deferred(bool deferred) -> void;

        /// @brief Returns whether path invalidation is currently deferred.
        /// @return True if invalidations are deferred, false otherwise.
        [[nodiscard]] auto is_invalidation_deferred() const noexcept -> bool;

        /// @brief Returns whether any path invalidations are currently queued.
        /// @return True if pending invalidations exist, false otherwise.
        [[nodiscard]] auto has_deferred_invalidation() const noexcept -> bool;

        /// @brief Flushes all queued path invalidations and rebuilds visible rows.
        /// @post Queued paths are cleared, `has_deferred_invalidation()` returns false, and visible rows are updated.
        auto flush_deferred_invalidation() -> void;

        /// @brief Invalidates a path on disk, rescanning its containing directory and updating visible rows.
        /// @param disk_path Path on disk to invalidate.
        /// @post If deferred, the path is queued; otherwise the containing directory is rescanned and rows rebuilt.
        auto invalidate_path(const filesystem::path& disk_path) -> void;

        /// @brief Handles a single filesystem watch event.
        /// @param event Filesystem watch event describing the changed path and action.
        /// @post If invalidation is deferred, the event paths are queued for later processing; otherwise, the affected directory is rescanned and visible rows rebuilt.
        auto handle_file_watch_event(const file_watch_event& event) -> void;

        /// @brief Handles a batch of filesystem watch events.
        /// @param events Span of filesystem watch events to process.
        /// @post If invalidation is deferred, the event paths are queued for later processing; otherwise, the affected directory is rescanned and visible rows rebuilt.
        auto handle_file_watch_events(span<const file_watch_event> events) -> void;

        /// @brief Sets the expansion state of a specific node.
        /// @param node Pointer to the node whose expansion state to update.
        /// @param expanded True to expand, false to collapse.
        /// @post If node is non-null and state changes, visible rows are rebuilt.
        auto set_expanded(project_tree_node* node, bool expanded) -> void;

        /// @brief Sets the expansion state of a node located by path.
        /// @param path Relative path, mount-prefixed path, or disk path of the node.
        /// @param expanded True to expand, false to collapse.
        /// @post If matching node is found and state changes, visible rows are rebuilt.
        auto set_expanded_by_path(string_view path, bool expanded) -> void;

        /// @brief Toggles the expansion state of a specific node.
        /// @param node Pointer to the node to toggle.
        /// @post If node is non-null, its expansion state is inverted and visible rows rebuilt.
        auto toggle_expanded(project_tree_node* node) -> void;

        /// @brief Expands all expandable nodes in the tree.
        /// @post All nodes with children have is_expanded set to true and visible rows rebuilt.
        auto expand_all() -> void;

        /// @brief Collapses all nodes in the tree.
        /// @post All nodes have is_expanded set to false and visible rows rebuilt.
        auto collapse_all() -> void;

        /// @brief Sets the active search filter query, switching to flattened search mode if non-empty.
        /// @param query Search query substring to filter nodes.
        /// @post If query differs from current search query, visible rows are rebuilt.
        auto set_search_query(string_view query) -> void;

        /// @brief Returns the current active search filter query.
        /// @return Active search query string view.
        [[nodiscard]] auto search_query() const noexcept -> string_view;

        /// @brief Clears the active search query, returning to hierarchical tree mode.
        /// @post Search query is cleared and visible rows rebuilt reflecting hierarchy.
        auto clear_search_query() -> void;

        /// @brief Returns whether a search query is currently active.
        /// @return True if search query is non-empty, false otherwise.
        [[nodiscard]] auto is_search_active() const noexcept -> bool;

        /// @brief Finds a node by relative or disk path (const overload).
        /// @param path Relative path, mount-prefixed path, or full disk path to find.
        /// @return Const pointer to the matching node, or `nullptr` if not found.
        [[nodiscard]] auto find_node_by_path(string_view path) const -> const project_tree_node*;

        /// @brief Finds a node by relative or disk path (mutable overload).
        /// @param path Relative path, mount-prefixed path, or full disk path to find.
        /// @return Mutable pointer to the matching node, or `nullptr` if not found.
        [[nodiscard]] auto find_node_by_path(string_view path) -> project_tree_node*;

        /// @brief Finds an imported file or sub-asset node by its GUID (const overload).
        /// @param id Asset GUID to locate in the tree.
        /// @return Const pointer to the matching node, or `nullptr` if not found or if `id` is nil.
        [[nodiscard]] auto find_node_by_guid(const guid& id) const -> const project_tree_node*;

        /// @brief Finds an imported file or sub-asset node by its GUID (mutable overload).
        /// @param id Asset GUID to locate in the tree.
        /// @return Mutable pointer to the matching node, or `nullptr` if not found or if `id` is nil.
        [[nodiscard]] auto find_node_by_guid(const guid& id) -> project_tree_node*;

        /// @brief Clears all roots and cached tree nodes.
        /// @post `roots()` is empty.
        auto clear() noexcept -> void;

      private:
        static constexpr bool default_rows_dirty = true;
        static constexpr bool default_invalidation_deferred = false;
        static constexpr bool default_has_deferred_invalidation = false;

        auto _find_directory_node_for_path(const filesystem::path& target_dir) -> project_tree_node*;

        vector<unique_ptr<project_tree_node>> _roots = {};
        assets::asset_database* _asset_db = default_asset_database;
        vector<project_tree_row> _visible_rows = {};
        string _search_query = {};
        vector<filesystem::path> _deferred_invalidated_paths = {};
        bool _rows_dirty = default_rows_dirty;
        bool _invalidation_deferred = default_invalidation_deferred;
        bool _has_deferred_invalidation = default_has_deferred_invalidation;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_project_tree_model_hpp
