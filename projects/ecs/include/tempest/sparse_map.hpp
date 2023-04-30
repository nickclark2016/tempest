#ifndef tempest_ecs_sparse_map_hpp
#define tempest_ecs_sparse_map_hpp

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
    template <sparse_key K, typename V, std::size_t SparsePageSize = 1024,
              typename DenseKeyAllocator = std::allocator<K>, typename DenseValueAllocator = std::allocator<V>,
              typename PageAllocator = std::allocator<std::uint32_t>,
              typename PageArrayAllocator = std::allocator<std::uint32_t*>>
    class sparse_map
    {
      public:
        using key_allocator_type = DenseKeyAllocator;
        using value_allocator_type = DenseValueAllocator;
        using page_allocator_type = PageAllocator;
        using page_array_allocator_type = PageArrayAllocator;

        using size_type = std::size_t;
        using key_type = K;
        using value_type = V;
        using pointer = V*;
        using const_pointer = const V*;
        using reference = V&;
        using const_reference = const V&;
        using iterator = V*;
        using const_iterator = const V*;

        static constexpr std::uint32_t tombstone = std::numeric_limits<std::uint32_t>::max();

        sparse_map() = default;
        sparse_map(const sparse_map& src);
        sparse_map(sparse_map&& src) noexcept;
        ~sparse_map();

        sparse_map& operator=(const sparse_map& rhs);
        sparse_map& operator=(sparse_map&& rhs) noexcept;

        [[nodiscard]] size_type size() const noexcept;
        [[nodiscard]] size_type capacity() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

        [[nodiscard]] bool contains(const K& key) const noexcept;
        [[nodiscard]] bool contains(const K& key, const V& value) const noexcept;
        [[nodiscard]] pointer* find(const K& key) noexcept;
        [[nodiscard]] const_pointer* find(const K& key) const noexcept;

        bool insert(const K& key, const V& value);
        bool insert(K&& key, V&& value);
        bool remove(const K& key);
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

        std::size_t _capacity{0};
        key_type* _keys_begin{nullptr};
        key_type* _keys_end{nullptr};
        value_type* _values_begin{nullptr};
        value_type* _values_end{nullptr};

        key_allocator_type _packed_key_alloc;
        value_allocator_type _packed_value_alloc;
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

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::sparse_map(const sparse_map& src)
        : _page_count{src._page_count}, _capacity{src._capacity}, _packed_key_alloc{src._packed_key_alloc},
          _packed_value_alloc{src._packed_value_alloc}, _page_alloc{src._page_alloc}, _page_array_alloc{
                                                                                          src._page_array_alloc}
    {
        using key_alloc_trats = std::allocator_traits<key_allocator_type>;
        using value_alloc_traits = std::allocator_traits<value_allocator_type>;
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
            core::copy_construct(src._sparse_pages[n], src._sparse_pages[n] + SparsePageSize, _sparse_pages[i]);
        }

        _keys_begin = key_alloc_trats::allocate(_packed_key_alloc, src.capacity());
        _keys_end = _keys_begin + src.size();
        _values_begin = value_alloc_traits::allocate(_packed_value_alloc, src.capacity());
        _values_end = _values_begin + src.size();

        core::copy_construct(src._keys_begin, src._keys_end, _keys_begin);
        core::copy_construct(src._values_begin, src._values_end, _values_begin);
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::sparse_map(sparse_map&& src) noexcept
        : _page_count{src._page_count}, _sparse_pages{src._sparse_pages},
          _keys_begin{src._keys_begin}, _keys_end{src._keys_end}, _values_begin{src._values_begin},
          _values_end{src._values_end}, _capacity{src._capacity}, _packed_key_alloc{std::move(src._packed_key_alloc)},
          _packed_value_alloc{std::move(src._packed_value_alloc)}, _page_alloc{std::move(src._page_alloc)},
          _page_array_alloc{std::move(src._page_array_alloc)}
    {
        src._capacity = 0;
        src._page_count = 0;
        src._sparse_pages = nullptr;
        src._keys_begin = src._keys_end = nullptr;
        src._values_begin = src._values_end = nullptr;
        src._packed_key_alloc = {};
        src._packed_value_alloc = {};
        src._page_alloc = {};
        src._page_array_alloc = {};
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::~sparse_map()
    {
        _release();
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator, PageArrayAllocator>&
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
               PageArrayAllocator>::operator=(const sparse_map& rhs)
    {
        using key_alloc_trats = std::allocator_traits<key_allocator_type>;
        using value_alloc_traits = std::allocator_traits<value_allocator_type>;
        using page_alloc_traits = std::allocator_traits<page_allocator_type>;
        using page_array_alloc_traits = std::allocator_traits<page_array_allocator_type>;

        if (&rhs == this)
        {
            return *this;
        }

        _release();

        // shallow copy data
        _page_count = rhs._page_count;
        _packed_key_alloc = rhs._packed_key_alloc;
        _packed_value_alloc = rhs._packed_value_alloc;
        _page_alloc = rhs._page_alloc;
        _page_array_alloc = rhs._page_array_alloc;

        if (_page_count == 0)
        {
            return this;
        }

        // TODO: Optimize case where the existing map is large enough to fit the map copied from

        _sparse_pages = page_array_alloc_traits::allocate(_page_array_alloc, _page_count);
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            _sparse_pages[i] = page_alloc_traits::allocate(_page_alloc, SparsePageSize);
            std::copy_n(rhs._sparse_pages[i], SparsePageSize, _sparse_pages[i]);
        }

        _keys_begin = key_alloc_trats::allocate(_packed_key_alloc, rhs.capacity());
        _keys_end = _keys_begin + rhs.size();
        _values_begin = value_alloc_traits::allocate(_packed_value_alloc, rhs.capacity());
        _values_end = _values_begin + rhs.size();

        core::copy_construct(rhs._keys_begin, rhs._keys_end, _keys_begin);
        core::copy_construct(rhs._values_begin, rhs._values_end, _values_begin);

        return *this;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator, PageArrayAllocator>&
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
               PageArrayAllocator>::operator=(sparse_map&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release();

        std::swap(_page_count, rhs._page_count);
        std::swap(_sparse_pages, rhs._sparse_pages);
        std::swap(_keys_begin, rhs._keys_begin);
        std::swap(_keys_end, rhs._keys_end);
        std::swap(_values_begin, rhs._values_begin);
        std::swap(_values_end, rhs._values_end);
        std::swap(_capacity, rhs._capacity);
        std::swap(_packed_key_alloc, rhs._packed_key_alloc);
        std::swap(_packed_value_alloc, rhs._packed_value_alloc);
        std::swap(_page_alloc, rhs._page_alloc);
        std::swap(_page_array_alloc, rhs._page_array_alloc);

        return *this;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::size_type
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator, PageArrayAllocator>::size()
        const noexcept
    {
        return static_cast<size_type>(_keys_end - _keys_begin);
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::size_type
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
               PageArrayAllocator>::capacity() const noexcept
    {
        return _capacity;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline bool sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                           PageArrayAllocator>::empty() const noexcept
    {
        return size() == 0;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline bool sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                           PageArrayAllocator>::contains(const K& key) const noexcept
    {
        const std::uint32_t id = value.id;
        const std::size_t page = _compute_page(id);
        const std::size_t offset = _compute_offset(id);

        if (page >= _page_count)
        {
            return false;
        }

        const auto packed_idx = _sparse_pages[page][offset];
        return _keys_begin[packed_idx] == key;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline bool sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                           PageArrayAllocator>::contains(const K& key, const V& value) const noexcept
    {
        const std::uint32_t id = key.id;
        const std::size_t page = _compute_page(id);
        const std::size_t offset = _compute_offset(id);

        if (page >= _page_count)
        {
            return false;
        }

        const auto packed_idx = _sparse_pages[page][offset];
        return _keys_begin[packed_idx] == key && _values_begin[packed_idx] == value;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::pointer*
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator, PageArrayAllocator>::find(
        const K& key) noexcept
    {
        const std::uint32_t id = key.id;
        const std::size_t page = _compute_page(id);
        const std::size_t offset = _compute_offset(id);

        if (page >= _page_count)
        {
            return nullptr;
        }

        const auto packed_idx = _sparse_pages[page][offset];

        return _keys_begin[packed_idx] == key ? _values_begin + packed_idx : nullptr;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::const_pointer*
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator, PageArrayAllocator>::find(
        const K& key) const noexcept
    {
        const std::uint32_t id = key.id;
        const std::size_t page = _compute_page(id);
        const std::size_t offset = _compute_offset(id);

        if (page >= _page_count)
        {
            return nullptr;
        }

        const auto packed_idx = _sparse_pages[page][offset];
        return _keys_begin[packed_idx] == key ? _values_begin + packed_idx : nullptr;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline bool sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                           PageArrayAllocator>::insert(const K& key, const V& value)
    {
        using key_alloc_traits = std::allocator_traits<key_allocator_type>;
        using value_alloc_traits = std::allocator_traits<value_allocator_type>;

        const std::uint32_t id = key.id;
        const std::size_t page = _compute_page(id);
        const std::size_t offset = _compute_offset(id);

        if (size() >= capacity())
        {
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
        key_alloc_traits::construct(_packed_key_alloc, _keys_begin + size(), key);
        value_alloc_traits::construct(_packed_value_alloc, _values_begin + size(), value);
        ++_keys_end;
        ++_values_end;

        return true;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline bool sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                           PageArrayAllocator>::insert(K&& key, V&& value)
    {
        using key_alloc_traits = std::allocator_traits<key_allocator_type>;
        using value_alloc_traits = std::allocator_traits<value_allocator_type>;

        const std::uint32_t id = key.id;
        const std::size_t page = _compute_page(id);
        const std::size_t offset = _compute_offset(id);

        if (size() >= capacity())
        {
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
        key_alloc_traits::construct(_packed_key_alloc, _keys_begin + size(), std::move(key));
        value_alloc_traits::construct(_packed_value_alloc, _values_begin + size(), std::move(value));
        ++_keys_end;
        ++_values_end;

        return true;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline bool sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                           PageArrayAllocator>::remove(const K& key)
    {
        using key_alloc_traits = std::allocator_traits<key_allocator_type>;
        using value_alloc_traits = std::allocator_traits<value_allocator_type>;

        const std::uint32_t id = key.id;
        const std::size_t page = _compute_page(id);
        const std::size_t offset = _compute_offset(id);

        if (page < _page_count)
        {
            const auto trampoline = _sparse_pages[page][offset];
            if (trampoline != tombstone && _keys_begin[trampoline] == key)
            {
                _sparse_pages[page][offset] = tombstone;
                K key_back = std::move(_keys_begin[size() - 1]);
                V value_back = std::move(_values_begin[size() - 1]);

                key_alloc_traits::destroy(_packed_key_alloc, _keys_begin + size() - 1);
                value_alloc_traits::destroy(_packed_value_alloc, _values_begin + size() - 1);

                _keys_begin[trampoline] = std::move(key_back);
                _values_begin[trampoline] = std::move(value_back);

                --_keys_end;
                --_values_end;

                // If we didn't pop the last element (set still has elements), then update the sparse page of the moved
                // element to point to its new location
                if (!empty())
                {
                    const auto last_page = _compute_page(_keys_begin[trampoline].id);
                    const auto last_offset = _compute_offset(_keys_begin[trampoline].id);
                    _sparse_pages[last_page][last_offset] = trampoline;
                }

                return true;
            }
        }

        return false;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline void sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                           PageArrayAllocator>::clear()
    {
        using key_alloc_traits = std::allocator_traits<key_allocator_type>;
        using value_alloc_traits = std::allocator_traits<value_allocator_type>;

        for (std::size_t i = 0; i < _page_count; ++i)
        {
            auto page = _sparse_pages[i];
            std::fill_n(page, SparsePageSize, tombstone);
        }

        for (std::size_t i = 0; i < size(); ++i)
        {
            key_alloc_traits::destroy(_packed_key_alloc, _keys_begin + i);
        }

        for (std::size_t i = 0; i < size(); ++i)
        {
            value_alloc_traits::destroy(_packed_value_alloc, _values_begin + i);
        }
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::iterator
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
               PageArrayAllocator>::begin() noexcept
    {
        return _values_begin;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::const_iterator
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
               PageArrayAllocator>::begin() const noexcept
    {
        return _values_begin;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::const_iterator
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator, PageArrayAllocator>::cbegin()
        const noexcept
    {
        return _values_begin;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::iterator
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
               PageArrayAllocator>::end() noexcept
    {
        return _values_end;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::const_iterator
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
               PageArrayAllocator>::end() const noexcept
    {
        return _values_end;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::const_iterator
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator, PageArrayAllocator>::cend()
        const noexcept
    {
        return _values_end;
    }
       

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline void sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                           PageArrayAllocator>::_release() noexcept
    {
        using key_alloc_traits = std::allocator_traits<key_allocator_type>;
        using value_alloc_traits = std::allocator_traits<value_allocator_type>;
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
            std::destroy_n(_keys_begin, sz);
            std::destroy_n(_values_begin, sz);
            key_alloc_traits::deallocate(_packed_key_alloc, _keys_begin, sz);
            value_alloc_traits::deallocate(_packed_value_alloc, _values_begin, sz);
        }

        _page_count = 0;
        _sparse_pages = nullptr;
        _keys_begin = _keys_end = nullptr;
        _values_begin = _values_end = nullptr;
        _capacity = 0;
        _packed_key_alloc = {};
        _packed_value_alloc = {};
        _page_alloc = {};
        _page_array_alloc = {};
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline std::size_t sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                                  PageArrayAllocator>::_compute_page(std::uint32_t id) const noexcept
    {
        return id / SparsePageSize;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline std::size_t sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                                  PageArrayAllocator>::_compute_offset(std::uint32_t id) const noexcept
    {
        return id % SparsePageSize;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                      PageArrayAllocator>::sparse_page_type
    sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
               PageArrayAllocator>::_new_page()
    {
        using page_alloc_traits = std::allocator_traits<page_allocator_type>;
        sparse_page_type page = page_alloc_traits::allocate(_page_alloc, SparsePageSize);
        std::fill_n(page, SparsePageSize, tombstone);
        return page;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline void sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                           PageArrayAllocator>::_allocate_pages(std::size_t element_count)
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

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline void sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                           PageArrayAllocator>::_allocate_dense(std::size_t element_count)
    {
        using key_alloc_traits = std::allocator_traits<key_allocator_type>;
        using value_alloc_traits = std::allocator_traits<value_allocator_type>;

        if (element_count <= capacity())
        {
            return;
        }

        K* keys = key_alloc_traits::allocate(_packed_key_alloc, element_count);
        K* values = value_alloc_traits::allocate(_packed_value_alloc, element_count);

        V* keys_end = core::optimal_construct(_keys_begin, _keys_end, keys);
        V* values_end = core::optimal_construct(_values_begin, _values_end, values);

        for (std::size_t i = 0; i < size(); ++i)
        {
            key_alloc_traits::destroy(_packed_key_alloc, _keys_begin + i);
        }

        for (std::size_t i = 0; i < size(); ++i)
        {
            value_alloc_traits::destroy(_packed_value_alloc, _values_begin + i);
        }

        key_alloc_traits::deallocate(_packed_key_alloc, _keys_begin, capacity());
        value_alloc_traits::deallocate(_packed_value_alloc, _values_begin, capacity());

        _keys_begin = keys;
        _keys_end = keys_end;
        _values_begin = values;
        _values_end = values_end;
        _capacity = element_count;
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline void sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                           PageArrayAllocator>::_allocate(std::size_t element_count)
    {
        _allocate_pages(element_count);
        _allocate_dense(element_count);
    }

    template <sparse_key K, typename V, std::size_t SparsePageSize, typename DenseKeyAllocator,
              typename DenseValueAllocator, typename PageAllocator, typename PageArrayAllocator>
    inline std::size_t sparse_map<K, V, SparsePageSize, DenseKeyAllocator, DenseValueAllocator, PageAllocator,
                                  PageArrayAllocator>::_alloc_size_strategy(std::size_t element_count) const noexcept
    {
        if (element_count <= 8)
        {
            return 8; // minimum size of allocation
        }

        return std::bit_ceil(element_count);
    }
} // namespace tempest::ecs

#endif // tempest_ecs_sparse_map_hpp