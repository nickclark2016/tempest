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
        auto* expected = static_cast<async_mutex_waiter*>(nullptr);
        return _waiters_in.compare_exchange_strong(expected, _locked_sentinel(), memory_order::acquire,
                                                   memory_order::relaxed);
    }

    auto async_mutex::_enqueue_waiter(async_mutex_waiter* waiter, coroutine_handle<> hnd) noexcept -> bool
    {
        waiter->handle = hnd;
        auto* old_head = _waiters_in.load(memory_order::acquire);
        while (true)
        {
            if (old_head == nullptr)
            {
                if (_waiters_in.compare_exchange_weak(old_head, _locked_sentinel(), memory_order::acquire,
                                                      memory_order::acquire))
                {
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

        // 2. Transfer incoming waiters to _waiters_out
        auto* current_head = _waiters_in.load(memory_order::acquire);
        while (true)
        {
            if (current_head == _locked_sentinel())
            {
                if (_waiters_in.compare_exchange_weak(current_head, nullptr, memory_order::release,
                                                      memory_order::acquire))
                {
                    return;
                }
                continue;
            }

            if (_waiters_in.compare_exchange_weak(current_head, _locked_sentinel(), memory_order::acq_rel,
                                                  memory_order::acquire))
            {
                auto* const reversed = intrusive_mpsc_stack<async_mutex_waiter>::reverse(current_head);
                _waiters_out = reversed->next;
                _resume(reversed->handle);
                return;
            }
        }
    }
} // namespace tempest::job
