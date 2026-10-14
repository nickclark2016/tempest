#include <tempest/editor_command.hpp>

#include <tempest/random.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/utility.hpp>

namespace tempest::editor
{
    namespace
    {
        auto generate_dynamic_local_id() -> scene::entity_local_id
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

        auto find_descendant_by_local_id(const ecs::basic_archetype_registry& registry, ecs::entity parent_entity,
                                         scene::entity_local_id target_id) -> ecs::entity
        {
            const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(parent_entity);
            if (rel == nullptr)
            {
                return ecs::tombstone;
            }

            auto child = rel->first_child;
            while (child != ecs::tombstone && registry.is_valid(child))
            {
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

                const auto found = find_descendant_by_local_id(registry, child, target_id);
                if (found != ecs::tombstone)
                {
                    return found;
                }

                const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
                child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
            }

            return ecs::tombstone;
        }
    } // namespace

    auto compute_entity_address(ecs::basic_archetype_registry& registry, ecs::entity ent) -> entity_address
    {
        if (!registry.is_valid(ent))
        {
            return entity_address{};
        }

        auto chain = vector<ecs::entity>{};
        auto current = ent;
        chain.push_back(current);

        while (registry.is_valid(current))
        {
            const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(current);
            if (rel == nullptr || rel->parent == ecs::tombstone || rel->parent == ecs::null ||
                !registry.is_valid(rel->parent))
            {
                break;
            }
            current = rel->parent;
            chain.push_back(current);
        }

        auto scene_entity_idx = optional<size_t>{};
        for (auto i = chain.size(); i > 0; --i)
        {
            const auto candidate = chain[i - 1];
            if (registry.has<scene::scene_entity_component>(candidate))
            {
                scene_entity_idx = i - 1;
                break;
            }
        }

        auto addr = entity_address{};

        if (scene_entity_idx.has_value())
        {
            const auto root_ent = chain[*scene_entity_idx];
            const auto* sec = registry.try_get<scene::scene_entity_component>(root_ent);
            addr.scene = sec->scene;
            addr.id_path.push_back(sec->local_id);

            for (auto i = *scene_entity_idx; i > 0; --i)
            {
                const auto descendant = chain[i - 1];
                if (const auto* pmc = registry.try_get<scene::prefab_member_component>(descendant); pmc != nullptr)
                {
                    addr.id_path.push_back(pmc->template_local_id);
                }
                else if (const auto* desc_sec = registry.try_get<scene::scene_entity_component>(descendant);
                         desc_sec != nullptr)
                {
                    addr.id_path.push_back(desc_sec->local_id);
                }
                else if (const auto* dyn = registry.try_get<dynamic_entity_id_component>(descendant); dyn != nullptr)
                {
                    addr.id_path.push_back(dyn->local_id);
                }
                else
                {
                    const auto local_id = generate_dynamic_local_id();
                    registry.assign(descendant, dynamic_entity_id_component{.local_id = local_id});
                    addr.id_path.push_back(local_id);
                }
            }
            return addr;
        }

        const auto* dyn = registry.try_get<dynamic_entity_id_component>(ent);
        if (dyn != nullptr && dyn->local_id.is_valid())
        {
            addr.scene = {};
            addr.id_path.push_back(dyn->local_id);
            return addr;
        }

        const auto local_id = generate_dynamic_local_id();
        registry.assign(ent, dynamic_entity_id_component{.local_id = local_id});
        addr.scene = {};
        addr.id_path.push_back(local_id);
        return addr;
    }

    auto compute_entity_address(const ecs::basic_archetype_registry& registry, ecs::entity ent) -> entity_address
    {
        if (!registry.is_valid(ent))
        {
            return entity_address{};
        }

        auto chain = vector<ecs::entity>{};
        auto current = ent;
        chain.push_back(current);

        while (registry.is_valid(current))
        {
            const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(current);
            if (rel == nullptr || rel->parent == ecs::tombstone || rel->parent == ecs::null ||
                !registry.is_valid(rel->parent))
            {
                break;
            }
            current = rel->parent;
            chain.push_back(current);
        }

        auto scene_entity_idx = optional<size_t>{};
        for (auto i = chain.size(); i > 0; --i)
        {
            const auto candidate = chain[i - 1];
            if (registry.has<scene::scene_entity_component>(candidate))
            {
                scene_entity_idx = i - 1;
                break;
            }
        }

        auto addr = entity_address{};

        if (scene_entity_idx.has_value())
        {
            const auto root_ent = chain[*scene_entity_idx];
            const auto* sec = registry.try_get<scene::scene_entity_component>(root_ent);
            addr.scene = sec->scene;
            addr.id_path.push_back(sec->local_id);

            for (auto i = *scene_entity_idx; i > 0; --i)
            {
                const auto descendant = chain[i - 1];
                if (const auto* pmc = registry.try_get<scene::prefab_member_component>(descendant); pmc != nullptr)
                {
                    addr.id_path.push_back(pmc->template_local_id);
                }
                else if (const auto* desc_sec = registry.try_get<scene::scene_entity_component>(descendant);
                         desc_sec != nullptr)
                {
                    addr.id_path.push_back(desc_sec->local_id);
                }
                else if (const auto* dyn = registry.try_get<dynamic_entity_id_component>(descendant); dyn != nullptr)
                {
                    addr.id_path.push_back(dyn->local_id);
                }
                else
                {
                    addr.id_path.push_back(scene::entity_local_id{static_cast<uint64_t>(descendant)});
                }
            }
            return addr;
        }

        const auto* dyn = registry.try_get<dynamic_entity_id_component>(ent);
        if (dyn != nullptr && dyn->local_id.is_valid())
        {
            addr.scene = {};
            addr.id_path.push_back(dyn->local_id);
            return addr;
        }

        addr.scene = {};
        addr.id_path.push_back(scene::entity_local_id{static_cast<uint64_t>(ent)});
        return addr;
    }

