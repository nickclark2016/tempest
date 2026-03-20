#ifndef tempest_assets_asset_type_registry_hpp
#define tempest_assets_asset_type_registry_hpp

#include <tempest/asset_type_id.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/functional.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets
{
    class asset_database;

    using asset_deserializer_fn = function<bool(span<const byte>, const guid&, asset_database&)>;
    using asset_serializer_fn = function<bool(const guid&, const asset_database&, vector<byte>&)>;

    struct type_entry
    {
        asset_type_id id;
        string canonical_name;
        asset_deserializer_fn deserializer;
        asset_serializer_fn serializer;
    };

    class asset_type_registry
    {
      public:
        asset_type_registry() = default;

        template <typename T>
        bool register_type(asset_deserializer_fn deserializer, asset_serializer_fn serializer);

        [[nodiscard]] const type_entry* find(asset_type_id type_id) const;
        [[nodiscard]] const type_entry* find_by_name(string_view name) const;
        [[nodiscard]] optional<string_view> name_of(asset_type_id type_id) const;
        [[nodiscard]] optional<bool> validate(asset_type_id type_id, string_view name) const;

        template <typename Fn>
        void for_each(Fn&& func) const;

      private:
        vector<unique_ptr<type_entry>> _entries;
        flat_unordered_map<size_t, size_t> _hash_to_index;
        flat_unordered_map<string, size_t> _name_to_index;
    };

    template <typename T>
    bool asset_type_registry::register_type(asset_deserializer_fn deserializer, asset_serializer_fn serializer)
    {
        auto type_id = asset_type_id::of<T>();
        auto name = string(core::type_name<T>::value());

        // Check if already registered by hash
        auto hash_it = _hash_to_index.find(type_id.hash());
        if (hash_it != _hash_to_index.end())
        {
            // Idempotent: same type registered again
            const auto& existing = _entries[hash_it->second];
            if (existing->canonical_name == name)
            {
                return true;
            }
            // Hash collision with different name — reject
            return false;
        }

        // Check if already registered by name
        auto name_it = _name_to_index.find(name);
        if (name_it != _name_to_index.end())
        {
            // Name collision with different hash — reject
            return false;
        }

        auto index = _entries.size();
        auto entry = make_unique<type_entry>(type_entry{
            .id = type_id,
            .canonical_name = tempest::move(name),
            .deserializer = tempest::move(deserializer),
            .serializer = tempest::move(serializer),
        });

        string name_copy = entry->canonical_name;
        _entries.push_back(tempest::move(entry));
        _hash_to_index.insert({type_id.hash(), index});
        _name_to_index.insert({tempest::move(name_copy), index});

        return true;
    }

    template <typename Fn>
    void asset_type_registry::for_each(Fn&& func) const
    {
        for (const auto& entry : _entries)
        {
            tempest::forward<Fn>(func)(*entry);
        }
    }
} // namespace tempest::assets

#endif // tempest_assets_asset_type_registry_hpp
