#include <tempest/scene/prefab_template.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/utility.hpp>

#include "scene_codec_detail.hpp"

namespace tempest::scene
{
    prefab_template::prefab_template(vector<prefab_entity_record> records) : _records{tempest::move(records)}
    {
        _rebuild_indices();
    }

    auto prefab_template::_rebuild_indices() -> void
    {
        _local_to_index.clear();
        _root_indices.clear();

        for (auto index = size_t{0}; index < _records.size(); ++index)
        {
            const auto& rec = _records[index];
            if (rec.local_id.is_valid())
            {
                _local_to_index[rec.local_id.value] = index;
            }
            if (!rec.parent_index.has_value())
            {
                _root_indices.push_back(index);
            }
        }
    }

    auto prefab_template::entity_count() const noexcept -> size_t
    {
        return _records.size();
    }

    auto prefab_template::is_empty() const noexcept -> bool
    {
        return _records.empty();
    }

    auto prefab_template::get_records() const noexcept -> span<const prefab_entity_record>
    {
        return span<const prefab_entity_record>{_records.data(), _records.size()};
    }

    auto prefab_template::get_record(size_t index) const -> const prefab_entity_record*
    {
        if (index < _records.size())
        {
            return &_records[index];
        }
        return nullptr;
    }

    auto prefab_template::find_index(entity_local_id local_id) const noexcept -> optional<size_t>
    {
        const auto iter = _local_to_index.find(local_id.value);
        if (iter != _local_to_index.end())
        {
            return iter->second;
        }
        return nullopt;
    }

    auto prefab_template::find_record(entity_local_id local_id) const -> const prefab_entity_record*
    {
        const auto idx = find_index(local_id);
        if (idx.has_value())
        {
            return &_records[*idx];
        }
        return nullptr;
    }

    auto prefab_template::get_root_indices() const noexcept -> span<const size_t>
    {
        return span<const size_t>{_root_indices.data(), _root_indices.size()};
    }

    auto prefab_template::get_root_record() const noexcept -> const prefab_entity_record*
    {
        if (_root_indices.empty())
        {
            return nullptr;
        }
        return get_record(_root_indices.front());
    }

    auto prefab_template::from_staged_entities(span<const staged_entity> entities) -> prefab_template
    {
        auto records = vector<prefab_entity_record>{};
        records.reserve(entities.size());

        for (const auto& staged : entities)
        {
            auto record = prefab_entity_record{
                .local_id = staged.local_id,
                .name = staged.name,
                .parent_index = staged.parent_index,
                .components = staged.components,
                .nested_instance = nullopt,
            };
            records.push_back(tempest::move(record));
        }

        return prefab_template{tempest::move(records)};
    }

