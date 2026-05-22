#ifndef tempest_core_atomic_hpp
#define tempest_core_atomic_hpp

#include <climits>
#include <tempest/api.hpp>
#include <tempest/concepts.hpp>
#include <tempest/enum.hpp>
#include <tempest/int.hpp>
#include <tempest/type_traits.hpp>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h> // Interlocked* functions

#ifdef __clang__
#define DISABLE_DEPRECATION_WARNINGS \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#define RESTORE_DEPRECATION_WARNINGS _Pragma("clang diagnostic pop")
#else
#define DISABLE_DEPRECATION_WARNINGS \
    __pragma(warning(push)) \
    __pragma(warning(disable : 4996))
#define RESTORE_DEPRECATION_WARNINGS __pragma(warning(pop))
#endif

#define TEMPEST_COMPILER_BARRIER() DISABLE_DEPRECATION_WARNINGS \
    _ReadWriteBarrier() \
    RESTORE_DEPRECATION_WARNINGS

#elif defined(TEMPEST_PLATFORM_LINUX)
#include <linux/futex.h>
#include <sys/syscall.h>
#else
#error "Unsupported platform"
#endif

namespace tempest
{
    enum class memory_order : uint8_t
    {
        relaxed,
        acquire,
        release,
        acq_rel,
        seq_cst,
    };

    namespace detail
    {
        inline void validate_memory_order([[maybe_unused]] memory_order order)
        {
        }

        inline auto find_stronger_order(memory_order lhs, memory_order rhs) -> memory_order
        {
            // NOLINTNEXTLINE
            static constexpr memory_order combined[5][5] = {
                {
                    memory_order::relaxed,
                    memory_order::acquire,
                    memory_order::release,
                    memory_order::acq_rel,
                    memory_order::seq_cst,
                },
                {
                    memory_order::acquire,
                    memory_order::acquire,
                    memory_order::acq_rel,
                    memory_order::acq_rel,
                    memory_order::seq_cst,
                },
                {
                    memory_order::release,
                    memory_order::acq_rel,
                    memory_order::release,
                    memory_order::acq_rel,
                    memory_order::seq_cst,
                },
                {
                    memory_order::acq_rel,
                    memory_order::acq_rel,
                    memory_order::acq_rel,
                    memory_order::acq_rel,
                    memory_order::seq_cst,
                },
                {
                    memory_order::seq_cst,
                    memory_order::seq_cst,
                    memory_order::seq_cst,
                    memory_order::seq_cst,
                    memory_order::seq_cst,
                },
            };

            return combined[to_underlying(lhs)][to_underlying(rhs)];
        }

        template <integral I, typename T>
        [[nodiscard]] auto address_as_atomic(T& src) noexcept -> volatile I*
        {
            return &reinterpret_cast<volatile I&>(src);
        }

        template <integral I, typename T>
        [[nodiscard]] auto address_as_atomic(const T& src) noexcept -> const volatile I*
        {
            return &reinterpret_cast<const volatile I&>(src);
        }

        template <typename T, size_t>
        struct atomic_storage;

#ifdef _WIN32

        [[nodiscard]] inline auto negate(integral auto value)
        {
            using type = decltype(value);
            using unsigned_type = make_unsigned_t<type>;
            return static_cast<type>(unsigned_type{0} - static_cast<unsigned_type>(value));
        }

        [[nodiscard]] inline auto atomic_wait_direct(const void* const storage, void* const comparand, const size_t size,
                                const uint32_t timeout) noexcept -> int
        {
            const auto result = WaitOnAddress(const_cast<volatile void*>(storage), comparand, size, timeout);
            return result;
        }

        template <typename T>
        struct atomic_storage<T, 1>
        {
            using type = remove_cvref_t<T>;

            void store(const type& value) noexcept
            {
                auto* mem = address_as_atomic<char>(storage);
                const auto as_bytes = bit_cast<int8_t>(value);
                _InterlockedExchange8(mem, as_bytes);
            }

