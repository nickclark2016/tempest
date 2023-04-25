#ifndef tempest_ecs_sparse_set_hpp
#define tempest_ecs_sparse_set_hpp

#include "keys.hpp"

#include <cstddef>
#include <memory>

namespace tempest::ecs
{
    template <sparse_key T, std::size_t SparsePageSize = 1024, typename Allocator = std::allocator<T>,
              typename PageAllocator = std::allocator<std::uint32_t>,
              typename PageArrayAllocator = std::allocator<std::uint32_t*>>
    class sparse_set
    {
      public:
        using size_type = std::size_t;
        using allocator_type = Allocator;
        using page_allocator_type = PageAllocator;
        using page_array_allocator_type = PageArrayAllocator;

        sparse_set() = default;
        sparse_set(const sparse_set& src);
        sparse_set(sparse_set&& src) noexcept;
        ~sparse_set();

        sparse_set& operator=(const sparse_set& rhs);
        sparse_set& operator=(sparse_set&& rhs) noexcept;

        [[nodiscard]] size_type size() const noexcept;
        [[nodiscard]] size_type capacity() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

      private:
        using sparse_page_type = std::uint32_t*;

        std::size_t _page_count{0};
        sparse_page_type* _sparse_pages{nullptr};

        T* _packed_begin{nullptr};
        T* _packed_end{nullptr};
        T* _packed_end_cap{nullptr};

        allocator_type _packed_alloc;
        page_allocator_type _page_alloc;
        page_array_allocator_type _page_array_alloc;

        void _release() noexcept;
    };

    template <sparse_key T, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::sparse_set(
        const sparse_set& src)
        : _page_count{src._page_count}, _packed_alloc{src._packed_alloc}, _page_alloc{src._page_alloc},
          _page_array_alloc{src._page_array_alloc}
    {
        using alloc_traits = std::allocator_traits<allocator_type>;
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

    template <sparse_key T, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::sparse_set(
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

    template <sparse_key T, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::~sparse_set()
    {
        _release();
    }

    template <sparse_key T, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>& sparse_set<
        T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::operator=(const sparse_set& rhs)
    {
        using alloc_traits = std::allocator_traits<allocator_type>;
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

    template <sparse_key T, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>& sparse_set<
        T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::operator=(sparse_set&& rhs) noexcept
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

    template <sparse_key T, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::size_type sparse_set<
        T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::size() const noexcept
    {
        return static_cast<size_type>(_packed_end - _packed_begin);
    }

    template <sparse_key T, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline sparse_set<T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::size_type sparse_set<
        T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::capacity() const noexcept
    {
        return static_cast<size_type>(_packed_end_cap - _packed_begin);
    }

    template <sparse_key T, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline bool sparse_set<T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::empty() const noexcept
    {
        return _packed_end == _packed_begin;
    }

    template <sparse_key T, std::size_t SparsePageSize, typename Allocator, typename PageAllocator,
              typename PageArrayAllocator>
    inline void sparse_set<T, SparsePageSize, Allocator, PageAllocator, PageArrayAllocator>::_release() noexcept
    {
        using alloc_traits = std::allocator_traits<allocator_type>;
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
} // namespace tempest::ecs

#endif // tempest_ecs_sparse_set_hpp