#include <tempest/scene/reverse_dependency_index.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>

namespace tempest::scene
{
    auto reverse_dependency_index::add_reference(const guid& asset_id, const asset_reference_location& loc) -> void
    {
        auto& locations = _dependencies[asset_id];
        const auto found_location = tempest::find(locations.begin(), locations.end(), loc);
        if (found_location == locations.end())
        {
            locations.push_back(loc);
        }
    }

    auto reverse_dependency_index::add_reference(const guid& asset_id, asset_reference_location&& loc) -> void
    {
        auto& locations = _dependencies[asset_id];
        const auto found_location = tempest::find(locations.begin(), locations.end(), loc);
        if (found_location == locations.end())
        {
            locations.push_back(tempest::move(loc));
        }
    }

    auto reverse_dependency_index::remove_reference(const guid& asset_id, const asset_reference_location& loc) -> bool
    {
        const auto entry = _dependencies.find(asset_id);
        if (entry == _dependencies.end())
        {
            return false;
        }

        auto& locations = entry->second;
        const auto found_location = tempest::find(locations.begin(), locations.end(), loc);
        if (found_location == locations.end())
        {
            return false;
        }

        locations.erase(found_location);
        if (locations.empty())
        {
            _dependencies.erase(asset_id);
        }
        return true;
    }

    auto reverse_dependency_index::remove_references_for_file(string_view file_path) -> size_t
    {
        auto removed_count = size_t{0};
        auto keys_to_prune = vector<guid>{};

        for (auto& entry : _dependencies)
        {
            auto& locations = entry.second;
            removed_count += tempest::erase_if(locations, [&](const asset_reference_location& loc) {
                return loc.owner_path == file_path;
            });
            if (locations.empty())
            {
                keys_to_prune.push_back(entry.first);
            }
        }

        for (const auto& key : keys_to_prune)
        {
            _dependencies.erase(key);
        }

        return removed_count;
    }

    auto reverse_dependency_index::clear() noexcept -> void
    {
        _dependencies.clear();
    }

    auto reverse_dependency_index::empty() const noexcept -> bool
    {
        return _dependencies.empty();
    }

    auto reverse_dependency_index::size() const noexcept -> size_t
    {
        return _dependencies.size();
    }

    auto reverse_dependency_index::find_references(const guid& asset_id) const -> span<const asset_reference_location>
    {
        const auto entry = _dependencies.find(asset_id);
        if (entry == _dependencies.end())
        {
            return {};
        }
        return span<const asset_reference_location>{entry->second.data(), entry->second.size()};
    }

    auto reverse_dependency_index::has_references(const guid& asset_id) const noexcept -> bool
    {
        const auto entry = _dependencies.find(asset_id);
        return entry != _dependencies.end() && !entry->second.empty();
    }

    auto reverse_dependency_index::all_dependencies() const noexcept
        -> const flat_unordered_map<guid, vector<asset_reference_location>>&
    {
        return _dependencies;
    }

    auto reverse_dependency_codec::serialize(const reverse_dependency_index& index,
                                             const reverse_dependency_serialize_options& options) -> string
    {
        auto writer = json_writer{};
        auto root_object = writer.create_object();

        root_object.set("format", default_format_identifier);
        root_object.set("version", current_schema_version);

        auto dependencies_object = root_object.create_child_object("dependencies");
        for (const auto& [asset_id, locations] : index.all_dependencies())
        {
            const auto guid_string = tempest::to_string(asset_id);
            auto locations_array = dependencies_object.create_child_array(guid_string);
            for (const auto& location : locations)
            {
                auto location_object = locations_array.create_child_object();
                location_object.set("owner_guid", tempest::to_string(location.owner_guid));
                location_object.set("owner_path", location.owner_path);
                location_object.set("target_path", to_string(location.target_path));
                location_object.set("component", location.component_name);
                location_object.set("field", location.field_name);
            }
        }

        writer.set_root(root_object);
        return writer.to_string(options.format);
    }

