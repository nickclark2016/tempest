#include <tempest/object_pool.hpp>

#include <cassert>

namespace tempest::core
{
    object_pool::object_pool(allocator* _alloc, std::uint32_t pool_size, std::uint32_t resource_size)
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
} // namespace tempest::core