#ifndef tempest_core_utility_hpp
#define tempest_core_utility_hpp

#include <tempest/type_traits.hpp>

namespace tempest
{
    template <typename T>
    struct tuple_size;

    template <size_t I, typename T>
    struct tuple_element;

    /// @brief Function used to indicate that an object may be moved from. Produces an xvalue expression from its
    /// argument.
    /// @tparam T Type of the object to move.
    /// @param t Object to move.
    /// @return Xvalue expression of the object.
    template <typename T>
    inline constexpr remove_reference_t<T>&& move(T&& t) noexcept
    {
        return static_cast<remove_reference_t<T>&&>(t);
    }

    template <typename T>
    inline constexpr conditional_t<is_nothrow_move_constructible_v<T> || !is_copy_constructible_v<T>, T&&, const T&>
    move_if_noexcept(T& t) noexcept
    {
        if constexpr (is_nothrow_move_constructible_v<T> || !is_copy_constructible_v<T>)
        {
            return move(t);
        }
        else
        {
            return t;
        }
    }

    /// @brief Function used to forward an lvalue as an lvalue or an rvalue reference.
    /// @tparam T Type of the object to forward.
    /// @param t Reference to the object to forward.
    /// @return Forwarded reference.
    template <typename T>
    inline constexpr T&& forward(remove_reference_t<T>& t) noexcept
    {
        return static_cast<T&&>(t);
    }

    /// @brief Function used to forward an lvalue as an lvalue or an rvalue reference.
    /// @tparam T Type of the object to forward.
    /// @param t Reference to the object to forward.
    /// @return Forwarded reference.
    template <typename T>
    inline constexpr T&& forward(remove_reference_t<T>&& t) noexcept
    {
        static_assert(!is_lvalue_reference<T>::value, "Can't forward an rvalue as an lvalue.");
        return static_cast<T&&>(t);
    }

    /// @brief Function used to swap two objects.
    /// @tparam T Type of the objects to swap.
    /// @param a First object to swap.
    /// @param b Second object to swap.
    template <typename T>
        requires(is_nothrow_move_constructible_v<T> && is_nothrow_move_assignable_v<T>)
    inline constexpr void swap(T& a,
                               T& b) noexcept(is_nothrow_move_constructible_v<T> && is_nothrow_move_assignable_v<T>)
    {
        T temp = move(a);
        a = move(b);
        b = move(temp);
    }

    /// @brief Converts an enumeration value to its underlying type.
    /// @tparam T Type of the enumeration.
    /// @param value Enumeration value to convert.
    /// @return Underlying type of the enumeration.
    template <typename T>
        requires is_enum_v<T>
    inline constexpr underlying_type_t<T> to_underlying(T value) noexcept
    {
        return static_cast<underlying_type_t<T>>(value);
    }

    /// @brief Exchanges the values of two objects.
    /// @tparam T Type of the objects to exchange.
    /// @tparam U Type of the new value.
    /// @param obj Object to exchange.
    /// @param new_value New value to assign to the object.
    /// @return Old value of the object.
    template <typename T, typename U = T>
    inline constexpr T exchange(T& obj, U&& new_value)
    {
        T old_value = tempest::move(obj);
        obj = tempest::forward<U>(new_value);
        return old_value;
    }

    /// @brief Tuple-like structure that holds two objects.
    /// @tparam T1 First type.
    /// @tparam T2 Second type.
    template <typename T1, typename T2>
    struct pair
    {
        /// @brief Type of the first object.
        using first_type = T1;

        /// @brief Type of the second object.
        using second_type = T2;

        /// @brief First object.
        T1 first{};

        /// @brief Second object.
        T2 second{};

        /// @brief Default constructor.
        constexpr pair() = default;

        /// @brief Constructs a pair from a copy of the first and second objects.
        /// @param first Object to copy to the first object.
        /// @param second Object to copy to the second object.
        constexpr pair(const T1& first, const T2& second)
            requires(is_copy_constructible_v<T1> && is_copy_constructible_v<T2>);

        /// @brief Constructs a pair from a perfect-forwarded first and second objects.
        /// @tparam U1 Type of the first object. Defaults to T1.
        /// @tparam U2 Type of the second object. Defaults to T2.
        /// @param first First object to perfect-forward.
        /// @param second Second object to perfect-forward.
        template <typename U1 = T1, typename U2 = T2>
            requires(is_constructible_v<T1, U1> && is_constructible_v<T2, U2>)
        constexpr pair(U1&& first, U2&& second);

        /// @brief Constructs a pair from a pair of convertible objects.
        /// @tparam U1 Type of the first object.
        /// @tparam U2 Type of the second object.
        /// @param other Pair to copy from.
        template <typename U1, typename U2>
        constexpr pair(const pair<U1, U2>& other)
            requires(is_constructible_v<T1, const U1&> && is_constructible_v<T2, const U2&>);