            void store(const type& value, memory_order order) noexcept
            {
                validate_memory_order(order);

                auto* const mem = address_as_atomic<char>(storage);
                const auto as_bytes = bit_cast<int8_t>(value);

                if (order == memory_order::relaxed)
                {
                    __iso_volatile_store8(mem, as_bytes);
                }
                else if (order == memory_order::release)
                {
                    TEMPEST_COMPILER_BARRIER();
#ifdef __clang__
                    __atomic_store_n(reinterpret_cast<volatile uint8_t*>(mem), static_cast<uint8_t>(as_bytes),
                                     __ATOMIC_RELEASE);
#else
                    __iso_volatile_store8(mem, as_bytes);
#endif
                }
                else
                {
                    store(value);
                }
            }

            [[nodiscard]] auto load(const memory_order order = memory_order::seq_cst) const noexcept -> type
            {
                validate_memory_order(order);

                auto* mem = address_as_atomic<char>(storage);
                auto as_bytes = __iso_volatile_load8(mem);
                if (order != memory_order::relaxed)
                {
                    TEMPEST_COMPILER_BARRIER();
                }
                return reinterpret_cast<type&>(as_bytes);
            }

            [[nodiscard]] auto exchange(const type& desired, const memory_order order) noexcept -> type
            {
                validate_memory_order(order);

                auto result = _InterlockedExchange8(address_as_atomic<char>(storage), bit_cast<int8_t>(desired));
                return reinterpret_cast<type&>(result);
            }

            [[nodiscard]] auto compare_exchange_strong(type& expected, const type desired,
                                                       const memory_order order) noexcept -> bool
            {
                validate_memory_order(order);

                const auto expected_bytes = bit_cast<int8_t>(expected);
                auto prev_bytes = _InterlockedCompareExchange8(address_as_atomic<char>(storage),
                                                               bit_cast<int8_t>(desired), expected_bytes);
                if (prev_bytes == expected_bytes)
                {
                    return true;
                }

                reinterpret_cast<int8_t&>(expected) = prev_bytes;
                return false;
            }

            auto wait(type expected, const memory_order order = memory_order::seq_cst) const noexcept -> void
            {
                validate_memory_order(order);

                for (;;)
                {
                    const auto observed = load(order);
                    if (observed != expected)
                    {
                        return;
                    }

                    atomic_wait_direct(&storage, &expected, sizeof(type), INFINITE);
                }
            }

            auto notify_one() noexcept -> void
            {
                WakeByAddressSingle(&storage);
            }

            auto notify_all() noexcept -> void
            {
                WakeByAddressAll(&storage);
            }

            [[nodiscard]] auto fetch_add(type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedExchangeAdd8(address_as_atomic<char>(storage), bit_cast<int8_t>(operand));
                return static_cast<type>(result);
            }

            [[nodiscard]] auto fetch_sub(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                return fetch_add(negate(operand), order);
            }

            [[nodiscard]] auto fetch_and(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedAnd8(address_as_atomic<char>(storage), bit_cast<int8_t>(operand));
                return static_cast<type>(result);
            }

            [[nodiscard]] auto fetch_or(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedOr8(address_as_atomic<char>(storage), bit_cast<int8_t>(operand));
                return static_cast<type>(result);
            }

            [[nodiscard]] auto fetch_xor(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedXor8(address_as_atomic<char>(storage), bit_cast<int8_t>(operand));
                return static_cast<type>(result);
            }

            type storage;
        };

        template <typename T>
        struct atomic_storage<T, 2>
        {
            using type = remove_cvref_t<T>;

            void store(const type& value) noexcept
            {
                auto* const mem = address_as_atomic<int16_t>(storage);
                const auto as_bytes = bit_cast<int16_t>(value);
                _InterlockedExchange16(mem, as_bytes);
            }

            void store(const type& value, memory_order order) noexcept
            {
                validate_memory_order(order);

                auto* const mem = address_as_atomic<int16_t>(storage);
                const auto as_bytes = bit_cast<int16_t>(value);

                if (order == memory_order::relaxed)
                {
                    __iso_volatile_store16(mem, as_bytes);
                }
                else if (order == memory_order::release)
                {
                    TEMPEST_COMPILER_BARRIER();
#ifdef __clang__
                    __atomic_store_n(reinterpret_cast<volatile uint16_t*>(mem), static_cast<uint16_t>(as_bytes),
                                     __ATOMIC_RELEASE);
#else
                    __iso_volatile_store16(mem, as_bytes);
#endif
                }
                else
                {
                    store(value);
                }
            }

