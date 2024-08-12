#ifndef tempest_core_array_hpp
#define tempest_core_array_hpp

#include <tempest/int.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    struct array
    {
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = T*;
        using const_iterator = const T*;

        [[nodiscard]] constexpr reference at(size_type pos) noexcept;
        [[nodiscard]] constexpr const_reference at(size_type pos) const noexcept;
        [[nodiscard]] constexpr reference operator[](size_type pos) noexcept;
        [[nodiscard]] constexpr const_reference operator[](size_type pos) const noexcept;

        [[nodiscard]] constexpr reference front() noexcept;
        [[nodiscard]] constexpr const_reference front() const noexcept;
        [[nodiscard]] constexpr reference back() noexcept;
        [[nodiscard]] constexpr const_reference back() const noexcept;

        [[nodiscard]] constexpr T* data() noexcept;
        [[nodiscard]] constexpr const T* data() const noexcept;

        [[nodiscard]] constexpr iterator begin() noexcept;
        [[nodiscard]] constexpr const_iterator begin() const noexcept;
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept;

        [[nodiscard]] constexpr iterator end() noexcept;
        [[nodiscard]] constexpr const_iterator end() const noexcept;
        [[nodiscard]] constexpr const_iterator cend() const noexcept;

        [[nodiscard]] constexpr bool empty() const noexcept;
        [[nodiscard]] constexpr size_type size() const noexcept;
        [[nodiscard]] constexpr size_type max_size() const noexcept;

        void fill(const T& value);
        void swap(array& other) noexcept;

        // Undefined to access directly.
        T _data[N > 0 ? N : 1];
    };

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::reference array<T, N>::at(size_type pos) noexcept
    {
        return _data[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_reference array<T, N>::at(size_type pos) const noexcept
    {
        return _data[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::reference array<T, N>::operator[](size_type pos) noexcept
    {
        return _data[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_reference array<T, N>::operator[](size_type pos) const noexcept
    {
        return _data[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::reference array<T, N>::front() noexcept
    {
        return _data[0];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_reference array<T, N>::front() const noexcept
    {
        return _data[0];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::reference array<T, N>::back() noexcept
    {
        return _data[N - 1];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_reference array<T, N>::back() const noexcept
    {
        return _data[N - 1];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr T* array<T, N>::data() noexcept
    {
        return _data;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr const T* array<T, N>::data() const noexcept
    {
        return _data;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::iterator array<T, N>::begin() noexcept
    {
        return _data;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_iterator array<T, N>::begin() const noexcept
    {
        return _data;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_iterator array<T, N>::cbegin() const noexcept
    {
        return _data;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::iterator array<T, N>::end() noexcept
    {
        return _data + N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_iterator array<T, N>::end() const noexcept
    {
        return _data + N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_iterator array<T, N>::cend() const noexcept
    {
        return _data + N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr bool array<T, N>::empty() const noexcept
    {
        return N == 0;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::size_type array<T, N>::size() const noexcept
    {
        return N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::size_type array<T, N>::max_size() const noexcept
    {
        return N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline void array<T, N>::fill(const T& value)
    {
        for (size_type i = 0; i < N; ++i)
        {
            _data[i] = value;
        }
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline void array<T, N>::swap(array& other) noexcept
    {
        for (size_type i = 0; i < N; ++i)
        {
            tempest::swap(_data[i], other._data[i]);
        }
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    [[nodiscard]] inline constexpr bool operator==(const array<T, N>& lhs, const array<T, N>& rhs)
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (lhs[i] != rhs[i])
            {
                return false;
            }
        }
        return true;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    [[nodiscard]] inline constexpr bool operator!=(const array<T, N>& lhs, const array<T, N>& rhs)
    {
        return !(lhs == rhs);
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    [[nodiscard]] inline constexpr bool operator<(const array<T, N>& lhs, const array<T, N>& rhs)
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (lhs[i] < rhs[i])
            {
                return true;
            }
            if (rhs[i] < lhs[i])
            {
                return false;
            }
        }
        return false;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    [[nodiscard]] inline constexpr bool operator<=(const array<T, N>& lhs, const array<T, N>& rhs)
    {
        return !(rhs < lhs);
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    [[nodiscard]] inline constexpr bool operator>(const array<T, N>& lhs, const array<T, N>& rhs)
    {
        return rhs < lhs;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    [[nodiscard]] inline constexpr bool operator>=(const array<T, N>& lhs, const array<T, N>& rhs)
    {
        return !(lhs < rhs);
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline void swap(array<T, N>& lhs, array<T, N>& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    template <typename T, typename... U>
    array(T, U...) -> array<T, 1 + sizeof...(U)>;

    template <typename T, size_t N>
        requires is_constructible_v<T, T&> && (!is_array_v<T>)
    inline constexpr array<remove_cv_t<T>, N> to_array(T (&arr)[N])
    {
        array<T, N> result;
        for (size_t i = 0; i < N; ++i)
        {
            result[i] = arr[i];
        }
        return result;
    }

    template <typename T, size_t N>
        requires is_move_constructible_v<T> && (!is_array_v<T>)
    inline constexpr array<remove_cv_t<T>, N> to_array(T (&&arr)[N])
    {
        array<T, N> result;
        for (size_t i = 0; i < N; ++i)
        {
            result[i] = tempest::move(arr[i]);
        }
        return result;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline T* begin(array<T, N>& arr) noexcept
    {
        return arr.begin();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline const T* begin(const array<T, N>& arr) noexcept
    {
        return arr.begin();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline const T* cbegin(const array<T, N>& arr) noexcept
    {
        return arr.cbegin();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline T* end(array<T, N>& arr) noexcept
    {
        return arr.end();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline const T* end(const array<T, N>& arr) noexcept
    {
        return arr.end();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline const T* cend(const array<T, N>& arr) noexcept
    {
        return arr.cend();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline T* data(array<T, N>& arr) noexcept
    {
        return arr.data();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline const T* data(const array<T, N>& arr) noexcept
    {
        return arr.data();
    }

    template <typename T, size_t N>
    inline bool empty(const array<T, N>& arr) noexcept
    {
        return arr.empty();
    }

    template <typename T, size_t N>
    inline size_t size(const array<T, N>& arr) noexcept
    {
        return arr.size();
    }

    template <typename T, size_t N>
    inline size_t max_size(const array<T, N>& arr) noexcept
    {
        return arr.max_size();
    }
} // namespace tempest

#endif // tempest_core_array_hpp