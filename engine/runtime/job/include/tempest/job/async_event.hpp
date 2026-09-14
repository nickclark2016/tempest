#ifndef tempest_job_async_event_hpp
#define tempest_job_async_event_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/checked.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/int.hpp>
#include <tempest/intrusive_stack.hpp>
#include <tempest/profiler/types.hpp>

namespace tempest::job
{
    enum class event_reset_mode : uint8_t
    {
        auto_reset,
        manual
    };

    struct async_event_waiter : treiber_node<async_event_waiter>
    {
        coroutine_handle<> handle{nullptr};
    };

    class job_system;

    class TEMPEST_API async_event
    {
      public:
        explicit async_event(bool initial_state = false, event_reset_mode mode = event_reset_mode::auto_reset) noexcept;
        explicit async_event(job_system& sys, bool initial_state = false,
                             event_reset_mode mode = event_reset_mode::auto_reset) noexcept;
        ~async_event() = default;

        async_event(const async_event&) = delete;
        auto operator=(const async_event&) -> async_event& = delete;
        async_event(async_event&&) = delete;
        auto operator=(async_event&&) -> async_event& = delete;

        auto set() noexcept -> void;
        auto reset() noexcept -> void;
        [[nodiscard]] auto is_set() const noexcept -> bool;

        struct event_awaiter
        {
            non_null<async_event> event;
            async_event_waiter waiter{};

            [[nodiscard]] auto await_ready() const noexcept -> bool;
            auto await_suspend(coroutine_handle<> hnd) noexcept -> bool;
            constexpr auto await_resume() const noexcept -> void
            {
            }

            [[nodiscard]] constexpr auto suspend_reason_tag() const noexcept -> profiler::suspend_reason
            {
                return profiler::suspend_reason::event_wait;
            }
        };

        [[nodiscard]] auto wait() noexcept -> event_awaiter
        {
            return event_awaiter{.event = *this};
        }

        [[nodiscard]] auto operator co_await() noexcept -> event_awaiter
        {
            return wait();
        }

      private:
        friend struct event_awaiter;

        async_event_waiter _sentinel{};

        [[nodiscard]] auto _signaled_sentinel() noexcept -> async_event_waiter*
        {
            return &_sentinel;
        }

        [[nodiscard]] auto _signaled_sentinel() const noexcept -> const async_event_waiter*
        {
            return &_sentinel;
        }

        event_reset_mode _mode{event_reset_mode::auto_reset};

        intrusive_mpsc_stack<async_event_waiter> _waiters_in{};
        async_event_waiter* _waiters_out{nullptr};
        atomic<bool> _claim{false};

        job_system* _sys{nullptr};

        auto _resume(coroutine_handle<> hnd) noexcept -> void;
    };
} // namespace tempest::job

#endif // tempest_job_async_event_hpp
