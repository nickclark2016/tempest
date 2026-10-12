#include <tempest/project_workspace.hpp>

#include <tempest/asset_database.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/utility.hpp>

namespace tempest::assets
{
    //==============================================================================
    // sidecar_meta Implementation
    //==============================================================================

    auto sidecar_meta::resolve_or_create_sub_asset(string_view sub_asset_key,
                                                   optional<asset_type_id> type) -> guid
    {
        const auto sub_it = sub_assets.find(sub_asset_key);
        if (sub_it != sub_assets.end())
        {
            if (type.has_value())
            {
                sub_asset_types[string(sub_asset_key)] = *type;
            }
            return sub_it->second;
        }

        const auto new_guid = generate_deterministic_guid(source_guid, sub_asset_key);
        sub_assets.insert({string(sub_asset_key), new_guid});
        if (type.has_value())
        {
            sub_asset_types.insert({string(sub_asset_key), *type});
        }
        return new_guid;
    }

    auto sidecar_meta::generate_deterministic_guid(const guid& source_guid,
                                                   string_view sub_asset_key) noexcept -> guid
    {
        auto buffer = vector<byte>{};
        buffer.reserve(source_guid.data.size() + sub_asset_key.size());
        for (const auto byte_val : source_guid.data)
        {
            buffer.push_back(byte_val);
        }
        for (const auto character : sub_asset_key)
        {
            buffer.push_back(static_cast<byte>(character));
        }

        const auto hash_digest = content_hash::compute(span<const byte>{buffer.data(), buffer.size()});

        auto result = guid{};
        tempest::memcpy(result.data.data(), hash_digest.data.data(), result.data.size());

        // RFC 4122 Version 5 (name-based SHA)
        result.data[6] = static_cast<byte>((static_cast<uint8_t>(result.data[6]) & 0x0FU) | 0x50U);
        // RFC 4122 Variant 1 (10xxxxxx)
        result.data[8] = static_cast<byte>((static_cast<uint8_t>(result.data[8]) & 0x3FU) | 0x80U);

        return result;
    }

    auto sidecar_meta::save_to_file(string_view meta_path) const -> bool
    {
        const auto json_str = to_string();
        const auto write_res = write_file_from_bytes(
            filesystem::path(meta_path),
            span<const byte>{reinterpret_cast<const byte*>(json_str.data()), json_str.size()});
        return write_res.has_value();
    }

    auto sidecar_meta::load_from_file(string_view meta_path) -> optional<sidecar_meta>
    {
        const auto content = read_file_to_string(filesystem::path(meta_path));
        if (!content.has_value())
        {
            return nullopt;
        }
        return load_from_string(*content);
    }

    auto sidecar_meta::load_from_string(string_view json_str) -> optional<sidecar_meta>
    {
        auto alloc = system_allocator{};
        auto doc_res = json_document::from_string(json_str, alloc);
        if (!doc_res.has_value())
        {
            return nullopt;
        }

        const auto root = doc_res->root();
        if (!root.is_object())
        {
            return nullopt;
        }

        auto meta = sidecar_meta{};

        const auto src_guid_val = root["source_guid"].as_string();
        if (src_guid_val.has_value())
        {
            const auto parsed = parse_guid(*src_guid_val);
            if (parsed.has_value())
            {
                meta.source_guid = *parsed;
            }
        }

        const auto importer_val = root["importer"].as_string();
        if (importer_val.has_value())
        {
            meta.importer = string(*importer_val);
        }

        const auto version_val = root["importer_version"].as_uint32();
        if (version_val.has_value())
        {
            meta.importer_version = *version_val;
        }

        const auto settings_val = root["settings"];
        if (settings_val.is_object())
        {
            const auto obj = settings_val.as_object().value();
            for (const auto& member : obj)
            {
                const auto str_val = member.value.as_string();
                if (str_val.has_value())
                {
                    meta.settings.insert({string(member.key), string(*str_val)});
                }
            }
        }

        const auto sub_assets_val = root["sub_assets"];
        if (sub_assets_val.is_object())
        {
            const auto obj = sub_assets_val.as_object().value();
            for (const auto& member : obj)
            {
                if (member.value.is_string())
                {
                    const auto parsed = parse_guid(member.value.as_string().value());
                    if (parsed.has_value())
                    {
                        meta.sub_assets.insert({string(member.key), *parsed});
                    }
                }
                else if (member.value.is_object())
                {
                    const auto sub_obj = member.value.as_object().value();
                    const auto guid_val = sub_obj["guid"].as_string();
                    if (guid_val.has_value())
                    {
                        const auto parsed = parse_guid(*guid_val);
                        if (parsed.has_value())
                        {
                            meta.sub_assets.insert({string(member.key), *parsed});
                        }
                    }
                    const auto type_val = sub_obj["type"].as_uint64();
                    if (type_val.has_value())
                    {
                        meta.sub_asset_types.insert(
                            {string(member.key), asset_type_id::from_hash(static_cast<size_t>(*type_val))});
                    }
                }
            }
        }

        const auto types_val = root["sub_asset_types"];
        if (types_val.is_object())
        {
            const auto obj = types_val.as_object().value();
            for (const auto& member : obj)
            {
                const auto type_u64 = member.value.as_uint64();
                if (type_u64.has_value())
                {
                    meta.sub_asset_types.insert(
                        {string(member.key), asset_type_id::from_hash(static_cast<size_t>(*type_u64))});
                }
            }
        }

        return meta;
    }

