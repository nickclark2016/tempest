#include <tempest/scene/prefab_operations.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_codec.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/span.hpp>
#include <tempest/utility.hpp>

#include "scene_codec_detail.hpp"

namespace tempest::scene
{
    namespace
    {
        auto find_descendant_by_local_id(const ecs::basic_archetype_registry& registry, ecs::entity parent_entity,
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

                const auto* scene_comp = registry.try_get<scene_entity_component>(child_entity);
                if (scene_comp != nullptr && scene_comp->local_id == target_id)
                {
                    return child_entity;
                }

                const auto* inst_comp = registry.try_get<prefab_instance_component>(child_entity);
                if (inst_comp != nullptr && inst_comp->instance_local_id == target_id)
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

        auto resolve_target_entity(const ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                   const id_path& target) -> ecs::entity
        {
            if (target.empty())
            {
                return root_entity;
            }

            const auto root_matches = [&](entity_local_id check_id) -> bool {
                const auto* member = registry.try_get<prefab_member_component>(root_entity);
                if (member != nullptr && member->template_local_id == check_id)
                {
                    return true;
                }
                const auto* inst = registry.try_get<prefab_instance_component>(root_entity);
                if (inst != nullptr && inst->instance_local_id == check_id)
                {
                    return true;
                }
                const auto* sec = registry.try_get<scene_entity_component>(root_entity);
                return sec != nullptr && sec->local_id == check_id;
            };

            if (target.size() == 1)
            {
                if (root_matches(target[0]))
                {
                    return root_entity;
                }
                return find_descendant_by_local_id(registry, root_entity, target[0]);
            }

            auto current_entity = root_entity;
            auto segment_index = size_t{0};

            if (root_matches(target[0]))
            {
                segment_index = 1;
            }
            else
            {
                const auto first_descendant = find_descendant_by_local_id(registry, root_entity, target[0]);
                if (first_descendant == ecs::tombstone)
                {
                    return ecs::tombstone;
                }
                current_entity = first_descendant;
                segment_index = 1;
            }

            for (; segment_index < target.size(); ++segment_index)
            {
                const auto next_entity = find_descendant_by_local_id(registry, current_entity, target[segment_index]);
                if (next_entity == ecs::tombstone)
                {
                    return ecs::tombstone;
                }
                current_entity = next_entity;
            }

            return current_entity;
        }

        struct template_lookup_result
        {
            guid template_guid{};
            entity_local_id local_id{};
            bool is_root{false};
        };

        auto find_owning_template_and_local_id(const ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                               ecs::entity target_entity) -> template_lookup_result
        {
            if (target_entity == root_entity)
            {
                const auto* root_inst = registry.try_get<prefab_instance_component>(root_entity);
                const auto* root_member = registry.try_get<prefab_member_component>(root_entity);
                const auto local_id = (root_member != nullptr && root_member->template_local_id.is_valid())
                                          ? root_member->template_local_id
                                          : entity_local_id{};
                return template_lookup_result{
                    .template_guid = (root_inst != nullptr) ? root_inst->prefab_guid : guid{},
                    .local_id = local_id,
                    .is_root = true,
                };
            }

            const auto* target_inst = registry.try_get<prefab_instance_component>(target_entity);
            if (target_inst != nullptr)
            {
                const auto* member = registry.try_get<prefab_member_component>(target_entity);
                return template_lookup_result{
                    .template_guid = target_inst->prefab_guid,
                    .local_id = (member != nullptr) ? member->template_local_id : entity_local_id{},
                    .is_root = true,
                };
            }

            auto curr = target_entity;
            while (curr != ecs::tombstone && curr != root_entity && registry.is_valid(curr))
            {
                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(curr);
                if (rel == nullptr || rel->parent == ecs::tombstone)
                {
                    break;
                }
                curr = rel->parent;
                if (curr != root_entity && registry.has<prefab_instance_component>(curr))
                {
                    const auto* nested_inst = registry.try_get<prefab_instance_component>(curr);
                    const auto* member = registry.try_get<prefab_member_component>(target_entity);
                    return template_lookup_result{
                        .template_guid = (nested_inst != nullptr) ? nested_inst->prefab_guid : guid{},
                        .local_id = (member != nullptr) ? member->template_local_id : entity_local_id{},
                        .is_root = false,
                    };
                }
            }

            const auto* root_inst = registry.try_get<prefab_instance_component>(root_entity);
            const auto* member = registry.try_get<prefab_member_component>(target_entity);
            return template_lookup_result{
                .template_guid = (root_inst != nullptr) ? root_inst->prefab_guid : guid{},
                .local_id = (member != nullptr) ? member->template_local_id : entity_local_id{},
                .is_root = false,
            };
        }

        auto collect_immediate_hierarchy(const ecs::basic_archetype_registry& registry, ecs::entity root_entity)
            -> vector<ecs::entity>
        {
            auto all_entities = vector<ecs::entity>{};
            auto collect = [&](auto& self, ecs::entity ent) -> void {
                all_entities.push_back(ent);
                if (ent != root_entity && registry.has<prefab_instance_component>(ent))
                {
                    return;
                }
                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(ent);
                if (rel != nullptr && rel->first_child != ecs::tombstone)
                {
                    auto child = rel->first_child;
                    while (child != ecs::tombstone && registry.is_valid(child))
                    {
                        self(self, child);
                        const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                        child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
                    }
                }
            };
            collect(collect, root_entity);
            return all_entities;
        }
    } // namespace