            [[nodiscard]] auto load(const memory_order order = memory_order::seq_cst) const noexcept -> type
            {
                validate_memory_order(order);

                const auto* const mem = address_as_atomic<int16_t>(storage);
                auto as_bytes = __iso_volatile_load16(mem);
                if (order != memory_order::relaxed)
                {
                    TEMPEST_COMPILER_BARRIER();
                }
                return reinterpret_cast<type&>(as_bytes);
            }

            [[nodiscard]] auto exchange(const type& desired, const memory_order order) noexcept -> type
            {
                validate_memory_order(order);

                auto result = _InterlockedExchange16(address_as_atomic<int16_t>(storage), bit_cast<int16_t>(desired));
                return reinterpret_cast<type&>(result);
            }

            [[nodiscard]] auto compare_exchange_strong(type& expected, const type desired,
                                                       const memory_order order) noexcept -> bool
            {
                validate_memory_order(order);

                const auto expected_bytes = bit_cast<int16_t>(expected);
                auto prev_bytes = _InterlockedCompareExchange16(address_as_atomic<int16_t>(storage),
                                                                bit_cast<int16_t>(desired), expected_bytes);
                if (prev_bytes == expected_bytes)
                {
                    return true;
                }

                memcpy(&expected, &prev_bytes, sizeof(type));
                return false;
            }

            auto wait(type expected, const memory_order order = memory_order::seq_cst) const noexcept -> void
            {
                validate_memory_order(order);

                for (;;)
                {
                    const auto observed = load(order);
                    if (observed != expected)
                    {
                        return;
                    }

                    atomic_wait_direct(&storage, &expected, sizeof(type), INFINITE);
                }
            }

            auto notify_one() noexcept -> void
            {
                WakeByAddressSingle(&storage);
            }

            auto notify_all() noexcept -> void
            {
                WakeByAddressAll(&storage);
            }

            [[nodiscard]] auto fetch_add(type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedExchangeAdd16(address_as_atomic<int16_t>(storage), bit_cast<int16_t>(operand));
                return static_cast<type>(result);
            }

            [[nodiscard]] auto fetch_sub(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                return fetch_add(negate(operand), order);
            }

            [[nodiscard]] auto fetch_and(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedAnd16(address_as_atomic<int16_t>(storage), bit_cast<int16_t>(operand));
                return static_cast<type>(result);
            }

            [[nodiscard]] auto fetch_or(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedOr16(address_as_atomic<int16_t>(storage), bit_cast<int16_t>(operand));
                return static_cast<type>(result);
            }

            [[nodiscard]] auto fetch_xor(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedXor16(address_as_atomic<int16_t>(storage), bit_cast<int16_t>(operand));
                return static_cast<type>(result);
            }

            type storage;
        };

        template <typename T>
        struct atomic_storage<T, 4>
        {
            using type = remove_cvref_t<T>;

            void store(const type& value) noexcept
            {
                auto* const mem = address_as_atomic<long>(storage);
                const auto as_bytes = bit_cast<int32_t>(value);
                _InterlockedExchange(mem, as_bytes);
            }

            void store(const type& value, memory_order order) noexcept
            {
                validate_memory_order(order);

                auto* const mem = address_as_atomic<int32_t>(storage);
                const auto as_bytes = bit_cast<int32_t>(value);

                if (order == memory_order::relaxed)
                {
                    __iso_volatile_store32(mem, as_bytes);
                }
                else if (order == memory_order::release)
                {
                    TEMPEST_COMPILER_BARRIER();
#ifdef __clang__
                    __atomic_store_n(reinterpret_cast<volatile uint32_t*>(mem), static_cast<uint32_t>(as_bytes),
                                     __ATOMIC_RELEASE);
#else
                    __iso_volatile_store32(mem, as_bytes);
#endif
                }
                else
                {
                    store(value);
                }
            }

