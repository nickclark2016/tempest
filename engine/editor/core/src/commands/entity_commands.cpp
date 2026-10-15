#include <tempest/commands/entity_commands.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/dynamic_entity_id_component.hpp>
#include <tempest/logger.hpp>
#include <tempest/math.hpp>
#include <tempest/random.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/utility.hpp>

namespace tempest::editor
{
    namespace
    {
        auto generate_local_id() -> scene::entity_local_id
        {
            auto device = random_device{};
            auto val = (static_cast<uint64_t>(device()) << 32U) | static_cast<uint64_t>(device()); // NOLINT
            auto local_id = scene::entity_local_id{val};
            while (!local_id.is_valid())
            {
                val = (static_cast<uint64_t>(device()) << 32U) | static_cast<uint64_t>(device()); // NOLINT
                local_id = scene::entity_local_id{val};
            }
            return local_id;
        }

        auto find_prefab_instance_root(const ecs::basic_archetype_registry& registry, ecs::entity ent) -> ecs::entity
        {
            auto curr = ent;
            while (curr != ecs::tombstone && curr != ecs::null && registry.is_valid(curr))
            {
                if (registry.has<scene::prefab_instance_component>(curr))
                {
                    return curr;
                }
                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(curr);
                curr = (rel != nullptr) ? rel->parent : ecs::tombstone;
            }
            return ecs::tombstone;
        }

        auto compute_prefab_id_path(const ecs::basic_archetype_registry& registry, ecs::entity root_ent,
                                    ecs::entity target_ent) -> scene::id_path
        {
            if (root_ent == target_ent)
            {
                if (const auto* pmc = registry.try_get<scene::prefab_member_component>(root_ent); pmc != nullptr)
                {
                    auto path = scene::id_path{};
                    path.push_back(pmc->template_local_id);
                    return path;
                }
                if (const auto* sec = registry.try_get<scene::scene_entity_component>(root_ent); sec != nullptr)
                {
                    auto path = scene::id_path{};
                    path.push_back(sec->local_id);
                    return path;
                }
                return scene::id_path{};
            }

            auto chain = vector<scene::entity_local_id>{};
            auto curr = target_ent;
            while (curr != ecs::tombstone && curr != root_ent && registry.is_valid(curr))
            {
                if (const auto* pmc = registry.try_get<scene::prefab_member_component>(curr); pmc != nullptr)
                {
                    chain.push_back(pmc->template_local_id);
                }
                else if (const auto* sec = registry.try_get<scene::scene_entity_component>(curr); sec != nullptr)
                {
                    chain.push_back(sec->local_id);
                }
                else if (const auto* dyn = registry.try_get<dynamic_entity_id_component>(curr); dyn != nullptr)
                {
                    chain.push_back(dyn->local_id);
                }
                else
                {
                    chain.push_back(scene::entity_local_id{static_cast<uint64_t>(curr)});
                }

                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(curr);
                curr = (rel != nullptr) ? rel->parent : ecs::tombstone;
            }

            auto path = scene::id_path{};
            for (auto i = chain.size(); i > 0; --i)
            {
                path.push_back(chain[i - 1]);
            }
            return path;
        }

        auto resolve_prefab_target_entity(const ecs::basic_archetype_registry& registry, ecs::entity root_entity,
                                          const scene::id_path& target) -> ecs::entity
        {
            if (target.empty())
            {
                return root_entity;
            }

            const auto root_matches = [&](scene::entity_local_id check_id) -> bool {
                const auto* member = registry.try_get<scene::prefab_member_component>(root_entity);
                if (member != nullptr && member->template_local_id == check_id)
                {
                    return true;
                }
                const auto* inst = registry.try_get<scene::prefab_instance_component>(root_entity);
                if (inst != nullptr && inst->instance_local_id == check_id)
                {
                    return true;
                }
                const auto* sec = registry.try_get<scene::scene_entity_component>(root_entity);
                return sec != nullptr && sec->local_id == check_id;
            };

            auto find_descendant = [](const ecs::basic_archetype_registry& reg, ecs::entity parent,
                                      scene::entity_local_id target_id) -> ecs::entity {
                auto rec = [&](auto& self, ecs::entity par) -> ecs::entity {
                    const auto* rel = reg.try_get<ecs::relationship_component<ecs::entity>>(par);
                    if (rel == nullptr)
                    {
                        return ecs::tombstone;
                    }
                    auto child = rel->first_child;
                    while (child != ecs::tombstone && reg.is_valid(child))
                    {
                        const auto* prefab_member = reg.try_get<scene::prefab_member_component>(child);
                        if (prefab_member != nullptr && prefab_member->template_local_id == target_id)
                        {
                            return child;
                        }
                        const auto* scene_entity = reg.try_get<scene::scene_entity_component>(child);
                        if (scene_entity != nullptr && scene_entity->local_id == target_id)
                        {
                            return child;
                        }
                        const auto* dyn_entity_id = reg.try_get<dynamic_entity_id_component>(child);
                        if (dyn_entity_id != nullptr && dyn_entity_id->local_id == target_id)
                        {
                            return child;
                        }
                        const auto first_child = self(self, child);
                        if (first_child != ecs::tombstone)
                        {
                            return first_child;
                        }
                        const auto* child_rel = reg.try_get<ecs::relationship_component<ecs::entity>>(child);
                        child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
                    }
                    return ecs::tombstone;
                };
                return rec(rec, parent);
            };

            if (target.size() == 1)
            {
                if (root_matches(target[0]))
                {
                    return root_entity;
                }
                return find_descendant(registry, root_entity, target[0]);
            }

            auto current_entity = root_entity;
            auto segment_index = size_t{0};
            if (root_matches(target[0]))
            {
                segment_index = 1;
            }
            else
            {
                const auto first_desc = find_descendant(registry, root_entity, target[0]);
                if (first_desc == ecs::tombstone)
                {
                    return ecs::tombstone;
                }
                current_entity = first_desc;
                segment_index = 1;
            }

            for (; segment_index < target.size(); ++segment_index)
            {
                const auto next_entity = find_descendant(registry, current_entity, target[segment_index]);
                if (next_entity == ecs::tombstone)
                {
                    return ecs::tombstone;
                }
                current_entity = next_entity;
            }

            return current_entity;
        }

        auto compute_world_matrix(const ecs::basic_archetype_registry& registry, ecs::entity ent) -> math::mat4<float>
        {
            auto chain = vector<ecs::entity>{};
            auto curr = ent;
            while (curr != ecs::tombstone && curr != ecs::null && registry.is_valid(curr))
            {
                chain.push_back(curr);
                const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(curr);
                curr = (rel != nullptr) ? rel->parent : ecs::tombstone;
            }

            auto world_mat = math::mat4<float>(1.0F);
            for (auto i = chain.size(); i > 0; --i)
            {
                const auto candidate = chain[i - 1];
                if (const auto* transform = registry.try_get<ecs::transform_component>(candidate); transform != nullptr)
                {
                    world_mat = world_mat * transform->matrix();
                }
            }
            return world_mat;
        }

        auto is_non_uniform_scale(const math::vec3<float>& scale) noexcept -> bool
        {
            return math::abs(scale.x - scale.y) > 1e-4F || math::abs(scale.x - scale.z) > 1e-4F; // NOLINT -- 1e-4F
        }

        auto is_internal_system_component(string_view name) noexcept -> bool
        {
            return tempest::contains(name, "self_component") || tempest::contains(name, "relationship_component") ||
                   tempest::contains(name, "scene_entity_component") ||
                   tempest::contains(name, "dynamic_entity_id_component") ||
                   tempest::contains(name, "prefab_member_component") ||
                   tempest::contains(name, "prefab_instance_component");
        }

        auto find_descendant_by_local_id(const ecs::basic_archetype_registry& registry, ecs::entity parent,
                                         scene::entity_local_id target_id, size_t depth = 0) -> ecs::entity
        {
            if (depth > 64)
            {
                return ecs::tombstone;
            }

            const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(parent);
            if (rel == nullptr)
            {
                return ecs::tombstone;
            }

            auto child = rel->first_child;
            auto count = size_t{0};
            while (child != ecs::tombstone && registry.is_valid(child) && count < 1024)
            {
                ++count;
                const auto* pmc = registry.try_get<scene::prefab_member_component>(child);
                if (pmc != nullptr && pmc->template_local_id == target_id)
                {
                    return child;
                }

                const auto* sec = registry.try_get<scene::scene_entity_component>(child);
                if (sec != nullptr && sec->local_id == target_id)
                {
                    return child;
                }

                const auto* dyn = registry.try_get<dynamic_entity_id_component>(child);
                if (dyn != nullptr && dyn->local_id == target_id)
                {
                    return child;
                }

                const auto found = find_descendant_by_local_id(registry, child, target_id, depth + 1);
                if (found != ecs::tombstone)
                {
                    return found;
                }

                const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
            }

            return ecs::tombstone;
        }

