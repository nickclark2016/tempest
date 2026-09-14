#include <tempest/job/async_mutex.hpp>
#include <tempest/job/job_system.hpp>

namespace tempest::job
{
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
        uint32_t expected = 0;
        return _locked.compare_exchange_strong(expected, 1, memory_order::acquire);
    }

    auto async_mutex::_enqueue_waiter(async_mutex_waiter* waiter, coroutine_handle<> hnd) noexcept -> bool
    {
        waiter->handle = hnd;
        _waiters_in.push(non_null{*waiter});
        return true;
    }

    auto async_mutex::lock_awaiter::await_suspend(coroutine_handle<> hnd) noexcept -> bool
    {
        return mutex->_enqueue_waiter(&waiter, hnd);
    }

    auto async_mutex::scoped_lock_awaiter::await_suspend(coroutine_handle<> hnd) noexcept -> bool
    {
        return mutex->_enqueue_waiter(&waiter, hnd);
    }

    auto async_mutex::unlock() noexcept -> void
    {
        // 1. If we have waiters in _waiters_out, pop the first one (FIFO order)
        if (_waiters_out != nullptr)
        {
            auto* const waiter = _waiters_out;
            _waiters_out = _waiters_out->next;
            // Hand off lock ownership directly to waiter
            _resume(waiter->handle);
            return;
        }

        // 2. Transfer incoming waiters to _waiters_out and reverse (LIFO -> FIFO)
        auto* const incoming = _waiters_in.drain();
        if (incoming != nullptr)
        {
            auto* const reversed = intrusive_mpsc_stack<async_mutex_waiter>::reverse(incoming);
            _waiters_out = reversed->next;
            _resume(reversed->handle);
            return;
        }

        // 3. No waiters: release lock
        _locked.store(0, memory_order::seq_cst);

        // Double-check race where a waiter enqueued right before we stored 0
        if (!_waiters_in.empty(memory_order::seq_cst) && try_lock())
        {
            // Recurse to pop that waiter
            unlock();
        }
    }
} // namespace tempest::job
