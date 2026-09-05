#ifndef tempest_core_vector_hpp
#define tempest_core_vector_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/assert.hpp>
#include <tempest/bit.hpp>
#include <tempest/iterator.hpp>
#include <tempest/memory.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    template <typename T, typename Allocator>
    class vector;

    namespace unsafe
    {
        template <typename T, typename Allocator>
        void resize_no_init(vector<T, Allocator>& vec, size_t count);
    }

    template <typename T, typename Allocator = allocator<T>>
    class vector
    {
      public:
        using value_type = T;
        using allocator_type = Allocator;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = allocator_traits<Allocator>::pointer;
        using const_pointer = allocator_traits<Allocator>::const_pointer;
        using iterator = pointer;
        using const_iterator = const_pointer;
        using reverse_iterator = tempest::reverse_iterator<iterator>;
        using const_reverse_iterator = tempest::reverse_iterator<const_iterator>;

        constexpr vector() = default;
        explicit constexpr vector(const Allocator& alloc) noexcept(noexcept(Allocator()));
        constexpr vector(size_type count, const T& value, const Allocator& alloc = Allocator());
        explicit constexpr vector(size_type count, const Allocator& alloc = Allocator());

        template <input_iterator It>
        constexpr vector(It first, It last, const Allocator& alloc = Allocator());

        constexpr vector(const vector& other)
            requires is_copy_constructible_v<T>;
        constexpr vector(const vector& other, const Allocator& alloc)
            requires is_copy_constructible_v<T>;
        constexpr vector(vector&& other) noexcept;
        constexpr vector(vector&& other, const Allocator& alloc);

        template <typename... Ts>
            requires(sizeof...(Ts) > 0) &&
                    (conjunction_v<is_convertible<Ts, T>...> || conjunction_v<is_constructible<T, Ts>...>)
        constexpr vector(init_list_t /*unused*/, Ts&&... values);

        constexpr ~vector();

        constexpr auto operator=(const vector& other) -> vector&
            requires is_copy_assignable_v<T>;
        constexpr auto operator=(vector&& other) noexcept(
            allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
            allocator_traits<Allocator>::is_always_equal::value) -> vector&;

        constexpr void assign(size_type count, const T& value);

        template <typename InputIt>
        constexpr void assign(InputIt first, InputIt last);

        constexpr auto get_allocator() const -> allocator_type;

        constexpr auto at(size_type pos) -> reference;
        constexpr auto at(size_type pos) const -> const_reference;
        constexpr auto operator[](size_type pos) -> reference;
        constexpr auto operator[](size_type pos) const -> const_reference;

        constexpr auto front() -> reference;
        constexpr auto front() const -> const_reference;
        constexpr auto back() -> reference;
        constexpr auto back() const -> const_reference;

        constexpr auto data() noexcept -> T*;
        constexpr auto data() const noexcept -> const T*;

        constexpr auto begin() noexcept -> iterator;
        constexpr auto begin() const noexcept -> const_iterator;
        constexpr auto cbegin() const noexcept -> const_iterator;

        constexpr auto end() noexcept -> iterator;
        constexpr auto end() const noexcept -> const_iterator;
        constexpr auto cend() const noexcept -> const_iterator;

        constexpr auto rbegin() noexcept -> reverse_iterator;
        constexpr auto rbegin() const noexcept -> const_reverse_iterator;
        constexpr auto crbegin() const noexcept -> const_reverse_iterator;

        constexpr auto rend() noexcept -> reverse_iterator;
        constexpr auto rend() const noexcept -> const_reverse_iterator;
        constexpr auto crend() const noexcept -> const_reverse_iterator;

        [[nodiscard]] constexpr auto empty() const noexcept -> bool;
        [[nodiscard]] constexpr auto size() const noexcept -> size_type;
        [[nodiscard]] constexpr auto max_size() const noexcept -> size_type;
        [[nodiscard]] constexpr auto capacity() const noexcept -> size_type;

        constexpr void reserve(size_type new_cap);
        constexpr void shrink_to_fit();

        constexpr void clear() noexcept;

        constexpr auto insert(const_iterator pos, const T& value) -> iterator
            requires is_copy_constructible_v<T>;
        constexpr auto insert(const_iterator pos, T&& value) -> iterator;

        constexpr auto insert(const_iterator pos, size_type count, const T& value) -> iterator
            requires is_copy_constructible_v<T>;

        template <typename InputIt>
        constexpr auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator;

        template <typename... Args>
        constexpr auto emplace(const_iterator pos, Args&&... args) -> iterator;

        constexpr auto erase(const_iterator pos) -> iterator;
        constexpr auto erase(const_iterator first, const_iterator last) -> iterator;

        constexpr void push_back(const T& value)
            requires is_copy_constructible_v<T>;

        constexpr void push_back(T&& value);

        template <typename... Args>
        constexpr auto emplace_back(Args&&... args) -> reference;

        constexpr void pop_back();

        constexpr void resize(size_type count);
        constexpr void resize(size_type count, const T& value);

        constexpr void swap(vector& other) noexcept(allocator_traits<Allocator>::propagate_on_container_swap::value ||
                                                    allocator_traits<Allocator>::is_always_equal::value);

        template <typename U>
        auto reinterpret_as() noexcept -> vector<U>;

      private:
        Allocator _alloc;

        T* _data{nullptr};
        T* _end{nullptr};
        T* _capacity_end{nullptr};

        [[nodiscard]] constexpr auto _compute_next_capacity(size_type requested_capacity) const noexcept -> size_type;

        template <typename... Args>
        constexpr void _emplace_one_at_back(Args&&... args);

        friend void unsafe::resize_no_init(vector<T, Allocator>& vec, size_t count);
    };

    template <typename T, typename Allocator>
    constexpr auto operator==(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs) -> bool;

    template <typename T, typename Allocator>
    constexpr auto operator<=>(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs);

    template <typename T, typename Allocator>
    constexpr void swap(vector<T, Allocator>& lhs, vector<T, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs)));

    template <typename T, typename Alloc, typename U>
    constexpr auto erase(vector<T, Alloc>& c, const U& value) -> vector<T, Alloc>::size_type;

    template <typename T, typename Alloc, typename Pred>
    constexpr auto erase_if(vector<T, Alloc>& c, Pred pred) -> vector<T, Alloc>::size_type;

    template <typename InputIt, typename Alloc = allocator<typename iterator_traits<InputIt>::value_type>>
    vector(InputIt, InputIt, Alloc = Alloc()) -> vector<typename iterator_traits<InputIt>::value_type, Alloc>;

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
    template <input_iterator It>
    constexpr vector<T, Allocator>::vector(It first, It last, const Allocator& alloc) : _alloc{alloc}
    {
        reserve(distance(first, last));

        if constexpr (contiguous_iterator<It> && is_trivial_v<typename iterator_traits<It>::value_type> &&
                      is_same_v<typename iterator_traits<It>::value_type, T>)
        {
            const auto count = tempest::distance(first, last);
            auto first_ptr = to_address(first);
            copy_n(first_ptr, count, _data);
            _end = _data + count;
        }
        else
        {
            for (auto it = first; it != last; ++it)
            {
                push_back(*it);
            }
        }
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>::vector(const vector& other)
        requires is_copy_constructible_v<T>
        : _alloc{allocator_traits<Allocator>::select_on_container_copy_construction(other._alloc)}
    {
        reserve(other.size());

        if constexpr (is_trivial_v<T>)
        {
            const auto count = other.size();
            const auto first_ptr = other.data();
            copy_n(first_ptr, count, _data);
            _end = _data + count;
        }
        else
        {
            for (const auto& value : other)
            {
                push_back(value);
            }
        }
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>::vector(const vector& other, const Allocator& alloc)
        requires is_copy_constructible_v<T>
        : _alloc{alloc}
    {
        reserve(other.size());

        for (const auto& value : other)
        {
            push_back(value);
        }
    }

    template <typename T, typename Allocator>
    constexpr vector<T, Allocator>::vector(vector&& other) noexcept : _alloc{tempest::move(other._alloc)}, _data(other._data), _end(other._end), _capacity_end(other._capacity_end)
    {
        
        
        

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
                push_back(tempest::move(value));
            }
        }
    }

    template <typename T, typename Allocator>
    template <typename... Ts>
        requires(sizeof...(Ts) > 0) &&
                (conjunction_v<is_convertible<Ts, T>...> || conjunction_v<is_constructible<T, Ts>...>)
    constexpr vector<T, Allocator>::vector(init_list_t /*unused*/, Ts&&... values)
    {
        static_assert(sizeof...(Ts) > 0, "At least one value must be provided");
        static_assert((conjunction_v<is_convertible<Ts, T>...> || conjunction_v<is_constructible<T, Ts>...>),
                      "All values must be convertible or constructible to T");

        reserve(sizeof...(Ts));
        (push_back(tempest::forward<Ts>(values)), ...);
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
    constexpr auto vector<T, Allocator>::operator=(const vector& other) -> vector<T, Allocator>&
        requires is_copy_assignable_v<T>
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
    constexpr auto vector<T, Allocator>::operator=(vector&& other) noexcept(
        allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
        allocator_traits<Allocator>::is_always_equal::value) -> vector<T, Allocator>&
    {
        if (this == &other)
        {
            return *this;
        }

        clear();
        allocator_traits<Allocator>::deallocate(_alloc, _data, capacity());

        _alloc = tempest::move(other._alloc);
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
        reserve(distance(first, last));

        for (auto it = first; it != last; ++it)
        {
            push_back(*it);
        }
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::get_allocator() const -> vector<T, Allocator>::allocator_type
    {
        return _alloc;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::at(size_type pos) -> vector<T, Allocator>::reference
    {
        TEMPEST_ASSERT(pos < size());
        return _data[pos];
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::at(size_type pos) const -> vector<T, Allocator>::const_reference
    {
        TEMPEST_ASSERT(pos < size());
        return _data[pos];
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::operator[](size_type pos) -> vector<T, Allocator>::reference
    {
        return _data[pos];
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::operator[](size_type pos) const -> vector<T, Allocator>::const_reference
    {
        return _data[pos];
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::front() -> vector<T, Allocator>::reference
    {
        return *_data;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::front() const -> vector<T, Allocator>::const_reference
    {
        return *_data;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::back() -> vector<T, Allocator>::reference
    {
        return *(_end - 1);
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::back() const -> vector<T, Allocator>::const_reference
    {
        return *(_end - 1);
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::data() noexcept -> T*
    {
        return _data;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::data() const noexcept -> const T*
    {
        return _data;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::begin() noexcept -> vector<T, Allocator>::iterator
    {
        return _data;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::begin() const noexcept -> vector<T, Allocator>::const_iterator
    {
        return _data;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::cbegin() const noexcept -> vector<T, Allocator>::const_iterator
    {
        return _data;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::end() noexcept -> vector<T, Allocator>::iterator
    {
        return _end;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::end() const noexcept -> vector<T, Allocator>::const_iterator
    {
        return _end;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::cend() const noexcept -> vector<T, Allocator>::const_iterator
    {
        return _end;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::rbegin() noexcept -> vector<T, Allocator>::reverse_iterator
    {
        return reverse_iterator{end()};
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::rbegin() const noexcept -> vector<T, Allocator>::const_reverse_iterator
    {
        return const_reverse_iterator{end()};
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::crbegin() const noexcept -> vector<T, Allocator>::const_reverse_iterator
    {
        return const_reverse_iterator{cend()};
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::rend() noexcept -> vector<T, Allocator>::reverse_iterator
    {
        return reverse_iterator{begin()};
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::rend() const noexcept -> vector<T, Allocator>::const_reverse_iterator
    {
        return const_reverse_iterator{begin()};
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::crend() const noexcept -> vector<T, Allocator>::const_reverse_iterator
    {
        return const_reverse_iterator{begin()};
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::empty() const noexcept -> bool
    {
        return size() == 0;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::size() const noexcept -> vector<T, Allocator>::size_type
    {
        return static_cast<size_type>(_end - _data);
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::max_size() const noexcept -> vector<T, Allocator>::size_type
    {
        return allocator_traits<Allocator>::max_size(_alloc);
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::capacity() const noexcept -> vector<T, Allocator>::size_type
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

        if constexpr (is_trivially_copyable_v<T>)
        {
            if (_data && size() > 0)
            {
                tempest::memcpy(new_data, _data, size() * sizeof(T));
            }
            new_end = new_data + size();
        }
        else
        {
            for (auto it = begin(); it != end(); ++it)
            {
                allocator_traits<Allocator>::construct(_alloc, new_end++, tempest::move(*it));
            }
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

        if constexpr (is_trivially_copyable_v<T>)
        {
            if (_data && size() > 0)
            {
                tempest::memcpy(new_data, _data, size() * sizeof(T));
            }
            new_end = new_data + size();
        }
        else
        {
            for (auto it = begin(); it != end(); ++it)
            {
                allocator_traits<Allocator>::construct(_alloc, new_end++, tempest::move(*it));
            }
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
        if constexpr (!is_trivially_destructible_v<T>)
        {
            for (auto it = begin(); it != end(); ++it)
            {
                allocator_traits<Allocator>::destroy(_alloc, it);
            }
        }

        _end = _data;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::insert(const_iterator pos, const T& value) -> vector<T, Allocator>::iterator
        requires is_copy_constructible_v<T>
    {
        auto index = pos - begin();
        reserve(_compute_next_capacity(size() + 1));

        ptrdiff_t end_index = end() - begin();

        // Move construct first element, then move the rest
        if (!empty())
        {
            allocator_traits<Allocator>::construct(_alloc, _end, tempest::move(_data[end_index - 1]));
        }

        for (auto it = end(); it != begin() + index; --it)
        {
            *it = tempest::move(*(it - 1));
        }

        allocator_traits<Allocator>::construct(_alloc, _data + index, value);
        ++_end;

        return begin() + index;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::insert(const_iterator pos, T&& value) -> vector<T, Allocator>::iterator
    {
        auto index = pos - begin();
        reserve(_compute_next_capacity(size() + 1));

        ptrdiff_t end_index = size();
        ptrdiff_t start_index = index;

        // Move construct first element, then move the rest
        if (!empty())
        {
            allocator_traits<Allocator>::construct(_alloc, _end, tempest::move(_data[end_index - 1]));
        }

        for (auto idx = end_index - 1; idx > start_index; --idx)
        {
            _data[idx] = tempest::move(_data[idx - 1]);
        }

        allocator_traits<Allocator>::construct(_alloc, _data + index, tempest::move(value));
        ++_end;

        return begin() + index;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::insert(const_iterator pos, size_type count,
                                                                                   const T& value) -> vector<T, Allocator>::iterator
        requires is_copy_constructible_v<T>
    {
        auto index = pos - begin();
        reserve(_compute_next_capacity(size() + count));

        // Move construct the first count elements, then move the rest
        auto count_remaining = size() - index;
        auto count_to_move = tempest::min(count, count_remaining);

        ptrdiff_t end_index = size();
        ptrdiff_t start_index = index;

        for (auto i = 0; i < count_to_move; ++i)
        {
            allocator_traits<Allocator>::construct(_alloc, _end + i, tempest::move(_data[index + i - 1]));
        }

        for (auto index = end_index - 1; index > start_index; --index)
        {
            _data[index] = tempest::move(_data[index - count]);
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
    constexpr auto vector<T, Allocator>::insert(const_iterator pos, InputIt first,
                                                                                   InputIt last) -> vector<T, Allocator>::iterator
    {
        const auto index = pos - begin();
        const auto count = static_cast<size_t>(distance(first, last));
        reserve(_compute_next_capacity(size() + count));

        // Move construct the first count elements, then move the rest
        size_t count_remaining = size() - index;
        const auto count_to_move = tempest::min(count, count_remaining);

        ptrdiff_t end_index = size();
        ptrdiff_t start_index = index;

        for (auto i = 0U; i < count_to_move; ++i)
        {
            allocator_traits<Allocator>::construct(_alloc, _end + i, tempest::move(_data[index + i - 1]));
        }

        for (auto index = end_index - 1; index > start_index; --index)
        {
            _data[index] = tempest::move(_data[index - count]);
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
    constexpr auto vector<T, Allocator>::emplace(const_iterator pos, Args&&... args) -> vector<T, Allocator>::iterator
    {
        auto index = pos - begin();
        reserve(_compute_next_capacity(size() + 1));

        for (auto it = end(); it != begin() + index; --it)
        {
            *it = tempest::move(*(it - 1));
        }

        allocator_traits<Allocator>::construct(_alloc, _data + index, tempest::forward<Args>(args)...);
        ++_end;

        return begin() + index;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::erase(const_iterator pos) -> vector<T, Allocator>::iterator
    {
        auto index = pos - begin();

        for (auto it = begin() + index; it != end() - 1; ++it)
        {
            *it = tempest::move(*(it + 1));
        }

        --_end;

        // Destroy the last element
        allocator_traits<Allocator>::destroy(_alloc, _end);

        return begin() + index;
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::erase(const_iterator first,
                                                                                  const_iterator last) -> vector<T, Allocator>::iterator
    {
        auto index = first - begin();
        auto count = last - first;

        for (auto it = begin() + index; it != end() - count; ++it)
        {
            *it = tempest::move(*(it + count));
        }

        for (auto it = end() - count; it != end(); ++it)
        {
            allocator_traits<Allocator>::destroy(_alloc, it);
        }

        _end -= count;

        return begin() + index;
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::push_back(const T& value)
        requires is_copy_constructible_v<T>
    {
        _emplace_one_at_back(value);
    }

    template <typename T, typename Allocator>
    constexpr void vector<T, Allocator>::push_back(T&& value)
    {
        _emplace_one_at_back(tempest::move(value));
    }

    template <typename T, typename Allocator>
    template <typename... Args>
    constexpr auto vector<T, Allocator>::emplace_back(Args&&... args) -> vector<T, Allocator>::reference
    {
        _emplace_one_at_back(tempest::forward<Args>(args)...);
        return back();
    }

    template <typename T, typename Allocator>
    template <typename U>
    inline auto tempest::vector<T, Allocator>::reinterpret_as() noexcept -> vector<U>
    {
        if constexpr (tempest::is_same_v<T, U>)
        {
            return vector<U>{tempest::move(*this)};
        }

        U* new_data = reinterpret_cast<U*>(_data);
        U* new_end = reinterpret_cast<U*>(_end);

        // Remove ownership of the data from the original vector
        _data = nullptr;
        _end = nullptr;
        _capacity_end = nullptr;

        return vector<U>(new_data, new_end);
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
                allocator_traits<Allocator>::construct(_alloc, _end++);
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
            tempest::swap(_data, other._data);
            tempest::swap(_end, other._end);
            tempest::swap(_capacity_end, other._capacity_end);
        }
        else
        {
            vector tmp{tempest::move(*this)};
            *this = tempest::move(other);
            other = tempest::move(tmp);
        }
    }

    template <typename T, typename Allocator>
    constexpr auto vector<T, Allocator>::_compute_next_capacity(
        size_type requested_capacity) const noexcept -> vector<T, Allocator>::size_type
    {
        return tempest::bit_ceil(requested_capacity);
    }

    template <typename T, typename Allocator>
    template <typename... Args>
    constexpr void vector<T, Allocator>::_emplace_one_at_back(Args&&... args)
    {
        reserve(_compute_next_capacity(size() + 1));
        (void)tempest::construct_at(_end++, tempest::forward<Args>(args)...);
    }

    template <typename T, typename Allocator>
    constexpr auto operator==(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs) -> bool
    {
        return tempest::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    template <typename T, typename Allocator>
    constexpr auto operator<=>(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs)
    {
        return tempest::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    template <typename T, typename Allocator>
    constexpr void swap(vector<T, Allocator>& lhs, vector<T, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    template <typename T, typename Alloc, typename U>
    constexpr auto erase(vector<T, Alloc>& c, const U& value) -> vector<T, Alloc>::size_type
    {
        auto it = tempest::remove(c.begin(), c.end(), value);
        auto count = tempest::distance(it, c.end());
        c.erase(it, c.end());
        return count;
    }

    template <typename T, typename Alloc, typename Pred>
    constexpr auto erase_if(vector<T, Alloc>& c, Pred pred) -> vector<T, Alloc>::size_type
    {
        auto it = tempest::remove_if(c.begin(), c.end(), pred);
        auto count = tempest::distance(it, c.end());
        c.erase(it, c.end());
        return count;
    }

    template <typename T, typename Alloc>
    constexpr auto size(const vector<T, Alloc>& c) noexcept -> vector<T, Alloc>::size_type
    {
        return c.size();
    }

    template <typename T, typename Alloc>
    constexpr auto data(vector<T, Alloc>& c) noexcept -> vector<T, Alloc>::pointer
    {
        return c.data();
    }

    template <typename T, typename Alloc>
    constexpr auto data(const vector<T, Alloc>& c) noexcept -> vector<T, Alloc>::const_pointer
    {
        return c.data();
    }

    template <typename T, typename Alloc>
    constexpr auto begin(vector<T, Alloc>& c) noexcept -> vector<T, Alloc>::iterator
    {
        return c.begin();
    }

    template <typename T, typename Alloc>
    constexpr auto begin(const vector<T, Alloc>& c) noexcept -> vector<T, Alloc>::const_iterator
    {
        return c.begin();
    }

    template <typename T, typename Alloc>
    constexpr auto cbegin(const vector<T, Alloc>& c) noexcept -> vector<T, Alloc>::const_iterator
    {
        return c.cbegin();
    }

    template <typename T, typename Alloc>
    constexpr auto end(vector<T, Alloc>& c) noexcept -> vector<T, Alloc>::iterator
    {
        return c.end();
    }

    template <typename T, typename Alloc>
    constexpr auto end(const vector<T, Alloc>& c) noexcept -> vector<T, Alloc>::const_iterator
    {
        return c.end();
    }

    template <typename T, typename Alloc>
    constexpr auto cend(const vector<T, Alloc>& c) noexcept -> vector<T, Alloc>::const_iterator
    {
        return c.cend();
    }

    template <typename T, typename Alloc>
    constexpr auto empty(const vector<T, Alloc>& c) noexcept -> bool
    {
        return c.empty();
    }

    namespace unsafe
    {
        template <typename T, typename Allocator>
        void resize_no_init(vector<T, Allocator>& vec, size_t count)
        {
            vec.reserve(count);
            vec._end = vec._data + count;
        }
    } // namespace unsafe

    // Deduction guides
    template <typename... Ts>
    vector(init_list_t, Ts&&...) -> vector<common_type_t<Ts...>>;
} // namespace tempest

#endif // tempest_core_vector_hpp