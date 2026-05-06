#ifndef tempest_core_coroutine_hpp
#define tempest_core_coroutine_hpp

#include <tempest/api.hpp>
#include <tempest/memory.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    template <typename T = void>
    class coroutine_handle;

    template <>
    class TEMPEST_API coroutine_handle<void>
    {
      public:
        constexpr coroutine_handle() noexcept = default;
        constexpr coroutine_handle(nullptr_t) noexcept
        {
        }

        coroutine_handle& operator=(nullptr_t) noexcept
        {
            _handle = nullptr;
            return *this;
        }

        [[nodiscard]] constexpr void* address() const noexcept
        {
            return _handle;
        }

        [[nodiscard]] static constexpr auto from_address(void* const addr) noexcept -> coroutine_handle
        {
            coroutine_handle handle;
            handle._handle = addr;
            return handle;
        }

        constexpr explicit operator bool() const noexcept
        {
            return _handle != nullptr;
        }

        [[nodiscard]] auto done() const noexcept -> bool
        {
            return __builtin_coro_done(_handle);
        }

        auto operator()() const -> void
        {
            __builtin_coro_resume(_handle);
        }

        auto resume() const -> void
        {
            __builtin_coro_resume(_handle);
        }

        auto destroy() const -> void
        {
            __builtin_coro_destroy(_handle);
        }

      private:
        void* _handle{nullptr};
    };

    template <typename PromiseT>
    class TEMPEST_API coroutine_handle
    {
      public:
        constexpr coroutine_handle() noexcept = default;
        constexpr coroutine_handle(nullptr_t) noexcept
        {
        }

        coroutine_handle(const coroutine_handle&) = default;
        coroutine_handle(coroutine_handle&&) noexcept = default;
        ~coroutine_handle() = default;
        coroutine_handle& operator=(const coroutine_handle&) = default;
        coroutine_handle& operator=(coroutine_handle&&) noexcept = default;

        [[nodiscard]] static coroutine_handle from_promise(PromiseT& prom) noexcept
        {
            auto* const promise_ptr = const_cast<void*>(static_cast<const volatile void*>(addressof(prom)));
            auto* const frame_ptr = __builtin_coro_promise(promise_ptr, 0, true);
            coroutine_handle result;
            result._handle = frame_ptr;
            return result;
        }

        coroutine_handle& operator=(nullptr_t) noexcept
        {
            _handle = nullptr;
            return *this;
        }

        [[nodiscard]] constexpr auto address() const noexcept -> void*
        {
            return _handle;
        }

        [[nodiscard]] static constexpr auto from_address(void* const addr) noexcept -> coroutine_handle
        {
            coroutine_handle handle;
            handle._handle = addr;
            return handle;
        }

        [[nodiscard]] constexpr operator coroutine_handle<>() const noexcept
        {
            return coroutine_handle<>::from_address(_handle);
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return _handle != nullptr;
        }

        [[nodiscard]] auto done() const noexcept -> bool
        {
            return __builtin_coro_done(_handle);
        }

        auto operator()() const -> void
        {
            __builtin_coro_resume(_handle);
        }

        auto resume() const -> void
        {
            __builtin_coro_resume(_handle);
        }

        auto destroy() const -> void
        {
            __builtin_coro_destroy(_handle);
        }

        [[nodiscard]] auto promise() const noexcept -> PromiseT&
        {
            auto* const promise_ptr = __builtin_coro_promise(_handle, 0, false);
            return *reinterpret_cast<PromiseT*>(promise_ptr);
        }

      private:
        void* _handle;
    };

    struct TEMPEST_API noop_coroutine_promise
    {
    };

    template <>
    class TEMPEST_API coroutine_handle<noop_coroutine_promise>
    {
        friend auto noop_coroutine() noexcept -> coroutine_handle<noop_coroutine_promise>;

      public:
        constexpr operator coroutine_handle<>() const noexcept
        {
            return coroutine_handle<>::from_address(_handle);
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return true;
        }

        [[nodiscard]] constexpr auto done() const noexcept -> bool // NOLINT(readability-convert-member-functions-to-static) - Must be a non-static member function to be used as a promise type.
        {
            return false;
        }

        constexpr void operator()() const {}
        constexpr void resume() const {}
        constexpr void destroy() const {}

        [[nodiscard]] noop_coroutine_promise& promise() const noexcept
        {
            auto* const promise_ptr = __builtin_coro_promise(_handle, 0, false);
            return *reinterpret_cast<noop_coroutine_promise*>(promise_ptr);
        }

        [[nodiscard]] constexpr auto address() const noexcept -> void*
        {
            return _handle;
        }

      private:
        coroutine_handle() noexcept = default;
      
        void* _handle = __builtin_coro_noop();
    };

    using noop_coroutine_handle = coroutine_handle<noop_coroutine_promise>;

    [[nodiscard]] inline auto noop_coroutine() noexcept -> noop_coroutine_handle
    {
        return {};
    }

    struct TEMPEST_API suspend_never
    {
        [[nodiscard]] constexpr auto await_ready() const noexcept -> bool { // NOLINT(readability-convert-member-functions-to-static) - Must be a non-static member function to be used as an awaitable type.
            return true;
        }

        constexpr void await_suspend(coroutine_handle<> /*unused*/) const noexcept {}
        constexpr void await_resume() const noexcept {}
    };

    struct TEMPEST_API suspend_always {
        [[nodiscard]] constexpr auto await_ready() const noexcept -> bool { // NOLINT(readability-convert-member-functions-to-static) - Must be a non-static member function to be used as an awaitable type.
            return false;
        }

        constexpr void await_suspend(coroutine_handle<> /*unused*/) const noexcept {}
        constexpr void await_resume() const noexcept {}
    };
} // namespace tempest

#endif // tempest_core_coroutine_hpp
