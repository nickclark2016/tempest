#include <tempest/job/async_event.hpp>
#include <tempest/job/job_system.hpp>

namespace tempest::job
{
    namespace
    {
        // Sentinel value used to indicate the event is currently in the signaled state.
        async_event_waiter* const signaled_sentinel =
            reinterpret_cast<async_event_waiter*>(static_cast<uintptr_t>(1));
    } // namespace

    async_event::async_event(bool initial_state, event_reset_mode mode) noexcept
        : _mode{mode}, _waiters{initial_state ? signaled_sentinel : nullptr}
    {
    }

    async_event::async_event(job_system& sys, bool initial_state, event_reset_mode mode) noexcept
        : _mode{mode}, _waiters{initial_state ? signaled_sentinel : nullptr}, _sys{&sys}
    {
    }

    auto async_event::_resume(coroutine_handle<> h) noexcept -> void
    {
        if (_sys != nullptr)
        {
            _sys->schedule(h);
        }
        else
        {
            h.resume();
        }
    }

    auto async_event::reset() noexcept -> void
    {
        auto* expected = signaled_sentinel;
        _waiters.compare_exchange_strong(expected, nullptr, memory_order::acq_rel);
    }

    [[nodiscard]] auto async_event::is_set() const noexcept -> bool
    {
        return _waiters.load(memory_order::acquire) == signaled_sentinel;
    }

    auto async_event::set() noexcept -> void
    {
        if (_mode == event_reset_mode::manual)
        {
            auto* head = _waiters.exchange(signaled_sentinel, memory_order::acq_rel);
            if (head == signaled_sentinel)
            {
                return;
            }
            while (head != nullptr)
            {
                auto* next = head->next;
                _resume(head->handle);
                head = next;
            }
        }
        else // auto_reset
        {
            auto* old_head = _waiters.load(memory_order::acquire);
            while (true)
            {
                if (old_head == signaled_sentinel)
                {
                    return; // Already signaled
                }
                if (old_head == nullptr)
                {
                    if (_waiters.compare_exchange_weak(old_head, signaled_sentinel, memory_order::acq_rel))
                    {
                        return;
                    }
                    continue;
                }

                // There is at least one waiter in the stack: pop it and awaken it
                if (_waiters.compare_exchange_weak(old_head, old_head->next, memory_order::acq_rel))
                {
                    _resume(old_head->handle);
                    return;
                }
            }
        }
    }

    auto async_event::event_awaiter::await_ready() const noexcept -> bool
    {
        if (event._mode == event_reset_mode::manual)
        {
            return event.is_set();
        }

        // Auto-reset mode: try to consume the signal immediately
        auto* expected = signaled_sentinel;
        return event._waiters.compare_exchange_strong(expected, nullptr, memory_order::acq_rel);
    }

    auto async_event::event_awaiter::await_suspend(coroutine_handle<> h) noexcept -> bool
    {
        waiter.handle = h;

        auto* old_head = event._waiters.load(memory_order::acquire);
        while (true)
        {
            if (old_head == signaled_sentinel)
            {
                if (event._mode == event_reset_mode::auto_reset)
                {
                    // Try to consume the signal
                    if (event._waiters.compare_exchange_weak(old_head, nullptr, memory_order::acq_rel))
                    {
                        return false; // Signal consumed, do not suspend!
                    }
                    continue; // Retry with new old_head
                }
                // Manual reset mode: already signaled, do not suspend!
                return false;
            }

            waiter.next = old_head;
            if (event._waiters.compare_exchange_weak(old_head, &waiter, memory_order::release, memory_order::acquire))
            {
                return true; // Successfully enqueued and suspended!
            }
        }
    }
} // namespace tempest::job
