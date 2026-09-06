#ifndef tempest_deque_hpp
#define tempest_deque_hpp

#include <tempest/algorithm.hpp>
#include <tempest/int.hpp>
#include <tempest/iterator.hpp>
#include <tempest/memory.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    template <typename T, typename Allocator = allocator<T>>
    class deque
    {
      public:
        using value_type = T;
        using allocator_type = Allocator;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = value_type*;
        using const_pointer = const value_type*;

        template <bool IsConst>
        class deque_iterator
        {
          public:
            using value_type = T;
            using difference_type = ptrdiff_t;
            using pointer = conditional_t<IsConst, const T*, T*>;
            using reference = conditional_t<IsConst, const T&, T&>;
            using deque_ptr = conditional_t<IsConst, const deque*, deque*>;

            deque_iterator() = default;
            deque_iterator(deque_ptr d, size_type index) : _d(d), _index(index)
            {
            }

            [[nodiscard]] reference operator*() const
            {
                return (*_d)[_index];
            }
            [[nodiscard]] pointer operator->() const
            {
                return &(*_d)[_index];
            }

            deque_iterator& operator++()
            {
                ++_index;
                return *this;
            }
            deque_iterator operator++(int)
            {
                auto copy = *this;
                ++_index;
                return copy;
            }
            deque_iterator& operator--()
            {
                --_index;
                return *this;
            }
            deque_iterator operator--(int)
            {
                auto copy = *this;
                --_index;
                return copy;
            }

            deque_iterator& operator+=(difference_type n)
            {
                _index += n;
                return *this;
            }
            deque_iterator& operator-=(difference_type n)
            {
                _index -= n;
                return *this;
            }

            [[nodiscard]] deque_iterator operator+(difference_type n) const
            {
                return deque_iterator(_d, _index + n);
            }
            [[nodiscard]] deque_iterator operator-(difference_type n) const
            {
                return deque_iterator(_d, _index - n);
            }

            [[nodiscard]] difference_type operator-(const deque_iterator& other) const
            {
                return _index - other._index;
            }

            [[nodiscard]] reference operator[](difference_type n) const
            {
                return (*_d)[_index + n];
            }

            [[nodiscard]] bool operator==(const deque_iterator& other) const
            {
                return _index == other._index && _d == other._d;
            }
            [[nodiscard]] bool operator!=(const deque_iterator& other) const
            {
                return !(*this == other);
            }
            [[nodiscard]] bool operator<(const deque_iterator& other) const
            {
                return _index < other._index;
            }
            [[nodiscard]] bool operator>(const deque_iterator& other) const
            {
                return _index > other._index;
            }
            [[nodiscard]] bool operator<=(const deque_iterator& other) const
            {
                return _index <= other._index;
            }
            [[nodiscard]] bool operator>=(const deque_iterator& other) const
            {
                return _index >= other._index;
            }

            operator deque_iterator<true>() const
            {
                return deque_iterator<true>(_d, _index);
            }

          private:
            deque_ptr _d = nullptr;
            size_type _index = 0;
        };

        using iterator = deque_iterator<false>;
        using const_iterator = deque_iterator<true>;
        using reverse_iterator = tempest::reverse_iterator<iterator>;
        using const_reverse_iterator = tempest::reverse_iterator<const_iterator>;

        deque() = default;

        ~deque()
        {
            clear();
            delete_map();
        }

        deque(const deque& other)
            requires is_copy_constructible_v<T>
        {
            for (auto idx = size_type{0}; idx < other.size(); ++idx)
            {
                push_back(other[idx]);
            }
        }

        deque& operator=(const deque& other)
            requires is_copy_constructible_v<T> && is_copy_assignable_v<T>
        {
            if (this != &other)
            {
                clear();
                for (auto idx = size_type{0}; idx < other.size(); ++idx)
                {
                    push_back(other[idx]);
                }
            }
            return *this;
        }

        deque(deque&& other) noexcept
            : _map(other._map), _map_cap(other._map_cap), _head_block(other._head_block),
              _head_offset(other._head_offset), _size(other._size), _alloc(tempest::move(other._alloc))
        {
            other._map = nullptr;
            other._map_cap = 0;
            other._head_block = 0;
            other._head_offset = 0;
            other._size = 0;
        }

        deque& operator=(deque&& other) noexcept
        {
            if (this != &other)
            {
                clear();
                delete_map();
                _map = other._map;
                _map_cap = other._map_cap;
                _head_block = other._head_block;
                _head_offset = other._head_offset;
                _size = other._size;
                _alloc = tempest::move(other._alloc);

                other._map = nullptr;
                other._map_cap = 0;
                other._head_block = 0;
                other._head_offset = 0;
                other._size = 0;
            }
            return *this;
        }

        [[nodiscard]] reference operator[](size_type i)
        {
            const auto global_offset = static_cast<size_type>(_head_offset + i);
            const auto map_idx = static_cast<size_type>((_head_block + (global_offset / block_size)) % _map_cap);
            const auto in_block = static_cast<size_type>(global_offset % block_size);
            return _map[map_idx][in_block];
        }

        [[nodiscard]] const_reference operator[](size_type i) const
        {
            const auto global_offset = static_cast<size_type>(_head_offset + i);
            const auto map_idx = static_cast<size_type>((_head_block + (global_offset / block_size)) % _map_cap);
            const auto in_block = static_cast<size_type>(global_offset % block_size);
            return _map[map_idx][in_block];
        }

        [[nodiscard]] reference front()
        {
            return (*this)[0];
        }
        [[nodiscard]] const_reference front() const
        {
            return (*this)[0];
        }

        [[nodiscard]] reference back()
        {
            return (*this)[_size - 1];
        }
        [[nodiscard]] const_reference back() const
        {
            return (*this)[_size - 1];
        }

        [[nodiscard]] bool empty() const
        {
            return _size == 0;
        }
        [[nodiscard]] size_type size() const
        {
            return _size;
        }

        void push_back(const T& value)
            requires is_copy_constructible_v<T>
        {
            emplace_back(value);
        }

        void push_back(T&& value)
        {
            emplace_back(tempest::move(value));
        }

        template <typename... Args>
        decltype(auto) emplace_back(Args&&... args)
        {
            if (_map_cap == 0)
            {
                expand_map();
            }
            const auto new_total_blocks = static_cast<size_type>((_head_offset + _size) / block_size + 1);
            if (new_total_blocks > _map_cap)
            {
                expand_map();
            }

            const auto global_offset = static_cast<size_type>(_head_offset + _size);
            const auto map_idx = static_cast<size_type>((_head_block + (global_offset / block_size)) % _map_cap);
            const auto in_block = static_cast<size_type>(global_offset % block_size);

            if (!_map[map_idx])
            {
                _map[map_idx] = allocator_traits<Allocator>::allocate(_alloc, block_size);
            }
            allocator_traits<Allocator>::construct(_alloc, &_map[map_idx][in_block], tempest::forward<Args>(args)...);
            ++_size;
            return back();
        }

        template <typename... Args>
        decltype(auto) emplace_front(Args&&... args)
        {
            if (_size == 0)
            {
                return emplace_back(tempest::forward<Args>(args)...);
            }
            if (_head_offset == 0)
            {
                const auto active_blocks = static_cast<size_type>((_size - 1) / block_size + 1);
                if (active_blocks + 1 > _map_cap)
                {
                    expand_map();
                }
                _head_block = (_head_block == 0) ? _map_cap - 1 : _head_block - 1;
                _head_offset = block_size - 1;
            }
            else
            {
                --_head_offset;
            }

            if (!_map[_head_block])
            {
                _map[_head_block] = allocator_traits<Allocator>::allocate(_alloc, block_size);
            }
            allocator_traits<Allocator>::construct(_alloc, &_map[_head_block][_head_offset],
                                                   tempest::forward<Args>(args)...);
            ++_size;
            return front();
        }

        void push_front(const T& value)
            requires is_copy_constructible_v<T>
        {
            emplace_front(value);
        }

        void push_front(T&& value)
        {
            emplace_front(tempest::move(value));
        }

        void pop_back()
        {
            const auto global_offset = static_cast<size_type>(_head_offset + _size - 1);
            const auto map_idx = static_cast<size_type>((_head_block + (global_offset / block_size)) % _map_cap);
            const auto in_block = static_cast<size_type>(global_offset % block_size);

            allocator_traits<Allocator>::destroy(_alloc, &_map[map_idx][in_block]);
            --_size;

            if (_size == 0)
            {
                if (_map[_head_block])
                {
                    allocator_traits<Allocator>::deallocate(_alloc, _map[_head_block], block_size);
                    _map[_head_block] = nullptr;
                }
                _head_offset = 0;
                _head_block = 0;
            }
            else if (in_block == 0)
            {
                if (_map[map_idx])
                {
                    allocator_traits<Allocator>::deallocate(_alloc, _map[map_idx], block_size);
                    _map[map_idx] = nullptr;
                }
            }
        }

        void pop_front()
        {
            allocator_traits<Allocator>::destroy(_alloc, &_map[_head_block][_head_offset]);
            --_size;
            if (_size == 0)
            {
                if (_map[_head_block])
                {
                    allocator_traits<Allocator>::deallocate(_alloc, _map[_head_block], block_size);
                    _map[_head_block] = nullptr;
                }
                _head_offset = 0;
                _head_block = 0;
            }
            else
            {
                _head_offset++;
                if (_head_offset == block_size)
                {
                    if (_map[_head_block])
                    {
                        allocator_traits<Allocator>::deallocate(_alloc, _map[_head_block], block_size);
                        _map[_head_block] = nullptr;
                    }
                    _head_offset = 0;
                    _head_block = (_head_block + 1) % _map_cap;
                }
            }
        }

        void clear()
        {
            while (!empty())
                pop_back();
        }

        void swap(deque& other) noexcept
        {
            tempest::swap(_map, other._map);
            tempest::swap(_map_cap, other._map_cap);
            tempest::swap(_head_block, other._head_block);
            tempest::swap(_head_offset, other._head_offset);
            tempest::swap(_size, other._size);
            tempest::swap(_alloc, other._alloc);
        }

        [[nodiscard]] iterator begin()
        {
            return iterator(this, 0);
        }
        [[nodiscard]] iterator end()
        {
            return iterator(this, _size);
        }
        [[nodiscard]] const_iterator begin() const
        {
            return const_iterator(this, 0);
        }
        [[nodiscard]] const_iterator end() const
        {
            return const_iterator(this, _size);
        }
        [[nodiscard]] const_iterator cbegin() const
        {
            return const_iterator(this, 0);
        }
        [[nodiscard]] const_iterator cend() const
        {
            return const_iterator(this, _size);
        }

        [[nodiscard]] reverse_iterator rbegin()
        {
            return reverse_iterator(end());
        }
        [[nodiscard]] reverse_iterator rend()
        {
            return reverse_iterator(begin());
        }
        [[nodiscard]] const_reverse_iterator rbegin() const
        {
            return const_reverse_iterator(end());
        }
        [[nodiscard]] const_reverse_iterator rend() const
        {
            return const_reverse_iterator(begin());
        }
        [[nodiscard]] const_reverse_iterator crbegin() const
        {
            return const_reverse_iterator(cend());
        }
        [[nodiscard]] const_reverse_iterator crend() const
        {
            return const_reverse_iterator(cbegin());
        }

      private:
        static constexpr auto next_power_of_2(size_type n) -> size_type
        {
            auto p = size_type{1};
            while (p < n)
            {
                p <<= 1;
            }
            return p;
        }

        static constexpr size_type block_size = next_power_of_2(sizeof(T) <= 1024 ? 4096 / sizeof(T) : 16);

        T** _map = nullptr;
        size_type _map_cap = 0;
        size_type _head_block = 0;
        size_type _head_offset = 0;
        size_type _size = 0;
        Allocator _alloc;

        using map_allocator_type = typename allocator_traits<Allocator>::template rebind_alloc<T*>;

        void delete_map()
        {
            if (_map)
            {
                for (auto i = size_type{0}; i < _map_cap; ++i)
                {
                    if (_map[i])
                    {
                        allocator_traits<Allocator>::deallocate(_alloc, _map[i], block_size);
                    }
                }
                auto map_alloc = map_allocator_type(_alloc);
                allocator_traits<map_allocator_type>::deallocate(map_alloc, _map, _map_cap);
            }
        }

        void expand_map()
        {
            const auto new_cap = _map_cap == 0 ? size_type{8} : static_cast<size_type>(_map_cap * 2);
            auto map_alloc = map_allocator_type(_alloc);
            auto* new_map = allocator_traits<map_allocator_type>::allocate(map_alloc, new_cap);
            for (auto i = size_type{0}; i < new_cap; ++i)
            {
                new_map[i] = nullptr;
            }

            const auto active_blocks =
                _size == 0 ? size_type{0} : static_cast<size_type>((_head_offset + _size - 1) / block_size + 1);
            for (auto i = size_type{0}; i < active_blocks; ++i)
            {
                new_map[i] = _map[(_head_block + i) % _map_cap];
            }

            if (_map)
            {
                allocator_traits<map_allocator_type>::deallocate(map_alloc, _map, _map_cap);
            }
            _map = new_map;
            _map_cap = new_cap;
            _head_block = 0;
        }
    };

    template <typename T, typename Allocator>
    constexpr auto operator==(const deque<T, Allocator>& lhs, const deque<T, Allocator>& rhs) -> bool
    {
        return lhs.size() == rhs.size() && tempest::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    template <typename T, typename Allocator>
    constexpr auto operator<=>(const deque<T, Allocator>& lhs, const deque<T, Allocator>& rhs)
    {
        return tempest::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    template <typename T, typename Allocator>
    constexpr void swap(deque<T, Allocator>& lhs, deque<T, Allocator>& rhs) noexcept
    {
        lhs.swap(rhs);
    }
} // namespace tempest

#endif // tempest_deque_hpp
