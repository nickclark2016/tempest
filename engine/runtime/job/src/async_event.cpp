#include <tempest/job/async_event.hpp>
#include <tempest/job/job_system.hpp>

namespace tempest::job
{
    async_event::async_event(bool initial_state, event_reset_mode mode) noexcept
        : _mode{mode}, _signaled{initial_state}
    {
    }

    async_event::async_event(job_system& sys, bool initial_state, event_reset_mode mode) noexcept
        : _mode{mode}, _signaled{initial_state}, _sys{&sys}
    {
    }

    auto async_event::_resume(coroutine_handle<> hnd) noexcept -> void
    {
        auto* sys = _sys;
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
        auto guard = lock_guard{_mutex};
        _signaled = false;
    }

    [[nodiscard]] auto async_event::is_set() const noexcept -> bool
    {
        auto guard = lock_guard{_mutex};
        return _signaled;
    }

    auto async_event::set() noexcept -> void
    {
        if (_mode == event_reset_mode::manual)
        {
            auto* head = static_cast<async_event_waiter*>(nullptr);
            {
                auto guard = lock_guard{_mutex};
                if (_signaled)
                {
                    return;
                }
                _signaled = true;
                head = _waiters;
                _waiters = nullptr;
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
            auto to_resume = coroutine_handle<>{nullptr};
            {
                auto guard = lock_guard{_mutex};
                if (_waiters != nullptr)
                {
                    auto* waiter = _waiters;
                    _waiters = waiter->next;
                    to_resume = waiter->handle;
                }
                else
                {
                    _signaled = true;
                }
            }

            if (to_resume)
            {
                _resume(to_resume);
            }
        }
    }

    auto async_event::event_awaiter::await_ready() const noexcept -> bool
    {
        auto guard = lock_guard{event->_mutex};
        if (event->_signaled)
        {
            if (event->_mode == event_reset_mode::auto_reset)
            {
                event->_signaled = false;
            }
            return true;
        }
        return false;
    }

    auto async_event::event_awaiter::await_suspend(coroutine_handle<> hnd) noexcept -> bool
    {
        waiter.handle = hnd;

        auto guard = lock_guard{event->_mutex};
        if (event->_signaled)
        {
            if (event->_mode == event_reset_mode::auto_reset)
            {
                event->_signaled = false;
            }
            return false;
        }

        waiter.next = event->_waiters;
        event->_waiters = &waiter;
        return true;
    }
} // namespace tempest::job
