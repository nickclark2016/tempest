#ifndef tempest_core_vector_hpp
#define tempest_core_vector_hpp

#include "memory.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iterator>

namespace tempest::core
{
    template <typename T, typename Allocator = allocator<T>>
    class vector
    {
      public:
        using value_type = T;
        using allocator_type = Allocator;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = typename allocator_traits<Allocator>::pointer;
        using const_pointer = typename allocator_traits<Allocator>::const_pointer;
        using iterator = pointer;
        using const_iterator = const_pointer;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        constexpr vector() = default;
        explicit constexpr vector(const Allocator& alloc) noexcept(noexcept(Allocator()));
        constexpr vector(size_type count, const T& value, const Allocator& alloc = Allocator());
        explicit constexpr vector(size_type count, const Allocator& alloc = Allocator());
        constexpr vector(std::initializer_list<T> init, const Allocator& alloc = Allocator());

        template <std::input_iterator It>
        constexpr vector(It first, It last, const Allocator& alloc = Allocator());

        constexpr vector(const vector& other);
        constexpr vector(const vector& other, const Allocator& alloc);
        constexpr vector(vector&& other) noexcept;
        constexpr vector(vector&& other, const Allocator& alloc);

        constexpr ~vector();

        constexpr vector& operator=(const vector& other);
        constexpr vector& operator=(vector&& other) noexcept(
            allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
            allocator_traits<Allocator>::is_always_equal::value);

        constexpr void assign(size_type count, const T& value);

        template <typename InputIt>
        constexpr void assign(InputIt first, InputIt last);

        constexpr allocator_type get_allocator() const;

        constexpr reference at(size_type pos);
        constexpr const_reference at(size_type pos) const;
        constexpr reference operator[](size_type pos);
        constexpr const_reference operator[](size_type pos) const;

        constexpr reference front();
        constexpr const_reference front() const;
        constexpr reference back();
        constexpr const_reference back() const;

        constexpr T* data() noexcept;
        constexpr const T* data() const noexcept;

        constexpr iterator begin() noexcept;
        constexpr const_iterator begin() const noexcept;
        constexpr const_iterator cbegin() const noexcept;

        constexpr iterator end() noexcept;
        constexpr const_iterator end() const noexcept;
        constexpr const_iterator cend() const noexcept;

        constexpr reverse_iterator rbegin() noexcept;
        constexpr const_reverse_iterator rbegin() const noexcept;
        constexpr const_reverse_iterator crbegin() const noexcept;

        constexpr reverse_iterator rend() noexcept;
        constexpr const_reverse_iterator rend() const noexcept;
        constexpr const_reverse_iterator crend() const noexcept;

        constexpr bool empty() const noexcept;
        constexpr size_type size() const noexcept;
        constexpr size_type max_size() const noexcept;
        constexpr size_type capacity() const noexcept;

        constexpr void reserve(size_type new_cap);
        constexpr void shrink_to_fit();

        constexpr void clear() noexcept;

        constexpr iterator insert(const_iterator pos, const T& value);
        constexpr iterator insert(const_iterator pos, T&& value);
        constexpr iterator insert(const_iterator pos, size_type count, const T& value);

        template <typename InputIt>
        constexpr iterator insert(const_iterator pos, InputIt first, InputIt last);

        template <typename... Args>
        constexpr iterator emplace(const_iterator pos, Args&&... args);

        constexpr iterator erase(const_iterator pos);
        constexpr iterator erase(const_iterator first, const_iterator last);

        constexpr void push_back(const T& value);
        constexpr void push_back(T&& value);

        template <typename... Args>
        constexpr reference emplace_back(Args&&... args);

        constexpr void pop_back();

        constexpr void resize(size_type count);
        constexpr void resize(size_type count, const T& value);

        constexpr void swap(vector& other) noexcept(allocator_traits<Allocator>::propagate_on_container_swap::value ||
                                                    allocator_traits<Allocator>::is_always_equal::value);

      private:
        Allocator _alloc;

        T* _data{nullptr};
        T* _end{nullptr};
        T* _capacity_end{nullptr};

        constexpr size_type _compute_next_capacity(size_type requested_capacity) const noexcept;
        constexpr void _emplace_one_at_back(const T& value);
        constexpr void _emplace_one_at_back(T&& value);
    };

    template <typename T, typename Allocator>
    constexpr bool operator==(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs);

    template <typename T, typename Allocator>
    constexpr auto operator<=>(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs);

