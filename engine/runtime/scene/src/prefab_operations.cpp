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

        // Commit any pending overrides to live entities
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

        // Build updated template from live entity hierarchy
        auto updated_tmpl = prefab_template::from_entity(registry, root_entity);
        if (updated_tmpl.is_empty())
        {
            return unexpected(prefab_operation_error::invalid_entity);
        }

        // Write to disk if path is provided (before updating cache, ensuring atomicity)
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

        // Update cache
        cache.unregister_template(prefab_guid);
        cache.register_template(prefab_guid, updated_tmpl);

        // Assign prefab_member_component to any newly added entities in the immediate hierarchy
        const auto all_entities = collect_immediate_hierarchy(registry, root_entity);
        const auto records = updated_tmpl.get_records();
        for (auto index = size_t{0}; index < all_entities.size() && index < records.size(); ++index)
        {
            registry.assign_or_replace(all_entities[index], prefab_member_component{
                                                                .template_local_id = records[index].local_id,
                                                            });
        }

        // Clear overrides
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

    namespace
    {
        auto find_prefab_instance_roots(ecs::basic_archetype_registry& registry, const guid& prefab_guid)
            -> vector<ecs::entity>
        {
            auto roots = vector<ecs::entity>{};
            registry.each([&](const ecs::self_component& self, const prefab_instance_component& inst) {
                if (inst.prefab_guid == prefab_guid)
                {
                    roots.push_back(self.entity);
                }
            });
            return roots;
        }

        auto collect_instance_hierarchy(ecs::basic_archetype_registry& registry,
                                        ecs::entity root_entity,
                                        entity_local_id root_template_local_id,
                                        flat_unordered_map<uint64_t, ecs::entity>& out_members,
                                        vector<ecs::entity>& out_added_children) -> void
        {
            auto traverse = [&](auto& self, ecs::entity current_entity) -> void {
                const auto* member_comp = registry.try_get<prefab_member_component>(current_entity);
                if (member_comp != nullptr && member_comp->template_local_id.is_valid())
                {
                    out_members[member_comp->template_local_id.value] = current_entity;
                }
                else if (current_entity != root_entity)
                {
                    out_added_children.push_back(current_entity);
                }

                if (current_entity != root_entity && registry.has<prefab_instance_component>(current_entity))
                {
                    return;
                }

                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(current_entity);
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

            traverse(traverse, root_entity);

            if (out_members.find(root_template_local_id.value) == out_members.end())
            {
                out_members[root_template_local_id.value] = root_entity;
                registry.assign_or_replace(root_entity, prefab_member_component{
                                                            .template_local_id = root_template_local_id,
                                                        });
            }
        }

        auto destroy_removed_template_members(ecs::basic_archetype_registry& registry,
                                              ecs::entity root_entity,
                                              flat_unordered_map<uint64_t, ecs::entity>& existing_members,
                                              const flat_unordered_map<uint64_t, size_t>& records_by_local_id) -> void
        {
            auto entities_to_destroy = vector<ecs::entity>{};
            for (const auto& [local_id_val, member_ent] : existing_members)
            {
                if (member_ent != root_entity && records_by_local_id.find(local_id_val) == records_by_local_id.end())
                {
                    entities_to_destroy.push_back(member_ent);
                }
            }

            for (const auto dead_ent : entities_to_destroy)
            {
                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(dead_ent);
                if (rel != nullptr && rel->first_child != ecs::tombstone)
                {
                    auto curr_child = rel->first_child;
                    while (curr_child != ecs::tombstone && registry.is_valid(curr_child))
                    {
                        const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(curr_child);
                        const auto next_child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;

                        auto is_child_dead = false;
                        for (const auto candidate_dead : entities_to_destroy)
                        {
                            if (candidate_dead == curr_child)
                            {
                                is_child_dead = true;
                                break;
                            }
                        }
                        if (!is_child_dead)
                        {
                            registry.unlink(curr_child);
                            ecs::create_parent_child_relationship(registry, root_entity, curr_child);
                        }
                        curr_child = next_child;
                    }
                }
                registry.destroy(dead_ent);
            }

            for (auto iter = existing_members.begin(); iter != existing_members.end();)
            {
                if (!registry.is_valid(iter->second))
                {
                    iter = existing_members.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }

        auto spawn_missing_template_members(ecs::basic_archetype_registry& registry,
                                            flat_unordered_map<uint64_t, ecs::entity>& existing_members,
                                            span<const prefab_entity_record> records) -> void
        {
            for (const auto& rec : records)
            {
                if (existing_members.find(rec.local_id.value) == existing_members.end())
                {
                    const auto new_ent = registry.create();
                    if (!rec.name.empty())
                    {
                        registry.name(new_ent, rec.name);
                    }
                    registry.assign(new_ent, prefab_member_component{
                                                 .template_local_id = rec.local_id,
                                             });
                    existing_members[rec.local_id.value] = new_ent;
                }
            }
        }

        auto is_target_entity_matched(const id_path& target, entity_local_id local_id, bool is_root) -> bool
        {
            if (is_root)
            {
                return target.empty() || (target.size() == 1 && target[0] == local_id);
            }
            return !target.empty() && target.back() == local_id;
        }

        auto is_component_removed(const prefab_overrides& overrides,
                                  entity_local_id local_id,
                                  bool is_root,
                                  string_view component_name) -> bool
        {
            for (const auto& removed_item : overrides.removed_components)
            {
                if (is_target_entity_matched(removed_item.target, local_id, is_root) &&
                    detail::component_name_matches(component_name, removed_item.component_name))
                {
                    return true;
                }
            }
            return false;
        }

        auto find_field_override(const prefab_overrides& overrides,
                                 entity_local_id local_id,
                                 bool is_root,
                                 string_view component_name,
                                 string_view field_name) -> const field_override*
        {
            for (const auto& f_override : overrides.field_overrides)
            {
                if (is_target_entity_matched(f_override.target, local_id, is_root) &&
                    detail::component_name_matches(component_name, f_override.component_name) &&
                    f_override.field_name == field_name)
                {
                    return &f_override;
                }
            }
            return nullptr;
        }

        auto prune_removed_or_stale_components(ecs::basic_archetype_registry& registry,
                                               ecs::entity target_entity,
                                               const prefab_entity_record& rec,
                                               bool is_root,
                                               const prefab_overrides& overrides) -> void
        {
            const auto current_types = ecs::raw_registry_access::get_component_types(registry, target_entity);
            for (const auto& cur_info : current_types)
            {
                if (!cur_info.is_serializable)
                {
                    continue;
                }
                if (is_root && detail::is_transform_component(cur_info.name))
                {
                    continue;
                }

                if (is_component_removed(overrides, rec.local_id, is_root, cur_info.name))
                {
                    ecs::raw_registry_access::remove_raw(registry, target_entity, cur_info.index);
                    continue;
                }

                auto exists_in_template = false;
                for (const auto& template_comp : rec.components)
                {
                    if (template_comp.type_info != nullptr &&
                        detail::component_name_matches(template_comp.type_info->name, cur_info.name))
                    {
                        exists_in_template = true;
                        break;
                    }
                }

                if (!exists_in_template)
                {
                    auto is_explicitly_added = false;
                    for (const auto& added : overrides.added_components)
                    {
                        if (is_target_entity_matched(added.target, rec.local_id, is_root) &&
                            added.component.type_info != nullptr &&
                            detail::component_name_matches(added.component.type_info->name, cur_info.name))
                        {
                            is_explicitly_added = true;
                            break;
                        }
                    }

                    if (!is_explicitly_added)
                    {
                        ecs::raw_registry_access::remove_raw(registry, target_entity, cur_info.index);
                    }
                }
            }
        }

        auto apply_field_updates_to_component(void* comp_ptr,
                                              const staged_component& staged_comp,
                                              entity_local_id local_id,
                                              bool is_root,
                                              const prefab_overrides& overrides,
                                              bool is_new_assignment) -> void
        {
            if (comp_ptr == nullptr || !staged_comp.type_info->descriptor.has_value())
            {
                return;
            }

            for (const auto& field : staged_comp.type_info->descriptor->fields)
            {
                const auto* f_override =
                    find_field_override(overrides, local_id, is_root, staged_comp.type_info->name, field.name);

                if (f_override != nullptr && !f_override->value_bytes.empty())
                {
                    if (field.set != nullptr)
                    {
                        field.set(comp_ptr, f_override->value_bytes.data());
                    }
                    else if (field.offset != ecs::field_descriptor::no_offset)
                    {
                        tempest::memcpy(static_cast<byte*>(comp_ptr) + field.offset, f_override->value_bytes.data(),
                                        tempest::min(f_override->value_bytes.size(), field.size));
                    }
                }
                else if (!is_new_assignment)
                {
                    if (field.set != nullptr)
                    {
                        if (field.offset != ecs::field_descriptor::no_offset &&
                            field.offset + field.size <= staged_comp.data.size())
                        {
                            field.set(comp_ptr, staged_comp.data.data() + field.offset);
                        }
                        else if (field.get != nullptr)
                        {
                            alignas(alignof(max_align_t)) auto temp_buf = array<byte, 256>{};
                            if (field.size <= temp_buf.size())
                            {
                                field.get(staged_comp.data.data(), temp_buf.data());
                                field.set(comp_ptr, temp_buf.data());
                            }
                        }
                    }
                    else if (field.offset != ecs::field_descriptor::no_offset &&
                             field.offset + field.size <= staged_comp.data.size())
                    {
                        tempest::memcpy(static_cast<byte*>(comp_ptr) + field.offset,
                                        staged_comp.data.data() + field.offset, field.size);
                    }
                }
            }

            if (staged_comp.type_info->descriptor->post_load != nullptr)
            {
                staged_comp.type_info->descriptor->post_load(comp_ptr);
            }
        }

        auto apply_template_components(ecs::basic_archetype_registry& registry,
                                       ecs::entity target_entity,
                                       const prefab_entity_record& rec,
                                       bool is_root,
                                       const prefab_overrides& overrides) -> void
        {
            for (const auto& staged_comp : rec.components)
            {
                if (staged_comp.type_info == nullptr)
                {
                    continue;
                }

                if (is_root && detail::is_transform_component(staged_comp.type_info->name))
                {
                    continue;
                }

                if (is_component_removed(overrides, rec.local_id, is_root, staged_comp.type_info->name))
                {
                    continue;
                }

                if (!ecs::raw_registry_access::has(registry, target_entity, staged_comp.type_info->index))
                {
                    auto* comp_ptr = ecs::raw_registry_access::assign_raw(
                        registry, target_entity, *staged_comp.type_info,
                        staged_comp.data.empty() ? nullptr : staged_comp.data.data());

                    apply_field_updates_to_component(comp_ptr, staged_comp, rec.local_id, is_root, overrides, true);
                }
                else
                {
                    auto* comp_ptr = ecs::raw_registry_access::get_component_raw(
                        registry, target_entity, staged_comp.type_info->index);

                    apply_field_updates_to_component(comp_ptr, staged_comp, rec.local_id, is_root, overrides, false);
                }
            }

            for (const auto& added : overrides.added_components)
            {
                if (is_target_entity_matched(added.target, rec.local_id, is_root) &&
                    added.component.type_info != nullptr &&
                    !ecs::raw_registry_access::has(registry, target_entity, added.component.type_info->index))
                {
                    auto* comp_ptr = ecs::raw_registry_access::assign_raw(
                        registry, target_entity, *added.component.type_info,
                        added.component.data.empty() ? nullptr : added.component.data.data());

                    if (comp_ptr != nullptr && added.component.type_info->descriptor.has_value() &&
                        added.component.type_info->descriptor->post_load != nullptr)
                    {
                        added.component.type_info->descriptor->post_load(comp_ptr);
                    }
                }
            }
        }

        auto synchronize_all_member_components(ecs::basic_archetype_registry& registry,
                                               ecs::entity root_entity,
                                               const flat_unordered_map<uint64_t, ecs::entity>& existing_members,
                                               span<const prefab_entity_record> records,
                                               const prefab_overrides& overrides) -> void
        {
            for (const auto& rec : records)
            {
                const auto target_entity_it = existing_members.find(rec.local_id.value);
                if (target_entity_it == existing_members.end() || !registry.is_valid(target_entity_it->second))
                {
                    continue;
                }

                const auto target_entity = target_entity_it->second;
                const auto is_root = (target_entity == root_entity);

                if (!rec.name.empty())
                {
                    registry.name(target_entity, rec.name);
                }

                if (!is_root && registry.has<prefab_instance_component>(target_entity))
                {
                    continue;
                }

                prune_removed_or_stale_components(registry, target_entity, rec, is_root, overrides);
                apply_template_components(registry, target_entity, rec, is_root, overrides);
            }
        }

        auto reconstruct_template_hierarchy(ecs::basic_archetype_registry& registry,
                                            const flat_unordered_map<uint64_t, ecs::entity>& existing_members,
                                            span<const prefab_entity_record> records) -> void
        {
            for (const auto& rec : records)
            {
                const auto target_entity_it = existing_members.find(rec.local_id.value);
                if (target_entity_it == existing_members.end() || !registry.is_valid(target_entity_it->second))
                {
                    continue;
                }
                const auto target_entity = target_entity_it->second;

                if (!rec.parent_index.has_value())
                {
                    continue;
                }

                const auto parent_idx = *rec.parent_index;
                if (parent_idx >= records.size())
                {
                    continue;
                }

                const auto parent_local_id = records[parent_idx].local_id;
                const auto parent_ent_it = existing_members.find(parent_local_id.value);
                if (parent_ent_it == existing_members.end() || !registry.is_valid(parent_ent_it->second))
                {
                    continue;
                }

                const auto parent_ent = parent_ent_it->second;
                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(target_entity);
                if (rel == nullptr || rel->parent != parent_ent)
                {
                    if (rel != nullptr && rel->parent != ecs::tombstone)
                    {
                        registry.unlink(target_entity);
                    }
                    ecs::create_parent_child_relationship(registry, parent_ent, target_entity);
                }
            }
        }

        auto reanchor_instance_added_children(ecs::basic_archetype_registry& registry,
                                              ecs::entity root_entity,
                                              span<const ecs::entity> instance_added_children) -> void
        {
            for (const auto child_ent : instance_added_children)
            {
                if (!registry.is_valid(child_ent))
                {
                    continue;
                }

                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child_ent);
                if (rel == nullptr || rel->parent == ecs::tombstone || !registry.is_valid(rel->parent))
                {
                    if (rel != nullptr && rel->parent != ecs::tombstone)
                    {
                        registry.unlink(child_ent);
                    }
                    ecs::create_parent_child_relationship(registry, root_entity, child_ent);
                }
            }
        }

        auto remap_entity_references(ecs::basic_archetype_registry& registry,
                                     const flat_unordered_map<uint64_t, ecs::entity>& existing_members) -> void
        {
            for (const auto& [local_id_val, member_ent] : existing_members)
            {
                if (!registry.is_valid(member_ent))
                {
                    continue;
                }

                const auto types = ecs::raw_registry_access::get_component_types(registry, member_ent);
                for (const auto& t_info : types)
                {
                    if (!t_info.descriptor.has_value())
                    {
                        continue;
                    }

                    auto* comp_ptr = ecs::raw_registry_access::get_component_raw(registry, member_ent, t_info.index);
                    if (comp_ptr == nullptr)
                    {
                        continue;
                    }

                    for (const auto& field : t_info.descriptor->fields)
                    {
                        if (field.kind != ecs::field_kind::entity_ref)
                        {
                            continue;
                        }

                        const auto ref_val = detail::get_field_val<uint64_t>(field, comp_ptr);
                        if (ref_val != static_cast<uint64_t>(ecs::tombstone) &&
                            ref_val != static_cast<uint64_t>(ecs::null) && ref_val != 0)
                        {
                            const auto iter = existing_members.find(ref_val);
                            if (iter != existing_members.end() && registry.is_valid(iter->second))
                            {
                                detail::set_field_val(field, comp_ptr, iter->second);
                            }
                            else
                            {
                                detail::set_field_val(field, comp_ptr, ecs::entity{ecs::tombstone});
                            }
                        }
                    }
                }
            }
        }

        auto propagate_single_instance(ecs::basic_archetype_registry& registry,
                                       ecs::entity root_entity,
                                       const guid& prefab_guid,
                                       span<const prefab_entity_record> records,
                                       entity_local_id root_template_local_id) -> bool
        {
            if (!registry.is_valid(root_entity))
            {
                return false;
            }

            const auto* inst_comp_ptr = registry.try_get<prefab_instance_component>(root_entity);
            if (inst_comp_ptr == nullptr || inst_comp_ptr->prefab_guid != prefab_guid)
            {
                return false;
            }

            const auto current_overrides = inst_comp_ptr->overrides;
            const auto current_instance_local_id = inst_comp_ptr->instance_local_id;

            auto existing_members = flat_unordered_map<uint64_t, ecs::entity>{};
            auto instance_added_children = vector<ecs::entity>{};
            collect_instance_hierarchy(registry, root_entity, root_template_local_id, existing_members,
                                       instance_added_children);

            auto records_by_local_id = flat_unordered_map<uint64_t, size_t>{};
            for (auto record_index = size_t{0}; record_index < records.size(); ++record_index)
            {
                records_by_local_id[records[record_index].local_id.value] = record_index;
            }

            destroy_removed_template_members(registry, root_entity, existing_members, records_by_local_id);
            spawn_missing_template_members(registry, existing_members, records);
            synchronize_all_member_components(registry, root_entity, existing_members, records, current_overrides);
            reconstruct_template_hierarchy(registry, existing_members, records);
            reanchor_instance_added_children(registry, root_entity, instance_added_children);
            remap_entity_references(registry, existing_members);

            registry.assign_or_replace(root_entity, prefab_instance_component{
                                                        .prefab_guid = prefab_guid,
                                                        .instance_local_id = current_instance_local_id,
                                                        .overrides = current_overrides,
                                                    });
            return true;
        }
    } // namespace

    auto prefab_operations::propagate_template_updates(ecs::basic_archetype_registry& registry,
                                                       const guid& prefab_guid,
                                                       const prefab_cache& cache) -> size_t
    {
        const auto tmpl_opt = cache.find_template(prefab_guid);
        if (!tmpl_opt.has_value() || tmpl_opt->is_empty())
        {
            return 0;
        }

        const auto& tmpl = *tmpl_opt;
        const auto root_indices = tmpl.get_root_indices();
        if (root_indices.empty())
        {
            return 0;
        }

        const auto target_instance_roots = find_prefab_instance_roots(registry, prefab_guid);
        if (target_instance_roots.empty())
        {
            return 0;
        }

        const auto records = tmpl.get_records();
        const auto root_template_local_id = records[root_indices.front()].local_id;

        auto instances_updated_count = size_t{0};
        for (const auto root_entity : target_instance_roots)
        {
            if (propagate_single_instance(registry, root_entity, prefab_guid, records, root_template_local_id))
            {
                ++instances_updated_count;
            }
        }

        return instances_updated_count;
    }
} // namespace tempest::scene