        auto capture_snapshot(const ecs::basic_archetype_registry& registry, ecs::entity ent, bool recursive)
            -> destroy_entity_command::entity_snapshot
        {
            auto snapshot = destroy_entity_command::entity_snapshot{};

            auto rec = [&](auto& self, ecs::entity current, bool recurse) -> size_t {
                auto node = destroy_entity_command::entity_node{};
                const auto name_opt = registry.name(current);
                node.name = name_opt.has_value() ? string{*name_opt} : string{};

                if (const auto* sec = registry.try_get<scene::scene_entity_component>(current); sec != nullptr)
                {
                    node.scene = sec->scene;
                    node.scene_local_id = sec->local_id;
                }

                if (const auto* dyn = registry.try_get<dynamic_entity_id_component>(current); dyn != nullptr)
                {
                    node.dynamic_local_id = dyn->local_id;
                }

                if (const auto* pmc = registry.try_get<scene::prefab_member_component>(current); pmc != nullptr)
                {
                    node.prefab_template_local_id = pmc->template_local_id;
                }

                if (const auto* pic = registry.try_get<scene::prefab_instance_component>(current); pic != nullptr)
                {
                    node.prefab_instance = *pic;
                }

                const auto types = ecs::raw_registry_access::get_component_types(registry, current);
                for (const auto& info : types)
                {
                    if (!info.should_duplicate || is_internal_system_component(info.name))
                    {
                        continue;
                    }

                    if (info.size > 0)
                    {
                        const auto* raw_ptr =
                            ecs::raw_registry_access::get_component_raw(registry, current, info.index);
                        if (raw_ptr != nullptr)
                        {
                            auto bytes = vector<byte>(info.size);
                            tempest::memcpy(bytes.data(), raw_ptr, info.size);
                            node.components.push_back({.type_index = info.index, .bytes = tempest::move(bytes)});
                        }
                    }
                }

                const auto node_idx = snapshot.nodes.size();
                snapshot.nodes.push_back(tempest::move(node));

                if (recurse)
                {
                    const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(current);
                    if (rel != nullptr)
                    {
                        auto child = rel->first_child;
                        while (child != ecs::tombstone && registry.is_valid(child))
                        {
                            const auto child_idx = self(self, child, true);
                            snapshot.nodes[node_idx].children.push_back(child_idx);
                            const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                            child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
                        }
                    }
                }

                return node_idx;
            };

            snapshot.root_index = rec(rec, ent, recursive);
            return snapshot;
        }

        auto restore_snapshot(ecs::basic_archetype_registry& registry,
                              const destroy_entity_command::entity_snapshot& snapshot) -> ecs::entity
        {
            if (snapshot.nodes.empty())
            {
                return ecs::tombstone;
            }

            auto rec = [&](auto& self, size_t node_idx) -> ecs::entity {
                const auto& node = snapshot.nodes[node_idx];
                const auto ent = registry.create();
                if (!node.name.empty())
                {
                    registry.name(ent, node.name);
                }

                if (node.scene_local_id.has_value())
                {
                    registry.assign(ent, scene::scene_entity_component{
                                             .scene = node.scene,
                                             .local_id = *node.scene_local_id,
                                         });
                }

                if (node.dynamic_local_id.has_value())
                {
                    registry.assign(ent, dynamic_entity_id_component{
                                             .local_id = *node.dynamic_local_id,
                                         });
                }

                if (node.prefab_template_local_id.has_value())
                {
                    registry.assign(ent, scene::prefab_member_component{
                                             .template_local_id = *node.prefab_template_local_id,
                                         });
                }

                if (node.prefab_instance.has_value())
                {
                    registry.assign(ent, *node.prefab_instance);
                }

                for (const auto& comp : node.components)
                {
                    const auto& info = registry.type_registry().type_info(comp.type_index);
                    auto* ptr = ecs::raw_registry_access::assign_raw(registry, ent, info, comp.bytes.data());
                    if (info.descriptor.has_value() && info.descriptor->post_load != nullptr)
                    {
                        info.descriptor->post_load(ptr);
                    }
                }

                for (const auto child_idx : node.children)
                {
                    const auto child_ent = self(self, child_idx);
                    registry.reparent(child_ent, ent);
                }

                return ent;
            };

            return rec(rec, snapshot.root_index);
        }

        auto destroy_subtree(ecs::basic_archetype_registry& registry, ecs::entity ent) -> void
        {
            const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(ent);
            if (rel != nullptr)
            {
                auto child = rel->first_child;
                while (child != ecs::tombstone && registry.is_valid(child))
                {
                    const auto* const next = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                    const auto next_child = (next != nullptr) ? next->next_sibling : ecs::tombstone;
                    destroy_subtree(registry, child);
                    child = next_child;
                }
            }
            registry.destroy(ent);
        }

        template <typename T>
        void set_field_value(const ecs::field_descriptor& field, void* comp_ptr, const T& val)
        {
            if (field.set != nullptr)
            {
                field.set(comp_ptr, &val);
            }
            else if (field.offset != ecs::field_descriptor::no_offset)
            {
                tempest::memcpy(static_cast<byte*>(comp_ptr) + field.offset, &val, sizeof(T));
            }
        }

        void deserialize_json_field(const ecs::field_descriptor& field, json_value val, void* comp_ptr)
        {
            using ecs::field_kind;
            switch (field.kind)
            {
            case field_kind::boolean: {
                if (val.is_bool())
                {
                    set_field_value(field, comp_ptr, *val.as_bool());
                }
                break;
            }
            case field_kind::int32: {
                if (val.is_int() || val.is_number())
                {
                    set_field_value(field, comp_ptr, static_cast<int32_t>(*val.as_int64()));
                }
                break;
            }
            case field_kind::uint32: {
                if (val.is_uint() || val.is_number())
                {
                    set_field_value(field, comp_ptr, static_cast<uint32_t>(*val.as_uint64()));
                }
                break;
            }
            case field_kind::int64: {
                if (val.is_int() || val.is_number())
                {
                    set_field_value(field, comp_ptr, *val.as_int64());
                }
                break;
            }
            case field_kind::uint64: {
                if (val.is_uint() || val.is_number())
                {
                    set_field_value(field, comp_ptr, *val.as_uint64());
                }
                break;
            }
            case field_kind::float32: {
                if (val.is_real() || val.is_number())
                {
                    set_field_value(field, comp_ptr, *val.as_float());
                }
                break;
            }
            case field_kind::float64: {
                if (val.is_real() || val.is_number())
                {
                    set_field_value(field, comp_ptr, *val.as_double());
                }
                break;
            }
            case field_kind::vec2: {
                if (val.is_array())
                {
                    const auto arr = *val.as_array();
                    if (arr.size() >= 2)
                    {
                        const auto vec =
                            math::vec2<float>{arr[0].as_float().value_or(0.0F), arr[1].as_float().value_or(0.0F)};
                        set_field_value(field, comp_ptr, vec);
                    }
                }
                break;
            }
            case field_kind::vec3:
            case field_kind::color3: {
                if (val.is_array())
                {
                    const auto arr = *val.as_array();
                    if (arr.size() >= 3)
                    {
                        const auto vec =
                            math::vec3<float>{arr[0].as_float().value_or(0.0F), arr[1].as_float().value_or(0.0F),
                                              arr[2].as_float().value_or(0.0F)};
                        set_field_value(field, comp_ptr, vec);
                    }
                }
                break;
            }
            case field_kind::vec4:
            case field_kind::color4: {
                if (val.is_array())
                {
                    const auto arr = *val.as_array();
                    if (arr.size() >= 4)
                    {
                        const auto vec =
                            math::vec4<float>{arr[0].as_float().value_or(0.0F), arr[1].as_float().value_or(0.0F),
                                              arr[2].as_float().value_or(0.0F), arr[3].as_float().value_or(0.0F)};
                        set_field_value(field, comp_ptr, vec);
                    }
                }
                break;
            }
            case field_kind::quat: {
                if (val.is_array())
                {
                    const auto arr = *val.as_array();
                    if (arr.size() >= 4)
                    {
                        const auto quat =
                            math::quat<float>{arr[0].as_float().value_or(0.0F), arr[1].as_float().value_or(0.0F),
                                              arr[2].as_float().value_or(0.0F), arr[3].as_float().value_or(1.0F)};
                        set_field_value(field, comp_ptr, quat);
                    }
                }
                break;
            }
            case field_kind::enumeration: {
                if (val.is_int() || val.is_number())
                {
                    const auto val_64 = *val.as_int64();
                    if (field.size == 1)
                    {
                        const auto val_8 = static_cast<int8_t>(val_64);
                        set_field_value(field, comp_ptr, val_8);
                    }
                    else if (field.size == 2)
                    {
                        const auto val_16 = static_cast<int16_t>(val_64);
                        set_field_value(field, comp_ptr, val_16);
                    }
                    else if (field.size == 4)
                    {
                        const auto val_32 = static_cast<int32_t>(val_64);
                        set_field_value(field, comp_ptr, val_32);
                    }
                    else
                    {
                        set_field_value(field, comp_ptr, val_64);
                    }
                }
                else if (val.is_string())
                {
                    const auto str = *val.as_string();
                    for (const auto& entry : field.enum_entries)
                    {
                        if (entry.name == str)
                        {
                            if (field.size == 1)
                            {
                                const auto val_8 = static_cast<int8_t>(entry.value);
                                set_field_value(field, comp_ptr, val_8);
                            }
                            else if (field.size == 2)
                            {
                                const auto val_16 = static_cast<int16_t>(entry.value);
                                set_field_value(field, comp_ptr, val_16);
                            }
                            else if (field.size == 4)
                            {
                                const auto val_32 = static_cast<int32_t>(entry.value);
                                set_field_value(field, comp_ptr, val_32);
                            }
                            else
                            {
                                set_field_value(field, comp_ptr, entry.value);
                            }
                            break;
                        }
                    }
                }
                break;
            }
            default:
                break;
            }
        }
    } // namespace

    //==============================================================================
    // create_entity_command
    //==============================================================================

    create_entity_command::create_entity_command(string_view name) : _name{name}
    {
    }

    create_entity_command::create_entity_command(scene::scene_handle target_scene, string_view name)
        : _scene{target_scene}, _name{name}
    {
    }