    auto prefab_operations::save_as_prefab(ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                           const guid& prefab_guid, string_view file_path, prefab_cache& cache)
        -> expected<prefab_template, prefab_operation_error>
    {
        if (!registry.is_valid(root_entity))
        {
            return unexpected(prefab_operation_error::invalid_entity);
        }

        auto tmpl = prefab_template::from_entity(registry, root_entity);
        if (tmpl.is_empty())
        {
            return unexpected(prefab_operation_error::invalid_entity);
        }

        if (!file_path.empty())
        {
            const auto json_str = prefab_codec::serialize(tmpl, prefab_guid);
            const auto fs_path = filesystem::path{file_path};
            if (fs_path.has_parent_path())
            {
                filesystem::create_directories(fs_path.parent_path());
            }
            const auto byte_span = span<const byte>{reinterpret_cast<const byte*>(json_str.data()), json_str.size()};
            const auto write_res = write_file_from_bytes(fs_path, byte_span);
            if (!write_res.has_value())
            {
                return unexpected(prefab_operation_error::file_io_error);
            }
        }

        cache.unregister_template(prefab_guid);
        cache.register_template(prefab_guid, tmpl);

        const auto all_entities = collect_immediate_hierarchy(registry, root_entity);
        const auto records = tmpl.get_records();

        for (auto index = size_t{0}; index < all_entities.size() && index < records.size(); ++index)
        {
            registry.assign_or_replace(all_entities[index], prefab_member_component{
                                                                .template_local_id = records[index].local_id,
                                                            });
        }

        const auto root_local_id = records.empty() ? entity_local_id{1} : records[0].local_id;
        registry.assign_or_replace(root_entity, prefab_instance_component{
                                                    .prefab_guid = prefab_guid,
                                                    .instance_local_id = root_local_id,
                                                    .overrides = {},
                                                });

        return tmpl;
    }

