#include <tempest/hierarchy_cache.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/format.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/utility.hpp>

namespace tempest::editor
{
    hierarchy_cache::hierarchy_cache() = default;

    hierarchy_cache::~hierarchy_cache()
    {
        detach_event_registry();
    }

    hierarchy_cache::hierarchy_cache(const hierarchy_cache& other)
        : _visible_rows{other._visible_rows},
          _scene_expanded{other._scene_expanded},
          _entity_expanded{other._entity_expanded},
          _dynamic_roots{other._dynamic_roots},
          _scene_roots{other._scene_roots},
          _dynamic_section_expanded{other._dynamic_section_expanded},
          _dirty{other._dirty},
          _last_active_scene{other._last_active_scene},
          _last_scene_dirty{other._last_scene_dirty},
          _last_scene_count{other._last_scene_count}
    {
        if (other._events != nullptr)
        {
            attach_event_registry(*other._events);
        }
    }

    hierarchy_cache::hierarchy_cache(hierarchy_cache&& other) noexcept
        : _visible_rows{tempest::move(other._visible_rows)},
          _scene_expanded{tempest::move(other._scene_expanded)},
          _entity_expanded{tempest::move(other._entity_expanded)},
          _dynamic_roots{tempest::move(other._dynamic_roots)},
          _scene_roots{tempest::move(other._scene_roots)},
          _dynamic_section_expanded{tempest::exchange(other._dynamic_section_expanded, default_section_expanded)},
          _dirty{tempest::exchange(other._dirty, default_dirty)},
          _last_active_scene{tempest::exchange(other._last_active_scene, default_active_scene)},
          _last_scene_dirty{tempest::move(other._last_scene_dirty)},
          _last_scene_count{tempest::exchange(other._last_scene_count, default_scene_count)}
    {
        if (other._events != nullptr)
        {
            auto* const events = other._events;
            other.detach_event_registry();
            attach_event_registry(*events);
        }
    }

    auto hierarchy_cache::operator=(const hierarchy_cache& other) -> hierarchy_cache&
    {
        if (this == &other)
        {
            return *this;
        }

        detach_event_registry();

        _visible_rows = other._visible_rows;
        _scene_expanded = other._scene_expanded;
        _entity_expanded = other._entity_expanded;
        _dynamic_roots = other._dynamic_roots;
        _scene_roots = other._scene_roots;
        _dynamic_section_expanded = other._dynamic_section_expanded;
        _dirty = other._dirty;
        _last_active_scene = other._last_active_scene;
        _last_scene_dirty = other._last_scene_dirty;
        _last_scene_count = other._last_scene_count;

        if (other._events != nullptr)
        {
            attach_event_registry(*other._events);
        }

        return *this;
    }

    auto hierarchy_cache::operator=(hierarchy_cache&& other) noexcept -> hierarchy_cache&
    {
        if (this == &other)
        {
            return *this;
        }

        detach_event_registry();

        _visible_rows = tempest::move(other._visible_rows);
        _scene_expanded = tempest::move(other._scene_expanded);
        _entity_expanded = tempest::move(other._entity_expanded);
        _dynamic_roots = tempest::move(other._dynamic_roots);
        _scene_roots = tempest::move(other._scene_roots);
        _dynamic_section_expanded = tempest::exchange(other._dynamic_section_expanded, default_section_expanded);
        _dirty = tempest::exchange(other._dirty, default_dirty);
        _last_active_scene = tempest::exchange(other._last_active_scene, default_active_scene);
        _last_scene_dirty = tempest::move(other._last_scene_dirty);
        _last_scene_count = tempest::exchange(other._last_scene_count, default_scene_count);

        if (other._events != nullptr)
        {
            auto* const events = other._events;
            other.detach_event_registry();
            attach_event_registry(*events);
        }

        return *this;
    }

    auto hierarchy_cache::get_visible_rows() const noexcept -> span<const hierarchy_row>
    {
        return span<const hierarchy_row>{_visible_rows.data(), _visible_rows.size()};
    }

