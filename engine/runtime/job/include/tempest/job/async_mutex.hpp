#ifndef tempest_job_async_mutex_hpp
#define tempest_job_async_mutex_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/int.hpp>
#include <tempest/profiler/types.hpp>

namespace tempest::job
{
    struct async_mutex_waiter
    {
        coroutine_handle<> handle{nullptr};
        async_mutex_waiter* next{nullptr};
    };

    class async_mutex;

    class TEMPEST_API scoped_lock_guard
    {
      public:
        scoped_lock_guard() noexcept = default;
        explicit scoped_lock_guard(async_mutex& m) noexcept;
        ~scoped_lock_guard();

        scoped_lock_guard(const scoped_lock_guard&) = delete;
        scoped_lock_guard& operator=(const scoped_lock_guard&) = delete;

        scoped_lock_guard(scoped_lock_guard&& other) noexcept;
        scoped_lock_guard& operator=(scoped_lock_guard&& other) noexcept;

      private:
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
        ~async_mutex() = default;

        async_mutex(const async_mutex&) = delete;
        async_mutex& operator=(const async_mutex&) = delete;
        async_mutex(async_mutex&&) = delete;
        async_mutex& operator=(async_mutex&&) = delete;

        [[nodiscard]] auto try_lock() noexcept -> bool;
        auto unlock() noexcept -> void;

        struct lock_awaiter
        {
            async_mutex& mutex;
            async_mutex_waiter waiter{};

            [[nodiscard]] auto await_ready() const noexcept -> bool
            {
                return mutex.try_lock();
            }

            auto await_suspend(coroutine_handle<> h) noexcept -> bool;

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
            async_mutex& mutex;
            async_mutex_waiter waiter{};

            [[nodiscard]] auto await_ready() const noexcept -> bool
            {
                return mutex.try_lock();
            }

            auto await_suspend(coroutine_handle<> h) noexcept -> bool;

            [[nodiscard]] auto await_resume() noexcept -> scoped_lock_guard
            {
                return scoped_lock_guard{mutex};
            }

            [[nodiscard]] constexpr auto suspend_reason_tag() const noexcept -> profiler::suspend_reason
            {
                return profiler::suspend_reason::mutex_contention;
            }
        };

        [[nodiscard]] auto lock() noexcept -> lock_awaiter
        {
            return lock_awaiter{.mutex = *this};
        }

        [[nodiscard]] auto scoped_lock() noexcept -> scoped_lock_awaiter
        {
            return scoped_lock_awaiter{.mutex = *this};
        }

      private:
        friend struct lock_awaiter;
        friend struct scoped_lock_awaiter;

        auto _enqueue_waiter(async_mutex_waiter* waiter, coroutine_handle<> h) noexcept -> bool;

        atomic<uint32_t> _locked{0};
        atomic<async_mutex_waiter*> _waiters_in{nullptr};
        async_mutex_waiter* _waiters_out{nullptr};
        job_system* _sys{nullptr};

        auto _resume(coroutine_handle<> h) noexcept -> void;
    };
} // namespace tempest::job

#endif // tempest_job_async_mutex_hpp
