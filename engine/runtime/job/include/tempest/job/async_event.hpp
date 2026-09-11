#ifndef tempest_job_async_event_hpp
#define tempest_job_async_event_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/int.hpp>

namespace tempest::job
{
    enum class event_reset_mode : uint8_t
    {
        auto_reset,
        manual
    };

    struct async_event_waiter
    {
        coroutine_handle<> handle{nullptr};
        async_event_waiter* next{nullptr};
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
        async_event& operator=(const async_event&) = delete;
        async_event(async_event&&) = delete;
        async_event& operator=(async_event&&) = delete;

        auto set() noexcept -> void;
        auto reset() noexcept -> void;
        [[nodiscard]] auto is_set() const noexcept -> bool;

        struct event_awaiter
        {
            async_event& event;
            async_event_waiter waiter{};

            [[nodiscard]] auto await_ready() const noexcept -> bool;
            auto await_suspend(coroutine_handle<> h) noexcept -> bool;
            constexpr auto await_resume() const noexcept -> void
            {
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

        event_reset_mode _mode{event_reset_mode::auto_reset};
        atomic<async_event_waiter*> _waiters{nullptr};
        job_system* _sys{nullptr};

        auto _resume(coroutine_handle<> h) noexcept -> void;
    };
} // namespace tempest::job

#endif // tempest_job_async_event_hpp
