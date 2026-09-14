#include <tempest/job/async_mutex.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/thread.hpp>

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

    async_mutex::~async_mutex()
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

        _drain_in_to_out_locked();

        while (_waiters_out != nullptr)
        {
            auto* const waiter = _waiters_out;
            _waiters_out = waiter->next;
            waiter->state.store(wait_state::retired, memory_order::release);
        }

        _waiters_in.store(nullptr, memory_order::release);
        _claim.store(false, memory_order::release);
    }

    auto async_mutex::_resume(coroutine_handle<> hnd) noexcept -> void
    {
        if (_sys != nullptr)
        {
            _sys->schedule(hnd);
        }
        else
        {
            hnd.resume();
        }
    }

    scoped_lock_guard::scoped_lock_guard(async_mutex& mtx) noexcept : _mutex{&mtx}
    {
    }

    scoped_lock_guard::~scoped_lock_guard()
    {
        if (_mutex != nullptr)
        {
            _mutex->unlock();
        }
    }

    scoped_lock_guard::scoped_lock_guard(scoped_lock_guard&& other) noexcept : _mutex{other._mutex}
    {
        other._mutex = nullptr;
    }

    auto scoped_lock_guard::operator=(scoped_lock_guard&& other) noexcept -> scoped_lock_guard&
    {
        if (this != &other)
        {
            if (_mutex != nullptr)
            {
                _mutex->unlock();
            }
            _mutex = other._mutex;
            other._mutex = nullptr;
        }
        return *this;
    }

    auto async_mutex::try_lock() noexcept -> bool
    {
        auto* expected = static_cast<async_mutex_waiter*>(nullptr);
        return _waiters_in.compare_exchange_strong(expected, _locked_sentinel(), memory_order::acquire,
                                                   memory_order::relaxed);
    }

    auto async_mutex::_enqueue_waiter(async_mutex_waiter* waiter, coroutine_handle<> hnd) noexcept -> bool
    {
        waiter->handle = hnd;
        waiter->state.store(wait_state::pending, memory_order::release);
        auto* old_head = _waiters_in.load(memory_order::acquire);
        while (true)
        {
            if (old_head == nullptr)
            {
                if (_waiters_in.compare_exchange_weak(old_head, _locked_sentinel(), memory_order::acquire,
                                                      memory_order::acquire))
                {
                    waiter->state.store(wait_state::completed, memory_order::relaxed);
                    return false;
                }
                continue;
            }

            waiter->next = (old_head == _locked_sentinel()) ? nullptr : old_head;
            if (_waiters_in.compare_exchange_weak(old_head, waiter, memory_order::release, memory_order::acquire))
            {
                return true;
            }
        }
    }

    auto async_mutex::_drain_in_to_out_locked() noexcept -> void
    {
        auto* current_head = _waiters_in.load(memory_order::acquire);
        while (current_head != nullptr && current_head != _locked_sentinel())
        {
            if (_waiters_in.compare_exchange_weak(current_head, _locked_sentinel(), memory_order::acq_rel,
                                                  memory_order::acquire))
            {
                auto* const reversed = intrusive_mpsc_stack<async_mutex_waiter>::reverse(current_head);
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
                break;
            }
        }
    }

    auto async_mutex::_cancel_waiter(async_mutex_waiter* target) noexcept -> void
    {
        auto expected = wait_state::pending;
        if (!target->state.compare_exchange_strong(expected, wait_state::cancelled, memory_order::acq_rel))
        {
            if (expected == wait_state::completed)
            {
                unlock();
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

        _drain_in_to_out_locked();

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

        target->state.store(wait_state::retired, memory_order::release);
        _claim.store(false, memory_order::release);
    }

    async_mutex::lock_awaiter::~lock_awaiter() noexcept
    {
        if (waiter.state.load(memory_order::acquire) != wait_state::completed)
        {
            mutex->_cancel_waiter(&waiter);
        }
    }

    auto async_mutex::lock_awaiter::await_suspend(coroutine_handle<> hnd) noexcept -> bool
    {
        return mutex->_enqueue_waiter(&waiter, hnd);
    }

    async_mutex::scoped_lock_awaiter::~scoped_lock_awaiter() noexcept
    {
        if (waiter.state.load(memory_order::acquire) != wait_state::completed)
        {
            mutex->_cancel_waiter(&waiter);
        }
    }

    auto async_mutex::scoped_lock_awaiter::await_suspend(coroutine_handle<> hnd) noexcept -> bool
    {
        return mutex->_enqueue_waiter(&waiter, hnd);
    }

    auto async_mutex::unlock() noexcept -> void
    {
        auto to_resume = coroutine_handle<>{nullptr};

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

        while (true)
        {
            // 1. If we have waiters in _waiters_out, pop the first valid one (FIFO order)
            while (_waiters_out != nullptr)
            {
                auto* const waiter = _waiters_out;
                _waiters_out = waiter->next;

                auto expected = wait_state::pending;
                if (waiter->state.compare_exchange_strong(expected, wait_state::completed, memory_order::acq_rel))
                {
                    to_resume = waiter->handle;
                    break;
                }

                // The waiter was cancelled. Mark it retired so its memory can be reclaimed.
                waiter->state.store(wait_state::retired, memory_order::release);
            }

            if (to_resume)
            {
                _claim.store(false, memory_order::release);
                break;
            }

            // 2. _waiters_out is empty. Check if there are incoming waiters in _waiters_in.
            auto* current_in = _waiters_in.load(memory_order::acquire);
            if (current_in != nullptr && current_in != _locked_sentinel())
            {
                _drain_in_to_out_locked();
                continue;
            }

            // 3. No incoming waiters and _waiters_out is empty.
            // Attempt to unlock by transitioning _waiters_in from _locked_sentinel() to nullptr.
            auto* expected = _locked_sentinel();
            if (_waiters_in.compare_exchange_strong(expected, nullptr, memory_order::release, memory_order::acquire))
            {
                // Successfully unlocked
                _claim.store(false, memory_order::release);
                return;
            }

            // A new waiter arrived during the transition attempt. Drain and continue loop.
            _drain_in_to_out_locked();
        }

        if (to_resume)
        {
            _resume(to_resume);
        }
    }
} // namespace tempest::job
