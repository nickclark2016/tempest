#ifndef tempest_inplace_vector_hpp
#define tempest_inplace_vector_hpp

#include <tempest/algorithm.hpp>
#include <tempest/int.hpp>
#include <tempest/iterator.hpp>
#include <tempest/memory.hpp>

namespace tempest
{
    template <typename T, size_t N>
    class inplace_vector
    {
      public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = pointer;
        using const_iterator = const_pointer;

        constexpr inplace_vector() noexcept = default;
        constexpr explicit inplace_vector(size_t count);
        constexpr inplace_vector(size_t count, const T& value);

        template <input_iterator InputIt>
        constexpr inplace_vector(InputIt first, InputIt last);

        constexpr inplace_vector(const inplace_vector& other);
        constexpr inplace_vector(inplace_vector&& other) noexcept;

        constexpr ~inplace_vector();

        constexpr inplace_vector& operator=(const inplace_vector& other);
        constexpr inplace_vector& operator=(inplace_vector&& other) noexcept;

        constexpr void assign(size_t count, const T& value);

        template <input_iterator InputIt>
        constexpr void assign(InputIt first, InputIt last);

        constexpr reference at(size_t pos);
        constexpr const_reference at(size_t pos) const;

        constexpr reference operator[](size_t pos);
        constexpr const_reference operator[](size_t pos) const;

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

        constexpr bool empty() const noexcept;
        constexpr size_t size() const noexcept;
        constexpr size_t capacity() const noexcept;
        constexpr size_t max_size() const noexcept;

        constexpr void resize(size_t count);
        constexpr void resize(size_t count, const T& value);

        constexpr iterator insert(const_iterator pos, const T& value);
        constexpr iterator insert(const_iterator pos, T&& value);
        constexpr iterator insert(const_iterator pos, size_t count, const T& value);

        template <input_iterator InputIt>
        constexpr iterator insert(const_iterator pos, InputIt first, InputIt last);

        template <typename... Args>
        constexpr iterator emplace(const_iterator pos, Args&&... args);

        template <typename... Args>
        constexpr void emplace_back(Args&&... args);

        template <typename... Args>
        constexpr bool try_emplace_back(Args&&... args);

        constexpr void push_back(const T& value);
        constexpr void push_back(T&& value);

        constexpr bool try_push_back(const T& value);
        constexpr bool try_push_back(T&& value);

        constexpr void pop_back();

        constexpr void clear() noexcept;

        constexpr iterator erase(const_iterator pos);
        constexpr iterator erase(const_iterator first, const_iterator last);

        constexpr void swap(inplace_vector& other) noexcept;

      private:
        alignas(T) byte _data[sizeof(T[N])] = {byte{}};
        T* _typed_data{nullptr};
        size_t _size{0};
    };

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::inplace_vector(size_t count) : _size{count}
    {
        if (count == 0) [[unlikely]]
        {
            return;
        }

        _typed_data = reinterpret_cast<T*>(_data);
        for (size_t idx = 0; idx < count; ++idx)
        {
            (void)tempest::construct_at(_typed_data + idx);
        }
    }

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::inplace_vector(size_t count, const T& value) : _size{count}
    {
        if (count == 0) [[unlikely]]
        {
            return;
        }

        _typed_data = reinterpret_cast<T*>(_data);
        for (size_t idx = 0; idx < count; ++idx)
        {
            (void)tempest::construct_at(_typed_data + idx, value);
        }
    }

