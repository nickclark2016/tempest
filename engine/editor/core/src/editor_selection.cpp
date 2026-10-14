#include <tempest/editor_selection.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/utility.hpp>

namespace tempest::editor
{
    editor_selection::editor_selection(const editor_selection& other)
        : _selected_addresses{other._selected_addresses}, _selected_entities{other._selected_entities},
          _address_to_entity{other._address_to_entity}, _primary_address{other._primary_address},
          _primary_entity{other._primary_entity}
    {
        if (other._events != nullptr)
        {
            attach_event_registry(*other._events);
        }
    }

    editor_selection::editor_selection(editor_selection&& other) noexcept
        : _selected_addresses{tempest::exchange(other._selected_addresses, {})},
          _selected_entities{tempest::exchange(other._selected_entities, {})},
          _address_to_entity{tempest::exchange(other._address_to_entity, {})},
          _primary_address{tempest::exchange(other._primary_address, nullopt)},
          _primary_entity{tempest::exchange(other._primary_entity, default_primary_entity)}
    {
        auto* events = other._events;
        other.detach_event_registry();
        if (events != nullptr)
        {
            attach_event_registry(*events);
        }
    }

    auto editor_selection::operator=(const editor_selection& other) -> editor_selection&
    {
        if (this == &other)
        {
            return *this;
        }

        detach_event_registry();
        _selected_addresses = other._selected_addresses;
        _selected_entities = other._selected_entities;
        _address_to_entity = other._address_to_entity;
        _primary_address = other._primary_address;
        _primary_entity = other._primary_entity;
        if (other._events != nullptr)
        {
            attach_event_registry(*other._events);
        }
        return *this;
    }

    auto editor_selection::operator=(editor_selection&& other) noexcept -> editor_selection&
    {
        if (this == &other)
        {
            return *this;
        }

        detach_event_registry();
        _selected_addresses = tempest::exchange(other._selected_addresses, {});
        _selected_entities = tempest::exchange(other._selected_entities, {});
        _address_to_entity = tempest::exchange(other._address_to_entity, {});
        _primary_address = tempest::exchange(other._primary_address, nullopt);
        _primary_entity = tempest::exchange(other._primary_entity, default_primary_entity);
        auto* events = other._events;
        other.detach_event_registry();
        if (events != nullptr)
        {
            attach_event_registry(*events);
        }
        return *this;
    }

    editor_selection::~editor_selection()
    {
        detach_event_registry();
    }

    auto editor_selection::set_selection(entity_address addr, ecs::entity ent) -> void
    {
        clear_selection();

        if (addr.id_path.empty() && ent == ecs::tombstone)
        {
            return;
        }

        _selected_addresses.push_back(addr);
        if (ent != ecs::tombstone && ent != ecs::null)
        {
            _selected_entities.push_back(ent);
            _address_to_entity[addr] = ent;
        }

        _primary_address = addr;
        _primary_entity = ent;
    }

    auto editor_selection::set_selection(ecs::entity ent, const ecs::basic_archetype_registry& reg,
                                         const scene::scene_manager* scene_mgr) -> void
    {
        if (ent == ecs::tombstone || ent == ecs::null || !reg.is_valid(ent))
        {
            clear_selection();
            return;
        }

        auto addr = compute_entity_address(reg, ent);
        if (scene_mgr != nullptr && !addr.scene.is_valid())
        {
            addr.scene = scene_mgr->get_active_scene();
        }
        set_selection(addr, ent);
    }

    auto editor_selection::toggle_selection(entity_address addr, ecs::entity ent) -> void
    {
        if (is_selected(addr))
        {
            remove_from_selection(addr);
        }
        else
        {
            add_to_selection(tempest::move(addr), ent);
        }
    }

    auto editor_selection::select_range(span<const entity_address> addrs, span<const ecs::entity> ents) -> void
    {
        for (auto idx = size_t{0}; idx < addrs.size(); ++idx)
        {
            const auto ent = (idx < ents.size()) ? ents[idx] : ecs::tombstone;
            add_to_selection(addrs[idx], ent);
        }
    }