    auto sidecar_meta::to_string() const -> string
    {
        auto writer = json_writer{};
        auto root = writer.create_object();

        root.set("source_guid", tempest::to_string(source_guid));
        root.set("importer", importer);
        root.set("importer_version", importer_version);

        auto settings_obj = root.create_child_object("settings");
        for (const auto& setting_entry : settings)
        {
            settings_obj.set(setting_entry.first, setting_entry.second);
        }

        auto sub_obj = root.create_child_object("sub_assets");
        for (const auto& sub_entry : sub_assets)
        {
            sub_obj.set(sub_entry.first, tempest::to_string(sub_entry.second));
        }

        if (!sub_asset_types.empty())
        {
            auto types_obj = root.create_child_object("sub_asset_types");
            for (const auto& type_entry : sub_asset_types)
            {
                types_obj.set(type_entry.first, static_cast<uint64_t>(type_entry.second.hash()));
            }
        }

        writer.set_root(root);
        return writer.to_string(json_format::pretty);
    }

    //==============================================================================
    // project_workspace Implementation
    //==============================================================================

    auto project_workspace::cache_directory() const -> string
    {
        if (root_directory.empty())
        {
            return ".cache";
        }
        return (filesystem::path(root_directory) / ".cache").generic_string();
    }

    auto project_workspace::cache_db_path() const -> string
    {
        return (filesystem::path(cache_directory()) / "assets.tassetdb").generic_string();
    }

    auto project_workspace::load_from_file(string_view tproject_path) -> bool
    {
        const auto content = read_file_to_string(filesystem::path(tproject_path));
        if (!content.has_value())
        {
            return false;
        }

        auto ws_opt = load_from_string(*content);
        if (!ws_opt.has_value())
        {
            return false;
        }

        *this = tempest::move(*ws_opt);
        if (root_directory.empty())
        {
            root_directory = filesystem::path(tproject_path).parent_path().generic_string();
        }

        return true;
    }

    auto project_workspace::save_to_file(string_view tproject_path) const -> bool
    {
        const auto json_str = to_string();
        const auto write_res = write_file_from_bytes(
            filesystem::path(tproject_path),
            span<const byte>{reinterpret_cast<const byte*>(json_str.data()), json_str.size()});
        return write_res.has_value();
    }

