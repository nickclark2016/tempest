#ifndef tempest_core_array_hpp
#define tempest_core_array_hpp

#include <tempest/api.hpp>
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

        [[nodiscard]] constexpr auto at(size_type pos) noexcept -> reference;
        [[nodiscard]] constexpr auto at(size_type pos) const noexcept -> const_reference;
        [[nodiscard]] constexpr auto operator[](size_type pos) noexcept -> reference;
        [[nodiscard]] constexpr auto operator[](size_type pos) const noexcept -> const_reference;

        [[nodiscard]] constexpr auto front() noexcept -> reference;
        [[nodiscard]] constexpr auto front() const noexcept -> const_reference;
        [[nodiscard]] constexpr auto back() noexcept -> reference;
        [[nodiscard]] constexpr auto back() const noexcept -> const_reference;

        [[nodiscard]] constexpr auto data() noexcept -> T*;
        [[nodiscard]] constexpr auto data() const noexcept -> const T*;

        [[nodiscard]] constexpr auto begin() noexcept -> iterator;
        [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator;
        [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator;

        [[nodiscard]] constexpr auto end() noexcept -> iterator;
        [[nodiscard]] constexpr auto end() const noexcept -> const_iterator;
        [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator;

        [[nodiscard]] constexpr auto empty() const noexcept -> bool;
        [[nodiscard]] constexpr auto size() const noexcept -> size_type;
        [[nodiscard]] constexpr auto max_size() const noexcept -> size_type;

        constexpr void fill(const T& value);
        constexpr void swap(array& other) noexcept;

        // Undefined to access directly.
        T _data[N];
    };

    template <typename T>
        requires is_default_constructible_v<T>
    struct array<T, 0>
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

        [[nodiscard]] constexpr auto data() noexcept -> T*;
        [[nodiscard]] constexpr auto data() const noexcept -> const T*;

        [[nodiscard]] constexpr auto begin() noexcept -> iterator;
        [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator;
        [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator;

        [[nodiscard]] constexpr auto end() noexcept -> iterator;
        [[nodiscard]] constexpr auto end() const noexcept -> const_iterator;
        [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator;

        [[nodiscard]] constexpr auto empty() const noexcept -> bool;
        [[nodiscard]] constexpr auto size() const noexcept -> size_type;
        [[nodiscard]] constexpr auto max_size() const noexcept -> size_type;

        constexpr void fill(const T& value);
        constexpr void swap(array& other) noexcept;
    };

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::at(size_type pos) noexcept -> array<T, N>::reference
    {
        return data()[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::at(size_type pos) const noexcept -> array<T, N>::const_reference
    {
        return data()[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::operator[](size_type pos) noexcept -> array<T, N>::reference
    {
        return data()[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::operator[](size_type pos) const noexcept -> array<T, N>::const_reference
    {
        return data()[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::front() noexcept -> array<T, N>::reference
    {
        return data()[0];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::front() const noexcept -> array<T, N>::const_reference
    {
        return data()[0];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::back() noexcept -> array<T, N>::reference
    {
        return data()[N - 1];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::back() const noexcept -> array<T, N>::const_reference
    {
        return data()[N - 1];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::data() noexcept -> T*
    {
        return _data;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::data() const noexcept -> const T*
    {
        return _data;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::begin() noexcept -> array<T, N>::iterator
    {
        return data();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::begin() const noexcept -> array<T, N>::const_iterator
    {
        return data();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::cbegin() const noexcept -> array<T, N>::const_iterator
    {
        return data();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::end() noexcept -> array<T, N>::iterator
    {
        return data() + N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::end() const noexcept -> array<T, N>::const_iterator
    {
        return data() + N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::cend() const noexcept -> array<T, N>::const_iterator
    {
        return data() + N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::empty() const noexcept -> bool
    {
        return N == 0;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::size() const noexcept -> array<T, N>::size_type
    {
        return N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto array<T, N>::max_size() const noexcept -> array<T, N>::size_type
    {
        return N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr void array<T, N>::fill(const T& value)
    {
        for (size_type i = 0; i < N; ++i)
        {
            _data[i] = value;
        }
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr void array<T, N>::swap(array& other) noexcept
    {
        for (size_type i = 0; i < N; ++i)
        {
            tempest::swap(_data[i], other._data[i]);
        }
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr auto array<T, 0>::data() noexcept -> T*
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr auto array<T, 0>::data() const noexcept -> const T*
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr auto array<T, 0>::begin() noexcept -> array<T, 0>::iterator
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr auto array<T, 0>::begin() const noexcept -> array<T, 0>::const_iterator
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr auto array<T, 0>::cbegin() const noexcept -> array<T, 0>::const_iterator
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr auto array<T, 0>::end() noexcept -> array<T, 0>::iterator
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr auto array<T, 0>::end() const noexcept -> array<T, 0>::const_iterator
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr auto array<T, 0>::cend() const noexcept -> array<T, 0>::const_iterator
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr auto array<T, 0>::empty() const noexcept -> bool
    {
        return true;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr auto array<T, 0>::size() const noexcept -> array<T, 0>::size_type
    {
        return 0;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr auto array<T, 0>::max_size() const noexcept -> array<T, 0>::size_type
    {
        return 0;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr void array<T, 0>::fill([[maybe_unused]] const T& value)
    {
    }

    template <typename T>
        requires is_default_constructible_v<T>
    constexpr void array<T, 0>::swap([[maybe_unused]] array& other) noexcept
    {
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    [[nodiscard]] constexpr auto operator==(const array<T, N>& lhs, const array<T, N>& rhs) -> bool
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
    [[nodiscard]] constexpr auto operator!=(const array<T, N>& lhs, const array<T, N>& rhs) -> bool
    {
        return !(lhs == rhs);
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    [[nodiscard]] constexpr auto operator<(const array<T, N>& lhs, const array<T, N>& rhs) -> bool
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
    [[nodiscard]] constexpr auto operator<=(const array<T, N>& lhs, const array<T, N>& rhs) -> bool
    {
        return !(rhs < lhs);
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    [[nodiscard]] constexpr auto operator>(const array<T, N>& lhs, const array<T, N>& rhs) -> bool
    {
        return rhs < lhs;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    [[nodiscard]] constexpr auto operator>=(const array<T, N>& lhs, const array<T, N>& rhs) -> bool
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
    constexpr auto to_array(T (&arr)[N]) -> array<remove_cv_t<T>, N>
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
    constexpr auto to_array(T (&&arr)[N]) -> array<remove_cv_t<T>, N>
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
    constexpr auto begin(array<T, N>& arr) noexcept -> T*
    {
        return arr.begin();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto begin(const array<T, N>& arr) noexcept -> const T*
    {
        return arr.begin();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto cbegin(const array<T, N>& arr) noexcept -> const T*
    {
        return arr.cbegin();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto end(array<T, N>& arr) noexcept -> T*
    {
        return arr.end();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto end(const array<T, N>& arr) noexcept -> const T*
    {
        return arr.end();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto cend(const array<T, N>& arr) noexcept -> const T*
    {
        return arr.cend();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto data(array<T, N>& arr) noexcept -> T*
    {
        return arr.data();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    constexpr auto data(const array<T, N>& arr) noexcept -> const T*
    {
        return arr.data();
    }

    template <typename T, size_t N>
    constexpr auto empty(const array<T, N>& arr) noexcept -> bool
    {
        return arr.empty();
    }

    template <typename T, size_t N>
    constexpr auto size(const array<T, N>& arr) noexcept -> decltype(arr.size())
    {
        return arr.size();
    }

    template <typename T, size_t N>
    constexpr auto ssize(const array<T, N>& arr) noexcept
    {
        return static_cast<ptrdiff_t>(arr.size());
    }

    template <typename T, size_t N>
    constexpr auto max_size(const array<T, N>& arr) noexcept -> size_t
    {
        return arr.max_size();
    }

    template <size_t I, typename T, size_t N>
    constexpr auto get(array<T, N>& arr) noexcept -> T&
    {
        static_assert(I < N);
        return arr[I];
    }

    template <size_t I, typename T, size_t N>
    constexpr auto get(const array<T, N>& arr) noexcept -> const T&
    {
        static_assert(I < N);
        return arr[I];
    }

    template <size_t I, typename T, size_t N>
    constexpr auto get(array<T, N>&& arr) noexcept -> T&&
    {
        static_assert(I < N);
        return tempest::move(arr[I]);
    }

    template <size_t I, typename T, size_t N>
    constexpr auto get(const array<T, N>&& arr) noexcept -> const T&&
    {
        static_assert(I < N);
        return tempest::move(arr[I]);
    }
} // namespace tempest

namespace std
{
    template <typename T>
    struct tuple_size;

    template <tempest::size_t I, typename T>
    struct tuple_element;

    template <typename T, tempest::size_t N>
    struct tuple_size<tempest::array<T, N>> : tempest::integral_constant<tempest::size_t, N>
    {
    };

    template <tempest::size_t I, typename T, tempest::size_t N>
    struct tuple_element<I, tempest::array<T, N>>
    {
        using type = T;
    };
} // namespace std

#endif // tempest_core_array_hpp