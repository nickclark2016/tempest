#ifndef tempest_core_charconv_hpp
#define tempest_core_charconv_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    /// @brief Floating-point formatting options for to_chars.
    enum class chars_format : uint8_t
    {
        scientific = 1,
        fixed = 2,
        hex = 4,
        general = fixed | scientific
    };

    constexpr auto operator|(chars_format lhs, chars_format rhs) noexcept -> chars_format
    {
        return static_cast<chars_format>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
    }

    constexpr auto operator&(chars_format lhs, chars_format rhs) noexcept -> chars_format
    {
        return static_cast<chars_format>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
    }

    constexpr auto operator^(chars_format lhs, chars_format rhs) noexcept -> chars_format
    {
        return static_cast<chars_format>(static_cast<uint8_t>(lhs) ^ static_cast<uint8_t>(rhs));
    }

    constexpr auto operator~(chars_format fmt) noexcept -> chars_format
    {
        return static_cast<chars_format>(~static_cast<uint8_t>(fmt));
    }

    /// @brief Error codes returned by to_chars.
    enum class errc : uint8_t
    {
        success = 0,
        value_too_large = 1,
        invalid_argument = 2
    };

    /// @brief Return type for to_chars conversions.
    struct to_chars_result
    {
        char* ptr{nullptr};
        errc ec{errc::success};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return ec == errc::success;
        }

        [[nodiscard]] constexpr auto operator==(const to_chars_result& other) const noexcept -> bool = default;
    };

    namespace detail
    {
        TEMPEST_API auto to_chars_i64(char* first, char* last, int64_t value, int base) noexcept -> to_chars_result;
        TEMPEST_API auto to_chars_u64(char* first, char* last, uint64_t value, int base) noexcept -> to_chars_result;
        TEMPEST_API auto to_chars_f32(char* first, char* last, float value, chars_format fmt, int precision) noexcept
            -> to_chars_result;
        TEMPEST_API auto to_chars_f64(char* first, char* last, double value, chars_format fmt, int precision) noexcept
            -> to_chars_result;
    } // namespace detail

    inline constexpr int default_base = 10;

    /// @brief Converts an integral value to a character sequence.
    template <typename T>
        requires(is_integral_v<T> && !is_same_v<T, bool>)
    auto to_chars(char* first, char* last, T value, int base = default_base) noexcept -> to_chars_result
    {
        if constexpr (is_signed_v<T>)
        {
            return detail::to_chars_i64(first, last, static_cast<int64_t>(value), base);
        }
        else
        {
            return detail::to_chars_u64(first, last, static_cast<uint64_t>(value), base);
        }
    }

    /// @brief Converts a boolean to a character sequence ("true" or "false" or "1"/"0").
    inline auto to_chars(char* first, char* last, bool value) noexcept -> to_chars_result
    {
        return detail::to_chars_u64(first, last, value ? 1U : 0U, default_base);
    }

    /// @brief Converts a 32-bit float to a character sequence using default general format.
    TEMPEST_API auto to_chars(char* first, char* last, float value) noexcept -> to_chars_result;

    /// @brief Converts a 32-bit float to a character sequence using specified format.
    TEMPEST_API auto to_chars(char* first, char* last, float value, chars_format fmt) noexcept -> to_chars_result;

    /// @brief Converts a 32-bit float to a character sequence using specified format and precision.
    TEMPEST_API auto to_chars(char* first, char* last, float value, chars_format fmt, int precision) noexcept
        -> to_chars_result;

    /// @brief Converts a 64-bit double to a character sequence using default general format.
    TEMPEST_API auto to_chars(char* first, char* last, double value) noexcept -> to_chars_result;

    /// @brief Converts a 64-bit double to a character sequence using specified format.
    TEMPEST_API auto to_chars(char* first, char* last, double value, chars_format fmt) noexcept -> to_chars_result;

    /// @brief Converts a 64-bit double to a character sequence using specified format and precision.
    TEMPEST_API auto to_chars(char* first, char* last, double value, chars_format fmt, int precision) noexcept
        -> to_chars_result;

} // namespace tempest

#endif // tempest_core_charconv_hpp
