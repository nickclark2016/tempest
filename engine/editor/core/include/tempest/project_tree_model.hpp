#ifndef tempest_editor_core_project_tree_model_hpp
#define tempest_editor_core_project_tree_model_hpp

#include <tempest/api.hpp>
#include <tempest/asset_type_id.hpp>
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
        vector<unique_ptr<project_tree_node>> _roots = {};
        assets::asset_database* _asset_db = default_asset_database;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_project_tree_model_hpp