            [[nodiscard]] auto load(const memory_order order = memory_order::seq_cst) const noexcept -> type
            {
                validate_memory_order(order);

                const auto* const mem = address_as_atomic<int32_t>(storage);
                auto as_bytes = __iso_volatile_load32(mem);
                if (order != memory_order::relaxed)
                {
                    TEMPEST_COMPILER_BARRIER();
                }
                return reinterpret_cast<type&>(as_bytes);
            }

            [[nodiscard]] auto exchange(const type& desired, const memory_order order) noexcept -> type
            {
                validate_memory_order(order);

                auto result = _InterlockedExchange(address_as_atomic<long>(storage), bit_cast<long>(desired));
                return reinterpret_cast<type&>(result);
            }

            [[nodiscard]] auto compare_exchange_strong(type& expected, const type desired,
                                                       const memory_order order) noexcept -> bool
            {
                validate_memory_order(order);

                const auto expected_bytes = bit_cast<long>(expected);
                auto prev_bytes = _InterlockedCompareExchange(address_as_atomic<long>(storage), bit_cast<long>(desired),
                                                              expected_bytes);
                if (prev_bytes == expected_bytes)
                {
                    return true;
                }

                memcpy(&expected, &prev_bytes, sizeof(type));
                return false;
            }

            auto wait(type expected, const memory_order order = memory_order::seq_cst) const noexcept -> void
            {
                validate_memory_order(order);

                for (;;)
                {
                    const auto observed = load(order);
                    if (observed != expected)
                    {
                        return;
                    }

                    atomic_wait_direct(&storage, &expected, sizeof(type), INFINITE);
                }
            }

            auto notify_one() noexcept -> void
            {
                WakeByAddressSingle(&storage);
            }

            auto notify_all() noexcept -> void
            {
                WakeByAddressAll(&storage);
            }

            [[nodiscard]] auto fetch_add(type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedExchangeAdd(address_as_atomic<long>(storage), bit_cast<long>(operand));
                return static_cast<type>(result);
            }

            [[nodiscard]] auto fetch_sub(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                return fetch_add(negate(operand), order);
            }

            [[nodiscard]] auto fetch_and(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedAnd(address_as_atomic<long>(storage), bit_cast<long>(operand));
                return static_cast<type>(result);
            }

            [[nodiscard]] auto fetch_or(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedOr(address_as_atomic<long>(storage), bit_cast<long>(operand));
                return static_cast<type>(result);
            }

            [[nodiscard]] auto fetch_xor(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedXor(address_as_atomic<long>(storage), bit_cast<long>(operand));
                return static_cast<type>(result);
            }

            type storage;
        };

