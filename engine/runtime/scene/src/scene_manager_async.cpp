#include <tempest/scene/scene_manager.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/files.hpp>
#include <tempest/guid.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/json.hpp>
#include <tempest/math.hpp>
#include <tempest/random.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/scene_load_handle.hpp>
#include <tempest/scene/scene_tags.hpp>
#include <tempest/utility.hpp>

namespace tempest::scene
{
    //==============================================================================
    // scene_load_handle Implementation
    //==============================================================================

    auto scene_load_handle::is_valid() const -> bool
    {
        if (manager == nullptr || index == invalid_index)
        {
            return false;
        }
        return manager->is_load_handle_valid(*this);
    }

    auto scene_load_handle::state() const -> scene_load_state
    {
        if (!is_valid())
        {
            return scene_load_state::invalid;
        }
        return manager->get_load_state(*this);
    }

    auto scene_load_handle::is_ready() const -> bool
    {
        return state() == scene_load_state::ready;
    }

    auto scene_load_handle::is_failed() const -> bool
    {
        return state() == scene_load_state::failed;
    }

    auto scene_load_handle::is_cancelled() const -> bool
    {
        return state() == scene_load_state::cancelled;
    }

    auto scene_load_handle::is_done() const -> bool
    {
        const auto current_state = state();
        return current_state == scene_load_state::ready || current_state == scene_load_state::failed ||
               current_state == scene_load_state::cancelled;
    }

    auto scene_load_handle::progress() const -> float
    {
        if (!is_valid())
        {
            return 0.0f;
        }
        return manager->get_load_progress(*this);
    }

    auto scene_load_handle::scene() const -> scene_handle
    {
        if (!is_valid())
        {
            return {};
        }
        return manager->get_load_scene(*this);
    }

    auto scene_load_handle::error() const -> optional<scene_error>
    {
        if (!is_valid())
        {
            return nullopt;
        }
        return manager->get_load_error(*this);
    }

    auto scene_load_handle::cancel() -> void
    {
        if (is_valid())
        {
            manager->cancel_load(*this);
        }
    }

    //==============================================================================
    // Stage 1 Intermediate Staging & Asset Resolution Helpers
    //==============================================================================

    namespace
    {
        struct async_load_job_task : async_load_task_base
        {
            job::task<scene_staging> task;

            explicit async_load_job_task(job::task<scene_staging> t) : task{tempest::move(t)}
            {
            }

            auto is_ready() -> bool override
            {
                return task.is_ready();
            }

            auto get_result() -> scene_staging override
            {
                return tempest::move(task.value());
            }
        };

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
                default:
                    break;
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

            for (auto i = 0U; i < static_cast<uint32_t>(type_reg.size()); ++i)
            {
                const auto& cand = type_reg.type_info(i);
                if (cand.name == comp_name)
                {
                    return &cand;
                }
            }
            return nullptr;
        }

