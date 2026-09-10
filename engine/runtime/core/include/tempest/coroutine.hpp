#ifndef tempest_coroutine_hpp
#define tempest_coroutine_hpp

#include <tempest/api.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    template <typename Result, typename... Args>
    struct coroutine_traits;

    namespace detail
    {
        template <typename Result, typename = void>
        struct coroutine_traits_impl;

        template <typename Result>
            requires requires { typename Result::promise_type; }
        struct coroutine_traits_impl<Result, void>
        {
            using promise_type = typename Result::promise_type;
        };
    } // namespace detail

    /// \brief A type trait that provides the promise type associated with a coroutine.
    /// \tparam Result The result type of the coroutine.
    /// \tparam Args The argument types of the coroutine.
    template <typename Result, typename... Args>
    struct coroutine_traits : detail::coroutine_traits_impl<Result>
    {
    };

    /// \brief A coroutine handle type that can be used to resume or destroy a coroutine.
    /// \tparam Promise The promise type associated with the coroutine. Defaults to void.
    template <typename Promise = void>
    struct coroutine_handle;

    template <>
    struct coroutine_handle<void>
    {
        constexpr coroutine_handle() noexcept : _frame_ptr(nullptr)
        {
        }

        constexpr coroutine_handle(nullptr_t nptr) noexcept : _frame_ptr(nptr)
        {
        }

        auto operator=(nullptr_t nptr) noexcept -> coroutine_handle&
        {
            _frame_ptr = nptr;
            return *this;
        }

        [[nodiscard]] constexpr auto address() const noexcept -> void*
        {
            return _frame_ptr;
        }

        [[nodiscard]] static constexpr auto from_address(void* addr) noexcept -> coroutine_handle
        {
            auto coro_handle = coroutine_handle{};
            coro_handle._frame_ptr = addr;
            return coro_handle;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return _frame_ptr != nullptr;
        }

        [[nodiscard]] auto done() const noexcept -> bool
        {
#if defined(TEMPEST_PLATFORM_WINDOWS)
            return _builtin_coro_done(_frame_ptr);
#elif defined(TEMPEST_PLATFORM_LINUX)
            return __builtin_coro_done(_frame_ptr);
#endif
        }

        auto resume() const -> void
        {
#if defined(TEMPEST_PLATFORM_WINDOWS)
            __builtin_coro_resume(_frame_ptr);
#elif defined(TEMPEST_PLATFORM_LINUX)
            __builtin_coro_resume(_frame_ptr);
#endif
        }

        auto operator()() const -> void
        {
            resume();
        }

        auto destroy() const -> void
        {
#if defined(TEMPEST_PLATFORM_WINDOWS)
            __builtin_coro_destroy(_frame_ptr);
#elif defined(TEMPEST_PLATFORM_LINUX)
            __builtin_coro_destroy(_frame_ptr);
#endif
        }

      private:
        void* _frame_ptr;
    };

    [[nodiscard]] constexpr auto operator==(const coroutine_handle<void>& lhs,
                                            const coroutine_handle<void>& rhs) noexcept -> bool
    {
        return lhs.address() == rhs.address();
    }

    [[nodiscard]] constexpr auto operator!=(const coroutine_handle<void>& lhs,
                                            const coroutine_handle<void>& rhs) noexcept -> bool
    {
        return !(lhs == rhs);
    }

    template <typename Promise>
    struct coroutine_handle
    {
        constexpr coroutine_handle() noexcept : _frame_ptr(nullptr)
        {
        }

        constexpr coroutine_handle(nullptr_t nptr) noexcept : _frame_ptr(nptr)
        {
        }

        [[nodiscard]] static auto from_promise(Promise& prom) -> coroutine_handle
        {
            auto* const prom_ptr = const_cast<void*>(static_cast<const volatile void*>(&prom));
            auto* const frame_ptr = __builtin_coro_promise(prom_ptr, alignof(Promise), true);
            auto coro_handle = coroutine_handle{};
            coro_handle._frame_ptr = frame_ptr;
            return coro_handle;
        }

        auto operator=(nullptr_t nptr) noexcept -> coroutine_handle&
        {
            _frame_ptr = nptr;
            return *this;
        }

        [[nodiscard]] constexpr auto address() const noexcept -> void*
        {
            return _frame_ptr;
        }

        [[nodiscard]] static constexpr auto from_address(void* addr) noexcept -> coroutine_handle
        {
            auto coro_handle = coroutine_handle{};
            coro_handle._frame_ptr = addr;
            return coro_handle;
        }

        [[nodiscard]] constexpr operator coroutine_handle<>() const noexcept
        {
            return coroutine_handle<>::from_address(_frame_ptr);
        }

        [[nodiscard]] constexpr auto promise() const -> Promise&
        {
            auto* const prom_ptr = __builtin_coro_promise(_frame_ptr, alignof(Promise), false);
            return *reinterpret_cast<Promise*>(prom_ptr);
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return _frame_ptr != nullptr;
        }

        [[nodiscard]] auto done() const noexcept -> bool
        {
#if defined(TEMPEST_PLATFORM_WINDOWS)
            return _builtin_coro_done(_frame_ptr);
#elif defined(TEMPEST_PLATFORM_LINUX)
            return __builtin_coro_done(_frame_ptr);
#endif
        }

        auto resume() const -> void
        {
#if defined(TEMPEST_PLATFORM_WINDOWS)
            __builtin_coro_resume(_frame_ptr);
#elif defined(TEMPEST_PLATFORM_LINUX)
            __builtin_coro_resume(_frame_ptr);
#endif
        }

        auto operator()() const -> void
        {
            resume();
        }

        auto destroy() const -> void
        {
#if defined(TEMPEST_PLATFORM_WINDOWS)
            __builtin_coro_destroy(_frame_ptr);
#elif defined(TEMPEST_PLATFORM_LINUX)
            __builtin_coro_destroy(_frame_ptr);
#endif
        }

      private:
        void* _frame_ptr;
    };

    /// \brief A promise type that can be used for coroutine that is a no-op
    ///
    /// A no-op coroutine behaves as if it does nothing other than control flow of a coroutine, suspends immediately
    /// upon beginning and resumption, has no state such that destroying it is a no-op, and never reaches a final
    /// suspended state if a coroutine handle refers to it.
    TEMPEST_API struct noop_coroutine_promise
    {
    };

    template <>
    struct coroutine_handle<noop_coroutine_promise>
    {
        friend auto noop_coroutine() noexcept -> coroutine_handle;

        [[nodiscard]] constexpr operator coroutine_handle<>() const noexcept
        {
            return coroutine_handle<>::from_address(_frame_ptr);
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return true;
        }

        [[nodiscard]] auto done() const noexcept -> bool
        {
            return false;
        }

        constexpr auto resume() const noexcept -> void
        {
        }

        constexpr auto operator()() const noexcept -> void
        {
            resume();
        }

        constexpr auto destroy() const noexcept -> void
        {
        }

        [[nodiscard]] auto promise() const noexcept -> noop_coroutine_promise&
        {
            return *static_cast<noop_coroutine_promise*>(
                __builtin_coro_promise(_frame_ptr, alignof(noop_coroutine_promise), false));
        }

        [[nodiscard]] constexpr auto address() const noexcept -> void*
        {
            return _frame_ptr;
        }

      private:
        coroutine_handle() noexcept = default;

        void* _frame_ptr = __builtin_coro_noop();
    };

    using noop_coroutine_handle = coroutine_handle<noop_coroutine_promise>;

    [[nodiscard]] inline auto noop_coroutine() noexcept -> noop_coroutine_handle
    {
        return noop_coroutine_handle{};
    }

    /// \brief A type that can be used to indicate that a coroutine should never suspend.
    TEMPEST_API struct suspend_never
    {
        [[nodiscard]] constexpr auto await_ready() const noexcept -> bool
        {
            return true;
        }

        constexpr auto await_suspend(coroutine_handle<> /* handle*/) const noexcept -> void
        {
        }

        constexpr auto await_resume() const noexcept -> void
        {
        }
    };

    /// \brief A type that can be used to indicate that a coroutine should always suspend.
    TEMPEST_API struct suspend_always
    {
        [[nodiscard]] constexpr auto await_ready() const noexcept -> bool
        {
            return false;
        }

        constexpr auto await_suspend(coroutine_handle<> /* handle*/) const noexcept -> void
        {
        }

        constexpr auto await_resume() const noexcept -> void
        {
        }
    };
} // namespace tempest

// To make the compiler magic happy, we provide an alias for the coroutine_handle<void> type in the std namespace.

namespace std
{
    template <typename Promise = void>
    struct coroutine_handle : tempest::coroutine_handle<Promise>
    {
    };

    using noop_coroutine_promise = tempest::noop_coroutine_promise;
    using noop_coroutine_handle = tempest::coroutine_handle<noop_coroutine_promise>;

    using tempest::noop_coroutine;

    template <typename Result, typename... Args>
    struct coroutine_traits : tempest::coroutine_traits<Result, Args...>
    {
    };
} // namespace std

#endif // tempest_coroutine_hpp
