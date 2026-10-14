#include <tempest/scene/model_prefab.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/format.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/utility.hpp>

#include "scene_codec_detail.hpp"

namespace tempest::scene
{
    namespace
    {
        struct staged_node
        {
            ecs::entity ent{ecs::tombstone};
            string path{};
            optional<size_t> parent_index{nullopt};
        };

        auto compute_disambiguated_segment(const ecs::basic_archetype_registry& registry,
                                           ecs::entity child,
                                           size_t child_index,
                                           flat_unordered_map<string, size_t>& seen_names,
                                           flat_unordered_map<string, bool>& assigned_segments) -> string
        {
            const auto child_name_opt = registry.name(child);
            const auto base_segment = (child_name_opt.has_value() && !child_name_opt->empty())
                                          ? string(*child_name_opt)
                                          : tempest::format("Node_{}", child_index);

            if (!assigned_segments.contains(base_segment))
            {
                seen_names[base_segment] = 1;
                assigned_segments.insert({base_segment, true});
                return base_segment;
            }

            auto count = seen_names[base_segment] + 1;
            auto disambiguated = string{};
            do
            {
                disambiguated = tempest::format("{}_{}", base_segment, count);
                ++count;
            } while (assigned_segments.contains(disambiguated));

            seen_names[base_segment] = count;
            assigned_segments.insert({disambiguated, true});
            return disambiguated;
        }

        auto collect_model_nodes(const ecs::basic_archetype_registry& registry,
                                 ecs::entity root_entity,
                                 vector<staged_node>& out_nodes,
                                 flat_unordered_map<ecs::entity, size_t>& out_entity_to_index) -> void
        {
            const auto root_name_opt = registry.name(root_entity);
            const auto root_name = (root_name_opt.has_value() && !root_name_opt->empty())
                                       ? string(*root_name_opt)
                                       : string("Root");

            auto root_path = string("/");
            root_path += root_name;

            auto traverse = [&](auto& self, ecs::entity current_entity, string current_path,
                                optional<size_t> current_parent_index) -> void {
                const auto item_index = out_nodes.size();
                out_nodes.push_back(staged_node{
                    .ent = current_entity,
                    .path = tempest::move(current_path),
                    .parent_index = current_parent_index,
                });
                out_entity_to_index.insert({current_entity, item_index});

                if (current_entity != root_entity && registry.has<prefab_instance_component>(current_entity))
                {
                    return;
                }

                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(current_entity);
                if (rel == nullptr || rel->first_child == ecs::tombstone)
                {
                    return;
                }

                auto child = rel->first_child;
                auto child_index = size_t{0};
                auto seen_names = flat_unordered_map<string, size_t>{};
                auto assigned_segments = flat_unordered_map<string, bool>{};

                while (child != ecs::tombstone && registry.is_valid(child))
                {
                    const auto segment =
                        compute_disambiguated_segment(registry, child, child_index, seen_names, assigned_segments);
                    auto child_path = out_nodes[item_index].path;
                    child_path += "/";
                    child_path += segment;

                    self(self, child, tempest::move(child_path), item_index);

                    const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                    child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
                    ++child_index;
                }
            };

            traverse(traverse, root_entity, tempest::move(root_path), nullopt);
        }

        auto remap_staged_entity_refs(staged_component& comp,
                                      const void* raw_ptr,
                                      const flat_unordered_map<ecs::entity, size_t>& entity_to_index,
                                      span<const entity_local_id> member_local_ids) -> void
        {
            if (comp.type_info == nullptr || !comp.type_info->descriptor.has_value())
            {
                return;
            }

            for (const auto& field : comp.type_info->descriptor->fields)
            {
                if (field.kind == ecs::field_kind::entity_ref)
                {
                    const auto target_ent = detail::get_field_val<ecs::entity>(field, raw_ptr);
                    const auto iter = entity_to_index.find(target_ent);
                    if (iter != entity_to_index.end() && iter->second < member_local_ids.size())
                    {
                        const auto target_local_id = member_local_ids[iter->second];
                        detail::set_field_val(field, comp.data.data(), static_cast<ecs::entity>(target_local_id.value));
                    }
                    else
                    {
                        detail::set_field_val(field, comp.data.data(), ecs::tombstone);
                    }
                }
            }
        }

        auto extract_entity_staged_components(const ecs::basic_archetype_registry& registry,
                                              ecs::entity current_entity,
                                              bool is_root,
                                              const flat_unordered_map<ecs::entity, size_t>& entity_to_index,
                                              span<const entity_local_id> member_local_ids) -> vector<staged_component>
        {
            auto result = vector<staged_component>{};
            const auto comp_types = ecs::raw_registry_access::get_component_types(registry, current_entity);

            for (const auto& info : comp_types)
            {
                if (!info.is_serializable || !info.descriptor.has_value())
                {
                    continue;
                }

                // Requirement 7c: Root entity omits transform_component
                if (is_root && detail::is_transform_component(info.name))
                {
                    continue;
                }

                const auto* raw_ptr = ecs::raw_registry_access::get_component_raw(registry, current_entity, info.index);
                if (raw_ptr == nullptr)
                {
                    continue;
                }

                const auto* persistent_info = &registry.type_registry().type_info(info.index);
                auto comp = staged_component{
                    .type_info = persistent_info,
                    .data = vector<byte>(info.size, byte{0}),
                };
                tempest::memcpy(comp.data.data(), raw_ptr, info.size);

                remap_staged_entity_refs(comp, raw_ptr, entity_to_index, member_local_ids);
                result.push_back(tempest::move(comp));
            }

            return result;
        }
    } // namespace

    auto synthesize_model_prefab(const ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                 const guid& source_guid) -> prefab_template
    {
        if (!registry.is_valid(root_entity))
        {
            return prefab_template{};
        }

        auto nodes = vector<staged_node>{};
        auto entity_to_index = flat_unordered_map<ecs::entity, size_t>{};
        collect_model_nodes(registry, root_entity, nodes, entity_to_index);

        auto member_local_ids = vector<entity_local_id>{};
        member_local_ids.reserve(nodes.size());
        for (const auto& node : nodes)
        {
            member_local_ids.push_back(compute_model_prefab_local_id(source_guid, node.path));
        }

        auto records = vector<prefab_entity_record>{};
        records.reserve(nodes.size());

        for (auto index = size_t{0}; index < nodes.size(); ++index)
        {
            const auto& node = nodes[index];
            const auto current_entity = node.ent;
            const auto is_root = (current_entity == root_entity);

            auto rec = prefab_entity_record{
                .local_id = member_local_ids[index],
                .name = registry.name(current_entity).value_or(string{}),
                .parent_index = node.parent_index,
            };

            if (!is_root)
            {
                const auto* inst_comp = registry.try_get<prefab_instance_component>(current_entity);
                if (inst_comp != nullptr)
                {
                    rec.nested_instance = nested_prefab_descriptor{
                        .prefab_guid = inst_comp->prefab_guid,
                        .instance_local_id = inst_comp->instance_local_id,
                    };
                }
            }

            rec.components =
                extract_entity_staged_components(registry, current_entity, is_root, entity_to_index, member_local_ids);
            records.push_back(tempest::move(rec));
        }

        return prefab_template{tempest::move(records)};
    }
} // namespace tempest::scene
