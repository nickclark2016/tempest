#ifndef tempest_core_flat_unordered_map_hpp
#define tempest_core_flat_unordered_map_hpp

#include "functional.hpp"
#include "hash.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>

namespace tempest
{
    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    class flat_unordered_map;

    namespace detail
    {
        using metadata_entry = std::uint8_t;

        constexpr metadata_entry empty_entry = 0b1111'1111;
        constexpr metadata_entry deleted_entry = 0b1000'0000;

        /// @brief Strategy for metadata entry operations.
        struct metadata_entry_strategy
        {
            /// @brief Fetches if the metadata entry represents an empty slot.
            /// @param e Entry to check.
            /// @return True if the entry is empty, false otherwise.
            bool is_empty(metadata_entry e) const noexcept;

            /// @brief Fetches if the metadata entry represents a full slot.
            /// @param e Entry to check.
            /// @return True if the entry is full, false otherwise.
            bool is_full(metadata_entry e) const noexcept;

            /// @brief Fetches if the metadata entry represents a deleted slot.
            /// @param e Entry to check.
            /// @return True if the entry is deleted, false otherwise.
            bool is_deleted(metadata_entry e) const noexcept;
        };

        struct metadata_group
        {
            static constexpr std::size_t group_size = 16;

            metadata_entry entries[group_size]{};

            bool any_empty() const noexcept;
            std::uint16_t match_byte(std::uint8_t h2) const noexcept;
            bool any_empty_or_deleted() const noexcept;
        };

        static_assert(sizeof(metadata_entry) == 1, "metadata_entry must be 1 byte");

        template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator, bool Const>
        struct flat_unordered_map_iterator
        {
            using value_type = std::pair<const K, V>;
            using pointer = std::conditional_t<Const, const value_type*, value_type*>;
            using reference = std::conditional_t<Const, const value_type&, value_type&>;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

            using map_type =
                typename std::conditional_t<Const, const flat_unordered_map<K, V, Hash, KeyEqual, Allocator>,
                                            flat_unordered_map<K, V, Hash, KeyEqual, Allocator>>;

            flat_unordered_map_iterator() noexcept = default;
            flat_unordered_map_iterator(std::size_t idx, map_type* map) noexcept;

            flat_unordered_map_iterator& operator++() noexcept;
            flat_unordered_map_iterator operator++(int) noexcept;
            reference operator*() const noexcept;
            pointer operator->() const noexcept;

            friend bool operator==(const flat_unordered_map_iterator& lhs,
                                   const flat_unordered_map_iterator& rhs) noexcept
            {
                return lhs._index == rhs._index;
            }

            friend auto operator<=>(const flat_unordered_map_iterator& lhs,
                                    const flat_unordered_map_iterator& rhs) noexcept
            {
                return lhs._index <=> rhs._index;
            }

            std::size_t index() const noexcept
            {
                return _index;
            }

          private:
            std::size_t _index{0};
            map_type* _map{nullptr};

            friend class flat_unordered_map<K, V, Hash, KeyEqual, Allocator>;
        };

        template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator, bool Const>
        bool operator==(const flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, Const>& lhs,
                        const flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, !Const>& rhs) noexcept
        {
            return lhs.index() == rhs.index();
        }

        template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator, bool Const>
        auto operator!=(const flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, Const>& lhs,
                        const flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, !Const>& rhs) noexcept
        {
            return lhs.index() != rhs.index();
        }

        template <typename Iter>
        struct flat_unordered_map_insert_result
        {
            Iter position;
            bool inserted;
        };
    } // namespace detail

    /// @brief Hash map with open addressing and linear probing. This map is based on the swiss table design.
    /// @tparam K Key type
    /// @tparam V Value type
    /// @tparam Hash Hash function. The hash function must match the signature of std::hash. For optimal performance
    ///             the hash function should distribute bits uniformly across the std::size_t range.
    /// @tparam KeyEqual Key equality function. The key equality function must match the signature of std::equal_to.
    /// @tparam Allocator Allocator type conforming to the C++17 Allocator concept.
    template <typename K, typename V, typename Hash = tempest::hash<K>,
              typename KeyEqual = tempest::equal_to<K>,
              typename Allocator = std::allocator<std::pair<const K, V>>>
    class flat_unordered_map
    {
      public:
        using value_type = std::pair<const K, V>;
        using key_type = K;
        using mapped_type = V;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using hasher = Hash;
        using key_equal = KeyEqual;
        using allocator_type = Allocator;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = typename std::allocator_traits<Allocator>::pointer;
        using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
        using iterator = detail::flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, false>;
        using const_iterator = detail::flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, true>;