    auto project_workspace::load_from_string(string_view json_str) -> optional<project_workspace>
    {
        auto alloc = system_allocator{};
        auto doc_res = json_document::from_string(json_str, alloc);
        if (!doc_res.has_value())
        {
            return nullopt;
        }

        const auto root = doc_res->root();
        if (!root.is_object())
        {
            return nullopt;
        }

        auto workspace = project_workspace{};

        const auto name_val = root["name"].as_string();
        if (name_val.has_value())
        {
            workspace.name = string(*name_val);
        }

        const auto root_dir_val = root["root_directory"].as_string();
        if (root_dir_val.has_value())
        {
            workspace.root_directory = string(*root_dir_val);
        }

        const auto mounts_val = root["mounts"];
        if (mounts_val.is_array())
        {
            const auto mount_elements = mounts_val.as_array().value();
            for (const auto mount_element : mount_elements)
            {
                if (mount_element.is_object())
                {
                    auto mount = mount_point{};
                    const auto mount_obj = mount_element.as_object().value();
                    const auto mount_path_value = mount_obj["path"].as_string();
                    if (mount_path_value.has_value())
                    {
                        mount.path = string(*mount_path_value);
                    }
                    const auto mount_priority_value = mount_obj["priority"].as_int32();
                    if (mount_priority_value.has_value())
                    {
                        mount.priority = *mount_priority_value;
                    }
                    const auto mount_alias_value = mount_obj["alias"].as_string();
                    if (mount_alias_value.has_value())
                    {
                        mount.alias = string(*mount_alias_value);
                    }
                    workspace.mounts.push_back(tempest::move(mount));
                }
            }
        }

        const auto startup_val = root["startup_scene"];
        if (startup_val.is_object())
        {
            const auto startup_obj = startup_val.as_object().value();
            const auto startup_guid_value = startup_obj["guid"].as_string();
            if (startup_guid_value.has_value() && !startup_guid_value->empty())
            {
                const auto parsed = parse_guid(*startup_guid_value);
                if (parsed.has_value())
                {
                    workspace.startup_scene.scene_guid = *parsed;
                }
            }
            const auto startup_path_value = startup_obj["path"].as_string();
            if (startup_path_value.has_value())
            {
                workspace.startup_scene.scene_path = string(*startup_path_value);
            }
        }
        else if (startup_val.is_string())
        {
            const auto scene_str = startup_val.as_string().value();
            const auto parsed = parse_guid(scene_str);
            if (parsed.has_value())
            {
                workspace.startup_scene.scene_guid = *parsed;
            }
            else
            {
                workspace.startup_scene.scene_path = string(scene_str);
            }
        }

        const auto bake_val = root["bake_settings"];
        if (bake_val.is_object())
        {
            const auto bake_obj = bake_val.as_object().value();
            const auto output_dir_value = bake_obj["output_directory"].as_string();
            if (output_dir_value.has_value())
            {
                workspace.settings.output_directory = string(*output_dir_value);
            }
            const auto compress_textures_value = bake_obj["compress_textures"].as_bool();
            if (compress_textures_value.has_value())
            {
                workspace.settings.compress_textures = *compress_textures_value;
            }
            const auto custom_val = bake_obj["custom_settings"];
            if (custom_val.is_object())
            {
                const auto custom_obj = custom_val.as_object().value();
                for (const auto& member : custom_obj)
                {
                    const auto member_str = member.value.as_string();
                    if (member_str.has_value())
                    {
                        workspace.settings.custom_settings.insert({string(member.key), string(*member_str)});
                    }
                }
            }
        }

        return workspace;
    }

    auto project_workspace::to_string() const -> string
    {
        auto writer = json_writer{};
        auto root = writer.create_object();

        root.set("name", name);
        root.set("root_directory", root_directory);

        auto mounts_arr = root.create_child_array("mounts");
        for (const auto& mount : mounts)
        {
            auto mount_obj = mounts_arr.create_child_object();
            mount_obj.set("path", mount.path);
            mount_obj.set("priority", mount.priority);
            mount_obj.set("alias", mount.alias);
        }

        auto scene_obj = root.create_child_object("startup_scene");
        if (startup_scene.scene_guid.has_value())
        {
            scene_obj.set("guid", tempest::to_string(*startup_scene.scene_guid));
        }
        scene_obj.set("path", startup_scene.scene_path);

        auto bake_obj = root.create_child_object("bake_settings");
        bake_obj.set("output_directory", settings.output_directory);
        bake_obj.set("compress_textures", settings.compress_textures);
        if (!settings.custom_settings.empty())
        {
            auto custom_obj = bake_obj.create_child_object("custom_settings");
            for (const auto& setting_entry : settings.custom_settings)
            {
                custom_obj.set(setting_entry.first, setting_entry.second);
            }
        }

        writer.set_root(root);
        return writer.to_string(json_format::pretty);
    }
} // namespace tempest::assets
