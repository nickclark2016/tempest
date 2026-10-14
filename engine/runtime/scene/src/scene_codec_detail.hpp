#ifndef tempest_scene_scene_codec_detail_hpp
#define tempest_scene_scene_codec_detail_hpp

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/atomic.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/json.hpp>
#include <tempest/math.hpp>
#include <tempest/memory.hpp>
#include <tempest/random.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest::scene::detail
{
    inline constexpr auto splitmix64(uint64_t x) noexcept -> uint64_t
    {
        auto z = x + 0x9e3779b97f4a7c15ULL;
        z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31U);
    }

    inline auto generate_random_local_id() -> entity_local_id
    {
        static atomic<uint64_t> sequence{0x9e3779b97f4a7c15ULL};
        auto val = splitmix64(sequence.fetch_add(0x9e3779b97f4a7c15ULL, memory_order::relaxed));
        while (val == 0)
        {
            val = splitmix64(sequence.fetch_add(0x9e3779b97f4a7c15ULL, memory_order::relaxed));
        }
        return entity_local_id{val};
    }

    template <typename T>
    requires is_trivially_copyable_v<T>
    auto get_field_val(const ecs::field_descriptor& field, const void* component_ptr) -> T
    {
        alignas(alignof(T)) byte storage[sizeof(T)] = {};
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

    template <typename T>
    requires is_trivially_copyable_v<T>
    auto get_raw_field_val(const void* field_val_ptr) -> T
    {
        auto result = T{};
        tempest::memcpy(&result, field_val_ptr, sizeof(T));
        return result;
    }

    template <typename T>
    requires is_trivially_copyable_v<T>
    void set_raw_field_val(void* field_dst_ptr, const T& value)
    {
        tempest::memcpy(field_dst_ptr, &value, sizeof(T));
    }

    inline auto component_name_matches(string_view registered_name, string_view query_name) noexcept -> bool
    {
        if (registered_name == query_name)
        {
            return true;
        }
        if (registered_name.size() > query_name.size() + 2)
        {
            if (tempest::ends_with(registered_name, query_name))
            {
                const auto prefix_len = registered_name.size() - query_name.size();
                if (registered_name[prefix_len - 1] == ':' && registered_name[prefix_len - 2] == ':')
                {
                    return true;
                }
            }
        }
        if (query_name.size() > registered_name.size() + 2)
        {
            if (tempest::ends_with(query_name, registered_name))
            {
                const auto prefix_len = query_name.size() - registered_name.size();
                if (query_name[prefix_len - 1] == ':' && query_name[prefix_len - 2] == ':')
                {
                    return true;
                }
            }
        }
        return false;
    }

    inline auto find_component_type_info(const ecs::component_type_registry& type_reg, string_view comp_name)
        -> const ecs::component_type_info*
    {
        const auto* exact_info = type_reg.type_info(comp_name);
        if (exact_info != nullptr)
        {
            return exact_info;
        }

        for (auto type_index = size_t{0}; type_index < type_reg.size(); ++type_index)
        {
            const auto& candidate_info = type_reg.type_info(static_cast<uint32_t>(type_index));
            if (component_name_matches(candidate_info.name, comp_name))
            {
                return &candidate_info;
            }
        }
        return nullptr;
    }

    inline auto is_transform_component(string_view name) noexcept -> bool
    {
        if (name == "tempest::ecs::transform_component" || name == "transform_component")
        {
            return true;
        }
        if (name.size() > 19 && tempest::ends_with(name, "transform_component"))
        {
            const auto prefix_len = name.size() - 19;
            if (name[prefix_len - 1] == ':' && name[prefix_len - 2] == ':')
            {
                return true;
            }
        }
        return false;
    }

    inline void serialize_raw_field_value_to_key(const ecs::field_descriptor& field, const void* field_val_ptr,
                                                 json_object_mut& target_obj, string_view key,
                                                 const ecs::basic_archetype_registry* registry)
    {
        using ecs::field_kind;

        switch (field.kind)
        {
            case field_kind::boolean:
            {
                const auto value = get_raw_field_val<bool>(field_val_ptr);
                target_obj.set(key, value);
                break;
            }
            case field_kind::int32:
            {
                const auto value = get_raw_field_val<int32_t>(field_val_ptr);
                target_obj.set(key, value);
                break;
            }
            case field_kind::uint32:
            {
                const auto value = get_raw_field_val<uint32_t>(field_val_ptr);
                target_obj.set(key, value);
                break;
            }
            case field_kind::int64:
            {
                const auto value = get_raw_field_val<int64_t>(field_val_ptr);
                target_obj.set(key, value);
                break;
            }
            case field_kind::uint64:
            {
                const auto value = get_raw_field_val<uint64_t>(field_val_ptr);
                target_obj.set(key, value);
                break;
            }
            case field_kind::float32:
            {
                const auto value = get_raw_field_val<float>(field_val_ptr);
                target_obj.set(key, value);
                break;
            }
            case field_kind::float64:
            {
                const auto value = get_raw_field_val<double>(field_val_ptr);
                target_obj.set(key, value);
                break;
            }
            case field_kind::vec2:
            {
                const auto value = get_raw_field_val<math::vec2<float>>(field_val_ptr);
                auto arr = target_obj.create_child_array(key);
                arr.push_back(value.x);
                arr.push_back(value.y);
                break;
            }
            case field_kind::vec3:
            case field_kind::color3:
            {
                const auto value = get_raw_field_val<math::vec3<float>>(field_val_ptr);
                auto arr = target_obj.create_child_array(key);
                arr.push_back(value.x);
                arr.push_back(value.y);
                arr.push_back(value.z);
                break;
            }
            case field_kind::vec4:
            case field_kind::color4:
            {
                const auto value = get_raw_field_val<math::vec4<float>>(field_val_ptr);
                auto arr = target_obj.create_child_array(key);
                arr.push_back(value.x);
                arr.push_back(value.y);
                arr.push_back(value.z);
                arr.push_back(value.w);
                break;
            }
            case field_kind::quat:
            {
                const auto value = get_raw_field_val<math::quat<float>>(field_val_ptr);
                auto arr = target_obj.create_child_array(key);
                arr.push_back(value.x);
                arr.push_back(value.y);
                arr.push_back(value.z);
                arr.push_back(value.w);
                break;
            }
            case field_kind::guid:
            {
                const auto value = get_raw_field_val<guid>(field_val_ptr);
                target_obj.set(key, tempest::to_string(value));
                break;
            }
            case field_kind::enumeration:
            {
                auto enum_val = int64_t{0};
                if (field.size == 1)
                {
                    enum_val = static_cast<int64_t>(get_raw_field_val<int8_t>(field_val_ptr));
                }
                else if (field.size == 2)
                {
                    enum_val = static_cast<int64_t>(get_raw_field_val<int16_t>(field_val_ptr));
                }
                else if (field.size == 4)
                {
                    enum_val = static_cast<int64_t>(get_raw_field_val<int32_t>(field_val_ptr));
                }
                else
                {
                    enum_val = get_raw_field_val<int64_t>(field_val_ptr);
                }

                auto found_name = false;
                for (const auto& entry : field.enum_entries)
                {
                    if (entry.value == enum_val)
                    {
                        target_obj.set(key, entry.name);
                        found_name = true;
                        break;
                    }
                }
                if (!found_name)
                {
                    target_obj.set(key, enum_val);
                }
                break;
            }
            case field_kind::entity_ref:
            {
                const auto ref_entity = get_raw_field_val<ecs::entity>(field_val_ptr);
                if (registry != nullptr && ref_entity != ecs::tombstone && ref_entity != ecs::null &&
                    registry->is_valid(ref_entity))
                {
                    const auto* scene_comp = registry->try_get<scene_entity_component>(ref_entity);
                    if (scene_comp != nullptr && scene_comp->local_id.is_valid())
                    {
                        target_obj.set(key, to_hex_string(scene_comp->local_id));
                    }
                    else
                    {
                        target_obj.set_null(key);
                    }
                }
                else if (ref_entity != ecs::tombstone && ref_entity != ecs::null &&
                         static_cast<uint64_t>(ref_entity) != 0)
                {
                    target_obj.set(key, to_hex_string(static_cast<uint64_t>(ref_entity)));
                }
                else
                {
                    target_obj.set_null(key);
                }
                break;
            }
            case field_kind::asset_ref:
            {
                if (field.size == sizeof(guid))
                {
                    const auto value = get_raw_field_val<guid>(field_val_ptr);
                    target_obj.set(key, tempest::to_string(value));
                }
                else
                {
                    const auto value = get_raw_field_val<uint64_t>(field_val_ptr);
                    target_obj.set(key, value);
                }
                break;
            }
            case field_kind::fixed_string:
            {
                if (field_val_ptr != nullptr && field.size > 0)
                {
                    const auto* str_ptr = static_cast<const char*>(field_val_ptr);
                    auto len = size_t{0};
                    while (len < field.size && str_ptr[len] != '\0')
                    {
                        ++len;
                    }
                    target_obj.set(key, string_view{str_ptr, len});
                }
                else
                {
                    target_obj.set(key, string_view{});
                }
                break;
            }
        }
    }

    inline void deserialize_raw_field_value(const ecs::field_descriptor& field, json_value field_val,
                                            void* field_dst_ptr)
    {
        using ecs::field_kind;

        switch (field.kind)
        {
            case field_kind::boolean:
            {
                if (field_val.is_bool())
                {
                    set_raw_field_val(field_dst_ptr, field_val.as_bool().value());
                }
                break;
            }
            case field_kind::int32:
            {
                if (field_val.is_int() || field_val.is_number())
                {
                    set_raw_field_val(field_dst_ptr, static_cast<int32_t>(field_val.as_int64().value()));
                }
                break;
            }
            case field_kind::uint32:
            {
                if (field_val.is_uint() || field_val.is_number())
                {
                    set_raw_field_val(field_dst_ptr, static_cast<uint32_t>(field_val.as_uint64().value()));
                }
                break;
            }
            case field_kind::int64:
            {
                if (field_val.is_int() || field_val.is_number())
                {
                    set_raw_field_val(field_dst_ptr, field_val.as_int64().value());
                }
                break;
            }
            case field_kind::uint64:
            {
                if (field_val.is_uint() || field_val.is_number())
                {
                    set_raw_field_val(field_dst_ptr, field_val.as_uint64().value());
                }
                break;
            }
            case field_kind::float32:
            {
                if (field_val.is_real() || field_val.is_number())
                {
                    set_raw_field_val(field_dst_ptr, field_val.as_float().value());
                }
                break;
            }
            case field_kind::float64:
            {
                if (field_val.is_real() || field_val.is_number())
                {
                    set_raw_field_val(field_dst_ptr, field_val.as_double().value());
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
                        set_raw_field_val(field_dst_ptr, val);
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
                        set_raw_field_val(field_dst_ptr, val);
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
                        set_raw_field_val(field_dst_ptr, val);
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
                        set_raw_field_val(field_dst_ptr, val);
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
                        set_raw_field_val(field_dst_ptr, *parsed);
                    }
                }
                break;
            }
            case field_kind::enumeration:
            {
                if (field_val.is_string())
                {
                    const auto name = field_val.as_string().value();
                    for (const auto& entry : field.enum_entries)
                    {
                        if (entry.name == name)
                        {
                            if (field.size == 1)
                            {
                                const auto val8 = static_cast<int8_t>(entry.value);
                                set_raw_field_val(field_dst_ptr, val8);
                            }
                            else if (field.size == 2)
                            {
                                const auto val16 = static_cast<int16_t>(entry.value);
                                set_raw_field_val(field_dst_ptr, val16);
                            }
                            else if (field.size == 4)
                            {
                                const auto val32 = static_cast<int32_t>(entry.value);
                                set_raw_field_val(field_dst_ptr, val32);
                            }
                            else
                            {
                                set_raw_field_val(field_dst_ptr, entry.value);
                            }
                            break;
                        }
                    }
                }
                else if (field_val.is_int() || field_val.is_number())
                {
                    const auto enum_val = field_val.as_int64().value();
                    if (field.size == 1)
                    {
                        const auto val8 = static_cast<int8_t>(enum_val);
                        set_raw_field_val(field_dst_ptr, val8);
                    }
                    else if (field.size == 2)
                    {
                        const auto val16 = static_cast<int16_t>(enum_val);
                        set_raw_field_val(field_dst_ptr, val16);
                    }
                    else if (field.size == 4)
                    {
                        const auto val32 = static_cast<int32_t>(enum_val);
                        set_raw_field_val(field_dst_ptr, val32);
                    }
                    else
                    {
                        set_raw_field_val(field_dst_ptr, enum_val);
                    }
                }
                break;
            }
            case field_kind::entity_ref:
            {
                if (field_val.is_string())
                {
                    const auto str = field_val.as_string().value();
                    const auto parsed = parse_local_id(str);
                    if (parsed.has_value())
                    {
                        set_raw_field_val(field_dst_ptr, static_cast<ecs::entity>(parsed->value));
                    }
                    else
                    {
                        set_raw_field_val(field_dst_ptr, ecs::tombstone);
                    }
                }
                else if (field_val.is_uint() || field_val.is_number())
                {
                    const auto val = field_val.as_uint64().value();
                    set_raw_field_val(field_dst_ptr, static_cast<ecs::entity>(val));
                }
                else
                {
                    set_raw_field_val(field_dst_ptr, ecs::tombstone);
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
                        set_raw_field_val(field_dst_ptr, *parsed);
                    }
                }
                else if (field_val.is_uint() || field_val.is_number())
                {
                    const auto val = field_val.as_uint64().value();
                    set_raw_field_val(field_dst_ptr, val);
                }
                break;
            }
            case field_kind::fixed_string:
            {
                if (field_val.is_string() && field_dst_ptr != nullptr && field.size > 0)
                {
                    const auto str = field_val.as_string().value();
                    auto* dst = static_cast<char*>(field_dst_ptr);
                    const auto max_len = field.size - 1;
                    const auto copy_len = str.size() < max_len ? str.size() : max_len;
                    tempest::memcpy(dst, str.data(), copy_len);
                    dst[copy_len] = '\0';
                }
                break;
            }
        }
    }

    inline void serialize_field_to_key(const ecs::field_descriptor& field, const void* comp_ptr,
                                       json_object_mut& target_obj, string_view key,
                                       const ecs::basic_archetype_registry* registry)
    {
        if (field.get != nullptr)
        {
            alignas(alignof(max_align_t)) byte buffer[64] = {};
            if (field.size <= sizeof(buffer))
            {
                field.get(comp_ptr, buffer);
                serialize_raw_field_value_to_key(field, buffer, target_obj, key, registry);
            }
            else
            {
                auto dyn_buffer = vector<byte>(field.size, byte{0});
                field.get(comp_ptr, dyn_buffer.data());
                serialize_raw_field_value_to_key(field, dyn_buffer.data(), target_obj, key, registry);
            }
        }
        else if (field.offset != ecs::field_descriptor::no_offset)
        {
            const auto* val_ptr = static_cast<const byte*>(comp_ptr) + field.offset;
            serialize_raw_field_value_to_key(field, val_ptr, target_obj, key, registry);
        }
    }

    inline void serialize_field(const ecs::field_descriptor& field, const void* comp_ptr,
                                json_object_mut& target_obj, const ecs::basic_archetype_registry* registry)
    {
        serialize_field_to_key(field, comp_ptr, target_obj, field.name, registry);
    }

    inline void serialize_field(const ecs::field_descriptor& field, const void* comp_ptr,
                                json_object_mut& target_obj, const ecs::basic_archetype_registry& registry)
    {
        serialize_field_to_key(field, comp_ptr, target_obj, field.name, &registry);
    }

    inline void deserialize_field(const ecs::field_descriptor& field, json_value field_val, void* comp_ptr)
    {
        if (field.set != nullptr)
        {
            alignas(alignof(max_align_t)) byte buffer[64] = {};
            if (field.size <= sizeof(buffer))
            {
                deserialize_raw_field_value(field, field_val, buffer);
                field.set(comp_ptr, buffer);
            }
            else
            {
                auto dyn_buffer = vector<byte>(field.size, byte{0});
                deserialize_raw_field_value(field, field_val, dyn_buffer.data());
                field.set(comp_ptr, dyn_buffer.data());
            }
        }
        else if (field.offset != ecs::field_descriptor::no_offset)
        {
            auto* dst_ptr = static_cast<byte*>(comp_ptr) + field.offset;
            deserialize_raw_field_value(field, field_val, dst_ptr);
        }
    }
} // namespace tempest::scene::detail

#endif // tempest_scene_scene_codec_detail_hpp