    auto create_entity_command::apply(command_context& ctx) -> void
    {
        if (!_executed)
        {
            if (!_scene.is_valid() && ctx.scene_manager->get_active_scene().is_valid())
            {
                _scene = ctx.scene_manager->get_active_scene();
            }

            const auto ent = ctx.registry->create();
            if (!_name.empty())
            {
                ctx.registry->name(ent, _name);
            }

            const auto local_id = generate_local_id();
            if (_scene.is_valid())
            {
                ctx.registry->assign(ent, scene::scene_entity_component{
                                              .scene = _scene,
                                              .local_id = local_id,
                                          });
                ctx.scene_manager->add_root_entity(_scene, ent);
                _created_address.scene = _scene;
                _created_address.id_path.push_back(local_id);
            }
            else
            {
                ctx.registry->assign(ent, dynamic_entity_id_component{
                                              .local_id = local_id,
                                          });
                _created_address.scene = {};
                _created_address.id_path.push_back(local_id);
            }
            _executed = true;
        }
        else
        {
            const auto ent = ctx.registry->create();
            if (!_name.empty())
            {
                ctx.registry->name(ent, _name);
            }

            if (_scene.is_valid())
            {
                ctx.registry->assign(ent, scene::scene_entity_component{
                                              .scene = _scene,
                                              .local_id = _created_address.id_path[0],
                                          });
                ctx.scene_manager->add_root_entity(_scene, ent);
            }
            else
            {
                ctx.registry->assign(ent, dynamic_entity_id_component{
                                              .local_id = _created_address.id_path[0],
                                          });
            }
        }

        if (_scene.is_valid())
        {
            ctx.scene_manager->set_dirty(_scene, true);
        }
    }

