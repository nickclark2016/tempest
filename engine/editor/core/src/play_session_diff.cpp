#include <tempest/play_session_diff.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/utility.hpp>

namespace tempest::editor
{
    play_session_diff::~play_session_diff()
    {
        detach();
    }

    play_session_diff::play_session_diff(play_session_diff&& other) noexcept
        : _spawned_entities{tempest::move(other._spawned_entities)},
          _added_components{tempest::move(other._added_components)},
          _removed_components{tempest::move(other._removed_components)}
    {
        if (other._events != nullptr)
        {
            auto* const events = other._events;
            other.detach();
            attach(*events);
        }
    }

    auto play_session_diff::operator=(play_session_diff&& other) noexcept -> play_session_diff&
    {
        if (this == &other)
        {
            return *this;
        }

        detach();

        _spawned_entities = tempest::move(other._spawned_entities);
        _added_components = tempest::move(other._added_components);
        _removed_components = tempest::move(other._removed_components);

        if (other._events != nullptr)
        {
            auto* const events = other._events;
            other.detach();
            attach(*events);
        }

        return *this;
    }

    auto play_session_diff::attach(event::event_registry& events) -> void
    {
        if (_events == &events)
        {
            return;
        }

        detach();
        _events = &events;

        _entity_created_sub = _events->dispatcher<ecs::entity_created_event<ecs::entity>>().subscribe(
            [this](const ecs::entity_created_event<ecs::entity>& evt) {
                if (tempest::find(_spawned_entities.begin(), _spawned_entities.end(), evt.entity) ==
                    _spawned_entities.end())
                {
                    _spawned_entities.push_back(evt.entity);
                }
            });

        _entity_destroyed_sub = _events->dispatcher<ecs::entity_destroyed_event<ecs::entity>>().subscribe(
            [this](const ecs::entity_destroyed_event<ecs::entity>& evt) {
                const auto it = tempest::find(_spawned_entities.begin(), _spawned_entities.end(), evt.entity);
                if (it != _spawned_entities.end())
                {
                    _spawned_entities.erase(it);
                }
                _added_components.erase(evt.entity);
                _removed_components.erase(evt.entity);
            });

        _component_added_sub = _events->dispatcher<ecs::entity_component_added_event<ecs::entity>>().subscribe(
            [this](const ecs::entity_component_added_event<ecs::entity>& evt) {
                auto was_previously_removed = false;
                const auto rem_it = _removed_components.find(evt.entity);
                if (rem_it != _removed_components.end())
                {
                    auto& removed = rem_it->second;
                    const auto it = tempest::find(removed.begin(), removed.end(), evt.component_type_index);
                    if (it != removed.end())
                    {
                        removed.erase(it);
                        if (removed.empty())
                        {
                            _removed_components.erase(rem_it);
                        }
                        was_previously_removed = true;
                    }
                }
                if (!was_previously_removed)
                {
                    auto& added = _added_components[evt.entity];
                    if (tempest::find(added.begin(), added.end(), evt.component_type_index) == added.end())
                    {
                        added.push_back(evt.component_type_index);
                    }
                }
            });

        _component_removed_sub = _events->dispatcher<ecs::entity_component_removed_event<ecs::entity>>().subscribe(
            [this](const ecs::entity_component_removed_event<ecs::entity>& evt) {
                auto was_previously_added = false;
                const auto add_it = _added_components.find(evt.entity);
                if (add_it != _added_components.end())
                {
                    auto& added = add_it->second;
                    const auto it = tempest::find(added.begin(), added.end(), evt.component_type_index);
                    if (it != added.end())
                    {
                        added.erase(it);
                        if (added.empty())
                        {
                            _added_components.erase(add_it);
                        }
                        was_previously_added = true;
                    }
                }
                if (!was_previously_added && !is_spawned(evt.entity))
                {
                    auto& removed = _removed_components[evt.entity];
                    if (tempest::find(removed.begin(), removed.end(), evt.component_type_index) == removed.end())
                    {
                        removed.push_back(evt.component_type_index);
                    }
                }
            });
    }

    auto play_session_diff::detach() -> void
    {
        if (_events == nullptr)
        {
            return;
        }

        if (_entity_created_sub != event::null_subscription<ecs::entity_created_event<ecs::entity>>)
        {
            [[maybe_unused]] const auto unsub =
                _events->dispatcher<ecs::entity_created_event<ecs::entity>>().unsubscribe(_entity_created_sub);
            _entity_created_sub = event::null_subscription<ecs::entity_created_event<ecs::entity>>;
        }
        if (_entity_destroyed_sub != event::null_subscription<ecs::entity_destroyed_event<ecs::entity>>)
        {
            [[maybe_unused]] const auto unsub =
                _events->dispatcher<ecs::entity_destroyed_event<ecs::entity>>().unsubscribe(_entity_destroyed_sub);
            _entity_destroyed_sub = event::null_subscription<ecs::entity_destroyed_event<ecs::entity>>;
        }
        if (_component_added_sub != event::null_subscription<ecs::entity_component_added_event<ecs::entity>>)
        {
            [[maybe_unused]] const auto unsub =
                _events->dispatcher<ecs::entity_component_added_event<ecs::entity>>().unsubscribe(_component_added_sub);
            _component_added_sub = event::null_subscription<ecs::entity_component_added_event<ecs::entity>>;
        }
        if (_component_removed_sub != event::null_subscription<ecs::entity_component_removed_event<ecs::entity>>)
        {
            [[maybe_unused]] const auto unsub =
                _events->dispatcher<ecs::entity_component_removed_event<ecs::entity>>().unsubscribe(_component_removed_sub);
            _component_removed_sub = event::null_subscription<ecs::entity_component_removed_event<ecs::entity>>;
        }

        _events = default_events;
    }

    auto play_session_diff::clear() noexcept -> void
    {
        _spawned_entities.clear();
        _added_components.clear();
        _removed_components.clear();
    }

    auto play_session_diff::is_spawned(ecs::entity entity) const noexcept -> bool
    {
        return tempest::find(_spawned_entities.begin(), _spawned_entities.end(), entity) != _spawned_entities.end();
    }

    auto play_session_diff::get_spawned_entities() const noexcept -> span<const ecs::entity>
    {
        return span<const ecs::entity>{_spawned_entities.data(), _spawned_entities.size()};
    }

    auto play_session_diff::is_component_added(ecs::entity entity, uint32_t type_index) const noexcept -> bool
    {
        const auto it = _added_components.find(entity);
        if (it == _added_components.end())
        {
            return false;
        }
        return tempest::find(it->second.begin(), it->second.end(), type_index) != it->second.end();
    }

    auto play_session_diff::get_added_components(ecs::entity entity) const -> span<const uint32_t>
    {
        const auto it = _added_components.find(entity);
        if (it == _added_components.end())
        {
            return {};
        }
        return span<const uint32_t>{it->second.data(), it->second.size()};
    }

    auto play_session_diff::is_component_removed(ecs::entity entity, uint32_t type_index) const noexcept -> bool
    {
        const auto it = _removed_components.find(entity);
        if (it == _removed_components.end())
        {
            return false;
        }
        return tempest::find(it->second.begin(), it->second.end(), type_index) != it->second.end();
    }

    auto play_session_diff::get_removed_components(ecs::entity entity) const -> span<const uint32_t>
    {
        const auto it = _removed_components.find(entity);
        if (it == _removed_components.end())
        {
            return {};
        }
        return span<const uint32_t>{it->second.data(), it->second.size()};
    }
} // namespace tempest::editor
