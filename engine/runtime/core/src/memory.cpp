#include <tempest/algorithm.hpp>
#include <tempest/memory.hpp>
#include <tempest/utility.hpp>

#include <tlsf/tlsf.h>

#include <cassert>
#include <cstdlib>

namespace tempest
{
    namespace
    {
        auto align_memory(size_t sz, size_t align) -> size_t
        {
            const auto _mask = align - 1;
            return (sz + _mask) & ~_mask;
        }
    } // namespace

    stack_allocator::stack_allocator(size_t bytes)
        : _buffer{reinterpret_cast<byte*>(::malloc(bytes))}, _capacity{bytes}
    {
    }

    stack_allocator::stack_allocator(stack_allocator&& other) noexcept
        : _buffer{other._buffer}, _capacity{other._capacity}, _allocated_bytes{other._allocated_bytes}
    {
        other._buffer = nullptr;
        other._allocated_bytes = 0;
        other._capacity = 0;
    }

    stack_allocator::~stack_allocator()
    {
        release();
    }

    auto stack_allocator::operator=(stack_allocator&& rhs) noexcept -> stack_allocator&
    {
        if (&rhs == this) [[unlikely]]
        {
            return *this;
        }

        release();

        tempest::swap(_buffer, rhs._buffer);
        tempest::swap(_capacity, rhs._capacity);
        tempest::swap(_allocated_bytes, rhs._allocated_bytes);

        return *this;
    }

    // TODO: Investigate bump down allocation instead of bump up
    auto stack_allocator::allocate(size_t size, size_t alignment, [[maybe_unused]] source_location loc) -> void*
    {
        if (size == 0)
        {
            return nullptr;
        }
        const auto start = align_memory(_allocated_bytes, alignment);
        assert(start < _capacity && "tempest::core::stack_allocator out of memory.");
        const auto new_allocated_byte_count = start + size;
        if (new_allocated_byte_count > _capacity)
        {
            return nullptr;
        }
        _allocated_bytes = new_allocated_byte_count;
        return _buffer + start;
    }

    void stack_allocator::deallocate(void* ptr)
    {
        assert(ptr > _buffer);                    // Tried to release memory from before allocated region
        assert(ptr < _buffer + _capacity);        // Tried to release memory from past allocated region
        assert(ptr < _buffer + _allocated_bytes); // Tried to release unallocated memory inside the allocated region

        const auto size_at_ptr = reinterpret_cast<byte*>(ptr) - _buffer;
        _allocated_bytes = size_at_ptr;
    }

    auto stack_allocator::get_marker() const noexcept -> size_t
    {
        return _allocated_bytes;
    }

    void stack_allocator::free_marker(size_t marker)
    {
        const auto diff = marker - _allocated_bytes;
        if (diff > 0)
        {
            _allocated_bytes = marker;
        }
    }

    void stack_allocator::release()
    {
        if (_buffer != nullptr)
        {
            ::free(_buffer);
            _buffer = nullptr;
            _capacity = 0;
            _allocated_bytes = 0;
        }
    }

    void stack_allocator::reset()
    {
        _allocated_bytes = 0;
    }

    heap_allocator::heap_allocator(size_t bytes)
        : _memory{reinterpret_cast<byte*>(::malloc(bytes))}, _max_size{bytes}
    {
        _tlsf_handle = tlsf_create_with_pool(_memory, _max_size);
    }

    heap_allocator::heap_allocator(heap_allocator&& other) noexcept
        : _tlsf_handle{other._tlsf_handle}, _memory{other._memory}, _allocated_size{other._allocated_size},
          _max_size{other._max_size}
    {
        other._tlsf_handle = nullptr;
        other._memory = nullptr;
        other._allocated_size = 0;
        other._max_size = 0;
    }

    heap_allocator::~heap_allocator()
    {
        _release();
    }

    auto heap_allocator::operator=(heap_allocator&& rhs) noexcept -> heap_allocator&
    {
        if (&rhs == this) [[unlikely]]
        {
            return *this;
        }

        _release();

        tempest::swap(_tlsf_handle, rhs._tlsf_handle);
        tempest::swap(_memory, rhs._memory);
        tempest::swap(_allocated_size, rhs._allocated_size);
        tempest::swap(_max_size, rhs._max_size);

        return *this;
    }

    auto heap_allocator::allocate(size_t size, [[maybe_unused]] size_t alignment, [[maybe_unused]] source_location loc)
        -> void*
    {
        return tlsf_malloc(_tlsf_handle, size);
    }

    void heap_allocator::deallocate(void* ptr)
    {
        tlsf_free(_tlsf_handle, ptr);
    }

    void heap_allocator::_release()
    {
        if (_memory != nullptr)
        {
            tlsf_destroy(_tlsf_handle);
            ::free(_memory);

            _tlsf_handle = nullptr;
            _memory = nullptr;
        }
    }

    auto system_allocator::allocate(size_t size, size_t alignment, [[maybe_unused]] source_location loc) -> void*
    {
        if (size == 0)
        {
            return nullptr;
        }
        alignment = max(alignment, sizeof(void*));
        return aligned_alloc(size, alignment);
    }

    void system_allocator::deallocate(void* ptr)
    {
        if (ptr != nullptr)
        {
            aligned_free(ptr);
        }
    }

    auto aligned_alloc(size_t n, size_t alignment) -> void*
    {
#ifdef _MSC_VER
        return _aligned_malloc(n, alignment);
#else
        return ::aligned_alloc(alignment, n);
#endif
    }

    void aligned_free(void* ptr)
    {
#ifdef _MSC_VER
        _aligned_free(ptr);
#else
        ::free(ptr);
#endif
    }
} // namespace tempest