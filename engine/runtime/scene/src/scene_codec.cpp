#include <tempest/scene/scene_codec.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/random.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::scene
{
    namespace
    {
        auto generate_random_local_id() -> entity_local_id
        {
            auto device = random_device{};
            auto val = (static_cast<uint64_t>(device()) << 32) | static_cast<uint64_t>(device());
            auto local_id = entity_local_id{val};
            while (!local_id.is_valid())
            {
                val = (static_cast<uint64_t>(device()) << 32) | static_cast<uint64_t>(device());
                local_id = entity_local_id{val};
            }
            return local_id;
        }

        template <typename T>
        requires is_trivially_copyable_v<T>
        auto get_field_val(const ecs::field_descriptor& field, const void* component_ptr) -> T
        {
            alignas(alignof(T)) byte storage[sizeof(T)];
            if (field.get != nullptr)
            {
                field.get(component_ptr, storage);
            }
            else if (field.offset != ecs::field_descriptor::no_offset)
            {
                tempest::memcpy(storage, static_cast<const byte*>(component_ptr) + field.offset, sizeof(T));
            }
            auto result = T{};
            tempest::memcpy(&result, storage, sizeof(T));
            return result;
        }

        template <typename T>
        requires is_trivially_copyable_v<T>
        void set_field_val(const ecs::field_descriptor& field, void* component_ptr, const T& value)
        {
            if (field.set != nullptr)
            {
                field.set(component_ptr, &value);
            }
            else if (field.offset != ecs::field_descriptor::no_offset)
            {
                tempest::memcpy(static_cast<byte*>(component_ptr) + field.offset, &value, sizeof(T));
            }
        }

        void serialize_field(const ecs::field_descriptor& field, const void* comp_ptr,
                             json_object_mut& target_obj, const ecs::basic_archetype_registry& registry)
        {
            using ecs::field_kind;

            switch (field.kind)
            {
                case field_kind::boolean:
                {
                    const auto value = get_field_val<bool>(field, comp_ptr);
                    target_obj.set(field.name, value);
                    break;
                }
                case field_kind::int32:
                {
                    const auto value = get_field_val<int32_t>(field, comp_ptr);
                    target_obj.set(field.name, value);
                    break;
                }
                case field_kind::uint32:
                {
                    const auto value = get_field_val<uint32_t>(field, comp_ptr);
                    target_obj.set(field.name, value);
                    break;
                }
                case field_kind::int64:
                {
                    const auto value = get_field_val<int64_t>(field, comp_ptr);
                    target_obj.set(field.name, value);
                    break;
                }
                case field_kind::uint64:
                {
                    const auto value = get_field_val<uint64_t>(field, comp_ptr);
                    target_obj.set(field.name, value);
                    break;
                }
                case field_kind::float32:
                {
                    const auto value = get_field_val<float>(field, comp_ptr);
                    target_obj.set(field.name, value);
                    break;
                }
                case field_kind::float64:
                {
                    const auto value = get_field_val<double>(field, comp_ptr);
                    target_obj.set(field.name, value);
                    break;
                }
                case field_kind::vec2:
                {
                    const auto value = get_field_val<math::vec2<float>>(field, comp_ptr);
                    auto arr = target_obj.create_child_array(field.name);
                    arr.push_back(value.x);
                    arr.push_back(value.y);
                    break;
                }
                case field_kind::vec3:
                case field_kind::color3:
                {
                    const auto value = get_field_val<math::vec3<float>>(field, comp_ptr);
                    auto arr = target_obj.create_child_array(field.name);
                    arr.push_back(value.x);
                    arr.push_back(value.y);
                    arr.push_back(value.z);
                    break;
                }
                case field_kind::vec4:
                case field_kind::color4:
                {
                    const auto value = get_field_val<math::vec4<float>>(field, comp_ptr);
                    auto arr = target_obj.create_child_array(field.name);
                    arr.push_back(value.x);
                    arr.push_back(value.y);
                    arr.push_back(value.z);
                    arr.push_back(value.w);
                    break;
                }
                case field_kind::quat:
                {
                    const auto value = get_field_val<math::quat<float>>(field, comp_ptr);
                    auto arr = target_obj.create_child_array(field.name);
                    arr.push_back(value.x);
                    arr.push_back(value.y);
                    arr.push_back(value.z);
                    arr.push_back(value.w);
                    break;
                }
                case field_kind::guid:
                {
                    const auto value = get_field_val<guid>(field, comp_ptr);
                    target_obj.set(field.name, tempest::to_string(value));
                    break;
                }
                case field_kind::enumeration:
                {
                    auto enum_val = int64_t{0};
                    if (field.size == 1)
                    {
                        enum_val = static_cast<int64_t>(get_field_val<int8_t>(field, comp_ptr));
                    }
                    else if (field.size == 2)
                    {
                        enum_val = static_cast<int64_t>(get_field_val<int16_t>(field, comp_ptr));
                    }
                    else if (field.size == 4)
                    {
                        enum_val = static_cast<int64_t>(get_field_val<int32_t>(field, comp_ptr));
                    }
                    else
                    {
                        enum_val = get_field_val<int64_t>(field, comp_ptr);
                    }

                    auto found_name = false;
                    for (const auto& entry : field.enum_entries)
                    {
                        if (entry.value == enum_val)
                        {
                            target_obj.set(field.name, entry.name);
                            found_name = true;
                            break;
                        }
                    }
                    if (!found_name)
                    {
                        target_obj.set(field.name, enum_val);
                    }
                    break;
                }
                case field_kind::entity_ref:
                {
                    const auto ref_entity = get_field_val<ecs::entity>(field, comp_ptr);
                    if (ref_entity == ecs::tombstone || ref_entity == ecs::null || !registry.is_valid(ref_entity))
                    {
                        target_obj.set_null(field.name);
                    }
                    else
                    {
                        const auto* scene_comp = registry.try_get<scene_entity_component>(ref_entity);
                        if (scene_comp != nullptr && scene_comp->local_id.is_valid())
                        {
                            target_obj.set(field.name, to_hex_string(scene_comp->local_id));
                        }
                        else
                        {
                            target_obj.set_null(field.name);
                        }
                    }
                    break;
                }
                case field_kind::asset_ref:
                {
                    if (field.size == sizeof(guid))
                    {
                        const auto value = get_field_val<guid>(field, comp_ptr);
                        target_obj.set(field.name, tempest::to_string(value));
                    }
                    else
                    {
                        const auto value = get_field_val<uint64_t>(field, comp_ptr);
                        target_obj.set(field.name, value);
                    }
                    break;
                }
                case field_kind::fixed_string:
                {
                    const auto* str_ptr = static_cast<const char*>(comp_ptr) + field.offset;
                    target_obj.set(field.name, string_view{str_ptr});
                    break;
                }
            }
        }

        void deserialize_field(const ecs::field_descriptor& field, json_value field_val, void* comp_ptr)
        {
            using ecs::field_kind;

            switch (field.kind)
            {
                case field_kind::boolean:
                {
                    if (field_val.is_bool())
                    {
                        set_field_val(field, comp_ptr, field_val.as_bool().value());
                    }
                    break;
                }
                case field_kind::int32:
                {
                    if (field_val.is_int() || field_val.is_number())
                    {
                        set_field_val(field, comp_ptr, static_cast<int32_t>(field_val.as_int64().value()));
                    }
                    break;
                }
                case field_kind::uint32:
                {
                    if (field_val.is_uint() || field_val.is_number())
                    {
                        set_field_val(field, comp_ptr, static_cast<uint32_t>(field_val.as_uint64().value()));
                    }
                    break;
                }
                case field_kind::int64:
                {
                    if (field_val.is_int() || field_val.is_number())
                    {
                        set_field_val(field, comp_ptr, field_val.as_int64().value());
                    }
                    break;
                }
                case field_kind::uint64:
                {
                    if (field_val.is_uint() || field_val.is_number())
                    {
                        set_field_val(field, comp_ptr, field_val.as_uint64().value());
                    }
                    break;
                }
                case field_kind::float32:
                {
                    if (field_val.is_real() || field_val.is_number())
                    {
                        set_field_val(field, comp_ptr, field_val.as_float().value());
                    }
                    break;
                }
                case field_kind::float64:
                {
                    if (field_val.is_real() || field_val.is_number())
                    {
                        set_field_val(field, comp_ptr, field_val.as_double().value());
                    }
                    break;
                }
                case field_kind::vec2:
                {
                    if (field_val.is_array())
                    {
                        const auto arr = field_val.as_array().value();
                        if (arr.size() >= 2)
                        {
                            const auto val = math::vec2<float>{
                                arr[0].as_float().value(),
                                arr[1].as_float().value(),
                            };
                            set_field_val(field, comp_ptr, val);
                        }
                    }
                    break;
                }
                case field_kind::vec3:
                case field_kind::color3:
                {
                    if (field_val.is_array())
                    {
                        const auto arr = field_val.as_array().value();
                        if (arr.size() >= 3)
                        {
                            const auto val = math::vec3<float>{
                                arr[0].as_float().value(),
                                arr[1].as_float().value(),
                                arr[2].as_float().value(),
                            };
                            set_field_val(field, comp_ptr, val);
                        }
                    }
                    break;
                }
                case field_kind::vec4:
                case field_kind::color4:
                {
                    if (field_val.is_array())
                    {
                        const auto arr = field_val.as_array().value();
                        if (arr.size() >= 4)
                        {
                            const auto val = math::vec4<float>{
                                arr[0].as_float().value(),
                                arr[1].as_float().value(),
                                arr[2].as_float().value(),
                                arr[3].as_float().value(),
                            };
                            set_field_val(field, comp_ptr, val);
                        }
                    }
                    break;
                }
                case field_kind::quat:
                {
                    if (field_val.is_array())
                    {
                        const auto arr = field_val.as_array().value();
                        if (arr.size() >= 4)
                        {
                            const auto val = math::quat<float>{
                                arr[0].as_float().value(),
                                arr[1].as_float().value(),
                                arr[2].as_float().value(),
                                arr[3].as_float().value(),
                            };
                            set_field_val(field, comp_ptr, val);
                        }
                    }
                    break;
                }
                case field_kind::guid:
                {
                    if (field_val.is_string())
                    {
                        const auto parsed = parse_guid(field_val.as_string().value());
                        if (parsed.has_value())
                        {
                            set_field_val(field, comp_ptr, *parsed);
                        }
                    }
                    break;
                }
                case field_kind::enumeration:
                {
                    auto enum_val = int64_t{0};
                    auto valid = false;
                    if (field_val.is_string())
                    {
                        const auto str = field_val.as_string().value();
                        for (const auto& entry : field.enum_entries)
                        {
                            if (entry.name == str)
                            {
                                enum_val = entry.value;
                                valid = true;
                                break;
                            }
                        }
                    }
                    else if (field_val.is_int() || field_val.is_number())
                    {
                        enum_val = field_val.as_int64().value();
                        valid = true;
                    }

                    if (valid)
                    {
                        if (field.size == 1)
                        {
                            const auto val8 = static_cast<int8_t>(enum_val);
                            set_field_val(field, comp_ptr, val8);
                        }
                        else if (field.size == 2)
                        {
                            const auto val16 = static_cast<int16_t>(enum_val);
                            set_field_val(field, comp_ptr, val16);
                        }
                        else if (field.size == 4)
                        {
                            const auto val32 = static_cast<int32_t>(enum_val);
                            set_field_val(field, comp_ptr, val32);
                        }
                        else
                        {
                            set_field_val(field, comp_ptr, enum_val);
                        }
                    }
                    break;
                }
                case field_kind::entity_ref:
                {
                    if (field_val.is_string())
                    {
                        const auto parsed = parse_hex_string(field_val.as_string().value());
                        if (parsed.has_value())
                        {
                            set_field_val(field, comp_ptr, static_cast<ecs::entity>(*parsed));
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
                    break;
                }
                case field_kind::asset_ref:
                {
                    if (field.size == sizeof(guid) && field_val.is_string())
                    {
                        const auto parsed = parse_guid(field_val.as_string().value());
                        if (parsed.has_value())
                        {
                            set_field_val(field, comp_ptr, *parsed);
                        }
                    }
                    else if (field_val.is_uint() || field_val.is_number())
                    {
                        const auto val = field_val.as_uint64().value();
                        set_field_val(field, comp_ptr, val);
                    }
                    break;
                }
                case field_kind::fixed_string:
                {
                    if (field_val.is_string())
                    {
                        const auto str = field_val.as_string().value();
                        auto* dst = static_cast<char*>(comp_ptr) + field.offset;
                        const auto max_len = field.size > 0 ? field.size - 1 : 0;
                        const auto copy_len = str.size() < max_len ? str.size() : max_len;
                        tempest::memcpy(dst, str.data(), copy_len);
                        dst[copy_len] = '\0';
                    }
                    break;
                }
            }
        }

        const ecs::component_type_info* find_component_type_info(const ecs::component_type_registry& type_reg,
                                                                 string_view comp_name)
        {
            const auto* info = type_reg.type_info(comp_name);
            if (info != nullptr)
            {
                return info;
            }

            for (size_t i = 0; i < type_reg.size(); ++i)
            {
                const auto& cand = type_reg.type_info(static_cast<uint32_t>(i));
                const auto cand_name = cand.name;
                if (cand_name == comp_name)
                {
                    return &cand;
                }
                if (cand_name.size() > comp_name.size() + 2)
                {
                    if (tempest::ends_with(cand_name, comp_name))
                    {
                        const auto prefix_len = cand_name.size() - comp_name.size();
                        if (cand_name[prefix_len - 1] == ':' && cand_name[prefix_len - 2] == ':')
                        {
                            return &cand;
                        }
                    }
                }
            }
            return nullptr;
        }

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

            auto comp_map_obj = entity_obj.create_child_object("components");

            const entity_unrecognized_data* entity_unrec = nullptr;
            const auto unrec_it = unrecognized_data.entities.find(static_cast<uint64_t>(local_id));
            if (unrec_it != unrecognized_data.entities.end())
            {
                entity_unrec = &unrec_it->second;
            }

            struct component_to_serialize
            {
                string name;
                const ecs::component_type_info* type_info{nullptr};
                const unknown_component* unrec_comp{nullptr};
            };

            auto comps_to_serialize = vector<component_to_serialize>{};

            const auto comp_types = ecs::raw_registry_access::get_component_types(registry, entity);
            for (const auto& info : comp_types)
            {
                if (!info.is_serializable || !info.descriptor.has_value())
                {
                    continue;
                }
                comps_to_serialize.push_back({
                    .name = string{info.name},
                    .type_info = &info,
                    .unrec_comp = nullptr,
                });
            }

            if (entity_unrec != nullptr)
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
                                else if (unrec_sv.size() > info.name.size() + 2 && tempest::ends_with(unrec_sv, info.name))
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

            const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(entity);
            if (rel != nullptr && rel->first_child != ecs::tombstone && rel->first_child != ecs::null &&
                registry.is_valid(rel->first_child))
            {
                auto children_arr = entity_obj.create_child_array("children");
                auto child = rel->first_child;
                while (child != ecs::tombstone && child != ecs::null && registry.is_valid(child))
                {
                    serialize_entity_recursive(registry, child, children_arr, unrecognized_data);
                    const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                    child = child_rel ? child_rel->next_sibling : ecs::tombstone;
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
            serialize_entity_recursive(registry, root, entities_arr, unrecognized_data);
        }

        writer.set_root(root_obj);
        return writer.to_string(options.format);
    }

    auto scene_codec::deserialize(ecs::basic_archetype_registry& registry,
                                  scene_handle scene,
                                  string_view json_content) -> scene_deserialize_result
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

            const auto entity = registry.create();
            registry.assign(entity, scene_entity_component{
                                        .scene = scene,
                                        .local_id = local_id,
                                    });

            const auto name_val = entity_val["name"];
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

            const auto comp_map_val = entity_val["components"];
            if (comp_map_val.is_object())
            {
                const auto comp_map_res = comp_map_val.as_object();
                if (comp_map_res.has_value())
                {
                    for (const auto& member : *comp_map_res)
                    {
                        const auto comp_name = member.key;
                        const auto comp_val = member.value;

                        const auto* type_info = find_component_type_info(registry.type_registry(), comp_name);
                        if (type_info != nullptr && type_info->descriptor.has_value())
                        {
                            auto* comp_ptr = ecs::raw_registry_access::assign_raw(registry, entity, *type_info);
                            const auto& desc = *type_info->descriptor;

                            if (comp_val.is_object())
                            {
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
                                            deserialize_field(*field_desc, field_v, comp_ptr);
                                        }
                                        else
                                        {
                                            const auto field_json = json_writer::to_string(field_v);
                                            result.unrecognized_data.entities[local_id.value]
                                                .unknown_fields[string{comp_name}]
                                                .push_back({
                                                    .field_name = string{field_name},
                                                    .json_payload = field_json,
                                                });
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            const auto comp_json = json_writer::to_string(comp_val);
                            result.unrecognized_data.entities[local_id.value].unknown_components.push_back({
                                .name = string{comp_name},
                                .json_payload = comp_json,
                            });
                        }
                    }
                }
            }

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
