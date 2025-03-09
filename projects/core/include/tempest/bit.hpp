#ifndef tempest_core_bit_hpp
#define tempest_core_bit_hpp

#include <tempest/concepts.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/unreachable.hpp>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

namespace tempest
{
#if defined(_MSC_VER) && !defined(__clang__)
    enum class endian
    {
        little = 0,
        big = 1,
        native = little
    };

#else
    enum class endian
    {
        little = __ORDER_LITTLE_ENDIAN__,
        big = __ORDER_BIG_ENDIAN__,
        native = __BYTE_ORDER__
    };
#endif

    /// @brief Copies the bits from one object to another.
    /// @tparam To Type to copy to
    /// @tparam From Type to copy from
    /// @param from Object to copy from
    /// @return Object initialized with the bits from `from`
    template <typename To, typename From>
        requires(sizeof(To) == sizeof(From) && is_trivially_copyable_v<To> && is_trivially_copyable_v<From>)
    [[nodiscard]] inline constexpr To bit_cast(const From& from) noexcept
    {
        return __builtin_bit_cast(To, from);
    }

    template <typename T>
        requires(has_unique_object_representations_v<T> && is_integral_v<T>)
    [[nodiscard]] inline constexpr T byteswap(T n) noexcept
    {
        // runtime
        if (!is_constant_evaluated())
        {
#if defined(_MSC_VER) && !defined(__clang__)
            if constexpr (sizeof(T) == 1)
            {
                return n;
            }
            else if constexpr (sizeof(T) == 2)
            {
                return static_cast<T>(_byteswap_ushort(static_cast<unsigned short>(n)));
            }
            else if constexpr (sizeof(T) == 4)
            {
                return static_cast<T>(_byteswap_ulong(static_cast<unsigned long>(n)));
            }
            else if constexpr (sizeof(T) == 8)
            {
                return static_cast<T>(_byteswap_uint64(static_cast<unsigned long long>(n)));
            }
#else
            if constexpr (sizeof(T) == 1)
            {
                return n;
            }
            else if constexpr (sizeof(T) == 2)
            {
                return static_cast<T>(__builtin_bswap16(static_cast<uint16_t>(n)));
            }
            else if constexpr (sizeof(T) == 4)
            {
                return static_cast<T>(__builtin_bswap32(static_cast<uint32_t>(n)));
            }
            else if constexpr (sizeof(T) == 8)
            {
                return static_cast<T>(__builtin_bswap64(static_cast<uint64_t>(n)));
            }
#endif
        }

        using up = make_unsigned_t<remove_cv_t<T>>;

        size_t diff = 8 * (sizeof(T) - 1);

        up mask1 = static_cast<unsigned char>(~0);
        up mask2 = mask1 << diff;
        up val = n;

        for (size_t i = 0; i < sizeof(T) / 2; ++i)
        {
            up b1 = val & mask1;
            up b2 = val & mask2;

            val = (val ^ b1 ^ b2 ^ (b1 << diff) ^ (b2 >> diff));

            mask1 <<= 8; // CHAR_BIT
            mask2 >>= 8; // CHAR_BIT

            diff -= 16; // 2 * CHAR_BIT
        }

        return diff;
    }

    template <typename T>
        requires(is_integral_v<T> && is_unsigned_v<T> && !is_same_v<T, bool> && !is_same_v<T, char> &&
                 !is_same_v<T, wchar_t> && !is_same_v<T, char16_t> && !is_same_v<T, char32_t>)
    inline constexpr bool has_single_bit(T n) noexcept
    {
        return n && !(n & (n - 1));
    }

    template <unsigned_integral T>
    inline constexpr int countl_zero(T n) noexcept
    {
#if defined(_MSC_VER) && !defined(__clang__)
        constexpr auto width = sizeof(T) * 8;

        if (is_constant_evaluated())
        {
            // Count how many bits are set to zero from left to right before the
            // first one bit
            int result = 0;
            for (size_t i = 0; i < width; ++i)
            {
                if (n & (T(1) << (width - i - 1)))
                {
                    break;
                }
                ++result;
            }
            return result;
        }

        if (n == 0)
        {
            return width;
        }

        constexpr auto nd_ull = sizeof(unsigned long long) * 8;
        constexpr auto nd_ul = sizeof(unsigned long) * 8;

        if constexpr (width <= nd_ul)
        {
            constexpr auto diff = nd_ul - width;
            return static_cast<int>(__lzcnt(n) - diff);
        }
        else if constexpr (width <= nd_ull)
        {
            constexpr auto diff = nd_ull - width;
            return static_cast<int>(__lzcnt64(n) - diff);
        }
        else
        {
            // unreachable
            unreachable();
        }
#else
        constexpr auto width = sizeof(T) * 8;

        if (n == 0)
        {
            return width;
        }

        constexpr auto nd_ull = sizeof(unsigned long long) * 8;
        constexpr auto nd_ul = sizeof(unsigned long) * 8;
        constexpr auto nd_u = sizeof(unsigned) * 8;

        if constexpr (width <= nd_u)
        {
            constexpr int diff = nd_u - width;
            return __builtin_clz(n) - diff;
        }
        else if constexpr (width <= nd_ul)
        {
            constexpr int diff = nd_ul - width;
            return __builtin_clzl(n) - diff;
        }
        else if constexpr (width <= nd_ull)
        {
            constexpr int diff = nd_ull - width;
            return __builtin_clzll(n) - diff;
        }
        else
        {
            // unreachable
            unreachable();
        }
#endif
    }

