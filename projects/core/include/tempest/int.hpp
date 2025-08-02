#ifndef tempest_core_int_hpp
#define tempest_core_int_hpp

namespace tempest
{
    /// @brief Unsigned integer type that is the result of the sizeof and alignof operators.
    using size_t = decltype(sizeof(0));

    /// @brief Signed integer type that is the result of subtracting two pointers.
    using ptrdiff_t = decltype(static_cast<int*>(nullptr) - static_cast<int*>(nullptr));

    /// @brief 32 bit floating point type.
    using float32_t = float;

    /// @brief 64 bit floating point type.
    using float64_t = double;

#if defined(_MSC_VER)

    /// @brief 8 bit signed integer type.
    using int8_t = signed char;

    /// @brief 16 bit signed integer type.
    using int16_t = signed short;

    /// @brief 32 bit signed integer type.
    using int32_t = int;

    /// @brief 64 bit signed integer type.
    using int64_t = long long;

    /// @brief 8 bit unsigned integer type.
    using uint8_t = unsigned char;

    /// @brief 16 bit unsigned integer type.
    using uint16_t = unsigned short;

    /// @brief 32 bit unsigned integer type.
    using uint32_t = unsigned int;

    /// @brief 64 bit unsigned integer type.
    using uint64_t = unsigned long long;

    /// @brief Wide character type used for wide characters, capable of storing any wchar_t value or wide EOF.
    using wint_t = unsigned short;

#else

    /// @brief 8 bit signed integer type.
    using int8_t = signed char;

    /// @brief 16 bit signed integer type.
    using int16_t = short;

    /// @brief 32 bit signed integer type.
    using int32_t = int;

    /// @brief 64 bit signed integer type.
    using int64_t = long;

    /// @brief 8 bit unsigned integer type.
    using uint8_t = unsigned char;

    /// @brief 16 bit unsigned integer type.
    using uint16_t = unsigned short;

    /// @brief 32 bit unsigned integer type.
    using uint32_t = unsigned int;

    /// @brief 64 bit unsigned integer type.
    using uint64_t = unsigned long;

    /// @brief Wide character type used for wide characters, capable of storing any wchar_t value or wide EOF.
    using wint_t = unsigned int;

#endif

    /// @brief Unsigned integer type capable of holding a pointer.
    using uintptr_t = decltype(sizeof(static_cast<void*>(nullptr)));

    /// @brief Integer type representing a byte as specified by the C++ language standard.
    enum class byte : unsigned char
    {
    };

    /// @brief Converts a byte to an integer type.
    /// @tparam T The integer type to convert to.
    /// @param b The byte to convert.
    /// @return The byte converted to the specified integer type.
    template <typename T>
    constexpr T to_integer(byte b) noexcept
    {
        return static_cast<T>(b);
    }

    /// @brief Left shift assignment operator for byte.
    /// @tparam T The type to shift by.
    /// @param b The byte to shift.
    /// @param shift The amount to shift by.
    /// @return The byte shifted by the specified amount.
    template <typename T>
    constexpr byte& operator<<=(byte& b, T shift) noexcept
    {
        return b = byte(to_integer<unsigned int>(b) << shift);
    }

    /// @brief Right shift assignment operator for byte.
    /// @tparam T The type to shift by.
    /// @param b The byte to shift.
    /// @param shift The amount to shift by.
    /// @return The byte shifted by the specified amount.
    template <typename T>
    constexpr byte& operator>>=(byte& b, T shift) noexcept
    {
        return b = byte(to_integer<unsigned int>(b) >> shift);
    }

    /// @brief Left shift operator for byte.
    /// @tparam T The type to shift by.
    /// @param b The byte to shift.
    /// @param shift The amount to shift by.
    /// @return The byte shifted by the specified amount.
    template <typename T>
    constexpr byte operator<<(byte b, T shift) noexcept
    {
        return byte(to_integer<unsigned int>(b) << shift);
    }

    /// @brief Right shift operator for byte.
    /// @tparam T The type to shift by.
    /// @param b The byte to shift.
    /// @param shift The amount to shift by.
    /// @return The byte shifted by the specified amount.
    template <typename T>
    constexpr byte operator>>(byte b, T shift) noexcept
    {
        return byte(to_integer<unsigned int>(b) >> shift);
    }

    /// @brief Bitwise AND assignment operator for byte.
    /// @param lhs The left hand side byte.
    /// @param rhs The right hand side byte.
    /// @return The result of the bitwise AND operation.
    constexpr byte& operator&=(byte& lhs, byte rhs) noexcept
    {
        return lhs = byte(to_integer<unsigned int>(lhs) & to_integer<unsigned int>(rhs));
    }

    /// @brief Bitwise OR assignment operator for byte.
    /// @param lhs The left hand side byte.
    /// @param rhs The right hand side byte.
    /// @return The result of the bitwise OR operation.
    constexpr byte& operator|=(byte& lhs, byte rhs) noexcept
    {
        return lhs = byte(to_integer<unsigned int>(lhs) | to_integer<unsigned int>(rhs));
    }

    /// @brief Bitwise XOR assignment operator for byte.
    /// @param lhs The left hand side byte.
    /// @param rhs The right hand side byte.
    /// @return The result of the bitwise XOR operation.
    constexpr byte& operator^=(byte& lhs, byte rhs) noexcept
    {
        return lhs = byte(to_integer<unsigned int>(lhs) ^ to_integer<unsigned int>(rhs));
    }

    /// @brief Bitwise AND operator for byte.
    /// @param lhs The left hand side byte.
    /// @param rhs The right hand side byte.
    /// @return The result of the bitwise AND operation.
    constexpr byte operator&(byte lhs, byte rhs) noexcept
    {
        return byte(to_integer<unsigned int>(lhs) & to_integer<unsigned int>(rhs));
    }

    /// @brief Bitwise OR operator for byte.
    /// @param lhs The left hand side byte.
    /// @param rhs The right hand side byte.
    /// @return The result of the bitwise OR operation.
    constexpr byte operator|(byte lhs, byte rhs) noexcept
    {
        return byte(to_integer<unsigned int>(lhs) | to_integer<unsigned int>(rhs));
    }

    /// @brief Bitwise XOR operator for byte.
    /// @param lhs The left hand side byte.
    /// @param rhs The right hand side byte.
    /// @return The result of the bitwise XOR operation.
    constexpr byte operator^(byte lhs, byte rhs) noexcept
    {
        return byte(to_integer<unsigned int>(lhs) ^ to_integer<unsigned int>(rhs));
    }

    /// @brief Bitwise NOT operator for byte.
    /// @param b The byte to invert.
    /// @return The inverted byte.
    constexpr byte operator~(byte b) noexcept
    {
        return byte(~to_integer<unsigned int>(b));
    }
} // namespace tempest

#endif