    auto hierarchy_cache::set_expanded(scene::scene_handle scene, bool expanded) -> void
    {
        const auto it = _scene_expanded.find(scene);
        if (it != _scene_expanded.end())
        {
            if (it->second != expanded)
            {
                it->second = expanded;
                mark_dirty();
            }
        }
        else
        {
            _scene_expanded[scene] = expanded;
            mark_dirty();
        }
    }

    auto hierarchy_cache::set_expanded(ecs::entity ent, bool expanded) -> void
    {
        const auto it = _entity_expanded.find(ent);
        if (it != _entity_expanded.end())
        {
            if (it->second != expanded)
            {
                it->second = expanded;
                mark_dirty();
            }
        }
        else
        {
            _entity_expanded[ent] = expanded;
            mark_dirty();
        }
    }

    auto hierarchy_cache::set_dynamic_section_expanded(bool expanded) -> void
    {
        if (_dynamic_section_expanded != expanded)
        {
            _dynamic_section_expanded = expanded;
            mark_dirty();
        }
    }

    auto hierarchy_cache::is_expanded(scene::scene_handle scene) const noexcept -> bool
    {
        const auto it = _scene_expanded.find(scene);
        if (it != _scene_expanded.end())
        {
            return it->second;
        }
        return true;
    }

    auto hierarchy_cache::is_expanded(ecs::entity ent) const noexcept -> bool
    {
        const auto it = _entity_expanded.find(ent);
        if (it != _entity_expanded.end())
        {
            return it->second;
        }
        return true;
    }

    auto hierarchy_cache::is_dynamic_section_expanded() const noexcept -> bool
    {
        return _dynamic_section_expanded;
    }

    auto hierarchy_cache::mark_dirty() -> void
    {
        _dirty = true;
    }

    auto hierarchy_cache::is_dirty() const noexcept -> bool
    {
        return _dirty;
    }

    auto hierarchy_cache::attach_event_registry(event::event_registry& events) -> void
    {
        if (_events == &events)
        {
            return;
        }

        detach_event_registry();
        _events = &events;

        _entity_created_sub = _events->dispatcher<ecs::entity_created_event<ecs::entity>>().subscribe(
            [this](const auto&) { mark_dirty(); });

        _entity_destroyed_sub = _events->dispatcher<ecs::entity_destroyed_event<ecs::entity>>().subscribe(
            [this](const ecs::entity_destroyed_event<ecs::entity>& evt) {
                _entity_expanded.erase(evt.entity);
                mark_dirty();
            });

        _entity_renamed_sub = _events->dispatcher<ecs::entity_renamed_event<ecs::entity>>().subscribe(
            [this](const auto&) { mark_dirty(); });

        _rel_added_sub =
            _events->dispatcher<ecs::component_added_event<ecs::entity, ecs::relationship_component<ecs::entity>>>()
                .subscribe([this](const auto&) { mark_dirty(); });

        _rel_removed_sub =
            _events->dispatcher<ecs::component_removed_event<ecs::entity, ecs::relationship_component<ecs::entity>>>()
                .subscribe([this](const auto&) { mark_dirty(); });

        _rel_replaced_sub =
            _events->dispatcher<ecs::component_replaced_event<ecs::entity, ecs::relationship_component<ecs::entity>>>()
                .subscribe([this](const auto&) { mark_dirty(); });

        _scene_comp_added_sub =
            _events->dispatcher<ecs::component_added_event<ecs::entity, scene::scene_entity_component>>().subscribe(
                [this](const auto&) { mark_dirty(); });

        _scene_comp_removed_sub =
            _events->dispatcher<ecs::component_removed_event<ecs::entity, scene::scene_entity_component>>().subscribe(
                [this](const auto&) { mark_dirty(); });

        _scene_comp_replaced_sub =
            _events->dispatcher<ecs::component_replaced_event<ecs::entity, scene::scene_entity_component>>().subscribe(
                [this](const auto&) { mark_dirty(); });
    }

