#include <tempest/scene/scene_reference_stripper.hpp>

#include "scene_codec_detail.hpp"

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/files.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/reverse_dependency_index.hpp>
#include <tempest/scene/scene_asset_scanner.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    namespace
    {
        auto entity_directly_references_asset(json_object ent_obj, const guid& target_asset_id,
                                              const ecs::component_type_registry& registry) -> bool
        {
            // Check if entity is an instance of the target prefab
            const auto prefab_val = ent_obj["prefab"];
            if (prefab_val.is_object())
            {
                const auto prefab_obj_res = prefab_val.as_object();
                if (prefab_obj_res.has_value())
                {
                    const auto prefab_obj = *prefab_obj_res;
                    auto prefab_guid_val = prefab_obj["prefab_guid"];
                    if (!prefab_guid_val.is_string())
                    {
                        prefab_guid_val = prefab_obj["guid"];
                    }
                    if (prefab_guid_val.is_string())
                    {
                        const auto parsed_prefab_guid = parse_guid(prefab_guid_val.as_string().value());
                        if (parsed_prefab_guid.has_value() && *parsed_prefab_guid == target_asset_id)
                        {
                            return true;
                        }
                    }
                }
            }

            // Check if any reflected component on this entity has an asset_ref field targeting target_asset_id
            const auto comp_map_val = ent_obj["components"];
            if (comp_map_val.is_object())
            {
                const auto comp_map_res = comp_map_val.as_object();
                if (comp_map_res.has_value())
                {
                    for (const auto& member : *comp_map_res)
                    {
                        const auto comp_val = member.value;
                        if (!comp_val.is_object())
                        {
                            continue;
                        }

                        const auto comp_obj_res = comp_val.as_object();
                        if (!comp_obj_res.has_value())
                        {
                            continue;
                        }
                        const auto comp_obj = *comp_obj_res;

                        const auto* type_info = detail::find_component_type_info(registry, member.key);
                        if (type_info != nullptr && type_info->descriptor.has_value())
                        {
                            for (const auto& field : type_info->descriptor->fields)
                            {
                                if (field.kind == ecs::field_kind::asset_ref)
                                {
                                    const auto field_val = comp_obj[field.name];
                                    if (field_val.is_string())
                                    {
                                        const auto parsed_guid = parse_guid(field_val.as_string().value());
                                        if (parsed_guid.has_value() && *parsed_guid == target_asset_id)
                                        {
                                            return true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            return false;
        }

        auto process_entity_recursive(json_value ent_val, json_array_mut& target_arr, const guid& target_asset_id,
                                      const ecs::component_type_registry& registry,
                                      vector<uint64_t>& stripped_ids, size_t& stripped_count) -> void
        {
            if (!ent_val.is_object())
            {
                return;
            }

            const auto ent_obj_res = ent_val.as_object();
            if (!ent_obj_res.has_value())
            {
                return;
            }
            const auto ent_obj = *ent_obj_res;

            auto local_id = entity_local_id{};
            const auto id_val = ent_obj["id"];
            if (id_val.is_string())
            {
                const auto parsed_id = parse_local_id(id_val.as_string().value());
                if (parsed_id.has_value())
                {
                    local_id = *parsed_id;
                }
            }

            // Check if parent was stripped (for flat entity hierarchies)
            const auto parent_val = ent_obj["parent"];
            if (parent_val.is_string())
            {
                const auto parsed_parent = parse_local_id(parent_val.as_string().value());
                if (parsed_parent.has_value() &&
                    tempest::find(stripped_ids.begin(), stripped_ids.end(), parsed_parent->value) != stripped_ids.end())
                {
                    if (local_id.is_valid())
                    {
                        stripped_ids.push_back(local_id.value);
                    }
                    ++stripped_count;
                    return;
                }
            }

            // Check if entity directly references target_asset_id
            if (entity_directly_references_asset(ent_obj, target_asset_id, registry))
            {
                if (local_id.is_valid())
                {
                    stripped_ids.push_back(local_id.value);
                }
                ++stripped_count;
                return;
            }

            // Entity is retained; rebuild its members and inspect nested overrides / children
            auto new_ent_obj = target_arr.create_child_object();

            for (const auto& member : ent_obj)
            {
                if (member.key == "prefab")
                {
                    if (member.value.is_object())
                    {
                        const auto prefab_res = member.value.as_object();
                        if (prefab_res.has_value())
                        {
                            auto new_prefab_obj = new_ent_obj.create_child_object("prefab");
                            const auto prefab_obj = *prefab_res;

                            for (const auto& p_member : prefab_obj)
                            {
                                if (p_member.key == "field_overrides" || p_member.key == "overrides")
                                {
                                    if (p_member.value.is_array())
                                    {
                                        const auto ov_arr_res = p_member.value.as_array();
                                        if (ov_arr_res.has_value())
                                        {
                                            auto new_ov_arr = new_prefab_obj.create_child_array(p_member.key);
                                            for (const auto& ov_item : *ov_arr_res)
                                            {
                                                auto override_references_target = false;
                                                if (ov_item.is_object())
                                                {
                                                    const auto ov_obj_res = ov_item.as_object();
                                                    if (ov_obj_res.has_value())
                                                    {
                                                        const auto ov_obj = *ov_obj_res;
                                                        const auto comp_val = ov_obj["component"];
                                                        const auto field_val = ov_obj["field"];
                                                        const auto value_val = ov_obj["value"];

                                                        if (comp_val.is_string() && field_val.is_string() &&
                                                            value_val.is_string())
                                                        {
                                                            const auto* type_info = detail::find_component_type_info(
                                                                registry, comp_val.as_string().value());
                                                            if (type_info != nullptr &&
                                                                type_info->descriptor.has_value())
                                                            {
                                                                const auto* f_desc = type_info->descriptor->find_field(
                                                                    field_val.as_string().value());
                                                                if (f_desc != nullptr &&
                                                                    f_desc->kind == ecs::field_kind::asset_ref)
                                                                {
                                                                    const auto parsed_ov_guid =
                                                                        parse_guid(value_val.as_string().value());
                                                                    if (parsed_ov_guid.has_value() &&
                                                                        *parsed_ov_guid == target_asset_id)
                                                                    {
                                                                        override_references_target = true;
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }

                                                if (override_references_target)
                                                {
                                                    ++stripped_count;
                                                }
                                                else
                                                {
                                                    new_ov_arr.push_back(ov_item);
                                                }
                                            }
                                            continue;
                                        }
                                    }
                                }
                                else if (p_member.key == "added_components")
                                {
                                    if (p_member.value.is_array())
                                    {
                                        const auto ac_arr_res = p_member.value.as_array();
                                        if (ac_arr_res.has_value())
                                        {
                                            auto new_ac_arr = new_prefab_obj.create_child_array("added_components");
                                            for (const auto& ac_item : *ac_arr_res)
                                            {
                                                auto added_references_target = false;
                                                if (ac_item.is_object())
                                                {
                                                    const auto ac_obj_res = ac_item.as_object();
                                                    if (ac_obj_res.has_value())
                                                    {
                                                        const auto ac_obj = *ac_obj_res;
                                                        const auto comp_val = ac_obj["component"];
                                                        if (comp_val.is_string())
                                                        {
                                                            const auto* type_info = detail::find_component_type_info(
                                                                registry, comp_val.as_string().value());
                                                            if (type_info != nullptr &&
                                                                type_info->descriptor.has_value())
                                                            {
                                                                const auto props_val = ac_obj["properties"];
                                                                if (props_val.is_object())
                                                                {
                                                                    const auto props_obj_res = props_val.as_object();
                                                                    if (props_obj_res.has_value())
                                                                    {
                                                                        const auto props_obj = *props_obj_res;
                                                                        for (const auto& field :
                                                                             type_info->descriptor->fields)
                                                                        {
                                                                            if (field.kind == ecs::field_kind::asset_ref)
                                                                            {
                                                                                const auto prop_field_val =
                                                                                    props_obj[field.name];
                                                                                if (prop_field_val.is_string())
                                                                                {
                                                                                    const auto parsed_ac_guid =
                                                                                        parse_guid(prop_field_val
                                                                                                       .as_string()
                                                                                                       .value());
                                                                                    if (parsed_ac_guid.has_value() &&
                                                                                        *parsed_ac_guid ==
                                                                                            target_asset_id)
                                                                                    {
                                                                                        added_references_target = true;
                                                                                        break;
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }

                                                if (added_references_target)
                                                {
                                                    ++stripped_count;
                                                }
                                                else
                                                {
                                                    new_ac_arr.push_back(ac_item);
                                                }
                                            }
                                            continue;
                                        }
                                    }
                                }

                                new_prefab_obj.set(p_member.key, p_member.value);
                            }
                            continue;
                        }
                    }
                }
                else if (member.key == "children")
                {
                    if (member.value.is_array())
                    {
                        const auto children_arr_res = member.value.as_array();
                        if (children_arr_res.has_value())
                        {
                            auto new_children_arr = new_ent_obj.create_child_array("children");
                            for (const auto& child_item : *children_arr_res)
                            {
                                process_entity_recursive(child_item, new_children_arr, target_asset_id, registry,
                                                         stripped_ids, stripped_count);
                            }
                            continue;
                        }
                    }
                }

                new_ent_obj.set(member.key, member.value);
            }
        }
    } // namespace

    auto strip_references_from_file(const filesystem::path& file_path, const guid& target_asset_id,
                                    const ecs::component_type_registry& registry) -> expected<size_t, scene_error>
    {
        const auto content_res = read_file_to_string(file_path);
        if (!content_res.has_value())
        {
            if (content_res.error() == file_error::not_found)
            {
                return unexpected(scene_error::file_not_found);
            }
            return unexpected(scene_error::io_error);
        }

        auto alloc = system_allocator{};
        auto doc_res = json_document::from_string(*content_res, alloc);
        if (!doc_res.has_value())
        {
            return unexpected(scene_error::invalid_json);
        }

        const auto root_val = doc_res->root();
        if (!root_val.is_object())
        {
            return unexpected(scene_error::invalid_json);
        }

        const auto root_obj_res = root_val.as_object();
        if (!root_obj_res.has_value())
        {
            return unexpected(scene_error::invalid_json);
        }
        const auto root_obj = *root_obj_res;

        const auto entities_val = root_obj["entities"];
        if (!entities_val.is_array())
        {
            return unexpected(scene_error::invalid_json);
        }

        const auto entities_arr_res = entities_val.as_array();
        if (!entities_arr_res.has_value())
        {
            return unexpected(scene_error::invalid_json);
        }
        const auto entities_arr = *entities_arr_res;

        auto writer = json_writer{alloc};
        auto new_root = writer.create_object();

        for (const auto& member : root_obj)
        {
            if (member.key != "entities")
            {
                new_root.set(member.key, member.value);
            }
        }

        auto new_entities_arr = new_root.create_child_array("entities");
        auto stripped_ids = vector<uint64_t>{};
        auto stripped_count = size_t{0};

        for (const auto& ent_val : entities_arr)
        {
            process_entity_recursive(ent_val, new_entities_arr, target_asset_id, registry, stripped_ids,
                                     stripped_count);
        }

        if (stripped_count > 0)
        {
            writer.set_root(new_root);
            const auto serialized = writer.to_string(json_format::pretty);
            const auto byte_span = span<const byte>{
                reinterpret_cast<const byte*>(serialized.data()),
                serialized.size(),
            };

            auto temp_str = file_path.string();
            temp_str.append(".tmp");
            const auto temp_path = filesystem::path{temp_str};
            const auto write_res = write_file_from_bytes(temp_path, byte_span);
            if (!write_res.has_value())
            {
                return unexpected(scene_error::io_error);
            }

            if (!filesystem::rename(temp_path, file_path))
            {
                const auto direct_write_res = write_file_from_bytes(file_path, byte_span);
                filesystem::remove(temp_path);
                if (!direct_write_res.has_value())
                {
                    return unexpected(scene_error::io_error);
                }
            }
        }

        return stripped_count;
    }

    auto strip_references_from_registry(ecs::basic_archetype_registry& registry, const guid& target_asset_id)
        -> vector<ecs::entity>
    {
        auto matching_entities = vector<ecs::entity>{};

        for (const auto [self] : registry.with<ecs::self_component>())
        {
            const auto entity = self.entity;
            if (!registry.is_valid(entity))
            {
                continue;
            }

            auto is_match = false;

            const auto* prefab_comp = registry.try_get<prefab_instance_component>(entity);
            if (prefab_comp != nullptr && prefab_comp->prefab_guid == target_asset_id)
            {
                is_match = true;
            }

            if (!is_match)
            {
                const auto comp_types = ecs::raw_registry_access::get_component_types(registry, entity);
                for (const auto& info : comp_types)
                {
                    if (!info.descriptor.has_value())
                    {
                        continue;
                    }

                    const auto* comp_ptr = ecs::raw_registry_access::get_component_raw(registry, entity, info.index);
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

                            if (field_guid == target_asset_id)
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

        for (const auto entity : matching_entities)
        {
            if (registry.is_valid(entity))
            {
                registry.destroy_recursive(entity);
            }
        }

        return matching_entities;
    }

    auto strip_asset_references(reverse_dependency_index& index, const guid& target_asset_id,
                                const ecs::component_type_registry& registry,
                                ecs::basic_archetype_registry* active_registry) -> size_t
    {
        auto total_stripped = size_t{0};

        if (active_registry != nullptr)
        {
            const auto destroyed = strip_references_from_registry(*active_registry, target_asset_id);
            total_stripped += destroyed.size();
        }

        const auto refs = index.find_references(target_asset_id);
        auto affected_files = vector<string>{};
        for (const auto& loc : refs)
        {
            if (!loc.owner_path.empty())
            {
                auto already_added = false;
                for (const auto& path : affected_files)
                {
                    if (path == loc.owner_path)
                    {
                        already_added = true;
                        break;
                    }
                }
                if (!already_added)
                {
                    affected_files.push_back(loc.owner_path);
                }
            }
        }

        for (const auto& file_path_str : affected_files)
        {
            const auto affected_path = filesystem::path{file_path_str};
            if (filesystem::exists(affected_path))
            {
                const auto stripped_res = strip_references_from_file(affected_path, target_asset_id, registry);
                if (stripped_res.has_value())
                {
                    total_stripped += *stripped_res;
                }

                const auto content_res = read_file_to_string(affected_path);
                if (content_res.has_value())
                {
                    scene_asset_scanner::record_references_for_file(index, file_path_str, *content_res, registry);
                }
                else
                {
                    index.remove_references_for_file(file_path_str);
                }
            }
            else
            {
                index.remove_references_for_file(file_path_str);
            }
        }

        if (index.has_references(target_asset_id))
        {
            index.remove_references_for_file("");
        }

        const auto remaining = index.find_references(target_asset_id);
        for (const auto& ref : remaining)
        {
            [[maybe_unused]] const auto removed = index.remove_reference(target_asset_id, ref);
        }

        return total_stripped;
    }
} // namespace tempest::scene
