#include <tempest/scene/scene_asset_scanner.hpp>

#include "scene_codec_detail.hpp"

#include <tempest/guid.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    auto scene_asset_scanner::scan_document(string_view json_content,
                                            const ecs::component_type_registry& registry,
                                            string_view file_path,
                                            const guid& owner_guid) -> vector<scanned_asset_reference>
    {
        auto alloc = system_allocator{};
        auto doc_res = json_document::from_string(json_content, alloc);
        if (!doc_res.has_value())
        {
            return {};
        }

        const auto root_val = doc_res->root();
        if (!root_val.is_object())
        {
            return {};
        }

        const auto root_obj_res = root_val.as_object();
        if (!root_obj_res.has_value())
        {
            return {};
        }
        const auto root_obj = *root_obj_res;

        auto effective_owner_guid = owner_guid;
        if (effective_owner_guid == guid{})
        {
            const auto guid_val = root_obj["guid"];
            if (guid_val.is_string())
            {
                const auto parsed_root_guid = parse_guid(guid_val.as_string().value());
                if (parsed_root_guid.has_value())
                {
                    effective_owner_guid = *parsed_root_guid;
                }
            }
        }

        const auto entities_val = root_obj["entities"];
        if (!entities_val.is_array())
        {
            return {};
        }

        const auto entities_arr_res = entities_val.as_array();
        if (!entities_arr_res.has_value())
        {
            return {};
        }
        const auto entities_arr = *entities_arr_res;

        auto out_refs = vector<scanned_asset_reference>{};
        auto current_path = id_path{};

        auto scan_entity = [&](auto& self, json_value ent_val) -> void {
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

            const auto id_val = ent_obj["id"];
            auto local_id = entity_local_id{};
            if (id_val.is_string())
            {
                const auto parsed_id = parse_local_id(id_val.as_string().value());
                if (parsed_id.has_value())
                {
                    local_id = *parsed_id;
                }
            }

            const auto pushed_to_path = current_path.size() < max_id_path_depth;
            if (pushed_to_path)
            {
                current_path.push_back(local_id);
            }

            // Inspect components on this entity
            const auto comp_map_val = ent_obj["components"];
            if (comp_map_val.is_object())
            {
                const auto comp_map_res = comp_map_val.as_object();
                if (comp_map_res.has_value())
                {
                    for (const auto& member : *comp_map_res)
                    {
                        const auto comp_name = member.key;
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

                        const auto* type_info = detail::find_component_type_info(registry, comp_name);
                        if (type_info != nullptr && type_info->descriptor.has_value())
                        {
                            for (const auto& field : type_info->descriptor->fields)
                            {
                                if (field.kind == ecs::field_kind::asset_ref)
                                {
                                    const auto field_val = comp_obj[field.name];
                                    if (field_val.is_string())
                                    {
                                        const auto parsed_asset_id = parse_guid(field_val.as_string().value());
                                        if (parsed_asset_id.has_value() && *parsed_asset_id != guid{})
                                        {
                                            out_refs.push_back(scanned_asset_reference{
                                                .asset_id = *parsed_asset_id,
                                                .location = asset_reference_location{
                                                    .owner_guid = effective_owner_guid,
                                                    .target_path = current_path,
                                                    .component_name = string{type_info->name},
                                                    .field_name = string{field.name},
                                                    .owner_path = string{file_path},
                                                },
                                            });
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Inspect prefab block
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
                        if (parsed_prefab_guid.has_value() && *parsed_prefab_guid != guid{})
                        {
                            out_refs.push_back(scanned_asset_reference{
                                .asset_id = *parsed_prefab_guid,
                                .location = asset_reference_location{
                                    .owner_guid = effective_owner_guid,
                                    .target_path = current_path,
                                    .component_name = "tempest::scene::prefab_instance_component",
                                    .field_name = "prefab_guid",
                                    .owner_path = string{file_path},
                                },
                            });
                        }
                    }

                    auto overrides_val = prefab_obj["field_overrides"];
                    if (!overrides_val.is_array())
                    {
                        overrides_val = prefab_obj["overrides"];
                    }
                    if (overrides_val.is_array())
                    {
                        const auto ov_arr_res = overrides_val.as_array();
                        if (ov_arr_res.has_value())
                        {
                            for (const auto& ov_item_val : *ov_arr_res)
                            {
                                if (!ov_item_val.is_object())
                                {
                                    continue;
                                }
                                const auto ov_item_res = ov_item_val.as_object();
                                if (!ov_item_res.has_value())
                                {
                                    continue;
                                }
                                const auto ov_item = *ov_item_res;

                                const auto comp_val = ov_item["component"];
                                const auto field_val = ov_item["field"];
                                const auto value_val = ov_item["value"];
                                if (!comp_val.is_string() || !field_val.is_string() || !value_val.is_string())
                                {
                                    continue;
                                }

                                const auto comp_name = comp_val.as_string().value();
                                const auto field_name = field_val.as_string().value();

                                const auto* type_info = detail::find_component_type_info(registry, comp_name);
                                if (type_info != nullptr && type_info->descriptor.has_value())
                                {
                                    const auto* field_desc = type_info->descriptor->find_field(field_name);
                                    if (field_desc != nullptr && field_desc->kind == ecs::field_kind::asset_ref)
                                    {
                                        const auto parsed_asset_id = parse_guid(value_val.as_string().value());
                                        if (parsed_asset_id.has_value() && *parsed_asset_id != guid{})
                                        {
                                            auto target_path = current_path;
                                            const auto target_val = ov_item["target"];
                                            const auto parsed_target = parse_id_path(target_val);
                                            if (parsed_target.has_value() && !parsed_target->empty())
                                            {
                                                target_path = *parsed_target;
                                            }

                                            out_refs.push_back(scanned_asset_reference{
                                                .asset_id = *parsed_asset_id,
                                                .location = asset_reference_location{
                                                    .owner_guid = effective_owner_guid,
                                                    .target_path = tempest::move(target_path),
                                                    .component_name = string{type_info->name},
                                                    .field_name = string{field_name},
                                                    .owner_path = string{file_path},
                                                },
                                            });
                                        }
                                    }
                                }
                            }
                        }
                    }

                    const auto added_val = prefab_obj["added_components"];
                    if (added_val.is_array())
                    {
                        const auto added_arr_res = added_val.as_array();
                        if (added_arr_res.has_value())
                        {
                            for (const auto& add_item_val : *added_arr_res)
                            {
                                if (!add_item_val.is_object())
                                {
                                    continue;
                                }
                                const auto add_item_res = add_item_val.as_object();
                                if (!add_item_res.has_value())
                                {
                                    continue;
                                }
                                const auto add_item = *add_item_res;

                                const auto comp_val = add_item["component"];
                                if (!comp_val.is_string())
                                {
                                    continue;
                                }
                                const auto comp_name = comp_val.as_string().value();

                                const auto* type_info = detail::find_component_type_info(registry, comp_name);
                                if (type_info != nullptr && type_info->descriptor.has_value())
                                {
                                    const auto props_val = add_item["properties"];
                                    if (props_val.is_object())
                                    {
                                        const auto props_obj_res = props_val.as_object();
                                        if (props_obj_res.has_value())
                                        {
                                            const auto props_obj = *props_obj_res;
                                            auto target_path = current_path;
                                            const auto target_val = add_item["target"];
                                            const auto parsed_target = parse_id_path(target_val);
                                            if (parsed_target.has_value() && !parsed_target->empty())
                                            {
                                                target_path = *parsed_target;
                                            }

                                            for (const auto& field : type_info->descriptor->fields)
                                            {
                                                if (field.kind == ecs::field_kind::asset_ref)
                                                {
                                                    const auto field_val = props_obj[field.name];
                                                    if (field_val.is_string())
                                                    {
                                                        const auto parsed_asset_id =
                                                            parse_guid(field_val.as_string().value());
                                                        if (parsed_asset_id.has_value() && *parsed_asset_id != guid{})
                                                        {
                                                            out_refs.push_back(scanned_asset_reference{
                                                                .asset_id = *parsed_asset_id,
                                                                .location = asset_reference_location{
                                                                    .owner_guid = effective_owner_guid,
                                                                    .target_path = target_path,
                                                                    .component_name = string{type_info->name},
                                                                    .field_name = string{field.name},
                                                                    .owner_path = string{file_path},
                                                                },
                                                            });
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
                }
            }

            // Inspect child entities
            const auto children_val = ent_obj["children"];
            if (children_val.is_array())
            {
                const auto children_arr_res = children_val.as_array();
                if (children_arr_res.has_value())
                {
                    for (const auto& child_item : *children_arr_res)
                    {
                        self(self, child_item);
                    }
                }
            }

            if (pushed_to_path)
            {
                current_path.pop_back();
            }
        };

        for (const auto& ent_val : entities_arr)
        {
            scan_entity(scan_entity, ent_val);
        }

        return out_refs;
    }

    auto scene_asset_scanner::scan_document(reverse_dependency_index& index,
                                            string_view file_path,
                                            string_view json_content,
                                            const ecs::component_type_registry& registry,
                                            const guid& owner_guid) -> size_t
    {
        const auto scanned = scan_document(json_content, registry, file_path, owner_guid);
        for (const auto& ref : scanned)
        {
            index.add_reference(ref.asset_id, ref.location);
        }
        return scanned.size();
    }

    auto scene_asset_scanner::remove_references_for_file(reverse_dependency_index& index, string_view file_path)
        -> size_t
    {
        return index.remove_references_for_file(file_path);
    }

    auto scene_asset_scanner::record_references_for_file(reverse_dependency_index& index,
                                                         string_view file_path,
                                                         string_view json_content,
                                                         const ecs::component_type_registry& registry,
                                                         const guid& owner_guid) -> size_t
    {
        remove_references_for_file(index, file_path);
        return scan_document(index, file_path, json_content, registry, owner_guid);
    }
} // namespace tempest::scene