    template <typename T, size_t N>
    template <input_iterator InputIt>
    constexpr inplace_vector<T, N>::inplace_vector(InputIt first, InputIt last)
    {
        auto is_range_empty = first == last;
        if (is_range_empty) [[unlikely]]
        {
            return;
        }

        _typed_data = reinterpret_cast<T*>(_data);

        while (first != last)
        {
            (void)tempest::construct_at(_typed_data + _size, *first);
            ++first;
            ++_size;
        }
    }

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::inplace_vector(const inplace_vector& other) : _size{other._size}
    {
        if (other.empty()) [[unlikely]]
        {
            return;
        }

        _typed_data = reinterpret_cast<T*>(_data);

        for (size_t idx = 0; idx < _size; ++idx)
        {
            (void)tempest::construct_at(_typed_data + idx, other._typed_data[idx]);
        }
    }

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::inplace_vector(inplace_vector&& other) noexcept : _size{other._size}
    {
        if (other.empty()) [[unlikely]]
        {
            return;
        }

        _typed_data = reinterpret_cast<T*>(_data);

        for (size_t idx = 0; idx < _size; ++idx)
        {
            (void)tempest::construct_at(_typed_data + idx, tempest::move(other._typed_data[idx]));
        }
        other.clear();
    }

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::~inplace_vector()
    {    
        for (size_t idx = 0; idx < _size; ++idx)
        {
            tempest::destroy_at(_typed_data + idx);
        }
    }

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>& inplace_vector<T, N>::operator=(const inplace_vector& other)
    {
        if (this == &other) [[unlikely]]
        {
            return *this;
        }

        clear();

        _size = other._size;
        if (other.empty())
        {
            _typed_data = nullptr;
            return *this;
        }

        _typed_data = reinterpret_cast<T*>(_data);
        for (size_t idx = 0; idx < _size; ++idx)
        {
            (void)tempest::construct_at(_typed_data + idx, other._typed_data[idx]);
        }

        return *this;
    }

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>& inplace_vector<T, N>::operator=(inplace_vector&& other) noexcept
    {
        if (this == &other) [[unlikely]]
        {
            return *this;
        }
        
        clear();
        
        _size = other._size;
        if (other.empty())
        {
            return *this;
        }
        
        _typed_data = reinterpret_cast<T*>(_data);
        for (size_t idx = 0; idx < _size; ++idx)
        {
            (void)tempest::construct_at(_typed_data + idx, tempest::move(other._typed_data[idx]));
            tempest::destroy_at(other._typed_data + idx);
        }

        other._typed_data = nullptr;
        other._size = 0;

        return *this;
    }

    template <typename T, size_t N>
    constexpr void inplace_vector<T, N>::assign(size_t count, const T& value)
    {
        clear();

        _size = count;

        if (count == 0) [[unlikely]]
        {
            return;
        }

        _typed_data = reinterpret_cast<T*>(_data);
        for (size_t idx = 0; idx < count; ++idx)
        {
            (void)tempest::construct_at(_typed_data + idx, value);
        }
    }

