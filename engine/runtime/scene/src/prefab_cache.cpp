#include <tempest/scene/prefab_cache.hpp>

#include <tempest/scene/prefab_operations.hpp>

namespace tempest::scene
{
    auto prefab_cache::register_template(const guid& tid, prefab_template tmpl) -> bool
    {
        auto lock = unique_lock{_mutex};
        const auto iter = _templates.find(tid);
        if (iter != _templates.end())
        {
            return false;
        }
        _templates.insert({tid, tempest::move(tmpl)});
        return true;
    }

    auto prefab_cache::unregister_template(const guid& tid) -> bool
    {
        auto lock = unique_lock{_mutex};
        const auto iter = _templates.find(tid);
        if (iter != _templates.end())
        {
            _templates.erase(iter);
            return true;
        }
        return false;
    }

    auto prefab_cache::has_template(const guid& tid) const -> bool
    {
        auto lock = shared_lock{_mutex};
        return _templates.contains(tid);
    }

    auto prefab_cache::find_template(const guid& tid) const -> optional<prefab_template>
    {
        auto lock = shared_lock{_mutex};
        const auto iter = _templates.find(tid);
        if (iter != _templates.end())
        {
            return iter->second;
        }
        return nullopt;
    }

    auto prefab_cache::template_count() const -> size_t
    {
        auto lock = shared_lock{_mutex};
        return _templates.size();
    }

    auto prefab_cache::clear() -> void
    {
        auto lock = unique_lock{_mutex};
        _templates.clear();
    }

    auto prefab_cache::propagate_updates(ecs::basic_archetype_registry& registry, const guid& prefab_guid) const
        -> size_t
    {
        return prefab_operations::propagate_template_updates(registry, prefab_guid, *this);
    }
} // namespace tempest::scene