        auto stage_decode_document(string_view json_content, const ecs::component_type_registry& type_reg)
            -> scene_staging
        {
            auto alloc = system_allocator{};
            auto doc_res = json_document::from_string(json_content, alloc);
            if (!doc_res.has_value())
            {
                return scene_staging{
                    .entities = {},
                    .root_indices = {},
                    .unrecognized_data = {},
                    .success = false,
                    .error_message = "Failed to parse JSON scene document",
                };
            }

            const auto root_val = doc_res->root();
            if (!root_val.is_object())
            {
                return scene_staging{
                    .entities = {},
                    .root_indices = {},
                    .unrecognized_data = {},
                    .success = false,
                    .error_message = "JSON scene root must be an object",
                };
            }

            const auto root_obj_res = root_val.as_object();
            if (!root_obj_res.has_value())
            {
                return scene_staging{
                    .entities = {},
                    .root_indices = {},
                    .unrecognized_data = {},
                    .success = false,
                    .error_message = "Failed to access root object",
                };
            }
            const auto root_obj = *root_obj_res;

            const auto entities_val = root_obj["entities"];
            if (!entities_val.is_array())
            {
                return scene_staging{
                    .entities = {},
                    .root_indices = {},
                    .unrecognized_data = {},
                    .success = false,
                    .error_message = "Scene document missing 'entities' array",
                };
            }

            const auto entities_arr_res = entities_val.as_array();
            if (!entities_arr_res.has_value())
            {
                return scene_staging{
                    .entities = {},
                    .root_indices = {},
                    .unrecognized_data = {},
                    .success = false,
                    .error_message = "Failed to access 'entities' array",
                };
            }

            auto staging = scene_staging{};

            auto decode_entity = [&](auto& self, json_value entity_val, optional<size_t> parent_idx) -> void {
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

                auto name = string{};
                const auto name_val = entity_val["name"];
                if (name_val.is_string())
                {
                    name = string{name_val.as_string().value()};
                }

                const auto current_idx = staging.entities.size();
                auto& staged = staging.entities.emplace_back();
                staged.local_id = local_id;
                staged.name = tempest::move(name);
                staged.parent_index = parent_idx;

                if (!parent_idx.has_value())
                {
                    staging.root_indices.push_back(current_idx);
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

                            const auto* type_info = find_component_type_info(type_reg, comp_name);
                            if (type_info != nullptr)
                            {
                                auto comp = staged_component{};
                                comp.type_info = type_info;
                                comp.data.resize(type_info->size, byte{0});
                                if (type_info->construct != nullptr)
                                {
                                    type_info->construct(comp.data.data());
                                }

                                if (type_info->descriptor.has_value() && comp_val.is_object())
                                {
                                    const auto fields_obj_res = comp_val.as_object();
                                    if (fields_obj_res.has_value())
                                    {
                                        for (const auto& field_member : *fields_obj_res)
                                        {
                                            const auto field_name = field_member.key;
                                            const auto field_v = field_member.value;

                                            const auto* field_desc = type_info->descriptor->find_field(field_name);
                                            if (field_desc != nullptr)
                                            {
                                                deserialize_field(*field_desc, field_v, comp.data.data());
                                            }
                                            else
                                            {
                                                const auto field_json = json_writer::to_string(field_v);
                                                staging.unrecognized_data.entities[local_id.value]
                                                    .unknown_fields[string{comp_name}]
                                                    .push_back({
                                                        .field_name = string{field_name},
                                                        .json_payload = field_json,
                                                    });
                                            }
                                        }
                                    }
                                }

                                staged.components.push_back(tempest::move(comp));
                            }
                            else
                            {
                                const auto comp_json = json_writer::to_string(comp_val);
                                staging.unrecognized_data.entities[local_id.value].unknown_components.push_back({
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
                            self(self, child_item, current_idx);
                        }
                    }
                }
            };

            for (const auto& entity_item : *entities_arr_res)
            {
                decode_entity(decode_entity, entity_item, nullopt);
            }

            staging.success = true;
            return staging;
        }

        auto resolve_staging_assets(scene_staging& staging, assets::asset_database* asset_db, bool strict) -> bool
        {
            if (asset_db == nullptr)
            {
                return true;
            }

            for (const auto& staged : staging.entities)
            {
                for (const auto& comp : staged.components)
                {
                    if (comp.type_info == nullptr || !comp.type_info->descriptor.has_value())
                    {
                        continue;
                    }

                    for (const auto& field : comp.type_info->descriptor->fields)
                    {
                        if (field.kind == ecs::field_kind::asset_ref)
                        {
                            if (field.size == sizeof(guid))
                            {
                                const auto asset_id = get_field_val<guid>(field, comp.data.data());
                                if (asset_id != guid{})
                                {
                                    const auto* src = asset_db->find_source_by_id(asset_id);
                                    const auto meta = asset_db->get_asset_metadata(asset_id);
                                    if (src == nullptr && !meta.has_value())
                                    {
                                        if (strict)
                                        {
                                            staging.success = false;
                                            staging.error_message =
                                                "Strict asset resolution failed: asset GUID not found in database";
                                            return false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            return true;
        }

        auto stage_decode_and_resolve(string_view json_content, const ecs::component_type_registry& type_reg,
                                      assets::asset_database* asset_db, bool strict) -> scene_staging
        {
            auto staging = stage_decode_document(json_content, type_reg);
            if (!staging.success)
            {
                return staging;
            }

            const auto resolved = resolve_staging_assets(staging, asset_db, strict);
            if (!resolved)
            {
                staging.success = false;
            }

            return staging;
        }
    } // namespace

    //==============================================================================
    // scene_manager Asynchronous API Implementation
    //==============================================================================

    auto scene_manager::allocate_async_slot() -> scene_load_handle
    {
        if (!_free_async_slots.empty())
        {
            const auto slot_index = _free_async_slots.back();
            _free_async_slots.pop_back();
            auto& slot = _async_ops[slot_index];
            slot.occupied = true;
            slot.state = scene_load_state::invalid;
            slot.progress = 0.0f;
            slot.commit_cursor = 0;
            slot.error = nullopt;
            slot.committed_entities.clear();
            slot.root_entities.clear();
            slot.local_to_entity.clear();
            slot.staging = {};
            slot.background_task.reset();
            return scene_load_handle{
                .index = slot_index,
                .generation = slot.generation,
                .manager = this,
            };
        }

        const auto slot_index = static_cast<uint32_t>(_async_ops.size());
        auto& slot = _async_ops.emplace_back();
        slot.occupied = true;
        slot.generation = 1;
        return scene_load_handle{
            .index = slot_index,
            .generation = slot.generation,
            .manager = this,
        };
    }

    auto scene_manager::free_async_slot(uint32_t index) -> void
    {
        auto& slot = _async_ops[index];
        slot.occupied = false;
        slot.generation++;
        slot.state = scene_load_state::invalid;
        slot.scene = {};
        slot.scene_name.clear();
        slot.scene_path.clear();
        slot.json_content.clear();
        slot.error = nullopt;
        slot.progress = 0.0f;
        slot.staging = {};
        slot.commit_cursor = 0;
        slot.local_to_entity.clear();
        slot.committed_entities.clear();
        slot.root_entities.clear();
        slot.background_task.reset();
        _free_async_slots.push_back(index);
    }

    auto scene_manager::is_load_handle_valid(scene_load_handle handle) const noexcept -> bool
    {
        if (handle.index >= _async_ops.size())
        {
            return false;
        }
        const auto& slot = _async_ops[handle.index];
        return slot.occupied && (slot.generation == handle.generation);
    }

    auto scene_manager::get_load_state(scene_load_handle handle) const noexcept -> scene_load_state
    {
        if (!is_load_handle_valid(handle))
        {
            return scene_load_state::invalid;
        }
        return _async_ops[handle.index].state;
    }

    auto scene_manager::get_load_progress(scene_load_handle handle) const noexcept -> float
    {
        if (!is_load_handle_valid(handle))
        {
            return 0.0f;
        }
        return _async_ops[handle.index].progress;
    }

    auto scene_manager::get_load_scene(scene_load_handle handle) const noexcept -> scene_handle
    {
        if (!is_load_handle_valid(handle))
        {
            return {};
        }
        return _async_ops[handle.index].scene;
    }

    auto scene_manager::get_load_error(scene_load_handle handle) const noexcept -> optional<scene_error>
    {
        if (!is_load_handle_valid(handle))
        {
            return nullopt;
        }
        return _async_ops[handle.index].error;
    }

    auto scene_manager::load_async(const filesystem::path& path, const scene_async_load_options& options)
        -> scene_load_handle
    {
        const auto content_res = read_file_to_string(path);
        if (!content_res.has_value())
        {
            const auto handle = allocate_async_slot();
            auto& op = _async_ops[handle.index];
            op.state = scene_load_state::failed;
            op.error = (content_res.error() == file_error::not_found) ? scene_error::file_not_found
                                                                     : scene_error::io_error;
            return handle;
        }

        const auto scene_name = path.stem().string();
        auto handle = load_from_string_async(*content_res, scene_name, options);
        if (is_load_handle_valid(handle))
        {
            _async_ops[handle.index].scene_path = path.generic_string();
        }
        return handle;
    }

    auto scene_manager::load_from_string_async(string_view json_content, string_view scene_name,
                                               const scene_async_load_options& options) -> scene_load_handle
    {
        if (options.mode == scene_load_mode::single && options.clear_first)
        {
            unload_all();
        }

        const auto handle = allocate_async_slot();
        auto& op = _async_ops[handle.index];
        op.scene = allocate_slot();
        op.options = options;
        if (!scene_name.empty())
        {
            op.scene_name = string{scene_name};
        }
        op.state = scene_load_state::decoding;
        op.progress = 0.0f;

        if (_jobs != nullptr)
        {
            auto content_str = string{json_content};
            auto* type_reg = &_registry->type_registry();
            auto* asset_db = _assets;
            const auto strict = options.strict_asset_resolution;

            auto job_task = _jobs->async("scene_stage1_decode", job::task_priority::normal, job::core_class::any,
                [content = tempest::move(content_str), type_reg, asset_db, strict]() -> scene_staging {
                    return stage_decode_and_resolve(content, *type_reg, asset_db, strict);
                });
            op.background_task = make_unique<async_load_job_task>(tempest::move(job_task));
        }
        else
        {
            op.json_content = string{json_content};
        }

        return handle;
    }

    auto scene_manager::tick() -> void
    {
        for (auto i = 0U; i < static_cast<uint32_t>(_async_ops.size()); ++i)
        {
            auto& op = _async_ops[i];
            if (!op.occupied)
            {
                continue;
            }

            // Stage 1 Check / Completion
            if (op.state == scene_load_state::decoding || op.state == scene_load_state::resolving_assets)
            {
                if (op.background_task)
                {
                    if (!op.background_task->is_ready())
                    {
                        op.progress = 0.2f;
                        continue;
                    }
                    op.staging = op.background_task->get_result();
                    op.background_task.reset();
                }
                else
                {
                    op.progress = 0.2f;
                    op.staging = stage_decode_and_resolve(op.json_content, _registry->type_registry(), _assets,
                                                          op.options.strict_asset_resolution);
                    op.json_content.clear();
                }

                if (!op.staging.success)
                {
                    op.state = scene_load_state::failed;
                    op.error = scene_error::deserialization_failed;
                    if (op.scene.is_valid())
                    {
                        free_slot(op.scene.index);
                        op.scene = {};
                    }
                    continue;
                }

                op.state = scene_load_state::committing;
                op.progress = 0.4f;
            }

            // Stage 2 (Commit Stage): Time-sliced entity instantiation
            if (op.state == scene_load_state::committing)
            {
                const auto total_entities = op.staging.entities.size();
                const auto batch_end =
                    tempest::min(op.commit_cursor + op.options.entities_per_frame, total_entities);

                for (auto entity_idx = op.commit_cursor; entity_idx < batch_end; ++entity_idx)
                {
                    const auto& staged = op.staging.entities[entity_idx];
                    const auto ecs_ent = _registry->create();

                    _registry->assign(ecs_ent, scene_inactive_tag{});
                    _registry->assign(ecs_ent, scene_entity_component{
                                                   .scene = op.scene,
                                                   .local_id = staged.local_id,
                                               });

                    if (!staged.name.empty())
                    {
                        _registry->name(ecs_ent, staged.name);
                    }

                    for (const auto& staged_comp : staged.components)
                    {
                        if (staged_comp.type_info != nullptr)
                        {
                            ecs::raw_registry_access::assign_raw(*_registry, ecs_ent, *staged_comp.type_info,
                                                                 staged_comp.data.data());
                        }
                    }

                    if (staged.parent_index.has_value() && *staged.parent_index < op.committed_entities.size())
                    {
                        const auto parent_ent = op.committed_entities[*staged.parent_index];
                        _registry->reparent(ecs_ent, parent_ent);
                    }
                    else
                    {
                        op.root_entities.push_back(ecs_ent);
                    }

                    op.committed_entities.push_back(ecs_ent);
                    op.local_to_entity[staged.local_id.value] = ecs_ent;
                }

                op.commit_cursor = batch_end;
                const auto fraction = (total_entities > 0)
                                          ? static_cast<float>(op.commit_cursor) / static_cast<float>(total_entities)
                                          : 1.0f;
                op.progress = 0.4f + 0.6f * fraction;

                // Finalize scene if commit is complete
                if (op.commit_cursor >= total_entities)
                {
                    for (const auto entity : op.committed_entities)
                    {
                        const auto comp_types = ecs::raw_registry_access::get_component_types(*_registry, entity);
                        for (const auto& info : comp_types)
                        {
                            if (!info.descriptor.has_value())
                            {
                                continue;
                            }
                            const auto& desc = *info.descriptor;
                            auto* comp_ptr =
                                ecs::raw_registry_access::get_component_raw(*_registry, entity, info.index);
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
                                    if (target_local_id != 0 &&
                                        target_local_id != static_cast<uint64_t>(ecs::tombstone))
                                    {
                                        const auto iter = op.local_to_entity.find(target_local_id);
                                        if (iter != op.local_to_entity.end())
                                        {
                                            set_field_val(field, comp_ptr, iter->second);
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

                    for (const auto entity : op.committed_entities)
                    {
                        _registry->remove<scene_inactive_tag>(entity);
                    }

                    auto& record = _scenes[op.scene.index];
                    record.root_entities = tempest::move(op.root_entities);
                    record.unrecognized_data = tempest::move(op.staging.unrecognized_data);
                    if (!op.scene_name.empty())
                    {
                        record.name = tempest::move(op.scene_name);
                    }
                    if (!op.scene_path.empty())
                    {
                        record.path = tempest::move(op.scene_path);
                    }

                    if (op.options.mode == scene_load_mode::single && !op.options.clear_first)
                    {
                        for (auto prior_idx = 0U; prior_idx < static_cast<uint32_t>(_scenes.size()); ++prior_idx)
                        {
                            if (prior_idx != op.scene.index && _scenes[prior_idx].occupied)
                            {
                                unload(scene_handle{
                                    .index = prior_idx,
                                    .generation = _scenes[prior_idx].generation,
                                });
                            }
                        }
                    }

                    if (!_active_scene.is_valid())
                    {
                        set_active_scene(op.scene);
                    }

                    op.state = scene_load_state::ready;
                    op.progress = 1.0f;
                }
            }
        }
    }

    auto scene_manager::cancel_load(scene_load_handle handle) -> void
    {
        if (!is_load_handle_valid(handle))
        {
            return;
        }

        auto& op = _async_ops[handle.index];
        if (op.state == scene_load_state::cancelled || op.state == scene_load_state::failed)
        {
            return;
        }

        for (const auto ent : op.committed_entities)
        {
            if (_registry->is_valid(ent))
            {
                _registry->destroy_recursive(ent);
            }
        }

        if (op.scene.is_valid())
        {
            auto sweep_entities = vector<ecs::entity>{};
            for (const auto [self, comp] : _registry->with<ecs::self_component, scene_entity_component>())
            {
                if (comp.scene == op.scene)
                {
                    sweep_entities.push_back(self.entity);
                }
            }
            for (const auto ent : sweep_entities)
            {
                if (_registry->is_valid(ent))
                {
                    _registry->destroy_recursive(ent);
                }
            }

            free_slot(op.scene.index);
            op.scene = {};
        }

        op.committed_entities.clear();
        op.local_to_entity.clear();
        op.root_entities.clear();
        op.background_task.reset();
        op.state = scene_load_state::cancelled;
    }
} // namespace tempest::scene
