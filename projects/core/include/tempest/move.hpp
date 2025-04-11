#ifndef tempest_core_move_hpp
#define tempest_core_move_hpp

#include <tempest/type_traits.hpp>

namespace tempest
{
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
} // namespace tempest

#endif // tempest_core_move_hpp
