#ifndef tempest_core_hash_hpp
#define tempest_core_hash_hpp

#include <bit>
#include <cstddef>
#include <cstdint>

namespace tempest::core
{
    namespace detail
    {
        /// @brief Uniform distributed hash function for the 64 bit signed integer type. The constants are derived from
        ///        the splitmix64 hashing mechanism.
        /// @param v Value to hash
        /// @return Hashed value
        inline std::int64_t i64_hash(std::int64_t v) noexcept
        {
            v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9;
            v = (v ^ (v >> 27)) * 0x94d049bb133111eb;
            v = v ^ (v >> 31);
            return v;
        }

        /// @brief Uniform distributed hash function for the 64 bit unsigned integer type. The constants are derived
        ///        from the splitmix64 hashing mechanism.
        /// @param v Value to hash
        /// @return Hashed value
        inline std::uint64_t u64_hash(std::uint64_t v) noexcept
        {
            v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9;
            v = (v ^ (v >> 27)) * 0x94d049bb133111eb;
            v = v ^ (v >> 31);
            return v;
        }

        inline std::int64_t i8_hash(std::int8_t v) noexcept
        {
            auto full_hash = i64_hash(v);
            auto masked_hash = full_hash & 0x1FFFFFFFFFFFFFF;      // mask off the upper 7 bits
            auto input_mask = static_cast<std::int64_t>(v) & 0x7F; // mask off the upper 1 bit
            auto shifted_input = input_mask << 57;                 // shift the input to the upper 7 bits
            return masked_hash | shifted_input;
        }

        inline std::uint64_t u8_hash(std::uint8_t v) noexcept
        {
            auto full_hash = u64_hash(v);
            auto masked_hash = full_hash & 0x1FFFFFFFFFFFFFF;       // mask off the upper 7 bits
            auto input_mask = static_cast<std::uint64_t>(v) & 0x7F; // mask off the upper 1 bit
            auto shifted_input = input_mask << 57;                  // shift the input to the upper 7 bits
            return masked_hash | shifted_input;
        }

        inline std::int64_t i16_hash(std::int16_t v) noexcept
        {
            return i64_hash(v);
        }

        inline std::uint64_t u16_hash(std::uint16_t v) noexcept
        {
            return u64_hash(v);
        }

        inline std::int64_t i32_hash(std::int32_t v) noexcept
        {
            return i64_hash(v);
        }

        inline std::uint64_t u32_hash(std::uint32_t v) noexcept
        {
            return u64_hash(v);
        }
    } // namespace detail

    /// @brief Templated hash function.
    /// @tparam T type of the key to hash.
    ///
    /// The implementation of this struct must match the signature of std::hash.  For optimal performance of flat
    /// unordered hash tables in tempest, the hash function should distribute bits uniformly across the std::size_t
    /// range. The upper 7 bits of the hash should be evenly distributed, as this is used for metadata tests, and more
    /// collisions will result in the need for more tests on the lower hash bits.
    template <typename T>
    struct hash;

    /// @brief Specialization of the hash function for the 8 bit signed integer type.
    template <>
    struct hash<std::int8_t>
    {
        std::size_t operator()(std::int8_t key) const noexcept
        {
            return static_cast<std::size_t>(detail::i8_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 8 bit unsigned integer type.
    template <>
    struct hash<std::uint8_t>
    {
        std::size_t operator()(std::uint8_t key) const noexcept
        {
            return static_cast<std::size_t>(detail::u8_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 16 bit signed integer type.
    template <>
    struct hash<std::int16_t>
    {
        std::size_t operator()(std::int16_t key) const noexcept
        {
            return static_cast<std::size_t>(detail::i16_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 16 bit unsigned integer type.
    template <>
    struct hash<std::uint16_t>
    {
        std::size_t operator()(std::uint16_t key) const noexcept
        {
            return static_cast<std::size_t>(detail::u16_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 32 bit signed integer type.
    template <>
    struct hash<std::int32_t>
    {
        std::size_t operator()(std::int32_t key) const noexcept
        {
            return static_cast<std::size_t>(detail::i32_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 32 bit unsigned integer type.
    template <>
    struct hash<std::uint32_t>
    {
        std::size_t operator()(std::uint32_t key) const noexcept
        {
            return static_cast<std::size_t>(detail::u32_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 64 bit signed integer type.
    template <>
    struct hash<std::int64_t>
    {
        std::size_t operator()(std::int64_t key) const noexcept
        {
            return static_cast<std::size_t>(detail::i64_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 64 bit unsigned integer type.
    template <>
    struct hash<std::uint64_t>
    {
        std::size_t operator()(std::uint64_t key) const noexcept
        {
            return static_cast<std::size_t>(detail::u64_hash(key));
        }
    };

    /// @brief Specialization of the hash function for single precision floating point type.
    template <>
    struct hash<float>
    {
        std::size_t operator()(float key) const noexcept
        {
            std::uint32_t uint_bytes = std::bit_cast<std::uint32_t>(key);
            return static_cast<std::size_t>(detail::u32_hash(uint_bytes));
        }
    };

    /// @brief Specialization of the hash function for double precision floating point type.
    template <>
    struct hash<double>
    {
        std::size_t operator()(double key) const noexcept
        {
            std::uint64_t uint_bytes = std::bit_cast<std::uint64_t>(key);
            return static_cast<std::size_t>(detail::u64_hash(uint_bytes));
        }
    };

    /// @brief Specialization of the hash function for pointer types.
    template <typename T>
    struct hash<T*>
    {
        std::size_t operator()(T* key) const noexcept
        {
            if constexpr (sizeof(T*) == 4)
            {
                return static_cast<std::size_t>(detail::u32_hash(std::bit_cast<std::uint32_t>(key)));
            }
            else
            {
                return static_cast<std::size_t>(detail::u64_hash(std::bit_cast<std::uint64_t>(key)));
            }
        }
    };
} // namespace tempest::core

#endif // tempest_core_hash_hpp