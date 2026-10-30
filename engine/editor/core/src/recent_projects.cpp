#include <tempest/recent_projects.hpp>

#include <tempest/chrono.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/utility.hpp>

namespace tempest::editor
{
    auto load_recent_projects(string_view session_file_path) -> vector<recent_project_entry>
    {
        const auto session_path = filesystem::path(session_file_path);
        if (!filesystem::exists(session_path))
        {
            return {};
        }

        const auto file_content = read_file_to_string(session_path);
        if (!file_content.has_value() || file_content->empty())
        {
            return {};
        }

        auto allocator = system_allocator{};
        auto parse_result = json_document::from_string(*file_content, allocator);
        if (!parse_result.has_value())
        {
            return {};
        }

        const auto root_val = parse_result->root();
        if (!root_val.is_object())
        {
            return {};
        }

        const auto root_obj = root_val.as_object().value();
        const auto recents_val = root_obj["recent_projects"];
        if (!recents_val.is_array())
        {
            return {};
        }

        auto result = vector<recent_project_entry>{};
        const auto recents_array = recents_val.as_array().value();
        for (const auto item_val : recents_array)
        {
            if (!item_val.is_object())
            {
                continue;
            }

            const auto item_obj = item_val.as_object().value();
            const auto path_val = item_obj["path"].as_string();
            if (!path_val.has_value() || path_val->empty())
            {
                continue;
            }

            auto entry = recent_project_entry{};
            entry.path = string(*path_val);

            const auto name_val = item_obj["name"].as_string();
            if (name_val.has_value() && !name_val->empty())
            {
                entry.name = string(*name_val);
            }
            else
            {
                entry.name = filesystem::path(entry.path).stem().generic_string();
            }

            const auto last_opened_val = item_obj["last_opened"].as_uint64();
            if (last_opened_val.has_value())
            {
                entry.last_opened = *last_opened_val;
            }

            result.push_back(tempest::move(entry));
        }

        return result;
    }

    auto save_recent_projects(span<const recent_project_entry> projects, string_view session_file_path) -> bool
    {
        const auto session_path = filesystem::path(session_file_path);
        auto allocator = system_allocator{};
        auto existing_document = optional<json_document>{};

        if (filesystem::exists(session_path))
        {
            const auto existing_content = read_file_to_string(session_path);
            if (existing_content.has_value() && !existing_content->empty())
            {
                auto parsed_doc = json_document::from_string(*existing_content, allocator);
                if (parsed_doc.has_value() && parsed_doc->root().is_object())
                {
                    existing_document = tempest::move(*parsed_doc);
                }
            }
        }

        auto writer = json_writer{};
        auto root_obj = writer.create_object();

        if (existing_document.has_value())
        {
            const auto existing_root = existing_document->root().as_object().value();
            for (const auto member : existing_root)
            {
                if (member.key == "recent_projects")
                {
                    continue;
                }
                root_obj.set(member.key, member.value);
            }
        }

        auto recents_array = root_obj.create_child_array("recent_projects");
        for (const auto& entry : projects)
        {
            auto item_obj = recents_array.create_child_object();
            item_obj.set("name", entry.name);
            item_obj.set("path", entry.path);
            item_obj.set("last_opened", entry.last_opened);
        }

        writer.set_root(root_obj);
        const auto serialized_json = writer.to_string(json_format::pretty);

        const auto parent_dir = session_path.parent_path();
        if (!parent_dir.empty())
        {
            filesystem::create_directories(parent_dir);
        }

        const auto write_result = write_file_from_bytes(
            session_path,
            span<const byte>{reinterpret_cast<const byte*>(serialized_json.data()), serialized_json.size()});
        return write_result.has_value();
    }

    auto add_recent_project(string_view project_path, string_view project_name, string_view session_file_path) -> bool
    {
        auto entries = load_recent_projects(session_file_path);
        const auto normalized_path = filesystem::path(project_path).generic_string();

        auto final_name = string(project_name);
        if (final_name.empty())
        {
            final_name = filesystem::path(project_path).stem().generic_string();
        }

        auto current_iterator = entries.begin();
        while (current_iterator != entries.end())
        {
            if (filesystem::path(current_iterator->path).generic_string() == normalized_path)
            {
                current_iterator = entries.erase(current_iterator);
            }
            else
            {
                ++current_iterator;
            }
        }

        const auto now_timestamp =
            static_cast<uint64_t>(chrono::system_clock::to_time_t(chrono::system_clock::now()));

        auto new_entry = recent_project_entry{
            .name = tempest::move(final_name),
            .path = normalized_path,
            .last_opened = now_timestamp,
        };

        auto updated_entries = vector<recent_project_entry>{};
        updated_entries.reserve(tempest::min(entries.size() + 1, default_max_recent_projects));
        updated_entries.push_back(tempest::move(new_entry));

        for (auto& existing_entry : entries)
        {
            if (updated_entries.size() >= default_max_recent_projects)
            {
                break;
            }
            updated_entries.push_back(tempest::move(existing_entry));
        }

        return save_recent_projects(
            span<const recent_project_entry>{updated_entries.data(), updated_entries.size()}, session_file_path);
    }

    auto clear_recent_projects(string_view session_file_path) -> bool
    {
        return save_recent_projects(span<const recent_project_entry>{}, session_file_path);
    }

    auto remove_recent_project(string_view project_path, string_view session_file_path) -> bool
    {
        auto entries = load_recent_projects(session_file_path);
        const auto normalized_path = filesystem::path(project_path).generic_string();

        auto erased = false;
        auto current_iterator = entries.begin();
        while (current_iterator != entries.end())
        {
            if (filesystem::path(current_iterator->path).generic_string() == normalized_path)
            {
                current_iterator = entries.erase(current_iterator);
                erased = true;
            }
            else
            {
                ++current_iterator;
            }
        }

        if (!erased)
        {
            return false;
        }

        return save_recent_projects(
            span<const recent_project_entry>{entries.data(), entries.size()}, session_file_path);
    }
} // namespace tempest::editor