    auto editor_selection::add_to_selection(entity_address addr, ecs::entity ent) -> void
    {
        const auto already_selected = is_selected(addr);
        if (!already_selected)
        {
            _selected_addresses.push_back(addr);
        }

        if (ent != ecs::tombstone && ent != ecs::null)
        {
            _address_to_entity[addr] = ent;
            auto* const found_ent = tempest::find(_selected_entities.begin(), _selected_entities.end(), ent);
            if (found_ent == _selected_entities.end())
            {
                _selected_entities.push_back(ent);
            }
        }
        else if (already_selected)
        {
            const auto iter = _address_to_entity.find(addr);
            if (iter != _address_to_entity.end())
            {
                ent = iter->second;
            }
        }

        _primary_address = addr;
        _primary_entity = ent;
    }

    auto editor_selection::remove_from_selection(const entity_address& addr) -> void
    {
        auto* const it_addr = tempest::find(_selected_addresses.begin(), _selected_addresses.end(), addr);
        if (it_addr == _selected_addresses.end())
        {
            return;
        }

        const auto was_primary = _primary_address.has_value() && (*_primary_address == addr);
        _selected_addresses.erase(it_addr);

        const auto it_map = _address_to_entity.find(addr);
        if (it_map != _address_to_entity.end())
        {
            const auto ent = it_map->second;
            _address_to_entity.erase(it_map);
            auto* const it_ent = tempest::find(_selected_entities.begin(), _selected_entities.end(), ent);
            if (it_ent != _selected_entities.end())
            {
                _selected_entities.erase(it_ent);
            }
            if (_primary_entity == ent && !was_primary)
            {
                _primary_entity = ecs::tombstone;
            }
        }

        if (was_primary)
        {
            if (!_selected_addresses.empty())
            {
                _primary_address = _selected_addresses.back();
                const auto next_it = _address_to_entity.find(*_primary_address);
                _primary_entity = (next_it != _address_to_entity.end()) ? next_it->second : ecs::tombstone;
            }
            else
            {
                _primary_address = nullopt;
                _primary_entity = ecs::tombstone;
            }
        }
    }

    auto editor_selection::clear_selection() -> void
    {
        _selected_addresses.clear();
        _selected_entities.clear();
        _address_to_entity.clear();
        _primary_address = nullopt;
        _primary_entity = default_primary_entity;
    }

    auto editor_selection::is_selected(ecs::entity ent) const noexcept -> bool
    {
        if (ent == ecs::tombstone || ent == ecs::null)
        {
            return false;
        }
        return tempest::find(_selected_entities.begin(), _selected_entities.end(), ent) != _selected_entities.end();
    }

    auto editor_selection::is_selected(const entity_address& addr) const noexcept -> bool
    {
        return tempest::find(_selected_addresses.begin(), _selected_addresses.end(), addr) != _selected_addresses.end();
    }

    auto editor_selection::get_primary_entity() const noexcept -> ecs::entity
    {
        return _primary_entity;
    }

    auto editor_selection::get_primary_address() const noexcept -> optional<entity_address>
    {
        return _primary_address;
    }

    auto editor_selection::get_selected_entities() const noexcept -> span<const ecs::entity>
    {
        return span<const ecs::entity>{_selected_entities.data(), _selected_entities.size()};
    }

    auto editor_selection::get_selected_addresses() const noexcept -> span<const entity_address>
    {
        return span<const entity_address>{_selected_addresses.data(), _selected_addresses.size()};
    }

    auto editor_selection::count() const noexcept -> size_t
    {
        return _selected_addresses.size();
    }

    auto editor_selection::is_empty() const noexcept -> bool
    {
        return _selected_addresses.empty();
    }

    auto editor_selection::re_resolve(const ecs::basic_archetype_registry& reg, const scene::scene_manager& scene_mgr)
        -> void
    {
        _selected_entities.clear();
        _address_to_entity.clear();

        for (const auto& addr : _selected_addresses)
        {
            const auto live = resolve_entity_address(reg, scene_mgr, addr);
            if (live != ecs::tombstone && live != ecs::null && reg.is_valid(live))
            {
                _selected_entities.push_back(live);
                _address_to_entity[addr] = live;
            }
        }

        if (_primary_address.has_value())
        {
            const auto live_primary = resolve_entity_address(reg, scene_mgr, *_primary_address);
            if (live_primary != ecs::tombstone && live_primary != ecs::null && reg.is_valid(live_primary))
            {
                _primary_entity = live_primary;
            }
            else
            {
                _primary_entity = ecs::tombstone;
            }
        }
        else
        {
            _primary_entity = ecs::tombstone;
        }
    }

