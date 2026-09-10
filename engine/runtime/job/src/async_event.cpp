#include <tempest/job/async_event.hpp>
#include <tempest/job/job_system.hpp>

namespace tempest::job
{
    namespace
    {
        auto resume_coroutine(coroutine_handle<> h) -> void
        {
            auto* js = job_system::get_current();
            if (js != nullptr)
            {
                js->schedule(h);
            }
            else
            {
                h.resume();
            }
        }
    } // namespace

    async_event::async_event(bool initial_state, event_reset_mode mode) noexcept
        : _mode{mode}, _signaled{initial_state}
    {
    }

    auto async_event::reset() noexcept -> void
    {
        _signaled.store(false, memory_order::release);
    }

    [[nodiscard]] auto async_event::is_set() const noexcept -> bool
    {
        return _signaled.load(memory_order::acquire);
    }

    auto async_event::_try_consume_signal() noexcept -> bool
    {
        if (_mode == event_reset_mode::manual)
        {
            return _signaled.load(memory_order::acquire);
        }
        return _signaled.exchange(false, memory_order::acq_rel);
    }

    auto async_event::set() noexcept -> void
    {
        if (_mode == event_reset_mode::manual)
        {
            _signaled.store(true, memory_order::release);
            auto* head = _waiters.exchange(nullptr, memory_order::acq_rel);
            while (head != nullptr)
            {
                auto* next = head->next;
                resume_coroutine(head->handle);
                head = next;
            }
        }
        else // auto_reset
        {
            // Try to pop one waiter
            auto* old_head = _waiters.load(memory_order::acquire);
            while (old_head != nullptr)
            {
                if (_waiters.compare_exchange_weak(old_head, old_head->next, memory_order::acq_rel))
                {
                    // Woke one waiter; do not set _signaled to true
                    resume_coroutine(old_head->handle);
                    return;
                }
            }

            // No waiting coroutines, set signaled state
            _signaled.store(true, memory_order::release);
        }
    }

    auto async_event::_enqueue_waiter(async_event_waiter* waiter, coroutine_handle<> h) noexcept -> bool
    {
        waiter->handle = h;

        auto* old_head = _waiters.load(memory_order::relaxed);
        do
        {
            waiter->next = old_head;
        } while (!_waiters.compare_exchange_weak(old_head, waiter, memory_order::release));

        if (_try_consume_signal())
        {
            return false; // Do not suspend
        }

        return true;
    }

    auto async_event::event_awaiter::await_ready() const noexcept -> bool
    {
        return event._try_consume_signal();
    }

    auto async_event::event_awaiter::await_suspend(coroutine_handle<> h) noexcept -> bool
    {
        return event._enqueue_waiter(&waiter, h);
    }
} // namespace tempest::job
