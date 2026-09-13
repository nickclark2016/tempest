#ifndef tempest_job_channel_hpp
#define tempest_job_channel_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/job/types.hpp>
#include <tempest/memory.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/utility.hpp>

namespace tempest::job
{
    struct channel_wait_node
    {
        coroutine_handle<> handle{nullptr};
        job_system* scheduler{nullptr};
        channel_wait_node* next{nullptr};
    };

    template <typename T, size_t Capacity>
    class channel
    {
        static_assert((Capacity & (Capacity - 1)) == 0 && Capacity >= 2,
                      "Channel Capacity must be a power of two and at least 2");

      public:
        using value_type = T;
        static constexpr size_t capacity = Capacity;

        channel()
        {
            for (size_t i = 0; i < Capacity; ++i)
            {
                _cells[i].sequence.store(i, memory_order::relaxed);
            }
        }

        explicit channel(job_system& sys) : _sys{&sys}
        {
            for (size_t i = 0; i < Capacity; ++i)
            {
                _cells[i].sequence.store(i, memory_order::relaxed);
            }
        }

        ~channel()
        {
            close();
            // Drain remaining items
            T item;
            while (try_pop().has_value())
            {
            }
        }

        channel(const channel&) = delete;
        channel(channel&&) noexcept = delete;
        auto operator=(const channel&) -> channel& = delete;
        auto operator=(channel&&) noexcept -> channel& = delete;

        auto close() noexcept -> void
        {
            if (_closed.exchange(true, memory_order::acq_rel))
            {
                return;
            }

            auto* ptr = _producer_waiters.exchange(nullptr, memory_order::acq_rel);
            while (ptr != nullptr)
            {
                auto* const next = ptr->next;
                _resume(ptr);
                ptr = next;
            }

            auto* chan = _consumer_waiters.exchange(nullptr, memory_order::acq_rel);
            while (chan != nullptr)
            {
                auto* const next = chan->next;
                _resume(chan);
                chan = next;
            }
        }

        [[nodiscard]] auto is_closed() const noexcept -> bool
        {
            return _closed.load(memory_order::acquire);
        }

        template <typename U>
        auto try_push(U&& value) -> expected<void, job_error>
        {
            if (_closed.load(memory_order::acquire))
            {
                return unexpected(job_error::channel_closed);
            }

            auto pos = _enqueue_pos.load(memory_order::relaxed);
            for (;;)
            {
                auto& cell = _cells[pos & (Capacity - 1)];
                const auto seq = cell.sequence.load(memory_order::acquire);
                const auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

                if (diff == 0)
                {
                    if (_enqueue_pos.compare_exchange_weak(pos, pos + 1, memory_order::relaxed))
                    {
                        new (cell.storage.data()) T(forward<U>(value));
                        cell.sequence.store(pos + 1, memory_order::release);
                        _notify_consumer();
                        return {};
                    }
                }
                else if (diff < 0)
                {
                    return unexpected(job_error::channel_full);
                }
                else
                {
                    pos = _enqueue_pos.load(memory_order::relaxed);
                }

                if (_closed.load(memory_order::acquire))
                {
                    return unexpected(job_error::channel_closed);
                }
            }
        }

        auto try_pop() -> expected<T, job_error>
        {
            size_t pos = _dequeue_pos.load(memory_order::relaxed);
            for (;;)
            {
                auto& cell = _cells[pos & (Capacity - 1)];
                const auto seq = cell.sequence.load(memory_order::acquire);
                const auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

                if (diff == 0)
                {
                    if (_dequeue_pos.compare_exchange_weak(pos, pos + 1, memory_order::relaxed))
                    {
                        auto* ptr = reinterpret_cast<T*>(cell.storage.data());
                        auto val = move(*ptr);
                        ptr->~T();
                        cell.sequence.store(pos + Capacity, memory_order::release);
                        _notify_producer();
                        return val;
                    }
                }
                else if (diff < 0)
                {
                    if (_closed.load(memory_order::acquire))
                    {
                        return unexpected(job_error::channel_closed);
                    }
                    return unexpected(job_error::channel_empty);
                }
                else
                {
                    pos = _dequeue_pos.load(memory_order::relaxed);
                }
            }
        }

