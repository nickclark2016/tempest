#ifndef tempest_job_spsc_channel_hpp
#define tempest_job_spsc_channel_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/checked.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/job/types.hpp>
#include <tempest/memory.hpp>
#include <tempest/thread.hpp>
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

            const auto prod_waiter = _producer_waiter.exchange(0, memory_order::seq_cst);
            if (prod_waiter != 0)
            {
                // NOLINTNEXTLINE
                _resume(coroutine_handle<>::from_address(reinterpret_cast<void*>(prod_waiter)));
            }

            const auto consume_waiter = _consumer_waiter.exchange(0, memory_order::seq_cst);
            if (consume_waiter != 0)
            {
                // NOLINTNEXTLINE
                _resume(coroutine_handle<>::from_address(reinterpret_cast<void*>(consume_waiter)));
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

            new (&_storage[(current_head & (Capacity - 1)) * sizeof(T)]) T(forward<U>(value));
            _head.store(current_head + 1, memory_order::release);

            const auto consume_writer = _consumer_waiter.exchange(0, memory_order::seq_cst);
            if (consume_writer != 0)
            {
                // NOLINTNEXTLINE
                _resume(coroutine_handle<>::from_address(reinterpret_cast<void*>(consume_writer)));
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

            auto* ptr = reinterpret_cast<T*>(&_storage[(current_tail & (Capacity - 1)) * sizeof(T)]);
            out = move(*ptr);
            ptr->~T();
            _tail.store(current_tail + 1, memory_order::release);

            const auto produce_writer = _producer_waiter.exchange(0, memory_order::seq_cst);
            if (produce_writer != 0)
            {
                // NOLINTNEXTLINE
                _resume(coroutine_handle<>::from_address(reinterpret_cast<void*>(produce_writer)));
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
            non_null<spsc_channel> chan;

            auto await_ready() const noexcept -> bool
            {
                return false;
            }

            auto await_suspend(coroutine_handle<> hnd) noexcept -> bool
            {
                const auto waiter_val = reinterpret_cast<uintptr_t>(hnd.address());
                chan->_producer_waiter.store(waiter_val, memory_order::seq_cst);

                const auto current_head = chan->_head.load(memory_order::relaxed);
                const auto current_tail = chan->_tail.load(memory_order::seq_cst);
                const auto has_slot = (current_head - current_tail) < Capacity;

                if (has_slot || chan->is_closed())
                {
                    auto expected = waiter_val;
                    if (chan->_producer_waiter.compare_exchange_strong(expected, 0, memory_order::seq_cst,
                                                                       memory_order::relaxed))
                    {
                        return false;
                    }
                }

                return true;
            }

            auto await_resume() const noexcept -> void
            {
            }
        };

        struct item_awaiter
        {
            non_null<spsc_channel> chan;

            auto await_ready() const noexcept -> bool
            {
                return false;
            }

            auto await_suspend(coroutine_handle<> hnd) noexcept -> bool
            {
                const auto waiter_val = reinterpret_cast<uintptr_t>(hnd.address());
                chan->_consumer_waiter.store(waiter_val, memory_order::seq_cst);

                const auto current_head = chan->_head.load(memory_order::seq_cst);
                const auto current_tail = chan->_tail.load(memory_order::relaxed);
                const auto has_item = (current_tail != current_head);

                if (has_item || chan->is_closed())
                {
                    auto expected = waiter_val;
                    if (chan->_consumer_waiter.compare_exchange_strong(expected, 0, memory_order::seq_cst,
                                                                       memory_order::relaxed))
                    {
                        return false;
                    }
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

        auto _resume(coroutine_handle<> hnd) noexcept -> void
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

        alignas(alignof(T)) array<unsigned char, Capacity * sizeof(T)> _storage;

        alignas(hardware_destructive_interference_size) atomic<size_t> _head{0};
        size_t _cached_tail{0};
        atomic<uintptr_t> _producer_waiter{0};

        alignas(hardware_destructive_interference_size) atomic<size_t> _tail{0};
        size_t _cached_head{0};
        atomic<uintptr_t> _consumer_waiter{0};

        atomic<bool> _closed{false};
        job_system* _sys = nullptr;
    };
} // namespace tempest::job

#endif // tempest_job_spsc_channel_hpp
