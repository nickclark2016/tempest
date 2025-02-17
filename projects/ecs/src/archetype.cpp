#include <tempest/archetype.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/memory.hpp>
#include <tempest/string.hpp>
#include <tempest/utility.hpp>

#include <bit>

namespace tempest::ecs
{
    basic_archetype_storage::basic_archetype_storage(basic_archetype_type_info info, size_t initial_capacity)
        : _storage{info}, _data{nullptr}, _size{info.size * initial_capacity}
    {
        reserve(initial_capacity);
    }

    basic_archetype_storage::basic_archetype_storage(basic_archetype_storage&& rhs) noexcept
        : _storage{tempest::move(rhs._storage)}, _data{tempest::exchange(rhs._data, nullptr)},
          _size{tempest::exchange(rhs._size, 0)}
    {
    }

    basic_archetype_storage::~basic_archetype_storage()
    {
        if (_data)
        {
            aligned_free(_data);
        }
        _data = nullptr;
    }

    basic_archetype_storage& basic_archetype_storage::operator=(basic_archetype_storage&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        if (_data)
        {
            aligned_free(_data);
        }

        _data = nullptr;

        _data = tempest::exchange(rhs._data, nullptr);
        _size = tempest::exchange(rhs._size, 0);
        _storage = tempest::exchange(rhs._storage, {});

        return *this;
    }

    void basic_archetype_storage::reserve(size_t count)
    {
        auto requested = count * _storage.size;
        if (requested <= _size)
        {
            return;
        }

        auto new_data = reinterpret_cast<byte*>(aligned_alloc(requested, _storage.alignment));
        copy_n(_data, _size, new_data);
        aligned_free(_data);

        _data = new_data;
        _size = requested;
    }

    byte* basic_archetype_storage::element_at(size_t index)
    {
        auto offset = index * _storage.size;
        return _data + offset;
    }

    const byte* basic_archetype_storage::element_at(size_t index) const
    {
        auto offset = index * _storage.size;
        return _data + offset;
    }

    void basic_archetype_storage::copy(size_t dst, size_t src)
    {
        auto dst_p = element_at(dst);
        auto src_p = element_at(src);
        copy_n(src_p, _storage.size, dst_p);
    }

    basic_archetype::basic_archetype(span<const basic_archetype_type_info> fields)
        : _element_count{0}, _element_capacity{0}, _first_free_element{0}
    {
        _storage.reserve(fields.size());

        for (const auto& field : fields)
        {
            _storage.emplace_back(field);
        }
    }

    typename basic_archetype::key_type basic_archetype::allocate()
    {
        if (_element_count >= _element_capacity)
        {
            // no elements in the implicit free list
            // reserve more entities

            auto new_size = std::bit_ceil(_element_capacity + 8); // Force a minimum of 8
            _trampoline.reserve(new_size);
            _look_back_table.reserve(new_size);

            for (size_t idx = _element_capacity; idx < new_size; ++idx)
            {
                auto next_idx = idx + 1;

                key_type next_key = {
                    .index = static_cast<uint32_t>(next_idx),
                    .generation = 0,
                };

                _trampoline.push_back(next_key);
                _look_back_table.push_back(static_cast<uint32_t>(idx));
            }

            _first_free_element = _element_capacity;

            _element_capacity = new_size;

            for (auto& storage : _storage)
            {
                storage.reserve(new_size);
            }
        }

        // Pop off the front of the implicit free list
        auto index = static_cast<uint32_t>(_first_free_element);
        auto trampoline = _trampoline[_first_free_element];
        auto next_index = trampoline.index;

        auto new_key = key_type{
            .index = index,
            .generation = trampoline.generation,
        };

        _trampoline[_first_free_element].index = index;

        _first_free_element = next_index;

        ++_element_count;

        return new_key;
    }

