#include <tempest/scene/prefab_instantiator.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/int.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>
#include <tempest/utility.hpp>

#include "scene_codec_detail.hpp"

namespace tempest::scene
{
    namespace
    {
        /// @brief Maximum stack buffer size for staging zero-padded field override payloads.
        /// Accommodates all primitive reflected field types (up to mat4 at 64 bytes or small structs)
        /// without requiring heap allocation.
        static constexpr size_t max_stack_field_override_buffer_size = 256;

        auto component_name_matches(string_view registered_name, string_view query_name) noexcept -> bool
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

        auto find_component_type_info(const ecs::component_type_registry& type_registry, string_view component_name)
            -> const ecs::component_type_info*
        {
            const auto* exact_info = type_registry.type_info(component_name);
            if (exact_info != nullptr)
            {
                return exact_info;
            }

            for (auto type_index = size_t{0}; type_index < type_registry.size(); ++type_index)
            {
                const auto& candidate_info = type_registry.type_info(static_cast<uint32_t>(type_index));
                if (component_name_matches(candidate_info.name, component_name))
                {
                    return &candidate_info;
                }
            }
            return nullptr;
        }

        auto find_descendant_by_local_id(const ecs::archetype_registry& registry, ecs::entity parent_entity,
                                         entity_local_id target_id) -> ecs::entity
        {
            const auto* parent_relationship = registry.try_get<ecs::relationship_component<ecs::entity>>(parent_entity);
            if (parent_relationship == nullptr || parent_relationship->first_child == ecs::tombstone)
            {
                return ecs::tombstone;
            }

            auto child_entity = parent_relationship->first_child;
            while (child_entity != ecs::tombstone && registry.is_valid(child_entity))
            {
                const auto* member_comp = registry.try_get<prefab_member_component>(child_entity);
                if (member_comp != nullptr && member_comp->template_local_id == target_id)
                {
                    return child_entity;
                }

                const auto found_descendant = find_descendant_by_local_id(registry, child_entity, target_id);
                if (found_descendant != ecs::tombstone)
                {
                    return found_descendant;
                }

                const auto* child_relationship =
                    registry.try_get<ecs::relationship_component<ecs::entity>>(child_entity);
                child_entity = (child_relationship != nullptr) ? child_relationship->next_sibling : ecs::tombstone;
            }

            return ecs::tombstone;
        }

        auto find_entity_by_id_path(const ecs::archetype_registry& registry, ecs::entity root_entity,
                                    const id_path& path,
                                    const flat_unordered_map<uint64_t, ecs::entity>& local_to_entity) -> ecs::entity
        {
            if (path.empty())
            {
                return root_entity;
            }

            const auto* root_member = registry.try_get<prefab_member_component>(root_entity);

            if (path.size() == 1)
            {
                if (root_member != nullptr && root_member->template_local_id == path[0])
                {
                    return root_entity;
                }

                const auto local_iter = local_to_entity.find(path[0].value);
                if (local_iter != local_to_entity.end())
                {
                    return local_iter->second;
                }

                return find_descendant_by_local_id(registry, root_entity, path[0]);
            }

            auto current_entity = root_entity;
            auto segment_index = size_t{0};

            if (root_member != nullptr && root_member->template_local_id == path[0])
            {
                segment_index = 1;
            }
            else
            {
                const auto local_iter = local_to_entity.find(path[0].value);
                if (local_iter != local_to_entity.end())
                {
                    current_entity = local_iter->second;
                    segment_index = 1;
                }
                else
                {
                    const auto found_root_child = find_descendant_by_local_id(registry, root_entity, path[0]);
                    if (found_root_child != ecs::tombstone)
                    {
                        current_entity = found_root_child;
                        segment_index = 1;
                    }
                    else
                    {
                        return ecs::tombstone;
                    }
                }
            }

            for (; segment_index < path.size(); ++segment_index)
            {
                const auto target_local_id = path[segment_index];
                const auto next_entity = find_descendant_by_local_id(registry, current_entity, target_local_id);
                if (next_entity == ecs::tombstone)
                {
                    return ecs::tombstone;
                }
                current_entity = next_entity;
            }

            return current_entity;
        }