    auto editor_selection::on_entity_destroyed(ecs::entity ent) -> void
    {
        if (ent == ecs::tombstone || ent == ecs::null)
        {
            return;
        }

        auto* iter = tempest::find(_selected_entities.begin(), _selected_entities.end(), ent);
        while (iter != _selected_entities.end())
        {
            _selected_entities.erase(iter);
            iter = tempest::find(_selected_entities.begin(), _selected_entities.end(), ent);
        }

        for (auto map_it = _address_to_entity.begin(); map_it != _address_to_entity.end();)
        {
            if (map_it->second == ent)
            {
                map_it = _address_to_entity.erase(map_it);
            }
            else
            {
                ++map_it;
            }
        }

        if (_primary_entity == ent)
        {
            _primary_entity = ecs::tombstone;
        }
    }

    auto editor_selection::purge_destroyed(const ecs::basic_archetype_registry& reg,
                                           const scene::scene_manager& scene_mgr) -> void
    {
        auto surviving_addresses = vector<entity_address>{};
        for (const auto& addr : _selected_addresses)
        {
            const auto live = resolve_entity_address(reg, scene_mgr, addr);
            if (live != ecs::tombstone && live != ecs::null && reg.is_valid(live))
            {
                surviving_addresses.push_back(addr);
            }
        }
        _selected_addresses = tempest::move(surviving_addresses);

        if (_primary_address.has_value())
        {
            const auto live_primary = resolve_entity_address(reg, scene_mgr, *_primary_address);
            if (live_primary == ecs::tombstone || live_primary == ecs::null || !reg.is_valid(live_primary))
            {
                if (!_selected_addresses.empty())
                {
                    _primary_address = _selected_addresses.back();
                }
                else
                {
                    _primary_address = nullopt;
                }
            }
        }

        re_resolve(reg, scene_mgr);
    }

    auto editor_selection::on_scene_unloaded(scene::scene_handle handle) -> void
    {
        auto surviving_addresses = vector<entity_address>{};
        for (const auto& addr : _selected_addresses)
        {
            if (addr.scene == handle)
            {
                const auto map_it = _address_to_entity.find(addr);
                if (map_it != _address_to_entity.end())
                {
                    const auto ent = map_it->second;
                    _address_to_entity.erase(map_it);
                    auto* const ent_it = tempest::find(_selected_entities.begin(), _selected_entities.end(), ent);
                    if (ent_it != _selected_entities.end())
                    {
                        _selected_entities.erase(ent_it);
                    }
                }
            }
            else
            {
                surviving_addresses.push_back(addr);
            }
        }
        _selected_addresses = tempest::move(surviving_addresses);

        if (_primary_address.has_value() && _primary_address->scene == handle)
        {
            if (!_selected_addresses.empty())
            {
                _primary_address = _selected_addresses.back();
                const auto next_it = _address_to_entity.find(*_primary_address);
                _primary_entity = (next_it != _address_to_entity.end()) ? next_it->second : ecs::tombstone;
            }
            else
            {
                _primary_address = nullopt;
                _primary_entity = ecs::tombstone;
            }
        }
    }

    auto editor_selection::attach_event_registry(event::event_registry& events) -> void
    {
        if (_events == &events &&
            _entity_destroyed_sub != event::null_subscription<ecs::entity_destroyed_event<ecs::entity>>)
        {
            return;
        }

        detach_event_registry();
        _events = &events;
        _entity_destroyed_sub = _events->dispatcher<ecs::entity_destroyed_event<ecs::entity>>().subscribe(
            [this](const ecs::entity_destroyed_event<ecs::entity>& evt) -> void { on_entity_destroyed(evt.entity); });
    }

    auto editor_selection::detach_event_registry() -> void
    {
        if (_events != nullptr)
        {
            if (_entity_destroyed_sub != event::null_subscription<ecs::entity_destroyed_event<ecs::entity>>)
            {
                [[maybe_unused]] const auto unsubscribed =
                    _events->dispatcher<ecs::entity_destroyed_event<ecs::entity>>().unsubscribe(_entity_destroyed_sub);
                _entity_destroyed_sub = {};
            }
            _events = nullptr;
        }
    }
} // namespace tempest::editor