    template <typename T, typename Allocator>
    void swap(vector<T, Allocator>& lhs, vector<T, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs)));

    template <typename T, typename Alloc, typename U>
    constexpr vector<T, Alloc>::size_type erase(vector<T, Alloc>& c, const U& value);

    template <typename T, typename Alloc, typename Pred>
    constexpr vector<T, Alloc>::size_type erase_if(vector<T, Alloc>& c, Pred pred);

    template <typename InputIt, typename Alloc = allocator<typename std::iterator_traits<InputIt>::value_type>>
    vector(InputIt, InputIt, Alloc = Alloc()) -> vector<typename std::iterator_traits<InputIt>::value_type, Alloc>;

    // Implementation

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>::vector(const Allocator& alloc) noexcept(noexcept(Allocator())) : _alloc{alloc}
    {
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>::vector(size_type count, const T& value, const Allocator& alloc) : _alloc{alloc}
    {
        resize(count, value);
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>::vector(size_type count, const Allocator& alloc) : _alloc{alloc}
    {
        resize(count);
    }

    template <typename T, typename Allocator>
    inline constexpr vector<T, Allocator>::vector(std::initializer_list<T> init, const Allocator& alloc) : _alloc{alloc}
    {
        reserve(init.size());

        for (const auto& value : init)
        {
            push_back(value);
        }
    }

    template <typename T, typename Allocator>
    template <std::input_iterator It>
    inline constexpr vector<T, Allocator>::vector(It first, It last, const Allocator& alloc) : _alloc{alloc}
    {
        reserve(std::distance(first, last));

        for (auto it = first; it != last; ++it)
        {
            push_back(*it);
        }
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>::vector(const vector& other)
        : _alloc{allocator_traits<Allocator>::select_on_container_copy_construction(other._alloc)}
    {
        reserve(other.size());

        for (const auto& value : other)
        {
            push_back(value);
        }
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>::vector(const vector& other, const Allocator& alloc) : _alloc{alloc}
    {
        reserve(other.size());

        for (const auto& value : other)
        {
            push_back(value);
        }
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>::vector(vector&& other) noexcept : _alloc{std::move(other._alloc)}
    {
        _data = other._data;
        _end = other._end;
        _capacity_end = other._capacity_end;

        other._data = nullptr;
        other._end = nullptr;
        other._capacity_end = nullptr;
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>::vector(vector&& other, const Allocator& alloc) : _alloc{alloc}
    {
        if (_alloc == other._alloc)
        {
            _data = other._data;
            _end = other._end;
            _capacity_end = other._capacity_end;

            other._data = nullptr;
            other._end = nullptr;
            other._capacity_end = nullptr;
        }
        else
        {
            reserve(other.size());

            for (auto&& value : other)
            {
                push_back(std::move(value));
            }
        }
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>::~vector()
    {
        if (_data)
        {
            clear();
            allocator_traits<Allocator>::deallocate(_alloc, _data, capacity());
        }
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>& vector<T, Allocator>::operator=(const vector& other)
    {
        if (this == &other)
        {
            return *this;
        }

        clear();
        reserve(other.size());

        for (const auto& value : other)
        {
            push_back(value);
        }

        return *this;
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>& vector<T, Allocator>::operator=(vector&& other) noexcept(
        allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
        allocator_traits<Allocator>::is_always_equal::value)
    {
        if (this == &other)
        {
            return *this;
        }

        clear();
        allocator_traits<Allocator>::deallocate(_alloc, _data, capacity());

        _alloc = std::move(other._alloc);
        _data = other._data;
        _end = other._end;
        _capacity_end = other._capacity_end;

        other._data = nullptr;
        other._end = nullptr;
        other._capacity_end = nullptr;

        return *this;
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::assign(size_type count, const T& value)
    {
        clear();
        reserve(count);

        for (size_type i = 0; i < count; ++i)
        {
            push_back(value);
        }
    }

    template <typename T, typename Allocator>
    template <typename InputIt>
    constexpr void vector<T, Allocator>::assign(InputIt first, InputIt last)
    {
        clear();
        reserve(std::distance(first, last));

        for (auto it = first; it != last; ++it)
        {
            push_back(*it);
        }
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::allocator_type vector<T, Allocator>::get_allocator() const
    {
        return _alloc;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::reference vector<T, Allocator>::at(size_type pos)
    {
        assert(pos < size());
        return _data[pos];
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_reference vector<T, Allocator>::at(size_type pos) const
    {
        assert(pos < size());
        return _data[pos];
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::reference vector<T, Allocator>::operator[](size_type pos)
    {
        return _data[pos];
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_reference vector<T, Allocator>::operator[](size_type pos) const
    {
        return _data[pos];
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::reference vector<T, Allocator>::front()
    {
        return *_data;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_reference vector<T, Allocator>::front() const
    {
        return *_data;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::reference vector<T, Allocator>::back()
    {
        return *(_end - 1);
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_reference vector<T, Allocator>::back() const
    {
        return *(_end - 1);
    }

    template <typename T, typename Allocator>
    constexpr T* vector<T, Allocator>::data() noexcept
    {
        return _data;
    }

    template <typename T, typename Allocator>
    constexpr const T* vector<T, Allocator>::data() const noexcept
    {
        return _data;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::begin() noexcept
    {
        return _data;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_iterator vector<T, Allocator>::begin() const noexcept
    {
        return _data;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_iterator vector<T, Allocator>::cbegin() const noexcept
    {
        return _data;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::end() noexcept
    {
        return _end;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_iterator vector<T, Allocator>::end() const noexcept
    {
        return _end;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_iterator vector<T, Allocator>::cend() const noexcept
    {
        return _end;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::reverse_iterator vector<T, Allocator>::rbegin() noexcept
    {
        return reverse_iterator{end()};
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_reverse_iterator vector<T, Allocator>::rbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_reverse_iterator vector<T, Allocator>::crbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::reverse_iterator vector<T, Allocator>::rend() noexcept
    {
        return reverse_iterator{begin()};
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_reverse_iterator vector<T, Allocator>::rend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::const_reverse_iterator vector<T, Allocator>::crend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

    template <typename T, typename Allocator>
    constexpr bool vector<T, Allocator>::empty() const noexcept
    {
        return size() == 0;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::size_type vector<T, Allocator>::size() const noexcept
    {
        return static_cast<size_type>(_end - _data);
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::size_type vector<T, Allocator>::max_size() const noexcept
    {
        return allocator_traits<Allocator>::max_size(_alloc);
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::size_type vector<T, Allocator>::capacity() const noexcept
    {
        return _capacity_end - _data;
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::reserve(size_type new_cap)
    {
        if (new_cap <= capacity())
        {
            return;
        }

        auto new_data = allocator_traits<Allocator>::allocate(_alloc, new_cap);
        auto new_end = new_data;

        for (auto it = begin(); it != end(); ++it)
        {
            allocator_traits<Allocator>::construct(_alloc, new_end++, std::move(*it));
        }

        clear();
        allocator_traits<Allocator>::deallocate(_alloc, _data, capacity());

        _data = new_data;
        _end = new_end;
        _capacity_end = _data + new_cap;
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::shrink_to_fit()
    {
        if (size() == capacity())
        {
            return;
        }

        auto new_data = allocator_traits<Allocator>::allocate(_alloc, size());
        auto new_end = new_data;

        for (auto it = begin(); it != end(); ++it)
        {
            allocator_traits<Allocator>::construct(_alloc, new_end++, std::move(*it));
        }

        clear();
        allocator_traits<Allocator>::deallocate(_alloc, _data, capacity());

        _data = new_data;
        _end = new_end;
        _capacity_end = _data + size();
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::clear() noexcept
    {
        for (auto it = begin(); it != end(); ++it)
        {
            allocator_traits<Allocator>::destroy(_alloc, it);
        }

        _end = _data;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert(const_iterator pos, const T& value)
    {
        auto index = pos - begin();
        reserve(_compute_next_capacity(size() + 1));

        for (auto it = end(); it != begin() + index; --it)
        {
            *it = std::move(*(it - 1));
        }

        allocator_traits<Allocator>::construct(_alloc, _data + index, value);
        ++_end;

        return begin() + index;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert(const_iterator pos, T&& value)
    {
        auto index = pos - begin();
        reserve(_compute_next_capacity(size() + 1));

        for (auto it = end(); it != begin() + index; --it)
        {
            *it = std::move(*(it - 1));
        }

        allocator_traits<Allocator>::construct(_alloc, _data + index, std::move(value));
        ++_end;

        return begin() + index;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert(const_iterator pos, size_type count,
                                                                                   const T& value)
    {
        auto index = pos - begin();
        reserve(_compute_next_capacity(size() + 1));

        for (auto it = end(); it != begin() + index; --it)
        {
            *it = std::move(*(it - count));
        }

        for (size_type i = 0; i < count; ++i)
        {
            allocator_traits<Allocator>::construct(_alloc, _data + index + i, value);
        }

        _end += count;

        return begin() + index;
    }

    template <typename T, typename Allocator>
    template <typename InputIt>
    constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert(const_iterator pos, InputIt first,
                                                                                   InputIt last)
    {
        auto index = pos - begin();
        auto count = std::distance(first, last);
        reserve(_compute_next_capacity(size() + 1));

        for (auto it = end(); it != begin() + index; --it)
        {
            *it = std::move(*(it - count));
        }

        for (size_type i = 0; i < count; ++i)
        {
            allocator_traits<Allocator>::construct(_alloc, _data + index + i, *first++);
        }

        _end += count;

        return begin() + index;
    }

    template <typename T, typename Allocator>
    template <typename... Args>
    constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::emplace(const_iterator pos, Args&&... args)
    {
        auto index = pos - begin();
        reserve(_compute_next_capacity(size() + 1));

        for (auto it = end(); it != begin() + index; --it)
        {
            *it = std::move(*(it - 1));
        }

        allocator_traits<Allocator>::construct(_alloc, _data + index, std::forward<Args>(args)...);
        ++_end;

        return begin() + index;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::erase(const_iterator pos)
    {
        auto index = pos - begin();

        for (auto it = begin() + index; it != end() - 1; ++it)
        {
            *it = std::move(*(it + 1));
        }

        --_end;

        // Destroy the last element
        allocator_traits<Allocator>::destroy(_alloc, _end);

        return begin() + index;
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::erase(const_iterator first,
                                                                                  const_iterator last)
    {
        auto index = first - begin();
        auto count = last - first;

        for (auto it = begin() + index; it != end() - count; ++it)
        {
            *it = std::move(*(it + count));
        }

        for (auto it = end() - count; it != end(); ++it)
        {
            allocator_traits<Allocator>::destroy(_alloc, it);
        }

        _end -= count;

        // Destroy the last count elements
        for (auto it = end(); it != end() + count; ++it)
        {
            allocator_traits<Allocator>::destroy(_alloc, it);
        }

        return begin() + index;
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::push_back(const T& value)
    {
        _emplace_one_at_back(value);
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::push_back(T&& value)
    {
        _emplace_one_at_back(std::move(value));
    }

    template <typename T, typename Allocator>
    template <typename... Args>
    constexpr typename vector<T, Allocator>::reference vector<T, Allocator>::emplace_back(Args&&... args)
    {
        _emplace_one_at_back(std::forward<Args>(args)...);
        return back();
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::pop_back()
    {
        erase(end() - 1);
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::resize(size_type count)
    {
        if (count < size())
        {
            erase(begin() + count, end());
        }
        else if (count > size())
        {
            reserve(count);
            for (size_type i = size(); i < count; ++i)
            {
                allocator_traits<Allocator>::construct(_alloc, _end++, T());
            }
        }
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::resize(size_type count, const T& value)
    {
        if (count < size())
        {
            erase(begin() + count, end());
        }
        else if (count > size())
        {
            reserve(count);
            for (size_type i = size(); i < count; ++i)
            {
                allocator_traits<Allocator>::construct(_alloc, _end++, value);
            }
        }
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::swap(vector& other) noexcept(
        allocator_traits<Allocator>::propagate_on_container_swap::value ||
        allocator_traits<Allocator>::is_always_equal::value)
    {
        if (_alloc == other._alloc)
        {
            std::swap(_data, other._data);
            std::swap(_end, other._end);
            std::swap(_capacity_end, other._capacity_end);
        }
        else
        {
            vector tmp{std::move(*this)};
            *this = std::move(other);
            other = std::move(tmp);
        }
    }

    template <typename T, typename Allocator>
    constexpr typename vector<T, Allocator>::size_type vector<T, Allocator>::_compute_next_capacity(
        size_type requested_capacity) const noexcept
    {
        return std::bit_ceil(requested_capacity);
    }

    template <typename T, typename Allocator>
    inline constexpr void vector<T, Allocator>::_emplace_one_at_back(const T& value)
    {
        reserve(_compute_next_capacity(size() + 1));
        allocator_traits<Allocator>::construct(_alloc, _end++, value);
    }

    template <typename T, typename Allocator>
    inline constexpr void vector<T, Allocator>::_emplace_one_at_back(T&& value)
    {
        reserve(_compute_next_capacity(size() + 1));
        allocator_traits<Allocator>::construct(_alloc, _end++, std::move(value));
    }

    template <typename T, typename Allocator>
    constexpr bool operator==(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs)
    {
        return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    template <typename T, typename Allocator>
    constexpr auto operator<=>(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs)
    {
        return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    template <typename T, typename Allocator>
    void swap(vector<T, Allocator>& lhs, vector<T, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    template <typename T, typename Alloc, typename U>
    constexpr vector<T, Alloc>::size_type erase(vector<T, Alloc>& c, const U& value)
    {
        auto it = std::remove(c.begin(), c.end(), value);
        auto count = std::distance(it, c.end());
        c.erase(it, c.end());
        return count;
    }

    template <typename T, typename Alloc, typename Pred>
    constexpr vector<T, Alloc>::size_type erase_if(vector<T, Alloc>& c, Pred pred)
    {
        auto it = std::remove_if(c.begin(), c.end(), pred);
        auto count = std::distance(it, c.end());
        c.erase(it, c.end());
        return count;
    }
} // namespace tempest::core

#endif // tempest_core_vector_hpp