    auto reverse_dependency_codec::deserialize(string_view json_str)
        -> expected<reverse_dependency_index, reverse_dependency_codec_error>
    {
        if (json_str.empty())
        {
            return unexpected(reverse_dependency_codec_error::invalid_json);
        }

        auto alloc = system_allocator{};
        auto document_result = json_document::from_string(json_str, alloc);
        if (!document_result.has_value())
        {
            return unexpected(reverse_dependency_codec_error::invalid_json);
        }

        const auto root_val = document_result->root();
        if (!root_val.is_object())
        {
            return unexpected(reverse_dependency_codec_error::invalid_root_object);
        }

        if (const auto format_val = root_val["format"].as_string(); format_val.has_value())
        {
            if (*format_val != default_format_identifier)
            {
                return unexpected(reverse_dependency_codec_error::unsupported_format);
            }
        }

        if (const auto version_val = root_val["version"].as_int64(); version_val.has_value())
        {
            if (*version_val > current_schema_version)
            {
                return unexpected(reverse_dependency_codec_error::version_mismatch);
            }
        }

        const auto dependencies_val = root_val["dependencies"];
        if (!dependencies_val.is_valid())
        {
            return unexpected(reverse_dependency_codec_error::missing_dependencies);
        }

        auto parsed_index = flat_unordered_map<guid, vector<asset_reference_location>>{};

        auto parse_locations_array = [](json_value array_val, vector<asset_reference_location>& out_locations) -> void {
            const auto array_opt = array_val.as_array();
            if (!array_opt.has_value())
            {
                return;
            }
            for (const auto location_val : *array_opt)
            {
                if (!location_val.is_object())
                {
                    continue;
                }

                auto location = asset_reference_location{};
                if (const auto owner_guid_val = location_val["owner_guid"].as_string(); owner_guid_val.has_value())
                {
                    if (const auto parsed_owner = parse_guid(*owner_guid_val); parsed_owner.has_value())
                    {
                        location.owner_guid = *parsed_owner;
                    }
                }

                if (const auto owner_path_val = location_val["owner_path"].as_string(); owner_path_val.has_value())
                {
                    location.owner_path = string{*owner_path_val};
                }

                if (const auto target_path_val = location_val["target_path"]; target_path_val.is_valid())
                {
                    if (const auto parsed_target = parse_id_path(target_path_val); parsed_target.has_value())
                    {
                        location.target_path = *parsed_target;
                    }
                }

                if (const auto comp_val = location_val["component"].as_string(); comp_val.has_value())
                {
                    location.component_name = string{*comp_val};
                }
                else if (const auto comp_alt = location_val["component_name"].as_string(); comp_alt.has_value())
                {
                    location.component_name = string{*comp_alt};
                }

                if (const auto field_val = location_val["field"].as_string(); field_val.has_value())
                {
                    location.field_name = string{*field_val};
                }
                else if (const auto field_alt = location_val["field_name"].as_string(); field_alt.has_value())
                {
                    location.field_name = string{*field_alt};
                }

                out_locations.push_back(tempest::move(location));
            }
        };

        if (dependencies_val.is_object())
        {
            const auto dependencies_object = *dependencies_val.as_object();
            for (const auto member : dependencies_object)
            {
                const auto parsed_asset_id = parse_guid(member.key);
                if (!parsed_asset_id.has_value())
                {
                    continue;
                }
                auto& locations = parsed_index[*parsed_asset_id];
                parse_locations_array(member.value, locations);
            }
        }
        else if (dependencies_val.is_array())
        {
            const auto dependencies_array = *dependencies_val.as_array();
            for (const auto entry_val : dependencies_array)
            {
                if (!entry_val.is_object())
                {
                    continue;
                }

                auto asset_guid_opt = optional<guid>{};
                if (const auto guid_str = entry_val["asset_guid"].as_string(); guid_str.has_value())
                {
                    asset_guid_opt = parse_guid(*guid_str);
                }
                else if (const auto guid_alt = entry_val["asset_id"].as_string(); guid_alt.has_value())
                {
                    asset_guid_opt = parse_guid(*guid_alt);
                }

                if (!asset_guid_opt.has_value())
                {
                    continue;
                }

                auto& locations = parsed_index[*asset_guid_opt];
                const auto references_val = entry_val["references"];
                if (references_val.is_valid())
                {
                    parse_locations_array(references_val, locations);
                }
                else
                {
                    parse_locations_array(entry_val["locations"], locations);
                }
            }
        }
        else
        {
            return unexpected(reverse_dependency_codec_error::missing_dependencies);
        }

        auto result = reverse_dependency_index{};
        result._dependencies = tempest::move(parsed_index);
        return result;
    }

    auto reverse_dependency_codec::save_to_file(const reverse_dependency_index& index,
                                                const filesystem::path& disk_path,
                                                const reverse_dependency_serialize_options& options)
        -> expected<void, reverse_dependency_codec_error>
    {
        if (disk_path.has_parent_path())
        {
            filesystem::create_directories(disk_path.parent_path());
        }

        const auto serialized_payload = serialize(index, options);
        const auto byte_data =
            span<const byte>{reinterpret_cast<const byte*>(serialized_payload.data()), serialized_payload.size()};
        const auto write_result = write_file_from_bytes(disk_path, byte_data);
        if (!write_result.has_value())
        {
            return unexpected(reverse_dependency_codec_error::write_failed);
        }
        return {};
    }

    auto reverse_dependency_codec::load_from_file(const filesystem::path& disk_path)
        -> expected<reverse_dependency_index, reverse_dependency_codec_error>
    {
        const auto read_result = read_file_to_string(disk_path);
        if (!read_result.has_value())
        {
            if (read_result.error() == file_error::not_found)
            {
                return unexpected(reverse_dependency_codec_error::file_not_found);
            }
            return unexpected(reverse_dependency_codec_error::read_failed);
        }
        return deserialize(*read_result);
    }
} // namespace tempest::scene