    auto create_entity_command::revert(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _created_address);
        if (ent != ecs::tombstone && ctx.registry->is_valid(ent))
        {
            if (_scene.is_valid())
            {
                ctx.scene_manager->remove_root_entity(_scene, ent);
                ctx.scene_manager->set_dirty(_scene, true);
            }
            ctx.registry->destroy(ent);
        }
    }

    auto create_entity_command::label() const noexcept -> string_view
    {
        return "Create Entity";
    }

    auto create_entity_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _scene;
    }

    auto create_entity_command::created_address() const noexcept -> const entity_address&
    {
        return _created_address;
    }

    //==============================================================================
    // destroy_entity_command
    //==============================================================================

    destroy_entity_command::destroy_entity_command(entity_address target, mode destroy_mode)
        : _target_addr{tempest::move(target)}, _mode{destroy_mode}
    {
    }

    auto destroy_entity_command::apply(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
        {
            return;
        }

        if (!_captured)
        {
            _scene = _target_addr.scene;
            const auto* rel = ctx.registry->try_get<ecs::relationship_component<ecs::entity>>(ent);
            if (rel != nullptr && rel->parent != ecs::tombstone && rel->parent != ecs::null &&
                ctx.registry->is_valid(rel->parent))
            {
                _prev_parent_addr = compute_entity_address(*ctx.registry, rel->parent);
                _was_root = false;
            }
            else
            {
                _was_root = true;
            }

            if (rel != nullptr && rel->next_sibling != ecs::tombstone && ctx.registry->is_valid(rel->next_sibling))
            {
                _prev_insert_before_addr = compute_entity_address(*ctx.registry, rel->next_sibling);
            }

            if (_mode == mode::recursive)
            {
                _snapshot = capture_snapshot(*ctx.registry, ent, true);
            }
            else
            {
                _snapshot = capture_snapshot(*ctx.registry, ent, false);
                auto child = (rel != nullptr) ? rel->first_child : ecs::tombstone;
                while (child != ecs::tombstone && ctx.registry->is_valid(child))
                {
                    _reparented_children.push_back({
                        .addr = compute_entity_address(*ctx.registry, child),
                        .entity = child,
                    });
                    const auto* child_rel = ctx.registry->try_get<ecs::relationship_component<ecs::entity>>(child);
                    child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
                }
            }
            _captured = true;
        }

        if (_mode == mode::reparent_children)
        {
            const auto parent_ent = _prev_parent_addr.has_value()
                                        ? resolve_entity_address(*ctx.registry, *ctx.scene_manager, *_prev_parent_addr)
                                        : ecs::tombstone;
            const auto sib_ent =
                _prev_insert_before_addr.has_value()
                    ? resolve_entity_address(*ctx.registry, *ctx.scene_manager, *_prev_insert_before_addr)
                    : ecs::tombstone;

            for (const auto& child_item : _reparented_children)
            {
                auto child_ent = child_item.entity;
                if (child_ent == ecs::tombstone || !ctx.registry->is_valid(child_ent))
                {
                    child_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, child_item.addr);
                }
                if (child_ent != ecs::tombstone && ctx.registry->is_valid(child_ent))
                {
                    ctx.registry->reparent(child_ent, parent_ent, sib_ent);
                    if (parent_ent == ecs::tombstone && _scene.is_valid())
                    {
                        ctx.scene_manager->add_root_entity(_scene, child_ent);
                    }
                }
            }
        }

        if (_was_root && _scene.is_valid())
        {
            ctx.scene_manager->remove_root_entity(_scene, ent);
        }

        if (_mode == mode::recursive)
        {
            destroy_subtree(*ctx.registry, ent);
        }
        else
        {
            ctx.registry->destroy(ent);
        }

        if (_scene.is_valid())
        {
            ctx.scene_manager->set_dirty(_scene, true);
        }
    }

    auto destroy_entity_command::revert(command_context& ctx) -> void
    {
        const auto parent_ent = _prev_parent_addr.has_value()
                                    ? resolve_entity_address(*ctx.registry, *ctx.scene_manager, *_prev_parent_addr)
                                    : ecs::tombstone;
        const auto sib_ent = _prev_insert_before_addr.has_value()
                                 ? resolve_entity_address(*ctx.registry, *ctx.scene_manager, *_prev_insert_before_addr)
                                 : ecs::tombstone;

        const auto restored_ent = restore_snapshot(*ctx.registry, _snapshot);

        if (_was_root)
        {
            if (_scene.is_valid())
            {
                ctx.scene_manager->add_root_entity(_scene, restored_ent);
            }
        }
        else
        {
            ctx.registry->reparent(restored_ent, parent_ent, sib_ent);
        }

        if (_mode == mode::reparent_children)
        {
            for (const auto& child_item : _reparented_children)
            {
                auto child_ent = child_item.entity;
                if (child_ent == ecs::tombstone || !ctx.registry->is_valid(child_ent))
                {
                    child_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, child_item.addr);
                }
                if (child_ent == ecs::tombstone && !child_item.addr.id_path.empty())
                {
                    const auto leaf_id = child_item.addr.id_path.back();
                    if (parent_ent != ecs::tombstone)
                    {
                        child_ent = find_descendant_by_local_id(*ctx.registry, parent_ent, leaf_id);
                    }
                    else if (_scene.is_valid())
                    {
                        const auto roots = ctx.scene_manager->get_root_entities(_scene);
                        if (roots.has_value())
                        {
                            for (const auto root_cand : *roots)
                            {
                                const auto* sec = ctx.registry->try_get<scene::scene_entity_component>(root_cand);
                                if (sec != nullptr && sec->local_id == leaf_id)
                                {
                                    child_ent = root_cand;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        for (const auto [self, dec] :
                             ctx.registry->with<ecs::self_component, dynamic_entity_id_component>())
                        {
                            if (dec.local_id == leaf_id)
                            {
                                child_ent = self.entity;
                                break;
                            }
                        }
                    }
                }

                if (child_ent != ecs::tombstone && ctx.registry->is_valid(child_ent))
                {
                    if (parent_ent == ecs::tombstone && _scene.is_valid())
                    {
                        ctx.scene_manager->remove_root_entity(_scene, child_ent);
                    }
                    ctx.registry->reparent(child_ent, restored_ent);
                }
            }
        }

        if (_scene.is_valid())
        {
            ctx.scene_manager->set_dirty(_scene, true);
        }
    }

    auto destroy_entity_command::label() const noexcept -> string_view
    {
        return "Destroy Entity";
    }

    auto destroy_entity_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _target_addr.scene;
    }

    //==============================================================================
    // duplicate_entity_command
    //==============================================================================

    duplicate_entity_command::duplicate_entity_command(entity_address target) : _target_addr{tempest::move(target)}
    {
    }

    auto duplicate_entity_command::apply(command_context& ctx) -> void
    {
        if (!_executed)
        {
            const auto target_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
            if (target_ent == ecs::tombstone || !ctx.registry->is_valid(target_ent))
            {
                return;
            }

            const auto* rel = ctx.registry->try_get<ecs::relationship_component<ecs::entity>>(target_ent);
            const auto parent = (rel != nullptr) ? rel->parent : ecs::tombstone;
            const auto next_sib = (rel != nullptr) ? rel->next_sibling : ecs::tombstone;

            if (parent != ecs::tombstone && parent != ecs::null && ctx.registry->is_valid(parent))
            {
                _parent_addr = compute_entity_address(*ctx.registry, parent);
                _was_root = false;
            }
            else
            {
                _was_root = true;
            }

            if (next_sib != ecs::tombstone && ctx.registry->is_valid(next_sib))
            {
                _insert_before_addr = compute_entity_address(*ctx.registry, next_sib);
            }

            auto clone_snapshot = destroy_entity_command::entity_snapshot{};

            auto clone_tree = [&](auto& self, ecs::entity src, bool is_root_copy) -> size_t {
                auto node = destroy_entity_command::entity_node{};
                const auto src_name_opt = ctx.registry->name(src);
                const auto src_name_view = src_name_opt.has_value() ? *src_name_opt : string_view{};
                auto str_name = string{src_name_view};
                if (is_root_copy)
                {
                    str_name += " Copy";
                }
                node.name = tempest::move(str_name);

                const auto new_id = generate_local_id();
                if (const auto* sec = ctx.registry->try_get<scene::scene_entity_component>(src); sec != nullptr)
                {
                    node.scene = sec->scene;
                    node.scene_local_id = new_id;
                }
                else
                {
                    node.dynamic_local_id = new_id;
                }

                if (const auto* pic = ctx.registry->try_get<scene::prefab_instance_component>(src); pic != nullptr)
                {
                    auto copy_pic = *pic;
                    copy_pic.instance_local_id = new_id;
                    node.prefab_instance = copy_pic;
                }

                const auto types = ecs::raw_registry_access::get_component_types(*ctx.registry, src);
                for (const auto& info : types)
                {
                    if (!info.should_duplicate || is_internal_system_component(info.name))
                    {
                        continue;
                    }

                    if (info.size > 0)
                    {
                        const auto* raw_ptr =
                            ecs::raw_registry_access::get_component_raw(*ctx.registry, src, info.index);
                        if (raw_ptr != nullptr)
                        {
                            auto bytes = vector<byte>(info.size);
                            tempest::memcpy(bytes.data(), raw_ptr, info.size);
                            node.components.push_back({.type_index = info.index, .bytes = tempest::move(bytes)});
                        }
                    }
                }

                const auto node_idx = clone_snapshot.nodes.size();
                clone_snapshot.nodes.push_back(tempest::move(node));

                const auto* src_rel = ctx.registry->try_get<ecs::relationship_component<ecs::entity>>(src);
                if (src_rel != nullptr)
                {
                    auto child = src_rel->first_child;
                    while (child != ecs::tombstone && ctx.registry->is_valid(child))
                    {
                        const auto child_idx = self(self, child, false);
                        clone_snapshot.nodes[node_idx].children.push_back(child_idx);
                        const auto* child_rel = ctx.registry->try_get<ecs::relationship_component<ecs::entity>>(child);
                        child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
                    }
                }

                return node_idx;
            };

            clone_snapshot.root_index = clone_tree(clone_tree, target_ent, true);
            _cloned_snapshot = tempest::move(clone_snapshot);
            const auto clone_ent = restore_snapshot(*ctx.registry, _cloned_snapshot);

            if (_was_root)
            {
                if (_target_addr.scene.is_valid())
                {
                    ctx.scene_manager->add_root_entity(_target_addr.scene, clone_ent);
                }
            }
            else
            {
                ctx.registry->reparent(clone_ent, parent, next_sib);
            }

            _cloned_addr = compute_entity_address(*ctx.registry, clone_ent);
            _executed = true;
        }
        else
        {
            const auto parent_ent = _parent_addr.has_value()
                                        ? resolve_entity_address(*ctx.registry, *ctx.scene_manager, *_parent_addr)
                                        : ecs::tombstone;
            const auto sib_ent = _insert_before_addr.has_value()
                                     ? resolve_entity_address(*ctx.registry, *ctx.scene_manager, *_insert_before_addr)
                                     : ecs::tombstone;

            const auto clone_ent = restore_snapshot(*ctx.registry, _cloned_snapshot);

            if (_was_root)
            {
                if (_target_addr.scene.is_valid())
                {
                    ctx.scene_manager->add_root_entity(_target_addr.scene, clone_ent);
                }
            }
            else
            {
                ctx.registry->reparent(clone_ent, parent_ent, sib_ent);
            }
        }
    }

    auto duplicate_entity_command::revert(command_context& ctx) -> void
    {
        const auto clone_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _cloned_addr);
        if (clone_ent != ecs::tombstone && ctx.registry->is_valid(clone_ent))
        {
            if (_was_root && _target_addr.scene.is_valid())
            {
                ctx.scene_manager->remove_root_entity(_target_addr.scene, clone_ent);
            }
            destroy_subtree(*ctx.registry, clone_ent);
        }
    }

    auto duplicate_entity_command::label() const noexcept -> string_view
    {
        return "Duplicate Entity";
    }

    auto duplicate_entity_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _target_addr.scene;
    }

    auto duplicate_entity_command::cloned_address() const noexcept -> const entity_address&
    {
        return _cloned_addr;
    }

    //==============================================================================
    // rename_entity_command
    //==============================================================================

    rename_entity_command::rename_entity_command(entity_address target, string_view new_name)
        : _target_addr{tempest::move(target)}, _new_name{new_name}
    {
    }

    auto rename_entity_command::apply(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent != ecs::tombstone && ctx.registry->is_valid(ent))
        {
            if (!_captured_old_name)
            {
                const auto name_opt = ctx.registry->name(ent);
                _old_name = name_opt.has_value() ? string{*name_opt} : string{};
                _captured_old_name = true;
            }
            ctx.registry->name(ent, _new_name);
        }
    }

    auto rename_entity_command::revert(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent != ecs::tombstone && ctx.registry->is_valid(ent))
        {
            ctx.registry->name(ent, _old_name);
        }
    }

    auto rename_entity_command::label() const noexcept -> string_view
    {
        return "Rename Entity";
    }

    auto rename_entity_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _target_addr.scene;
    }

    auto rename_entity_command::old_name() const noexcept -> string_view
    {
        return string_view{_old_name};
    }

    auto rename_entity_command::new_name() const noexcept -> string_view
    {
        return string_view{_new_name};
    }

    //==============================================================================
    // reparent_entity_command
    //==============================================================================

    reparent_entity_command::reparent_entity_command(entity_address child, optional<entity_address> new_parent,
                                                     optional<entity_address> insert_before,
                                                     bool preserve_world_transform)
        : _child_addr{tempest::move(child)}, _new_parent_addr{tempest::move(new_parent)},
          _insert_before_addr{tempest::move(insert_before)}, _preserve_world_transform{preserve_world_transform}
    {
    }

    auto reparent_entity_command::apply(command_context& ctx) -> void
    {
        const auto child = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _child_addr);
        if (child == ecs::tombstone || !ctx.registry->is_valid(child))
        {
            return;
        }

        if (!_captured)
        {
            const auto* rel = ctx.registry->try_get<ecs::relationship_component<ecs::entity>>(child);
            if (rel != nullptr && rel->parent != ecs::tombstone && rel->parent != ecs::null &&
                ctx.registry->is_valid(rel->parent))
            {
                _prev_parent_addr = compute_entity_address(*ctx.registry, rel->parent);
                _was_root = false;
            }
            else
            {
                _was_root = true;
            }

            if (rel != nullptr && rel->next_sibling != ecs::tombstone && ctx.registry->is_valid(rel->next_sibling))
            {
                _prev_insert_before_addr = compute_entity_address(*ctx.registry, rel->next_sibling);
            }

            if (const auto* transform = ctx.registry->try_get<ecs::transform_component>(child); transform != nullptr)
            {
                _prev_position = transform->position();
                _prev_rotation = transform->rotation_quat();
                _prev_scale = transform->scale();
                _has_transform = true;
            }
            _captured = true;
        }

        const auto new_parent = _new_parent_addr.has_value()
                                    ? resolve_entity_address(*ctx.registry, *ctx.scene_manager, *_new_parent_addr)
                                    : ecs::tombstone;
        const auto insert_before = _insert_before_addr.has_value()
                                       ? resolve_entity_address(*ctx.registry, *ctx.scene_manager, *_insert_before_addr)
                                       : ecs::tombstone;

        if (_preserve_world_transform && _has_transform)
        {
            const auto child_world = compute_world_matrix(*ctx.registry, child);
            const auto parent_world = (new_parent != ecs::tombstone && ctx.registry->is_valid(new_parent))
                                          ? compute_world_matrix(*ctx.registry, new_parent)
                                          : math::mat4<float>(1.0F);

            const auto new_local_mat = math::inverse(parent_world) * child_world;
            auto new_pos = math::vec3<float>{};
            auto new_rot = math::quat<float>{};
            auto new_sca = math::vec3<float>{};

            if (math::decompose(new_local_mat, new_pos, new_rot, new_sca))
            {
                if (ctx.registry->has<ecs::transform_component>(child))
                {
                    auto transform = ctx.registry->get<ecs::transform_component>(child);
                    transform.position(new_pos);
                    transform.rotation(new_rot);
                    transform.scale(new_sca);
                    ctx.registry->assign_or_replace(child, transform);
                }

                if (new_parent != ecs::tombstone && ctx.registry->is_valid(new_parent))
                {
                    if (const auto* ptf = ctx.registry->try_get<ecs::transform_component>(new_parent); ptf != nullptr)
                    {
                        if (is_non_uniform_scale(ptf->scale()) && ctx.log != nullptr)
                        {
                            ctx.log->warn("Reparenting under parent with non-uniform scale and rotation may introduce "
                                          "shear distortion.");
                        }
                    }
                }
            }
        }

        ctx.registry->reparent(child, new_parent, insert_before);

        if (_child_addr.scene.is_valid())
        {
            if (_was_root && new_parent != ecs::tombstone)
            {
                ctx.scene_manager->remove_root_entity(_child_addr.scene, child);
            }
            else if (!_was_root && new_parent == ecs::tombstone)
            {
                ctx.scene_manager->add_root_entity(_child_addr.scene, child);
            }
        }
    }

    auto reparent_entity_command::revert(command_context& ctx) -> void
    {
        const auto child = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _child_addr);
        if (child == ecs::tombstone || !ctx.registry->is_valid(child))
        {
            return;
        }

        const auto prev_p = _prev_parent_addr.has_value()
                                ? resolve_entity_address(*ctx.registry, *ctx.scene_manager, *_prev_parent_addr)
                                : ecs::tombstone;
        const auto prev_sib = _prev_insert_before_addr.has_value()
                                  ? resolve_entity_address(*ctx.registry, *ctx.scene_manager, *_prev_insert_before_addr)
                                  : ecs::tombstone;

        ctx.registry->reparent(child, prev_p, prev_sib);

        if (_has_transform && ctx.registry->has<ecs::transform_component>(child))
        {
            auto transform = ctx.registry->get<ecs::transform_component>(child);
            transform.position(_prev_position);
            transform.rotation(_prev_rotation);
            transform.scale(_prev_scale);
            ctx.registry->assign_or_replace(child, transform);
        }

        if (_child_addr.scene.is_valid())
        {
            if (_was_root)
            {
                ctx.scene_manager->add_root_entity(_child_addr.scene, child);
            }
            else
            {
                ctx.scene_manager->remove_root_entity(_child_addr.scene, child);
            }
        }
    }

    auto reparent_entity_command::label() const noexcept -> string_view
    {
        return "Reparent Entity";
    }

    auto reparent_entity_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _child_addr.scene;
    }

    //==============================================================================
    // add_component_command
    //==============================================================================

    add_component_command::add_component_command(entity_address target, uint32_t type_index)
        : _target_addr{tempest::move(target)}, _type_index{type_index}
    {
    }

    add_component_command::add_component_command(entity_address target, string_view type_name)
        : _target_addr{tempest::move(target)}, _type_name{type_name}
    {
    }

    auto add_component_command::apply(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
        {
            return;
        }

        if (_type_index == invalid_type_index && !_type_name.empty())
        {
            const auto opt_idx = ctx.registry->type_registry().type_index(_type_name);
            if (opt_idx.has_value())
            {
                _type_index = *opt_idx;
            }
        }

        if (_type_index == invalid_type_index)
        {
            return;
        }

        const auto& info = ctx.registry->type_registry().type_info(_type_index);
        auto* comp_ptr = ecs::raw_registry_access::assign_raw(*ctx.registry, ent, info);
        if (info.descriptor.has_value() && info.descriptor->post_load != nullptr)
        {
            info.descriptor->post_load(comp_ptr);
        }

        const auto root_ent = find_prefab_instance_root(*ctx.registry, ent);
        if (root_ent != ecs::tombstone)
        {
            _added_override = true;
            _prefab_root_addr = compute_entity_address(*ctx.registry, root_ent);
            _override_target_path = compute_prefab_id_path(*ctx.registry, root_ent, ent);

            auto inst = ctx.registry->get<scene::prefab_instance_component>(root_ent);
            auto staged = scene::staged_component{.type_info = &info};
            if (info.size > 0 && comp_ptr != nullptr)
            {
                staged.data.resize(info.size);
                tempest::memcpy(staged.data.data(), comp_ptr, info.size);
            }
            inst.overrides.added_components.push_back(scene::added_component{
                .target = _override_target_path,
                .component = tempest::move(staged),
            });
            ctx.registry->assign_or_replace(root_ent, tempest::move(inst));
        }
    }

    auto add_component_command::revert(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
        {
            return;
        }

        if (_type_index != invalid_type_index)
        {
            ecs::raw_registry_access::remove_raw(*ctx.registry, ent, _type_index);
        }

        if (_added_override)
        {
            const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _prefab_root_addr);
            if (root_ent != ecs::tombstone && ctx.registry->is_valid(root_ent))
            {
                auto inst = ctx.registry->get<scene::prefab_instance_component>(root_ent);
                const auto& info = ctx.registry->type_registry().type_info(_type_index);
                for (auto* iter = inst.overrides.added_components.begin();
                     iter != inst.overrides.added_components.end(); ++iter)
                {
                    if (iter->target == _override_target_path && iter->component.type_info != nullptr &&
                        iter->component.type_info->name == info.name)
                    {
                        inst.overrides.added_components.erase(iter);
                        break;
                    }
                }
                ctx.registry->assign_or_replace(root_ent, tempest::move(inst));
            }
        }
    }

    auto add_component_command::label() const noexcept -> string_view
    {
        return "Add Component";
    }

    auto add_component_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _target_addr.scene;
    }

    //==============================================================================
    // remove_component_command
    //==============================================================================

    remove_component_command::remove_component_command(entity_address target, uint32_t type_index)
        : _target_addr{tempest::move(target)}, _type_index{type_index}
    {
    }

    remove_component_command::remove_component_command(entity_address target, string_view type_name)
        : _target_addr{tempest::move(target)}, _type_name{type_name}
    {
    }

    auto remove_component_command::apply(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
        {
            return;
        }

        if (_type_index == invalid_type_index && !_type_name.empty())
        {
            const auto opt_idx = ctx.registry->type_registry().type_index(_type_name);
            if (opt_idx.has_value())
            {
                _type_index = *opt_idx;
            }
        }

        if (_type_index == invalid_type_index)
        {
            return;
        }

        const auto& info = ctx.registry->type_registry().type_info(_type_index);
        const auto* raw_ptr = ecs::raw_registry_access::get_component_raw(*ctx.registry, ent, _type_index);
        if (raw_ptr != nullptr && info.size > 0)
        {
            _saved_bytes.resize(info.size);
            tempest::memcpy(_saved_bytes.data(), raw_ptr, info.size);
        }

        const auto root_ent = find_prefab_instance_root(*ctx.registry, ent);
        if (root_ent != ecs::tombstone)
        {
            _added_override = true;
            _prefab_root_addr = compute_entity_address(*ctx.registry, root_ent);
            _override_target_path = compute_prefab_id_path(*ctx.registry, root_ent, ent);

            auto inst = ctx.registry->get<scene::prefab_instance_component>(root_ent);
            inst.overrides.removed_components.push_back(scene::removed_component{
                .target = _override_target_path,
                .component_name = string{info.name},
            });
            ctx.registry->assign_or_replace(root_ent, tempest::move(inst));
        }

        ecs::raw_registry_access::remove_raw(*ctx.registry, ent, _type_index);
    }

    auto remove_component_command::revert(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
        {
            return;
        }

        if (_type_index != invalid_type_index)
        {
            const auto& info = ctx.registry->type_registry().type_info(_type_index);
            auto* comp_ptr = ecs::raw_registry_access::assign_raw(*ctx.registry, ent, info,
                                                                  _saved_bytes.empty() ? nullptr : _saved_bytes.data());
            if (info.descriptor.has_value() && info.descriptor->post_load != nullptr)
            {
                info.descriptor->post_load(comp_ptr);
            }
        }

        if (_added_override)
        {
            const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _prefab_root_addr);
            if (root_ent != ecs::tombstone && ctx.registry->is_valid(root_ent))
            {
                auto inst = ctx.registry->get<scene::prefab_instance_component>(root_ent);
                const auto& info = ctx.registry->type_registry().type_info(_type_index);
                for (auto* iter = inst.overrides.removed_components.begin();
                     iter != inst.overrides.removed_components.end(); ++iter)
                {
                    if (iter->target == _override_target_path && iter->component_name == info.name)
                    {
                        inst.overrides.removed_components.erase(iter);
                        break;
                    }
                }
                ctx.registry->assign_or_replace(root_ent, tempest::move(inst));
            }
        }
    }

    auto remove_component_command::label() const noexcept -> string_view
    {
        return "Remove Component";
    }

    auto remove_component_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _target_addr.scene;
    }

    //==============================================================================
    // reset_component_command
    //==============================================================================

    reset_component_command::reset_component_command(entity_address target, uint32_t type_index)
        : _target_addr{tempest::move(target)}, _type_index{type_index}
    {
    }

    reset_component_command::reset_component_command(entity_address target, string_view type_name)
        : _target_addr{tempest::move(target)}, _type_name{type_name}
    {
    }

    auto reset_component_command::apply(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
        {
            return;
        }

        if (_type_index == invalid_type_index && !_type_name.empty())
        {
            const auto opt_idx = ctx.registry->type_registry().type_index(_type_name);
            if (opt_idx.has_value())
            {
                _type_index = *opt_idx;
            }
        }

        if (_type_index == invalid_type_index)
        {
            return;
        }

        const auto& info = ctx.registry->type_registry().type_info(_type_index);
        auto* comp_ptr = ecs::raw_registry_access::get_component_raw(*ctx.registry, ent, _type_index);
        if (comp_ptr == nullptr)
        {
            return;
        }

        if (info.size > 0)
        {
            _saved_bytes.resize(info.size);
            tempest::memcpy(_saved_bytes.data(), comp_ptr, info.size);
        }

        if (info.construct != nullptr)
        {
            info.construct(comp_ptr);
        }
        else
        {
            tempest::memset(comp_ptr, 0, info.size);
        }

        if (info.descriptor.has_value() && info.descriptor->post_load != nullptr)
        {
            info.descriptor->post_load(comp_ptr);
        }

        if (info.publish_replaced != nullptr)
        {
            info.publish_replaced(ctx.registry->event_registry(), ent, _saved_bytes.data(), comp_ptr);
        }

        const auto root_ent = find_prefab_instance_root(*ctx.registry, ent);
        if (root_ent != ecs::tombstone)
        {
            _prefab_root_addr = compute_entity_address(*ctx.registry, root_ent);
            const auto target_path = compute_prefab_id_path(*ctx.registry, root_ent, ent);
            auto inst = ctx.registry->get<scene::prefab_instance_component>(root_ent);

            auto remaining = vector<scene::field_override>{};
            for (auto& field_over : inst.overrides.field_overrides)
            {
                if (field_over.target == target_path && field_over.component_name == info.name)
                {
                    _saved_field_overrides.push_back(tempest::move(field_over));
                }
                else
                {
                    remaining.push_back(tempest::move(field_over));
                }
            }
            inst.overrides.field_overrides = tempest::move(remaining);
            ctx.registry->assign_or_replace(root_ent, tempest::move(inst));
        }
    }

    auto reset_component_command::revert(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
        {
            return;
        }

        if (_type_index != invalid_type_index && !_saved_bytes.empty())
        {
            auto* comp_ptr = ecs::raw_registry_access::get_component_raw(*ctx.registry, ent, _type_index);
            if (comp_ptr != nullptr)
            {
                const auto& info = ctx.registry->type_registry().type_info(_type_index);
                auto current_bytes = vector<byte>(info.size);
                if (info.size > 0)
                {
                    tempest::memcpy(current_bytes.data(), comp_ptr, info.size);
                }

                tempest::memcpy(comp_ptr, _saved_bytes.data(), _saved_bytes.size());
                if (info.descriptor.has_value() && info.descriptor->post_load != nullptr)
                {
                    info.descriptor->post_load(comp_ptr);
                }
                if (info.publish_replaced != nullptr)
                {
                    info.publish_replaced(ctx.registry->event_registry(), ent, current_bytes.data(), comp_ptr);
                }
            }
        }

        if (!_saved_field_overrides.empty())
        {
            const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _prefab_root_addr);
            if (root_ent != ecs::tombstone && ctx.registry->is_valid(root_ent))
            {
                auto inst = ctx.registry->get<scene::prefab_instance_component>(root_ent);
                for (const auto& field_over : _saved_field_overrides)
                {
                    inst.overrides.field_overrides.push_back(field_over);
                }
                ctx.registry->assign_or_replace(root_ent, tempest::move(inst));
            }
        }
    }

    auto reset_component_command::label() const noexcept -> string_view
    {
        return "Reset Component";
    }

    auto reset_component_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _target_addr.scene;
    }

    //==============================================================================
    // set_field_command
    //==============================================================================

    set_field_command::set_field_command(entity_address target, string_view component_name, string_view field_name,
                                         vector<byte> new_value_bytes)
        : _target_addr{tempest::move(target)}, _component_name{component_name}, _field_name{field_name},
          _new_value_bytes{tempest::move(new_value_bytes)}
    {
    }

    auto set_field_command::apply(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
        {
            return;
        }

        const auto opt_idx = ctx.registry->type_registry().type_index(_component_name);
        if (!opt_idx.has_value())
        {
            return;
        }

        const auto& info = ctx.registry->type_registry().type_info(*opt_idx);
        auto* comp_ptr = ecs::raw_registry_access::get_component_raw(*ctx.registry, ent, *opt_idx);
        if (comp_ptr == nullptr || !info.descriptor.has_value())
        {
            return;
        }

        const auto* field = info.descriptor->find_field(_field_name);
        if (field == nullptr)
        {
            return;
        }

        if (_prev_component_bytes.empty() && info.size > 0)
        {
            _prev_component_bytes.resize(info.size);
            tempest::memcpy(_prev_component_bytes.data(), comp_ptr, info.size);
        }

        if (!_captured_prev)
        {
            _prev_value_bytes.resize(field->size);
            if (field->get != nullptr)
            {
                field->get(comp_ptr, _prev_value_bytes.data());
            }
            else if (field->offset != ecs::field_descriptor::no_offset)
            {
                tempest::memcpy(_prev_value_bytes.data(), static_cast<const byte*>(comp_ptr) + field->offset,
                                field->size);
            }
            _captured_prev = true;
        }

        if (field->set != nullptr)
        {
            field->set(comp_ptr, _new_value_bytes.data());
        }
        else if (field->offset != ecs::field_descriptor::no_offset)
        {
            tempest::memcpy(static_cast<byte*>(comp_ptr) + field->offset, _new_value_bytes.data(),
                            tempest::min(field->size, _new_value_bytes.size()));
        }

        if (info.descriptor->post_load != nullptr)
        {
            info.descriptor->post_load(comp_ptr);
        }

        if (info.publish_replaced != nullptr)
        {
            info.publish_replaced(ctx.registry->event_registry(), ent, _prev_component_bytes.data(), comp_ptr);
        }

        const auto root_ent = find_prefab_instance_root(*ctx.registry, ent);
        if (root_ent != ecs::tombstone)
        {
            _is_in_prefab = true;
            _prefab_root_addr = compute_entity_address(*ctx.registry, root_ent);
            _override_target_path = compute_prefab_id_path(*ctx.registry, root_ent, ent);

            auto inst = ctx.registry->get<scene::prefab_instance_component>(root_ent);
            auto* existing_override_iter = inst.overrides.field_overrides.end();
            for (auto* iter = inst.overrides.field_overrides.begin(); iter != inst.overrides.field_overrides.end();
                 ++iter)
            {
                if (iter->target == _override_target_path && iter->component_name == _component_name &&
                    iter->field_name == _field_name)
                {
                    existing_override_iter = iter;
                    break;
                }
            }

            if (!_had_prev_field_override)
            {
                if (existing_override_iter != inst.overrides.field_overrides.end())
                {
                    _prev_field_override = *existing_override_iter;
                }
                else
                {
                    _prev_field_override = nullopt;
                }
                _had_prev_field_override = true;
            }

            auto template_val = optional<vector<byte>>{nullopt};
            if (ctx.prefab_cache != nullptr)
            {
                ctx.prefab_cache->with_template(inst.prefab_guid, [&](const scene::prefab_template& tmpl) {
                    const auto* pmc = ctx.registry->try_get<scene::prefab_member_component>(ent);
                    const auto target_local_id = (pmc != nullptr) ? pmc->template_local_id : scene::entity_local_id{};
                    const auto* rec = target_local_id.is_valid() ? tmpl.find_record(target_local_id)
                                                                 : (ent == root_ent ? tmpl.get_root_record() : nullptr);
                    if (rec != nullptr)
                    {
                        for (const auto& comp : rec->components)
                        {
                            if (comp.type_info != nullptr && comp.type_info->name == _component_name)
                            {
                                if (comp.type_info->descriptor.has_value())
                                {
                                    const auto* tmpl_field = comp.type_info->descriptor->find_field(_field_name);
                                    if (tmpl_field != nullptr && !comp.data.empty())
                                    {
                                        auto t_bytes = vector<byte>(tmpl_field->size);
                                        if (tmpl_field->get != nullptr)
                                        {
                                            tmpl_field->get(comp.data.data(), t_bytes.data());
                                        }
                                        else if (tmpl_field->offset != ecs::field_descriptor::no_offset)
                                        {
                                            tempest::memcpy(t_bytes.data(), comp.data.data() + tmpl_field->offset,
                                                            tmpl_field->size);
                                        }
                                        template_val = tempest::move(t_bytes);
                                    }
                                }
                                break;
                            }
                        }
                    }
                });
            }

            const auto is_equal_to_template = template_val.has_value() && (*template_val == _new_value_bytes);

            if (is_equal_to_template)
            {
                if (existing_override_iter != inst.overrides.field_overrides.end())
                {
                    inst.overrides.field_overrides.erase(existing_override_iter);
                }
            }
            else
            {
                if (existing_override_iter != inst.overrides.field_overrides.end())
                {
                    existing_override_iter->value_bytes = _new_value_bytes;
                }
                else
                {
                    inst.overrides.field_overrides.push_back(scene::field_override{
                        .target = _override_target_path,
                        .component_name = _component_name,
                        .field_name = _field_name,
                        .value_bytes = _new_value_bytes,
                    });
                }
            }
            ctx.registry->assign_or_replace(root_ent, tempest::move(inst));
        }
    }

    auto set_field_command::revert(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
        {
            return;
        }

        const auto opt_idx = ctx.registry->type_registry().type_index(_component_name);
        if (!opt_idx.has_value())
        {
            return;
        }

        const auto& info = ctx.registry->type_registry().type_info(*opt_idx);
        auto* comp_ptr = ecs::raw_registry_access::get_component_raw(*ctx.registry, ent, *opt_idx);
        if (comp_ptr == nullptr || !info.descriptor.has_value())
        {
            return;
        }

        const auto* field = info.descriptor->find_field(_field_name);
        if (field == nullptr || _prev_value_bytes.empty())
        {
            return;
        }

        if (_prev_component_bytes.empty() && info.size > 0)
        {
            _prev_component_bytes.resize(info.size);
            tempest::memcpy(_prev_component_bytes.data(), comp_ptr, info.size);
        }

        auto current_bytes = vector<byte>(info.size);
        if (info.size > 0)
        {
            tempest::memcpy(current_bytes.data(), comp_ptr, info.size);
        }

        if (field->set != nullptr)
        {
            field->set(comp_ptr, _prev_value_bytes.data());
        }
        else if (field->offset != ecs::field_descriptor::no_offset)
        {
            tempest::memcpy(static_cast<byte*>(comp_ptr) + field->offset, _prev_value_bytes.data(),
                            tempest::min(field->size, _prev_value_bytes.size()));
        }

        if (info.descriptor->post_load != nullptr)
        {
            info.descriptor->post_load(comp_ptr);
        }

        if (info.publish_replaced != nullptr)
        {
            info.publish_replaced(ctx.registry->event_registry(), ent, current_bytes.data(), comp_ptr);
        }

        if (_is_in_prefab)
        {
            const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _prefab_root_addr);
            if (root_ent != ecs::tombstone && ctx.registry->is_valid(root_ent))
            {
                auto inst = ctx.registry->get<scene::prefab_instance_component>(root_ent);
                for (auto* iter = inst.overrides.field_overrides.begin(); iter != inst.overrides.field_overrides.end();
                     ++iter)
                {
                    if (iter->target == _override_target_path && iter->component_name == _component_name &&
                        iter->field_name == _field_name)
                    {
                        inst.overrides.field_overrides.erase(iter);
                        break;
                    }
                }
                if (_had_prev_field_override && _prev_field_override.has_value())
                {
                    inst.overrides.field_overrides.push_back(*_prev_field_override);
                }
                ctx.registry->assign_or_replace(root_ent, tempest::move(inst));
            }
        }
    }

    auto set_field_command::label() const noexcept -> string_view
    {
        return "Set Field";
    }

    auto set_field_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _target_addr.scene;
    }

    auto set_field_command::merge_with(const editor_command& next) -> bool
    {
        const auto* next_cmd = dynamic_cast<const set_field_command*>(&next);
        if (next_cmd == nullptr)
        {
            return false;
        }
        if (_target_addr == next_cmd->_target_addr && _component_name == next_cmd->_component_name &&
            _field_name == next_cmd->_field_name)
        {
            _new_value_bytes = next_cmd->_new_value_bytes;
            return true;
        }
        return false;
    }

    auto set_field_command::target_address() const noexcept -> const entity_address&
    {
        return _target_addr;
    }

    auto set_field_command::component_name() const noexcept -> string_view
    {
        return string_view{_component_name};
    }

    auto set_field_command::field_name() const noexcept -> string_view
    {
        return string_view{_field_name};
    }

    auto set_field_command::new_value_bytes() const noexcept -> span<const byte>
    {
        return span<const byte>{_new_value_bytes.data(), _new_value_bytes.size()};
    }

    auto set_field_command::prev_value_bytes() const noexcept -> span<const byte>
    {
        return span<const byte>{_prev_value_bytes.data(), _prev_value_bytes.size()};
    }

    //==============================================================================
    // revert_override_command
    //==============================================================================

    revert_override_command::revert_override_command(entity_address instance_root, scene::id_path target,
                                                     string_view component_name, string_view field_name)
        : _instance_root_addr{tempest::move(instance_root)}, _target_path{tempest::move(target)},
          _component_name{component_name}, _field_name{field_name}, _kind{override_kind::field}
    {
    }

    revert_override_command::revert_override_command(entity_address instance_root, scene::id_path target,
                                                     string_view component_name, override_kind kind)
        : _instance_root_addr{tempest::move(instance_root)}, _target_path{tempest::move(target)},
          _component_name{component_name}, _kind{kind}
    {
    }

    auto revert_override_command::apply(command_context& ctx) -> void
    {
        const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _instance_root_addr);
        if (root_ent == ecs::tombstone || !ctx.registry->is_valid(root_ent))
        {
            return;
        }

        const auto* inst = ctx.registry->try_get<scene::prefab_instance_component>(root_ent);
        if (inst == nullptr)
        {
            return;
        }

        if (!_captured)
        {
            const auto target_ent = resolve_prefab_target_entity(*ctx.registry, root_ent, _target_path);
            if (_kind == override_kind::field)
            {
                for (const auto& field_over : inst->overrides.field_overrides)
                {
                    if (field_over.target == _target_path && field_over.component_name == _component_name &&
                        field_over.field_name == _field_name)
                    {
                        _saved_field_override = field_over;
                        break;
                    }
                }

                if (target_ent != ecs::tombstone && ctx.registry->is_valid(target_ent))
                {
                    const auto opt_idx = ctx.registry->type_registry().type_index(_component_name);
                    if (opt_idx.has_value())
                    {
                        const auto& info = ctx.registry->type_registry().type_info(*opt_idx);
                        if (info.descriptor.has_value())
                        {
                            const auto* field = info.descriptor->find_field(_field_name);
                            const auto* comp_ptr =
                                ecs::raw_registry_access::get_component_raw(*ctx.registry, target_ent, *opt_idx);
                            if (field != nullptr && comp_ptr != nullptr)
                            {
                                _saved_live_bytes.resize(field->size);
                                if (field->get != nullptr)
                                {
                                    field->get(comp_ptr, _saved_live_bytes.data());
                                }
                                else if (field->offset != ecs::field_descriptor::no_offset)
                                {
                                    tempest::memcpy(_saved_live_bytes.data(),
                                                    static_cast<const byte*>(comp_ptr) + field->offset, field->size);
                                }
                            }
                        }
                    }
                }
            }
            else if (_kind == override_kind::added_component)
            {
                for (const auto& added_override : inst->overrides.added_components)
                {
                    if (added_override.target == _target_path && added_override.component.type_info != nullptr &&
                        added_override.component.type_info->name == _component_name)
                    {
                        _saved_added_component = added_override;
                        break;
                    }
                }

                if (target_ent != ecs::tombstone && ctx.registry->is_valid(target_ent))
                {
                    const auto opt_idx = ctx.registry->type_registry().type_index(_component_name);
                    if (opt_idx.has_value())
                    {
                        const auto& info = ctx.registry->type_registry().type_info(*opt_idx);
                        const auto* comp_ptr =
                            ecs::raw_registry_access::get_component_raw(*ctx.registry, target_ent, *opt_idx);
                        if (comp_ptr != nullptr && info.size > 0)
                        {
                            _saved_live_bytes.resize(info.size);
                            tempest::memcpy(_saved_live_bytes.data(), comp_ptr, info.size);
                        }
                    }
                }
            }
            else if (_kind == override_kind::removed_component)
            {
                for (const auto& removed_override : inst->overrides.removed_components)
                {
                    if (removed_override.target == _target_path && removed_override.component_name == _component_name)
                    {
                        _saved_removed_component = removed_override;
                        break;
                    }
                }
            }
            _captured = true;
        }

        if (_kind == override_kind::field && ctx.prefab_cache != nullptr)
        {
            static_cast<void>(scene::prefab_operations::revert_field_override(
                *ctx.registry, root_ent, _target_path, _component_name, _field_name, *ctx.prefab_cache));
        }
        else if (_kind == override_kind::added_component)
        {
            static_cast<void>(scene::prefab_operations::revert_added_component(*ctx.registry, root_ent, _target_path,
                                                                               _component_name));
        }
        else if (_kind == override_kind::removed_component && ctx.prefab_cache != nullptr)
        {
            static_cast<void>(scene::prefab_operations::revert_removed_component(*ctx.registry, root_ent, _target_path,
                                                                                 _component_name, *ctx.prefab_cache));
        }
    }

    auto revert_override_command::revert(command_context& ctx) -> void
    {
        const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _instance_root_addr);
        if (root_ent == ecs::tombstone || !ctx.registry->is_valid(root_ent))
        {
            return;
        }

        const auto target_ent = resolve_prefab_target_entity(*ctx.registry, root_ent, _target_path);
        auto inst = ctx.registry->get<scene::prefab_instance_component>(root_ent);

        if (_kind == override_kind::field)
        {
            if (_saved_field_override.has_value())
            {
                inst.overrides.field_overrides.push_back(*_saved_field_override);
            }
            if (target_ent != ecs::tombstone && ctx.registry->is_valid(target_ent) && !_saved_live_bytes.empty())
            {
                const auto opt_idx = ctx.registry->type_registry().type_index(_component_name);
                if (opt_idx.has_value())
                {
                    const auto& info = ctx.registry->type_registry().type_info(*opt_idx);
                    auto* comp_ptr = ecs::raw_registry_access::get_component_raw(*ctx.registry, target_ent, *opt_idx);
                    if (info.descriptor.has_value() && comp_ptr != nullptr)
                    {
                        const auto* field = info.descriptor->find_field(_field_name);
                        if (field != nullptr)
                        {
                            if (field->set != nullptr)
                            {
                                field->set(comp_ptr, _saved_live_bytes.data());
                            }
                            else if (field->offset != ecs::field_descriptor::no_offset)
                            {
                                tempest::memcpy(static_cast<byte*>(comp_ptr) + field->offset, _saved_live_bytes.data(),
                                                tempest::min(field->size, _saved_live_bytes.size()));
                            }
                            if (info.descriptor->post_load != nullptr)
                            {
                                info.descriptor->post_load(comp_ptr);
                            }
                        }
                    }
                }
            }
        }
        else if (_kind == override_kind::added_component)
        {
            if (_saved_added_component.has_value())
            {
                inst.overrides.added_components.push_back(*_saved_added_component);
            }
            if (target_ent != ecs::tombstone && ctx.registry->is_valid(target_ent))
            {
                const auto opt_idx = ctx.registry->type_registry().type_index(_component_name);
                if (opt_idx.has_value())
                {
                    const auto& info = ctx.registry->type_registry().type_info(*opt_idx);
                    auto* comp_ptr = ecs::raw_registry_access::assign_raw(
                        *ctx.registry, target_ent, info,
                        _saved_live_bytes.empty() ? nullptr : _saved_live_bytes.data());
                    if (info.descriptor.has_value() && info.descriptor->post_load != nullptr)
                    {
                        info.descriptor->post_load(comp_ptr);
                    }
                }
            }
        }
        else if (_kind == override_kind::removed_component)
        {
            if (_saved_removed_component.has_value())
            {
                inst.overrides.removed_components.push_back(*_saved_removed_component);
            }
            if (target_ent != ecs::tombstone && ctx.registry->is_valid(target_ent))
            {
                const auto opt_idx = ctx.registry->type_registry().type_index(_component_name);
                if (opt_idx.has_value())
                {
                    ecs::raw_registry_access::remove_raw(*ctx.registry, target_ent, *opt_idx);
                }
            }
        }

        ctx.registry->assign_or_replace(root_ent, tempest::move(inst));
    }

    auto revert_override_command::label() const noexcept -> string_view
    {
        return "Revert Prefab Override";
    }

    auto revert_override_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _instance_root_addr.scene;
    }

    //==============================================================================
    // apply_override_command
    //==============================================================================

    apply_override_command::apply_override_command(entity_address instance_root, string_view file_path)
        : _instance_root_addr{tempest::move(instance_root)}, _file_path{file_path}
    {
    }

    auto apply_override_command::apply(command_context& ctx) -> void
    {
        const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _instance_root_addr);
        if (root_ent == ecs::tombstone || !ctx.registry->is_valid(root_ent) || ctx.prefab_cache == nullptr)
        {
            return;
        }

        const auto* inst = ctx.registry->try_get<scene::prefab_instance_component>(root_ent);
        if (inst == nullptr)
        {
            return;
        }

        _prev_overrides = inst->overrides;
        const auto res =
            scene::prefab_operations::apply_to_prefab(*ctx.registry, root_ent, _file_path, *ctx.prefab_cache);
        _applied = res.has_value();
    }

    auto apply_override_command::revert(command_context& ctx) -> void
    {
        if (!_applied)
        {
            return;
        }

        const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _instance_root_addr);
        if (root_ent != ecs::tombstone && ctx.registry->is_valid(root_ent))
        {
            auto inst = ctx.registry->get<scene::prefab_instance_component>(root_ent);
            inst.overrides = _prev_overrides;
            ctx.registry->assign_or_replace(root_ent, tempest::move(inst));
        }
    }

    auto apply_override_command::label() const noexcept -> string_view
    {
        return "Apply Overrides to Prefab";
    }

    auto apply_override_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _instance_root_addr.scene;
    }

    //==============================================================================
    // unpack_prefab_command
    //==============================================================================

    unpack_prefab_command::unpack_prefab_command(entity_address instance_root, bool completely)
        : _instance_root_addr{tempest::move(instance_root)}, _completely{completely}
    {
    }

    auto unpack_prefab_command::apply(command_context& ctx) -> void
    {
        const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _instance_root_addr);
        if (root_ent == ecs::tombstone || !ctx.registry->is_valid(root_ent))
        {
            return;
        }

        const auto* inst = ctx.registry->try_get<scene::prefab_instance_component>(root_ent);
        if (inst == nullptr)
        {
            return;
        }

        if (!_captured)
        {
            _saved_instance_comp = *inst;

            auto collect = [&](auto& self, ecs::entity ent) -> void {
                if (const auto* pmc = ctx.registry->try_get<scene::prefab_member_component>(ent); pmc != nullptr)
                {
                    auto snap = member_snapshot{
                        .addr = compute_entity_address(*ctx.registry, ent),
                        .live_entity = ent,
                        .template_local_id = pmc->template_local_id,
                        .nested_instance = nullopt,
                    };
                    if (ent != root_ent)
                    {
                        if (const auto* nested = ctx.registry->try_get<scene::prefab_instance_component>(ent);
                            nested != nullptr)
                        {
                            snap.nested_instance = *nested;
                        }
                    }
                    _saved_members.push_back(tempest::move(snap));
                }

                const auto* rel = ctx.registry->try_get<ecs::relationship_component<ecs::entity>>(ent);
                if (rel != nullptr)
                {
                    auto child = rel->first_child;
                    while (child != ecs::tombstone && ctx.registry->is_valid(child))
                    {
                        self(self, child);
                        const auto* child_rel = ctx.registry->try_get<ecs::relationship_component<ecs::entity>>(child);
                        child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
                    }
                }
            };
            collect(collect, root_ent);
            _captured = true;
        }

        if (_completely)
        {
            static_cast<void>(scene::prefab_operations::unpack_completely(*ctx.registry, root_ent));
        }
        else
        {
            static_cast<void>(scene::prefab_operations::unpack(*ctx.registry, root_ent));
        }
    }

    auto unpack_prefab_command::revert(command_context& ctx) -> void
    {
        const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _instance_root_addr);
        if (root_ent == ecs::tombstone || !ctx.registry->is_valid(root_ent))
        {
            return;
        }

        ctx.registry->assign(root_ent, _saved_instance_comp);

        for (const auto& mem : _saved_members)
        {
            auto ent = mem.live_entity;
            if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
            {
                ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, mem.addr);
            }
            if (ent != ecs::tombstone && ctx.registry->is_valid(ent))
            {
                ctx.registry->assign(ent, scene::prefab_member_component{
                                              .template_local_id = mem.template_local_id,
                                          });
                if (mem.nested_instance.has_value())
                {
                    ctx.registry->assign(ent, *mem.nested_instance);
                }
            }
        }
    }

    auto unpack_prefab_command::label() const noexcept -> string_view
    {
        return "Unpack Prefab";
    }

    auto unpack_prefab_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _instance_root_addr.scene;
    }

    //==============================================================================
    // save_as_prefab_command
    //==============================================================================

    save_as_prefab_command::save_as_prefab_command(entity_address root, guid prefab_guid, string_view file_path)
        : _root_addr{tempest::move(root)}, _prefab_guid{prefab_guid}, _file_path{file_path}
    {
    }

    auto save_as_prefab_command::apply(command_context& ctx) -> void
    {
        const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _root_addr);
        if (root_ent == ecs::tombstone || !ctx.registry->is_valid(root_ent) || ctx.prefab_cache == nullptr)
        {
            return;
        }

        const auto res = scene::prefab_operations::save_as_prefab(*ctx.registry, root_ent, _prefab_guid, _file_path,
                                                                  *ctx.prefab_cache);
        _saved = res.has_value();
    }

    auto save_as_prefab_command::revert(command_context& ctx) -> void
    {
        if (!_saved)
        {
            return;
        }

        const auto root_ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _root_addr);
        if (root_ent != ecs::tombstone && ctx.registry->is_valid(root_ent))
        {
            static_cast<void>(scene::prefab_operations::unpack(*ctx.registry, root_ent));
        }
    }

    auto save_as_prefab_command::label() const noexcept -> string_view
    {
        return "Save As Prefab";
    }

    auto save_as_prefab_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _root_addr.scene;
    }

    //==============================================================================
    // remove_unknown_component_command
    //==============================================================================

    remove_unknown_component_command::remove_unknown_component_command(scene::scene_handle scene,
                                                                       uint64_t entity_local_id,
                                                                       string_view component_name)
        : _scene{scene}, _entity_local_id{entity_local_id}, _component_name{component_name}
    {
    }

    auto remove_unknown_component_command::apply(command_context& ctx) -> void
    {
        auto* unrec = ctx.scene_manager->get_unrecognized_data(_scene);
        if (unrec == nullptr)
        {
            return;
        }

        const auto ent_it = unrec->entities.find(_entity_local_id);
        if (ent_it == unrec->entities.end())
        {
            return;
        }

        for (auto* comp_it = ent_it->second.unknown_components.begin();
             comp_it != ent_it->second.unknown_components.end(); ++comp_it)
        {
            if (comp_it->name == _component_name)
            {
                _saved_component = *comp_it;
                ent_it->second.unknown_components.erase(comp_it);
                if (ent_it->second.empty())
                {
                    unrec->entities.erase(ent_it);
                }
                _captured = true;
                break;
            }
        }

        ctx.scene_manager->set_dirty(_scene, true);
    }

    auto remove_unknown_component_command::revert(command_context& ctx) -> void
    {
        if (!_captured || !_saved_component.has_value())
        {
            return;
        }

        auto* unrec = ctx.scene_manager->get_unrecognized_data(_scene);
        if (unrec != nullptr)
        {
            unrec->entities[_entity_local_id].unknown_components.push_back(*_saved_component);
            ctx.scene_manager->set_dirty(_scene, true);
        }
    }

    auto remove_unknown_component_command::label() const noexcept -> string_view
    {
        return "Remove Unknown Component";
    }

    auto remove_unknown_component_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _scene;
    }

    //==============================================================================
    // map_unknown_component_command
    //==============================================================================

    map_unknown_component_command::map_unknown_component_command(entity_address target,
                                                                 string_view unknown_component_name,
                                                                 uint32_t target_type_index)
        : _target_addr{tempest::move(target)}, _unknown_component_name{unknown_component_name},
          _target_type_index{target_type_index}
    {
    }

    map_unknown_component_command::map_unknown_component_command(entity_address target,
                                                                 string_view unknown_component_name,
                                                                 string_view target_type_name)
        : _target_addr{tempest::move(target)}, _unknown_component_name{unknown_component_name},
          _target_type_name{target_type_name}
    {
    }

    auto map_unknown_component_command::apply(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
        {
            return;
        }

        if (_target_type_index == invalid_type_index && !_target_type_name.empty())
        {
            const auto opt_idx = ctx.registry->type_registry().type_index(_target_type_name);
            if (opt_idx.has_value())
            {
                _target_type_index = *opt_idx;
            }
        }

        if (_target_type_index == invalid_type_index)
        {
            return;
        }

        if (_target_addr.scene.is_valid() && !_target_addr.id_path.empty())
        {
            auto* unrec = ctx.scene_manager->get_unrecognized_data(_target_addr.scene);
            if (unrec != nullptr)
            {
                const auto ent_id = _target_addr.id_path[0].value;
                const auto ent_it = unrec->entities.find(ent_id);
                if (ent_it != unrec->entities.end())
                {
                    for (auto* comp_it = ent_it->second.unknown_components.begin();
                         comp_it != ent_it->second.unknown_components.end(); ++comp_it)
                    {
                        if (comp_it->name == _unknown_component_name)
                        {
                            _saved_unknown_component = *comp_it;
                            ent_it->second.unknown_components.erase(comp_it);
                            if (ent_it->second.empty())
                            {
                                unrec->entities.erase(ent_it);
                            }
                            _captured = true;
                            break;
                        }
                    }
                }
            }
        }

        const auto& info = ctx.registry->type_registry().type_info(_target_type_index);
        auto* comp_ptr = ecs::raw_registry_access::assign_raw(*ctx.registry, ent, info);

        if (_saved_unknown_component.has_value() && info.descriptor.has_value())
        {
            auto alloc = system_allocator{};
            const auto doc_res = json_document::from_string(_saved_unknown_component->json_payload, alloc);
            if (doc_res.has_value() && doc_res->root().is_object())
            {
                const auto root_obj = doc_res->root().as_object().value();
                for (const auto& field : info.descriptor->fields)
                {
                    if (root_obj.contains(field.name))
                    {
                        deserialize_json_field(field, root_obj[field.name], comp_ptr);
                    }
                }
            }
        }

        if (info.descriptor.has_value() && info.descriptor->post_load != nullptr)
        {
            info.descriptor->post_load(comp_ptr);
        }

        if (_target_addr.scene.is_valid())
        {
            ctx.scene_manager->set_dirty(_target_addr.scene, true);
        }
    }

    auto map_unknown_component_command::revert(command_context& ctx) -> void
    {
        const auto ent = resolve_entity_address(*ctx.registry, *ctx.scene_manager, _target_addr);
        if (ent == ecs::tombstone || !ctx.registry->is_valid(ent))
        {
            return;
        }

        if (_target_type_index != invalid_type_index)
        {
            ecs::raw_registry_access::remove_raw(*ctx.registry, ent, _target_type_index);
        }

        if (_captured && _saved_unknown_component.has_value() && _target_addr.scene.is_valid() &&
            !_target_addr.id_path.empty())
        {
            auto* unrec = ctx.scene_manager->get_unrecognized_data(_target_addr.scene);
            if (unrec != nullptr)
            {
                unrec->entities[_target_addr.id_path[0].value].unknown_components.push_back(*_saved_unknown_component);
            }
        }

        if (_target_addr.scene.is_valid())
        {
            ctx.scene_manager->set_dirty(_target_addr.scene, true);
        }
    }

    auto map_unknown_component_command::label() const noexcept -> string_view
    {
        return "Map Unknown Component";
    }

    auto map_unknown_component_command::target_scene() const noexcept -> scene::scene_handle
    {
        return _target_addr.scene;
    }
} // namespace tempest::editor
