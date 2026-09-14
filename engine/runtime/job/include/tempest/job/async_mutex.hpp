#ifndef tempest_job_async_mutex_hpp
#define tempest_job_async_mutex_hpp

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
    struct async_mutex_waiter : treiber_node<async_mutex_waiter>
    {
        coroutine_handle<> handle{nullptr};
        atomic<wait_state> state{wait_state::completed};

        async_mutex_waiter() noexcept = default;
        async_mutex_waiter(coroutine_handle<> hnd, wait_state st = wait_state::completed) noexcept
            : handle{hnd}, state{st}
        {
        }
        async_mutex_waiter(const async_mutex_waiter&) = delete;
        auto operator=(const async_mutex_waiter&) -> async_mutex_waiter& = delete;
        async_mutex_waiter(async_mutex_waiter&& other) noexcept
            : handle{other.handle}, state{other.state.load(memory_order::relaxed)}
        {
        }
        auto operator=(async_mutex_waiter&& other) noexcept -> async_mutex_waiter&
        {
            if (this != &other)
            {
                handle = other.handle;
                state.store(other.state.load(memory_order::relaxed), memory_order::relaxed);
            }
            return *this;
        }
    };

    class async_mutex;

    class TEMPEST_API scoped_lock_guard
    {
      public:
        scoped_lock_guard() noexcept = default;
        scoped_lock_guard(const scoped_lock_guard&) = delete;
        scoped_lock_guard(scoped_lock_guard&& other) noexcept;
        ~scoped_lock_guard();

        auto operator=(const scoped_lock_guard&) -> scoped_lock_guard& = delete;
        auto operator=(scoped_lock_guard&& other) noexcept -> scoped_lock_guard&;

      private:
        friend class async_mutex;

        explicit scoped_lock_guard(async_mutex& mtx) noexcept;

        async_mutex* _mutex{nullptr};
    };

    class job_system;

    class TEMPEST_API async_mutex
    {
      public:
        async_mutex() noexcept = default;
        explicit async_mutex(job_system& sys) noexcept : _sys{&sys}
        {
        }
        ~async_mutex();

        async_mutex(const async_mutex&) = delete;
        async_mutex(async_mutex&&) = delete;
        auto operator=(const async_mutex&) -> async_mutex& = delete;
        auto operator=(async_mutex&&) -> async_mutex& = delete;

        [[nodiscard]] auto try_lock() noexcept -> bool;
        auto unlock() noexcept -> void;

        struct lock_awaiter
        {
            non_null<async_mutex> mutex;
            async_mutex_waiter waiter{};

            explicit lock_awaiter(non_null<async_mutex> mtx) noexcept : mutex{mtx}
            {
            }
            lock_awaiter(const lock_awaiter&) = delete;
            auto operator=(const lock_awaiter&) -> lock_awaiter& = delete;
            lock_awaiter(lock_awaiter&& other) noexcept
                : mutex{other.mutex}, waiter{tempest::move(other.waiter)}
            {
            }
            auto operator=(lock_awaiter&& other) noexcept -> lock_awaiter&
            {
                if (this != &other)
                {
                    mutex = other.mutex;
                    waiter = tempest::move(other.waiter);
                }
                return *this;
            }
            ~lock_awaiter() noexcept;

            [[nodiscard]] auto await_ready() const noexcept -> bool
            {
                return mutex->try_lock();
            }

            auto await_suspend(coroutine_handle<> hnd) noexcept -> bool;

            constexpr auto await_resume() const noexcept -> void
            {
            }

            [[nodiscard]] constexpr auto suspend_reason_tag() const noexcept -> profiler::suspend_reason
            {
                return profiler::suspend_reason::mutex_contention;
            }
        };

        struct scoped_lock_awaiter
        {
            non_null<async_mutex> mutex;
            async_mutex_waiter waiter{};

            explicit scoped_lock_awaiter(non_null<async_mutex> mtx) noexcept : mutex{mtx}
            {
            }
            scoped_lock_awaiter(const scoped_lock_awaiter&) = delete;
            auto operator=(const scoped_lock_awaiter&) -> scoped_lock_awaiter& = delete;
            scoped_lock_awaiter(scoped_lock_awaiter&& other) noexcept
                : mutex{other.mutex}, waiter{tempest::move(other.waiter)}
            {
            }
            auto operator=(scoped_lock_awaiter&& other) noexcept -> scoped_lock_awaiter&
            {
                if (this != &other)
                {
                    mutex = other.mutex;
                    waiter = tempest::move(other.waiter);
                }
                return *this;
            }
            ~scoped_lock_awaiter() noexcept;

            [[nodiscard]] auto await_ready() const noexcept -> bool
            {
                return mutex->try_lock();
            }

            auto await_suspend(coroutine_handle<> hnd) noexcept -> bool;

            [[nodiscard]] auto await_resume() noexcept -> scoped_lock_guard
            {
                return scoped_lock_guard{*mutex};
            }

            [[nodiscard]] constexpr auto suspend_reason_tag() const noexcept -> profiler::suspend_reason
            {
                return profiler::suspend_reason::mutex_contention;
            }
        };

        [[nodiscard]] auto lock() noexcept -> lock_awaiter
        {
            return lock_awaiter{*this};
        }

        [[nodiscard]] auto scoped_lock() noexcept -> scoped_lock_awaiter
        {
            return scoped_lock_awaiter{*this};
        }

      private:
        friend struct lock_awaiter;
        friend struct scoped_lock_awaiter;

        auto _enqueue_waiter(async_mutex_waiter* waiter, coroutine_handle<> hnd) noexcept -> bool;

        async_mutex_waiter _sentinel{};

        [[nodiscard]] auto _locked_sentinel() noexcept -> async_mutex_waiter*
        {
            return &_sentinel;
        }

        [[nodiscard]] auto _locked_sentinel() const noexcept -> const async_mutex_waiter*
        {
            return &_sentinel;
        }

        intrusive_mpsc_stack<async_mutex_waiter> _waiters_in;
        async_mutex_waiter* _waiters_out{nullptr};
        atomic<bool> _claim{false};
        job_system* _sys{nullptr};

        auto _resume(coroutine_handle<> hnd) noexcept -> void;
        auto _drain_in_to_out_locked() noexcept -> void;
        auto _cancel_waiter(async_mutex_waiter* waiter) noexcept -> void;
    };
} // namespace tempest::job

#endif // tempest_job_async_mutex_hpp
