#include <tempest/charconv.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/bit.hpp>
#include <tempest/limits.hpp>
#include <tempest/math.hpp>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <errno.h>
#include <iconv.h>
#include <stdio.h>
#endif

namespace tempest
{
    namespace
    {
        inline constexpr int default_float_precision = 6;
        inline constexpr int max_float_precision = 32;
        inline constexpr int decimal_base = 10;
        inline constexpr int decimal_hundred = 100;
        inline constexpr int min_radix_base = 2;
        inline constexpr int max_radix_base = 36;
        inline constexpr size_t int_buffer_size = 64;
        inline constexpr double rounding_factor_half = 0.5;
        inline constexpr double decimal_base_float = 10.0;
        inline constexpr int max_single_digit = 9;
        inline constexpr uint64_t float64_sign_mask = 0x8000'0000'0000'0000ULL;
        inline constexpr uint32_t float64_exp_mask = 0x7FFU;
        inline constexpr uint64_t float64_mantissa_mask = 0x000F'FFFF'FFFF'FFFFULL;
        inline constexpr int float64_exp_shift = 52;
        inline constexpr int max_pow10_table_exp = 22;
        inline constexpr size_t num_scale_steps = 9;
        constexpr const char* digits_lower = "0123456789abcdefghijklmnopqrstuvwxyz";

