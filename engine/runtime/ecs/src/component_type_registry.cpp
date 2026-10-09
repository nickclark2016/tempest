#include <tempest/component_type_registry.hpp>

namespace tempest::ecs
{
    auto component_type_registry::type_index(string_view name) const -> optional<uint32_t>
    {
        const auto resolved = resolve_alias(name);
        const auto it = _name_to_index.find(string{resolved});
        if (it != _name_to_index.end())
        {
            return it->second;
        }
        return nullopt;
    }

    auto component_type_registry::type_info(uint32_t index) const -> const component_type_info&
    {
        TEMPEST_ASSERT(index < _count);
        return _types[index];
    }

    auto component_type_registry::type_info(string_view name) const -> const component_type_info*
    {
        const auto opt_idx = type_index(name);
        if (!opt_idx.has_value())
        {
            return nullptr;
        }
        return &_types[*opt_idx];
    }

    void component_type_registry::add_alias(string_view legacy_name, string_view current_name)
    {
        _aliases[string{legacy_name}] = string{current_name};
    }

    auto component_type_registry::resolve_alias(string_view name) const -> string_view
    {
        auto current = name;
        for (size_t iteration = 0; iteration < _aliases.size(); ++iteration)
        {
            const auto it = _aliases.find(string{current});
            if (it == _aliases.end())
            {
                break;
            }
            current = string_view{it->second};
        }
        return current;
    }

    auto component_type_registry::size() const noexcept -> size_t
    {
        return _count;
    }

    auto component_type_registry::empty() const noexcept -> bool
    {
        return _count == 0;
    }

    void component_type_registry::_register_type_info(const component_type_info& info)
    {
        TEMPEST_ASSERT(_count < max_component_types);
        const auto idx = info.index;
        _types[idx] = info;
        _name_to_index[string{info.name}] = idx;
        ++_count;
    }
} // namespace tempest::ecs
