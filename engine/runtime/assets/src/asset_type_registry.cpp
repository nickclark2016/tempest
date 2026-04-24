#include <tempest/asset_type_registry.hpp>

namespace tempest::assets
{
    auto asset_type_registry::find(asset_type_id type_id) const -> const type_entry*
    {
        auto iter = _hash_to_index.find(type_id.hash());
        if (iter != _hash_to_index.end())
        {
            return _entries[iter->second].get();
        }
        return nullptr;
    }

    auto asset_type_registry::find_by_name(string_view name) const -> const type_entry*
    {
        auto iter = _name_to_index.find(string(name));
        if (iter != _name_to_index.end())
        {
            return _entries[iter->second].get();
        }
        return nullptr;
    }

    auto asset_type_registry::name_of(asset_type_id type_id) const -> optional<string_view>
    {
        const auto* entry = find(type_id);
        if (entry != nullptr)
        {
            return string_view(entry->canonical_name);
        }
        return nullopt;
    }

    auto asset_type_registry::validate(asset_type_id type_id, string_view name) const -> optional<bool>
    {
        const auto* entry = find(type_id);
        if (entry == nullptr)
        {
            return nullopt;
        }
        return string_view(entry->canonical_name) == name;
    }
} // namespace tempest::assets