    auto prefab_operations::revert_field_override(ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                                  const id_path& target, string_view component_name,
                                                  string_view field_name, const prefab_cache& cache)
        -> expected<void, prefab_operation_error>
    {
        if (!registry.is_valid(root_entity))
        {
            return unexpected(prefab_operation_error::invalid_entity);
        }
        const auto* inst_comp = registry.try_get<prefab_instance_component>(root_entity);
        if (inst_comp == nullptr)
        {
            return unexpected(prefab_operation_error::not_a_prefab_instance);
        }

        const auto target_entity = resolve_target_entity(registry, root_entity, target);
        if (target_entity == ecs::tombstone || !registry.is_valid(target_entity))
        {
            return unexpected(prefab_operation_error::target_not_found);
        }

        auto updated_inst = *inst_comp;
        auto* override_iter = updated_inst.overrides.field_overrides.end();
        for (auto* it = updated_inst.overrides.field_overrides.begin();
             it != updated_inst.overrides.field_overrides.end(); ++it)
        {
            if (it->target == target && detail::component_name_matches(it->component_name, component_name) &&
                it->field_name == field_name)
            {
                override_iter = it;
                break;
            }
        }
        if (override_iter == updated_inst.overrides.field_overrides.end())
        {
            return unexpected(prefab_operation_error::override_not_found);
        }

        const auto lookup = find_owning_template_and_local_id(registry, root_entity, target_entity);
        if (lookup.template_guid == guid{})
        {
            return unexpected(prefab_operation_error::template_not_found);
        }

        auto result_err = optional<prefab_operation_error>{};

        const auto found_tmpl = cache.with_template(lookup.template_guid, [&](const prefab_template& tmpl) {
            const auto* rec = lookup.is_root ? (lookup.local_id.is_valid() ? tmpl.find_record(lookup.local_id)
                                                                           : tmpl.get_root_record())
                                             : tmpl.find_record(lookup.local_id);
            if (rec == nullptr && lookup.is_root)
            {
                rec = tmpl.get_root_record();
            }
            if (rec == nullptr)
            {
                result_err = prefab_operation_error::target_not_found;
                return;
            }

            const staged_component* target_comp = nullptr;
            for (const auto& comp : rec->components)
            {
                if (comp.type_info != nullptr && detail::component_name_matches(comp.type_info->name, component_name))
                {
                    target_comp = &comp;
                    break;
                }
            }
            if (target_comp == nullptr || target_comp->type_info == nullptr)
            {
                result_err = prefab_operation_error::component_not_found;
                return;
            }

            const auto* type_info = target_comp->type_info;
            auto* live_comp_ptr =
                ecs::raw_registry_access::get_component_raw(registry, target_entity, type_info->index);
            if (live_comp_ptr == nullptr)
            {
                result_err = prefab_operation_error::component_not_found;
                return;
            }

            if (!type_info->descriptor.has_value())
            {
                result_err = prefab_operation_error::field_not_found;
                return;
            }

            const auto* field_desc = type_info->descriptor->find_field(field_name);
            if (field_desc == nullptr)
            {
                result_err = prefab_operation_error::field_not_found;
                return;
            }

            if (field_desc->set != nullptr)
            {
                if (field_desc->get != nullptr)
                {
                    static constexpr auto max_temp_buf_size = 256;

                    if (field_desc->size <= max_temp_buf_size)
                    {
                        alignas(alignof(max_align_t)) auto temp_buf = array<byte, max_temp_buf_size>{};
                        field_desc->get(target_comp->data.data(), temp_buf.data());
                        field_desc->set(live_comp_ptr, temp_buf.data());
                    }
                    else
                    {
                        auto temp_buf = vector<byte>(field_desc->size);
                        field_desc->get(target_comp->data.data(), temp_buf.data());
                        field_desc->set(live_comp_ptr, temp_buf.data());
                    }
                }
                else if (field_desc->offset != ecs::field_descriptor::no_offset &&
                         target_comp->data.size() >= field_desc->offset + field_desc->size)
                {
                    field_desc->set(live_comp_ptr, target_comp->data.data() + field_desc->offset);
                }
            }
            else if (field_desc->offset != ecs::field_descriptor::no_offset &&
                     target_comp->data.size() >= field_desc->offset + field_desc->size)
            {
                tempest::memcpy(static_cast<byte*>(live_comp_ptr) + field_desc->offset,
                                target_comp->data.data() + field_desc->offset, field_desc->size);
            }

            if (type_info->descriptor->post_load != nullptr)
            {
                type_info->descriptor->post_load(live_comp_ptr);
            }
        });

        if (!found_tmpl)
        {
            return unexpected(prefab_operation_error::template_not_found);
        }
        if (result_err.has_value())
        {
            return unexpected(*result_err);
        }

        updated_inst.overrides.field_overrides.erase(override_iter);
        registry.assign_or_replace(root_entity, tempest::move(updated_inst));
        return {};
    }