        auto push(T value) -> task<expected<void, job_error>>
        {
            while (true)
            {
                auto res = try_push(value);
                if (res.has_value())
                {
                    co_return res;
                }
                if (res.error() == job_error::channel_closed)
                {
                    co_return res;
                }

                // Channel is full: guarded yield
                co_await _wait_for_slot();
            }
        }

        auto pop() -> task<expected<T, job_error>>
        {
            while (true)
            {
                auto res = try_pop();
                if (res.has_value())
                {
                    co_return res;
                }
                if (res.error() == job_error::channel_closed)
                {
                    co_return res;
                }

                // Channel is empty: guarded yield
                co_await _wait_for_item();
            }
        }

      private:
        struct cell
        {
            atomic<size_t> sequence{0};
            alignas(alignof(T)) array<byte, sizeof(T)> storage{};
        };

        struct producer_waiter
        {
            non_null<channel> chan;
            channel_wait_node node{};

            auto await_ready() const noexcept -> bool
            {
                return false;
            }

            auto await_suspend(coroutine_handle<> hnd) noexcept -> bool
            {
                node.handle = hnd;
                node.scheduler = chan->_sys;
                auto* old_head = chan->_producer_waiters.load(memory_order::relaxed);
                do
                {
                    node.next = old_head;
                } while (!chan->_producer_waiters.compare_exchange_weak(old_head, &node, memory_order::release));

                // If space freed or closed, avoid missing wakeups
                return !chan->is_closed();
            }

            auto await_resume() const noexcept -> void
            {
            }

            [[nodiscard]] constexpr auto suspend_reason_tag() const noexcept -> profiler::suspend_reason
            {
                return profiler::suspend_reason::channel_full;
            }
        };

        struct consumer_waiter
        {
            non_null<channel> chan;
            channel_wait_node node{};

            auto await_ready() const noexcept -> bool
            {
                return false;
            }

            auto await_suspend(coroutine_handle<> hnd) noexcept -> bool
            {
                node.handle = hnd;
                node.scheduler = chan->_sys;
                auto* old_head = chan->_consumer_waiters.load(memory_order::relaxed);
                do
                {
                    node.next = old_head;
                } while (!chan->_consumer_waiters.compare_exchange_weak(old_head, &node, memory_order::release));

                return !chan->is_closed();
            }

            auto await_resume() const noexcept -> void
            {
            }

            [[nodiscard]] constexpr auto suspend_reason_tag() const noexcept -> profiler::suspend_reason
            {
                return profiler::suspend_reason::channel_empty;
            }
        };

        auto _wait_for_slot() noexcept -> producer_waiter
        {
            return producer_waiter{.chan = *this};
        }

        auto _wait_for_item() noexcept -> consumer_waiter
        {
            return consumer_waiter{.chan = *this};
        }

        static auto _resume(channel_wait_node* node) -> void
        {
            if (node->scheduler != nullptr)
            {
                node->scheduler->schedule(node->handle);
            }
            else
            {
                node->handle.resume();
            }
        }

        auto _notify_consumer() noexcept -> void
        {
            auto* old_head = _consumer_waiters.load(memory_order::acquire);
            while (old_head != nullptr)
            {
                if (_consumer_waiters.compare_exchange_weak(old_head, old_head->next, memory_order::acq_rel))
                {
                    _resume(old_head);
                    return;
                }
            }
        }

        auto _notify_producer() noexcept -> void
        {
            auto* old_head = _producer_waiters.load(memory_order::acquire);
            while (old_head != nullptr)
            {
                if (_producer_waiters.compare_exchange_weak(old_head, old_head->next, memory_order::acq_rel))
                {
                    _resume(old_head);
                    return;
                }
            }
        }

        array<cell, Capacity> _cells{};
        atomic<size_t> _enqueue_pos{0};
        atomic<size_t> _dequeue_pos{0};
        atomic<bool> _closed{false};
        atomic<channel_wait_node*> _producer_waiters{nullptr};
        atomic<channel_wait_node*> _consumer_waiters{nullptr};
        job_system* _sys{nullptr};
    };
} // namespace tempest::job

#endif // tempest_job_channel_hpp
