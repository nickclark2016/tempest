#include <tempest/project_tree_model.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/utility.hpp>

namespace tempest::editor
{
    namespace
    {
        auto is_meta_file(string_view filename) noexcept -> bool
        {
            return tempest::ends_with(filename, ".tmeta");
        }

        auto normalize_path_string(string_view str) -> string
        {
            auto result = string{str};
            for (auto& ch : result)
            {
                if (ch == '\\')
                {
                    ch = '/';
                }
            }
            return result;
        }

        struct dir_item
        {
            filesystem::path path = {};
            string filename = {};
            bool is_directory = false;
        };

        auto populate_directory_children(project_tree_node* parent_node,
                                         const filesystem::path& dir_path,
                                         string_view relative_path,
                                         assets::asset_database* asset_db) -> void
        {
            auto items = vector<dir_item>{};
            for (const auto& entry : filesystem::directory_iterator(dir_path))
            {
                const auto entry_path = entry.path();
                const auto fname = entry_path.filename().string();
                if (is_meta_file(fname))
                {
                    continue;
                }

                auto item = dir_item{
                    .path = entry_path,
                    .filename = fname,
                    .is_directory = entry.is_directory(),
                };
                items.push_back(tempest::move(item));
            }

            tempest::sort(items.begin(), items.end(), [](const dir_item& lhs, const dir_item& rhs) {
                if (lhs.is_directory != rhs.is_directory)
                {
                    return lhs.is_directory > rhs.is_directory;
                }
                return lhs.filename < rhs.filename;
            });

            for (const auto& item : items)
            {
                auto child_rel_path = string{};
                if (!relative_path.empty())
                {
                    child_rel_path.append(relative_path.data(), relative_path.size());
                    child_rel_path.push_back('/');
                }
                child_rel_path.append(item.filename);

                if (item.is_directory)
                {
                    auto dir_node = tempest::make_unique<project_tree_node>();
                    dir_node->name = item.filename;
                    dir_node->path = tempest::move(child_rel_path);
                    dir_node->disk_path = item.path.generic_string();
                    dir_node->kind = tree_node_kind::directory;

                    populate_directory_children(dir_node.get(), item.path, dir_node->path, asset_db);
                    parent_node->add_child(tempest::move(dir_node));
                }
                else
                {
                    auto file_node = tempest::make_unique<project_tree_node>();
                    file_node->name = item.filename;
                    file_node->path = tempest::move(child_rel_path);
                    file_node->disk_path = item.path.generic_string();

                    auto meta_path = file_node->disk_path;
                    meta_path.append(".tmeta");
                    auto meta_opt = assets::sidecar_meta::load_from_file(meta_path);
                    if (!meta_opt.has_value() && asset_db != nullptr)
                    {
                        meta_opt = asset_db->find_sidecar(file_node->path);
                    }

                    if (meta_opt.has_value())
                    {
                        file_node->kind = tree_node_kind::imported_file;
                        file_node->asset_id = meta_opt->source_guid;

                        if (asset_db != nullptr)
                        {
                            const auto* entry = asset_db->find_by_guid(file_node->asset_id);
                            if (entry != nullptr)
                            {
                                file_node->type = entry->type;
                            }
                            else
                            {
                                const auto* entry_by_path = asset_db->find_by_path(file_node->path);
                                if (entry_by_path != nullptr)
                                {
                                    file_node->type = entry_by_path->type;
                                }
                            }
                        }

                        for (const auto& sub_pair : meta_opt->sub_assets)
                        {
                            const auto& sub_key = sub_pair.first;
                            const auto& sub_guid = sub_pair.second;

                            auto sub_node = tempest::make_unique<project_tree_node>();
                            sub_node->name = sub_key;
                            sub_node->path = file_node->path;
                            sub_node->path.push_back(':');
                            sub_node->path.append(sub_key);
                            sub_node->disk_path = file_node->disk_path;
                            sub_node->asset_id = sub_guid;
                            sub_node->sub_asset_key = sub_key;
                            sub_node->kind = tree_node_kind::sub_asset;

                            const auto type_it = meta_opt->sub_asset_types.find(sub_key);
                            if (type_it != meta_opt->sub_asset_types.end())
                            {
                                sub_node->type = type_it->second;
                            }
                            else if (asset_db != nullptr)
                            {
                                const auto* sub_entry = asset_db->find_by_guid(sub_guid);
                                if (sub_entry != nullptr)
                                {
                                    sub_node->type = sub_entry->type;
                                }
                            }

                            file_node->add_child(tempest::move(sub_node));
                        }

                        tempest::sort(file_node->children.begin(), file_node->children.end(),
                                      [](const auto& lhs, const auto& rhs) { return lhs->name < rhs->name; });
                    }
                    else
                    {
                        file_node->kind = tree_node_kind::loose_file;
                        file_node->asset_id = {};
                        file_node->type = {};
                    }

                    parent_node->add_child(tempest::move(file_node));
                }
            }
        }