    auto prefab_operations::revert_added_component(ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                                   const id_path& target, string_view component_name)
        -> expected<void, prefab_operation_error>
    {
        if (!registry.is_valid(root_entity))
        {
            return unexpected(prefab_operation_error::invalid_entity);
        }
        const auto* inst_comp = registry.try_get<prefab_instance_component>(root_entity);
        if (inst_comp == nullptr)
        {
            return unexpected(prefab_operation_error::not_a_prefab_instance);
        }

        const auto target_entity = resolve_target_entity(registry, root_entity, target);
        if (target_entity == ecs::tombstone || !registry.is_valid(target_entity))
        {
            return unexpected(prefab_operation_error::target_not_found);
        }

        auto updated_inst = *inst_comp;
        auto* added_iter = updated_inst.overrides.added_components.end();
        for (auto* iter = updated_inst.overrides.added_components.begin();
             iter != updated_inst.overrides.added_components.end(); ++iter)
        {
            if (iter->target == target && iter->component.type_info != nullptr &&
                detail::component_name_matches(iter->component.type_info->name, component_name))
            {
                added_iter = iter;
                break;
            }
        }
        if (added_iter == updated_inst.overrides.added_components.end())
        {
            return unexpected(prefab_operation_error::override_not_found);
        }

        const auto type_index = added_iter->component.type_info->index;
        ecs::raw_registry_access::remove_raw(registry, target_entity, type_index);

        updated_inst.overrides.added_components.erase(added_iter);
        registry.assign_or_replace(root_entity, tempest::move(updated_inst));
        return {};
    }

    auto prefab_operations::revert_removed_component(ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                                     const id_path& target, string_view component_name,
                                                     const prefab_cache& cache)
        -> expected<void, prefab_operation_error>
    {
        if (!registry.is_valid(root_entity))
        {
            return unexpected(prefab_operation_error::invalid_entity);
        }
        const auto* inst_comp = registry.try_get<prefab_instance_component>(root_entity);
        if (inst_comp == nullptr)
        {
            return unexpected(prefab_operation_error::not_a_prefab_instance);
        }

        const auto target_entity = resolve_target_entity(registry, root_entity, target);
        if (target_entity == ecs::tombstone || !registry.is_valid(target_entity))
        {
            return unexpected(prefab_operation_error::target_not_found);
        }

        auto updated_inst = *inst_comp;
        auto* removed_iter = updated_inst.overrides.removed_components.end();
        for (auto* iter = updated_inst.overrides.removed_components.begin();
             iter != updated_inst.overrides.removed_components.end(); ++iter)
        {
            if (iter->target == target && detail::component_name_matches(iter->component_name, component_name))
            {
                removed_iter = iter;
                break;
            }
        }
        if (removed_iter == updated_inst.overrides.removed_components.end())
        {
            return unexpected(prefab_operation_error::override_not_found);
        }

        const auto lookup = find_owning_template_and_local_id(registry, root_entity, target_entity);
        if (lookup.template_guid == guid{})
        {
            return unexpected(prefab_operation_error::template_not_found);
        }

        auto result_err = optional<prefab_operation_error>{};

        const auto found_tmpl = cache.with_template(lookup.template_guid, [&](const prefab_template& tmpl) {
            const auto* rec = lookup.is_root ? (lookup.local_id.is_valid() ? tmpl.find_record(lookup.local_id)
                                                                           : tmpl.get_root_record())
                                             : tmpl.find_record(lookup.local_id);
            if (rec == nullptr && lookup.is_root)
            {
                rec = tmpl.get_root_record();
            }
            if (rec == nullptr)
            {
                result_err = prefab_operation_error::target_not_found;
                return;
            }

            const staged_component* target_comp = nullptr;
            for (const auto& comp : rec->components)
            {
                if (comp.type_info != nullptr && detail::component_name_matches(comp.type_info->name, component_name))
                {
                    target_comp = &comp;
                    break;
                }
            }
            if (target_comp == nullptr || target_comp->type_info == nullptr)
            {
                result_err = prefab_operation_error::component_not_found;
                return;
            }

            const auto* type_info = target_comp->type_info;
            auto* comp_ptr = ecs::raw_registry_access::assign_raw(
                registry, target_entity, *type_info, target_comp->data.empty() ? nullptr : target_comp->data.data());

            if (comp_ptr != nullptr && type_info->descriptor.has_value())
            {
                for (const auto& field : type_info->descriptor->fields)
                {
                    if (field.kind == ecs::field_kind::entity_ref)
                    {
                        const auto ref_val = detail::get_field_val<uint64_t>(field, comp_ptr);
                        if (ref_val != static_cast<uint64_t>(ecs::tombstone) &&
                            ref_val != static_cast<uint64_t>(ecs::null) && ref_val != 0)
                        {
                            auto target_path = id_path{};
                            target_path.push_back(entity_local_id{ref_val});
                            const auto target_ref_ent = resolve_target_entity(registry, root_entity, target_path);
                            if (target_ref_ent != ecs::tombstone && registry.is_valid(target_ref_ent))
                            {
                                detail::set_field_val(field, comp_ptr, target_ref_ent);
                            }
                            else
                            {
                                detail::set_field_val(field, comp_ptr, ecs::entity{ecs::tombstone});
                            }
                        }
                    }
                }

                if (type_info->descriptor->post_load != nullptr)
                {
                    type_info->descriptor->post_load(comp_ptr);
                }
            }
        });

        if (!found_tmpl)
        {
            return unexpected(prefab_operation_error::template_not_found);
        }
        if (result_err.has_value())
        {
            return unexpected(*result_err);
        }

        updated_inst.overrides.removed_components.erase(removed_iter);
        registry.assign_or_replace(root_entity, tempest::move(updated_inst));
        return {};
    }

