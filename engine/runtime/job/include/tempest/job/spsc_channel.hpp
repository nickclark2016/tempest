#ifndef tempest_job_spsc_channel_hpp
#define tempest_job_spsc_channel_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/job/types.hpp>
#include <tempest/memory.hpp>
#include <tempest/utility.hpp>

namespace tempest::job
{
    template <typename T, size_t Capacity>
    class spsc_channel
    {
        static_assert((Capacity & (Capacity - 1)) == 0 && Capacity > 0, "Capacity must be a power of two");

      public:
        using value_type = T;
        static constexpr size_t capacity = Capacity;

        spsc_channel() = default;
        explicit spsc_channel(job_system& sys) noexcept : _sys{&sys}
        {
        }

        ~spsc_channel()
        {
            close();
            T dummy;
            while (try_pop(dummy))
            {
            }
        }

        spsc_channel(const spsc_channel&) = delete;
        spsc_channel& operator=(const spsc_channel&) = delete;
        spsc_channel(spsc_channel&&) = delete;
        spsc_channel& operator=(spsc_channel&&) = delete;

        auto close() noexcept -> void
        {
            if (_closed.exchange(true, memory_order::acq_rel))
            {
                return;
            }

            auto pw = _producer_waiter.exchange(0, memory_order::acq_rel);
            if (pw != 0)
            {
                _resume(coroutine_handle<>::from_address(reinterpret_cast<void*>(pw)));
            }

            auto cw = _consumer_waiter.exchange(0, memory_order::acq_rel);
            if (cw != 0)
            {
                _resume(coroutine_handle<>::from_address(reinterpret_cast<void*>(cw)));
            }
        }

        [[nodiscard]] auto is_closed() const noexcept -> bool
        {
            return _closed.load(memory_order::acquire);
        }

        template <typename U>
        auto try_push(U&& value) -> bool
        {
            if (_closed.load(memory_order::acquire))
            {
                return false;
            }

            const auto current_head = _head.load(memory_order::relaxed);
            if (current_head - _cached_tail == Capacity)
            {
                _cached_tail = _tail.load(memory_order::acquire);
                if (current_head - _cached_tail == Capacity)
                {
                    return false; // Full
                }
            }

            new (&_storage[current_head & (Capacity - 1)]) T(forward<U>(value));
            _head.store(current_head + 1, memory_order::release);

            auto cw = _consumer_waiter.exchange(0, memory_order::acq_rel);
            if (cw != 0)
            {
                _resume(coroutine_handle<>::from_address(reinterpret_cast<void*>(cw)));
            }

            return true;
        }

        auto try_pop(T& out) -> bool
        {
            const auto current_tail = _tail.load(memory_order::relaxed);
            if (current_tail == _cached_head)
            {
                _cached_head = _head.load(memory_order::acquire);
                if (current_tail == _cached_head)
                {
                    return false; // Empty
                }
            }

            auto* ptr = reinterpret_cast<T*>(&_storage[current_tail & (Capacity - 1)]);
            out = move(*ptr);
            ptr->~T();
            _tail.store(current_tail + 1, memory_order::release);

            auto pw = _producer_waiter.exchange(0, memory_order::acq_rel);
            if (pw != 0)
            {
                _resume(coroutine_handle<>::from_address(reinterpret_cast<void*>(pw)));
            }

            return true;
        }

        auto push(T value) -> task<expected<void, job_error>>
        {
            while (true)
            {
                if (try_push(value))
                {
                    co_return expected<void, job_error>{};
                }
                if (is_closed())
                {
                    co_return unexpected(job_error::channel_closed);
                }

                co_await _wait_for_slot();
            }
        }

        auto pop() -> task<expected<T, job_error>>
        {
            while (true)
            {
                T item;
                if (try_pop(item))
                {
                    co_return item;
                }
                if (is_closed())
                {
                    co_return unexpected(job_error::channel_closed);
                }

                co_await _wait_for_item();
            }
        }

      private:
        struct slot_awaiter
        {
            spsc_channel& chan;

            auto await_ready() const noexcept -> bool
            {
                return false;
            }

            auto await_suspend(coroutine_handle<> h) noexcept -> bool
            {
                chan._producer_waiter.store(reinterpret_cast<uintptr_t>(h.address()), memory_order::release);
                if (chan.is_closed())
                {
                    return false;
                }
                return true;
            }

            auto await_resume() const noexcept -> void
            {
            }
        };

        struct item_awaiter
        {
            spsc_channel& chan;

            auto await_ready() const noexcept -> bool
            {
                return false;
            }

            auto await_suspend(coroutine_handle<> h) noexcept -> bool
            {
                chan._consumer_waiter.store(reinterpret_cast<uintptr_t>(h.address()), memory_order::release);
                if (chan.is_closed())
                {
                    return false;
                }
                return true;
            }

            auto await_resume() const noexcept -> void
            {
            }
        };

        auto _wait_for_slot() noexcept -> slot_awaiter
        {
            return slot_awaiter{.chan = *this};
        }

        auto _wait_for_item() noexcept -> item_awaiter
        {
            return item_awaiter{.chan = *this};
        }

        auto _resume(coroutine_handle<> h) noexcept -> void
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

        alignas(alignof(T)) unsigned char _storage[Capacity * sizeof(T)];

        alignas(64) atomic<size_t> _head{0};
        size_t _cached_tail{0};
        atomic<uintptr_t> _producer_waiter{0};

        alignas(64) atomic<size_t> _tail{0};
        size_t _cached_head{0};
        atomic<uintptr_t> _consumer_waiter{0};

        atomic<bool> _closed{false};
        job_system* _sys{nullptr};
    };
} // namespace tempest::job

#endif // tempest_job_spsc_channel_hpp