        flat_unordered_map() noexcept = default;
        flat_unordered_map(const flat_unordered_map& other);
        flat_unordered_map(flat_unordered_map&& other) noexcept;
        ~flat_unordered_map();

        flat_unordered_map& operator=(const flat_unordered_map& other);
        flat_unordered_map& operator=(flat_unordered_map&& other) noexcept;

        std::size_t size() const noexcept;
        std::size_t capacity() const noexcept;
        std::size_t max_size() const noexcept;
        bool empty() const noexcept;
        double load_factor() const noexcept;

        iterator begin() noexcept;
        const_iterator begin() const noexcept;
        const_iterator cbegin() const noexcept;

        iterator end() noexcept;
        const_iterator end() const noexcept;
        const_iterator cend() const noexcept;

        iterator find(const K& key) noexcept;
        const_iterator find(const K& key) const noexcept;

        detail::flat_unordered_map_insert_result<iterator> insert(const value_type& value);
        detail::flat_unordered_map_insert_result<iterator> insert(value_type&& value);

        template <typename InputIt>
        void insert(InputIt first, InputIt last);

        void erase(iterator pos);
        void erase(const K& key);
        void clear() noexcept;

        V& operator[](const K& key);

      private:
        static constexpr std::size_t _page_size{detail::metadata_group::group_size};
        static constexpr double _default_load_factor{0.75};

        using metadata_page = detail::metadata_group;
        using data_page = std::array<std::pair<const K, V>, _page_size>;

        using alloc_type = typename std::allocator_traits<Allocator>::template rebind_alloc<data_page>;
        using alloc_traits = std::allocator_traits<alloc_type>;
        using metadata_alloc_type = typename alloc_traits::template rebind_alloc<metadata_page>;
        using metadata_alloc_traits = std::allocator_traits<metadata_alloc_type>;

        metadata_page* _metadata_pages{nullptr};
        data_page* _data_pages{nullptr};
        size_type _size{0};
        size_type _page_count{0};

        metadata_alloc_type _metadata_alloc;
        alloc_type _alloc;
        hasher _hash;
        detail::metadata_entry_strategy _metadata_strategy;

        void _request_grow(std::size_t new_size);
        metadata_page* _request_empty_metadata_pages(std::size_t count);
        data_page* _request_empty_data_pages(std::size_t count);
        std::size_t _compute_default_growth(std::size_t requested) const noexcept;
        std::size_t _first_occupied_index() const noexcept;

        std::uint64_t _get_h1(std::size_t hc) const noexcept;
        std::uint8_t _get_h2(std::size_t hc) const noexcept;

        std::uint16_t _get_hash_match(std::uint8_t h2, std::size_t page) const noexcept;
        bool _match_empty(std::size_t page) const noexcept;
        bool _match_empty_or_deleted(std::size_t page) const noexcept;
        std::pair<std::size_t, std::size_t> _find_next_empty(std::size_t h1, metadata_page* pages,
                                                             std::size_t page_count) const noexcept;
        std::size_t _next_occupied_index(std::size_t search_start) const noexcept;

        void _release();

        friend struct detail::flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, false>;
        friend struct detail::flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, true>;
    };

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::flat_unordered_map(const flat_unordered_map& other)
        : _metadata_alloc{metadata_alloc_traits::select_on_container_copy_construction(other._metadata_alloc)},
          _alloc{alloc_traits::select_on_container_copy_construction(other._alloc)}, _hash{other._hash},
          _metadata_strategy{other._metadata_strategy}
    {
        _page_count = other._page_count;
        _metadata_pages = _request_empty_metadata_pages(_page_count);
        _data_pages = _request_empty_data_pages(_page_count);

        // TODO: investigate if it's worth having separate loops for metadata and data pages
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            for (std::size_t j = 0; j < _page_size; ++j)
            {
                _metadata_pages[i].entries[j] = other._metadata_pages[i].entries[j];
                if (_metadata_strategy.is_full(other._metadata_pages[i].entries[j]))
                {
                    std::construct_at(&_data_pages[i][j], other._data_pages[i][j]);
                }
            }
        }