        // Powers of 10 table for fast float scaling
        constexpr double pow10_table[] = { // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
            1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

        auto get_pow10(int exp) noexcept -> double
        {
            if (exp >= 0 && exp <= max_pow10_table_exp)
            {
                return pow10_table[exp];
            }
            if (exp < 0 && exp >= -max_pow10_table_exp)
            {
                return 1.0 / pow10_table[-exp];
            }
            // For larger exponents, compute via square-and-multiply
            auto result = 1.0;
            auto base = decimal_base_float;
            auto abs_exp = (exp < 0) ? -exp : exp;
            while (abs_exp > 0)
            {
                if ((abs_exp & 1) != 0)
                {
                    result *= base;
                }
                base *= base;
                abs_exp >>= 1;
            }
            return (exp < 0) ? (1.0 / result) : result;
        }

        constexpr double pow10_scale_table[] = { // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            1e256, 1e128, 1e64, 1e32, 1e16, 1e8, 1e4, 1e2, 1e1};

        constexpr int pow10_scale_exponents[] = { // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            256, 128, 64, 32, 16, 8, 4, 2, 1};

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        void normalize_float_exp(double& val, int& exp) noexcept
        {
            if (val <= 0.0)
            {
                return;
            }

            for (size_t step = 0; step < num_scale_steps; ++step)
            {
                while (val >= pow10_scale_table[step] * decimal_base_float)
                {
                    val /= pow10_scale_table[step];
                    exp += pow10_scale_exponents[step];
                }
            }
            while (val >= decimal_base_float)
            {
                val /= decimal_base_float;
                exp++;
            }

            for (size_t step = 0; step < num_scale_steps; ++step)
            {
                while (val < 1.0 / pow10_scale_table[step])
                {
                    val *= pow10_scale_table[step];
                    exp -= pow10_scale_exponents[step];
                }
            }
            while (val < 1.0)
            {
                val *= decimal_base_float;
                exp--;
            }
        }

        // Fast base 10 lookup table
        constexpr const char* two_digits_table = "00010203040506070809"
                                                 "10111213141516171819"
                                                 "20212223242526272829"
                                                 "30313233343536373839"
                                                 "40414243444546474849"
                                                 "50515253545556575859"
                                                 "60616263646566676869"
                                                 "70717273747576777879"
                                                 "80818283848586878889"
                                                 "90919293949596979899";

        auto format_float_scientific(char* first, char* last, double val, int precision, bool upper) noexcept
            -> to_chars_result;
        auto format_float_fixed(char* first, char* last, double val, int precision) noexcept -> to_chars_result;
        auto format_float_general(char* first, char* last, double val, int precision, bool upper) noexcept
            -> to_chars_result;
    } // namespace

    namespace detail
    {
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        auto to_chars_u64(char* first, char* last, uint64_t value, int base) noexcept -> to_chars_result
        {
            if (base < min_radix_base || base > max_radix_base)
            {
                return {.ptr = last, .ec = errc::invalid_argument};
            }

            if (first >= last)
            {
                return {.ptr = last, .ec = errc::value_too_large};
            }

            if (value == 0)
            {
                *first++ = '0';
                return {.ptr = first, .ec = errc::success};
            }

            char temp[int_buffer_size]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            auto temp_idx = 0;

            if (base == decimal_base)
            {
                while (value >= decimal_hundred)
                {
                    auto rem = static_cast<size_t>(value % decimal_hundred);
                    value /= decimal_hundred;
                    temp[temp_idx++] = two_digits_table[(rem * 2) + 1];
                    temp[temp_idx++] = two_digits_table[rem * 2];
                }

                if (value >= decimal_base)
                {
                    auto rem = static_cast<size_t>(value);
                    temp[temp_idx++] = two_digits_table[(rem * 2) + 1];
                    temp[temp_idx++] = two_digits_table[rem * 2];
                }
                else
                {
                    temp[temp_idx++] = static_cast<char>('0' + value);
                }
            }
            else
            {
                auto ubase = static_cast<uint64_t>(base);
                while (value > 0)
                {
                    auto rem = value % ubase;
                    value /= ubase;
                    temp[temp_idx++] = digits_lower[rem];
                }
            }

            if (static_cast<size_t>(last - first) < static_cast<size_t>(temp_idx))
            {
                return {.ptr = last, .ec = errc::value_too_large};
            }

            while (temp_idx > 0)
            {
                *first++ = temp[--temp_idx];
            }

            return {.ptr = first, .ec = errc::success};
        }

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        auto to_chars_i64(char* first, char* last, int64_t value, int base) noexcept -> to_chars_result
        {
            if (base < min_radix_base || base > max_radix_base)
            {
                return {.ptr = last, .ec = errc::invalid_argument};
            }

            if (first >= last)
            {
                return {.ptr = last, .ec = errc::value_too_large};
            }

            if (value < 0)
            {
                *first++ = '-';
                auto uval = (value == tempest::numeric_limits<int64_t>::min())
                                ? static_cast<uint64_t>(tempest::numeric_limits<int64_t>::max()) + 1ULL
                                : static_cast<uint64_t>(-value);
                return to_chars_u64(first, last, uval, base);
            }

            return to_chars_u64(first, last, static_cast<uint64_t>(value), base);
        }

        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        auto to_chars_f64_impl(char* first, char* last, double value, chars_format fmt, int precision) noexcept
            -> to_chars_result
        {
            // Extract IEEE-754 bits
            auto bits = bit_cast<uint64_t>(value);
            auto is_neg = (bits & float64_sign_mask) != 0;
            auto exp_bits = static_cast<uint32_t>((bits >> float64_exp_shift) & float64_exp_mask);
            auto mantissa_bits = bits & float64_mantissa_mask;

            // Check for NaN
            if (exp_bits == float64_exp_mask && mantissa_bits != 0)
            {
                const char* nan_str = is_neg ? "-nan" : "nan";
                auto len = is_neg ? 4 : 3;
                if (last - first < len)
                {
                    return {.ptr = last, .ec = errc::value_too_large};
                }
                for (auto idx = 0; idx < len; ++idx)
                {
                    *first++ = nan_str[idx];
                }
                return {.ptr = first, .ec = errc::success};
            }

            // Check for Infinity
            if (exp_bits == float64_exp_mask && mantissa_bits == 0)
            {
                const char* inf_str = is_neg ? "-inf" : "inf";
                auto len = is_neg ? 4 : 3;
                if (last - first < len)
                {
                    return {.ptr = last, .ec = errc::value_too_large};
                }
                for (auto idx = 0; idx < len; ++idx)
                {
                    *first++ = inf_str[idx];
                }
                return {.ptr = first, .ec = errc::success};
            }

            // Clamp precision
            if (precision < 0)
            {
                precision = default_float_precision;
            }
            precision = tempest::min(precision, max_float_precision);

            // Write sign for negative numbers (including -0.0)
            if (is_neg)
            {
                if (first >= last)
                {
                    return {.ptr = last, .ec = errc::value_too_large};
                }
                *first++ = '-';
            }

            auto abs_val = is_neg ? -value : value;

            if (fmt == chars_format::scientific)
            {
                return format_float_scientific(first, last, abs_val, precision, false);
            }
            if (fmt == chars_format::fixed)
            {
                return format_float_fixed(first, last, abs_val, precision);
            }
            return format_float_general(first, last, abs_val, precision, false);
        }

        auto to_chars_f32(char* first, char* last, float value, chars_format fmt, int precision) noexcept
            -> to_chars_result
        {
            return to_chars_f64_impl(first, last, static_cast<double>(value), fmt, precision);
        }

        auto to_chars_f64(char* first, char* last, double value, chars_format fmt, int precision) noexcept
            -> to_chars_result
        {
            return to_chars_f64_impl(first, last, value, fmt, precision);
        }
    } // namespace detail

    namespace
    {
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        auto format_float_fixed(char* first, char* last, double val, int precision) noexcept -> to_chars_result
        {
            // Add rounding factor
            auto round_factor = rounding_factor_half * get_pow10(-precision);
            val += round_factor;

            auto int_part = static_cast<uint64_t>(val);
            auto frac = val - static_cast<double>(int_part);

            // Write integer part
            auto res = detail::to_chars_u64(first, last, int_part, decimal_base);
            if (!res)
            {
                return res;
            }
            first = res.ptr;

            if (precision > 0)
            {
                if (first >= last)
                {
                    return {.ptr = last, .ec = errc::value_too_large};
                }
                *first++ = '.';

                for (auto idx = 0; idx < precision; ++idx)
                {
                    frac *= decimal_base_float;
                    auto digit = static_cast<int>(frac);
                    digit = tempest::min(digit, max_single_digit);
                    frac -= digit;
                    if (first >= last)
                    {
                        return {.ptr = last, .ec = errc::value_too_large};
                    }
                    *first++ = static_cast<char>('0' + digit);
                }
            }

            return {.ptr = first, .ec = errc::success};
        }

        // NOLINTNEXTLINE(readability-function-cognitive-complexity,bugprone-easily-swappable-parameters)
        auto format_float_scientific(char* first, char* last, double val, int precision, bool upper) noexcept
            -> to_chars_result
        {
            auto exp = 0;
            normalize_float_exp(val, exp);

            // Rounding for scientific: val is in [1.0, 10.0)
            auto round_factor = rounding_factor_half * get_pow10(-precision);
            val += round_factor;
            if (val >= decimal_base_float)
            {
                val /= decimal_base_float;
                exp++;
            }

            auto int_part = static_cast<int>(val);
            auto frac = val - static_cast<double>(int_part);

            if (first >= last)
            {
                return {.ptr = last, .ec = errc::value_too_large};
            }
            *first++ = static_cast<char>('0' + int_part);

            if (precision > 0)
            {
                if (first >= last)
                {
                    return {.ptr = last, .ec = errc::value_too_large};
                }
                *first++ = '.';

                for (auto idx = 0; idx < precision; ++idx)
                {
                    frac *= decimal_base_float;
                    auto digit = static_cast<int>(frac);
                    digit = tempest::min(digit, max_single_digit);
                    frac -= digit;
                    if (first >= last)
                    {
                        return {.ptr = last, .ec = errc::value_too_large};
                    }
                    *first++ = static_cast<char>('0' + digit);
                }
            }

            // Exponent part: e+xx or e-xx
            if (last - first < 4)
            {
                return {.ptr = last, .ec = errc::value_too_large};
            }
            *first++ = upper ? 'E' : 'e';
            *first++ = (exp >= 0) ? '+' : '-';
            auto abs_exp = (exp < 0) ? -exp : exp;
            if (abs_exp < decimal_base)
            {
                *first++ = '0';
                *first++ = static_cast<char>('0' + abs_exp);
            }
            else
            {
                auto exp_res = detail::to_chars_u64(first, last, static_cast<uint64_t>(abs_exp), decimal_base);
                if (!exp_res)
                {
                    return exp_res;
                }
                first = exp_res.ptr;
            }

            return {.ptr = first, .ec = errc::success};
        }

        // NOLINTNEXTLINE(readability-function-cognitive-complexity,bugprone-easily-swappable-parameters)
        auto format_float_general(char* first, char* last, double val, int precision, bool upper) noexcept
            -> to_chars_result
        {
            if (val == 0.0)
            {
                if (first >= last)
                {
                    return {.ptr = last, .ec = errc::value_too_large};
                }
                *first++ = '0';
                return {.ptr = first, .ec = errc::success};
            }

            auto exp = 0;
            auto temp_val = val;
            normalize_float_exp(temp_val, exp);

            if (precision == 0)
            {
                precision = 1;
            }

            if (exp >= -4 && exp < precision)
            {
                auto fixed_precision = precision - 1 - exp;
                auto res = format_float_fixed(first, last, val, fixed_precision);
                if (!res)
                {
                    return res;
                }

                // In general format, trim trailing zeros after decimal point
                auto* result_ptr = res.ptr;
                auto* decimal_ptr = first;
                while (decimal_ptr < result_ptr && *decimal_ptr != '.')
                {
                    decimal_ptr++;
                }

                if (decimal_ptr < result_ptr) // Has decimal point
                {
                    while (result_ptr > decimal_ptr && *(result_ptr - 1) == '0')
                    {
                        result_ptr--;
                    }
                    if (result_ptr > decimal_ptr && *(result_ptr - 1) == '.')
                    {
                        result_ptr--; // Remove decimal point if all fractional digits were zeros
                    }
                }
                return {.ptr = result_ptr, .ec = errc::success};
            }

            auto sci_precision = precision - 1;
            auto res = format_float_scientific(first, last, val, sci_precision, upper);
            if (!res)
            {
                return res;
            }

            // Trim trailing zeros from mantissa
            auto* result_ptr = res.ptr;
            // Find 'e' or 'E'
            auto* exp_ptr = first;
            while (exp_ptr < result_ptr && *exp_ptr != 'e' && *exp_ptr != 'E')
            {
                exp_ptr++;
            }

            if (exp_ptr < result_ptr)
            {
                auto* mantissa_end = exp_ptr;
                while (mantissa_end > first && *(mantissa_end - 1) == '0')
                {
                    mantissa_end--;
                }
                if (mantissa_end > first && *(mantissa_end - 1) == '.')
                {
                    mantissa_end--;
                }

                if (mantissa_end != exp_ptr)
                {
                    // Move exponent part backwards
                    auto exp_len = static_cast<size_t>(result_ptr - exp_ptr);
                    for (size_t idx = 0; idx < exp_len; ++idx)
                    {
                        mantissa_end[idx] = exp_ptr[idx];
                    }
                    result_ptr = mantissa_end + exp_len;
                }
            }

            return {.ptr = result_ptr, .ec = errc::success};
        }
    } // namespace

    auto to_chars(char* first, char* last, float value) noexcept -> to_chars_result
    {
        return detail::to_chars_f32(first, last, value, chars_format::general, default_float_precision);
    }

    auto to_chars(char* first, char* last, float value, chars_format fmt) noexcept -> to_chars_result
    {
        return detail::to_chars_f32(first, last, value, fmt, default_float_precision);
    }

    auto to_chars(char* first, char* last, float value, chars_format fmt, int precision) noexcept -> to_chars_result
    {
        return detail::to_chars_f32(first, last, value, fmt, precision);
    }

    auto to_chars(char* first, char* last, double value) noexcept -> to_chars_result
    {
        return detail::to_chars_f64(first, last, value, chars_format::general, default_float_precision);
    }

    auto to_chars(char* first, char* last, double value, chars_format fmt) noexcept -> to_chars_result
    {
        return detail::to_chars_f64(first, last, value, fmt, default_float_precision);
    }

    auto to_chars(char* first, char* last, double value, chars_format fmt, int precision) noexcept -> to_chars_result
    {
        return detail::to_chars_f64(first, last, value, fmt, precision);
    }

    auto convert_wide_to_narrow(tempest::wstring_view wide_str) -> string
    {
        tempest::string result;

#ifdef _WIN32
        const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_str.data(), static_cast<int>(wide_str.size()),
                                                     nullptr, 0, nullptr, nullptr);
        result.resize(static_cast<size_t>(size_needed), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide_str.data(), static_cast<int>(wide_str.size()), result.data(),
                            static_cast<int>(size_needed), nullptr, nullptr);
#else
        auto conv_desc = iconv_open("UTF-8", "WCHAR_T");
        if (conv_desc == bit_cast<iconv_t>(-1ll))
        {
            perror("iconv_open failed");
            return result;
        }

        auto in_bytes = wide_str.size() * sizeof(wchar_t);
        const auto out_bytes = in_bytes * 4 + 1;
        result.resize(out_bytes, '\0');

        char* in_buf = const_cast<char*>(reinterpret_cast<const char*>(wide_str.data()));
        char* out_buf = result.data();

        auto bytes_left = out_bytes;

        const auto res = iconv(conv_desc, &in_buf, &in_bytes, &out_buf, &bytes_left);
        if (res == static_cast<size_t>(-1))
        {
            perror("iconv failed");
            iconv_close(conv_desc);
            return string();
        }

        iconv_close(conv_desc);
        result.resize(out_bytes - bytes_left);
#endif

        return result;
    }

    auto convert_narrow_to_wide(tempest::string_view narrow_str) -> wstring
    {
        wstring result;
#ifdef _WIN32
        const auto size_needed =
            MultiByteToWideChar(CP_UTF8, 0, narrow_str.data(), static_cast<int>(narrow_str.size()), nullptr, 0);
        result.resize(static_cast<size_t>(size_needed), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, narrow_str.data(), static_cast<int>(narrow_str.size()), result.data(),
                            static_cast<int>(size_needed));
#else
        auto conv_desc = iconv_open("WCHAR_T", "UTF-8");
        if (conv_desc == bit_cast<iconv_t>(-1ll))
        {
            perror("iconv_open failed");
            return result;
        }

        auto in_bytes = narrow_str.size();
        const auto out_bytes = in_bytes * sizeof(wchar_t);
        result.resize(out_bytes / sizeof(wchar_t), L'\0');

        char* in_buf = const_cast<char*>(narrow_str.data());
        char* out_buf = reinterpret_cast<char*>(result.data());

        auto bytes_left = out_bytes;

        const auto res = iconv(conv_desc, &in_buf, &in_bytes, &out_buf, &bytes_left);
        if (res == static_cast<size_t>(-1))
        {
            perror("iconv failed");
            iconv_close(conv_desc);
            return wstring();
        }

        iconv_close(conv_desc);
        result.resize((out_bytes - bytes_left) / sizeof(wchar_t));
#endif
        return result;
    }
} // namespace tempest
