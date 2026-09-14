#include <tempest/job/async_event.hpp>
#include <tempest/job/job_system.hpp>

namespace tempest::job
{
    namespace
    {
        inline auto cpu_pause() noexcept -> void
        {
#if defined(__x86_64__) || defined(_M_X64)
            asm volatile("pause" ::: "memory");
#elif defined(__aarch64__)
            asm volatile("yield" ::: "memory");
#endif
        }
    } // namespace

    async_event::async_event(bool initial_state, event_reset_mode mode) noexcept
        : _mode{mode}
    {
        if (initial_state)
        {
            _waiters_in.store(_signaled_sentinel(), memory_order::relaxed);
        }
    }

    async_event::async_event(job_system& sys, bool initial_state, event_reset_mode mode) noexcept
        : _mode{mode}, _sys{&sys}
    {
        if (initial_state)
        {
            _waiters_in.store(_signaled_sentinel(), memory_order::relaxed);
        }
    }

    auto async_event::_resume(coroutine_handle<> hnd) noexcept -> void
    {
        auto* const sys = _sys;
        if (sys != nullptr)
        {
            sys->schedule(hnd);
        }
        else
        {
            hnd.resume();
        }
    }

    auto async_event::reset() noexcept -> void
    {
        auto* expected_sentinel = _signaled_sentinel();
        [[maybe_unused]] const auto _ =
            _waiters_in.compare_exchange_strong(expected_sentinel, nullptr, memory_order::acq_rel, memory_order::relaxed);
    }

    [[nodiscard]] auto async_event::is_set() const noexcept -> bool
    {
        return _waiters_in.load(memory_order::acquire) == _signaled_sentinel();
    }

    auto async_event::set() noexcept -> void
    {
        if (_mode == event_reset_mode::manual)
        {
            auto* const old_head = _waiters_in.exchange(_signaled_sentinel(), memory_order::acq_rel);
            if (old_head == _signaled_sentinel())
            {
                return;
            }

            auto* current_waiter = old_head;
            while (current_waiter != nullptr)
            {
                auto* const next_waiter = current_waiter->next;
                _resume(current_waiter->handle);
                current_waiter = next_waiter;
            }
            return;
        }

        // Auto-reset mode:
        auto to_resume = coroutine_handle<>{nullptr};
        {
            while (_claim.exchange(true, memory_order::acquire))
            {
                cpu_pause();
            }

            if (_waiters_out == nullptr)
            {
                auto* const incoming = _waiters_in.drain();
                if (incoming == _signaled_sentinel())
                {
                    _waiters_in.store(_signaled_sentinel(), memory_order::release);
                }
                else if (incoming != nullptr)
                {
                    _waiters_out = intrusive_mpsc_stack<async_event_waiter>::reverse(incoming);
                }
            }

            if (_waiters_out != nullptr)
            {
                auto* const waiter = _waiters_out;
                _waiters_out = waiter->next;
                to_resume = waiter->handle;
                _claim.store(false, memory_order::release);
            }
            else
            {
                _claim.store(false, memory_order::release);
                _waiters_in.store(_signaled_sentinel(), memory_order::release);
            }
        }

        if (to_resume)
        {
            _resume(to_resume);
        }
    }

    auto async_event::event_awaiter::await_ready() const noexcept -> bool
    {
        if (event->_mode == event_reset_mode::manual)
        {
            return event->_waiters_in.load(memory_order::acquire) == event->_signaled_sentinel();
        }

        auto* expected = event->_signaled_sentinel();
        return event->_waiters_in.compare_exchange_strong(expected, nullptr, memory_order::acq_rel, memory_order::acquire);
    }

    auto async_event::event_awaiter::await_suspend(coroutine_handle<> hnd) noexcept -> bool
    {
        waiter.handle = hnd;

        if (event->_mode == event_reset_mode::manual)
        {
            auto* old_head = event->_waiters_in.load(memory_order::acquire);
            while (true)
            {
                if (old_head == event->_signaled_sentinel())
                {
                    return false;
                }
                waiter.next = old_head;
                if (event->_waiters_in.compare_exchange_weak(old_head, &waiter, memory_order::release,
                                                             memory_order::acquire))
                {
                    return true;
                }
            }
        }
        else // auto_reset
        {
            auto* old_head = event->_waiters_in.load(memory_order::acquire);
            while (true)
            {
                if (old_head == event->_signaled_sentinel())
                {
                    if (event->_waiters_in.compare_exchange_weak(old_head, nullptr, memory_order::acq_rel,
                                                                 memory_order::acquire))
                    {
                        return false;
                    }
                    continue;
                }

                waiter.next = old_head;
                if (event->_waiters_in.compare_exchange_weak(old_head, &waiter, memory_order::release,
                                                             memory_order::acquire))
                {
                    return true;
                }
            }
        }
    }
} // namespace tempest::job
