#ifndef tempest_core_utility_hpp
#define tempest_core_utility_hpp

#include <tempest/type_traits.hpp>

namespace tempest
{
    /// @brief Function used to forward an lvalue as an lvalue or an rvalue reference.
    /// @tparam T Type of the object to forward.
    /// @param t Reference to the object to forward.
    /// @return Forwarded reference.
    template <typename T>
    constexpr T&& forward(remove_reference_t<T>& t) noexcept
    {
        return static_cast<T&&>(t);
    }

    /// @brief Function used to forward an lvalue as an lvalue or an rvalue reference.
    /// @tparam T Type of the object to forward.
    /// @param t Reference to the object to forward.
    /// @return Forwarded reference.
    template <typename T>
    constexpr T&& forward(remove_reference_t<T>&& t) noexcept
    {
        static_assert(!is_lvalue_reference<T>::value, "Can't forward an rvalue as an lvalue.");
        return static_cast<T&&>(t);
    }

    /// @brief Function used to swap two objects.
    /// @tparam T Type of the objects to swap.
    /// @param a First object to swap.
    /// @param b Second object to swap.
    template <typename T>
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
} // namespace tempest

#endif // tempest_core_utility_hpp