    auto prefab_operations::apply_to_prefab(ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                            string_view file_path, prefab_cache& cache)
        -> expected<void, prefab_operation_error>
    {
        if (!registry.is_valid(root_entity))
        {
            return unexpected(prefab_operation_error::invalid_entity);
        }
        const auto* inst_comp_ptr = registry.try_get<prefab_instance_component>(root_entity);
        if (inst_comp_ptr == nullptr)
        {
            return unexpected(prefab_operation_error::not_a_prefab_instance);
        }
        const auto root_instance = *inst_comp_ptr;

        const auto prefab_guid = root_instance.prefab_guid;
        if (!cache.has_template(prefab_guid))
        {
            return unexpected(prefab_operation_error::template_not_found);
        }

        // 1. Commit any pending overrides to live entities
        for (const auto& added : root_instance.overrides.added_components)
        {
            const auto target_ent = resolve_target_entity(registry, root_entity, added.target);
            if (target_ent != ecs::tombstone && registry.is_valid(target_ent) && added.component.type_info != nullptr)
            {
                if (!ecs::raw_registry_access::has(registry, target_ent, added.component.type_info->index))
                {
                    auto* comp_ptr = ecs::raw_registry_access::assign_raw(
                        registry, target_ent, *added.component.type_info,
                        added.component.data.empty() ? nullptr : added.component.data.data());
                    if (comp_ptr != nullptr && added.component.type_info->descriptor.has_value() &&
                        added.component.type_info->descriptor->post_load != nullptr)
                    {
                        added.component.type_info->descriptor->post_load(comp_ptr);
                    }
                }
            }
        }

        for (const auto& removed : root_instance.overrides.removed_components)
        {
            const auto target_ent = resolve_target_entity(registry, root_entity, removed.target);
            if (target_ent != ecs::tombstone && registry.is_valid(target_ent))
            {
                const auto* type_info =
                    detail::find_component_type_info(registry.type_registry(), removed.component_name);
                if (type_info != nullptr)
                {
                    ecs::raw_registry_access::remove_raw(registry, target_ent, type_info->index);
                }
            }
        }

        for (const auto& field_over : root_instance.overrides.field_overrides)
        {
            const auto target_ent = resolve_target_entity(registry, root_entity, field_over.target);
            if (target_ent != ecs::tombstone && registry.is_valid(target_ent))
            {
                const auto* type_info =
                    detail::find_component_type_info(registry.type_registry(), field_over.component_name);
                if (type_info != nullptr && type_info->descriptor.has_value())
                {
                    auto* comp_ptr =
                        ecs::raw_registry_access::get_component_raw(registry, target_ent, type_info->index);
                    if (comp_ptr != nullptr)
                    {
                        const auto* field_desc = type_info->descriptor->find_field(field_over.field_name);
                        if (field_desc != nullptr)
                        {
                            if (field_desc->set != nullptr)
                            {
                                if (field_over.value_bytes.size() >= field_desc->size)
                                {
                                    field_desc->set(comp_ptr, field_over.value_bytes.data());
                                }
                            }
                            else if (field_desc->offset != ecs::field_descriptor::no_offset &&
                                     !field_over.value_bytes.empty())
                            {
                                tempest::memcpy(static_cast<byte*>(comp_ptr) + field_desc->offset,
                                                field_over.value_bytes.data(),
                                                tempest::min(field_over.value_bytes.size(), field_desc->size));
                            }
                            if (type_info->descriptor->post_load != nullptr)
                            {
                                type_info->descriptor->post_load(comp_ptr);
                            }
                        }
                    }
                }
            }
        }

        // 2. Build updated template from live entity hierarchy
        auto updated_tmpl = prefab_template::from_entity(registry, root_entity);
        if (updated_tmpl.is_empty())
        {
            return unexpected(prefab_operation_error::invalid_entity);
        }

        // 3. Write to disk if path is provided (before updating cache, ensuring atomicity)
        if (!file_path.empty())
        {
            const auto json_str = prefab_codec::serialize(updated_tmpl, prefab_guid);
            const auto fs_path = filesystem::path{file_path};
            if (fs_path.has_parent_path())
            {
                filesystem::create_directories(fs_path.parent_path());
            }
            const auto byte_span = span<const byte>{reinterpret_cast<const byte*>(json_str.data()), json_str.size()};
            const auto write_res = write_file_from_bytes(fs_path, byte_span);
            if (!write_res.has_value())
            {
                return unexpected(prefab_operation_error::file_io_error);
            }
        }

        // 4. Update cache
        cache.unregister_template(prefab_guid);
        cache.register_template(prefab_guid, updated_tmpl);

        // 5. Assign prefab_member_component to any newly added entities in the immediate hierarchy
        const auto all_entities = collect_immediate_hierarchy(registry, root_entity);
        const auto records = updated_tmpl.get_records();
        for (auto index = size_t{0}; index < all_entities.size() && index < records.size(); ++index)
        {
            registry.assign_or_replace(all_entities[index], prefab_member_component{
                                                                .template_local_id = records[index].local_id,
                                                            });
        }

        // 6. Clear overrides
        auto updated_inst = prefab_instance_component{
            .prefab_guid = prefab_guid,
            .instance_local_id = root_instance.instance_local_id,
            .overrides = {},
        };
        registry.assign_or_replace(root_entity, tempest::move(updated_inst));

        return {};
    }

