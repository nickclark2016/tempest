#include <tempest/object_pool.hpp>

#include <cassert>

namespace tempest::core
{
    object_pool::object_pool(abstract_allocator* _alloc, std::uint32_t pool_size, std::uint32_t resource_size)
        : _alloc{_alloc}, _pool_size{pool_size}, _resource_size{resource_size}
    {
        auto alloc_size = pool_size * (_resource_size + sizeof(std::uint32_t));
        _memory = reinterpret_cast<std::byte*>(_alloc->allocate(alloc_size, 1));
        std::fill_n(_memory, alloc_size, static_cast<std::byte>(0));

        _free_indices = reinterpret_cast<std::uint32_t*>(_memory + pool_size * resource_size);
        _free_index_head = 0;

        for (std::uint32_t i = 0; i < _pool_size; ++i)
        {
            _free_indices[i] = i;
        }

        _used_index_count = 0;
    }

    object_pool::~object_pool()
    {
        // log that there are unreleased resources
        if (_free_index_head != 0)
        {
        }

        _alloc->deallocate(_memory);
        _memory = nullptr;
    }

    std::uint32_t object_pool::acquire_resource()
    {
        if (_free_index_head < _pool_size)
        {
            const auto free_index = _free_indices[_free_index_head++];
            ++_used_index_count;
            return free_index;
        }

        assert(false);
        return std::numeric_limits<std::uint32_t>::max();
    }

    void object_pool::release_resource(std::uint32_t index)
    {
        _free_indices[--_free_index_head] = index;
        --_used_index_count;
    }

    void object_pool::release_all_resources()
    {
        _free_index_head = 0;
        _used_index_count = 0;

        for (std::uint32_t i = 0; i < _pool_size; ++i)
        {
            _free_indices[i] = i;
        }
    }

    void* object_pool::access(std::uint32_t index)
    {
        if (index != std::numeric_limits<std::uint32_t>::max()) [[likely]]
        {
            return _memory + (index * _resource_size);
        }
        return nullptr;
    }

    const void* object_pool::access(std::uint32_t index) const
    {
        if (index != std::numeric_limits<std::uint32_t>::max()) [[likely]]
        {
            return _memory + (index * _resource_size);
        }
        return nullptr;
    }

    std::size_t object_pool::size() const noexcept
    {
        return _pool_size;
    }

    generational_object_pool::generational_object_pool(abstract_allocator* _alloc, std::uint32_t pool_size,
                                                       std::uint32_t resource_size)
        : _alloc{_alloc}, _pool_size{pool_size}, _resource_size{resource_size}, _used_index_count{0}
    {
        auto per_element = (resource_size + sizeof(key) + sizeof(std::uint32_t));
        auto total_size = _pool_size * per_element;

        _memory = reinterpret_cast<std::byte*>(_alloc->allocate(total_size, 16));
        std::fill_n(_memory, total_size, std::byte(0));

        _keys = reinterpret_cast<key*>(_memory);
        _erased = reinterpret_cast<std::uint32_t*>(_memory + (_pool_size * sizeof(key)));
        _payload = _memory + _pool_size * (sizeof(std::uint32_t) + sizeof(key));

        for (std::uint32_t i = 0; i < _pool_size; ++i)
        {
            auto& k = _keys[i];
            k.index = i + 1;
        }

        _free_index_head = 0;
    }

    generational_object_pool::~generational_object_pool()
    {
        _alloc->deallocate(_memory);
        _memory = nullptr;
        _erased = nullptr;
        _keys = nullptr;
        _payload = nullptr;
    }

    generational_object_pool::key generational_object_pool::acquire_resource()
    {
        if (_used_index_count != _pool_size)
        {
            auto head = _free_index_head;
            auto next = _keys[head].index;
            _free_index_head = next;

            auto& trampoline = _keys[head];
            trampoline.index = _used_index_count++;

            _erased[trampoline.index] = head;

            return key{
                .index = head,
                .generation = trampoline.generation,
            };
        }

        return invalid_key;
    }

    void generational_object_pool::release_resource(key index)
    {
        auto trampoline = _keys[index.index];
        if (index.generation == trampoline.generation)
        {
            auto to_erase = trampoline.index;

            if (to_erase != _used_index_count - 1)
            {
                auto trampoline_idx = _erased[_used_index_count - 1];
                _keys[trampoline_idx].index = trampoline_idx;
            }

            auto next_free = _free_index_head;
            _keys[index.index].index = next_free;
            _keys[index.index].generation++;
            _free_index_head = index.index;

            --_used_index_count;
        }
    }

    void generational_object_pool::release_all_resources()
    {
        _used_index_count = 0;
        _free_index_head = 0;

        for (std::size_t i = 0; i < _pool_size; ++i)
        {
            _keys[i].index = static_cast<std::uint32_t>(i) + 1;
            _keys[i].generation++;
        }
    }

    void* generational_object_pool::access(key index)
    {
        auto trampoline = _keys[index.index];
        if (trampoline.generation == index.generation)
        {
            return _payload + _resource_size * trampoline.index;
        }
        return nullptr;
    }

    const void* generational_object_pool::access(key index) const
    {
        auto trampoline = _keys[index.index];
        if (trampoline.generation == index.generation)
        {
            return _payload + _resource_size * trampoline.index;
        }
        return nullptr;
    }

    std::size_t generational_object_pool::size() const noexcept
    {
        return _pool_size;
    }
} // namespace tempest::core