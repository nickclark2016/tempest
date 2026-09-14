#ifndef tempest_job_async_event_hpp
#define tempest_job_async_event_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/checked.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/int.hpp>
#include <tempest/intrusive_stack.hpp>
#include <tempest/job/types.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/utility.hpp>

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
        atomic<wait_state> state{wait_state::completed};

        async_event_waiter() noexcept = default;
        async_event_waiter(coroutine_handle<> hnd, wait_state st = wait_state::completed) noexcept
            : handle{hnd}, state{st}
        {
        }
        async_event_waiter(const async_event_waiter&) = delete;
        auto operator=(const async_event_waiter&) -> async_event_waiter& = delete;
        async_event_waiter(async_event_waiter&& other) noexcept
            : handle{other.handle}, state{other.state.load(memory_order::relaxed)}
        {
        }
        auto operator=(async_event_waiter&& other) noexcept -> async_event_waiter&
        {
            if (this != &other)
            {
                handle = other.handle;
                state.store(other.state.load(memory_order::relaxed), memory_order::relaxed);
            }
            return *this;
        }
    };

    class job_system;

    class TEMPEST_API async_event
    {
      public:
        explicit async_event(bool initial_state = false, event_reset_mode mode = event_reset_mode::auto_reset) noexcept;
        explicit async_event(job_system& sys, bool initial_state = false,
                             event_reset_mode mode = event_reset_mode::auto_reset) noexcept;
        ~async_event();

        async_event(const async_event&) = delete;
        auto operator=(const async_event&) -> async_event& = delete;
        async_event(async_event&&) = delete;
        auto operator=(async_event&&) -> async_event& = delete;

        auto set() noexcept -> void;
        auto reset() noexcept -> void;
        [[nodiscard]] auto is_set() const noexcept -> bool;

        struct TEMPEST_API event_awaiter
        {
            non_null<async_event> event;
            async_event_waiter waiter{};

            explicit event_awaiter(non_null<async_event> evt) noexcept : event{evt}
            {
            }
            event_awaiter(const event_awaiter&) = delete;
            auto operator=(const event_awaiter&) -> event_awaiter& = delete;
            event_awaiter(event_awaiter&& other) noexcept
                : event{other.event}, waiter{tempest::move(other.waiter)}
            {
            }
            auto operator=(event_awaiter&& other) noexcept -> event_awaiter&
            {
                if (this != &other)
                {
                    event = other.event;
                    waiter = tempest::move(other.waiter);
                }
                return *this;
            }
            ~event_awaiter() noexcept;

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
            return event_awaiter{*this};
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

        intrusive_mpsc_stack<async_event_waiter> _waiters_in;
        async_event_waiter* _waiters_out{nullptr};
        atomic<bool> _claim{false};

        job_system* _sys{nullptr};

        auto _resume(coroutine_handle<> hnd) noexcept -> void;
        auto _cancel_waiter(async_event_waiter* waiter) noexcept -> void;
    };
} // namespace tempest::job

#endif // tempest_job_async_event_hpp