        template <typename T>
        struct atomic_storage<T, 8> // NOLINT - We are lockfree, because we only support 64-bit architectures, so 64-bit
                                    // atomics are always lockfree
        {
            using type = remove_cvref_t<T>;

            void store(const type& value) noexcept
            {
                auto* const mem = address_as_atomic<int64_t>(storage);
                const auto as_bytes = bit_cast<int64_t>(value);
                _InterlockedExchange64(mem, as_bytes);
            }

            void store(const type& value, memory_order order) noexcept
            {
                validate_memory_order(order);

                auto* const mem = address_as_atomic<int64_t>(storage);
                const auto as_bytes = bit_cast<int64_t>(value);

                if (order == memory_order::relaxed)
                {
                    __iso_volatile_store64(mem, as_bytes);
                }
                else if (order == memory_order::release)
                {
                    TEMPEST_COMPILER_BARRIER();
#ifdef __clang__
                    __atomic_store_n(reinterpret_cast<volatile uint64_t*>(mem), static_cast<uint64_t>(as_bytes),
                                     __ATOMIC_RELEASE);
#else
                    __iso_volatile_store64(mem, as_bytes);
#endif
                }
                else
                {
                    store(value);
                }
            }

            [[nodiscard]] auto load(const memory_order order = memory_order::seq_cst) const noexcept -> type
            {
                validate_memory_order(order);

                const auto* const mem = address_as_atomic<int64_t>(storage);
                auto as_bytes = __iso_volatile_load64(mem);
                if (order != memory_order::relaxed)
                {
                    TEMPEST_COMPILER_BARRIER();
                }
                return reinterpret_cast<type&>(as_bytes);
            }

            [[nodiscard]] auto exchange(const type& desired, const memory_order order) noexcept -> type
            {
                validate_memory_order(order);

                auto result = _InterlockedExchange64(address_as_atomic<int64_t>(storage), bit_cast<int64_t>(desired));
                return reinterpret_cast<type&>(result);
            }

            [[nodiscard]] auto compare_exchange_strong(type& expected, const type desired,
                                                       const memory_order order) noexcept -> bool
            {
                validate_memory_order(order);

                const auto expected_bytes = bit_cast<int64_t>(expected);
                auto prev_bytes = _InterlockedCompareExchange64(address_as_atomic<int64_t>(storage),
                                                                bit_cast<int64_t>(desired), expected_bytes);
                if (prev_bytes == expected_bytes)
                {
                    return true;
                }

                memcpy(&expected, &prev_bytes, sizeof(type));
                return false;
            }

            auto wait(type expected, const memory_order order = memory_order::seq_cst) const noexcept -> void
            {
                validate_memory_order(order);

                for (;;)
                {
                    const auto observed = load(order);
                    if (observed != expected)
                    {
                        return;
                    }

                    atomic_wait_direct(&storage, &expected, sizeof(type), INFINITE);
                }
            }

            auto notify_one() noexcept -> void
            {
                WakeByAddressSingle(&storage);
            }

            auto notify_all() noexcept -> void
            {
                WakeByAddressAll(&storage);
            }

            [[nodiscard]] auto fetch_add(type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedExchangeAdd64(address_as_atomic<int64_t>(storage), bit_cast<int64_t>(operand));
                return reinterpret_cast<type&>(result);
            }

            [[nodiscard]] auto fetch_sub(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                const auto neg = negate(operand);
                return fetch_add(neg, order);
            }

            [[nodiscard]] auto fetch_and(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedAnd64(address_as_atomic<int64_t>(storage), bit_cast<int64_t>(operand));
                return reinterpret_cast<type&>(result);
            }

            [[nodiscard]] auto fetch_or(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedOr64(address_as_atomic<int64_t>(storage), bit_cast<int64_t>(operand));
                return reinterpret_cast<type&>(result);
            }

            [[nodiscard]] auto fetch_xor(const type operand, const memory_order order) noexcept -> type
                requires integral<type>
            {
                validate_memory_order(order);

                auto result = _InterlockedXor64(address_as_atomic<int64_t>(storage), bit_cast<int64_t>(operand));
                return reinterpret_cast<type&>(result);
            }

            type storage;
        };
#elif defined(__linux__) || defined (__APPLE__)

        inline auto convert_memory_order(const memory_order order) -> int
        {
            switch (order)
            {
            case memory_order::relaxed:
                return __ATOMIC_RELAXED;
            case memory_order::acquire:
                return __ATOMIC_ACQUIRE;
            case memory_order::release:
                return __ATOMIC_RELEASE;
            case memory_order::acq_rel:
                return __ATOMIC_ACQ_REL;
            case memory_order::seq_cst:
                return __ATOMIC_SEQ_CST;
            default:
                return __ATOMIC_SEQ_CST;
            }
        }

        auto get_proxy_futex(const void* addr) -> int32_t*;

        template <typename T>
        void wait_on_address(const T* addr, T expected, memory_order order) noexcept {
            // If T is 4 bytes, we can use futex directly on the address. Otherwise, we need to use a separate futex word.
            if constexpr (sizeof(T) == 4) {
                syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, expected, nullptr, nullptr, 0);
            }

            auto* proxy_futex = get_proxy_futex(addr); // snag the separate futex word for this address
            while (true) {
                auto futex_version = __atomic_load_n(proxy_futex, convert_memory_order(order));
                if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected) {
                    return;
                }

