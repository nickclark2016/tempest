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

        auto* old_head = _waiters_in.load(memory_order::relaxed);
        do
        {
            waiter->next = old_head;
        } while (!_waiters_in.compare_exchange_weak(old_head, waiter, memory_order::release));

        // If we successfully enqueued the waiter, we need to check if the mutex is still locked.
        // If the mutex is unlocked, we can try to acquire it and resume the waiter immediately
        return _locked.load(memory_order::acquire) == 0 && try_lock();
    }

    auto async_mutex::lock_awaiter::await_suspend(coroutine_handle<> hnd) noexcept -> bool
    {
        return mutex-> _enqueue_waiter(&waiter, hnd);
    }

    auto async_mutex::scoped_lock_awaiter::await_suspend(coroutine_handle<> hnd) noexcept -> bool
    {
        return mutex-> _enqueue_waiter(&waiter, hnd);
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
        auto* incoming = _waiters_in.exchange(nullptr, memory_order::acq_rel);
        if (incoming != nullptr)
        {
            // Reverse list
            async_mutex_waiter* reversed = nullptr;
            while (incoming != nullptr)
            {
                auto* next = incoming->next;
                incoming->next = reversed;
                reversed = incoming;
                incoming = next;
            }

            _waiters_out = reversed->next;
            _resume(reversed->handle);
            return;
        }

        // 3. No waiters: release lock
        _locked.store(0, memory_order::release);

        // Double-check race where a waiter enqueued right before we stored 0
        if (_waiters_in.load(memory_order::acquire) != nullptr && try_lock())
        {
            // Recurse to pop that waiter
            unlock();
        }
    }
} // namespace tempest::job