    auto hierarchy_cache::detach_event_registry() -> void
    {
        if (_events == nullptr)
        {
            return;
        }

        if (_entity_created_sub != event::null_subscription<ecs::entity_created_event<ecs::entity>>)
        {
            [[maybe_unused]] const auto unsub =
                _events->dispatcher<ecs::entity_created_event<ecs::entity>>().unsubscribe(_entity_created_sub);
            _entity_created_sub = {};
        }
        if (_entity_destroyed_sub != event::null_subscription<ecs::entity_destroyed_event<ecs::entity>>)
        {
            [[maybe_unused]] const auto unsub =
                _events->dispatcher<ecs::entity_destroyed_event<ecs::entity>>().unsubscribe(_entity_destroyed_sub);
            _entity_destroyed_sub = {};
        }
        if (_entity_renamed_sub != event::null_subscription<ecs::entity_renamed_event<ecs::entity>>)
        {
            [[maybe_unused]] const auto unsub =
                _events->dispatcher<ecs::entity_renamed_event<ecs::entity>>().unsubscribe(_entity_renamed_sub);
            _entity_renamed_sub = {};
        }
        if (_rel_added_sub !=
            event::null_subscription<ecs::component_added_event<ecs::entity, ecs::relationship_component<ecs::entity>>>)
        {
            [[maybe_unused]] const auto unsub =
                _events
                    ->dispatcher<ecs::component_added_event<ecs::entity, ecs::relationship_component<ecs::entity>>>()
                    .unsubscribe(_rel_added_sub);
            _rel_added_sub = {};
        }
        if (_rel_removed_sub !=
            event::null_subscription<ecs::component_removed_event<ecs::entity, ecs::relationship_component<ecs::entity>>>)
        {
            [[maybe_unused]] const auto unsub =
                _events
                    ->dispatcher<ecs::component_removed_event<ecs::entity, ecs::relationship_component<ecs::entity>>>()
                    .unsubscribe(_rel_removed_sub);
            _rel_removed_sub = {};
        }
        if (_rel_replaced_sub !=
            event::null_subscription<
                ecs::component_replaced_event<ecs::entity, ecs::relationship_component<ecs::entity>>>)
        {
            [[maybe_unused]] const auto unsub =
                _events
                    ->dispatcher<
                        ecs::component_replaced_event<ecs::entity, ecs::relationship_component<ecs::entity>>>()
                    .unsubscribe(_rel_replaced_sub);
            _rel_replaced_sub = {};
        }
        if (_scene_comp_added_sub !=
            event::null_subscription<ecs::component_added_event<ecs::entity, scene::scene_entity_component>>)
        {
            [[maybe_unused]] const auto unsub =
                _events->dispatcher<ecs::component_added_event<ecs::entity, scene::scene_entity_component>>()
                    .unsubscribe(_scene_comp_added_sub);
            _scene_comp_added_sub = {};
        }
        if (_scene_comp_removed_sub !=
            event::null_subscription<ecs::component_removed_event<ecs::entity, scene::scene_entity_component>>)
        {
            [[maybe_unused]] const auto unsub =
                _events->dispatcher<ecs::component_removed_event<ecs::entity, scene::scene_entity_component>>()
                    .unsubscribe(_scene_comp_removed_sub);
            _scene_comp_removed_sub = {};
        }
        if (_scene_comp_replaced_sub !=
            event::null_subscription<ecs::component_replaced_event<ecs::entity, scene::scene_entity_component>>)
        {
            [[maybe_unused]] const auto unsub =
                _events->dispatcher<ecs::component_replaced_event<ecs::entity, scene::scene_entity_component>>()
                    .unsubscribe(_scene_comp_replaced_sub);
            _scene_comp_replaced_sub = {};
        }

        _events = nullptr;
    }