        _size = other._size;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::flat_unordered_map(flat_unordered_map&& other) noexcept
        : _metadata_alloc{std::move(other._metadata_alloc)}, _alloc{std::move(other._alloc)},
          _hash{std::move(other._hash)}, _metadata_strategy{std::move(other._metadata_strategy)}
    {
        _metadata_pages = other._metadata_pages;
        _data_pages = other._data_pages;
        _size = other._size;
        _page_count = other._page_count;

        other._metadata_pages = nullptr;
        other._data_pages = nullptr;
        other._size = 0;
        other._page_count = 0;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::~flat_unordered_map()
    {
        _release();
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline flat_unordered_map<K, V, Hash, KeyEqual, Allocator>& flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::operator=(const flat_unordered_map& other)
    {
        if (this == &other)
        {
            return *this;
        }

        _release();

        // copy the allocators
        _metadata_alloc = metadata_alloc_traits::select_on_container_copy_construction(other._metadata_alloc);
        _alloc = alloc_traits::select_on_container_copy_construction(other._alloc);

        _hash = other._hash;
        _metadata_strategy = other._metadata_strategy;

        _page_count = other._page_count;
        _metadata_pages = _request_empty_metadata_pages(_page_count);
        _data_pages = _request_empty_data_pages(_page_count);

        // Copy the metadata and value pages
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            for (std::size_t j = 0; j < _page_size; ++j)
            {
                _metadata_pages[i].entries[j] = other._metadata_pages[i].entries[j];
                if (_metadata_strategy.is_full(other._metadata_pages[i].entries[j]))
                {
                    std::construct_at(&_data_pages[i][j], other._data_pages[i][j]);
                }
            }
        }

        _size = other._size;

        return *this;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline flat_unordered_map<K, V, Hash, KeyEqual, Allocator>& flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::operator=(flat_unordered_map&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        _release();

        _metadata_alloc = std::move(other._metadata_alloc);
        _alloc = std::move(other._alloc);
        _hash = std::move(other._hash);
        _metadata_strategy = std::move(other._metadata_strategy);

        _metadata_pages = other._metadata_pages;
        _data_pages = other._data_pages;
        _size = other._size;
        _page_count = other._page_count;

        other._metadata_pages = nullptr;
        other._data_pages = nullptr;
        other._size = 0;
        other._page_count = 0;

        return *this;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline std::size_t flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::size() const noexcept
    {
        return _size;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline std::size_t flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::capacity() const noexcept
    {
        return _page_count * _page_size;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline std::size_t flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::max_size() const noexcept
    {
        return std::numeric_limits<size_type>::max();
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline bool flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::empty() const noexcept
    {
        return _size == 0;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline double flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::load_factor() const noexcept
    {
        return empty() ? 1.0 : static_cast<double>(size()) / static_cast<double>(capacity());
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline typename flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::iterator flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::begin() noexcept
    {
        return iterator{_first_occupied_index(), this};
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline typename flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::const_iterator flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::begin() const noexcept
    {
        return const_iterator{_first_occupied_index(), this};
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline typename flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::const_iterator flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::cbegin() const noexcept
    {
        return const_iterator{_first_occupied_index(), this};
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline typename flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::iterator flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::end() noexcept
    {
        return iterator{_page_count * _page_size, this};
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline typename flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::const_iterator flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::end() const noexcept
    {
        return const_iterator{_page_count * _page_size, this};
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline typename flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::const_iterator flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::cend() const noexcept
    {
        return const_iterator{_page_count * _page_size, this};
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline typename flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::iterator flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::find(const K& key) noexcept
    {
        // Add const to this
        const auto* self = this;

        const_iterator it = self->find(key);
        return iterator{it.index(), this};
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline typename flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::const_iterator flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::find(const K& key) const noexcept
    {
        auto hash = _hash(key);
        auto h1 = _get_h1(hash);

        // start probing
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            auto current_page = (h1 + i) % _page_count;
            auto matches = _get_hash_match(_get_h2(hash), current_page);

            for (std::size_t j = 0; j < _page_size; ++j)
            {
                if ((matches & (1 << j)) != 0)
                {
                    auto idx = current_page * _page_size + j;
                    if (key_equal{}(_data_pages[current_page][j].first, key))
                    {
                        return const_iterator{idx, this};
                    }
                }
            }

            // If any of entries in the metadata page are empty, that means that there
            // are no elements in the page beyond this, and thus the key was never inserted.
            if (_match_empty(current_page))
            {
                break;
            }

            assert(i < _page_count);
        }

        return cend();
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline detail::flat_unordered_map_insert_result<
        typename flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::iterator>
    flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::insert(const value_type& value)
    {
        // Check if we require growth
        if (load_factor() >= _default_load_factor)
        {
            _request_grow(_compute_default_growth(capacity() + 1));
        }

        auto hash = _hash(value.first);
        auto h1 = _get_h1(hash);
        auto h2 = _get_h2(hash);

        std::pair<std::uint32_t, std::uint32_t> next_empty{};

        // Find the slot in the map, check for the key already existing
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            auto current_page = (h1 + i) % _page_count;
            auto matches = _get_hash_match(h2, current_page);

            for (std::size_t j = 0; j < _page_size; ++j)
            {
                if ((matches & (1 << j)) != 0)
                {
                    auto idx = current_page * _page_size + j;
                    if (key_equal{}(_data_pages[current_page][j].first, value.first))
                    {
                        return {iterator{idx, this}, false};
                    }
                }
                else if (!_metadata_strategy.is_full(_metadata_pages[current_page].entries[j]))
                {
                    next_empty = {current_page, j};
                }
            }

            // If any of entries in the metadata page are empty, that means that there
            // are no elements in the page beyond this, and thus the key was never inserted.
            if (_match_empty(current_page))
            {
                break;
            }

            assert(i < _page_count);
        }

        _metadata_pages[next_empty.first].entries[next_empty.second] = h2;
        std::construct_at(&_data_pages[next_empty.first][next_empty.second], value);

        ++_size;

        return {iterator{next_empty.first * _page_size + next_empty.second, this}, true};
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline detail::flat_unordered_map_insert_result<
        typename flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::iterator>
    flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::insert(value_type&& value)
    {
        // Check if we require growth
        if (load_factor() >= _default_load_factor)
        {
            _request_grow(_compute_default_growth(capacity() + 1));
        }

        auto hash = _hash(value.first);
        auto h1 = _get_h1(hash);
        auto h2 = _get_h2(hash);

        std::pair<std::size_t, std::size_t> next_empty{};

        // Find the slot in the map, check for the key already existing
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            auto current_page = (h1 + i) % _page_count;
            auto matches = _get_hash_match(h2, current_page);

            for (std::size_t j = 0; j < _page_size; ++j)
            {
                if ((matches & (1 << j)) != 0)
                {
                    auto idx = current_page * _page_size + j;
                    if (key_equal{}(_data_pages[current_page][j].first, value.first))
                    {
                        return {iterator{idx, this}, false};
                    }
                }
                else if (!_metadata_strategy.is_full(_metadata_pages[current_page].entries[j]))
                {
                    next_empty = {current_page, j};
                }
            }

            // If any of entries in the metadata page are empty, that means that there
            // are no elements in the page beyond this, and thus the key was never inserted.
            if (_match_empty(current_page))
            {
                break;
            }

            assert(i < _page_count);
        }

        _metadata_pages[next_empty.first].entries[next_empty.second] = h2;
        std::construct_at(&_data_pages[next_empty.first][next_empty.second], std::move(value));

        ++_size;

        return {iterator{next_empty.first * _page_size + next_empty.second, this}, true};
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline void flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::erase(iterator pos)
    {
        auto idx = pos.index();
        auto page = idx / _page_size;
        auto slot = idx % _page_size;

        std::destroy_at(&_data_pages[page][slot]);
        _metadata_pages[page].entries[slot] = detail::deleted_entry;

        --_size;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline void flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::erase(const K& key)
    {
        auto it = find(key);
        if (it != end())
        {
            erase(it);
        }
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline void flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::clear() noexcept
    {
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            for (std::size_t j = 0; j < _page_size; ++j)
            {
                if (_metadata_strategy.is_full(_metadata_pages[i].entries[j]))
                {
                    std::destroy_at(&_data_pages[i][j]);
                    _metadata_pages[i].entries[j] = detail::empty_entry;
                }
            }
        }

        _size = 0;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline V& flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::operator[](const K& key)
    {
        static_assert(std::is_default_constructible_v<V>, "Value type must be default constructible");

        auto it = find(key);
        if (it != end())
        {
            return it->second;
        }

        auto result = insert({key, V{}});
        return result.position->second;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    template <typename InputIt>
    inline void flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::insert(InputIt first, InputIt last)
    {
        for (auto it = first; it != last; ++it)
        {
            insert(*it);
        }
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline void flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::_request_grow(std::size_t new_size)
    {
        assert(std::popcount(new_size) == 1); // Ensure that the new size is a power of 2

        auto page_count = new_size / _page_size;

        auto new_metadata_pages = _request_empty_metadata_pages(page_count);
        auto new_data_pages = _request_empty_data_pages(page_count);

        // Iterate over the existing pages and copy the data to the new pages
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            for (std::size_t j = 0; j < _page_size; ++j)
            {
                auto entry = _metadata_pages[i].entries[j];
                if (_metadata_strategy.is_full(entry))
                {
                    auto hash = _hash(_data_pages[i][j].first);
                    auto h1 = _get_h1(hash);
                    auto h2 = _get_h2(hash);

                    auto next_empty = _find_next_empty(h1, new_metadata_pages, page_count);

                    // Copy metadata
                    new_metadata_pages[next_empty.first].entries[next_empty.second] = h2;
                    std::destroy_at(&_metadata_pages[i].entries[j]);

                    // Move data is nothrow_move_constructible, else copy
                    if constexpr (std::is_nothrow_move_constructible_v<V>)
                    {
                        std::construct_at(&new_data_pages[next_empty.first][next_empty.second],
                                          std::move(_data_pages[i][j]));
                    }
                    else
                    {
                        std::construct_at(&new_data_pages[next_empty.first][next_empty.second], _data_pages[i][j]);
                    }
                    std::destroy_at(&_data_pages[i][j]);
                }
            }
        }

        // Deallocate the old pages
        metadata_alloc_traits::deallocate(_metadata_alloc, _metadata_pages, _page_count);
        alloc_traits::deallocate(_alloc, _data_pages, _page_count);

        // Assign the new pages
        _metadata_pages = new_metadata_pages;
        _data_pages = new_data_pages;

        _page_count = page_count;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::metadata_page* flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::_request_empty_metadata_pages(std::size_t count)
    {
        detail::metadata_group* pages = metadata_alloc_traits::allocate(_metadata_alloc, count);

        for (std::size_t i = 0; i < count; ++i)
        {
            std::construct_at(&pages[i]);
            for (std::size_t j = 0; j < _page_size; ++j)
            {
                std::construct_at(&pages[i].entries[j], detail::empty_entry);
            }
        }

        return pages;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline typename flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::data_page* flat_unordered_map<
        K, V, Hash, KeyEqual, Allocator>::_request_empty_data_pages(std::size_t count)
    {
        return alloc_traits::allocate(_alloc, count);
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline std::size_t flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::_compute_default_growth(
        std::size_t requested) const noexcept
    {
        if (requested < _page_size)
        {
            return _page_size;
        }
        return std::bit_ceil(requested);
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline std::size_t flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::_first_occupied_index() const noexcept
    {
        // TODO: Evaluate computing this value during insertions
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            for (std::size_t j = 0; j < _page_size; ++j)
            {
                if (_metadata_strategy.is_full(_metadata_pages[i].entries[j]))
                {
                    return i * _page_size + j;
                }
            }
        }

        // Table is empty
        return 0;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline std::uint64_t flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::_get_h1(std::size_t hc) const noexcept
    {
        // get the bottom 57 bits of the hash
        return hc & 0x00'7F'FF'FF'FF'FF'FF'FF;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline std::uint8_t flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::_get_h2(std::size_t hc) const noexcept
    {
        // get the upper 7 bits of the hash
        return static_cast<std::uint8_t>((hc >> 57) & 0x7F);
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline std::uint16_t flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::_get_hash_match(
        std::uint8_t h2, std::size_t page) const noexcept
    {
        // TODO: SIMD implementation

        std::uint32_t result{0};

        // For each entry in the metadata page, check if the hash matches the h2 value in the metadata. If it does,
        // set the nth bit of the result to 1.
        for (std::size_t i = 0; i < _page_size; ++i)
        {
            auto entry = _metadata_pages[page].entries[i];
            auto match = _metadata_strategy.is_full(entry);
            if (match && entry == h2)
            {
                result |= (1 << i);
            }
        }

        return result;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline bool flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::_match_empty(std::size_t page) const noexcept
    {
        for (std::size_t i = 0; i < _page_size; ++i)
        {
            bool empty = _metadata_pages[page].entries[i] == detail::empty_entry;
            if (empty)
            {
                return true;
            }
        }

        return false;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline bool flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::_match_empty_or_deleted(
        std::size_t page) const noexcept
    {
        for (std::size_t i = 0; i < _page_size; ++i)
        {
            bool empty = _metadata_pages[page].entries[i] == detail::empty_entry;
            bool deleted = _metadata_pages[page].entries[i] == detail::deleted_entry;
            if (empty || deleted)
            {
                return true;
            }
        }

        return false;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline std::pair<std::size_t, std::size_t> flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::_find_next_empty(
        std::size_t h1, metadata_page* pages, std::size_t page_count) const noexcept
    {
        for (std::size_t i = 0; i < page_count; ++i)
        {
            auto current_page = (h1 + i) % page_count;
            for (std::size_t j = 0; j < _page_size; ++j)
            {
                if (!_metadata_strategy.is_full(pages[current_page].entries[j]))
                {
                    return std::pair<std::size_t, std::size_t>{current_page, j};
                }
            }
        }

        std::abort();

        return std::pair<std::size_t, std::size_t>();
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline std::size_t flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::_next_occupied_index(
        std::size_t search_start) const noexcept
    {
        std::size_t current_page = search_start / _page_size;
        std::size_t current_slot = search_start % _page_size;

        for (std::size_t i = current_page; i < _page_count; ++i)
        {
            for (std::size_t j = current_slot; j < _page_size; ++j)
            {
                if (_metadata_strategy.is_full(_metadata_pages[i].entries[j]))
                {
                    return i * _page_size + j;
                }
            }

            current_slot = 0;
        }

        return _page_count * _page_size;
    }

    template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator>
    inline void flat_unordered_map<K, V, Hash, KeyEqual, Allocator>::_release()
    {
        // This only happens if the map has never been populated or has been moved from.  In those two cases, the
        // pointers are null and nothing needs to be cleaned up.
        if (_metadata_pages == nullptr)
        {
            return;
        }

        // Destroy all values in the value pages if they are present
        for (std::size_t i = 0; i < _page_count; ++i)
        {
            for (std::size_t j = 0; j < _page_size; ++j)
            {
                if (_metadata_strategy.is_full(_metadata_pages[i].entries[j]))
                {
                    std::destroy_at(&_data_pages[i][j]);
                }
            }
        }

        // Destroy all metadata pages
        std::destroy_n(_metadata_pages, _page_count);

        // Deallocate all memory
        metadata_alloc_traits::deallocate(_metadata_alloc, _metadata_pages, _page_count);
        alloc_traits::deallocate(_alloc, _data_pages, _page_count);

        _metadata_pages = nullptr;
        _data_pages = nullptr;

        _size = 0;
        _page_count = 0;
    }

    namespace detail
    {
        template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator, bool Const>
        inline flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, Const>::flat_unordered_map_iterator(
            std::size_t idx, map_type* map) noexcept
            : _index{idx}, _map{map}
        {
        }

        template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator, bool Const>
        inline flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, Const>& flat_unordered_map_iterator<
            K, V, Hash, KeyEqual, Allocator, Const>::operator++() noexcept
        {
            _index = _map->_next_occupied_index(_index + 1);

            return *this;
        }

        template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator, bool Const>
        inline flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, Const> flat_unordered_map_iterator<
            K, V, Hash, KeyEqual, Allocator, Const>::operator++(int) noexcept
        {
            auto copy = *this;
            ++(*this);
            return copy;
        }

        template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator, bool Const>
        inline typename flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, Const>::reference
        flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, Const>::operator*() const noexcept
        {
            return _map->_data_pages[_index / _map->_page_size][_index % _map->_page_size];
        }

        template <typename K, typename V, typename Hash, typename KeyEqual, typename Allocator, bool Const>
        inline typename flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, Const>::pointer
        flat_unordered_map_iterator<K, V, Hash, KeyEqual, Allocator, Const>::operator->() const noexcept
        {
            return &_map->_data_pages[_index / _map->_page_size][_index % _map->_page_size];
        }
    } // namespace detail
} // namespace tempest

#endif // tempest_core_flat_unordered_map_hpp