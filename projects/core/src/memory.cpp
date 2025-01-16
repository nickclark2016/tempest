#include <tempest/memory.hpp>

#include <tlsf/tlsf.h>

#include <cassert>
#include <cstdlib>
#include <utility>

namespace tempest
{
    namespace
    {
        size_t align_memory(size_t sz, size_t align)
        {
            const auto _mask = align - 1;
            return (sz + _mask) & ~_mask;
        }
    } // namespace

    stack_allocator::stack_allocator(size_t bytes)
        : _buffer{reinterpret_cast<byte*>(std::malloc(bytes))}, _capacity{bytes}
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

    stack_allocator& stack_allocator::operator=(stack_allocator&& rhs) noexcept
    {
        if (&rhs == this) [[unlikely]]
        {
            return *this;
        }

        release();

        std::swap(_buffer, rhs._buffer);
        std::swap(_capacity, rhs._capacity);
        std::swap(_allocated_bytes, rhs._allocated_bytes);

        return *this;
    }

    // TODO: Investigate bump down allocation instead of bump up
    void* stack_allocator::allocate(size_t size, size_t alignment, [[maybe_unused]] std::source_location loc)
    {
        assert(size > 0 && "Size must be non-zero.");
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

    size_t stack_allocator::get_marker() const noexcept
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
        if (_buffer)
        {
            std::free(_buffer);
            _buffer = nullptr;
            _capacity = 0;
            _allocated_bytes = 0;
        }
    }

    heap_allocator::heap_allocator(size_t bytes)
        : _memory{reinterpret_cast<byte*>(std::malloc(bytes))}, _allocated_size{0}, _max_size{bytes}
    {
        _tlsf_handle = tlsf_create_with_pool(_memory, _max_size);
    }

    heap_allocator::heap_allocator(heap_allocator&& other) noexcept
        : _tlsf_handle{std::move(other._tlsf_handle)}, _memory{std::move(other._memory)},
          _allocated_size{std::move(other._allocated_size)}, _max_size{std::move(other._max_size)}
    {
        _tlsf_handle = nullptr;
        _memory = nullptr;
    }

    heap_allocator::~heap_allocator()
    {
        _release();
    }

    heap_allocator& heap_allocator::operator=(heap_allocator&& rhs) noexcept
    {
        if (&rhs == this) [[unlikely]]
        {
            return *this;
        }

        _release();

        std::swap(_tlsf_handle, rhs._tlsf_handle);
        std::swap(_memory, rhs._memory);
        std::swap(_allocated_size, rhs._allocated_size);
        std::swap(_max_size, rhs._max_size);

        return *this;
    }

    void* heap_allocator::allocate(size_t size, [[maybe_unused]] size_t alignment,
                                   [[maybe_unused]] std::source_location loc)
    {
        return tlsf_malloc(_tlsf_handle, size);
    }

    void heap_allocator::deallocate(void* ptr)
    {
        return tlsf_free(_tlsf_handle, ptr);
    }

    void heap_allocator::_release()
    {
        if (_memory)
        {
            tlsf_destroy(_tlsf_handle);
            std::free(_memory);

            _tlsf_handle = nullptr;
            _memory = nullptr;
        }
    }

    void* aligned_alloc(size_t n, size_t alignment)
    {
#ifdef _MSC_VER
        return _aligned_malloc(n, alignment);
#else
        return std::aligned_alloc(alignment, n);
#endif
    }

    void aligned_free(void* ptr)
    {
#ifdef _MSC_VER
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }
} // namespace tempest