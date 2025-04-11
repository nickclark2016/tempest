#ifndef tempest_core_forward_hpp
#define tempest_core_forward_hpp

#include <tempest/move.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    template <typename T>
    constexpr add_const_t<T>& as_const(T& t) noexcept
    {
        return t;
    }

    template <typename T>
    void as_const(const T&&) = delete;

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

    template <typename T, typename U>
    constexpr auto&& forward_like(U&& u) noexcept
    {
        constexpr bool is_adding_const = is_const_v<remove_reference_t<T>>;
        if constexpr (is_lvalue_reference_v<T&&>)
        {
            if constexpr (is_adding_const)
            {
                return as_const(u);
            }
            else
            {
                return static_cast<U&>(u);
            }
        }
        else
        {
            if constexpr (is_adding_const)
            {
                return tempest::move(as_const(u));
            }
            else
            {
                return tempest::move(u);
            }
        }
    }
} // namespace tempest

#endif // tempest_core_forward_hpp
