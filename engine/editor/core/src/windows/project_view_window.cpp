#include <tempest/windows/project_view_window.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/checked.hpp>
#include <tempest/command_history.hpp>
#include <tempest/commands/entity_commands.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/entity_hierarchy.hpp>
#include <tempest/file_watcher.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/format.hpp>
#include <tempest/material.hpp>
#include <tempest/optional.hpp>
#include <tempest/payloads/asset_payload.hpp>
#include <tempest/project_workspace.hpp>
#include <tempest/render_target_asset.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/reverse_dependency_index.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/scene/scene_reference_stripper.hpp>
#include <tempest/span.hpp>
#include <tempest/texture.hpp>
#include <tempest/utility.hpp>
#include <tempest/vertex.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace tempest::editor
{
    namespace
    {
#if defined(_WIN32)
        auto to_wide_string(string_view text) -> vector<wchar_t>
        {
            if (text.empty())
            {
                return vector<wchar_t>{L'\0'};
            }
            const auto length =
                MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
            if (length <= 0)
            {
                return vector<wchar_t>{L'\0'};
            }
            auto wide_str = vector<wchar_t>(static_cast<size_t>(length + 1), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide_str.data(), length);
            wide_str[length] = L'\0';
            return wide_str;
        }

        auto open_file_in_shell(string_view file_path) -> void
        {
            if (file_path.empty())
            {
                return;
            }
            auto wide_path = to_wide_string(file_path);

            auto* shell32 = LoadLibraryA("shell32.dll");
            if (shell32 != nullptr)
            {
                using PFN_ShellExecuteW = HINSTANCE(WINAPI*)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
                auto shell_execute_fn = reinterpret_cast<PFN_ShellExecuteW>(GetProcAddress(shell32, "ShellExecuteW"));
                if (shell_execute_fn != nullptr)
                {
                    shell_execute_fn(nullptr, L"open", wide_path.data(), nullptr, nullptr, SW_SHOWNORMAL);
                }
                FreeLibrary(shell32);
            }
        }

        auto open_in_file_manager(string_view disk_path) -> void
        {
            if (disk_path.empty())
            {
                return;
            }

            auto fs_path = filesystem::path{disk_path};
            auto native_path = fs_path.make_preferred().string();

            auto* shell32 = LoadLibraryA("shell32.dll");
            if (shell32 == nullptr)
            {
                return;
            }

            using PFN_ShellExecuteW = HINSTANCE(WINAPI*)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
            auto shell_execute_fn = reinterpret_cast<PFN_ShellExecuteW>(GetProcAddress(shell32, "ShellExecuteW"));
            if (shell_execute_fn != nullptr)
            {
                if (filesystem::is_directory(fs_path))
                {
                    auto wide_dir = to_wide_string(native_path);
                    shell_execute_fn(nullptr, L"open", wide_dir.data(), nullptr, nullptr, SW_SHOWNORMAL);
                }
                else
                {
                    const auto select_arg = tempest::format("/select,\"{}\"", native_path);
                    auto wide_arg = to_wide_string(select_arg);
                    shell_execute_fn(nullptr, L"open", L"explorer.exe", wide_arg.data(), nullptr, SW_SHOWNORMAL);
                }
            }
            FreeLibrary(shell32);
        }
#else
        auto open_file_in_shell(string_view) -> void
        {
        }

        auto open_in_file_manager(string_view) -> void
        {
        }
#endif

        auto is_importable_extension(string_view ext, const assets::asset_database* asset_db) noexcept -> bool
        {
            if (ext.empty())
            {
                return false;
            }
            if (asset_db != nullptr)
            {
                return asset_db->has_importer(ext);
            }
            return assets::asset_database::is_default_supported_extension(ext);
        }

        template <typename Callback>
        auto scan_directory_recursive(const filesystem::path& root_dir, Callback&& callback) -> void
        {
            if (!filesystem::exists(root_dir) || !filesystem::is_directory(root_dir))
            {
                return;
            }
            for (const auto& entry : filesystem::directory_iterator{root_dir})
            {
                if (entry.is_directory())
                {
                    scan_directory_recursive(entry.path(), callback);
                }
                else
                {
                    callback(entry.path());
                }
            }
        }

        auto find_last_scope(string_view text) noexcept -> string_view
        {
            if (text.size() < 2)
            {
                return text;
            }
            for (auto index = text.size() - 2; index > 0; --index)
            {
                if (text[index] == ':' && text[index + 1] == ':')
                {
                    return string_view{text.data() + index + 2, text.size() - (index + 2)};
                }
            }
            if (text[0] == ':' && text[1] == ':')
            {
                return string_view{text.data() + 2, text.size() - 2};
            }
            return text;
        }

        auto find_category(string_view text) noexcept -> string_view
        {
            for (auto index = size_t{0}; index < text.size(); ++index)
            {
                if (text[index] == '/')
                {
                    return string_view{text.data(), index};
                }
            }
            return text;
        }

        auto find_extension(string_view text) noexcept -> string_view
        {
            if (text.empty())
            {
                return {};
            }
            for (auto index = text.size(); index > 0; --index)
            {
                if (text[index - 1] == '.')
                {
                    if (index < text.size())
                    {
                        return string_view{text.data() + index, text.size() - index};
                    }
                    return {};
                }
            }
            return {};
        }

        auto format_badge_from_type_name(string_view type_name) -> string
        {
            const auto short_name = find_last_scope(type_name);
            if (short_name.empty())
            {
                return "[Asset]";
            }

            auto result = string{"["};
            result.push_back(static_cast<char>(short_name[0] >= 'a' && short_name[0] <= 'z'
                                                   ? short_name[0] - ('a' - 'A')
                                                   : short_name[0]));
            if (short_name.size() > 1)
            {
                result.append(short_name.data() + 1, short_name.size() - 1);
            }
            result.push_back(']');
            return result;
        }

        auto contains_illegal_character(string_view name) noexcept -> bool
        {
            for (const auto ch : name)
            {
                switch (ch)
                {
                    case '/':
                    case '\\':
                    case ':':
                    case '*':
                    case '?':
                    case '"':
                    case '<':
                    case '>':
                    case '|':
                        return true;
                    default:
                        break;
                }
            }
            return false;
        }

        auto is_case_insensitive_equal(string_view lhs, string_view rhs) noexcept -> bool
        {
#if defined(_WIN32)
            if (lhs.size() != rhs.size())
            {
                return false;
            }
            for (auto index = size_t{0}; index < lhs.size(); ++index)
            {
                const auto char_lhs = (lhs[index] >= 'A' && lhs[index] <= 'Z')
                                          ? static_cast<char>(lhs[index] + ('a' - 'A'))
                                          : lhs[index];
                const auto char_rhs = (rhs[index] >= 'A' && rhs[index] <= 'Z')
                                          ? static_cast<char>(rhs[index] + ('a' - 'A'))
                                          : rhs[index];
                if (char_lhs != char_rhs)
                {
                    return false;
                }
            }
            return true;
#else
            return lhs == rhs;
#endif
        }

        auto generate_unique_item_name(const filesystem::path& target_disk_dir,
                                       string_view base_name,
                                       string_view extension = {}) -> string
        {
            auto candidate = string{base_name};
            if (!extension.empty())
            {
                candidate.append(extension.data(), extension.size());
            }

            auto item_exists = [&](const string& name) -> bool {
                if (filesystem::exists(target_disk_dir / filesystem::path{name}))
                {
                    return true;
                }
                auto meta_name = name;
                meta_name.append(".tmeta");
                if (filesystem::exists(target_disk_dir / filesystem::path{meta_name}))
                {
                    return true;
                }
                return false;
            };

            if (!item_exists(candidate))
            {
                return candidate;
            }

            for (auto counter = size_t{2}; counter < project_view_window::max_name_collision_attempts; ++counter)
            {
                candidate = tempest::format("{} {}{}", base_name, counter, extension);
                if (!item_exists(candidate))
                {
                    return candidate;
                }
            }

            return candidate;
        }
    } // namespace

    project_view_window::project_view_window(scene::scene_manager& in_scene_mgr,
                                             editor_selection& in_selection,
                                             assets::asset_database* asset_db,
                                             command_history* history,
                                             ecs::basic_archetype_registry* registry,
                                             scene::reverse_dependency_index* reverse_deps)
        : _model{asset_db}, _scene_mgr{in_scene_mgr}, _selection{in_selection},
          _asset_db{asset_db}, _history{history}, _registry{registry}, _reverse_deps{reverse_deps}
    {
    }

    project_view_window::project_view_window(project_tree_model model,
                                             scene::scene_manager& in_scene_mgr,
                                             editor_selection& in_selection,
                                             assets::asset_database* asset_db,
                                             command_history* history,
                                             ecs::basic_archetype_registry* registry,
                                             scene::reverse_dependency_index* reverse_deps)
        : _model{tempest::move(model)}, _scene_mgr{in_scene_mgr}, _selection{in_selection},
          _asset_db{asset_db != nullptr ? asset_db : _model.asset_database()},
          _history{history}, _registry{registry}, _reverse_deps{reverse_deps}
    {
    }

    auto project_view_window::set_search_query(string_view query) -> void
    {
        const auto copy_length = tempest::min(query.size(), search_buffer_capacity - 1);
        tempest::memcpy(_search_buffer.data(), query.data(), copy_length);
        _search_buffer[copy_length] = '\0';
        _model.set_search_query(_search_buffer.data());
    }

    auto project_view_window::clear_search_query() -> void
    {
        _search_buffer[0] = '\0';
        _model.clear_search_query();
    }

    auto project_view_window::set_current_directory(string_view dir_path) -> void
    {
        _current_directory = string{dir_path};
    }

    auto project_view_window::navigate_to_directory(string_view dir_path) -> void
    {
        _current_directory = string{dir_path};

        auto* target_node = _model.find_node_by_path(dir_path);
        if (target_node != nullptr)
        {
            auto* current = target_node;
            auto any_expanded = false;
            while (current != nullptr)
            {
                if (!current->is_expanded)
                {
                    current->is_expanded = true;
                    any_expanded = true;
                }
                current = current->parent;
            }

            if (any_expanded)
            {
                _model.rebuild_visible_rows();
            }
        }
    }

    auto project_view_window::get_node_badge(const project_tree_row& row,
                                             const assets::asset_database* asset_db) -> string
    {
        if (row.kind == tree_node_kind::mount_root || row.kind == tree_node_kind::directory)
        {
            return "[Folder]";
        }

        if (row.kind == tree_node_kind::loose_file)
        {
            const auto ext = find_extension(row.display_label);
            if (!ext.empty() && is_importable_extension(ext, asset_db))
            {
                return "[Loose]";
            }
            return "[File]";
        }

        // Procedurally resolve registered type if present
        if (row.type.hash() != 0)
        {
            if (asset_db != nullptr && asset_db->type_registry() != nullptr)
            {
                const auto registered_name = asset_db->type_registry()->name_of(row.type);
                if (registered_name.has_value())
                {
                    return format_badge_from_type_name(*registered_name);
                }
            }

            // Fast-path / fallback for core engine types
            if (row.type == assets::asset_type_id::of<core::mesh>())
            {
                return "[Mesh]";
            }
            if (row.type == assets::asset_type_id::of<core::material>())
            {
                return "[Material]";
            }
            if (row.type == assets::asset_type_id::of<core::texture>())
            {
                return "[Texture]";
            }
            if (row.type == assets::asset_type_id::of<assets::shader_asset>())
            {
                return "[Shader]";
            }
            if (row.type == assets::asset_type_id::of<assets::render_target_asset>())
            {
                return "[RenderTarget]";
            }
            if (row.type == assets::asset_type_id::of<assets::entity_hierarchy>())
            {
                return "[Hierarchy]";
            }
        }

        if (row.kind == tree_node_kind::sub_asset)
        {
            const auto key = (row.node != nullptr && !row.node->sub_asset_key.empty())
                                 ? string_view{row.node->sub_asset_key}
                                 : row.display_label;

            const auto category = find_category(key);

            if (category == "mesh")
            {
                return "[Mesh]";
            }
            if (category == "material")
            {
                return "[Material]";
            }
            if (category == "texture")
            {
                return "[Texture]";
            }
            if (category == "prefab")
            {
                return "[Prefab]";
            }
            if (category == "shader")
            {
                return "[Shader]";
            }

            return "[SubAsset]";
        }

        if (row.kind == tree_node_kind::imported_file)
        {
            const auto name = (row.node != nullptr && !row.node->name.empty())
                                  ? string_view{row.node->name}
                                  : row.display_label;

            const auto ext_view = find_extension(name);
            if (!ext_view.empty())
            {
                auto result = string{"["};
                for (auto char_index = size_t{0}; char_index < ext_view.size(); ++char_index)
                {
                    const auto ch = ext_view[char_index];
                    result.push_back(static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch));
                }
                result.push_back(']');
                return result;
            }

            return "[Asset]";
        }

        return "[Asset]";
    }

    auto project_view_window::split_breadcrumb_path(string_view path) -> vector<breadcrumb_item>
    {
        auto results = vector<breadcrumb_item>{};
        if (path.empty())
        {
            return results;
        }

        results.reserve(4);

        auto accumulated_path = string{};
        auto cursor = size_t{0};

        while (cursor < path.size())
        {
            while (cursor < path.size() && (path[cursor] == '/' || path[cursor] == '\\'))
            {
                ++cursor;
            }

            if (cursor >= path.size())
            {
                break;
            }

            const auto start_index = cursor;
            while (cursor < path.size() && path[cursor] != '/' && path[cursor] != '\\')
            {
                ++cursor;
            }

            const auto segment_len = cursor - start_index;
            const auto segment = string_view{path.data() + start_index, segment_len};

            if (!accumulated_path.empty())
            {
                accumulated_path.push_back('/');
            }
            accumulated_path.append(segment.data(), segment.size());

            auto item = breadcrumb_item{
                .name = string{segment},
                .path = accumulated_path,
            };
            results.push_back(tempest::move(item));
        }

        return results;
    }

    auto project_view_window::set_selected_path(string_view path) -> void
    {
        if (path.empty())
        {
            clear_selection();
            return;
        }

        _selected_path = string{path};
        const auto* node = _model.find_node_by_path(path);
        if (node != nullptr)
        {
            _selected_asset_id = node->asset_id;
            _selected_kind = node->kind;
            return;
        }

        for (const auto& root : _model.roots())
        {
            if (root && (root->name == path || root->path == path))
            {
                _selected_asset_id = root->asset_id;
                _selected_kind = root->kind;
                return;
            }
        }

        for (const auto& row : _model.visible_rows())
        {
            if (row.relative_path == path || (row.relative_path.empty() && row.display_label == path))
            {
                _selected_asset_id = row.asset_id;
                _selected_kind = row.kind;
                return;
            }
        }

        _selected_asset_id = {};
        _selected_kind = default_selected_kind;
    }

    auto project_view_window::clear_selection() noexcept -> void
    {
        _selected_path.clear();
        _selected_asset_id = {};
        _selected_kind = default_selected_kind;
    }

    auto project_view_window::activate_item(const project_tree_row& row) -> void
    {
        const auto item_path = row.relative_path.empty() ? row.display_label : row.relative_path;
        set_selected_path(item_path);

        if (row.kind == tree_node_kind::mount_root || row.kind == tree_node_kind::directory)
        {
            _model.set_expanded_by_path(item_path, !row.is_expanded);
            _current_directory = string{item_path};
            return;
        }

        auto disk_path = string_view{};
        if (row.node != nullptr && !row.node->disk_path.empty())
        {
            disk_path = row.node->disk_path;
        }
        else
        {
            const auto* found_node = _model.find_node_by_path(row.relative_path);
            if (found_node != nullptr && !found_node->disk_path.empty())
            {
                disk_path = found_node->disk_path;
            }
            else
            {
                disk_path = item_path;
            }
        }

        const auto is_scene = tempest::ends_with(row.relative_path, ".tscene") ||
                              tempest::ends_with(row.display_label, ".tscene") ||
                              tempest::ends_with(disk_path, ".tscene");
        if (is_scene)
        {
            open_scene_requested(disk_path);
            return;
        }

        const auto is_prefab = tempest::ends_with(row.relative_path, ".tprefab") ||
                               tempest::ends_with(row.display_label, ".tprefab") ||
                               tempest::ends_with(disk_path, ".tprefab");

        const auto is_material = tempest::ends_with(row.relative_path, ".tmaterial") ||
                                 tempest::ends_with(row.display_label, ".tmaterial") ||
                                 tempest::ends_with(disk_path, ".tmaterial") ||
                                 row.type == assets::asset_type_id::of<core::material>();

        auto is_sub_prefab_or_material = false;
        if (row.kind == tree_node_kind::sub_asset)
        {
            const auto key = (row.node != nullptr && !row.node->sub_asset_key.empty())
                                 ? string_view{row.node->sub_asset_key}
                                 : row.display_label;
            const auto cat = find_category(key);
            if (cat == "prefab" || cat == "material")
            {
                is_sub_prefab_or_material = true;
            }
        }

        if (is_prefab || is_material || is_sub_prefab_or_material)
        {
            if (_on_inspect_asset)
            {
                _on_inspect_asset(row.asset_id, row.type, row.relative_path);
            }
            return;
        }

        if (_on_open_external_file)
        {
            _on_open_external_file(disk_path);
        }
        else
        {
            open_file_in_shell(disk_path);
        }
    }

    auto project_view_window::open_scene_requested(string_view disk_path) -> void
    {
        const auto loaded_scenes = _scene_mgr->get_loaded_scenes();
        auto found_dirty = false;
        auto dirty_handle = scene::scene_handle{};
        for (const auto& handle : loaded_scenes)
        {
            if (_scene_mgr->is_dirty(handle))
            {
                found_dirty = true;
                dirty_handle = handle;
                break;
            }
        }

        if (found_dirty)
        {
            _show_dirty_dialog = true;
            _pending_scene_path = string{disk_path};
            _dirty_scene_to_save = dirty_handle;
            const auto maybe_name = _scene_mgr->get_scene_name(dirty_handle);
            if (maybe_name.has_value() && !maybe_name->empty())
            {
                _dirty_scene_name = string{*maybe_name};
            }
            else
            {
                const auto maybe_path = _scene_mgr->get_scene_path(dirty_handle);
                if (maybe_path.has_value() && !maybe_path->empty())
                {
                    _dirty_scene_name = string{*maybe_path};
                }
                else
                {
                    _dirty_scene_name = "Untitled";
                }
            }
        }
        else
        {
            [[maybe_unused]] const auto load_res = _scene_mgr->load_sync(
                filesystem::path{disk_path}, scene::scene_load_options{.mode = scene::scene_load_mode::single});
        }
    }

    auto project_view_window::confirm_save_and_open() -> void
    {
        auto path_to_save = filesystem::path{};
        const auto maybe_path = _scene_mgr->get_scene_path(_dirty_scene_to_save);
        if (maybe_path.has_value() && !maybe_path->empty())
        {
            path_to_save = filesystem::path{*maybe_path};
        }
        else if (!_dirty_scene_name.empty())
        {
            path_to_save = filesystem::path{_dirty_scene_name};
        }
        else
        {
            path_to_save = filesystem::path{"untitled.tscene"};
        }

        if (!path_to_save.empty())
        {
            const auto save_res = _scene_mgr->save_sync(_dirty_scene_to_save, path_to_save);
            if (!save_res.has_value())
            {
                return;
            }
        }

        if (!_pending_scene_path.empty())
        {
            [[maybe_unused]] const auto load_res = _scene_mgr->load_sync(
                filesystem::path{_pending_scene_path}, scene::scene_load_options{.mode = scene::scene_load_mode::single});
        }

        _show_dirty_dialog = false;
        _pending_scene_path.clear();
        _dirty_scene_to_save = {};
        _dirty_scene_name.clear();
    }

    auto project_view_window::confirm_discard_and_open() -> void
    {
        if (!_pending_scene_path.empty())
        {
            [[maybe_unused]] const auto load_res = _scene_mgr->load_sync(
                filesystem::path{_pending_scene_path}, scene::scene_load_options{.mode = scene::scene_load_mode::single});
        }

        _show_dirty_dialog = false;
        _pending_scene_path.clear();
        _dirty_scene_to_save = {};
        _dirty_scene_name.clear();
    }

    auto project_view_window::cancel_open_scene() -> void
    {
        _show_dirty_dialog = false;
        _pending_scene_path.clear();
        _dirty_scene_to_save = {};
        _dirty_scene_name.clear();
    }

    auto project_view_window::_find_target_node(string_view path) const -> const project_tree_node*
    {
        if (path.empty())
        {
            return nullptr;
        }

        const auto* node = _model.find_node_by_path(path);
        if (node != nullptr)
        {
            return node;
        }

        for (const auto& root : _model.roots())
        {
            if (root && (root->name == path || root->path == path))
            {
                return root.get();
            }
        }

        for (const auto& row : _model.visible_rows())
        {
            if (row.relative_path == path || (row.relative_path.empty() && row.display_label == path))
            {
                return row.node;
            }
        }

        for (auto index = size_t{0}; index < path.size(); ++index)
        {
            if (path[index] == '/')
            {
                auto alt_path = string{path.data(), index};
                alt_path.push_back(':');
                if (index + 1 < path.size())
                {
                    alt_path.append(path.data() + index + 1, path.size() - index - 1);
                }
                const auto* alt_node = _model.find_node_by_path(alt_path);
                if (alt_node != nullptr)
                {
                    return alt_node;
                }
            }
        }

        return nullptr;
    }

    auto project_view_window::start_rename(string_view path) -> bool
    {
        if (path.empty())
        {
            return false;
        }

        if (_is_renaming)
        {
            cancel_rename();
        }

        const auto* node = _find_target_node(path);
        if (node == nullptr)
        {
            return false;
        }

        if (node->kind == tree_node_kind::mount_root || node->kind == tree_node_kind::sub_asset)
        {
            return false;
        }

        _is_renaming = true;
        _renaming_path = string{path};

        auto name_str = string{};
        if (!node->name.empty())
        {
            name_str = node->name;
        }
        else
        {
            name_str = filesystem::path{node->path}.filename().string();
        }
        const auto copy_length = tempest::min(name_str.size(), rename_buffer_capacity - 1);
        tempest::memcpy(_rename_buffer.data(), name_str.data(), copy_length);
        _rename_buffer[copy_length] = '\0';

        _rename_first_frame = true;
        _rename_error.clear();
        _model.set_invalidation_deferred(true);

        return true;
    }

    auto project_view_window::commit_rename() -> bool
    {
        if (!_is_renaming)
        {
            return false;
        }

        const auto name_view = string_view{static_cast<const char*>(_rename_buffer.data())};
        const auto error_opt = validate_rename_name(name_view);
        if (error_opt.has_value())
        {
            _rename_error = string{*error_opt};
            return false;
        }

        const auto* node = _find_target_node(_renaming_path);
        if (node == nullptr)
        {
            cancel_rename();
            return false;
        }

        if (node->name == name_view)
        {
            cancel_rename();
            return true;
        }

        auto old_disk_path = filesystem::path{node->disk_path};
        if (old_disk_path.empty())
        {
            old_disk_path = filesystem::path{_renaming_path};
        }
        const auto parent_dir = old_disk_path.parent_path();
        const auto new_disk_path = parent_dir / filesystem::path{name_view};

        if (filesystem::exists(old_disk_path))
        {
            if (!filesystem::rename(old_disk_path, new_disk_path))
            {
                _rename_error = "Failed to rename file on disk.";
                return false;
            }
        }

        auto old_meta_str = old_disk_path.generic_string();
        old_meta_str.append(".tmeta");
        const auto old_meta_path = filesystem::path{old_meta_str};

        auto new_meta_str = new_disk_path.generic_string();
        new_meta_str.append(".tmeta");
        const auto new_meta_path = filesystem::path{new_meta_str};

        if (filesystem::exists(old_meta_path))
        {
            filesystem::rename(old_meta_path, new_meta_path);
        }

        _model.handle_file_watch_event(file_watch_event{
            .type = file_watch_event_type::renamed,
            .path = new_disk_path,
            .old_path = old_disk_path,
        });

        auto new_relative_path = string{};
        const auto rel_parent = filesystem::path{_renaming_path}.parent_path().generic_string();
        if (!rel_parent.empty() && rel_parent != ".")
        {
            new_relative_path = rel_parent;
            new_relative_path.push_back('/');
            new_relative_path.append(name_view.data(), name_view.size());
        }
        else
        {
            new_relative_path = string{name_view};
        }

        const auto was_selected = (_selected_path == _renaming_path);
        if (was_selected)
        {
            _selected_path = new_relative_path;
        }

        if (_current_directory == _renaming_path)
        {
            _current_directory = new_relative_path;
        }

        _model.set_invalidation_deferred(false);

        if (was_selected)
        {
            set_selected_path(new_relative_path);
        }

        _is_renaming = default_is_renaming;
        _renaming_path.clear();
        _rename_buffer[0] = '\0';
        _rename_error.clear();
        _rename_first_frame = default_rename_first_frame;

        return true;
    }

    auto project_view_window::cancel_rename() noexcept -> void
    {
        _model.set_invalidation_deferred(false);
        _is_renaming = default_is_renaming;
        _renaming_path.clear();
        _rename_buffer[0] = '\0';
        _rename_error.clear();
        _rename_first_frame = default_rename_first_frame;
    }

    auto project_view_window::set_rename_buffer(string_view text) -> void
    {
        const auto copy_length = tempest::min(text.size(), rename_buffer_capacity - 1);
        tempest::memcpy(_rename_buffer.data(), text.data(), copy_length);
        _rename_buffer[copy_length] = '\0';

        if (ImGui::GetCurrentContext() != nullptr)
        {
            auto& imgui_ctx = *ImGui::GetCurrentContext();
            if (imgui_ctx.InputTextState.ID != 0)
            {
                imgui_ctx.InputTextState.ReloadUserBufAndSelectAll();
            }
        }
    }

    auto project_view_window::validate_rename_name(string_view new_name) const -> optional<string_view>
    {
        if (new_name.empty())
        {
            return string_view{"Name cannot be empty."};
        }

        auto is_all_whitespace = true;
        for (const auto ch : new_name)
        {
            if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
            {
                is_all_whitespace = false;
                break;
            }
        }
        if (is_all_whitespace)
        {
            return string_view{"Name cannot be empty."};
        }

        if (contains_illegal_character(new_name))
        {
            return string_view{"Name contains illegal characters."};
        }

        if (_renaming_path.empty())
        {
            return nullopt;
        }

        const auto* node = _find_target_node(_renaming_path);
        if (node == nullptr)
        {
            return nullopt;
        }

        if (is_case_insensitive_equal(node->name, new_name))
        {
            return nullopt;
        }

        if (node->parent != nullptr)
        {
            for (const auto& child : node->parent->children)
            {
                if (child && child.get() != node)
                {
                    if (is_case_insensitive_equal(child->name, new_name))
                    {
                        return string_view{"An item with this name already exists."};
                    }
                }
            }
        }
        else
        {
            for (const auto& root : _model.roots())
            {
                if (root && root.get() != node)
                {
                    if (is_case_insensitive_equal(root->name, new_name))
                    {
                        return string_view{"An item with this name already exists."};
                    }
                }
            }
        }

        if (!node->disk_path.empty())
        {
            const auto current_path = filesystem::path{node->disk_path};
            const auto parent_dir = current_path.parent_path();
            if (!parent_dir.empty())
            {
                const auto target_disk_path = parent_dir / filesystem::path{new_name};
                if (filesystem::exists(target_disk_path))
                {
                    return string_view{"An item with this name already exists."};
                }
            }
        }

        return nullopt;
    }

    auto project_view_window::_resolve_target_folder_node(string_view parent_path) const -> const project_tree_node*
    {
        auto climb_to_folder = [](const project_tree_node* start_node) -> const project_tree_node* {
            auto* node = start_node;
            while (node != nullptr && node->kind != tree_node_kind::directory &&
                   node->kind != tree_node_kind::mount_root)
            {
                node = node->parent;
            }
            return node;
        };

        if (!parent_path.empty())
        {
            const auto* node = _find_target_node(parent_path);
            if (node == nullptr)
            {
                return nullptr;
            }
            return climb_to_folder(node);
        }

        if (has_selection())
        {
            const auto* node = _find_target_node(_selected_path);
            const auto* folder_node = climb_to_folder(node);
            if (folder_node != nullptr)
            {
                return folder_node;
            }
        }

        if (!_current_directory.empty())
        {
            const auto* node = _find_target_node(_current_directory);
            const auto* folder_node = climb_to_folder(node);
            if (folder_node != nullptr)
            {
                return folder_node;
            }
        }

        for (const auto& root : _model.roots())
        {
            if (root)
            {
                return root.get();
            }
        }

        return nullptr;
    }

    auto project_view_window::_finish_creation(const project_tree_node& target_folder_node,
                                               const filesystem::path& new_disk_path,
                                               string_view unique_name) -> bool
    {
        _model.set_expanded_by_path(target_folder_node.path, true);
        _model.invalidate_path(new_disk_path);

        auto new_relative_path = string{};
        if (target_folder_node.kind == tree_node_kind::mount_root || target_folder_node.path.empty())
        {
            new_relative_path = string{unique_name};
        }
        else
        {
            new_relative_path = target_folder_node.path;
            new_relative_path.push_back('/');
            new_relative_path.append(unique_name.data(), unique_name.size());
        }

        set_selected_path(new_relative_path);
        start_rename(new_relative_path);
        return true;
    }

    auto project_view_window::create_folder(string_view parent_path) -> bool
    {
        if (_is_renaming)
        {
            cancel_rename();
        }

        const auto* target_folder_node = _resolve_target_folder_node(parent_path);
        if (target_folder_node == nullptr)
        {
            return false;
        }

        auto target_disk_dir = filesystem::path{target_folder_node->disk_path};
        if (target_disk_dir.empty())
        {
            target_disk_dir = filesystem::path{target_folder_node->path};
        }
        if (target_disk_dir.empty() || !filesystem::exists(target_disk_dir))
        {
            return false;
        }
        if (!filesystem::is_directory(target_disk_dir))
        {
            target_disk_dir = target_disk_dir.parent_path();
        }

        const auto unique_name = generate_unique_item_name(target_disk_dir, default_new_folder_name);
        const auto new_disk_path = target_disk_dir / filesystem::path{unique_name};

        if (!filesystem::create_directory(new_disk_path))
        {
            return false;
        }

        return _finish_creation(*target_folder_node, new_disk_path, unique_name);
    }

    auto project_view_window::create_material(string_view parent_path) -> bool
    {
        if (_is_renaming)
        {
            cancel_rename();
        }

        const auto* target_folder_node = _resolve_target_folder_node(parent_path);
        if (target_folder_node == nullptr)
        {
            return false;
        }

        auto target_disk_dir = filesystem::path{target_folder_node->disk_path};
        if (target_disk_dir.empty())
        {
            target_disk_dir = filesystem::path{target_folder_node->path};
        }
        if (target_disk_dir.empty() || !filesystem::exists(target_disk_dir))
        {
            return false;
        }
        if (!filesystem::is_directory(target_disk_dir))
        {
            target_disk_dir = target_disk_dir.parent_path();
        }

        const auto unique_name = generate_unique_item_name(target_disk_dir, default_new_material_base_name,
                                                           default_new_material_extension);
        const auto new_disk_path = target_disk_dir / filesystem::path{unique_name};

        const auto mat_span = span<const byte>{
            reinterpret_cast<const byte*>(default_material_content.data()),
            default_material_content.size(),
        };
        const auto write_res = write_file_from_bytes(new_disk_path, mat_span);
        if (!write_res.has_value())
        {
            return false;
        }

        auto meta = assets::sidecar_meta{};
        meta.source_guid = guid::generate_random_guid();
        meta.importer = default_material_importer;
        auto meta_path_str = new_disk_path.generic_string();
        meta_path_str.append(".tmeta");
        if (!meta.save_to_file(meta_path_str))
        {
            filesystem::remove(new_disk_path);
            return false;
        }

        return _finish_creation(*target_folder_node, new_disk_path, unique_name);
    }

    auto project_view_window::create_scene(string_view parent_path) -> bool
    {
        if (_is_renaming)
        {
            cancel_rename();
        }

        const auto* target_folder_node = _resolve_target_folder_node(parent_path);
        if (target_folder_node == nullptr)
        {
            return false;
        }

        auto target_disk_dir = filesystem::path{target_folder_node->disk_path};
        if (target_disk_dir.empty())
        {
            target_disk_dir = filesystem::path{target_folder_node->path};
        }
        if (target_disk_dir.empty() || !filesystem::exists(target_disk_dir))
        {
            return false;
        }
        if (!filesystem::is_directory(target_disk_dir))
        {
            target_disk_dir = target_disk_dir.parent_path();
        }

        const auto unique_name =
            generate_unique_item_name(target_disk_dir, default_new_scene_base_name, default_new_scene_extension);
        const auto new_disk_path = target_disk_dir / filesystem::path{unique_name};

        const auto scene_span = span<const byte>{
            reinterpret_cast<const byte*>(default_scene_content.data()),
            default_scene_content.size(),
        };
        const auto write_res = write_file_from_bytes(new_disk_path, scene_span);
        if (!write_res.has_value())
        {
            return false;
        }

        return _finish_creation(*target_folder_node, new_disk_path, unique_name);
    }

    auto project_view_window::_resolve_disk_path(string_view path) const -> filesystem::path
    {
        auto target_path = path;
        if (target_path.empty())
        {
            if (has_selection())
            {
                target_path = _selected_path;
            }
            else if (!_current_directory.empty())
            {
                target_path = _current_directory;
            }
            else if (!_model.roots().empty() && _model.roots().front())
            {
                target_path = _model.roots().front()->path;
            }
        }

        if (target_path.empty())
        {
            return {};
        }

        const auto* node = _find_target_node(target_path);
        if (node != nullptr && !node->disk_path.empty())
        {
            return filesystem::path{node->disk_path};
        }

        const auto direct_fs_path = filesystem::path{target_path};
        if (filesystem::exists(direct_fs_path))
        {
            return direct_fs_path;
        }

        for (const auto& root : _model.roots())
        {
            if (root && !root->disk_path.empty())
            {
                const auto candidate = filesystem::path{root->disk_path} / direct_fs_path;
                if (filesystem::exists(candidate))
                {
                    return candidate;
                }
            }
        }

        return {};
    }

    auto project_view_window::_resolve_asset_database() const noexcept -> const assets::asset_database*
    {
        if (_asset_db != nullptr)
        {
            return _asset_db;
        }
        if (_model.asset_database() != nullptr)
        {
            return _model.asset_database();
        }
        if (_scene_mgr != nullptr)
        {
            return _scene_mgr->get_asset_database();
        }
        return nullptr;
    }

    auto project_view_window::_resolve_mutable_asset_database() noexcept -> assets::asset_database*
    {
        if (_asset_db != nullptr)
        {
            return _asset_db;
        }
        if (_model.asset_database() != nullptr)
        {
            return _model.asset_database();
        }
        if (_scene_mgr != nullptr)
        {
            return _scene_mgr->get_asset_database();
        }
        return nullptr;
    }

    auto project_view_window::reverse_dependency_index() const noexcept -> scene::reverse_dependency_index*
    {
        if (_reverse_deps != nullptr)
        {
            return _reverse_deps;
        }
        if (_scene_mgr != nullptr)
        {
            return _scene_mgr->get_reverse_dependency_index();
        }
        return nullptr;
    }

    auto project_view_window::deduce_importer(string_view path_or_extension) const -> optional<string_view>
    {
        const auto ext = find_extension(path_or_extension);
        const auto target_ext = ext.empty() ? path_or_extension : ext;
        const auto* db = _resolve_asset_database();
        if (db != nullptr)
        {
            return db->deduce_importer_name(target_ext);
        }
        return assets::asset_database::default_importer_name_for_extension(target_ext);
    }

    auto project_view_window::is_importable(string_view path_or_extension) const -> bool
    {
        return deduce_importer(path_or_extension).has_value();
    }

    auto project_view_window::import_asset(string_view path) -> bool
    {
        const auto disk_path = _resolve_disk_path(path);
        if (disk_path.empty() || !filesystem::exists(disk_path))
        {
            return false;
        }

        if (filesystem::is_directory(disk_path))
        {
            auto imported_any = false;
            scan_directory_recursive(disk_path, [&](const filesystem::path& file_path) {
                const auto file_str = file_path.generic_string();
                if (tempest::ends_with(file_str, ".tmeta"))
                {
                    return;
                }

                auto meta_str = file_str;
                meta_str.append(".tmeta");
                if (filesystem::exists(filesystem::path{meta_str}))
                {
                    return;
                }

                const auto importer_opt = deduce_importer(file_str);
                if (!importer_opt.has_value())
                {
                    return;
                }

                auto meta = assets::sidecar_meta{};
                meta.source_guid = guid::generate_random_guid();
                meta.importer = string{*importer_opt};
                if (!meta.save_to_file(meta_str))
                {
                    return;
                }

                if (_model.asset_database() != nullptr)
                {
                    _model.asset_database()->notify_file_changed(file_str);
                }
                imported_any = true;
            });

            if (imported_any)
            {
                _model.invalidate_path(disk_path);
                return true;
            }
            return false;
        }

        const auto file_str = disk_path.generic_string();
        if (tempest::ends_with(file_str, ".tmeta"))
        {
            return false;
        }

        auto meta_str = file_str;
        meta_str.append(".tmeta");
        if (filesystem::exists(filesystem::path{meta_str}))
        {
            return false;
        }

        const auto importer_opt = deduce_importer(file_str);
        if (!importer_opt.has_value())
        {
            return false;
        }

        auto meta = assets::sidecar_meta{};
        meta.source_guid = guid::generate_random_guid();
        meta.importer = string{*importer_opt};
        if (!meta.save_to_file(meta_str))
        {
            return false;
        }

        if (_model.asset_database() != nullptr)
        {
            _model.asset_database()->notify_file_changed(file_str);
        }

        _model.invalidate_path(disk_path);
        return true;
    }

    auto project_view_window::reimport_asset(string_view path) -> bool
    {
        const auto disk_path = _resolve_disk_path(path);
        if (disk_path.empty() || !filesystem::exists(disk_path))
        {
            return false;
        }

        if (filesystem::is_directory(disk_path))
        {
            auto reimported_any = false;
            scan_directory_recursive(disk_path, [&](const filesystem::path& file_path) {
                const auto file_str = file_path.generic_string();
                if (tempest::ends_with(file_str, ".tmeta"))
                {
                    return;
                }

                auto meta_str = file_str;
                meta_str.append(".tmeta");
                if (!filesystem::exists(filesystem::path{meta_str}))
                {
                    return;
                }

                auto loaded_meta = assets::sidecar_meta::load_from_file(meta_str);
                if (!loaded_meta.has_value())
                {
                    return;
                }

                if (loaded_meta->importer.empty())
                {
                    const auto importer_opt = deduce_importer(file_str);
                    if (importer_opt.has_value())
                    {
                        loaded_meta->importer = string{*importer_opt};
                    }
                }
                if (!loaded_meta->save_to_file(meta_str))
                {
                    return;
                }

                if (_model.asset_database() != nullptr)
                {
                    _model.asset_database()->notify_file_changed(file_str);
                }
                reimported_any = true;
            });

            if (reimported_any)
            {
                _model.invalidate_path(disk_path);
                return true;
            }
            return false;
        }

        const auto file_str = disk_path.generic_string();
        if (tempest::ends_with(file_str, ".tmeta"))
        {
            return false;
        }

        auto meta_str = file_str;
        meta_str.append(".tmeta");
        if (!filesystem::exists(filesystem::path{meta_str}))
        {
            return false;
        }

        auto loaded_meta = assets::sidecar_meta::load_from_file(meta_str);
        if (!loaded_meta.has_value())
        {
            return false;
        }

        if (loaded_meta->importer.empty())
        {
            const auto importer_opt = deduce_importer(file_str);
            if (importer_opt.has_value())
            {
                loaded_meta->importer = string{*importer_opt};
            }
        }
        if (!loaded_meta->save_to_file(meta_str))
        {
            return false;
        }

        if (_model.asset_database() != nullptr)
        {
            _model.asset_database()->notify_file_changed(file_str);
        }

        _model.invalidate_path(disk_path);
        return true;
    }

    auto project_view_window::open_scene_additive(string_view disk_path) -> bool
    {
        const auto resolved_path = _resolve_disk_path(disk_path);
        if (resolved_path.empty() || !filesystem::exists(resolved_path))
        {
            return false;
        }

        const auto load_result = _scene_mgr->load_sync(
            resolved_path, scene::scene_load_options{.mode = scene::scene_load_mode::additive});

        if (!load_result.has_value())
        {
            return false;
        }

        if (_on_open_scene_additive)
        {
            _on_open_scene_additive(resolved_path.generic_string());
        }

        return true;
    }

    auto project_view_window::reveal_in_explorer(string_view path) -> bool
    {
        const auto disk_path = _resolve_disk_path(path);
        if (disk_path.empty() || !filesystem::exists(disk_path))
        {
            return false;
        }

        const auto disk_str = disk_path.generic_string();
        if (_on_reveal_in_explorer)
        {
            _on_reveal_in_explorer(disk_str);
            return true;
        }

        open_in_file_manager(disk_str);
        return true;
    }

    auto project_view_window::request_remove_asset(string_view target_path) -> bool
    {
        if (target_path.empty())
        {
            return false;
        }

        const auto* const node = _find_target_node(target_path);
        if (node == nullptr || node->kind == tree_node_kind::mount_root)
        {
            return false;
        }

        _removal_target_path = node->path.empty() ? string{target_path} : node->path;
        _removal_item_name = node->name;
        _removal_disk_path = node->disk_path;
        if (_removal_disk_path.empty())
        {
            _removal_disk_path = _resolve_disk_path(target_path).generic_string();
        }

        _removal_asset_guid = node->asset_id;
        if (_removal_asset_guid == guid{})
        {
            auto meta_path = _removal_disk_path;
            meta_path.append(".tmeta");
            if (filesystem::exists(filesystem::path{meta_path}))
            {
                const auto loaded_meta = assets::sidecar_meta::load_from_file(meta_path);
                if (loaded_meta.has_value() && loaded_meta->source_guid != guid{})
                {
                    _removal_asset_guid = loaded_meta->source_guid;
                }
            }

            if (_removal_asset_guid == guid{})
            {
                const auto* const asset_db = _resolve_asset_database();
                if (asset_db != nullptr)
                {
                    const auto* const entry = asset_db->find_by_path(_removal_target_path);
                    if (entry != nullptr && entry->id != guid{})
                    {
                        _removal_asset_guid = entry->id;
                    }
                }
            }
        }

        _removal_references.clear();
        const auto* const rev_deps = reverse_dependency_index();
        if (_removal_asset_guid != guid{} && rev_deps != nullptr)
        {
            const auto refs = rev_deps->find_references(_removal_asset_guid);
            _removal_references.reserve(refs.size());
            for (const auto& loc : refs)
            {
                _removal_references.push_back(loc);
            }
        }

        _show_removal_dialog = true;
        _delete_source_file = false;
        return true;
    }

    auto project_view_window::confirm_clean_removal() -> bool
    {
        const auto disk_path = filesystem::path{_removal_disk_path};
        if (_delete_source_file && !disk_path.empty() && filesystem::exists(disk_path))
        {
            if (filesystem::is_directory(disk_path))
            {
                static_cast<void>(filesystem::remove_all(disk_path));
            }
            else
            {
                static_cast<void>(filesystem::remove(disk_path));
            }
        }

        if (!disk_path.empty())
        {
            auto meta_path = disk_path;
            meta_path += ".tmeta";
            if (filesystem::exists(meta_path))
            {
                static_cast<void>(filesystem::remove(meta_path));
            }
        }

        auto* const asset_db = _resolve_mutable_asset_database();
        if (asset_db != nullptr)
        {
            if (_removal_asset_guid != guid{})
            {
                static_cast<void>(asset_db->unregister_source(_removal_asset_guid));
            }
            if (!disk_path.empty())
            {
                static_cast<void>(asset_db->notify_file_removed(disk_path.generic_string()));
            }
        }

        if (!disk_path.empty())
        {
            _model.invalidate_path(disk_path.parent_path());
            _model.invalidate_path(disk_path);
        }

        if (_selected_path == _removal_target_path || tempest::starts_with(_selected_path, _removal_target_path))
        {
            clear_selection();
        }

        _show_removal_dialog = false;
        _removal_references.clear();
        return true;
    }

    auto project_view_window::confirm_removal_abort() -> void
    {
        _show_removal_dialog = false;
        _removal_references.clear();
    }

    auto project_view_window::confirm_removal_leave_dangling() -> bool
    {
        return confirm_clean_removal();
    }

    auto project_view_window::confirm_removal_remove_referencing_entities() -> bool
    {
        auto* const rev_deps = reverse_dependency_index();
        auto fallback_types = ecs::component_type_registry{};
        const auto& comp_types = _registry != nullptr ? _registry->type_registry() : fallback_types;

        if (_removal_asset_guid != guid{})
        {
            if (_history != nullptr && _registry != nullptr)
            {
                auto matching_entities = vector<ecs::entity>{};
                for (const auto [self] : _registry->with<ecs::self_component>())
                {
                    const auto entity = self.entity;
                    if (!_registry->is_valid(entity))
                    {
                        continue;
                    }

                    auto is_match = false;
                    const auto* const prefab_comp = _registry->try_get<scene::prefab_instance_component>(entity);
                    if (prefab_comp != nullptr && prefab_comp->prefab_guid == _removal_asset_guid)
                    {
                        is_match = true;
                    }

                    if (!is_match)
                    {
                        const auto entity_comp_types = ecs::raw_registry_access::get_component_types(*_registry, entity);
                        for (const auto& info : entity_comp_types)
                        {
                            if (!info.descriptor.has_value())
                            {
                                continue;
                            }

                            const auto* const comp_ptr =
                                ecs::raw_registry_access::get_component_raw(*_registry, entity, info.index);
                            if (comp_ptr == nullptr)
                            {
                                continue;
                            }

                            for (const auto& field : info.descriptor->fields)
                            {
                                if (field.kind == ecs::field_kind::asset_ref)
                                {
                                    auto field_guid = guid{};
                                    if (field.get != nullptr)
                                    {
                                        field.get(comp_ptr, &field_guid);
                                    }
                                    else if (field.offset != ecs::field_descriptor::no_offset)
                                    {
                                        tempest::memcpy(&field_guid, static_cast<const byte*>(comp_ptr) + field.offset,
                                                        sizeof(guid));
                                    }

                                    if (field_guid == _removal_asset_guid)
                                    {
                                        is_match = true;
                                        break;
                                    }
                                }
                            }

                            if (is_match)
                            {
                                break;
                            }
                        }
                    }

                    if (is_match)
                    {
                        matching_entities.push_back(entity);
                    }
                }

                auto cmd_ctx = command_context{
                    .registry = *_registry,
                    .scene_manager = *_scene_mgr,
                };

                for (const auto entity : matching_entities)
                {
                    if (_registry->is_valid(entity))
                    {
                        const auto addr = compute_entity_address(*_registry, entity);
                        auto cmd = make_unique<destroy_entity_command>(addr, destroy_entity_command::mode::recursive);
                        _history->execute(tempest::move(cmd), cmd_ctx);
                    }
                }

                if (rev_deps != nullptr)
                {
                    static_cast<void>(scene::strip_asset_references(*rev_deps, _removal_asset_guid, comp_types, nullptr));
                }
            }
            else
            {
                if (rev_deps != nullptr)
                {
                    static_cast<void>(scene::strip_asset_references(*rev_deps, _removal_asset_guid, comp_types, _registry));
                }
                else if (_registry != nullptr)
                {
                    static_cast<void>(scene::strip_references_from_registry(*_registry, _removal_asset_guid));
                }
            }
        }

        return confirm_clean_removal();
    }

    auto project_view_window::draw() -> void
    {
        if (!_open)
        {
            return;
        }

        if (_model.is_dirty())
        {
            _model.rebuild_visible_rows();
        }

        if (ImGui::Begin(window_name().data(), &_open))
        {
            // Search text input and synchronization
            const auto text_changed = ImGui::InputTextWithHint("##ProjectSearch", "Search assets...", _search_buffer.data(),
                                                               search_buffer_capacity);
            if (text_changed || string_view{static_cast<const char*>(_search_buffer.data())} != _model.search_query())
            {
                _model.set_search_query(_search_buffer.data());
            }

            ImGui::SameLine();
            if (ImGui::Button("Clear"))
            {
                clear_search_query();
            }

            if (_model.is_search_active())
            {
                ImGui::SameLine();
                const auto match_count = _model.visible_row_count();
                ImGui::TextDisabled(match_count == 1 ? "1 match" : "%zu matches", match_count);
            }

            // Breadcrumb navigation strip
            auto effective_dir = string_view{_current_directory};
            if (effective_dir.empty() && !_model.roots().empty() && _model.roots()[0] != nullptr)
            {
                effective_dir = _model.roots()[0]->name;
            }

            const auto breadcrumbs = split_breadcrumb_path(effective_dir);
            if (!breadcrumbs.empty())
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0F, 2.0F));
                for (auto segment_index = size_t{0}; segment_index < breadcrumbs.size(); ++segment_index)
                {
                    if (segment_index > 0)
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled(">");
                        ImGui::SameLine();
                    }

                    const auto& item = breadcrumbs[segment_index];
                    ImGui::PushID(static_cast<int>(segment_index));
                    if (ImGui::SmallButton(item.name.c_str()))
                    {
                        navigate_to_directory(item.path);
                    }
                    ImGui::PopID();
                }
                ImGui::PopStyleVar();
                ImGui::Separator();
            }

            // Virtualized tree rows region
            if (ImGui::BeginChild("##ProjectTreeScrollRegion"))
            {
                if (has_selection() && !_is_renaming && ImGui::IsKeyPressed(ImGuiKey_F2))
                {
                    start_rename(_selected_path);
                }

                const auto rows = _model.visible_rows();
                auto clipper = ImGuiListClipper{};
                clipper.Begin(static_cast<int>(rows.size()));

                auto pending_toggle_path = string{};
                auto pending_toggle_open = false;
                auto has_pending_toggle = false;
                auto rename_committed = false;
                auto action_triggered = false;

                while (clipper.Step())
                {
                    for (auto row_index = clipper.DisplayStart; row_index < clipper.DisplayEnd; ++row_index)
                    {
                        if (row_index < 0 || static_cast<size_t>(row_index) >= rows.size())
                        {
                            continue;
                        }

                        const auto& row = rows[static_cast<size_t>(row_index)];
                        ImGui::PushID(row_index);

                        const auto indent_px = static_cast<float>(row.depth) * 16.0F;
                        if (indent_px > 0.0F)
                        {
                            ImGui::Indent(indent_px);
                        }

                        const auto badge = get_node_badge(row, _model.asset_database());
                        const auto row_path = row.relative_path.empty() ? row.display_label : row.relative_path;

                        if (_is_renaming && row_path == _renaming_path)
                        {
                            ImGui::TextUnformatted(badge.c_str());
                            ImGui::SameLine();

                            if (_rename_first_frame)
                            {
                                ImGui::SetKeyboardFocusHere();
                                _rename_first_frame = false;
                            }

                            const auto error_opt =
                                validate_rename_name(string_view{static_cast<const char*>(_rename_buffer.data())});
                            const auto has_error = error_opt.has_value();
                            if (has_error)
                            {
                                _rename_error = string{*error_opt};
                            }
                            else
                            {
                                _rename_error.clear();
                            }

                            if (has_error)
                            {
                                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0F, 0.2F, 0.2F, 1.0F));
                            }
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0F);

                            ImGui::SetNextItemWidth(-1.0F);
                            const auto enter_pressed =
                                ImGui::InputText("##InlineRename", _rename_buffer.data(), rename_buffer_capacity,
                                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

                            ImGui::PopStyleVar();
                            if (has_error)
                            {
                                ImGui::PopStyleColor();
                            }

                            if (has_error && ImGui::IsItemHovered())
                            {
                                ImGui::BeginTooltip();
                                ImGui::TextUnformatted(_rename_error.c_str());
                                ImGui::EndTooltip();
                            }

                            if (enter_pressed)
                            {
                                if (!has_error)
                                {
                                    commit_rename();
                                    rename_committed = true;
                                }
                            }
                            else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                            {
                                cancel_rename();
                            }
                        }
                        else
                        {
                            auto node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                                              ImGuiTreeNodeFlags_NoTreePushOnOpen;
                            if (!row.has_children)
                            {
                                node_flags |= ImGuiTreeNodeFlags_Leaf;
                            }

                            const auto is_selected = (_selected_path == row_path);
                            if (is_selected)
                            {
                                node_flags |= ImGuiTreeNodeFlags_Selected;
                            }

                            ImGui::SetNextItemOpen(row.is_expanded);

                            const auto is_loose = (row.kind == tree_node_kind::loose_file);
                            if (is_loose)
                            {
                                const auto ext = find_extension(row.display_label);
                                const auto importable = !ext.empty() && is_importable_extension(ext, _resolve_asset_database());
                                if (importable)
                                {
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65F, 0.65F, 0.65F, 1.0F));
                                }
                                else
                                {
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42F, 0.42F, 0.42F, 1.0F));
                                }
                            }

                            auto node_open = false;
                            if (_model.is_search_active() && !row.relative_path.empty())
                            {
                                node_open = ImGui::TreeNodeEx("##row", node_flags, "%.*s %.*s (%.*s)",
                                                              static_cast<int>(badge.size()), badge.data(),
                                                              static_cast<int>(row.display_label.size()),
                                                              row.display_label.data(),
                                                              static_cast<int>(row.relative_path.size()),
                                                              row.relative_path.data());
                            }
                            else
                            {
                                node_open = ImGui::TreeNodeEx("##row", node_flags, "%.*s %.*s",
                                                              static_cast<int>(badge.size()), badge.data(),
                                                              static_cast<int>(row.display_label.size()),
                                                              row.display_label.data());
                            }

                            if (is_loose)
                            {
                                ImGui::PopStyleColor();
                            }

                            if (row.has_children && ImGui::IsItemToggledOpen())
                            {
                                has_pending_toggle = true;
                                pending_toggle_path = string{row_path};
                                pending_toggle_open = node_open;
                            }

                            if (ImGui::IsItemClicked())
                            {
                                set_selected_path(row_path);
                                if (row.kind == tree_node_kind::directory || row.kind == tree_node_kind::mount_root)
                                {
                                    _current_directory = string{row_path};
                                }
                            }

                            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            {
                                activate_item(row);
                            }

                            if (row.kind != tree_node_kind::directory && row.kind != tree_node_kind::mount_root)
                            {
                                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                                {
                                    auto payload = asset_payload{};
                                    payload.asset_guid = row.asset_id;
                                    payload.type_hash = row.type.hash();
                                    if (row.node != nullptr)
                                    {
                                        payload.set_path(row.node->disk_path.empty() ? row.relative_path
                                                                                     : string_view{row.node->disk_path});
                                        if (!row.node->sub_asset_key.empty())
                                        {
                                            payload.set_sub_asset_key(row.node->sub_asset_key);
                                        }
                                    }
                                    else
                                    {
                                        payload.set_path(row.relative_path);
                                    }

                                    const auto* asset_db = _resolve_asset_database();
                                    if (payload.asset_guid == guid{} && asset_db != nullptr)
                                    {
                                        const auto* entry = asset_db->find_by_path(payload.path_view());
                                        if (entry != nullptr)
                                        {
                                            payload.asset_guid = entry->id;
                                            if (payload.type_hash == 0)
                                            {
                                                payload.type_hash = entry->type.hash();
                                            }
                                        }
                                    }

                                    if (asset_db != nullptr && asset_db->type_registry() != nullptr && row.type.hash() != 0)
                                    {
                                        const auto name_opt = asset_db->type_registry()->name_of(row.type);
                                        if (name_opt.has_value())
                                        {
                                            payload.set_type_name(*name_opt);
                                        }
                                    }

                                    if (payload.type_name_view().empty())
                                    {
                                        if (row.type == assets::asset_type_id::of<core::material>() ||
                                            tempest::ends_with(row.relative_path, ".tmaterial") ||
                                            (row.node != nullptr && tempest::ends_with(row.node->disk_path, ".tmaterial")) ||
                                            (row.node != nullptr && tempest::starts_with(row.node->sub_asset_key, "material")))
                                        {
                                            payload.set_type_name("material");
                                        }
                                        else if (row.type == assets::asset_type_id::of<assets::entity_hierarchy>() ||
                                                 tempest::ends_with(row.relative_path, ".tprefab") ||
                                                 (row.node != nullptr && tempest::ends_with(row.node->disk_path, ".tprefab")) ||
                                                 tempest::ends_with(row.relative_path, ".gltf") ||
                                                 tempest::ends_with(row.relative_path, ".glb") ||
                                                 (row.node != nullptr && tempest::starts_with(row.node->sub_asset_key, "prefab")))
                                        {
                                            payload.set_type_name("prefab");
                                        }
                                    }

                                    ImGui::SetDragDropPayload(asset_payload_type, &payload, sizeof(payload));
                                    const auto drag_label = (row.node != nullptr && !row.node->name.empty())
                                                                ? string_view{row.node->name}
                                                                : row.display_label;
                                    ImGui::Text("%.*s", static_cast<int>(drag_label.size()), drag_label.data());
                                    ImGui::EndDragDropSource();
                                }
                            }

                            if (ImGui::BeginPopupContextItem())
                            {
                                if (_selected_path != row_path)
                                {
                                    set_selected_path(row_path);
                                    if (row.kind == tree_node_kind::directory || row.kind == tree_node_kind::mount_root)
                                    {
                                        _current_directory = string{row_path};
                                    }
                                }

                                const auto is_scene_file = tempest::ends_with(row.relative_path, ".tscene") ||
                                                           tempest::ends_with(row.display_label, ".tscene") ||
                                                           (row.node != nullptr && tempest::ends_with(row.node->disk_path, ".tscene"));

                                if (is_scene_file)
                                {
                                    auto row_disk_path = string_view{row_path};
                                    if (row.node != nullptr && !row.node->disk_path.empty())
                                    {
                                        row_disk_path = row.node->disk_path;
                                    }
                                    else
                                    {
                                        const auto* found_node = _model.find_node_by_path(row_path);
                                        if (found_node != nullptr && !found_node->disk_path.empty())
                                        {
                                            row_disk_path = found_node->disk_path;
                                        }
                                    }

                                    if (ImGui::MenuItem("Open Scene (Single)"))
                                    {
                                        open_scene_requested(row_disk_path);
                                        action_triggered = true;
                                    }
                                    if (ImGui::MenuItem("Open Scene (Additive)"))
                                    {
                                        open_scene_additive(row_disk_path);
                                        action_triggered = true;
                                    }
                                    ImGui::Separator();
                                }

                                if (ImGui::BeginMenu("Create"))
                                {
                                    if (ImGui::MenuItem("Folder"))
                                    {
                                        create_folder(row_path);
                                        action_triggered = true;
                                    }
                                    if (ImGui::MenuItem("Material"))
                                    {
                                        create_material(row_path);
                                        action_triggered = true;
                                    }
                                    if (ImGui::MenuItem("Scene"))
                                    {
                                        create_scene(row_path);
                                        action_triggered = true;
                                    }
                                    ImGui::EndMenu();
                                }

                                if (row.kind == tree_node_kind::loose_file)
                                {
                                    const auto importable = is_importable(row_path);
                                    if (ImGui::MenuItem("Import", nullptr, false, importable))
                                    {
                                        import_asset(row_path);
                                        action_triggered = true;
                                    }
                                    if (!importable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                    {
                                        ImGui::SetTooltip("No asset importer is registered for this file type.");
                                    }
                                }
                                else if (row.kind == tree_node_kind::imported_file)
                                {
                                    if (ImGui::MenuItem("Reimport"))
                                    {
                                        reimport_asset(row_path);
                                        action_triggered = true;
                                    }
                                }
                                else if (row.kind == tree_node_kind::directory || row.kind == tree_node_kind::mount_root)
                                {
                                    if (ImGui::MenuItem("Import All Loose Files"))
                                    {
                                        import_asset(row_path);
                                        action_triggered = true;
                                    }
                                    if (ImGui::MenuItem("Reimport All"))
                                    {
                                        reimport_asset(row_path);
                                        action_triggered = true;
                                    }
                                }

                                if (row.kind != tree_node_kind::mount_root && row.kind != tree_node_kind::sub_asset)
                                {
                                    if (ImGui::MenuItem("Rename", "F2"))
                                    {
                                        start_rename(row_path);
                                    }
                                }

                                if (row.kind != tree_node_kind::mount_root)
                                {
                                    if (ImGui::MenuItem("Remove from Project"))
                                    {
                                        request_remove_asset(row_path);
                                        action_triggered = true;
                                    }
                                }

                                ImGui::Separator();

                                if (ImGui::MenuItem("Reveal in Explorer"))
                                {
                                    reveal_in_explorer(row_path);
                                    action_triggered = true;
                                }

                                ImGui::EndPopup();
                            }
                        }

                        if (indent_px > 0.0F)
                        {
                            ImGui::Unindent(indent_px);
                        }

                        ImGui::PopID();

                        if (rename_committed || action_triggered)
                        {
                            break;
                        }
                    }

                    if (rename_committed || action_triggered)
                    {
                        break;
                    }
                }
                clipper.End();

                if (has_pending_toggle)
                {
                    _model.set_expanded_by_path(pending_toggle_path, pending_toggle_open);
                }

                if (ImGui::BeginPopupContextWindow("##ProjectViewBackgroundContext",
                                                   ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
                {
                    if (ImGui::BeginMenu("Create"))
                    {
                        if (ImGui::MenuItem("Folder"))
                        {
                            create_folder();
                        }
                        if (ImGui::MenuItem("Material"))
                        {
                            create_material();
                        }
                        if (ImGui::MenuItem("Scene"))
                        {
                            create_scene();
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::MenuItem("Import All Loose Files"))
                    {
                        import_asset();
                    }
                    if (ImGui::MenuItem("Reimport All"))
                    {
                        reimport_asset();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Reveal in Explorer"))
                    {
                        reveal_in_explorer();
                    }
                    ImGui::EndPopup();
                }
            }
            ImGui::EndChild();

            if (_show_dirty_dialog)
            {
                ImGui::OpenPopup("Save Changes?");
            }

            if (ImGui::BeginPopupModal("Save Changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Save changes to '%s' before opening a new scene?",
                            _dirty_scene_name.empty() ? "Untitled" : _dirty_scene_name.c_str());
                ImGui::Separator();

                if (ImGui::Button("Save"))
                {
                    confirm_save_and_open();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Don't Save"))
                {
                    confirm_discard_and_open();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    cancel_open_scene();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if (_show_removal_dialog)
            {
                if (has_removal_references())
                {
                    ImGui::OpenPopup("Referenced Asset Removal");
                }
                else
                {
                    ImGui::OpenPopup("Remove from Project?");
                }
            }

            if (ImGui::BeginPopupModal("Remove from Project?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Remove '%s' from the project?",
                            _removal_item_name.empty() ? _removal_target_path.c_str() : _removal_item_name.c_str());
                ImGui::Separator();

                ImGui::Checkbox("Delete source file from disk", &_delete_source_file);
                ImGui::Separator();

                if (ImGui::Button("Remove"))
                {
                    confirm_clean_removal();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    confirm_removal_abort();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if (ImGui::BeginPopupModal("Referenced Asset Removal", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Asset '%s' is referenced by %zu location(s):",
                            _removal_item_name.empty() ? _removal_target_path.c_str() : _removal_item_name.c_str(),
                            _removal_references.size());
                ImGui::Separator();

                if (ImGui::BeginChild("ReferencesList", ImVec2(400, 150), true))
                {
                    for (const auto& loc : _removal_references)
                    {
                        ImGui::Text("%s: %s.%s", loc.owner_path.c_str(), loc.component_name.c_str(),
                                    loc.field_name.c_str());
                    }
                }
                ImGui::EndChild();

                ImGui::Separator();
                ImGui::Checkbox("Delete source file from disk", &_delete_source_file);
                ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
                                   "Note: Disk rewrites for closed scenes cannot be undone.");
                ImGui::Separator();

                if (ImGui::Button("Abort"))
                {
                    confirm_removal_abort();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Leave Dangling"))
                {
                    confirm_removal_leave_dangling();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove Referencing Entities"))
                {
                    confirm_removal_remove_referencing_entities();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
} // namespace tempest::editor