    void basic_archetype::reserve(size_t count)
    {
        if (count < _element_capacity)
        {
            return;
        }

        // Round up to a power of 2
        count = std::bit_ceil(count);

        _trampoline.reserve(count);
        _look_back_table.reserve(count);

        for (size_t idx = _element_capacity; idx < count; ++idx)
        {
            auto next_idx = idx + 1;

            key_type next_key = {
                .index = static_cast<uint32_t>(next_idx),
                .generation = 0,
            };

            _trampoline.push_back(next_key);
            _look_back_table.push_back(static_cast<uint32_t>(idx));
        }

        // Patch together the free lists
        // If previously full, the first free element already points at the head
        // Else, point the last element of the new chain at the first element of the old chain,
        // and reset the first element
        if (_element_count != _element_capacity)
        {
            _trampoline[count - 1].index = static_cast<uint32_t>(_first_free_element);
            _first_free_element = _element_capacity;
        }

        for (auto& storage : _storage)
        {
            storage.reserve(count);
        }

        _element_capacity = count;
    }

    bool basic_archetype::erase(typename basic_archetype::key_type key)
    {
        auto& trampoline = _trampoline[key.index];
        if (trampoline.generation != key.generation)
        {
            return false;
        }

        auto index_to_erase = trampoline.index;
        auto index_to_move = _element_count - 1;

        // If the index to erase is the same as the index to move, just destroy
        // Else, move the target element to the index of the erased element, update the lookback
        if (index_to_erase != index_to_move)
        {
        }
        else
        {
            for (auto& s : _storage)
            {
                s.copy(index_to_erase, index_to_move);
            }
            _look_back_table[index_to_erase] = key.index;
        }

        --_element_count;

        trampoline.generation++;
        trampoline.index = static_cast<uint32_t>(_first_free_element);
        _first_free_element = key.index;

        return true;
    }

    byte* basic_archetype::element_at(size_t el_index, size_t type_info_index)
    {
        auto& s = _storage[type_info_index];
        return s.element_at(el_index);
    }

    const byte* basic_archetype::element_at(size_t el_index, size_t type_info_index) const
    {
        const auto& s = _storage[type_info_index];
        return s.element_at(el_index);
    }

    byte* basic_archetype::element_at(typename basic_archetype::key_type key, size_t type_info_index)
    {
        auto trampoline = _trampoline[key.index];
        if (trampoline.generation != key.generation)
        {
            return nullptr;
        }
        return _storage[type_info_index].element_at(trampoline.index);
    }

    const byte* basic_archetype::element_at(typename basic_archetype::key_type key, size_t type_info_index) const
    {
        auto trampoline = _trampoline[key.index];
        if (trampoline.generation != key.generation)
        {
            return nullptr;
        }
        return _storage[type_info_index].element_at(trampoline.index);
    }

    namespace detail
    {
        size_t get_archetype_type_index(string_view name)
        {
            static flat_unordered_map<string, size_t> type_index_map;
            static size_t next_index = 0;
            auto it = type_index_map.find(name);
            if (it != type_index_map.end())
            {
                return it->second;
            }
            type_index_map[name] = next_index;
            return next_index++;
        }
    } // namespace detail

    void basic_archetype_registry::destroy(typename basic_archetype_registry::entity_type entity)
    {
        const auto& key = _entity_archetype_mapping[entity];

        auto archetype_index = key.archetype_index;
        auto& archetype = _archetypes[archetype_index];
        archetype.erase(key.archetype_key);
        _entities.release(entity);
    }