                syscall(SYS_futex, proxy_futex, FUTEX_WAIT_PRIVATE, futex_version, nullptr, nullptr, 0);
            }
        }

        template <typename T>
        auto wake_by_address_all(T* addr, memory_order order) -> void {
            if constexpr (sizeof(T) == 4) {
                syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr, nullptr, 0);
                return;
            }

            auto* proxy_futex = get_proxy_futex(addr);
            __atomic_fetch_add(proxy_futex, 1, convert_memory_order(order));
            syscall(SYS_futex, proxy_futex, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr, nullptr, 0);
        }

        template <typename T>
        auto wake_by_address_single(T* addr, memory_order order) -> void {
            if constexpr (sizeof(T) == 4) {
                syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
                return;
            }

            auto* proxy_futex = get_proxy_futex(addr);
            __atomic_fetch_add(proxy_futex, 1, convert_memory_order(order));
            syscall(SYS_futex, proxy_futex, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr, nullptr, 0); // We still wake MAX threads, since multiple threads could be waiting on the same shadow futex
        }

        template <typename T>
        struct atomic_storage<T, sizeof(T)>
        {
            using type = remove_cvref_t<T>;

            auto store(const type& value, memory_order order = memory_order::seq_cst) noexcept -> void
            {
                __atomic_store_n(&storage, value, convert_memory_order(order));
            }

            [[nodiscard]] auto load(memory_order order = memory_order::seq_cst) const noexcept -> type
            {
                return __atomic_load_n(&storage, convert_memory_order(order));
            }

            [[nodiscard]] auto exchange(const type& desired, memory_order order = memory_order::seq_cst) noexcept -> type
            {
                return __atomic_exchange_n(&storage, desired, convert_memory_order(order));
            }

            [[nodiscard]] auto compare_exchange_strong(type& expected, const type desired,
                                                       memory_order order = memory_order::seq_cst) noexcept -> bool
            {
                return __atomic_compare_exchange_n(&storage, &expected, desired, false, convert_memory_order(order),
                                                 convert_memory_order(order));
            }

            auto fetch_add(type operand, memory_order order = memory_order::seq_cst) noexcept -> type
                requires integral<type>
            {
                return __atomic_fetch_add(&storage, operand, convert_memory_order(order));
            }

            auto fetch_sub(type operand, memory_order order = memory_order::seq_cst) noexcept -> type
                requires integral<type>
            {
                return __atomic_fetch_sub(&storage, operand, convert_memory_order(order));
            }

            auto fetch_and(type operand, memory_order order = memory_order::seq_cst) noexcept -> type
                requires integral<type>
            {
                return __atomic_fetch_and(&storage, operand, convert_memory_order(order));
            }

            auto fetch_or(type operand, memory_order order = memory_order::seq_cst) noexcept -> type
                requires integral<type>
            {
                return __atomic_fetch_or(&storage, operand, convert_memory_order(order));
            }

            auto fetch_xor(type operand, memory_order order = memory_order::seq_cst) noexcept -> type
                requires integral<type>
            {
                return __atomic_fetch_xor(&storage, operand, convert_memory_order(order));
            }

            auto wait(const type expected, memory_order order = memory_order::seq_cst) const noexcept -> void
            {
                wait_on_address(&storage, expected, order);
            }

            auto notify_one() noexcept -> void
            {
                wake_by_address_single(&storage, memory_order::release);
            }

            auto notify_all() noexcept -> void
            {
                wake_by_address_all(&storage, memory_order::release);
            }

            type storage;
        };

