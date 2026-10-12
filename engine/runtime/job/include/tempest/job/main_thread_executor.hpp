#ifndef tempest_job_main_thread_executor_hpp
#define tempest_job_main_thread_executor_hpp

#include <tempest/api.hpp>
#include <tempest/checked.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/functional.hpp>
#include <tempest/int.hpp>
#include <tempest/job/mpsc_mailbox.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/utility.hpp>

namespace tempest::job
{
    class main_thread_executor;

    /// @brief Represents a unit of work to be dispatched on the main thread,
    ///        wrapping either an executable callback or a suspended coroutine handle.
    struct TEMPEST_API main_thread_task
    {
        enum class task_kind : uint8_t
        {
            empty = 0,
            coroutine,
            callback,
        };

        task_kind kind{task_kind::empty};
        coroutine_handle<> handle{nullptr};
        function<void()> callback{};

        main_thread_task() noexcept = default;

        explicit main_thread_task(coroutine_handle<> coroutine_hnd) noexcept
            : kind{task_kind::coroutine}, handle{coroutine_hnd}
        {
        }

        explicit main_thread_task(function<void()> callback_fn) noexcept
            : kind{task_kind::callback}, callback{tempest::move(callback_fn)}
        {
        }

        main_thread_task(const main_thread_task&) = default;
        main_thread_task(main_thread_task&&) noexcept = default;
        ~main_thread_task() = default;
        auto operator=(const main_thread_task&) -> main_thread_task& = default;
        auto operator=(main_thread_task&&) noexcept -> main_thread_task& = default;

        auto operator()() -> void;
        auto execute() -> void
        {
            (*this)();
        }
    };

    /// @brief Lock-free bounded multi-producer, single-consumer executor confined to the main thread.
    ///        Allows any worker or thread to post callbacks or schedule suspended coroutines for main-thread execution.
    class TEMPEST_API main_thread_executor
    {
      public:
        static constexpr size_t default_capacity = 2048;

        main_thread_executor() = default;
        ~main_thread_executor() = default;

        main_thread_executor(const main_thread_executor&) = delete;
        main_thread_executor(main_thread_executor&&) noexcept = delete;
        auto operator=(const main_thread_executor&) -> main_thread_executor& = delete;
        auto operator=(main_thread_executor&&) noexcept -> main_thread_executor& = delete;

        /// @brief Posts a function callback to execute on the main thread.
        auto post(function<void()> task) -> void;

        /// @brief Schedules a suspended coroutine handle to resume on the main thread.
        auto schedule(coroutine_handle<> handle) -> void;

        /// @brief Dispatches all currently pending tasks on the calling thread (main thread).
        /// @return The count of tasks dispatched during this invocation.
        auto dispatch_pending() -> size_t;

        /// @brief Queries whether there are currently any pending tasks.
        /// @note Thread-confined to the main thread consumer.
        [[nodiscard]] auto empty() const noexcept -> bool;

        /// @brief Returns the bounded capacity of the backing mailbox.
        [[nodiscard]] constexpr auto capacity() const noexcept -> size_t
        {
            return default_capacity;
        }

      private:
        auto _push(main_thread_task task) -> void;

        mpsc_mailbox<main_thread_task, default_capacity> _mailbox{};
    };

    /// @brief Coroutine awaiter that suspends the active coroutine and enqueues its resumption
    ///        onto the specified main_thread_executor.
    struct TEMPEST_API resume_on_main_thread
    {
        explicit resume_on_main_thread(main_thread_executor& executor) noexcept
            : _executor{executor}
        {
        }

        resume_on_main_thread(const resume_on_main_thread&) noexcept = default;
        resume_on_main_thread(resume_on_main_thread&&) noexcept = default;
        ~resume_on_main_thread() = default;
        auto operator=(const resume_on_main_thread&) noexcept -> resume_on_main_thread& = default;
        auto operator=(resume_on_main_thread&&) noexcept -> resume_on_main_thread& = default;

        [[nodiscard]] constexpr auto await_ready() const noexcept -> bool
        {
            return false;
        }

        auto await_suspend(coroutine_handle<> handle) const -> void;

        constexpr auto await_resume() const noexcept -> void
        {
        }

        [[nodiscard]] constexpr auto suspend_reason_tag() const noexcept -> profiler::suspend_reason
        {
            return profiler::suspend_reason::yield;
        }

      private:
        non_null<main_thread_executor> _executor;
    };
} // namespace tempest::job

#endif // tempest_job_main_thread_executor_hpp
