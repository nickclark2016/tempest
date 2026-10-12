#include <tempest/job/main_thread_executor.hpp>
#include <tempest/thread.hpp>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace tempest::job
{
    namespace
    {
        inline auto cpu_pause() noexcept -> void
        {
#if defined(_MSC_VER)
#if defined(_M_X64) || defined(_M_IX86)
            _mm_pause();
#elif defined(_M_ARM64) || defined(_M_ARM)
            __yield();
#else
            this_thread::yield();
#endif
#elif defined(__x86_64__) || defined(__i386__)
            asm volatile("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
            asm volatile("yield" ::: "memory");
#else
            this_thread::yield();
#endif
        }
    } // namespace

    auto main_thread_task::operator()() -> void
    {
        switch (kind)
        {
        case task_kind::coroutine:
            if (handle)
            {
                const auto h = handle;
                handle = nullptr;
                h.resume();
            }
            break;
        case task_kind::callback:
            if (callback)
            {
                auto cb = tempest::move(callback);
                cb();
            }
            break;
        case task_kind::empty:
            break;
        }
    }

    auto resume_on_main_thread::await_suspend(coroutine_handle<> handle) const -> void
    {
        _executor->schedule(handle);
    }

    auto main_thread_executor::post(function<void()> task) -> void
    {
        _push(main_thread_task{tempest::move(task)});
    }

    auto main_thread_executor::schedule(coroutine_handle<> handle) -> void
    {
        _push(main_thread_task{handle});
    }

    auto main_thread_executor::dispatch_pending() -> size_t
    {
        return _mailbox.drain([](main_thread_task&& task) {
            task();
        });
    }

    auto main_thread_executor::empty() const noexcept -> bool
    {
        return _mailbox.empty();
    }

    auto main_thread_executor::_push(main_thread_task task) -> void
    {
        for (auto spin = 0U;; ++spin)
        {
            if (_mailbox.try_push(tempest::move(task)) == push_result::success)
            {
                return;
            }

            constexpr auto spin_threshold = 16U;
            if (spin < spin_threshold)
            {
                cpu_pause();
            }
            else
            {
                tempest::this_thread::yield();
            }
        }
    }
} // namespace tempest::job