        template <typename NodePtr>
        auto find_node_by_path_impl(NodePtr node, string_view norm_query, string_view stripped_query) -> NodePtr
        {
            if (node == nullptr)
            {
                return nullptr;
            }

            if (node->path == norm_query)
            {
                return node;
            }

            if (!stripped_query.empty() && node->path == stripped_query)
            {
                return node;
            }

            if (!node->disk_path.empty() && node->disk_path == norm_query)
            {
                return node;
            }

            for (auto& child : node->children)
            {
                auto* found = find_node_by_path_impl(child.get(), norm_query, stripped_query);
                if (found != nullptr)
                {
                    return found;
                }
            }

            return nullptr;
        }

        template <typename NodePtr>
        auto find_node_by_guid_impl(NodePtr node, const guid& id) -> NodePtr
        {
            if (node == nullptr)
            {
                return nullptr;
            }

            if (node->asset_id == id)
            {
                return node;
            }

            for (auto& child : node->children)
            {
                auto* found = find_node_by_guid_impl(child.get(), id);
                if (found != nullptr)
                {
                    return found;
                }
            }

            return nullptr;
        }

        auto to_lower_char(char ch) noexcept -> char
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                return static_cast<char>(ch + ('a' - 'A'));
            }
            return ch;
        }

        auto case_insensitive_contains(string_view text, string_view query) noexcept -> bool
        {
            if (query.empty())
            {
                return true;
            }
            if (text.size() < query.size())
            {
                return false;
            }

            const auto max_index = text.size() - query.size();
            for (auto index = size_t{0}; index <= max_index; ++index)
            {
                auto matched = true;
                for (auto offset = size_t{0}; offset < query.size(); ++offset)
                {
                    if (to_lower_char(text[index + offset]) != to_lower_char(query[offset]))
                    {
                        matched = false;
                        break;
                    }
                }
                if (matched)
                {
                    return true;
                }
            }
            return false;
        }

        auto append_hierarchy_rows(vector<project_tree_row>& rows, const project_tree_node* node, size_t depth) -> void
        {
            if (node == nullptr)
            {
                return;
            }

            auto row = project_tree_row{
                .node = node,
                .depth = depth,
                .display_label = node->name,
                .relative_path = node->path,
                .kind = node->kind,
                .asset_id = node->asset_id,
                .type = node->type,
                .is_expanded = node->is_expanded,
                .has_children = node->has_children(),
            };
            rows.push_back(tempest::move(row));

            if (node->is_expanded)
            {
                for (const auto& child : node->children)
                {
                    if (child)
                    {
                        append_hierarchy_rows(rows, child.get(), depth + 1);
                    }
                }
            }
        }

        auto append_search_matches(vector<project_tree_row>& rows, const project_tree_node* node, string_view query) -> void
        {
            if (node == nullptr)
            {
                return;
            }

            const auto is_file_or_sub_asset = (node->kind == tree_node_kind::imported_file ||
                                               node->kind == tree_node_kind::loose_file ||
                                               node->kind == tree_node_kind::sub_asset);

            if (is_file_or_sub_asset)
            {
                if (case_insensitive_contains(node->name, query) ||
                    case_insensitive_contains(node->path, query))
                {
                    auto row = project_tree_row{
                        .node = node,
                        .depth = 0,
                        .display_label = node->name,
                        .relative_path = node->path,
                        .kind = node->kind,
                        .asset_id = node->asset_id,
                        .type = node->type,
                        .is_expanded = false,
                        .has_children = false,
                    };
                    rows.push_back(tempest::move(row));
                }
            }

            for (const auto& child : node->children)
            {
                if (child)
                {
                    append_search_matches(rows, child.get(), query);
                }
            }
        }

        auto set_all_expanded_impl(project_tree_node* node, bool expanded) -> void
        {
            if (node == nullptr)
            {
                return;
            }

            node->is_expanded = expanded;

            for (auto& child : node->children)
            {
                if (child)
                {
                    set_all_expanded_impl(child.get(), expanded);
                }
            }
        }
    } // namespace

    // =========================================================================
    // project_tree_node Special Member Functions & Methods
    // =========================================================================

    project_tree_node::project_tree_node(project_tree_node&& other) noexcept
        : name{tempest::move(other.name)},
          path{tempest::move(other.path)},
          disk_path{tempest::move(other.disk_path)},
          sub_asset_key{tempest::move(other.sub_asset_key)},
          asset_id{tempest::exchange(other.asset_id, {})},
          type{tempest::exchange(other.type, {})},
          parent{tempest::exchange(other.parent, default_parent)},
          children{tempest::move(other.children)},
          kind{tempest::exchange(other.kind, default_kind)},
          is_expanded{tempest::exchange(other.is_expanded, default_is_expanded)}
    {
        for (auto& child : children)
        {
            if (child)
            {
                child->parent = this;
            }
        }
    }

    auto project_tree_node::operator=(project_tree_node&& other) noexcept -> project_tree_node&
    {
        if (this == &other)
        {
            return *this;
        }

        name = tempest::move(other.name);
        path = tempest::move(other.path);
        disk_path = tempest::move(other.disk_path);
        sub_asset_key = tempest::move(other.sub_asset_key);
        asset_id = tempest::exchange(other.asset_id, {});
        type = tempest::exchange(other.type, {});
        parent = tempest::exchange(other.parent, default_parent);
        children = tempest::move(other.children);
        kind = tempest::exchange(other.kind, default_kind);
        is_expanded = tempest::exchange(other.is_expanded, default_is_expanded);

        for (auto& child : children)
        {
            if (child)
            {
                child->parent = this;
            }
        }

        return *this;
    }

    auto project_tree_node::add_child(unique_ptr<project_tree_node> child) -> project_tree_node*
    {
        if (!child)
        {
            return nullptr;
        }
        child->parent = this;
        children.push_back(tempest::move(child));
        return children.back().get();
    }

    // =========================================================================
    // project_tree_model Special Member Functions & Methods
    // =========================================================================

    project_tree_model::project_tree_model(assets::asset_database* asset_db) noexcept
        : _asset_db{asset_db}
    {
    }

    project_tree_model::~project_tree_model() = default;

    project_tree_model::project_tree_model(project_tree_model&& other) noexcept
        : _roots{tempest::move(other._roots)},
          _asset_db{tempest::exchange(other._asset_db, default_asset_database)},
          _visible_rows{tempest::move(other._visible_rows)},
          _search_query{tempest::move(other._search_query)},
          _rows_dirty{tempest::exchange(other._rows_dirty, default_rows_dirty)}
    {
    }

    auto project_tree_model::operator=(project_tree_model&& other) noexcept -> project_tree_model&
    {
        if (this == &other)
        {
            return *this;
        }

        _roots = tempest::move(other._roots);
        _asset_db = tempest::exchange(other._asset_db, default_asset_database);
        _visible_rows = tempest::move(other._visible_rows);
        _search_query = tempest::move(other._search_query);
        _rows_dirty = tempest::exchange(other._rows_dirty, default_rows_dirty);

        return *this;
    }

    auto project_tree_model::set_asset_database(assets::asset_database* asset_db) noexcept -> void
    {
        _asset_db = asset_db;
    }

    auto project_tree_model::asset_database() const noexcept -> assets::asset_database*
    {
        return _asset_db;
    }

    auto project_tree_model::rebuild_from_mounts(span<const assets::mount_point> mounts) -> void
    {
        clear();

        for (const auto& mount : mounts)
        {
            auto root_node = tempest::make_unique<project_tree_node>();
            root_node->kind = tree_node_kind::mount_root;
            root_node->is_expanded = true;

            const auto mount_path = filesystem::path(mount.path);
            auto name = string{};
            if (!mount.alias.empty())
            {
                name = mount.alias;
            }
            else
            {
                name = mount_path.filename().string();
                if (name.empty())
                {
                    name = mount.path;
                }
            }

            root_node->name = name;
            root_node->path = name;
            root_node->disk_path = mount_path.generic_string();

            if (filesystem::exists(mount_path) && filesystem::is_directory(mount_path))
            {
                populate_directory_children(root_node.get(), mount_path, "", _asset_db);
            }

            _roots.push_back(tempest::move(root_node));
        }

        rebuild_visible_rows();
    }

    auto project_tree_model::rebuild_from_workspace(const assets::project_workspace& workspace) -> void
    {
        auto resolved_mounts = vector<assets::mount_point>{};
        resolved_mounts.reserve(workspace.mounts.size());

        for (const auto& mount : workspace.mounts)
        {
            auto resolved = mount;
            if (!filesystem::path(mount.path).is_absolute() && !workspace.root_directory.empty())
            {
                resolved.path = (filesystem::path(workspace.root_directory) / filesystem::path(mount.path)).generic_string();
            }
            resolved_mounts.push_back(tempest::move(resolved));
        }

        rebuild_from_mounts(resolved_mounts);
    }

    auto project_tree_model::find_node_by_path(string_view path) -> project_tree_node*
    {
        if (path.empty())
        {
            return nullptr;
        }

        const auto norm_query = normalize_path_string(path);

        for (auto& root : _roots)
        {
            if (!root)
            {
                continue;
            }

            auto stripped_query = string_view{};
            if (tempest::starts_with(string_view{norm_query}, string_view{root->name}) &&
                norm_query.size() > root->name.size() &&
                norm_query[root->name.size()] == '/')
            {
                stripped_query = string_view{norm_query.data() + root->name.size() + 1,
                                             norm_query.size() - root->name.size() - 1};
            }

            auto* found = find_node_by_path_impl(root.get(), norm_query, stripped_query);
            if (found != nullptr)
            {
                return found;
            }
        }

        return nullptr;
    }

    auto project_tree_model::find_node_by_path(string_view path) const -> const project_tree_node*
    {
        if (path.empty())
        {
            return nullptr;
        }

        const auto norm_query = normalize_path_string(path);

        for (const auto& root : _roots)
        {
            if (!root)
            {
                continue;
            }

            auto stripped_query = string_view{};
            if (tempest::starts_with(string_view{norm_query}, string_view{root->name}) &&
                norm_query.size() > root->name.size() &&
                norm_query[root->name.size()] == '/')
            {
                stripped_query = string_view{norm_query.data() + root->name.size() + 1,
                                             norm_query.size() - root->name.size() - 1};
            }

            const auto* found = find_node_by_path_impl(static_cast<const project_tree_node*>(root.get()), norm_query, stripped_query);
            if (found != nullptr)
            {
                return found;
            }
        }

        return nullptr;
    }

    auto project_tree_model::find_node_by_guid(const guid& id) -> project_tree_node*
    {
        if (id == guid{})
        {
            return nullptr;
        }

        for (auto& root : _roots)
        {
            auto* found = find_node_by_guid_impl(root.get(), id);
            if (found != nullptr)
            {
                return found;
            }
        }

        return nullptr;
    }

    auto project_tree_model::find_node_by_guid(const guid& id) const -> const project_tree_node*
    {
        if (id == guid{})
        {
            return nullptr;
        }

        for (const auto& root : _roots)
        {
            const auto* found = find_node_by_guid_impl(root.get(), id);
            if (found != nullptr)
            {
                return found;
            }
        }

        return nullptr;
    }

    auto project_tree_model::clear() noexcept -> void
    {
        _roots.clear();
        _visible_rows.clear();
        _search_query.clear();
        _rows_dirty = false;
    }

    auto project_tree_model::get_row(size_t index) const noexcept -> const project_tree_row*
    {
        if (index < _visible_rows.size())
        {
            return &_visible_rows[index];
        }
        return nullptr;
    }

    auto project_tree_model::rebuild_visible_rows() -> void
    {
        _visible_rows.clear();

        if (_search_query.empty())
        {
            for (const auto& root : _roots)
            {
                if (root)
                {
                    append_hierarchy_rows(_visible_rows, root.get(), 0);
                }
            }
        }
        else
        {
            for (const auto& root : _roots)
            {
                if (root)
                {
                    append_search_matches(_visible_rows, root.get(), _search_query);
                }
            }

            tempest::sort(_visible_rows.begin(), _visible_rows.end(),
                          [](const project_tree_row& lhs, const project_tree_row& rhs) {
                              if (lhs.relative_path != rhs.relative_path)
                              {
                                  return lhs.relative_path < rhs.relative_path;
                              }
                              return lhs.display_label < rhs.display_label;
                          });
        }

        _rows_dirty = false;
    }

    auto project_tree_model::set_expanded(project_tree_node* node, bool expanded) -> void
    {
        if (node == nullptr || node->is_expanded == expanded)
        {
            return;
        }

        node->is_expanded = expanded;
        _rows_dirty = true;
        rebuild_visible_rows();
    }

    auto project_tree_model::set_expanded_by_path(string_view path, bool expanded) -> void
    {
        auto* node = find_node_by_path(path);
        if (node != nullptr)
        {
            set_expanded(node, expanded);
        }
    }

    auto project_tree_model::toggle_expanded(project_tree_node* node) -> void
    {
        if (node == nullptr)
        {
            return;
        }

        set_expanded(node, !node->is_expanded);
    }

    auto project_tree_model::expand_all() -> void
    {
        for (auto& root : _roots)
        {
            set_all_expanded_impl(root.get(), true);
        }
        _rows_dirty = true;
        rebuild_visible_rows();
    }

    auto project_tree_model::collapse_all() -> void
    {
        for (auto& root : _roots)
        {
            set_all_expanded_impl(root.get(), false);
        }
        _rows_dirty = true;
        rebuild_visible_rows();
    }

    auto project_tree_model::set_search_query(string_view query) -> void
    {
        if (_search_query == query)
        {
            return;
        }

        _search_query = query;
        _rows_dirty = true;
        rebuild_visible_rows();
    }

    auto project_tree_model::search_query() const noexcept -> string_view
    {
        return _search_query;
    }

    auto project_tree_model::clear_search_query() -> void
    {
        if (_search_query.empty())
        {
            return;
        }

        _search_query.clear();
        _rows_dirty = true;
        rebuild_visible_rows();
    }

    auto project_tree_model::is_search_active() const noexcept -> bool
    {
        return !_search_query.empty();
    }
} // namespace tempest::editor
