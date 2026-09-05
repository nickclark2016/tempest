#ifndef tempest_core_hash_hpp
#define tempest_core_hash_hpp

#include <tempest/api.hpp>
#include <tempest/bit.hpp>
#include <tempest/int.hpp>

namespace tempest
{
    namespace detail
    {
        /// @brief Uniform distributed hash function for the 64 bit signed integer type. The constants are derived from
        ///        the splitmix64 hashing mechanism.
        /// @param v Value to hash
        /// @return Hashed value
        TEMPEST_API
        inline auto i64_hash(int64_t v) noexcept -> int64_t
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
        TEMPEST_API
        inline auto u64_hash(uint64_t v) noexcept -> uint64_t
        {
            v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9;
            v = (v ^ (v >> 27)) * 0x94d049bb133111eb;
            v = v ^ (v >> 31);
            return v;
        }

        TEMPEST_API
        inline auto i8_hash(int8_t v) noexcept -> int64_t
        {
            auto full_hash = i64_hash(v);
            auto masked_hash = full_hash & 0x1FFFFFFFFFFFFFF; // mask off the upper 7 bits
            auto input_mask = static_cast<int64_t>(v) & 0x7F; // mask off the upper 1 bit
            auto shifted_input = input_mask << 57;            // shift the input to the upper 7 bits
            return masked_hash | shifted_input;
        }

        TEMPEST_API
        inline auto u8_hash(uint8_t v) noexcept -> uint64_t
        {
            auto full_hash = u64_hash(v);
            auto masked_hash = full_hash & 0x1FFFFFFFFFFFFFF;  // mask off the upper 7 bits
            auto input_mask = static_cast<uint64_t>(v) & 0x7F; // mask off the upper 1 bit
            auto shifted_input = input_mask << 57;             // shift the input to the upper 7 bits
            return masked_hash | shifted_input;
        }

        TEMPEST_API
        inline auto i16_hash(int16_t v) noexcept -> int64_t
        {
            return i64_hash(v);
        }

        TEMPEST_API
        inline auto u16_hash(uint16_t v) noexcept -> uint64_t
        {
            return u64_hash(v);
        }

        TEMPEST_API
        inline auto i32_hash(int32_t v) noexcept -> int64_t
        {
            return i64_hash(v);
        }

        TEMPEST_API
        inline auto u32_hash(uint32_t v) noexcept -> uint64_t
        {
            return u64_hash(v);
        }

        template <typename T>
        inline auto fnv1a32(const T* data, size_t sz) noexcept -> uint32_t
        {
            constexpr uint32_t fnv_offset = 2166136261U;
            constexpr uint32_t fnv_prime = 16777619U;

            uint32_t hash = fnv_offset;
            for (size_t i = 0; i < sz; ++i)
            {
                hash = hash ^ static_cast<uint32_t>(data[i]);
                hash = hash * fnv_prime;
            }

            return hash;
        }

        template <typename T>
        inline auto fnv1a64(const T* data, size_t sz) noexcept -> uint64_t
        {
            constexpr uint64_t fnv_offset = 14695981039346656037ULL;
            constexpr uint64_t fnv_prime = 1099511628211ULL;

            uint64_t hash = fnv_offset;
            for (size_t i = 0; i < sz; ++i)
            {
                hash = hash ^ static_cast<uint64_t>(data[i]);
                hash = hash * fnv_prime;
            }

            return hash;
        }

        template <typename T>
        inline auto fnv1a_auto(const T* data, size_t sz) noexcept -> size_t
        {
            if constexpr (sizeof(size_t) == 4)
            {
                return fnv1a32(data, sz);
            }
            else
            {
                return fnv1a64(data, sz);
            }
        }
    } // namespace detail

    /// @brief Templated hash function.
    /// @tparam T type of the key to hash.
    ///
    /// The implementation of this struct must match the signature of hash.  For optimal performance of flat
    /// unordered hash tables in tempest, the hash function should distribute bits uniformly across the size_t
    /// range. The upper 7 bits of the hash should be evenly distributed, as this is used for metadata tests, and more
    /// collisions will result in the need for more tests on the lower hash bits.
    template <typename T>
    struct hash;

    /// @brief Specialization of the hash function for the 8 bit signed integer type.
    template <>
    struct hash<int8_t>
    {
        auto operator()(int8_t key) const noexcept -> size_t
        {
            return static_cast<size_t>(detail::i8_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 8 bit unsigned integer type.
    template <>
    struct hash<uint8_t>
    {
        auto operator()(uint8_t key) const noexcept -> size_t
        {
            return static_cast<size_t>(detail::u8_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 16 bit signed integer type.
    template <>
    struct hash<int16_t>
    {
        auto operator()(int16_t key) const noexcept -> size_t
        {
            return static_cast<size_t>(detail::i16_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 16 bit unsigned integer type.
    template <>
    struct hash<uint16_t>
    {
        auto operator()(uint16_t key) const noexcept -> size_t
        {
            return static_cast<size_t>(detail::u16_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 32 bit signed integer type.
    template <>
    struct hash<int32_t>
    {
        auto operator()(int32_t key) const noexcept -> size_t
        {
            return static_cast<size_t>(detail::i32_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 32 bit unsigned integer type.
    template <>
    struct hash<uint32_t>
    {
        auto operator()(uint32_t key) const noexcept -> size_t
        {
            return static_cast<size_t>(detail::u32_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 64 bit signed integer type.
    template <>
    struct hash<int64_t>
    {
        auto operator()(int64_t key) const noexcept -> size_t
        {
            return static_cast<size_t>(detail::i64_hash(key));
        }
    };

    /// @brief Specialization of the hash function for the 64 bit unsigned integer type.
    template <>
    struct hash<uint64_t>
    {
        auto operator()(uint64_t key) const noexcept -> size_t
        {
            return static_cast<size_t>(detail::u64_hash(key));
        }
    };

    /// @brief Specialization of the hash function for single precision floating point type.
    template <>
    struct hash<float>
    {
        auto operator()(float key) const noexcept -> size_t
        {
            auto uint_bytes = bit_cast<uint32_t>(key);
            return static_cast<size_t>(detail::u32_hash(uint_bytes));
        }
    };

    /// @brief Specialization of the hash function for double precision floating point type.
    template <>
    struct hash<double>
    {
        auto operator()(double key) const noexcept -> size_t
        {
            auto uint_bytes = bit_cast<uint64_t>(key);
            return static_cast<size_t>(detail::u64_hash(uint_bytes));
        }
    };

    /// @brief Specialization of the hash function for pointer types.
    template <typename T>
    struct hash<T*>
    {
        auto operator()(T* key) const noexcept -> size_t
        {
            if constexpr (sizeof(T*) == 4)
            {
                return static_cast<size_t>(detail::u32_hash(bit_cast<uint32_t>(key)));
            }
            else
            {
                return static_cast<size_t>(detail::u64_hash(bit_cast<uint64_t>(key)));
            }
        }
    };

    template <typename T>
        requires is_enum_v<T>
    struct hash<T>
    {
        auto operator()(T key) const noexcept -> size_t
        {
            using underlying = underlying_type_t<T>;
            return hash<underlying>()(static_cast<underlying>(key));
        }
    };

    template <typename... Ts>
        requires(sizeof...(Ts) > 0)
    inline auto hash_combine(Ts... keys) noexcept -> size_t
    {
        size_t hv = 0;
        ((hv ^= hash<Ts>()(forward<Ts>(keys)) + 0x9e3779b9 + (hv << 6) + (hv >> 2)), ...);
        return hv;
    }
} // namespace tempest

#endif // tempest_core_hash_hpp