    template <typename T, size_t N>
    template <input_iterator InputIt>
    constexpr void inplace_vector<T, N>::assign(InputIt first, InputIt last)
    {
        clear();
     
        auto is_range_empty = first == last;
        if (is_range_empty) [[unlikely]]
        {
            return;
        }
     
        _typed_data = reinterpret_cast<T*>(_data);
        while (first != last)
        {
            (void)tempest::construct_at(_typed_data + _size, *first);
            ++first;
            ++_size;
        }
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::reference inplace_vector<T, N>::at(size_t pos)
    {
        return _typed_data[pos];
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::const_reference inplace_vector<T, N>::at(size_t pos) const
    {
        return _typed_data[pos];
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::reference inplace_vector<T, N>::operator[](size_t pos)
    {
        return _typed_data[pos];
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::const_reference inplace_vector<T, N>::operator[](size_t pos) const
    {
        return _typed_data[pos];
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::reference inplace_vector<T, N>::front()
    {
        return _typed_data[0];
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::const_reference inplace_vector<T, N>::front() const
    {
        return _typed_data[0];
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::reference inplace_vector<T, N>::back()
    {
        return _typed_data[_size - 1];
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::const_reference inplace_vector<T, N>::back() const
    {
        return _typed_data[_size - 1];
    }

    template <typename T, size_t N>
    constexpr T* inplace_vector<T, N>::data() noexcept
    {
        return _typed_data;
    }

    template <typename T, size_t N>
    constexpr const T* inplace_vector<T, N>::data() const noexcept
    {
        return _typed_data;
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::iterator inplace_vector<T, N>::begin() noexcept
    {
        return _typed_data;
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::const_iterator inplace_vector<T, N>::begin() const noexcept
    {
        return _typed_data;
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::const_iterator inplace_vector<T, N>::cbegin() const noexcept
    {
        return _typed_data;
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::iterator inplace_vector<T, N>::end() noexcept
    {
        return _typed_data + _size;
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::const_iterator inplace_vector<T, N>::end() const noexcept
    {
        return _typed_data + _size;
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::const_iterator inplace_vector<T, N>::cend() const noexcept
    {
        return _typed_data + _size;
    }

    template <typename T, size_t N>
    constexpr bool inplace_vector<T, N>::empty() const noexcept
    {
        return _size == 0;
    }

    template <typename T, size_t N>
    constexpr size_t inplace_vector<T, N>::size() const noexcept
    {
        return _size;
    }

    template <typename T, size_t N>
    constexpr size_t inplace_vector<T, N>::capacity() const noexcept
    {
        return N;
    }

    template <typename T, size_t N>
    constexpr size_t inplace_vector<T, N>::max_size() const noexcept
    {
        return N;
    }

    template <typename T, size_t N>
    constexpr void inplace_vector<T, N>::resize(size_t count)
    {
        resize(count, T{});
    }

    template <typename T, size_t N>
    constexpr void inplace_vector<T, N>::resize(size_t count, const T& value)
    {
        if (count == _size) [[unlikely]]
        {
            return;
        }

        _typed_data = reinterpret_cast<T*>(_data);

        if (count < _size)
        {
            for (size_t idx = count; idx < _size; ++idx)
            {
                tempest::destroy_at(_typed_data + idx);
            }
        }
        else
        {
            if (count > N)
            {
                count = N;
            }
            for (size_t idx = _size; idx < count; ++idx)
            {
                (void)tempest::construct_at(_typed_data + idx, value);
            }
        }
        _size = count;
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::iterator inplace_vector<T, N>::insert(const_iterator pos, const T& value)
    {
        return emplace(pos, value);
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::iterator inplace_vector<T, N>::insert(const_iterator pos, T&& value)
    {
        return emplace(pos, tempest::move(value));
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::iterator inplace_vector<T, N>::insert(const_iterator pos, size_t count,
                                                                                   const T& value)
    {
        if (count == 0) [[unlikely]]
        {
            return _typed_data + (pos - _typed_data);
        }

        if (_size + count > N)
        {
            return end();
        }

        _typed_data = reinterpret_cast<T*>(_data);

        auto insert_pos = pos - _typed_data;
        auto insert_end = insert_pos + count;
        auto shift_pos = _size - 1;
        auto shift_end = insert_pos - 1;

        // Move assign elements to the right of the insertion point
        // Move construct new elements at indices where no such elements exist
        // Move assign elements to indices where existing elements where

        for (size_t idx = shift_pos; idx >= insert_pos; --idx)
        {
            if (idx >= _size)
            {
                (void)tempest::construct_at(_typed_data + idx);
            }
            else
            {
                (void)tempest::construct_at(_typed_data + idx + count, tempest::move(_typed_data[idx]));
                tempest::destroy_at(_typed_data + idx);
            }
        }

        for (size_t idx = insert_pos; idx < insert_end; ++idx)
        {
            (void)tempest::construct_at(_typed_data + idx, value);
        }

        _size += count;

        return _typed_data + insert_pos;
    }

    template <typename T, size_t N>
    template <input_iterator InputIt>
    constexpr typename inplace_vector<T, N>::iterator inplace_vector<T, N>::insert(const_iterator pos, InputIt first,
                                                                                   InputIt last)
    {
        auto is_range_empty = first == last;
        if (is_range_empty) [[unlikely]]
        {
            return _typed_data + (pos - _typed_data);
        }

        auto count = tempest::distance(first, last);

        if (_size + count > N)
        {
            return end();
        }

        _typed_data = reinterpret_cast<T*>(_data);

        auto insert_pos = static_cast<size_t>(pos - _typed_data);
        auto insert_end = insert_pos + count;
        auto shift_pos = _size - 1;

        // Move assign elements to the right of the insertion point
        // Move construct new elements at indices where no such elements exist
        // Move assign elements to indices where existing elements where
        for (size_t idx = shift_pos; idx >= insert_pos; --idx)
        {
            if (idx >= _size)
            {
                (void)tempest::construct_at(_typed_data + idx);
            }
            else
            {
                (void)tempest::construct_at(_typed_data + idx + count, tempest::move(_typed_data[idx]));
                tempest::destroy_at(_typed_data + idx);
            }
        }
        
        for (size_t idx = insert_pos; idx < insert_end; ++idx)
        {
            (void)tempest::construct_at(_typed_data + idx, *first);
            ++first;
            ++_size;
        }

        return _typed_data + insert_pos;
    }

    template <typename T, size_t N>
    template <typename... Args>
    constexpr typename inplace_vector<T, N>::iterator inplace_vector<T, N>::emplace(const_iterator pos, Args&&... args)
    {
        if (_size == N) [[unlikely]]
        {
            return end();
        }

        _typed_data = reinterpret_cast<T*>(_data);
        
        auto insert_pos = static_cast<size_t>(pos - _typed_data);
        auto shift_pos = _size - 1;

        for (size_t idx = shift_pos; idx >= insert_pos; --idx)
        {
            if (idx >= _size)
            {
                (void)tempest::construct_at(_typed_data + idx);
            }
            else
            {
                (void)tempest::construct_at(_typed_data + idx + 1, tempest::move(_typed_data[idx]));
                tempest::destroy_at(_typed_data + idx);
            }
        }
        (void)tempest::construct_at(_typed_data + insert_pos, tempest::forward<Args>(args)...);
        ++_size;
        return _typed_data + insert_pos;
    }

    template <typename T, size_t N>
    template <typename... Args>
    constexpr void inplace_vector<T, N>::emplace_back(Args&&... args)
    {
        if (_size == N) [[unlikely]]
        {
            return;
        }

        _typed_data = reinterpret_cast<T*>(_data);

        (void)tempest::construct_at(_typed_data + _size, tempest::forward<Args>(args)...);
        ++_size;
    }

    template <typename T, size_t N>
    template <typename... Args>
    constexpr bool inplace_vector<T, N>::try_emplace_back(Args&&... args)
    {
        if (_size == N) [[unlikely]]
        {
            return false;
        }
        (void)tempest::construct_at(_typed_data + _size, tempest::forward<Args>(args)...);
        ++_size;
        return true;
    }

    template <typename T, size_t N>
    constexpr void inplace_vector<T, N>::push_back(const T& value)
    {
        if (_size == N) [[unlikely]]
        {
            return;
        }

        _typed_data = reinterpret_cast<T*>(_data);

        (void)tempest::construct_at(_typed_data + _size, value);
        ++_size;
    }

    template <typename T, size_t N>
    constexpr void inplace_vector<T, N>::push_back(T&& value)
    {
        if (_size == N) [[unlikely]]
        {
            return;
        }

        _typed_data = reinterpret_cast<T*>(_data);

        (void)tempest::construct_at(_typed_data + _size, tempest::move(value));
        ++_size;
    }

    template <typename T, size_t N>
    constexpr bool inplace_vector<T, N>::try_push_back(const T& value)
    {
        if (_size == N) [[unlikely]]
        {
            return false;
        }

        _typed_data = reinterpret_cast<T*>(_data);

        (void)tempest::construct_at(_typed_data + _size, value);
        ++_size;
        return true;
    }

    template <typename T, size_t N>
    constexpr bool inplace_vector<T, N>::try_push_back(T&& value)
    {
        if (_size == N) [[unlikely]]
        {
            return false;
        }

        _typed_data = reinterpret_cast<T*>(_data);

        (void)tempest::construct_at(_typed_data + _size, tempest::move(value));
        ++_size;
        return true;
    }

    template <typename T, size_t N>
    constexpr void inplace_vector<T, N>::pop_back()
    {
        if (_size == 0) [[unlikely]]
        {
            return;
        }
        tempest::destroy_at(_typed_data + _size - 1);
        --_size;
    }

    template <typename T, size_t N>
    constexpr void inplace_vector<T, N>::clear() noexcept
    {
        for (size_t idx = 0; idx < _size; ++idx)
        {
            tempest::destroy_at(_typed_data + idx);
        }
        _size = 0;
        _typed_data = nullptr;
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::iterator inplace_vector<T, N>::erase(const_iterator pos)
    {
        auto erase_pos = pos - _typed_data;
        tempest::destroy_at(_typed_data + erase_pos);
        for (size_t idx = erase_pos; idx < _size - 1; ++idx)
        {
            (void)tempest::construct_at(_typed_data + idx, tempest::move(_typed_data[idx + 1]));
            tempest::destroy_at(_typed_data + idx + 1);
        }
        --_size;

        if (_size == 0)
        {
            _typed_data = nullptr;
        }

        return _typed_data + erase_pos;
    }

    template <typename T, size_t N>
    constexpr typename inplace_vector<T, N>::iterator inplace_vector<T, N>::erase(const_iterator first,
                                                                                  const_iterator last)
    {
        auto erase_pos = first - _typed_data;
        auto erase_end = last - _typed_data;
        auto erase_count = erase_end - erase_pos;
        
        // Move assign elements to the removed range
        // Destroy the last erase_count elements

        for (size_t idx = erase_pos; idx < _size - erase_count; ++idx)
        {
            (void)tempest::construct_at(_typed_data + idx, tempest::move(_typed_data[idx + erase_count]));
            tempest::destroy_at(_typed_data + idx + erase_count);
        }

        for (size_t idx = _size - erase_count; idx < _size; ++idx)
        {
            tempest::destroy_at(_typed_data + idx);
        }
        
        _size -= erase_count;

        if (_size == 0)
        {
            _typed_data = nullptr;
        }

        return _typed_data + erase_pos;
    }

    template <typename T, size_t N>
    constexpr void inplace_vector<T, N>::swap(inplace_vector& other) noexcept
    {
        using tempest::swap;

        for (size_t idx = 0; idx < N; ++idx)
        {
            // If idx is less than the size of both vectors, swap the elements
            // If idx is less than the size of one vector, move the element to the other vector and destroy the element
            // If idx is greater than the size of both vectors, exit the loop
            if (idx < _size && idx < other._size)
            {
                swap(_typed_data[idx], other._typed_data[idx]);
            }
            else if (idx < _size)
            {
                (void)tempest::construct_at(other._typed_data + idx, tempest::move(_typed_data[idx]));
                tempest::destroy_at(_typed_data + idx);
            }
            else if (idx < other._size)
            {
                (void)tempest::construct_at(_typed_data + idx, tempest::move(other._typed_data[idx]));
                tempest::destroy_at(other._typed_data + idx);
            }
            else
            {
                break;
            }
        }

        // Swap the sizes
        swap(_size, other._size);

        // Reset the typed data pointers
        if (_size == 0)
        {
            _typed_data = nullptr;
        }
        else
        {
            _typed_data = reinterpret_cast<T*>(_data);
        }

        if (other._size == 0)
        {
            other._typed_data = nullptr;
        }
        else
        {
            other._typed_data = reinterpret_cast<T*>(other._data);
        }
    }

    template <typename T, size_t N>
    constexpr bool operator==(const inplace_vector<T, N>& lhs, const inplace_vector<T, N>& rhs)
    {
        if (lhs.size() != rhs.size())
        {
            return false;
        }
        for (size_t idx = 0; idx < lhs.size(); ++idx)
        {
            if (lhs[idx] != rhs[idx])
            {
                return false;
            }
        }
        return true;
    }

    template <typename T, size_t N>
    constexpr bool operator!=(const inplace_vector<T, N>& lhs, const inplace_vector<T, N>& rhs)
    {
        return !(lhs == rhs);
    }

    template <typename T, size_t N>
    constexpr void swap(inplace_vector<T, N>& lhs, inplace_vector<T, N>& rhs) noexcept
    {
        lhs.swap(rhs);
    }
} // namespace tempest

#endif // tempest_inplace_vector_hpp