    template <unsigned_integral T>
    inline constexpr int countl_one(T n) noexcept
    {
        return countl_zero(static_cast<T>(~n));
    }

    template <unsigned_integral T>
    inline constexpr int countr_zero(T n) noexcept
    {
#if defined(_MSC_VER) && !defined(__clang__)
        constexpr auto width = sizeof(T) * 8;

        if (is_constant_evaluated())
        {
            // Count how many bits are set to zero from right to left before the
            // first one bit
            int result = 0;

            for (size_t i = 0; i < width; ++i)
            {
                if (n & (T(1) << i))
                {
                    break;
                }
                ++result;
            }

            return result;
        }

        if (n == 0)
        {
            return width;
        }

        constexpr auto nd_ull = sizeof(unsigned long long) * 8;
        constexpr auto nd_ul = sizeof(unsigned long) * 8;

        if constexpr (width <= nd_ul)
        {
            unsigned long uint_val = n;
            unsigned long r;
            _BitScanForward(&r, uint_val);
            return static_cast<int>(r);
        }
        else if constexpr (width <= nd_ull)
        {
            unsigned long long uint_val = n;
            unsigned long r;
            _BitScanForward64(&r, uint_val);
            return static_cast<int>(r);
        }
        else
        {
            // unreachable
            unreachable();
        }
#else
        constexpr auto width = sizeof(T) * 8;

        if (n == 0)
        {
            return width;
        }

        constexpr auto nd_ull = sizeof(unsigned long long) * 8;
        constexpr auto nd_ul = sizeof(unsigned long) * 8;
        constexpr auto nd_u = sizeof(unsigned) * 8;

        if constexpr (width <= nd_u)
        {
            return __builtin_ctz(n);
        }
        else if constexpr (width <= nd_ul)
        {
            return __builtin_ctzl(n);
        }
        else if constexpr (width <= nd_ull)
        {
            return __builtin_ctzll(n);
        }
        else
        {
            // unreachable
            unreachable();
        }
#endif
    }

    template <unsigned_integral T>
    inline constexpr int countr_one(T n) noexcept
    {
        return countr_zero(static_cast<T>(~n));
    }

    template <unsigned_integral T>
    inline constexpr int popcount(T n) noexcept
    {
#if defined(_MSC_VER) && !defined(__clang__)
        if (is_constant_evaluated())
        {
            // Count number of bits set to one
            constexpr auto width = sizeof(T) * 8;
            int result = 0;

            for (size_t i = 0; i < width; ++i)
            {
                if (n & (T(1) << i))
                {
                    ++result;
                }
            }

            return result;
        }

        constexpr auto width = sizeof(T) * 8;
        constexpr auto nd_ull = sizeof(unsigned long long) * 8;
        constexpr auto nd_ul = sizeof(unsigned long) * 8;

        if constexpr (width <= nd_ul)
        {
            return __popcnt(n);
        }
        else if constexpr (width <= nd_ull)
        {
            return __popcnt64(n);
        }
        else
        {
            // unreachable
            unreachable();
        }
#else
        constexpr auto width = sizeof(T) * 8;
        constexpr auto nd_ull = sizeof(unsigned long long) * 8;
        constexpr auto nd_ul = sizeof(unsigned long) * 8;
        constexpr auto nd_u = sizeof(unsigned) * 8;

        if constexpr (width <= nd_u)
        {
            return __builtin_popcount(n);
        }
        else if constexpr (width <= nd_ul)
        {
            return __builtin_popcountl(n);
        }
        else if constexpr (width <= nd_ull)
        {
            return __builtin_popcountll(n);
        }
        else
        {
            // unreachable
            unreachable();
        }
#endif
    }

    template <unsigned_integral T>
    inline constexpr int bit_width(T n) noexcept
    {
        return (8 * sizeof(T)) - countl_zero(n);
    }

    template <unsigned_integral T>
    inline constexpr T bit_ceil(T n) noexcept
    {
        if (n <= 1u)
        {
            return T(1);
        }

        if constexpr (same_as<T, decltype(+n)>)
        {
            return T(1) << bit_width(T(n - 1));
        }
        else
        {
            // Case of promoted integral type
            constexpr int offset = static_cast<int>((sizeof(unsigned) * 8) - (sizeof(T) * 8));
            return T(1u << (bit_width(T(n - 1)) + offset) >> offset);
        }
    }

    template <unsigned_integral T>
    inline constexpr T bit_floor(T n) noexcept
    {
        if (n == 0)
            return 0;
        return T(1) << (bit_width(n) - 1);
    }
} // namespace tempest

#endif // tempest_core_bit_hpp