    auto hierarchy_cache::rebuild(const ecs::basic_archetype_registry& reg, const scene::scene_manager& scene_mgr)
        -> void
    {
        const auto current_active = scene_mgr.get_active_scene();
        const auto current_scene_count = scene_mgr.get_scene_count();

        // Fast-path clean check: avoid dynamic allocations when clean and scene state unchanged
        if (!_dirty && current_active == _last_active_scene && current_scene_count == _last_scene_count)
        {
            auto any_dirty_changed = false;
            for (const auto& [scn, was_dirty] : _last_scene_dirty)
            {
                if (scene_mgr.is_dirty(scn) != was_dirty)
                {
                    any_dirty_changed = true;
                    break;
                }
            }
            if (!any_dirty_changed)
            {
                return;
            }
        }

        auto loaded_scenes = scene_mgr.get_loaded_scenes();
        _visible_rows.clear();
        _last_active_scene = current_active;
        _last_scene_count = current_scene_count;
        _last_scene_dirty.clear();

        for (const auto scn : loaded_scenes)
        {
            _last_scene_dirty[scn] = scene_mgr.is_dirty(scn);
        }

        // Single pass over scene_entity_component to discover untracked scenes
        for (const auto& [comp] : reg.with<scene::scene_entity_component>())
        {
            if (comp.scene.is_valid())
            {
                if (tempest::find(loaded_scenes.begin(), loaded_scenes.end(), comp.scene) == loaded_scenes.end())
                {
                    loaded_scenes.push_back(comp.scene);
                }
            }
        }

        auto append_entity_tree = [&](auto&& self, ecs::entity ent, size_t depth, scene::scene_handle scn,
                                      bool is_dyn) -> void {
            if (!reg.is_valid(ent))
            {
                return;
            }

            const auto* const rel = reg.try_get<ecs::relationship_component<ecs::entity>>(ent);
            const auto has_children =
                (rel != nullptr && rel->first_child != ecs::tombstone && reg.is_valid(rel->first_child));
            const auto expanded = is_expanded(ent);

            auto ent_row = hierarchy_row{};
            ent_row.type = row_type::entity_node;
            ent_row.depth = depth;
            ent_row.entity = ent;
            ent_row.scene = scn;

            const auto name_opt = reg.name(ent);
            if (name_opt.has_value() && !name_opt->empty())
            {
                ent_row.name = string(*name_opt);
            }
            else
            {
                ent_row.name = tempest::format("<Unnamed:{}>", static_cast<size_t>(ent));
            }

            ent_row.is_expanded = expanded;
            ent_row.has_children = has_children;
            ent_row.is_dirty = false;
            ent_row.is_active_scene = false;
            ent_row.is_prefab_instance = reg.has<scene::prefab_instance_component>(ent);
            ent_row.is_dynamic = is_dyn;
            ent_row.is_networked = reg.has<networked_tag>(ent);

            _visible_rows.push_back(tempest::move(ent_row));

            if (expanded && has_children)
            {
                auto child = rel->first_child;
                while (child != ecs::tombstone && reg.is_valid(child))
                {
                    self(self, child, depth + 1, scn, is_dyn);
                    const auto* const child_rel = reg.try_get<ecs::relationship_component<ecs::entity>>(child);
                    child = (child_rel != nullptr) ? child_rel->next_sibling : ecs::tombstone;
                }
            }
        };

        // Pre-index root entities per scene while preserving persistent order
        for (const auto scn : loaded_scenes)
        {
            auto& roots = _scene_roots[scn];

            // Prune invalid or non-root entities from persistent cache
            auto scn_it = roots.begin();
            while (scn_it != roots.end())
            {
                const auto ent = *scn_it;
                if (!reg.is_valid(ent))
                {
                    scn_it = roots.erase(scn_it);
                    continue;
                }
                const auto* const rel = reg.try_get<ecs::relationship_component<ecs::entity>>(ent);
                if (rel != nullptr && rel->parent != ecs::tombstone && rel->parent != ecs::null)
                {
                    scn_it = roots.erase(scn_it);
                    continue;
                }
                ++scn_it;
            }

            // Sync with scene_mgr root entities while preserving insertion order
            if (scene_mgr.is_valid(scn))
            {
                const auto roots_opt = scene_mgr.get_root_entities(scn);
                if (roots_opt.has_value())
                {
                    for (const auto ent : *roots_opt)
                    {
                        if (reg.is_valid(ent))
                        {
                            const auto* const rel = reg.try_get<ecs::relationship_component<ecs::entity>>(ent);
                            const auto is_root =
                                (rel == nullptr || rel->parent == ecs::tombstone || rel->parent == ecs::null);
                            if (is_root && tempest::find(roots.begin(), roots.end(), ent) == roots.end())
                            {
                                roots.push_back(ent);
                            }
                        }
                    }
                }
            }
        }

        // Include any scene entities with scene_entity_component
        for (const auto& [comp, self] : reg.with<scene::scene_entity_component, ecs::self_component>())
        {
            if (comp.scene.is_valid() && reg.is_valid(self.entity))
            {
                const auto* const rel = reg.try_get<ecs::relationship_component<ecs::entity>>(self.entity);
                const auto is_root =
                    (rel == nullptr || rel->parent == ecs::tombstone || rel->parent == ecs::null);
                if (is_root)
                {
                    auto& roots = _scene_roots[comp.scene];
                    if (tempest::find(roots.begin(), roots.end(), self.entity) == roots.end())
                    {
                        roots.push_back(self.entity);
                    }
                }
            }
        }

        // Emit scene headers and their entity hierarchies
        for (const auto scn : loaded_scenes)
        {
            const auto& scene_roots = _scene_roots[scn];

            const auto scn_expanded = is_expanded(scn);

            auto header_row = hierarchy_row{};
            header_row.type = row_type::scene_header;
            header_row.depth = 0;
            header_row.scene = scn;

            const auto scene_name = scene_mgr.get_scene_name(scn);
            if (scene_name.has_value() && !scene_name->empty())
            {
                header_row.name = string(*scene_name);
            }
            else
            {
                header_row.name = tempest::format("Scene {}", scn.index);
            }

            header_row.is_expanded = scn_expanded;
            header_row.has_children = !scene_roots.empty();
            header_row.is_dirty = scene_mgr.is_dirty(scn);
            header_row.is_active_scene = (current_active == scn);
            header_row.is_prefab_instance = false;
            header_row.is_dynamic = false;
            header_row.is_networked = false;

            _visible_rows.push_back(tempest::move(header_row));

            if (scn_expanded)
            {
                for (const auto root_ent : scene_roots)
                {
                    append_entity_tree(append_entity_tree, root_ent, 1, scn, false);
                }
            }
        }

        // Dynamic entities - preserve persistent insertion order across deletions
        auto dyn_it = _dynamic_roots.begin();
        while (dyn_it != _dynamic_roots.end())
        {
            const auto ent = *dyn_it;
            if (!reg.is_valid(ent) || reg.try_get<scene::scene_entity_component>(ent) != nullptr)
            {
                dyn_it = _dynamic_roots.erase(dyn_it);
                continue;
            }
            const auto* const rel = reg.try_get<ecs::relationship_component<ecs::entity>>(ent);
            if (rel != nullptr && rel->parent != ecs::tombstone && rel->parent != ecs::null)
            {
                dyn_it = _dynamic_roots.erase(dyn_it);
                continue;
            }
            ++dyn_it;
        }

        for (const auto& [self] : reg.with<ecs::self_component>())
        {
            const auto ent = self.entity;
            if (reg.try_get<scene::scene_entity_component>(ent) == nullptr)
            {
                const auto* const rel = reg.try_get<ecs::relationship_component<ecs::entity>>(ent);
                const auto is_root = (rel == nullptr || rel->parent == ecs::tombstone || rel->parent == ecs::null);
                if (is_root && tempest::find(_dynamic_roots.begin(), _dynamic_roots.end(), ent) == _dynamic_roots.end())
                {
                    _dynamic_roots.push_back(ent);
                }
            }
        }

        if (!_dynamic_roots.empty())
        {
            const auto dyn_expanded = is_dynamic_section_expanded();

            auto dyn_header = hierarchy_row{};
            dyn_header.type = row_type::dynamic_header;
            dyn_header.depth = 0;
            dyn_header.scene = {};
            dyn_header.name = "Dynamic Entities";
            dyn_header.is_expanded = dyn_expanded;
            dyn_header.has_children = !_dynamic_roots.empty();
            dyn_header.is_dirty = false;
            dyn_header.is_active_scene = false;
            dyn_header.is_prefab_instance = false;
            dyn_header.is_dynamic = true;
            dyn_header.is_networked = false;

            _visible_rows.push_back(tempest::move(dyn_header));

            if (dyn_expanded)
            {
                for (const auto dyn_root : _dynamic_roots)
                {
                    append_entity_tree(append_entity_tree, dyn_root, 1, scene::scene_handle{}, true);
                }
            }
        }

        _dirty = false;
    }
} // namespace tempest::editor
