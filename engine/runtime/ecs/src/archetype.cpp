#include "tempest/ecs_events.hpp"
#include <tempest/archetype.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/memory.hpp>
#include <tempest/string.hpp>
#include <tempest/utility.hpp>

#include <tempest/bit.hpp>

namespace tempest::ecs
{
    basic_archetype_storage::basic_archetype_storage(basic_archetype_type_info info, size_t initial_capacity)
        : _storage{info}, _size{info.size * initial_capacity}
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
        if (_data != nullptr)
        {
            aligned_free(_data);
        }
        _data = nullptr;
    }

    auto basic_archetype_storage::operator=(basic_archetype_storage&& rhs) noexcept -> basic_archetype_storage&
    {
        if (&rhs == this)
        {
            return *this;
        }

        if (_data != nullptr)
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

        auto* new_data = reinterpret_cast<byte*>(aligned_alloc(requested, _storage.alignment));
        copy_n(_data, _size, new_data);
        aligned_free(_data);

        _data = new_data;
        _size = requested;
    }

    auto basic_archetype_storage::element_at(size_t index) -> byte*
    {
        auto offset = index * _storage.size;
        return _data + offset;
    }

    auto basic_archetype_storage::element_at(size_t index) const -> const byte*
    {
        auto offset = index * _storage.size;
        return _data + offset;
    }

    void basic_archetype_storage::copy(size_t dst, size_t src)
    {
        auto* dst_p = element_at(dst);
        auto* src_p = element_at(src);
        copy_n(src_p, _storage.size, dst_p);
    }

    basic_archetype::basic_archetype(span<const basic_archetype_type_info> fields)

    {
        _storage.reserve(fields.size());

        for (const auto& field : fields)
        {
            _storage.emplace_back(field);
        }
    }

    auto basic_archetype::allocate() -> basic_archetype::key_type
    {
        if (_element_count >= _element_capacity)
        {
            // no elements in the implicit free list
            // reserve more entities

            auto new_size = tempest::bit_ceil(_element_capacity + 8); // Force a minimum of 8
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

        _trampoline[_first_free_element].index = static_cast<uint32_t>(_element_count);

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
        count = tempest::bit_ceil(count);

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

    auto basic_archetype::erase(basic_archetype::key_type key) -> bool
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

    auto basic_archetype::element_at(size_t el_index, size_t type_info_index) -> byte*
    {
        auto& s = _storage[type_info_index];
        return s.element_at(el_index);
    }

    auto basic_archetype::element_at(size_t el_index, size_t type_info_index) const -> const byte*
    {
        const auto& s = _storage[type_info_index];
        return s.element_at(el_index);
    }

    auto basic_archetype::element_at(basic_archetype::key_type key, size_t type_info_index) -> byte*
    {
        auto trampoline = _trampoline[key.index];
        if (trampoline.generation != key.generation)
        {
            return nullptr;
        }
        return _storage[type_info_index].element_at(trampoline.index);
    }

    auto basic_archetype::element_at(basic_archetype::key_type key, size_t type_info_index) const -> const byte*
    {
        auto trampoline = _trampoline[key.index];
        if (trampoline.generation != key.generation)
        {
            return nullptr;
        }
        return _storage[type_info_index].element_at(trampoline.index);
    }

    void basic_archetype_registry::destroy(basic_archetype_registry::entity_type entity)
    {
        using rel_comp_type = relationship_component<entity_type>;

        if (has<rel_comp_type>(entity))
        {
            unlink(entity);

            const auto ent_rel = get<rel_comp_type>(entity);
            auto curr_child = ent_rel.first_child;
            while (curr_child != tombstone && is_valid(curr_child) && has<rel_comp_type>(curr_child))
            {
                auto child_rel = get<rel_comp_type>(curr_child);
                const auto next_child = child_rel.next_sibling;
                child_rel.parent = tombstone;
                replace(curr_child, child_rel);
                curr_child = next_child;
            }
        }

        _names.erase(entity);

        const auto& key = _entity_archetype_mapping[entity];

        auto archetype_index = key.archetype_index;
        auto& archetype = _archetypes[archetype_index];
        archetype.erase(key.archetype_key);
        _entities.release(entity);

        _event_registry->dispatcher<entity_destroyed_event<basic_archetype_registry::entity_type>>().publish(
            entity_destroyed_event{
                .entity = entity,
            });
    }

    auto basic_archetype_registry::duplicate(basic_archetype_registry::entity_type src)
        -> basic_archetype_registry::entity_type
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
        const auto self_comp_idx = _type_registry->ensure<self_component>();
        const auto self_component_ti = _type_registry->type_info(self_comp_idx);
        const auto updated_byte =
            set_bit(static_cast<unsigned int>(hash.hash[self_component_ti.index / 8]), self_component_ti.index % 8);
        hash.hash[self_component_ti.index / 8] = static_cast<byte>(updated_byte);

        // Find the archetype
        auto* it = tempest::find(_hashes.begin(), _hashes.end(), hash);
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
            new_types.push_back(self_component_ti);

            tempest::sort(new_types.begin(), new_types.end(),
                          [](const auto& lhs, const auto& rhs) -> auto { return lhs.index < rhs.index; });

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

            auto* src_bytes = src_arch.element_at(src_key.archetype_key, src_index);
            auto* dst_bytes = new_arch.element_at(new_key, dst_index);
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

        const auto* src_rel_comp = try_get<relationship_component<basic_archetype_registry::entity_type>>(src);
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

    auto basic_archetype_registry::name(entity_type entity) const -> optional<string_view>
    {
        if (auto it = _names.find(entity); it != _names.end())
        {
            return it->second;
        }
        return none();
    }

    void basic_archetype_registry::name(entity_type entity, string_view name)
    {
        const auto it = _names.find(entity);
        if (it != _names.end() && it->second == name)
        {
            return;
        }

        tempest::string old_name_storage;
        string_view old_name_view;
        if (it != _names.end())
        {
            old_name_storage = it->second;
            old_name_view = old_name_storage;
        }

        _names[entity] = name;

        _event_registry->dispatcher<entity_renamed_event<entity_type>>().publish({
            .entity = entity,
            .old_name = old_name_view,
            .new_name = name,
        });
    }

    [[nodiscard]] auto basic_archetype_registry::find_first_with_name(string_view name) const
        -> optional<basic_archetype_registry::entity_type>
    {
        for (const auto& [entity, entity_name] : _names)
        {
            if (entity_name == name)
            {
                return entity;
            }
        }
        return none();
    }

    [[nodiscard]] auto basic_archetype_registry::find_all_with_name(string_view name) const -> vector<entity_type>
    {
        vector<entity_type> result;
        for (const auto& [entity, entity_name] : _names)
        {
            if (entity_name == name)
            {
                result.push_back(entity);
            }
        }
        return result;
    }

    namespace
    {
        using entity_type = basic_archetype_registry::entity_type;
        using rel_comp_type = relationship_component<entity_type>;

        auto find_tail_sibling(const basic_archetype_registry& reg, entity_type head) -> entity_type
        {
            auto tail = head;
            while (tail != tombstone && reg.is_valid(tail) && reg.has<rel_comp_type>(tail))
            {
                const auto next = reg.get<rel_comp_type>(tail).next_sibling;
                if (next == tombstone)
                {
                    break;
                }
                tail = next;
            }
            return tail;
        }

        auto find_prev_sibling(const basic_archetype_registry& reg, entity_type head, entity_type target) -> entity_type
        {
            auto prev = head;
            while (prev != tombstone && reg.is_valid(prev) && reg.has<rel_comp_type>(prev))
            {
                if (reg.get<rel_comp_type>(prev).next_sibling == target)
                {
                    return prev;
                }
                prev = reg.get<rel_comp_type>(prev).next_sibling;
            }
            return tombstone;
        }
    } // namespace

    auto basic_archetype_registry::reparent(entity_type child, entity_type new_parent, entity_type insert_before)
        -> bool
    {
        using rel_comp_type = relationship_component<entity_type>;

        if (child == tombstone || child == new_parent || !is_valid(child))
        {
            return false;
        }

        if (new_parent != tombstone && !is_valid(new_parent))
        {
            return false;
        }

        if (new_parent != tombstone)
        {
            const auto ancestor_view = basic_archetype_entity_ancestor_view{*this, new_parent};
            for (const auto ancestor : ancestor_view)
            {
                if (ancestor == child)
                {
                    return false;
                }
            }
        }

        if (insert_before != tombstone)
        {
            if (insert_before == child || !is_valid(insert_before) || !has<rel_comp_type>(insert_before))
            {
                return false;
            }

            const auto insert_before_rel = get<rel_comp_type>(insert_before);
            if (insert_before_rel.parent != new_parent)
            {
                return false;
            }
        }

        unlink(child);

        if (new_parent == tombstone)
        {
            if (has<rel_comp_type>(child))
            {
                auto child_rel = get<rel_comp_type>(child);
                child_rel.parent = tombstone;
                child_rel.next_sibling = tombstone;
                replace(child, child_rel);
            }
            else
            {
                assign(child, rel_comp_type{
                                  .parent = tombstone,
                                  .next_sibling = tombstone,
                                  .first_child = tombstone,
                              });
            }
            return true;
        }

        if (!has<rel_comp_type>(new_parent))
        {
            assign_or_replace(new_parent, rel_comp_type{
                                              .parent = tombstone,
                                              .next_sibling = tombstone,
                                              .first_child = tombstone,
                                          });
        }

        if (!has<rel_comp_type>(child))
        {
            assign_or_replace(child, rel_comp_type{
                                         .parent = tombstone,
                                         .next_sibling = tombstone,
                                         .first_child = tombstone,
                                     });
        }

        auto parent_rel = get<rel_comp_type>(new_parent);
        auto child_rel = get<rel_comp_type>(child);
        child_rel.parent = new_parent;

        if (insert_before == parent_rel.first_child)
        {
            child_rel.next_sibling = parent_rel.first_child;
            parent_rel.first_child = child;
            replace(child, child_rel);
            replace(new_parent, parent_rel);
        }
        else if (insert_before == tombstone)
        {
            if (parent_rel.first_child == tombstone)
            {
                child_rel.next_sibling = tombstone;
                parent_rel.first_child = child;
                replace(child, child_rel);
                replace(new_parent, parent_rel);
            }
            else
            {
                const auto tail = find_tail_sibling(*this, parent_rel.first_child);
                auto tail_rel = get<rel_comp_type>(tail);
                child_rel.next_sibling = tombstone;
                replace(child, child_rel);
                tail_rel.next_sibling = child;
                replace(tail, tail_rel);
            }
        }
        else
        {
            const auto prev = find_prev_sibling(*this, parent_rel.first_child, insert_before);
            if (prev == tombstone)
            {
                return false;
            }

            auto prev_rel = get<rel_comp_type>(prev);
            child_rel.next_sibling = insert_before;
            replace(child, child_rel);
            prev_rel.next_sibling = child;
            replace(prev, prev_rel);
        }

        return true;
    }

    auto basic_archetype_registry::reparent_head(entity_type child, entity_type new_parent) -> bool
    {
        using rel_comp_type = relationship_component<entity_type>;

        if (child == tombstone || child == new_parent || !is_valid(child))
        {
            return false;
        }

        if (new_parent != tombstone && !is_valid(new_parent))
        {
            return false;
        }

        if (new_parent != tombstone)
        {
            const auto ancestor_view = basic_archetype_entity_ancestor_view{*this, new_parent};
            for (const auto ancestor : ancestor_view)
            {
                if (ancestor == child)
                {
                    return false;
                }
            }
        }

        unlink(child);

        if (new_parent == tombstone)
        {
            if (has<rel_comp_type>(child))
            {
                auto child_rel = get<rel_comp_type>(child);
                child_rel.parent = tombstone;
                child_rel.next_sibling = tombstone;
                replace(child, child_rel);
            }
            else
            {
                assign(child, rel_comp_type{
                                  .parent = tombstone,
                                  .next_sibling = tombstone,
                                  .first_child = tombstone,
                              });
            }
            return true;
        }

        if (!has<rel_comp_type>(new_parent))
        {
            assign_or_replace(new_parent, rel_comp_type{
                                              .parent = tombstone,
                                              .next_sibling = tombstone,
                                              .first_child = tombstone,
                                          });
        }

        if (!has<rel_comp_type>(child))
        {
            assign_or_replace(child, rel_comp_type{
                                         .parent = tombstone,
                                         .next_sibling = tombstone,
                                         .first_child = tombstone,
                                     });
        }

        auto parent_rel = get<rel_comp_type>(new_parent);
        auto child_rel = get<rel_comp_type>(child);
        child_rel.parent = new_parent;
        child_rel.next_sibling = parent_rel.first_child;
        parent_rel.first_child = child;

        replace(child, child_rel);
        replace(new_parent, parent_rel);

        return true;
    }

    auto basic_archetype_registry::reparent_children(span<const entity_type> children, entity_type new_parent,
                                                     entity_type insert_before) -> bool
    {
        using rel_comp_type = relationship_component<entity_type>;

        if (children.empty())
        {
            return true;
        }

        if (children.size() == 1)
        {
            return reparent(children.front(), new_parent, insert_before);
        }

        if (new_parent != tombstone && !is_valid(new_parent))
        {
            return false;
        }

        for (const auto child : children)
        {
            if (child == tombstone || child == new_parent || !is_valid(child))
            {
                return false;
            }
        }

        if (new_parent != tombstone)
        {
            const auto ancestor_view = basic_archetype_entity_ancestor_view{*this, new_parent};
            for (const auto ancestor : ancestor_view)
            {
                for (const auto child : children)
                {
                    if (ancestor == child)
                    {
                        return false;
                    }
                }
            }
        }

        if (insert_before != tombstone)
        {
            if (!is_valid(insert_before) || !has<rel_comp_type>(insert_before))
            {
                return false;
            }

            const auto insert_before_rel = get<rel_comp_type>(insert_before);
            if (insert_before_rel.parent != new_parent)
            {
                return false;
            }

            for (const auto child : children)
            {
                if (child == insert_before)
                {
                    return false;
                }
            }
        }

        for (const auto child : children)
        {
            unlink(child);
        }

        if (new_parent == tombstone)
        {
            for (const auto child : children)
            {
                if (has<rel_comp_type>(child))
                {
                    auto child_rel = get<rel_comp_type>(child);
                    child_rel.parent = tombstone;
                    child_rel.next_sibling = tombstone;
                    replace(child, child_rel);
                }
                else
                {
                    assign(child, rel_comp_type{
                                      .parent = tombstone,
                                      .next_sibling = tombstone,
                                      .first_child = tombstone,
                                  });
                }
            }
            return true;
        }

        if (!has<rel_comp_type>(new_parent))
        {
            assign_or_replace(new_parent, rel_comp_type{
                                              .parent = tombstone,
                                              .next_sibling = tombstone,
                                              .first_child = tombstone,
                                          });
        }

        for (const auto child : children)
        {
            if (!has<rel_comp_type>(child))
            {
                assign_or_replace(child, rel_comp_type{
                                             .parent = tombstone,
                                             .next_sibling = tombstone,
                                             .first_child = tombstone,
                                         });
            }
        }

        auto child_it = children.begin();
        const auto last_it = children.end() - 1;
        while (child_it != last_it)
        {
            const auto curr_ent = *child_it;
            ++child_it;
            const auto next_ent = *child_it;

            auto curr_rel = get<rel_comp_type>(curr_ent);
            curr_rel.parent = new_parent;
            curr_rel.next_sibling = next_ent;
            replace(curr_ent, curr_rel);
        }

        const auto last_child = *last_it;
        auto last_child_rel = get<rel_comp_type>(last_child);
        last_child_rel.parent = new_parent;

        auto parent_rel = get<rel_comp_type>(new_parent);

        if (insert_before == parent_rel.first_child)
        {
            last_child_rel.next_sibling = parent_rel.first_child;
            replace(last_child, last_child_rel);
            parent_rel.first_child = children.front();
            replace(new_parent, parent_rel);
        }
        else if (insert_before == tombstone)
        {
            last_child_rel.next_sibling = tombstone;
            replace(last_child, last_child_rel);

            if (parent_rel.first_child == tombstone)
            {
                parent_rel.first_child = children.front();
                replace(new_parent, parent_rel);
            }
            else
            {
                const auto tail = find_tail_sibling(*this, parent_rel.first_child);
                auto tail_rel = get<rel_comp_type>(tail);
                tail_rel.next_sibling = children.front();
                replace(tail, tail_rel);
            }
        }
        else
        {
            const auto prev = find_prev_sibling(*this, parent_rel.first_child, insert_before);
            if (prev == tombstone)
            {
                return false;
            }

            auto prev_rel = get<rel_comp_type>(prev);
            last_child_rel.next_sibling = insert_before;
            replace(last_child, last_child_rel);
            prev_rel.next_sibling = children.front();
            replace(prev, prev_rel);
        }

        return true;
    }

    void basic_archetype_registry::unlink(entity_type child)
    {
        using rel_comp_type = relationship_component<entity_type>;

        if (child == tombstone || !is_valid(child) || !has<rel_comp_type>(child))
        {
            return;
        }

        auto child_rel = get<rel_comp_type>(child);
        const auto parent = child_rel.parent;

        if (parent != tombstone && is_valid(parent) && has<rel_comp_type>(parent))
        {
            auto parent_rel = get<rel_comp_type>(parent);
            if (parent_rel.first_child == child)
            {
                parent_rel.first_child = child_rel.next_sibling;
                replace(parent, parent_rel);
            }
            else
            {
                const auto prev = find_prev_sibling(*this, parent_rel.first_child, child);
                if (prev != tombstone)
                {
                    auto prev_rel = get<rel_comp_type>(prev);
                    prev_rel.next_sibling = child_rel.next_sibling;
                    replace(prev, prev_rel);
                }
            }
        }

        child_rel.parent = tombstone;
        child_rel.next_sibling = tombstone;
        replace(child, child_rel);
    }

    void basic_archetype_registry::destroy_recursive(entity_type root)
    {
        using rel_comp_type = relationship_component<entity_type>;

        if (root == tombstone || !is_valid(root))
        {
            return;
        }

        if (has<rel_comp_type>(root))
        {
            vector<entity_type> children;
            auto curr = get<rel_comp_type>(root).first_child;
            while (curr != tombstone && is_valid(curr) && has<rel_comp_type>(curr))
            {
                children.push_back(curr);
                curr = get<rel_comp_type>(curr).next_sibling;
            }

            for (const auto child : children)
            {
                destroy_recursive(child);
            }
        }

        destroy(root);
    }

    void basic_archetype_registry::destroy_and_reparent_children(entity_type target)
    {
        using rel_comp_type = relationship_component<entity_type>;

        if (target == tombstone || !is_valid(target))
        {
            return;
        }

        if (!has<rel_comp_type>(target))
        {
            destroy(target);
            return;
        }

        auto target_rel = get<rel_comp_type>(target);
        if (target_rel.first_child == tombstone)
        {
            destroy(target);
            return;
        }

        const auto child_head = target_rel.first_child;
        auto child_tail = child_head;
        auto curr = child_head;

        while (curr != tombstone && is_valid(curr) && has<rel_comp_type>(curr))
        {
            auto child_rel = get<rel_comp_type>(curr);
            child_rel.parent = target_rel.parent;
            replace(curr, child_rel);
            child_tail = curr;
            curr = child_rel.next_sibling;
        }

        const auto parent = target_rel.parent;
        if (parent != tombstone && is_valid(parent) && has<rel_comp_type>(parent))
        {
            auto parent_rel = get<rel_comp_type>(parent);
            if (parent_rel.first_child == target)
            {
                parent_rel.first_child = child_head;
                replace(parent, parent_rel);
            }
            else
            {
                const auto prev = find_prev_sibling(*this, parent_rel.first_child, target);
                if (prev != tombstone)
                {
                    auto prev_rel = get<rel_comp_type>(prev);
                    prev_rel.next_sibling = child_head;
                    replace(prev, prev_rel);
                }
            }

            auto tail_rel = get<rel_comp_type>(child_tail);
            tail_rel.next_sibling = target_rel.next_sibling;
            replace(child_tail, tail_rel);
        }
        else
        {
            auto tail_rel = get<rel_comp_type>(child_tail);
            tail_rel.next_sibling = target_rel.next_sibling;
            replace(child_tail, tail_rel);
        }

        target_rel.parent = tombstone;
        target_rel.first_child = tombstone;
        target_rel.next_sibling = tombstone;
        replace(target, target_rel);

        destroy(target);
    }

    auto reparent(basic_archetype_registry& reg, basic_archetype_registry::entity_type child,
                  basic_archetype_registry::entity_type new_parent,
                  basic_archetype_registry::entity_type insert_before) -> bool
    {
        return reg.reparent(child, new_parent, insert_before);
    }

    auto reparent_head(basic_archetype_registry& reg, basic_archetype_registry::entity_type child,
                       basic_archetype_registry::entity_type new_parent) -> bool
    {
        return reg.reparent_head(child, new_parent);
    }

    auto reparent_children(basic_archetype_registry& reg,
                           span<const basic_archetype_registry::entity_type> children,
                           basic_archetype_registry::entity_type new_parent,
                           basic_archetype_registry::entity_type insert_before) -> bool
    {
        return reg.reparent_children(children, new_parent, insert_before);
    }

    void unlink(basic_archetype_registry& reg, basic_archetype_registry::entity_type child)
    {
        reg.unlink(child);
    }

    void destroy_recursive(basic_archetype_registry& reg, basic_archetype_registry::entity_type root)
    {
        reg.destroy_recursive(root);
    }

    void destroy_and_reparent_children(basic_archetype_registry& reg, basic_archetype_registry::entity_type target)
    {
        reg.destroy_and_reparent_children(target);
    }

    void create_parent_child_relationship(basic_archetype_registry& reg, basic_archetype_registry::entity_type parent,
                                          basic_archetype_registry::entity_type child)
    {
        reg.reparent(child, parent, tombstone);
    }
} // namespace tempest::ecs