#include <tempest/job/async_event.hpp>
#include <tempest/job/job_system.hpp>
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

    async_event::~async_event()
    {
        for (auto spin = 0U; _claim.exchange(true, memory_order::acquire); ++spin)
        {
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

        while (_waiters_out != nullptr)
        {
            auto* const waiter = _waiters_out;
            _waiters_out = waiter->next;
            waiter->state.store(wait_state::retired, memory_order::release);
        }

        auto* in_head = _waiters_in.drain();
        if (in_head != nullptr && in_head != _signaled_sentinel())
        {
            while (in_head != nullptr)
            {
                auto* const next = in_head->next;
                in_head->state.store(wait_state::retired, memory_order::release);
                in_head = next;
            }
        }

        _claim.store(false, memory_order::release);
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
        [[maybe_unused]] const auto old_value =
            _waiters_in.compare_exchange_strong(expected_sentinel, nullptr, memory_order::acq_rel, memory_order::relaxed);
    }

    [[nodiscard]] auto async_event::is_set() const noexcept -> bool
    {
        return _waiters_in.load(memory_order::acquire) == _signaled_sentinel();
    }

    auto async_event::set() noexcept -> void
    {
        auto* const sys = _sys;

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
                auto expected = wait_state::pending;
                if (current_waiter->state.compare_exchange_strong(expected, wait_state::completed, memory_order::acq_rel))
                {
                    if (sys != nullptr)
                    {
                        sys->schedule(current_waiter->handle);
                    }
                    else
                    {
                        current_waiter->handle.resume();
                    }
                }
                else
                {
                    current_waiter->state.store(wait_state::retired, memory_order::release);
                }
                current_waiter = next_waiter;
            }
            return;
        }

        // Auto-reset mode:
        auto to_resume = coroutine_handle<>{nullptr};
        while (true)
        {
            for (auto spin = 0U; _claim.exchange(true, memory_order::acquire); ++spin)
            {
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

            if (_waiters_out == nullptr)
            {
                if (_waiters_in.load(memory_order::acquire) == _signaled_sentinel())
                {
                    _claim.store(false, memory_order::release);
                    return;
                }

                auto* const incoming = _waiters_in.drain();
                if (incoming == _signaled_sentinel())
                {
                    _waiters_in.store(_signaled_sentinel(), memory_order::release);
                    _claim.store(false, memory_order::release);
                    return;
                }
                if (incoming != nullptr)
                {
                    _waiters_out = intrusive_mpsc_stack<async_event_waiter>::reverse(incoming);
                }
            }

            while (_waiters_out != nullptr)
            {
                auto* const waiter = _waiters_out;
                _waiters_out = waiter->next;

                auto expected = wait_state::pending;
                if (waiter->state.compare_exchange_strong(expected, wait_state::completed, memory_order::acq_rel))
                {
                    to_resume = waiter->handle;
                    _claim.store(false, memory_order::release);
                    break;
                }

                waiter->state.store(wait_state::retired, memory_order::release);
            }

            if (to_resume)
            {
                break;
            }

            // No waiters in _waiters_out and ingress was empty.
            // Release claim BEFORE publishing the sentinel, because the instant
            // _signaled_sentinel() is published, a consumer can wake up and destroy `this`.
            _claim.store(false, memory_order::release);

            auto* expected = static_cast<async_event_waiter*>(nullptr);
            if (_waiters_in.compare_exchange_strong(expected, _signaled_sentinel(), memory_order::acq_rel,
                                                    memory_order::acquire))
            {
                // Successfully signaled. `this` might be deleted by consumer immediately.
                return;
            }

            if (expected == _signaled_sentinel())
            {
                // Already signaled by another concurrent set().
                return;
            }

            // A waiter arrived during the window! Loop to re-acquire claim and pop it.
        }

        if (to_resume)
        {
            if (sys != nullptr)
            {
                sys->schedule(to_resume);
            }
            else
            {
                to_resume.resume();
            }
        }
    }

    auto async_event::_cancel_waiter(async_event_waiter* target) noexcept -> void
    {
        auto expected = wait_state::pending;
        if (!target->state.compare_exchange_strong(expected, wait_state::cancelled, memory_order::acq_rel))
        {
            if (expected == wait_state::completed && _mode == event_reset_mode::auto_reset)
            {
                set();
            }
            return;
        }

        for (auto spin = 0U; _claim.exchange(true, memory_order::acquire); ++spin)
        {
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

        if (_mode == event_reset_mode::auto_reset)
        {
            if (_waiters_in.load(memory_order::acquire) != _signaled_sentinel())
            {
                auto* const incoming = _waiters_in.drain();
                if (incoming == _signaled_sentinel())
                {
                    _waiters_in.store(_signaled_sentinel(), memory_order::release);
                }
                else if (incoming != nullptr)
                {
                    auto* const reversed = intrusive_mpsc_stack<async_event_waiter>::reverse(incoming);
                    if (_waiters_out == nullptr)
                    {
                        _waiters_out = reversed;
                    }
                    else
                    {
                        auto* tail = _waiters_out;
                        while (tail->next != nullptr)
                        {
                            tail = tail->next;
                        }
                        tail->next = reversed;
                    }
                }
            }

            auto** curr = &_waiters_out;
            while (*curr != nullptr)
            {
                if (*curr == target)
                {
                    *curr = target->next;
                    break;
                }
                curr = &((*curr)->next);
            }
        }
        else // manual reset mode
        {
            auto* current_in = _waiters_in.load(memory_order::acquire);
            if (current_in != _signaled_sentinel())
            {
                auto* incoming = _waiters_in.drain();
                if (incoming == _signaled_sentinel())
                {
                    _waiters_in.store(_signaled_sentinel(), memory_order::release);
                }
                else if (incoming != nullptr)
                {
                    auto* filtered_head = static_cast<async_event_waiter*>(nullptr);
                    auto* filtered_tail = static_cast<async_event_waiter*>(nullptr);

                    auto* curr = incoming;
                    while (curr != nullptr)
                    {
                        auto* const next = curr->next;
                        if (curr != target)
                        {
                            curr->next = nullptr;
                            if (filtered_head == nullptr)
                            {
                                filtered_head = curr;
                                filtered_tail = curr;
                            }
                            else
                            {
                                filtered_tail->next = curr;
                                filtered_tail = curr;
                            }
                        }
                        curr = next;
                    }

                    if (filtered_head != nullptr)
                    {
                        _waiters_in.push_range(non_null<async_event_waiter>{filtered_head},
                                               non_null<async_event_waiter>{filtered_tail});
                    }
                }
            }
        }

        target->state.store(wait_state::retired, memory_order::release);
        _claim.store(false, memory_order::release);
    }

    async_event::event_awaiter::~event_awaiter() noexcept
    {
        if (waiter.state.load(memory_order::acquire) != wait_state::completed)
        {
            event->_cancel_waiter(&waiter);
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
        waiter.state.store(wait_state::pending, memory_order::release);

        if (event->_mode == event_reset_mode::manual)
        {
            auto* old_head = event->_waiters_in.load(memory_order::acquire);
            while (true)
            {
                if (old_head == event->_signaled_sentinel())
                {
                    waiter.state.store(wait_state::completed, memory_order::relaxed);
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
                        waiter.state.store(wait_state::completed, memory_order::relaxed);
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