#endif
    } // namespace detail

    template <typename T>
    class atomic;

    template <typename T>
        requires is_trivially_copyable_v<T> && is_copy_constructible_v<T> && is_copy_assignable_v<T> &&
                 is_move_constructible_v<T> && is_move_assignable_v<T> && is_same_v<T, remove_cvref_t<T>>
    class atomic<T>
    {
      public:
        using value_type = T;
        using difference_type = decltype(declval<T>() - declval<T>());

        constexpr atomic() noexcept(is_nothrow_default_constructible_v<T>) : _value()
        {
        }

        constexpr atomic(T desired) noexcept : _value(desired)
        {
        }

        atomic(const atomic&) = delete;
        atomic(atomic&&) = delete;

        ~atomic() = default;

        T& operator=(T desired) noexcept // NOLINT
        {
            store(desired);
            return desired;
        }

        T& operator=(T desired) volatile noexcept // NOLINT
        {
            store(desired);
            return desired;
        }

        atomic& operator=(const atomic&) = delete;
        atomic& operator=(const atomic&) volatile = delete;
        atomic& operator=(atomic&&) = delete;

        [[nodiscard]] auto is_lock_free() const noexcept -> bool
        {
            return true;
        }

        [[nodiscard]] auto is_lock_free() const volatile noexcept -> bool
        {
            return true;
        }

        auto store(T desired, memory_order order = memory_order::seq_cst) noexcept -> void
        {
            _value.store(desired, order);
        }

        auto store(T desired, memory_order order = memory_order::seq_cst) volatile noexcept -> void
        {
            _value.store(desired, order);
        }

        [[nodiscard]] auto load(memory_order order = memory_order::seq_cst) const noexcept -> T
        {
            return _value.load(order);
        }

        [[nodiscard]] auto load(memory_order order = memory_order::seq_cst) const volatile noexcept -> T
        {
            return _value.load(order);
        }

        [[nodiscard]] operator T() const noexcept
        {
            return load();
        }

        [[nodiscard]] operator T() const volatile noexcept
        {
            return load();
        }

        [[nodiscard]] auto exchange(T desired, memory_order order = memory_order::seq_cst) noexcept -> T
        {
            return _value.exchange(desired, order);
        }

        [[nodiscard]] auto exchange(T desired, memory_order order = memory_order::seq_cst) volatile noexcept -> T
        {
            return _value.exchange(desired, order);
        }

        [[nodiscard]] auto compare_exchange_weak(T& expected, T desired, memory_order success,
                                                 memory_order failure) noexcept -> bool
        {
            return compare_exchange_strong(expected, desired, success, failure);
        }

        [[nodiscard]] auto compare_exchange_weak(T& expected, T desired, memory_order success,
                                                 memory_order failure) volatile noexcept -> bool
        {
            return compare_exchange_strong(expected, desired, success, failure);
        }

        [[nodiscard]] auto compare_exchange_weak(T& expected, T desired,
                                                 memory_order order = memory_order::seq_cst) noexcept -> bool
        {
            return compare_exchange_strong(expected, desired, order);
        }

        [[nodiscard]] auto compare_exchange_weak(T& expected, T desired,
                                                 memory_order order = memory_order::seq_cst) volatile noexcept -> bool
        {
            return compare_exchange_strong(expected, desired, order);
        }

        [[nodiscard]] auto compare_exchange_strong(T& expected, T desired, memory_order success,
                                                   memory_order failure) noexcept -> bool
        {
            return _value.compare_exchange_strong(expected, desired, success,
                                                  detail::find_stronger_order(success, failure));
        }

        [[nodiscard]] auto compare_exchange_strong(T& expected, T desired, memory_order success,
                                                   memory_order failure) volatile noexcept -> bool
        {
            return _value.compare_exchange_strong(expected, desired, detail::find_stronger_order(success, failure));
        }

        [[nodiscard]] auto compare_exchange_strong(T& expected, T desired,
                                                   memory_order order = memory_order::seq_cst) noexcept -> bool
        {
            return _value.compare_exchange_strong(expected, desired, order);
        }

        [[nodiscard]] auto compare_exchange_strong(T& expected, T desired,
                                                   memory_order order = memory_order::seq_cst) volatile noexcept -> bool
        {
            return _value.compare_exchange_strong(expected, desired, order);
        }

        auto wait(T old, memory_order order = memory_order::seq_cst) const noexcept -> void
        {
            _value.wait(old, order);
        }

        auto wait(T old, memory_order order = memory_order::seq_cst) const volatile noexcept -> void
        {
            _value.wait(old, order);
        }

        auto notify_one() noexcept -> void
        {
            _value.notify_one();
        }

        auto notify_one() volatile noexcept -> void
        {
            _value.notify_one();
        }

        auto notify_all() noexcept -> void
        {
            _value.notify_all();
        }

        auto notify_all() volatile noexcept -> void
        {
            _value.notify_all();
        }

        [[nodiscard]] auto fetch_add(T arg, memory_order order = memory_order::seq_cst) noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return _value.fetch_add(arg, order);
        }

        [[nodiscard]] auto fetch_add(T arg, memory_order order = memory_order::seq_cst) volatile noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return _value.fetch_add(arg, order);
        }

        [[nodiscard]] auto operator+=(T arg) noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_add(arg);
        }

        [[nodiscard]] auto operator+=(T arg) volatile noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_add(arg);
        }

        [[nodiscard]] auto fetch_sub(T arg, memory_order order = memory_order::seq_cst) noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return _value.fetch_sub(arg, order);
        }

        [[nodiscard]] auto fetch_sub(T arg, memory_order order = memory_order::seq_cst) volatile noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return _value.fetch_sub(arg, order);
        }

        [[nodiscard]] auto operator-=(T arg) noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_sub(arg);
        }

        [[nodiscard]] auto operator-=(T arg) volatile noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_sub(arg);
        }

        [[nodiscard]] auto operator++() noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_add(1) + 1;
        }

        [[nodiscard]] auto operator++() volatile noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_add(1) + 1;
        }

        [[nodiscard]] auto operator++(int) noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_add(1);
        }

        [[nodiscard]] auto operator++(int) volatile noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_add(1);
        }

        [[nodiscard]] auto operator--() noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_sub(1) - 1;
        }

        [[nodiscard]] auto operator--() volatile noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_sub(1) - 1;
        }

        [[nodiscard]] auto operator--(int) noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_sub(1);
        }

        [[nodiscard]] auto operator--(int) volatile noexcept -> T
            requires (integral<T> && !same_as<T, bool>)
        {
            return fetch_sub(1) - 1;
        }

        [[nodiscard]] auto fetch_and(T arg, memory_order order = memory_order::seq_cst) noexcept -> T
            requires integral<T>
        {
            return _value.fetch_and(arg, order);
        }

        [[nodiscard]] auto fetch_and(T arg, memory_order order = memory_order::seq_cst) volatile noexcept -> T
            requires integral<T>
        {
            return _value.fetch_and(arg, order);
        }

        [[nodiscard]] auto fetch_or(T arg, memory_order order = memory_order::seq_cst) noexcept -> T
            requires integral<T>
        {
            return _value.fetch_or(arg, order);
        }

        [[nodiscard]] auto fetch_or(T arg, memory_order order = memory_order::seq_cst) volatile noexcept -> T
            requires integral<T>
        {
            return _value.fetch_or(arg, order);
        }

        [[nodiscard]] auto fetch_xor(T arg, memory_order order = memory_order::seq_cst) noexcept -> T
            requires integral<T>
        {
            return _value.fetch_xor(arg, order);
        }

        [[nodiscard]] auto fetch_xor(T arg, memory_order order = memory_order::seq_cst) volatile noexcept -> T
            requires integral<T>
        {
            return _value.fetch_xor(arg, order);
        }

        [[nodiscard]] auto operator&=(T arg) noexcept -> T
            requires integral<T>
        {
            return fetch_and(arg);
        }

        [[nodiscard]] auto operator&=(T arg) volatile noexcept -> T
            requires integral<T>
        {
            return fetch_and(arg);
        }

        [[nodiscard]] auto operator|=(T arg) noexcept -> T
            requires integral<T>
        {
            return fetch_or(arg);
        }

        [[nodiscard]] auto operator|=(T arg) volatile noexcept -> T
            requires integral<T>
        {
            return fetch_or(arg);
        }

        [[nodiscard]] auto operator^=(T arg) noexcept -> T
            requires integral<T>
        {
            return fetch_xor(arg);
        }

        [[nodiscard]] auto operator^=(T arg) volatile noexcept -> T
            requires integral<T>
        {
            return fetch_xor(arg);
        }

      private:
        detail::atomic_storage<T, sizeof(T)> _value;
    };
} // namespace tempest

#endif // tempest_core_atomic_hpp
