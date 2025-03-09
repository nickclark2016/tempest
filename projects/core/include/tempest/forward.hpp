#ifndef tempest_core_forward_hpp
#define tempest_core_forward_hpp

#include <tempest/type_traits.hpp>

namespace tempest
{
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
} // namespace tempest

#endif // tempest_core_forward_hpp
