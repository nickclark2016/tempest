#include <tempest/scene/prefab_codec.hpp>

#include "scene_codec_detail.hpp"

#include <tempest/algorithm.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    auto prefab_codec::serialize(const prefab_template& tmpl, const guid& prefab_guid,
                                 const prefab_serialize_options& options) -> string
    {
        auto writer = json_writer{};
        auto root_obj = writer.create_object();

        root_obj.set("format", "tempest.prefab");
        root_obj.set("format_version", static_cast<int32_t>(1));
        root_obj.set("guid", tempest::to_string(prefab_guid));

        auto entities_arr = root_obj.create_child_array("entities");
        const auto records = tmpl.get_records();
        if (records.empty())
        {
            writer.set_root(root_obj);
            return writer.to_string(options.format);
        }

        auto children_of = vector<vector<size_t>>(records.size());
        auto root_indices = vector<size_t>{};

        for (auto rec_idx = size_t{0}; rec_idx < records.size(); ++rec_idx)
        {
            if (records[rec_idx].parent_index.has_value())
            {
                const auto parent_idx = *records[rec_idx].parent_index;
                if (parent_idx < records.size())
                {
                    children_of[parent_idx].push_back(rec_idx);
                }
            }
            else
            {
                root_indices.push_back(rec_idx);
            }
        }

        auto serialize_rec = [&](auto& self, size_t rec_idx, json_array_mut& parent_arr) -> void {
            const auto& rec = records[rec_idx];
            auto ent_obj = parent_arr.create_child_object();

            ent_obj.set("id", to_hex_string(rec.local_id));
            if (!rec.name.empty())
            {
                ent_obj.set("name", rec.name);
            }

            if (rec.parent_index.has_value() && *rec.parent_index < records.size())
            {
                ent_obj.set("parent", to_hex_string(records[*rec.parent_index].local_id));
            }
            else
            {
                ent_obj.set_null("parent");
            }

            if (rec.nested_instance.has_value())
            {
                auto nested_obj = ent_obj.create_child_object("prefab");
                nested_obj.set("guid", tempest::to_string(rec.nested_instance->prefab_guid));
                nested_obj.set("instance_id", to_hex_string(rec.nested_instance->instance_local_id));
            }

            auto comp_map_obj = ent_obj.create_child_object("components");
            const auto is_root = !rec.parent_index.has_value();

            auto sorted_comps = vector<const staged_component*>{};
            for (const auto& comp : rec.components)
            {
                if (comp.type_info != nullptr && comp.type_info->descriptor.has_value())
                {
                    // Requirement 7c: Root transform is never persisted to .tprefab!
                    if (is_root && detail::is_transform_component(comp.type_info->name))
                    {
                        continue;
                    }
                    sorted_comps.push_back(&comp);
                }
            }

            tempest::sort(sorted_comps.begin(), sorted_comps.end(), [](const auto* lhs, const auto* rhs) -> bool {
                return lhs->type_info->name < rhs->type_info->name;
            });

            for (const auto* comp_ptr : sorted_comps)
            {
                const auto& info = *comp_ptr->type_info;
                auto comp_obj = comp_map_obj.create_child_object(info.name);
                for (const auto& field : info.descriptor->fields)
                {
                    if ((field.flags & ecs::field_flags::transient) != ecs::field_flags::none)
                    {
                        continue;
                    }
                    detail::serialize_field(field, comp_ptr->data.data(), comp_obj, nullptr);
                }
            }

            if (!children_of[rec_idx].empty())
            {
                auto children_arr = ent_obj.create_child_array("children");
                for (const auto child_idx : children_of[rec_idx])
                {
                    self(self, child_idx, children_arr);
                }
            }
        };

        for (const auto root_idx : root_indices)
        {
            serialize_rec(serialize_rec, root_idx, entities_arr);
        }

        writer.set_root(root_obj);
        return writer.to_string(options.format);
    }

    auto prefab_codec::serialize(const ecs::archetype_registry& registry, ecs::entity root_entity,
                                 const guid& prefab_guid, const prefab_serialize_options& options) -> string
    {
        const auto tmpl = prefab_template::from_entity(registry, root_entity);
        return serialize(tmpl, prefab_guid, options);
    }

    auto prefab_codec::deserialize(string_view json_content, const ecs::component_type_registry& type_registry)
        -> expected<prefab_deserialize_result, prefab_codec_error>
    {
        auto alloc = system_allocator{};
        auto doc_res = json_document::from_string(json_content, alloc);
        if (!doc_res.has_value())
        {
            return unexpected(prefab_codec_error::invalid_json);
        }

        const auto root_val = doc_res->root();
        if (!root_val.is_object())
        {
            return unexpected(prefab_codec_error::invalid_root_object);
        }

        const auto root_obj_res = root_val.as_object();
        if (!root_obj_res.has_value())
        {
            return unexpected(prefab_codec_error::invalid_root_object);
        }
        const auto root_obj = *root_obj_res;

        const auto format_val = root_obj["format"];
        if (!format_val.is_string() || format_val.as_string().value() != "tempest.prefab")
        {
            return unexpected(prefab_codec_error::unsupported_format);
        }

        const auto ver_val = root_obj["format_version"];
        if ((!ver_val.is_int() && !ver_val.is_uint() && !ver_val.is_number()) || ver_val.as_int64().value() != 1)
        {
            return unexpected(prefab_codec_error::version_mismatch);
        }

        auto prefab_guid = guid{};
        const auto guid_val = root_obj["guid"];
        if (guid_val.is_string())
        {
            const auto parsed = parse_guid(guid_val.as_string().value());
            if (parsed.has_value())
            {
                prefab_guid = *parsed;
            }
        }

        const auto entities_val = root_obj["entities"];
        if (!entities_val.is_array())
        {
            return unexpected(prefab_codec_error::missing_entities);
        }

        const auto entities_arr_res = entities_val.as_array();
        if (!entities_arr_res.has_value() || entities_arr_res->empty())
        {
            return unexpected(prefab_codec_error::missing_entities);
        }

        auto records = vector<prefab_entity_record>{};
        auto local_to_index = flat_unordered_map<uint64_t, size_t>{};

        struct pending_parent_link
        {
            size_t record_index;
            entity_local_id parent_local_id;
        };
        auto pending_links = vector<pending_parent_link>{};

        auto parse_entity = [&](auto& self, json_value ent_val, optional<size_t> parent_idx,
                                bool is_root) -> expected<size_t, prefab_codec_error> {
            if (!ent_val.is_object())
            {
                return unexpected(prefab_codec_error::invalid_entity_record);
            }
            const auto ent_obj_res = ent_val.as_object();
            if (!ent_obj_res.has_value())
            {
                return unexpected(prefab_codec_error::invalid_entity_record);
            }
            const auto ent_obj = *ent_obj_res;

            const auto id_val = ent_obj["id"];
            if (!id_val.is_string())
            {
                return unexpected(prefab_codec_error::invalid_entity_record);
            }
            const auto parsed_id = parse_local_id(id_val.as_string().value());
            if (!parsed_id.has_value() || !parsed_id->is_valid())
            {
                return unexpected(prefab_codec_error::invalid_entity_record);
            }
            const auto local_id = *parsed_id;

            auto name = string{};
            const auto name_val = ent_obj["name"];
            if (name_val.is_string())
            {
                name = string{name_val.as_string().value()};
            }

            // Requirement 7c: Root transform is forbidden in .tprefab!
            const auto comp_map_val = ent_obj["components"];
            if (comp_map_val.is_object())
            {
                const auto comp_map_res = comp_map_val.as_object();
                if (comp_map_res.has_value())
                {
                    for (const auto& member : *comp_map_res)
                    {
                        if (is_root && detail::is_transform_component(member.key))
                        {
                            return unexpected(prefab_codec_error::root_transform_forbidden);
                        }
                    }
                }
            }

            auto rec = prefab_entity_record{};
            rec.local_id = local_id;
            rec.name = tempest::move(name);
            rec.parent_index = parent_idx;

            const auto parent_field = ent_obj["parent"];
            if (!parent_field.is_null())
            {
                if (!parent_field.is_string())
                {
                    return unexpected(prefab_codec_error::invalid_entity_record);
                }
                const auto parsed_parent_id = parse_local_id(parent_field.as_string().value());
                if (!parsed_parent_id.has_value() || !parsed_parent_id->is_valid())
                {
                    return unexpected(prefab_codec_error::invalid_entity_record);
                }
                if (!rec.parent_index.has_value())
                {
                    const auto iter = local_to_index.find(parsed_parent_id->value);
                    if (iter != local_to_index.end())
                    {
                        rec.parent_index = iter->second;
                    }
                    else
                    {
                        pending_links.push_back({
                            .record_index = records.size(),
                            .parent_local_id = *parsed_parent_id,
                        });
                    }
                }
            }

            const auto prefab_val = ent_obj["prefab"];
            if (prefab_val.is_object())
            {
                const auto p_obj_res = prefab_val.as_object();
                if (p_obj_res.has_value())
                {
                    const auto p_obj = *p_obj_res;
                    auto n_guid = guid{};
                    const auto g_val = p_obj["guid"];
                    if (g_val.is_string())
                    {
                        const auto pg = parse_guid(g_val.as_string().value());
                        if (pg.has_value())
                        {
                            n_guid = *pg;
                        }
                    }
                    auto n_id = entity_local_id{};
                    const auto i_val = p_obj["instance_id"];
                    if (i_val.is_string())
                    {
                        const auto pi = parse_local_id(i_val.as_string().value());
                        if (pi.has_value())
                        {
                            n_id = *pi;
                        }
                    }
                    rec.nested_instance = nested_prefab_descriptor{
                        .prefab_guid = n_guid,
                        .instance_local_id = n_id,
                    };
                }
            }

            if (comp_map_val.is_object())
            {
                const auto comp_map_res = comp_map_val.as_object();
                if (comp_map_res.has_value())
                {
                    for (const auto& member : *comp_map_res)
                    {
                        const auto* type_info = detail::find_component_type_info(type_registry, member.key);
                        if (is_root && type_info != nullptr && detail::is_transform_component(type_info->name))
                        {
                            return unexpected(prefab_codec_error::root_transform_forbidden);
                        }
                        if (type_info != nullptr && type_info->descriptor.has_value())
                        {
                            auto staged = staged_component{};
                            staged.type_info = type_info;
                            staged.data.resize(type_info->size, byte{0});
                            if (type_info->construct != nullptr)
                            {
                                type_info->construct(staged.data.data());
                            }
                            for (const auto& field : type_info->descriptor->fields)
                            {
                                if (field.kind == ecs::field_kind::entity_ref)
                                {
                                    detail::set_field_val(field, staged.data.data(), ecs::tombstone);
                                }
                            }

                            if (member.value.is_object())
                            {
                                const auto fields_obj_res = member.value.as_object();
                                if (fields_obj_res.has_value())
                                {
                                    for (const auto& field_member : *fields_obj_res)
                                    {
                                        const auto* field_desc = type_info->descriptor->find_field(field_member.key);
                                        if (field_desc != nullptr)
                                        {
                                            detail::deserialize_field(*field_desc, field_member.value,
                                                                      staged.data.data());
                                        }
                                    }
                                }
                            }

                            if (type_info->descriptor->post_load != nullptr)
                            {
                                type_info->descriptor->post_load(staged.data.data());
                            }

                            rec.components.push_back(tempest::move(staged));
                        }
                    }
                }
            }

            if (local_to_index.find(local_id.value) != local_to_index.end())
            {
                return unexpected(prefab_codec_error::invalid_entity_record);
            }

            const auto current_idx = records.size();
            records.push_back(tempest::move(rec));
            local_to_index[local_id.value] = current_idx;

            const auto children_val = ent_obj["children"];
            if (children_val.is_array())
            {
                const auto children_arr_res = children_val.as_array();
                if (children_arr_res.has_value())
                {
                    for (const auto& child_val : *children_arr_res)
                    {
                        const auto child_res = self(self, child_val, current_idx, false);
                        if (!child_res.has_value())
                        {
                            return unexpected(child_res.error());
                        }
                    }
                }
            }

            return current_idx;
        };

        for (auto item_idx = size_t{0}; item_idx < entities_arr_res->size(); ++item_idx)
        {
            const auto ent_item = (*entities_arr_res)[item_idx];
            auto is_item_root = true;
            if (ent_item.is_object())
            {
                const auto obj_res = ent_item.as_object();
                if (obj_res.has_value())
                {
                    const auto parent_field = (*obj_res)["parent"];
                    if (parent_field.is_string())
                    {
                        is_item_root = false;
                    }
                }
            }
            const auto parse_res = parse_entity(parse_entity, ent_item, nullopt, is_item_root);
            if (!parse_res.has_value())
            {
                return unexpected(parse_res.error());
            }
        }

        // Resolve any pending parent references
        for (const auto& link : pending_links)
        {
            const auto iter = local_to_index.find(link.parent_local_id.value);
            if (iter == local_to_index.end())
            {
                return unexpected(prefab_codec_error::invalid_entity_record);
            }
            records[link.record_index].parent_index = iter->second;
        }

        // Cycle and depth validation
        for (auto rec_idx = size_t{0}; rec_idx < records.size(); ++rec_idx)
        {
            auto current = records[rec_idx].parent_index;
            auto depth = size_t{0};
            while (current.has_value())
            {
                ++depth;
                if (depth > records.size())
                {
                    return unexpected(prefab_codec_error::cyclic_hierarchy);
                }
                if (depth > max_id_path_depth)
                {
                    return unexpected(prefab_codec_error::depth_exceeded);
                }
                current = records[*current].parent_index;
            }
        }

        auto tmpl = prefab_template{tempest::move(records)};
        if (tmpl.get_root_indices().size() != 1)
        {
            return unexpected(prefab_codec_error::invalid_entity_record);
        }

        auto result = prefab_deserialize_result{};
        result.template_record = tempest::move(tmpl);
        result.prefab_guid = prefab_guid;
        result.success = true;

        return result;
    }
} // namespace tempest::scene