        auto instantiate_impl(const prefab_template& tmpl, ecs::archetype_registry& registry,
                              const prefab_overrides& overrides, const prefab_cache* cache, const guid& prefab_guid,
                              entity_local_id instance_local_id, span<const guid> active_stack)
            -> expected<prefab_instance_result, prefab_error>
        {
            // 1. Validate template
            if (tmpl.is_empty())
            {
                return unexpected(prefab_error::invalid_template);
            }

            const auto records = tmpl.get_records();
            const auto root_indices = tmpl.get_root_indices();
            if (root_indices.empty())
            {
                return unexpected(prefab_error::invalid_template);
            }

            for (auto record_index = size_t{0}; record_index < records.size(); ++record_index)
            {
                const auto& rec = records[record_index];
                if (rec.parent_index.has_value())
                {
                    if (*rec.parent_index >= records.size() || *rec.parent_index == record_index)
                    {
                        return unexpected(prefab_error::invalid_template);
                    }
                }
            }

            // 2. Create entities in world registry
            auto all_entities = vector<ecs::entity>{};
            all_entities.reserve(records.size());

            auto cleanup_on_error = [&](vector<ecs::entity>& entities_to_destroy) {
                for (auto entity_index = entities_to_destroy.size(); entity_index > 0; --entity_index)
                {
                    const auto entity = entities_to_destroy[entity_index - 1];
                    if (registry.is_valid(entity))
                    {
                        registry.destroy(entity);
                    }
                }
            };

            for (auto record_index = size_t{0}; record_index < records.size(); ++record_index)
            {
                all_entities.push_back(registry.create());
            }

            const auto root_record_index = root_indices.front();
            const auto root_entity = all_entities[root_record_index];

            // 3. Assign names
            for (auto record_index = size_t{0}; record_index < records.size(); ++record_index)
            {
                if (!records[record_index].name.empty())
                {
                    registry.name(all_entities[record_index], records[record_index].name);
                }
            }

            // Helper to check if a component is marked removed
            auto is_component_removed = [&](size_t record_index, string_view component_name) -> bool {
                const auto is_root = (record_index == root_record_index);
                const auto local_id = records[record_index].local_id;
                const auto root_local_id = records[root_record_index].local_id;

                for (const auto& removed_item : overrides.removed_components)
                {
                    auto target_matches = false;
                    if (is_root)
                    {
                        if (removed_item.target.empty() ||
                            (removed_item.target.size() == 1 && removed_item.target[0] == local_id))
                        {
                            target_matches = true;
                        }
                    }
                    else
                    {
                        if (removed_item.target.size() == 1 && removed_item.target[0] == local_id)
                        {
                            target_matches = true;
                        }
                        else if (removed_item.target.size() == 2 && removed_item.target[0] == root_local_id &&
                                 removed_item.target[1] == local_id)
                        {
                            target_matches = true;
                        }
                    }

                    if (target_matches && component_name_matches(component_name, removed_item.component_name))
                    {
                        return true;
                    }
                }
                return false;
            };

            // 4. Assign components from record.components, skipping removed_components
            for (auto record_index = size_t{0}; record_index < records.size(); ++record_index)
            {
                const auto target_entity = all_entities[record_index];
                for (const auto& staged_comp : records[record_index].components)
                {
                    if (staged_comp.type_info != nullptr)
                    {
                        if (is_component_removed(record_index, staged_comp.type_info->name))
                        {
                            continue;
                        }

                        auto* comp_ptr = ecs::raw_registry_access::assign_raw(
                            registry, target_entity, *staged_comp.type_info,
                            staged_comp.data.empty() ? nullptr : staged_comp.data.data());

                        if (comp_ptr != nullptr && staged_comp.type_info->descriptor.has_value() &&
                            staged_comp.type_info->descriptor->post_load != nullptr)
                        {
                            staged_comp.type_info->descriptor->post_load(comp_ptr);
                        }
                    }
                }
            }

            // 5. Assign prefab_member_component to each member entity
            for (auto record_index = size_t{0}; record_index < records.size(); ++record_index)
            {
                registry.assign(all_entities[record_index], prefab_member_component{
                                                                .template_local_id = records[record_index].local_id,
                                                            });
            }

            // 6. Assign prefab_instance_component to the root entity
            registry.assign(root_entity, prefab_instance_component{
                                             .prefab_guid = prefab_guid,
                                             .instance_local_id = instance_local_id,
                                             .overrides = overrides,
                                         });

            // 7. Build hierarchy using record.parent_index
            for (auto record_index = size_t{0}; record_index < records.size(); ++record_index)
            {
                if (records[record_index].parent_index.has_value())
                {
                    const auto parent_idx = *records[record_index].parent_index;
                    if (parent_idx < all_entities.size())
                    {
                        ecs::create_parent_child_relationship(registry, all_entities[parent_idx],
                                                              all_entities[record_index]);
                    }
                }
            }

            // Local-to-entity map for this template's entities
            auto local_to_entity = flat_unordered_map<uint64_t, ecs::entity>{};
            for (auto record_index = size_t{0}; record_index < records.size(); ++record_index)
            {
                if (records[record_index].local_id.is_valid())
                {
                    local_to_entity[records[record_index].local_id.value] = all_entities[record_index];
                }
            }

            // 8. Handle nested prefab records
            for (auto record_index = size_t{0}; record_index < records.size(); ++record_index)
            {
                const auto& rec = records[record_index];
                if (!rec.nested_instance.has_value())
                {
                    continue;
                }

                const auto& nested_desc = *rec.nested_instance;
                if (cache == nullptr || !cache->has_template(nested_desc.prefab_guid))
                {
                    cleanup_on_error(all_entities);
                    return unexpected(prefab_error::template_not_found);
                }

                // Cycle detection
                for (const auto& active_guid : active_stack)
                {
                    if (active_guid == nested_desc.prefab_guid)
                    {
                        cleanup_on_error(all_entities);
                        return unexpected(prefab_error::cyclic_nesting);
                    }
                }
                if (prefab_guid != guid{} && nested_desc.prefab_guid == prefab_guid)
                {
                    cleanup_on_error(all_entities);
                    return unexpected(prefab_error::cyclic_nesting);
                }

                // Collect overrides targeting this nested instance
                auto nested_overrides = prefab_overrides{};
                for (const auto& field_override_item : overrides.field_overrides)
                {
                    if (!field_override_item.target.empty() && field_override_item.target.front() == rec.local_id)
                    {
                        auto stripped = field_override_item;
                        stripped.target.erase(stripped.target.begin());
                        nested_overrides.field_overrides.push_back(tempest::move(stripped));
                    }
                }
                for (const auto& removed_item : overrides.removed_components)
                {
                    if (!removed_item.target.empty() && removed_item.target.front() == rec.local_id)
                    {
                        auto stripped = removed_item;
                        stripped.target.erase(stripped.target.begin());
                        nested_overrides.removed_components.push_back(tempest::move(stripped));
                    }
                }
                for (const auto& added_item : overrides.added_components)
                {
                    if (!added_item.target.empty() && added_item.target.front() == rec.local_id)
                    {
                        auto stripped = added_item;
                        stripped.target.erase(stripped.target.begin());
                        nested_overrides.added_components.push_back(tempest::move(stripped));
                    }
                }

                auto next_stack = vector<guid>{active_stack.begin(), active_stack.end()};
                if (prefab_guid != guid{})
                {
                    auto found_self = false;
                    for (const auto& existing_guid : next_stack)
                    {
                        if (existing_guid == prefab_guid)
                        {
                            found_self = true;
                            break;
                        }
                    }
                    if (!found_self)
                    {
                        next_stack.push_back(prefab_guid);
                    }
                }
                next_stack.push_back(nested_desc.prefab_guid);

                auto nested_result = optional<expected<prefab_instance_result, prefab_error>>{};
                const auto nested_found =
                    cache->with_template(nested_desc.prefab_guid, [&](const prefab_template& nested_tmpl) {
                        nested_result =
                            instantiate_impl(nested_tmpl, registry, nested_overrides, cache, nested_desc.prefab_guid,
                                             nested_desc.instance_local_id, next_stack);
                    });
                if (!nested_found || !nested_result.has_value())
                {
                    cleanup_on_error(all_entities);
                    return unexpected(prefab_error::template_not_found);
                }
                if (!nested_result->has_value())
                {
                    cleanup_on_error(all_entities);
                    return unexpected(nested_result->error());
                }

                const auto& nested_inst = nested_result->value();

                // Attach nested root entity under record's entity
                ecs::create_parent_child_relationship(registry, all_entities[record_index], nested_inst.root_entity);

                // Merge nested entities into all_entities and local_to_entity
                for (const auto nested_ent : nested_inst.all_entities)
                {
                    all_entities.push_back(nested_ent);
                }
                for (const auto& [nested_id, nested_ent] : nested_inst.local_to_entity)
                {
                    local_to_entity.insert({nested_id, nested_ent});
                }
            }

            // Helper to check whether a record index corresponds to a nested instance
            auto is_nested_host_local_id = [&](entity_local_id check_id) -> bool {
                for (const auto& record_item : records)
                {
                    if (record_item.local_id == check_id && record_item.nested_instance.has_value())
                    {
                        return true;
                    }
                }
                return false;
            };

            // 9. Apply added components (before field overrides so field overrides can target added components)
            for (const auto& added_item : overrides.added_components)
            {
                if (!added_item.target.empty() && is_nested_host_local_id(added_item.target.front()))
                {
                    continue;
                }

                const auto target_entity =
                    find_entity_by_id_path(registry, root_entity, added_item.target, local_to_entity);
                if (target_entity == ecs::tombstone || !registry.is_valid(target_entity))
                {
                    cleanup_on_error(all_entities);
                    return unexpected(prefab_error::component_not_found);
                }

                if (added_item.component.type_info == nullptr)
                {
                    cleanup_on_error(all_entities);
                    return unexpected(prefab_error::component_not_found);
                }

                const auto* type_info_ptr = added_item.component.type_info;
                auto* component_data_ptr = ecs::raw_registry_access::assign_raw(
                    registry, target_entity, *type_info_ptr,
                    added_item.component.data.empty() ? nullptr : added_item.component.data.data());

                if (component_data_ptr == nullptr)
                {
                    cleanup_on_error(all_entities);
                    return unexpected(prefab_error::component_not_found);
                }

                if (type_info_ptr->descriptor.has_value() && type_info_ptr->descriptor->post_load != nullptr)
                {
                    type_info_ptr->descriptor->post_load(component_data_ptr);
                }
            }

            // 10. Apply field overrides
            struct post_load_target
            {
                void* component_data{nullptr};
                void (*post_load_fn)(void*){nullptr};
            };
            auto modified_components = vector<post_load_target>{};

            for (const auto& override_item : overrides.field_overrides)
            {
                // If override was delegated to a nested instance, it was already applied recursively
                if (!override_item.target.empty() && is_nested_host_local_id(override_item.target.front()))
                {
                    continue;
                }

                const auto target_entity =
                    find_entity_by_id_path(registry, root_entity, override_item.target, local_to_entity);
                if (target_entity == ecs::tombstone || !registry.is_valid(target_entity))
                {
                    cleanup_on_error(all_entities);
                    return unexpected(prefab_error::component_not_found);
                }

                const auto* type_info_ptr =
                    find_component_type_info(registry.type_registry(), override_item.component_name);
                if (type_info_ptr == nullptr)
                {
                    cleanup_on_error(all_entities);
                    return unexpected(prefab_error::component_not_found);
                }

                auto* component_data_ptr =
                    ecs::raw_registry_access::get_component_raw(registry, target_entity, type_info_ptr->index);
                if (component_data_ptr == nullptr)
                {
                    cleanup_on_error(all_entities);
                    return unexpected(prefab_error::component_not_found);
                }

                if (!type_info_ptr->descriptor.has_value())
                {
                    cleanup_on_error(all_entities);
                    return unexpected(prefab_error::field_not_found);
                }

                const auto* field_descriptor_ptr = type_info_ptr->descriptor->find_field(override_item.field_name);
                if (field_descriptor_ptr == nullptr)
                {
                    cleanup_on_error(all_entities);
                    return unexpected(prefab_error::field_not_found);
                }

                if (field_descriptor_ptr->set != nullptr)
                {
                    if (override_item.value_bytes.size() >= field_descriptor_ptr->size)
                    {
                        field_descriptor_ptr->set(component_data_ptr, override_item.value_bytes.data());
                    }
                    else if (field_descriptor_ptr->size <= max_stack_field_override_buffer_size)
                    {
                        alignas(alignof(max_align_t)) auto temp_buffer =
                            array<byte, max_stack_field_override_buffer_size>{};
                        tempest::memcpy(temp_buffer.data(), override_item.value_bytes.data(),
                                        override_item.value_bytes.size());
                        field_descriptor_ptr->set(component_data_ptr, temp_buffer.data());
                    }
                    else
                    {
                        auto temp_buffer = vector<byte>(field_descriptor_ptr->size, byte{0});
                        tempest::memcpy(temp_buffer.data(), override_item.value_bytes.data(),
                                        override_item.value_bytes.size());
                        field_descriptor_ptr->set(component_data_ptr, temp_buffer.data());
                    }
                }
                else if (field_descriptor_ptr->offset != ecs::field_descriptor::no_offset &&
                         !override_item.value_bytes.empty())
                {
                    tempest::memcpy(static_cast<byte*>(component_data_ptr) + field_descriptor_ptr->offset,
                                    override_item.value_bytes.data(),
                                    tempest::min(override_item.value_bytes.size(), field_descriptor_ptr->size));
                }

                if (type_info_ptr->descriptor->post_load != nullptr)
                {
                    auto already_recorded = false;
                    for (const auto& item : modified_components)
                    {
                        if (item.component_data == component_data_ptr)
                        {
                            already_recorded = true;
                            break;
                        }
                    }
                    if (!already_recorded)
                    {
                        modified_components.push_back({
                            .component_data = component_data_ptr,
                            .post_load_fn = type_info_ptr->descriptor->post_load,
                        });
                    }
                }
            }

            // Post-load callbacks executed once per modified component
            for (const auto& item : modified_components)
            {
                item.post_load_fn(item.component_data);
            }

            // 11. Remap entity_ref fields from template local IDs to runtime ecs::entity
            for (const auto ent : all_entities)
            {
                const auto types = ecs::raw_registry_access::get_component_types(registry, ent);
                for (const auto& t_info : types)
                {
                    if (t_info.descriptor.has_value())
                    {
                        auto* comp_ptr = ecs::raw_registry_access::get_component_raw(registry, ent, t_info.index);
                        if (comp_ptr == nullptr)
                        {
                            continue;
                        }
                        for (const auto& field : t_info.descriptor->fields)
                        {
                            if (field.kind == ecs::field_kind::entity_ref)
                            {
                                const auto ref_val = detail::get_field_val<uint64_t>(field, comp_ptr);
                                if (ref_val != static_cast<uint64_t>(ecs::tombstone) &&
                                    ref_val != static_cast<uint64_t>(ecs::null) && ref_val != 0)
                                {
                                    const auto iter = local_to_entity.find(ref_val);
                                    if (iter != local_to_entity.end())
                                    {
                                        detail::set_field_val(field, comp_ptr, iter->second);
                                    }
                                    else
                                    {
                                        const auto tomb = ecs::entity{ecs::tombstone};
                                        detail::set_field_val(field, comp_ptr, tomb);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 12. Return successful result
            auto result = prefab_instance_result{};
            result.root_entity = root_entity;
            result.all_entities = tempest::move(all_entities);
            result.local_to_entity = tempest::move(local_to_entity);
            result.success = true;

            return result;
        }
    } // namespace

    auto prefab_instantiator::instantiate(const prefab_template& tmpl, ecs::archetype_registry& registry,
                                          const prefab_overrides& overrides, const prefab_cache* cache,
                                          const guid& prefab_guid, entity_local_id instance_local_id)
        -> expected<prefab_instance_result, prefab_error>
    {
        auto initial_stack = vector<guid>{};
        if (prefab_guid != guid{})
        {
            initial_stack.push_back(prefab_guid);
        }
        return instantiate_impl(tmpl, registry, overrides, cache, prefab_guid, instance_local_id, initial_stack);
    }

    auto prefab_instantiator::instantiate(const guid& prefab_guid, const prefab_cache& cache,
                                          ecs::archetype_registry& registry, const prefab_overrides& overrides,
                                          entity_local_id instance_local_id)
        -> expected<prefab_instance_result, prefab_error>
    {
        auto result = optional<expected<prefab_instance_result, prefab_error>>{};
        const auto found = cache.with_template(prefab_guid, [&](const prefab_template& tmpl) {
            result = instantiate(tmpl, registry, overrides, &cache, prefab_guid, instance_local_id);
        });
        if (!found || !result.has_value())
        {
            return unexpected(prefab_error::template_not_found);
        }
        return tempest::move(*result);
    }
} // namespace tempest::scene