    auto prefab_operations::unpack(ecs::basic_archetype_registry& registry, ecs::entity root_entity)
        -> expected<void, prefab_operation_error>
    {
        if (!registry.is_valid(root_entity))
        {
            return unexpected(prefab_operation_error::invalid_entity);
        }
        if (!registry.has<prefab_instance_component>(root_entity))
        {
            return unexpected(prefab_operation_error::not_a_prefab_instance);
        }

        auto entities_to_strip = vector<ecs::entity>{};
        entities_to_strip.push_back(root_entity);

        auto collect_immediate_members = [&](auto& self, ecs::entity current) -> void {
            const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(current);
            if (rel == nullptr || rel->first_child == ecs::tombstone)
            {
                return;
            }

            auto child = rel->first_child;
            while (child != ecs::tombstone && registry.is_valid(child))
            {
                if (!registry.has<prefab_instance_component>(child))
                {
                    entities_to_strip.push_back(child);
                    self(self, child);
                }
                const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
            }
        };

        collect_immediate_members(collect_immediate_members, root_entity);

        registry.remove<prefab_instance_component>(root_entity);

        for (const auto ent : entities_to_strip)
        {
            if (registry.has<prefab_member_component>(ent))
            {
                registry.remove<prefab_member_component>(ent);
            }
        }

        return {};
    }

    auto prefab_operations::unpack_completely(ecs::basic_archetype_registry& registry, ecs::entity root_entity)
        -> expected<void, prefab_operation_error>
    {
        if (!registry.is_valid(root_entity))
        {
            return unexpected(prefab_operation_error::invalid_entity);
        }
        if (!registry.has<prefab_instance_component>(root_entity))
        {
            return unexpected(prefab_operation_error::not_a_prefab_instance);
        }

        auto all_subtree = vector<ecs::entity>{};
        auto collect_all = [&](auto& self, ecs::entity current) -> void {
            all_subtree.push_back(current);
            const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(current);
            if (rel == nullptr || rel->first_child == ecs::tombstone)
            {
                return;
            }

            auto child = rel->first_child;
            while (child != ecs::tombstone && registry.is_valid(child))
            {
                self(self, child);
                const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
            }
        };

        collect_all(collect_all, root_entity);

        for (const auto ent : all_subtree)
        {
            if (registry.has<prefab_instance_component>(ent))
            {
                registry.remove<prefab_instance_component>(ent);
            }
            if (registry.has<prefab_member_component>(ent))
            {
                registry.remove<prefab_member_component>(ent);
            }
        }

        return {};
    }
} // namespace tempest::scene
