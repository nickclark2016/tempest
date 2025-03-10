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
    };

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::reference array<T, N>::at(size_type pos) noexcept
    {
        return data()[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_reference array<T, N>::at(size_type pos) const noexcept
    {
        return data()[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::reference array<T, N>::operator[](size_type pos) noexcept
    {
        return data()[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_reference array<T, N>::operator[](size_type pos) const noexcept
    {
        return data()[pos];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::reference array<T, N>::front() noexcept
    {
        return data()[0];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_reference array<T, N>::front() const noexcept
    {
        return data()[0];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::reference array<T, N>::back() noexcept
    {
        return data()[N - 1];
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_reference array<T, N>::back() const noexcept
    {
        return data()[N - 1];
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
        return data();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_iterator array<T, N>::begin() const noexcept
    {
        return data();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_iterator array<T, N>::cbegin() const noexcept
    {
        return data();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::iterator array<T, N>::end() noexcept
    {
        return data() + N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_iterator array<T, N>::end() const noexcept
    {
        return data() + N;
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, N>::const_iterator array<T, N>::cend() const noexcept
    {
        return data() + N;
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

    template <typename T>
        requires is_default_constructible_v<T>
    inline constexpr T* array<T, 0>::data() noexcept
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline constexpr const T* array<T, 0>::data() const noexcept
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, 0>::iterator array<T, 0>::begin() noexcept
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, 0>::const_iterator array<T, 0>::begin() const noexcept
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, 0>::const_iterator array<T, 0>::cbegin() const noexcept
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, 0>::iterator array<T, 0>::end() noexcept
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, 0>::const_iterator array<T, 0>::end() const noexcept
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, 0>::const_iterator array<T, 0>::cend() const noexcept
    {
        return nullptr;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline constexpr bool array<T, 0>::empty() const noexcept
    {
        return true;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, 0>::size_type array<T, 0>::size() const noexcept
    {
        return 0;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline constexpr typename array<T, 0>::size_type array<T, 0>::max_size() const noexcept
    {
        return 0;
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline void array<T, 0>::fill([[maybe_unused]] const T& value)
    {
    }

    template <typename T>
        requires is_default_constructible_v<T>
    inline void array<T, 0>::swap([[maybe_unused]] array& other) noexcept
    {
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
    inline constexpr T* begin(array<T, N>& arr) noexcept
    {
        return arr.begin();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr const T* begin(const array<T, N>& arr) noexcept
    {
        return arr.begin();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr const T* cbegin(const array<T, N>& arr) noexcept
    {
        return arr.cbegin();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr T* end(array<T, N>& arr) noexcept
    {
        return arr.end();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr const T* end(const array<T, N>& arr) noexcept
    {
        return arr.end();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr const T* cend(const array<T, N>& arr) noexcept
    {
        return arr.cend();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr T* data(array<T, N>& arr) noexcept
    {
        return arr.data();
    }

    template <typename T, size_t N>
        requires is_default_constructible_v<T>
    inline constexpr const T* data(const array<T, N>& arr) noexcept
    {
        return arr.data();
    }

    template <typename T, size_t N>
    inline constexpr bool empty(const array<T, N>& arr) noexcept
    {
        return arr.empty();
    }

    template <typename T, size_t N>
    inline constexpr auto size(const array<T, N>& arr) noexcept -> decltype(arr.size())
    {
        return arr.size();
    }

    template <typename T, size_t N>
    inline constexpr auto ssize(const array<T, N>& arr) noexcept
    {
        return static_cast<ptrdiff_t>(arr.size());
    }

    template <typename T, size_t N>
    inline constexpr size_t max_size(const array<T, N>& arr) noexcept
    {
        return arr.max_size();
    }

    template <size_t I, typename T, size_t N>
    inline constexpr T& get(array<T, N>& arr) noexcept
    {
        static_assert(I < N);
        return arr[I];
    }

    template <size_t I, typename T, size_t N>
    inline constexpr const T& get(const array<T, N>& arr) noexcept
    {
        static_assert(I < N);
        return arr[I];
    }

    template <size_t I, typename T, size_t N>
    inline constexpr T&& get(array<T, N>&& arr) noexcept
    {
        static_assert(I < N);
        return tempest::move(arr[I]);
    }

    template <size_t I, typename T, size_t N>
    inline constexpr const T&& get(const array<T, N>&& arr) noexcept
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