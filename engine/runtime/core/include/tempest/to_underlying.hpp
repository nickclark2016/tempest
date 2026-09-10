#ifndef tempest_core_to_underlying_hpp
#define tempest_core_to_underlying_hpp

#include <tempest/type_traits.hpp>

namespace tempest
{
    /// @brief Converts an enumeration value to its underlying type.
    /// @tparam T Type of the enumeration.
    /// @param value Enumeration value to convert.
    /// @return Underlying type of the enumeration.
    template <typename T>
        requires is_enum_v<T>
    constexpr auto to_underlying(T value) noexcept -> underlying_type_t<T>
    {
        return static_cast<underlying_type_t<T>>(value);
    }
} // namespace tempest

#endif // tempest_core_to_underlying_hpp