    typename basic_archetype_registry::entity_type basic_archetype_registry::duplicate(
        typename basic_archetype_registry::entity_type src)
    {
        auto src_key = _entity_archetype_mapping[src];
        auto& src_arch = _archetypes[src_key.archetype_index];

        // Create a hash of the archetype without the non-duplicatable components
        auto hash = _hashes[src_key.archetype_index];
        for (size_t i = 0; i < src_arch.storages().size(); ++i)
        {
            if (!src_arch.storages()[i].type_info().should_duplicate)
            {
                hash.hash[src_arch.storages()[i].type_info().index / 8] &=
                    static_cast<byte>(~(1 << (src_arch.storages()[i].type_info().index % 8)));
            }
        }

        // We will always have a self_component, so add that to the hash
        static const auto self_component_ti = create_archetype_type_info<self_component>();
        const auto updated_byte = set_bit(static_cast<unsigned int>(hash.hash[self_component_ti.index / 8]), self_component_ti.index % 8);
        hash.hash[self_component_ti.index / 8] = static_cast<byte>(updated_byte);

        // Find the archetype
        auto it = tempest::find(_hashes.begin(), _hashes.end(), hash);
        if (it == _hashes.end())
        {
            // Create a new archetype
            auto existing_storage_view = src_arch.storages();
            vector<basic_archetype_type_info> new_types;
            for (const auto& storage : existing_storage_view)
            {
                if (storage.type_info().should_duplicate)
                {
                    new_types.push_back(storage.type_info());
                }
            }

            // Ensure self component exists
            new_types.push_back(create_archetype_type_info<self_component>());

            std::sort(new_types.begin(), new_types.end(),
                      [](const auto& lhs, const auto& rhs) { return lhs.index < rhs.index; });

            _archetypes.emplace_back(new_types);
            _hashes.push_back(hash);
            it = _hashes.end() - 1;
        }

        auto new_archetype_index = tempest::distance(_hashes.begin(), it);

        auto& new_arch = _archetypes[new_archetype_index];
        auto new_key = new_arch.allocate();

        // Copy the entity's data to the new archetype, skipping the non-duplicatable components
        for (const auto& storage : new_arch.storages())
        {
            const auto index = storage.type_info().index;
            auto src_index = _index_of_component_in_archetype(src_key.archetype_index, index);
            auto dst_index = _index_of_component_in_archetype(new_archetype_index, index);

            auto src_bytes = src_arch.element_at(src_key.archetype_key, src_index);
            auto dst_bytes = new_arch.element_at(new_key, dst_index);
            copy_n(src_bytes, storage.type_info().size, dst_bytes);
        }

        // Create the new entity
        basic_archetype_entity entity_payload = {
            .archetype_key = new_key,
            .archetype_index = static_cast<uint32_t>(new_archetype_index),
        };

        auto result = _entities.acquire();
        _entity_archetype_mapping.insert(result, entity_payload);

        replace(result, self_component{
                            .entity = result,
                        });

        // If the entity has a name, copy the name to the new entity
        if (auto n = name(src); n.has_value())
        {
            name(result, *n);
        }

        auto src_rel_comp = try_get<relationship_component<basic_archetype_registry::entity_type>>(src);
        if (src_rel_comp != nullptr && src_rel_comp->first_child != tombstone)
        {
            auto child = src_rel_comp->first_child;
            while (child != tombstone)
            {
                auto dup_child = duplicate(child);
                create_parent_child_relationship(*this, result, dup_child);

                auto sibling =
                    try_get<relationship_component<basic_archetype_registry::entity_type>>(child)->next_sibling;
                child = sibling;
            }
        }

        return result;
    }

    optional<string_view> basic_archetype_registry::name(entity_type entity) const
    {
        if (auto it = _names.find(entity); it != _names.end())
        {
            return it->second;
        }
        return none();
    }

    void basic_archetype_registry::name(entity_type entity, string_view name)
    {
        _names[entity] = name;
    }

    void create_parent_child_relationship(basic_archetype_registry& reg, basic_archetype_registry::entity_type parent,
                                          basic_archetype_registry::entity_type child)
    {
        using rel_comp_type = relationship_component<basic_archetype_registry::entity_type>;

        // If the parent does not have a relationship component, create one
        if (!reg.has<rel_comp_type>(parent))
        {
            rel_comp_type rel{
                .parent = tombstone,
                .next_sibling = tombstone,
                .first_child = tombstone,
            };

            reg.assign_or_replace(parent, rel);
        }

        // If the child does not have a relationship component, create one
        if (!reg.has<rel_comp_type>(child))
        {
            rel_comp_type rel{
                .parent = parent,
                .next_sibling = tombstone,
                .first_child = tombstone,
            };

            reg.assign_or_replace(child, rel);
        }

        auto& opt_parent_rel = reg.get<rel_comp_type>(parent);
        auto& opt_child_rel = reg.get<rel_comp_type>(child);

        // If the parent has no children, set the child as the first child
        // And the parent as the parent of the child
        if (opt_parent_rel.first_child == ecs::tombstone)
        {
            opt_parent_rel.first_child = child;
            opt_child_rel.parent = parent;
        }
        else
        {
            // Otherwise, set the first child of the parent as the next sibling of the child
            // And the child as the first child of the parent
            opt_child_rel.next_sibling = opt_parent_rel.first_child;
            opt_child_rel.parent = parent;
            opt_parent_rel.first_child = child;
        }
    }
} // namespace tempest::ecs