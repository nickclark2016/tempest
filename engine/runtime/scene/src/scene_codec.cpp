#include <tempest/scene/scene_codec.hpp>

#include "scene_codec_detail.hpp"

#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/random.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_instantiator.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    namespace
    {
        using namespace detail;

        struct staging_buffer
        {
            static constexpr size_t default_stack_limit = 512;
            static constexpr size_t default_max_stack_alignment = 64;

            alignas(default_max_stack_alignment) array<byte, default_stack_limit> stack_storage = {};
            byte* heap_storage = nullptr;
            byte* data = nullptr;

            staging_buffer(size_t size, size_t alignment)
            {
                const auto effective_alignment = tempest::max(alignment, alignof(void*));
                if (size <= default_stack_limit && effective_alignment <= default_max_stack_alignment)
                {
                    data = stack_storage.data();
                }
                else
                {
                    heap_storage = static_cast<byte*>(tempest::aligned_alloc(size, effective_alignment));
                    data = heap_storage;
                }
            }

            ~staging_buffer()
            {
                if (heap_storage != nullptr)
                {
                    tempest::aligned_free(heap_storage);
                }
            }

            staging_buffer(const staging_buffer&) = delete;
            staging_buffer(staging_buffer&&) = delete;
            auto operator=(const staging_buffer&) -> staging_buffer& = delete;
            auto operator=(staging_buffer&&) -> staging_buffer& = delete;
        };

        void ensure_entity_local_ids_recursive(ecs::basic_archetype_registry& registry,
                                               ecs::entity entity,
                                               scene_handle handle)
        {
            if (!registry.is_valid(entity))
            {
                return;
            }

            const auto* scene_comp = registry.try_get<scene_entity_component>(entity);
            if (scene_comp == nullptr)
            {
                const auto new_id = generate_random_local_id();
                registry.assign(entity, scene_entity_component{
                                            .scene = handle,
                                            .local_id = new_id,
                                        });
            }
            else if (!scene_comp->local_id.is_valid())
            {
                const auto new_id = generate_random_local_id();
                registry.replace(entity, scene_entity_component{
                                             .scene = scene_comp->scene,
                                             .local_id = new_id,
                                         });
            }

            const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(entity);
            if (rel != nullptr)
            {
                auto child = rel->first_child;
                while (child != ecs::tombstone && child != ecs::null && registry.is_valid(child))
                {
                    ensure_entity_local_ids_recursive(registry, child, handle);
                    const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                    child = child_rel ? child_rel->next_sibling : ecs::tombstone;
                }
            }
        }

        void serialize_entity_recursive(ecs::basic_archetype_registry& registry,
                                        ecs::entity entity,
                                        json_array_mut& parent_array,
                                        const scene_unrecognized_data& unrecognized_data)
        {
            const auto* scene_comp = registry.try_get<scene_entity_component>(entity);
            const auto local_id = scene_comp ? scene_comp->local_id : entity_local_id{};

            auto entity_obj = parent_array.create_child_object();
            entity_obj.set("id", to_hex_string(local_id));

            const auto name_opt = registry.name(entity);
            if (name_opt.has_value() && !name_opt->empty())
            {
                entity_obj.set("name", *name_opt);
            }

            const auto* prefab_comp = registry.try_get<prefab_instance_component>(entity);
            if (prefab_comp != nullptr)
            {
                auto prefab_obj = entity_obj.create_child_object("prefab");
                prefab_obj.set("guid", tempest::to_string(prefab_comp->prefab_guid));
                prefab_obj.set("instance_id", to_hex_string(prefab_comp->instance_local_id));

                auto overrides_arr = prefab_obj.create_child_array("overrides");
                for (const auto& ov : prefab_comp->overrides.field_overrides)
                {
                    auto ov_obj = overrides_arr.create_child_object();
                    auto target_arr = ov_obj.create_child_array("target");
                    for (const auto target_id : ov.target)
                    {
                        target_arr.push_back(to_hex_string(target_id));
                    }
                    ov_obj.set("component", ov.component_name);
                    ov_obj.set("field", ov.field_name);

                    const auto* type_info = find_component_type_info(registry.type_registry(), ov.component_name);
                    if (type_info != nullptr && type_info->descriptor.has_value())
                    {
                        const auto* field_desc = type_info->descriptor->find_field(ov.field_name);
                        if (field_desc != nullptr && !ov.value_bytes.empty())
                        {
                            serialize_raw_field_value_to_key(*field_desc, ov.value_bytes.data(), ov_obj, "value", &registry);
                        }
                        else
                        {
                            ov_obj.set_null("value");
                        }
                    }
                    else
                    {
                        ov_obj.set_null("value");
                    }
                }

                auto added_arr = prefab_obj.create_child_array("added_components");
                for (const auto& ac : prefab_comp->overrides.added_components)
                {
                    auto ac_obj = added_arr.create_child_object();
                    auto target_arr = ac_obj.create_child_array("target");
                    for (const auto target_id : ac.target)
                    {
                        target_arr.push_back(to_hex_string(target_id));
                    }
                    if (ac.component.type_info != nullptr)
                    {
                        const auto& type_info = *ac.component.type_info;
                        ac_obj.set("component", type_info.name);
                        if (type_info.descriptor.has_value())
                        {
                            auto props_obj = ac_obj.create_child_object("properties");
                            for (const auto& field : type_info.descriptor->fields)
                            {
                                if ((field.flags & ecs::field_flags::transient) != ecs::field_flags::none)
                                {
                                    continue;
                                }
                                serialize_field(field, ac.component.data.data(), props_obj, registry);
                            }
                        }
                    }
                }

                auto removed_arr = prefab_obj.create_child_array("removed_components");
                for (const auto& rc : prefab_comp->overrides.removed_components)
                {
                    auto rc_obj = removed_arr.create_child_object();
                    auto target_arr = rc_obj.create_child_array("target");
                    for (const auto target_id : rc.target)
                    {
                        target_arr.push_back(to_hex_string(target_id));
                    }
                    rc_obj.set("component", rc.component_name);
                }

                auto added_children_arr = prefab_obj.create_child_array("added_children");
                for (const auto child_id : prefab_comp->overrides.added_children)
                {
                    added_children_arr.push_back(to_hex_string(child_id));
                }
            }

            auto comp_map_obj = entity_obj.create_child_object("components");

            const entity_unrecognized_data* entity_unrec = nullptr;
            const auto unrec_it = unrecognized_data.entities.find(static_cast<uint64_t>(local_id));
            if (unrec_it != unrecognized_data.entities.end())
            {
                entity_unrec = &unrec_it->second;
            }

            struct component_to_serialize
            {
                string_view name;
                const ecs::component_type_info* type_info{nullptr};
                const unknown_component* unrec_comp{nullptr};
            };

            auto comps_to_serialize = vector<component_to_serialize>{};

            const auto comp_types = ecs::raw_registry_access::get_component_types(registry, entity);
            for (const auto& info : comp_types)
            {
                if (!info.is_serializable || !info.is_authorable || !info.descriptor.has_value())
                {
                    continue;
                }
                if (prefab_comp != nullptr && !detail::is_transform_component(info.name))
                {
                    continue;
                }
                comps_to_serialize.push_back({
                    .name = info.name,
                    .type_info = &info,
                    .unrec_comp = nullptr,
                });
            }

            if (entity_unrec != nullptr && prefab_comp == nullptr)
            {
                for (const auto& unrec_comp : entity_unrec->unknown_components)
                {
                    comps_to_serialize.push_back({
                        .name = unrec_comp.name,
                        .type_info = nullptr,
                        .unrec_comp = &unrec_comp,
                    });
                }
            }

            tempest::sort(comps_to_serialize.begin(), comps_to_serialize.end(),
                          [](const auto& lhs, const auto& rhs) -> bool {
                              return lhs.name < rhs.name;
                          });

            for (const auto& comp_entry : comps_to_serialize)
            {
                if (comp_entry.type_info != nullptr)
                {
                    const auto& info = *comp_entry.type_info;
                    const auto& desc = *info.descriptor;
                    const auto* comp_ptr = ecs::raw_registry_access::get_component_raw(registry, entity, info.index);
                    if (comp_ptr == nullptr)
                    {
                        continue;
                    }

                    auto comp_obj = comp_map_obj.create_child_object(info.name);

                    for (const auto& field : desc.fields)
                    {
                        if ((field.flags & ecs::field_flags::transient) != ecs::field_flags::none)
                        {
                            continue;
                        }
                        serialize_field(field, comp_ptr, comp_obj, registry);
                    }

                    if (entity_unrec != nullptr)
                    {
                        for (const auto& entry : entity_unrec->unknown_fields)
                        {
                            const auto unrec_sv = string_view{entry.first.data(), entry.first.size()};
                            auto match = (unrec_sv == info.name);
                            if (!match)
                            {
                                if (info.name.size() > unrec_sv.size() + 2 && tempest::ends_with(info.name, unrec_sv))
                                {
                                    const auto prefix_len = info.name.size() - unrec_sv.size();
                                    if (info.name[prefix_len - 1] == ':' && info.name[prefix_len - 2] == ':')
                                    {
                                        match = true;
                                    }
                                }
                                else if (unrec_sv.size() > info.name.size() + 2 &&
                                         tempest::ends_with(unrec_sv, info.name))
                                {
                                    const auto prefix_len = unrec_sv.size() - info.name.size();
                                    if (unrec_sv[prefix_len - 1] == ':' && unrec_sv[prefix_len - 2] == ':')
                                    {
                                        match = true;
                                    }
                                }
                            }
                            if (match)
                            {
                                for (const auto& unrec_field : entry.second)
                                {
                                    comp_obj.set_raw(unrec_field.field_name, unrec_field.json_payload);
                                }
                            }
                        }
                    }
                }
                else if (comp_entry.unrec_comp != nullptr)
                {
                    comp_map_obj.set_raw(comp_entry.unrec_comp->name, comp_entry.unrec_comp->json_payload);
                }
            }

            auto serializable_children = vector<ecs::entity>{};
            auto collect_children = [&](auto& self, ecs::entity curr) -> void {
                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(curr);
                if (rel == nullptr || rel->first_child == ecs::tombstone || rel->first_child == ecs::null ||
                    !registry.is_valid(rel->first_child))
                {
                    return;
                }
                auto child = rel->first_child;
                while (child != ecs::tombstone && child != ecs::null && registry.is_valid(child))
                {
                    const auto* member_comp = registry.try_get<prefab_member_component>(child);
                    const auto* child_scene_comp = registry.try_get<scene_entity_component>(child);
                    const auto child_local_id = child_scene_comp ? child_scene_comp->local_id : entity_local_id{};

                    auto is_instance_added = false;
                    if (prefab_comp != nullptr)
                    {
                        for (const auto added_id : prefab_comp->overrides.added_children)
                        {
                            if (added_id == child_local_id)
                            {
                                is_instance_added = true;
                                break;
                            }
                        }
                    }

                    if (member_comp == nullptr || is_instance_added)
                    {
                        serializable_children.push_back(child);
                    }
                    else if (prefab_comp != nullptr)
                    {
                        self(self, child);
                    }

                    const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                    child = child_rel ? child_rel->next_sibling : ecs::tombstone;
                }
            };
            collect_children(collect_children, entity);

            if (!serializable_children.empty())
            {
                auto children_arr = entity_obj.create_child_array("children");
                for (const auto ser_child : serializable_children)
                {
                    serialize_entity_recursive(registry, ser_child, children_arr, unrecognized_data);
                }
            }
        }
    } // namespace

    auto scene_codec::serialize(ecs::basic_archetype_registry& registry,
                                span<const ecs::entity> root_entities,
                                const scene_unrecognized_data& unrecognized_data,
                                const scene_serialize_options& options) -> string
    {
        for (const auto root : root_entities)
        {
            ensure_entity_local_ids_recursive(registry, root, scene_handle{});
        }

        auto writer = json_writer{};
        auto root_obj = writer.create_object();
        root_obj.set("version", static_cast<int32_t>(1));

        auto entities_arr = root_obj.create_child_array("entities");
        for (const auto root : root_entities)
        {
            const auto* member = registry.try_get<prefab_member_component>(root);
            const auto* instance = registry.try_get<prefab_instance_component>(root);
            if (member != nullptr && instance == nullptr)
            {
                continue;
            }
            serialize_entity_recursive(registry, root, entities_arr, unrecognized_data);
        }

        writer.set_root(root_obj);
        return writer.to_string(options.format);
    }

    auto scene_codec::deserialize(ecs::basic_archetype_registry& registry,
                                  scene_handle scene,
                                  string_view json_content,
                                  const prefab_cache* cache) -> scene_deserialize_result
    {
        auto alloc = system_allocator{};
        auto doc_res = json_document::from_string(json_content, alloc);
        if (!doc_res.has_value())
        {
            return scene_deserialize_result{
                .root_entities = {},
                .all_entities = {},
                .unrecognized_data = {},
                .success = false,
                .error_message = "Failed to parse JSON scene document",
            };
        }

        const auto root_val = doc_res->root();
        if (!root_val.is_object())
        {
            return scene_deserialize_result{
                .root_entities = {},
                .all_entities = {},
                .unrecognized_data = {},
                .success = false,
                .error_message = "JSON scene root must be an object",
            };
        }

        const auto root_obj_res = root_val.as_object();
        if (!root_obj_res.has_value())
        {
            return scene_deserialize_result{
                .root_entities = {},
                .all_entities = {},
                .unrecognized_data = {},
                .success = false,
                .error_message = "Failed to access root object",
            };
        }
        const auto root_obj = *root_obj_res;

        const auto entities_val = root_obj["entities"];
        if (!entities_val.is_array())
        {
            return scene_deserialize_result{
                .root_entities = {},
                .all_entities = {},
                .unrecognized_data = {},
                .success = false,
                .error_message = "Scene document missing 'entities' array",
            };
        }

        const auto entities_arr_res = entities_val.as_array();
        if (!entities_arr_res.has_value())
        {
            return scene_deserialize_result{
                .root_entities = {},
                .all_entities = {},
                .unrecognized_data = {},
                .success = false,
                .error_message = "Failed to access 'entities' array",
            };
        }

        auto result = scene_deserialize_result{};
        auto local_to_entity = flat_unordered_map<uint64_t, ecs::entity>{};

        auto deserialize_components_onto = [&](ecs::entity target_ent, entity_local_id ent_local_id,
                                               json_value comp_map_val, bool only_transform = false) {
            if (!comp_map_val.is_object())
            {
                return;
            }
            const auto comp_map_res = comp_map_val.as_object();
            if (!comp_map_res.has_value())
            {
                return;
            }
            for (const auto& member : *comp_map_res)
            {
                const auto comp_name = member.key;
                const auto comp_val = member.value;

                if (only_transform && !detail::is_transform_component(comp_name))
                {
                    continue;
                }

                const auto* type_info = find_component_type_info(registry.type_registry(), comp_name);
                if (only_transform && type_info != nullptr && !detail::is_transform_component(type_info->name))
                {
                    continue;
                }

                if (type_info != nullptr)
                {
                    auto* existing =
                        ecs::raw_registry_access::get_component_raw(registry, target_ent, type_info->index);

                    auto staged = staging_buffer{type_info->size, type_info->alignment};
                    if (existing != nullptr)
                    {
                        tempest::memcpy(staged.data, existing, type_info->size);
                    }
                    else
                    {
                        if (type_info->construct != nullptr)
                        {
                            type_info->construct(staged.data);
                        }
                        else
                        {
                            tempest::memset(staged.data, 0, type_info->size);
                        }

                        if (type_info->descriptor.has_value())
                        {
                            for (const auto& field : type_info->descriptor->fields)
                            {
                                if (field.kind == ecs::field_kind::entity_ref)
                                {
                                    detail::set_field_val(field, staged.data, ecs::tombstone);
                                }
                            }
                        }
                    }

                    if (type_info->descriptor.has_value() && comp_val.is_object())
                    {
                        const auto& desc = *type_info->descriptor;
                        const auto fields_obj_res = comp_val.as_object();
                        if (fields_obj_res.has_value())
                        {
                            for (const auto& field_member : *fields_obj_res)
                            {
                                const auto field_name = field_member.key;
                                const auto field_v = field_member.value;

                                const auto* field_desc = desc.find_field(field_name);
                                if (field_desc != nullptr)
                                {
                                    deserialize_field(*field_desc, field_v, staged.data);
                                }
                                else
                                {
                                    const auto field_json = json_writer::to_string(field_v);
                                    result.unrecognized_data.entities[ent_local_id.value]
                                        .unknown_fields[string{comp_name}]
                                        .push_back({
                                            .field_name = string{field_name},
                                            .json_payload = field_json,
                                        });
                                }
                            }
                        }
                    }

                    ecs::raw_registry_access::assign_raw(registry, target_ent, *type_info, staged.data);
                }
                else
                {
                    const auto comp_json = json_writer::to_string(comp_val);
                    result.unrecognized_data.entities[ent_local_id.value].unknown_components.push_back({
                        .name = string{comp_name},
                        .json_payload = comp_json,
                    });
                }
            }

            auto* const hist = registry.try_get<ecs::transform_history_component>(target_ent);
            const auto* const tx = registry.try_get<ecs::transform_component>(target_ent);
            if (hist != nullptr && tx != nullptr)
            {
                *hist = ecs::transform_history_component::create(tx->position(), tx->rotation());
            }
        };

        auto deserialize_entity = [&](auto& self, json_value entity_val, ecs::entity parent_entity) -> void {
            if (!entity_val.is_object())
            {
                return;
            }

            const auto id_val = entity_val["id"];
            auto local_id = entity_local_id{};
            if (id_val.is_string())
            {
                const auto parsed = parse_local_id(id_val.as_string().value());
                if (parsed.has_value())
                {
                    local_id = *parsed;
                }
            }
            if (!local_id.is_valid())
            {
                local_id = generate_random_local_id();
            }

            const auto name_val = entity_val["name"];
            const auto prefab_val = entity_val["prefab"];

            if (prefab_val.is_object())
            {
                const auto prefab_obj_res = prefab_val.as_object();
                if (prefab_obj_res.has_value())
                {
                    const auto prefab_obj = *prefab_obj_res;
                    auto prefab_guid = guid{};
                    const auto guid_val = prefab_obj["guid"];
                    if (guid_val.is_string())
                    {
                        const auto parsed = parse_guid(guid_val.as_string().value());
                        if (parsed.has_value())
                        {
                            prefab_guid = *parsed;
                        }
                    }

                    auto instance_id = local_id;
                    const auto instance_id_val = prefab_obj["instance_id"];
                    if (instance_id_val.is_string())
                    {
                        const auto parsed = parse_local_id(instance_id_val.as_string().value());
                        if (parsed.has_value() && parsed->is_valid())
                        {
                            instance_id = *parsed;
                        }
                    }

                    auto overrides = prefab_overrides{};

                    const auto overrides_val = prefab_obj["overrides"];
                    if (overrides_val.is_array())
                    {
                        const auto ov_arr_res = overrides_val.as_array();
                        if (ov_arr_res.has_value())
                        {
                            for (const auto& ov_item : *ov_arr_res)
                            {
                                if (!ov_item.is_object())
                                {
                                    continue;
                                }
                                const auto ov_item_res = ov_item.as_object();
                                if (!ov_item_res.has_value())
                                {
                                    continue;
                                }
                                const auto ov_o = *ov_item_res;

                                auto target_path = id_path{};
                                const auto target_v = ov_o["target"];
                                const auto parsed_path = parse_id_path(target_v);
                                if (parsed_path.has_value())
                                {
                                    target_path = *parsed_path;
                                }

                                auto comp_name = string{};
                                const auto c_val = ov_o["component"];
                                if (c_val.is_string())
                                {
                                    comp_name = string{c_val.as_string().value()};
                                }

                                auto f_name = string{};
                                const auto f_val = ov_o["field"];
                                if (f_val.is_string())
                                {
                                    f_name = string{f_val.as_string().value()};
                                }

                                const auto val_v = ov_o["value"];
                                auto val_bytes = vector<byte>{};

                                const auto* type_info =
                                    find_component_type_info(registry.type_registry(), comp_name);
                                if (type_info != nullptr && type_info->descriptor.has_value())
                                {
                                    const auto* field_desc = type_info->descriptor->find_field(f_name);
                                    if (field_desc != nullptr)
                                    {
                                        val_bytes.resize(field_desc->size, byte{0});
                                        deserialize_raw_field_value(*field_desc, val_v, val_bytes.data());
                                    }
                                }

                                overrides.field_overrides.push_back(field_override{
                                    .target = tempest::move(target_path),
                                    .component_name = tempest::move(comp_name),
                                    .field_name = tempest::move(f_name),
                                    .value_bytes = tempest::move(val_bytes),
                                });
                            }
                        }
                    }

                    const auto rem_val = prefab_obj["removed_components"];
                    if (rem_val.is_array())
                    {
                        const auto rem_arr_res = rem_val.as_array();
                        if (rem_arr_res.has_value())
                        {
                            for (const auto& rem_item : *rem_arr_res)
                            {
                                if (!rem_item.is_object())
                                {
                                    continue;
                                }
                                const auto rem_item_res = rem_item.as_object();
                                if (!rem_item_res.has_value())
                                {
                                    continue;
                                }
                                const auto rem_o = *rem_item_res;

                                auto target_path = id_path{};
                                const auto target_v = rem_o["target"];
                                const auto parsed_path = parse_id_path(target_v);
                                if (parsed_path.has_value())
                                {
                                    target_path = *parsed_path;
                                }

                                auto comp_name = string{};
                                const auto c_val = rem_o["component"];
                                if (c_val.is_string())
                                {
                                    comp_name = string{c_val.as_string().value()};
                                }

                                overrides.removed_components.push_back(removed_component{
                                    .target = tempest::move(target_path),
                                    .component_name = tempest::move(comp_name),
                                });
                            }
                        }
                    }

                    const auto add_val = prefab_obj["added_components"];
                    if (add_val.is_array())
                    {
                        const auto add_arr_res = add_val.as_array();
                        if (add_arr_res.has_value())
                        {
                            for (const auto& add_item : *add_arr_res)
                            {
                                if (!add_item.is_object())
                                {
                                    continue;
                                }
                                const auto add_item_res = add_item.as_object();
                                if (!add_item_res.has_value())
                                {
                                    continue;
                                }
                                const auto add_o = *add_item_res;

                                auto target_path = id_path{};
                                const auto target_v = add_o["target"];
                                const auto parsed_path = parse_id_path(target_v);
                                if (parsed_path.has_value())
                                {
                                    target_path = *parsed_path;
                                }

                                auto comp_name = string{};
                                const auto c_val = add_o["component"];
                                if (c_val.is_string())
                                {
                                    comp_name = string{c_val.as_string().value()};
                                }

                                auto staged = staged_component{};
                                const auto* type_info =
                                    find_component_type_info(registry.type_registry(), comp_name);
                                if (type_info != nullptr && type_info->descriptor.has_value())
                                {
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
                                    const auto props_val = add_o["properties"];
                                    if (props_val.is_object())
                                    {
                                        const auto props_obj = *props_val.as_object();
                                        for (const auto& member : props_obj)
                                        {
                                            const auto* field_desc = type_info->descriptor->find_field(member.key);
                                            if (field_desc != nullptr)
                                            {
                                                deserialize_field(*field_desc, member.value, staged.data.data());
                                            }
                                        }
                                    }
                                }

                                overrides.added_components.push_back(added_component{
                                    .target = tempest::move(target_path),
                                    .component = tempest::move(staged),
                                });
                            }
                        }
                    }

                    const auto child_ids_val = prefab_obj["added_children"];
                    if (child_ids_val.is_array())
                    {
                        const auto arr_res = child_ids_val.as_array();
                        if (arr_res.has_value())
                        {
                            for (const auto& id_item : *arr_res)
                            {
                                if (id_item.is_string())
                                {
                                    const auto parsed = parse_local_id(id_item.as_string().value());
                                    if (parsed.has_value() && parsed->is_valid())
                                    {
                                        overrides.added_children.push_back(*parsed);
                                    }
                                }
                            }
                        }
                    }

                    if (cache != nullptr && cache->has_template(prefab_guid))
                    {
                        auto inst_res = prefab_instantiator::instantiate(prefab_guid, *cache, registry, overrides,
                                                                         instance_id);
                        if (inst_res.has_value())
                        {
                            const auto root_ent = inst_res->root_entity;
                            if (name_val.is_string())
                            {
                                registry.name(root_ent, name_val.as_string().value());
                            }

                            registry.assign_or_replace(root_ent, scene_entity_component{
                                                                     .scene = scene,
                                                                     .local_id = local_id,
                                                                 });

                            for (const auto ent : inst_res->all_entities)
                            {
                                if (ent != root_ent)
                                {
                                    if (!registry.has<scene_entity_component>(ent))
                                    {
                                        const auto* member = registry.try_get<prefab_member_component>(ent);
                                        const auto derived_val =
                                            (member && member->template_local_id.is_valid())
                                                ? static_cast<uint64_t>(tempest::hash_combine(
                                                      local_id.value, member->template_local_id.value))
                                                : generate_random_local_id().value;
                                        const auto member_local_id =
                                            entity_local_id{derived_val == 0 ? 1 : derived_val};
                                        registry.assign(ent, scene_entity_component{
                                                                 .scene = scene,
                                                                 .local_id = member_local_id,
                                                             });
                                        local_to_entity[member_local_id.value] = ent;
                                    }
                                    else
                                    {
                                        const auto* sc = registry.try_get<scene_entity_component>(ent);
                                        if (sc && sc->local_id.is_valid())
                                        {
                                            local_to_entity[sc->local_id.value] = ent;
                                        }
                                    }
                                }
                                result.all_entities.push_back(ent);
                            }

                            local_to_entity[local_id.value] = root_ent;

                            if (parent_entity != ecs::tombstone)
                            {
                                registry.reparent(root_ent, parent_entity);
                            }
                            else
                            {
                                result.root_entities.push_back(root_ent);
                            }

                            deserialize_components_onto(root_ent, local_id, entity_val["components"],
                                                        /*only_transform=*/true);

                            const auto children_val = entity_val["children"];
                            if (children_val.is_array())
                            {
                                const auto children_arr_res = children_val.as_array();
                                if (children_arr_res.has_value())
                                {
                                    for (const auto& child_item : *children_arr_res)
                                    {
                                        self(self, child_item, root_ent);
                                    }
                                }
                            }
                            return;
                        }
                    }

                    // Fallback when cache == nullptr or template not found
                    const auto entity = registry.create();
                    registry.assign(entity, scene_entity_component{
                                                .scene = scene,
                                                .local_id = local_id,
                                            });
                    registry.assign(entity, prefab_instance_component{
                                                .prefab_guid = prefab_guid,
                                                .instance_local_id = instance_id,
                                                .overrides = tempest::move(overrides),
                                            });
                    if (name_val.is_string())
                    {
                        registry.name(entity, name_val.as_string().value());
                    }

                    local_to_entity[local_id.value] = entity;
                    result.all_entities.push_back(entity);

                    if (parent_entity != ecs::tombstone)
                    {
                        registry.reparent(entity, parent_entity);
                    }
                    else
                    {
                        result.root_entities.push_back(entity);
                    }

                    deserialize_components_onto(entity, local_id, entity_val["components"]);

                    const auto children_val = entity_val["children"];
                    if (children_val.is_array())
                    {
                        const auto children_arr_res = children_val.as_array();
                        if (children_arr_res.has_value())
                        {
                            for (const auto& child_item : *children_arr_res)
                            {
                                self(self, child_item, entity);
                            }
                        }
                    }
                    return;
                }
            }

            // Normal entity without prefab block
            const auto entity = registry.create();
            registry.assign(entity, scene_entity_component{
                                        .scene = scene,
                                        .local_id = local_id,
                                    });

            if (name_val.is_string())
            {
                registry.name(entity, name_val.as_string().value());
            }

            local_to_entity[local_id.value] = entity;
            result.all_entities.push_back(entity);

            if (parent_entity != ecs::tombstone)
            {
                registry.reparent(entity, parent_entity);
            }
            else
            {
                result.root_entities.push_back(entity);
            }

            deserialize_components_onto(entity, local_id, entity_val["components"]);

            const auto children_val = entity_val["children"];
            if (children_val.is_array())
            {
                const auto children_arr_res = children_val.as_array();
                if (children_arr_res.has_value())
                {
                    for (const auto& child_item : *children_arr_res)
                    {
                        self(self, child_item, entity);
                    }
                }
            }
        };

        for (const auto& entity_item : *entities_arr_res)
        {
            deserialize_entity(deserialize_entity, entity_item, ecs::tombstone);
        }

        for (const auto entity : result.all_entities)
        {
            const auto comp_types = ecs::raw_registry_access::get_component_types(registry, entity);
            for (const auto& info : comp_types)
            {
                if (!info.descriptor.has_value())
                {
                    continue;
                }
                const auto& desc = *info.descriptor;
                auto* comp_ptr = ecs::raw_registry_access::get_component_raw(registry, entity, info.index);
                if (comp_ptr == nullptr)
                {
                    continue;
                }

                for (const auto& field : desc.fields)
                {
                    if (field.kind == ecs::field_kind::entity_ref)
                    {
                        const auto raw_ref = get_field_val<ecs::entity>(field, comp_ptr);
                        const auto target_local_id = static_cast<uint64_t>(raw_ref);
                        if (target_local_id != 0 && target_local_id != static_cast<uint64_t>(ecs::tombstone))
                        {
                            const auto it = local_to_entity.find(target_local_id);
                            if (it != local_to_entity.end())
                            {
                                set_field_val(field, comp_ptr, it->second);
                            }
                            else
                            {
                                set_field_val(field, comp_ptr, ecs::tombstone);
                            }
                        }
                        else
                        {
                            set_field_val(field, comp_ptr, ecs::tombstone);
                        }
                    }
                }

                if (desc.post_load != nullptr)
                {
                    desc.post_load(comp_ptr);
                }
            }
        }

        result.success = true;
        return result;
    }
} // namespace tempest::scene