    auto resolve_entity_address(const ecs::basic_archetype_registry& registry, const scene::scene_manager& scene_mgr,
                                const entity_address& addr) -> ecs::entity
    {
        if (addr.id_path.empty())
        {
            return ecs::tombstone;
        }

        if (addr.scene.is_valid())
        {
            auto root_entity = static_cast<ecs::entity>(ecs::tombstone);
            const auto roots = scene_mgr.get_root_entities(addr.scene);
            if (roots.has_value())
            {
                for (const auto candidate : *roots)
                {
                    if (registry.is_valid(candidate))
                    {
                        const auto* sec = registry.try_get<scene::scene_entity_component>(candidate);
                        if (sec != nullptr && sec->local_id == addr.id_path[0])
                        {
                            root_entity = candidate;
                            break;
                        }
                    }
                }
            }

            if (root_entity == ecs::tombstone)
            {
                for (const auto [self, sec] : registry.with<ecs::self_component, scene::scene_entity_component>())
                {
                    if (sec.scene == addr.scene && sec.local_id == addr.id_path[0])
                    {
                        root_entity = self.entity;
                        break;
                    }
                }
            }

            if (root_entity == ecs::tombstone || !registry.is_valid(root_entity))
            {
                return ecs::tombstone;
            }

            if (addr.id_path.size() == 1)
            {
                return root_entity;
            }

            auto current_entity = root_entity;
            for (auto seg_idx = size_t{1}; seg_idx < addr.id_path.size(); ++seg_idx)
            {
                const auto target_id = addr.id_path[seg_idx];
                const auto next_entity = find_descendant_by_local_id(registry, current_entity, target_id);
                if (next_entity == ecs::tombstone)
                {
                    return ecs::tombstone;
                }
                current_entity = next_entity;
            }

            return current_entity;
        }

        auto root_entity = static_cast<ecs::entity>(ecs::tombstone);
        for (const auto [self, dyn] : registry.with<ecs::self_component, dynamic_entity_id_component>())
        {
            if (dyn.local_id == addr.id_path[0])
            {
                root_entity = self.entity;
                break;
            }
        }

        if (root_entity == ecs::tombstone)
        {
            for (const auto [self, sec] : registry.with<ecs::self_component, scene::scene_entity_component>())
            {
                if (sec.local_id == addr.id_path[0])
                {
                    root_entity = self.entity;
                    break;
                }
            }
        }

        if (root_entity == ecs::tombstone)
        {
            for (const auto [self, pic] : registry.with<ecs::self_component, scene::prefab_instance_component>())
            {
                if (pic.instance_local_id == addr.id_path[0])
                {
                    root_entity = self.entity;
                    break;
                }
            }
        }

        if (root_entity == ecs::tombstone || !registry.is_valid(root_entity))
        {
            return ecs::tombstone;
        }

        if (addr.id_path.size() == 1)
        {
            return root_entity;
        }

        auto current_entity = root_entity;
        for (auto seg_idx = size_t{1}; seg_idx < addr.id_path.size(); ++seg_idx)
        {
            const auto target_id = addr.id_path[seg_idx];
            const auto next_entity = find_descendant_by_local_id(registry, current_entity, target_id);
            if (next_entity == ecs::tombstone)
            {
                return ecs::tombstone;
            }
            current_entity = next_entity;
        }

        return current_entity;
    }

    compound_command::compound_command(string_view label) : _label{label}
    {
    }

    compound_command::compound_command(vector<unique_ptr<editor_command>> commands, string_view label)
        : _commands{tempest::move(commands)}, _label{label}
    {
    }

    auto compound_command::add_command(unique_ptr<editor_command> cmd) -> void
    {
        _commands.push_back(tempest::move(cmd));
    }

    auto compound_command::apply(command_context& ctx) -> void
    {
        for (auto& cmd : _commands)
        {
            if (cmd != nullptr)
            {
                cmd->apply(ctx);
            }
        }
    }

    auto compound_command::revert(command_context& ctx) -> void
    {
        for (auto i = _commands.size(); i > 0; --i)
        {
            auto& cmd = _commands[i - 1];
            if (cmd != nullptr)
            {
                cmd->revert(ctx);
            }
        }
    }

    auto compound_command::label() const noexcept -> string_view
    {
        return string_view{_label};
    }

    auto compound_command::target_scene() const noexcept -> scene::scene_handle
    {
        for (const auto& cmd : _commands)
        {
            if (cmd != nullptr && cmd->target_scene().is_valid())
            {
                return cmd->target_scene();
            }
        }
        return {};
    }

    auto compound_command::commands() const noexcept -> span<const unique_ptr<editor_command>>
    {
        return span<const unique_ptr<editor_command>>{_commands.data(), _commands.size()};
    }
} // namespace tempest::editor