        /// @brief Constructs a pair from a pair of convertible objects.
        /// @tparam U1 Type of the first object.
        /// @tparam U2 Type of the second object.
        /// @param other Pair to move from.
        template <typename U1, typename U2>
        constexpr pair(pair<U1, U2>&& other)
            requires(is_constructible_v<T1, U1 &&> && is_constructible_v<T2, U2 &&>);

        /// @brief Copy constructor.
        /// @param other Object to copy from.
        pair(const pair& other) = default;

        /// @brief Move constructor.
        /// @param other Object to move from.
        pair(pair&& other) = default;

        /// @brief Destructor.
        ~pair() = default;

        /// @brief Copy assignment operator.
        /// @param other Object to copy from.
        /// @return Reference to this object.
        constexpr pair& operator=(const pair& other) = default;

        /// @brief Move assignment operator.
        /// @param other Object to move from.
        /// @return Reference to this object.
        constexpr pair& operator=(pair&& other) = default;

        /// @brief Copy assignment operator from a pair of convertible objects.
        /// @tparam U1 Type of the first object.
        /// @tparam U2 Type of the second object.
        /// @param other Pair to copy from.
        /// @return Reference to this object.
        template <typename U1, typename U2>
        constexpr pair& operator=(const pair<U1, U2>& other)
            requires(is_assignable_v<T1&, const U1&> && is_assignable_v<T2&, const U2&>);

        /// @brief Move assignment operator from a pair of convertible objects.
        /// @tparam U1 Type of the first object.
        /// @tparam U2 Type of the second object.
        /// @param other Pair to move from.
        /// @return Reference to this object.
        template <typename U1, typename U2>
        constexpr pair& operator=(pair<U1, U2>&& other)
            requires(is_assignable_v<T1&, U1 &&> && is_assignable_v<T2&, U2 &&>);

        constexpr void swap(pair& other) noexcept(is_nothrow_swappable_v<T1> && is_nothrow_swappable_v<T2>);
    };

    template <typename T1, typename T2>
    inline constexpr pair<T1, T2>::pair(const T1& first, const T2& second)
        requires(is_copy_constructible_v<T1> && is_copy_constructible_v<T2>)
        : first(first), second(second)
    {
    }

    template <typename T1, typename T2>
    template <typename U1, typename U2>
        requires(is_constructible_v<T1, U1> && is_constructible_v<T2, U2>)
    inline constexpr pair<T1, T2>::pair(U1&& first, U2&& second)
        : first(tempest::forward<U1>(first)), second(tempest::forward<U2>(second))
    {
    }

    template <typename T1, typename T2>
    template <typename U1, typename U2>
    constexpr pair<T1, T2>::pair(const pair<U1, U2>& other)
        requires(is_constructible_v<T1, const U1&> && is_constructible_v<T2, const U2&>)
        : first(other.first), second(other.second)
    {
    }

    template <typename T1, typename T2>
    template <typename U1, typename U2>
    inline constexpr pair<T1, T2>::pair(pair<U1, U2>&& other)
        requires(is_constructible_v<T1, U1 &&> && is_constructible_v<T2, U2 &&>)
        : first(move(other.first)), second(move(other.second))
    {
    }

    template <typename T1, typename T2>
    template <typename U1, typename U2>
    inline constexpr pair<T1, T2>& pair<T1, T2>::operator=(const pair<U1, U2>& other)
        requires(is_assignable_v<T1&, const U1&> && is_assignable_v<T2&, const U2&>)
    {
        first = other.first;
        second = other.second;
        return *this;
    }

    template <typename T1, typename T2>
    template <typename U1, typename U2>
    inline constexpr pair<T1, T2>& pair<T1, T2>::operator=(pair<U1, U2>&& other)
        requires(is_assignable_v<T1&, U1 &&> && is_assignable_v<T2&, U2 &&>)
    {
        first = move(other.first);
        second = move(other.second);
        return *this;
    }

    template <typename T1, typename T2>
    inline constexpr void pair<T1, T2>::swap(pair& other) noexcept(is_nothrow_swappable_v<T1> &&
                                                                   is_nothrow_swappable_v<T2>)
    {
        swap(first, other.first);
        swap(second, other.second);
    }

    /// @brief Creates a pair from two objects.
    /// @tparam T1 Type of the first object.
    /// @tparam T2 Type of the second object.
    /// @param first First object.
    /// @param second Second object.
    /// @return Pair containing the two objects.
    template <typename T1, typename T2>
    inline constexpr pair<T1, T2> make_pair(T1&& first, T2&& second)
    {
        return pair<T1, T2>(forward<T1>(first), forward<T2>(second));
    }

    /// @brief Lexicographically compares two pairs for equality.
    /// @tparam T1 First type of the left-hand side pair.
    /// @tparam T2 Second type of the left-hand side pair.
    /// @tparam U1 First type of the right-hand side pair.
    /// @tparam U2 Second type of the right-hand side pair.
    /// @param lhs Left-hand side pair.
    /// @param rhs Right-hand side pair.
    /// @return True if the pairs are equal, false otherwise.
    template <typename T1, typename T2, typename U1, typename U2>
    inline constexpr bool operator==(const pair<T1, T2>& lhs, const pair<U1, U2>& rhs)
    {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }

    /// @brief Lexicographically compares two pairs for inequality.
    /// @tparam T1 First type of the left-hand side pair.
    /// @tparam T2 Second type of the left-hand side pair.
    /// @tparam U1 First type of the right-hand side pair.
    /// @tparam U2 Second type of the right-hand side pair.
    /// @param lhs Left-hand side pair.
    /// @param rhs Right-hand side pair.
    /// @return True if the pairs are not equal, false otherwise.
    template <typename T1, typename T2, typename U1, typename U2>
    inline constexpr bool operator!=(const pair<T1, T2>& lhs, const pair<U1, U2>& rhs)
    {
        return !(lhs == rhs);
    }

    /// @brief Lexicographically three-way compares two pairs.
    /// @tparam T1 First type of the left-hand side pair.
    /// @tparam T2 Second type of the left-hand side pair.
    /// @tparam U1 First type of the right-hand side pair.
    /// @tparam U2 Second type of the right-hand side pair.
    /// @param lhs Left-hand side pair.
    /// @param rhs Right-hand side pair.
    /// @return Comparison result of the first objects of each pair if they differ, otherwise the comparison result of
    /// the second objects.
    template <typename T1, typename T2, typename U1, typename U2>
    constexpr auto operator<=>(const pair<T1, T2>& lhs, const pair<U1, U2>& rhs)
    {
        if (auto cmp = lhs.first <=> rhs.first; cmp != 0)
        {
            return cmp;
        }
        return lhs.second <=> rhs.second;
    }

    /// @brief Swaps two pairs.
    /// @tparam T1 First type in the pairs.
    /// @tparam T2 Second type in the pairs.
    /// @param lhs Left-hand side pair.
    /// @param rhs Right-hand side pair.
    template <typename T1, typename T2>
    void swap(pair<T1, T2>& lhs, pair<T1, T2>& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    template <typename T>
    struct tuple_size<const T> : integral_constant<size_t, tuple_size<T>::value>
    {
    };

    template <typename T1, typename T2>
    struct tuple_size<pair<T1, T2>> : integral_constant<size_t, 2>
    {
    };

    template <size_t I, typename T1, typename T2>
    struct tuple_element<I, pair<T1, T2>>
    {
        static_assert(I < 2, "Index out of bounds.");
    };

    template <typename T1, typename T2>
    struct tuple_element<0, pair<T1, T2>>
    {
        using type = T1;
    };

    template <typename T1, typename T2>
    struct tuple_element<1, pair<T1, T2>>
    {
        using type = T2;
    };

    template <size_t I, typename T1, typename T2>
    inline constexpr typename tuple_element<I, pair<T1, T2>>::type& get(pair<T1, T2>& p)
    {
        if constexpr (I == 0)
        {
            return p.first;
        }
        else
        {
            return p.second;
        }
    }

    template <size_t I, typename T1, typename T2>
    inline constexpr const typename tuple_element<I, pair<T1, T2>>::type& get(const pair<T1, T2>& p)
    {
        if constexpr (I == 0)
        {
            return p.first;
        }
        else
        {
            return p.second;
        }
    }

    template <size_t I, typename T1, typename T2>
    inline constexpr typename tuple_element<I, pair<T1, T2>>::type&& get(pair<T1, T2>&& p) noexcept
    {
        if constexpr (I == 0)
        {
            return tempest::move(p).first;
        }
        else
        {
            return tempest::move(p).second;
        }
    }

    template <size_t I, typename T1, typename T2>
    inline constexpr const typename tuple_element<I, pair<T1, T2>>::type&& get(const pair<T1, T2>&& p) noexcept
    {
        if constexpr (I == 0)
        {
            return tempest::move(p).first;
        }
        else
        {
            return tempest::move(p).second;
        }
    }

    template <typename T, typename U>
    constexpr T& get(pair<T, U>& p)
    {
        return p.first;
    }

    template <typename T, typename U>
    constexpr const T& get(const pair<T, U>& p)
    {
        return p.first;
    }

    template <typename T, typename U>
    constexpr T&& get(pair<T, U>&& p) noexcept
    {
        return tempest::move(p).first;
    }

    template <typename T, typename U>
    constexpr const T&& get(const pair<T, U>&& p) noexcept
    {
        return tempest::move(p).first;
    }

    template <typename T, typename U>
    constexpr U& get(pair<T, U>& p)
    {
        return p.second;
    }

    template <typename T, typename U>
    constexpr const U& get(const pair<T, U>& p)
    {
        return p.second;
    }

    template <typename T, typename U>
    constexpr U&& get(pair<T, U>&& p) noexcept
    {
        return tempest::move(p).second;
    }

    template <typename T, typename U>
    constexpr const U&& get(const pair<T, U>&& p) noexcept
    {
        return tempest::move(p).second;
    }
} // namespace tempest

#endif // tempest_core_utility_hpp