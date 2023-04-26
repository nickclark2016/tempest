#ifndef tempest_ecs_sparse_set_hpp
#define tempest_ecs_sparse_set_hpp

#include "keys.hpp"

#include <tempest/algorithm.hpp>

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace tempest::ecs
{
    template <sparse_key K, std::size_t SparsePageSize = 1024, typename Allocator = std::allocator<K>,
              typename PageAllocator = std::allocator<std::uint32_t>,
              typename PageArrayAllocator = std::allocator<std::uint32_t*>>
    class sparse_set
    {
      public:
        using size_type = std::size_t;
        using dense_allocator_type = Allocator;
        using page_allocator_type = PageAllocator;
        using page_array_allocator_type = PageArrayAllocator;

        using pointer = K*;
        using const_pointer = const K*;
        using reference = K&;
        using const_reference = const K&;
        using iterator = K*;
        using const_iterator = const K*;

        static constexpr std::uint32_t tombstone = std::numeric_limits<std::uint32_t>::max();

        sparse_set() = default;
        sparse_set(const sparse_set& src);
        sparse_set(sparse_set&& src) noexcept;
        ~sparse_set();

        sparse_set& operator=(const sparse_set& rhs);
        sparse_set& operator=(sparse_set&& rhs) noexcept;

        [[nodiscard]] size_type size() const noexcept;
        [[nodiscard]] size_type capacity() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

        [[nodiscard]] bool contains(const K& value) const noexcept;

        bool insert(const K& value);
        bool insert(K&& value);
        bool remove(const K& value);
        void clear();

        [[nodiscard]] iterator begin() noexcept;
        [[nodiscard]] const_iterator begin() const noexcept;
        [[nodiscard]] const_iterator cbegin() const noexcept;
        [[nodiscard]] iterator end() noexcept;
        [[nodiscard]] const_iterator end() const noexcept;
        [[nodiscard]] const_iterator cend() const noexcept;

      private:
        using sparse_page_type = std::uint32_t*;

        std::size_t _page_count{0};
        sparse_page_type* _sparse_pages{nullptr};

        K* _packed_begin{nullptr};
        K* _packed_end{nullptr};
        K* _packed_end_cap{nullptr};

        dense_allocator_type _packed_alloc;
        page_allocator_type _page_alloc;
        page_array_allocator_type _page_array_alloc;

        void _release() noexcept;
        std::size_t _compute_page(std::uint32_t id) const noexcept;
        std::size_t _compute_offset(std::uint32_t id) const noexcept;
        sparse_page_type _new_page();
        void _allocate_pages(std::size_t element_count);
        void _allocate_dense(std::size_t element_count);
        void _allocate(std::size_t element_count);
        std::size_t _alloc_size_strategy(std::size_t element_count) const noexcept;
    };

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::sparse_set(
        const sparse_set& src)
        : _page_count{src._page_count}, _packed_alloc{src._packed_alloc}, _page_alloc{src._page_alloc},
          _page_array_alloc{src._page_array_alloc}
    {
        using alloc_traits = std::allocator_traits<dense_allocator_type>;
        using page_alloc_traits = std::allocator_traits<page_allocator_type>;
        using page_array_alloc_traits = std::allocator_traits<page_array_allocator_type>;

        if (_page_count == 0)
        {
            return;
        }

        _sparse_pages = page_array_alloc_traits::allocate(_page_array_alloc, _page_count);
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            _sparse_pages[i] = page_alloc_traits::allocate(_page_alloc, SparsePageSize);
            std::copy_n(src._sparse_pages[i], SparsePageSize, _sparse_pages[i]);
        }

        // make a new packed array
        _packed_begin = alloc_traits::allocate(_packed_alloc, src.capacity());
        _packed_end = _packed_begin + src.size();
        _packed_end_cap = _packed_begin + src.capacity();
        std::copy_n(src._packed_begin, src.size(), _packed_begin);
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::sparse_set(
        sparse_set&& src) noexcept
        : _page_count{src._page_count}, _sparse_pages{src._sparse_pages}, _packed_begin{src._packed_begin},
          _packed_end{src._packed_end}, _packed_end_cap{src._packed_end_cap}, _packed_alloc{std::move(
                                                                                  src._packed_alloc)},
          _page_alloc{std::move(src._page_alloc)}, _page_array_alloc{std::move(src._page_array_alloc)}
    {
        src._page_count = 0;
        src._sparse_pages = nullptr;
        src._packed_begin = src._packed_end = src._packed_end_cap = nullptr;
        src._packed_alloc = {};
        src._page_alloc = {};
        src._page_array_alloc = {};
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::~sparse_set()
    {
        _release();
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>& sparse_set<
        K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::operator=(const sparse_set& rhs)
    {
        using alloc_traits = std::allocator_traits<dense_allocator_type>;
        using page_alloc_traits = std::allocator_traits<page_allocator_type>;
        using page_array_alloc_traits = std::allocator_traits<page_array_allocator_type>;

        if (&rhs == this)
        {
            return *this;
        }

        _release();

        // copy shallow
        _page_count = rhs._page_count;
        _packed_alloc = rhs._packed_alloc;
        _page_alloc = rhs._page_alloc;
        _page_array_alloc = rhs._page_array_alloc;

        if (_page_count == 0)
        {
            return *this;
        }

        // make a new page array
        _sparse_pages = page_array_alloc_traits::allocate(_page_array_alloc, _page_count);
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            _sparse_pages[i] = page_alloc_traits::allocate(_page_alloc, SparsePageSize);
            std::copy_n(rhs._sparse_pages[i], SparsePageSize, _sparse_pages[i]);
        }

        // make a new packed array
        _packed_begin = alloc_traits::allocate(_packed_alloc, rhs.capacity());
        _packed_end = _packed_begin + rhs.size();
        _packed_end_cap = _packed_begin + rhs.capacity();
        std::copy_n(rhs._packed_begin, rhs.size(), _packed_begin);

        return *this;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>& sparse_set<
        K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::operator=(sparse_set&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release();

        std::swap(_page_count, rhs._page_count);
        std::swap(_sparse_pages, rhs._sparse_pages);
        std::swap(_packed_begin, rhs._packed_begin);
        std::swap(_packed_end, rhs._packed_end);
        std::swap(_packed_end_cap, rhs._packed_end_cap);
        std::swap(_packed_alloc, rhs._packed_alloc);
        std::swap(_page_alloc, rhs._page_alloc);
        std::swap(_page_array_alloc, rhs._page_array_alloc);

        return *this;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::size_type sparse_set<
        K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::size() const noexcept
    {
        return static_cast<size_type>(_packed_end - _packed_begin);
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::size_type sparse_set<
        K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::capacity() const noexcept
    {
        return static_cast<size_type>(_packed_end_cap - _packed_begin);
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline bool sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::empty() const noexcept
    {
        return _packed_end == _packed_begin;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline bool sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::contains(
        const K& value) const noexcept
    {
        const std::uint32_t id = value.id;
        const std::size_t page = _compute_page(id);
        const std::size_t offset = _compute_offset(id);

        if (page >= _page_count)
        {
            return false;
        }

        const auto packed_idx = _sparse_pages[page][offset];
        return packed_idx < size() && _packed_begin[packed_idx] == value;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline bool sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::insert(const K& value)
    {
        using alloc_traits = std::allocator_traits<dense_allocator_type>;

        const std::uint32_t id = value.id;
        const std::size_t page = _compute_page(id);
        const std::size_t offset = _compute_offset(id);

        if (size() >= capacity())
        {
            // ensure the requested size is at least one larger than the current capacity
            std::size_t requested = _alloc_size_strategy(capacity() + 1);
            assert(requested > capacity());
            _allocate(requested);
        }

        sparse_page_type sparse_page = _sparse_pages[page];
        if (sparse_page[offset] != tombstone)
        {
            return false;
        }

        sparse_page[offset] = static_cast<std::uint32_t>(size());
        alloc_traits::construct(_packed_alloc, _packed_begin + size(), value);
        ++_packed_end;

        return true;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline bool sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::insert(K&& value)
    {
        using alloc_traits = std::allocator_traits<dense_allocator_type>;

        const std::uint32_t id = value.id;
        const std::size_t page = _compute_page(id);
        const std::size_t offset = _compute_offset(id);

        if (size() >= capacity())
        {
            // ensure the requested size is at least one larger than the current capacity
            std::size_t requested = _alloc_size_strategy(capacity() + 1);
            assert(requested > capacity());
            _allocate(requested);
        }

        sparse_page_type sparse_page = _sparse_pages[page];
        if (sparse_page[offset] != tombstone)
        {
            return false;
        }

        sparse_page[offset] = static_cast<std::uint32_t>(size());
        alloc_traits::construct(_packed_alloc, _packed_begin + size(), std::move(value));
        ++_packed_end;

        return true;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline bool sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::remove(const K& value)
    {
        using alloc_traits = std::allocator_traits<dense_allocator_type>;

        const std::uint32_t id = value.id;
        const std::size_t page = _compute_page(id);
        const std::size_t offset = _compute_offset(id);

        if (page < _page_count)
        {
            const std::uint32_t trampoline = _sparse_pages[page][offset];
            if (trampoline != tombstone && _packed_begin[trampoline] == value)
            {
                // remove from sparse array
                _sparse_pages[page][offset] = tombstone;

                K back = std::move(_packed_begin[size() - 1]); // pop from back

                // if the type cannot be trivially destroyed, invoke the destructor
                if constexpr (!std::is_trivially_destructible_v<K>)
                {
                    alloc_traits::destroy(_packed_alloc, _packed_begin + size() - 1);
                }
                
                _packed_begin[trampoline] = std::move(back);

                --_packed_end;

                // If we didn't pop the last element (set still has elements), then update the sparse page of the moved
                // element to point to its new location
                if (!empty())
                {
                    const std::size_t last_page = _compute_page(back.id);
                    const std::size_t last_offset = _compute_offset(back.id);
                    _sparse_pages[last_page][last_offset] = trampoline;
                }

                return true;
            }
        }

        return false;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline void sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::clear()
    {
        using alloc_traits = std::allocator_traits<dense_allocator_type>;

        for (std::size_t i = 0; i < _page_count; ++i)
        {
            auto page = _sparse_pages[i];
            std::fill_n(page, SparsePageSize, tombstone);
        }

        for (std::size_t i = 0; i < size(); ++i)
        {
            alloc_traits::destroy(_packed_alloc, _packed_begin + i);
        }

        _packed_end = _packed_begin;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::iterator sparse_set<
        K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::begin() noexcept
    {
        return _packed_begin;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::const_iterator sparse_set<
        K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::begin()
        const noexcept
    {
        return _packed_begin;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::const_iterator sparse_set<
        K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::cbegin() const noexcept
    {
        return _packed_begin;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::iterator sparse_set<
        K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::end() noexcept
    {
        return _packed_end;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::const_iterator sparse_set<
        K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::end()
        const noexcept
    {
        return _packed_end;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::const_iterator sparse_set<
        K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::cend() const noexcept
    {
        return _packed_end;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline void sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::_release() noexcept
    {
        using alloc_traits = std::allocator_traits<dense_allocator_type>;
        using page_alloc_traits = std::allocator_traits<page_allocator_type>;
        using page_array_alloc_traits = std::allocator_traits<page_array_allocator_type>;

        if (!empty())
        {
            for (std::size_t i = 0; i < _page_count; ++i)
            {
                sparse_page_type page = _sparse_pages[i];
                std::destroy_n(page, SparsePageSize);
                page_alloc_traits::deallocate(_page_alloc, page, SparsePageSize);
            }

            page_array_alloc_traits::deallocate(_page_array_alloc, _sparse_pages, _page_count);

            _page_count = 0;

            const auto sz = size();
            std::destroy_n(_packed_begin, sz);
            alloc_traits::deallocate(_packed_alloc, _packed_begin, sz);
        }

        _page_count = 0;
        _sparse_pages = nullptr;
        _packed_begin = _packed_end = _packed_end_cap = nullptr;
        _packed_alloc = {};
        _page_alloc = {};
        _page_array_alloc = {};
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline std::size_t sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::_compute_page(
        std::uint32_t id) const noexcept
    {
        return id / SparsePageSize;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline std::size_t sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::_compute_offset(
        std::uint32_t id) const noexcept
    {
        return id % SparsePageSize;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::sparse_page_type sparse_set<
        K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::_new_page()
    {
        using page_alloc_traits = std::allocator_traits<page_allocator_type>;
        sparse_page_type page = page_alloc_traits::allocate(_page_alloc, SparsePageSize);
        std::fill_n(page, SparsePageSize, tombstone);
        return page;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline void sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::_allocate_pages(
        std::size_t element_count)
    {
        using page_alloc_traits = std::allocator_traits<page_allocator_type>;
        using page_array_alloc_traits = std::allocator_traits<page_array_allocator_type>;

        const auto page_count = (element_count + SparsePageSize - 1) / SparsePageSize;

        if (page_count <= _page_count)
        {
            return;
        }

        sparse_page_type* pages = page_array_alloc_traits::allocate(_page_array_alloc, page_count);
        std::copy_n(_sparse_pages, _page_count, pages);

        for (std::size_t i = _page_count; i < page_count; ++i)
        {
            pages[i] = _new_page();
        }

        page_array_alloc_traits::deallocate(_page_array_alloc, _sparse_pages, _page_count);

        _sparse_pages = pages;
        _page_count = page_count;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline void sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::_allocate_dense(
        std::size_t element_count)
    {
        using alloc_traits = std::allocator_traits<dense_allocator_type>;

        if (element_count <= capacity())
        {
            return;
        }

        K* dense = alloc_traits::allocate(_packed_alloc, element_count);
        K* dense_end = core::copy_construct(_packed_begin, _packed_end, dense);
        alloc_traits::deallocate(_packed_alloc, _packed_begin, capacity());

        _packed_begin = dense;
        _packed_end = dense_end;
        _packed_end_cap = dense + element_count;
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline void sparse_set<K, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::_allocate(
        std::size_t element_count)
    {
        _allocate_pages(element_count);
        _allocate_dense(element_count);
    }

    template <sparse_key K, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline std::size_t sparse_set<K, SparsePageSize, Allocator, PageAllocator,
                                  PageArrayAllocator>::_alloc_size_strategy(std::size_t element_count) const noexcept
    {
        if (element_count <= 8)
        {
            return 8; // minimum size of allocation
        }

        return std::bit_ceil(element_count);
    }
} // namespace tempest::ecs

#endif // tempest_ecs_sparse_set_hpp