    auto prefab_template::from_entity(const ecs::archetype_registry& registry, ecs::entity root) -> prefab_template
    {
        if (!registry.is_valid(root))
        {
            return prefab_template{};
        }

        auto all_entities = vector<ecs::entity>{};
        auto entity_to_index = flat_unordered_map<ecs::entity, size_t>{};

        auto collect = [&](auto& self, ecs::entity ent) -> void {
            const auto idx = all_entities.size();
            all_entities.push_back(ent);
            entity_to_index.insert({ent, idx});

            // If this child is an instantiated nested prefab, do not flatten its internal hierarchy
            if (ent != root && registry.has<prefab_instance_component>(ent))
            {
                return;
            }

            const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(ent);
            if (rel != nullptr && rel->first_child != ecs::tombstone)
            {
                auto child = rel->first_child;
                while (child != ecs::tombstone)
                {
                    self(self, child);
                    const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                    child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
                }
            }
        };
        collect(collect, root);

        auto records = vector<prefab_entity_record>{};
        records.reserve(all_entities.size());

        auto max_existing_id = uint64_t{0};
        for (const auto ent : all_entities)
        {
            const auto* member = registry.try_get<prefab_member_component>(ent);
            const auto* sec = registry.try_get<scene_entity_component>(ent);
            if (member != nullptr && member->template_local_id.is_valid())
            {
                max_existing_id = tempest::max(max_existing_id, member->template_local_id.value);
            }
            else if (sec != nullptr && sec->local_id.is_valid())
            {
                max_existing_id = tempest::max(max_existing_id, sec->local_id.value);
            }
        }

        auto entity_local_ids = vector<entity_local_id>{};
        entity_local_ids.reserve(all_entities.size());
        auto next_generated_id = max_existing_id + 1;
        for (const auto ent : all_entities)
        {
            const auto* member = registry.try_get<prefab_member_component>(ent);
            const auto* sec = registry.try_get<scene_entity_component>(ent);
            if (member != nullptr && member->template_local_id.is_valid())
            {
                entity_local_ids.push_back(member->template_local_id);
            }
            else if (sec != nullptr && sec->local_id.is_valid())
            {
                entity_local_ids.push_back(sec->local_id);
            }
            else
            {
                entity_local_ids.push_back(entity_local_id{next_generated_id++});
            }
        }

        for (auto index = size_t{0}; index < all_entities.size(); ++index)
        {
            const auto ent = all_entities[index];
            auto rec = prefab_entity_record{};
            rec.local_id = entity_local_ids[index];

            // Name
            const auto name_opt = registry.name(ent);
            if (name_opt.has_value())
            {
                rec.name = string{*name_opt};
            }

            // Parent index
            if (ent == root)
            {
                rec.parent_index = nullopt;
            }
            else
            {
                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(ent);
                if (rel != nullptr && rel->parent != ecs::tombstone)
                {
                    const auto iter = entity_to_index.find(rel->parent);
                    if (iter != entity_to_index.end())
                    {
                        rec.parent_index = iter->second;
                    }
                }
            }

            // Nested prefab instance descriptor
            if (ent != root)
            {
                const auto* inst_comp = registry.try_get<prefab_instance_component>(ent);
                if (inst_comp != nullptr)
                {
                    rec.nested_instance = nested_prefab_descriptor{
                        .prefab_guid = inst_comp->prefab_guid,
                        .instance_local_id = inst_comp->instance_local_id,
                    };
                }
            }

            // Components
            const auto comp_types = ecs::raw_registry_access::get_component_types(registry, ent);
            for (const auto& info : comp_types)
            {
                if (!info.is_serializable || !info.is_authorable || !info.descriptor.has_value())
                {
                    continue;
                }

                if (ent == root && detail::is_transform_component(info.name))
                {
                    continue;
                }

                const auto* raw_ptr = ecs::raw_registry_access::get_component_raw(registry, ent, info.index);
                if (raw_ptr == nullptr)
                {
                    continue;
                }

                const auto* persistent_info = &registry.type_registry().type_info(info.index);
                auto comp = staged_component{};
                comp.type_info = persistent_info;
                comp.data.resize(info.size, byte{0});
                tempest::memcpy(comp.data.data(), raw_ptr, info.size);

                // Remap entity_ref fields from runtime ecs::entity to entity_local_id
                for (const auto& field : info.descriptor->fields)
                {
                    if (field.kind == ecs::field_kind::entity_ref)
                    {
                        const auto target_ent = detail::get_field_val<ecs::entity>(field, raw_ptr);
                        const auto iter = entity_to_index.find(target_ent);
                        if (iter != entity_to_index.end() && iter->second < entity_local_ids.size())
                        {
                            const auto target_local_id = entity_local_ids[iter->second];
                            detail::set_field_val(field, comp.data.data(),
                                                  static_cast<ecs::entity>(target_local_id.value));
                        }
                        else
                        {
                            detail::set_field_val(field, comp.data.data(), ecs::tombstone);
                        }
                    }
                }

                rec.components.push_back(tempest::move(comp));
            }

            records.push_back(tempest::move(rec));
        }

        return prefab_template{tempest::move(records)};
    }

    auto prefab_template_builder::add_entity(entity_local_id local_id, string name, optional<size_t> parent_index)
        -> size_t
    {
        const auto idx = _records.size();
        auto rec = prefab_entity_record{
            .local_id = local_id,
            .name = tempest::move(name),
            .parent_index = tempest::move(parent_index),
            .components = {},
            .nested_instance = nullopt,
        };
        _records.push_back(tempest::move(rec));
        return idx;
    }

    auto prefab_template_builder::add_component(size_t entity_index, staged_component comp) -> void
    {
        if (entity_index < _records.size())
        {
            _records[entity_index].components.push_back(tempest::move(comp));
        }
    }

    auto prefab_template_builder::add_component(size_t entity_index, const ecs::component_type_info* type_info,
                                                span<const byte> data) -> void
    {
        if (entity_index < _records.size())
        {
            auto comp = staged_component{};
            comp.type_info = type_info;
            comp.data.resize(data.size(), byte{0});
            if (!data.empty())
            {
                tempest::memcpy(comp.data.data(), data.data(), data.size());
            }
            _records[entity_index].components.push_back(tempest::move(comp));
        }
    }

    auto prefab_template_builder::set_nested_prefab(size_t entity_index, const guid& prefab_guid,
                                                    entity_local_id instance_local_id) -> void
    {
        if (entity_index < _records.size())
        {
            _records[entity_index].nested_instance = nested_prefab_descriptor{
                .prefab_guid = prefab_guid,
                .instance_local_id = instance_local_id,
            };
        }
    }

    auto prefab_template_builder::build() -> prefab_template
    {
        return prefab_template{tempest::move(_records)};
    }
} // namespace tempest::scene
