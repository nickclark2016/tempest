#ifndef tempest_tasks_task_queue_hpp
#define tempest_tasks_task_queue_hpp

#include <tempest/array.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/deque.hpp>
#include <tempest/int.hpp>
#include <tempest/mutex.hpp>
#include <tempest/optional.hpp>

namespace tempest
{
    using task_handle = coroutine_handle<>;

    template <size_t RingSize = 4>
    class task_queue
    {
      public:
        void push_local(task_handle handle)
        {
            if (_ring_count == RingSize)
            {
                // Ring is full, steal from the local queue and push it to the FIFO queue
                // Then push the new task to the ring
                auto evicted_handle = _ring[_ring_head];

                {
                    [[maybe_unused]] auto fifo_mutex_guard = lock_guard(_fifo_mutex);
                    _fifo_queue.push_back(evicted_handle);
                }

                _ring[_ring_head] = handle;
                _ring_head = (_ring_head + 1) % RingSize;
            }
            else
            {
                const auto ring_index = (_ring_head + _ring_count) % RingSize;
                _ring[ring_index] = handle;
                ++_ring_count;
            }
        }

        [[nodiscard]] auto try_pop_local() -> optional<task_handle>
        {
            if (_ring_count > 0)
            {
                --_ring_count;
                const auto index = (_ring_head + _ring_count) % RingSize;
                return _ring[index];
            }

            [[maybe_unused]] auto fifo_mutex_guard = lock_guard(_fifo_mutex);
            if (!_fifo_queue.empty())
            {
                auto handle = _fifo_queue.front();
                _fifo_queue.pop_front();
                return handle;
            }

            return nullopt;
        }

        [[nodiscard]] auto try_steal_fifo() -> optional<task_handle>
        {
            [[maybe_unused]] auto fifo_mutex_guard = lock_guard(_fifo_mutex);
            if (!_fifo_queue.empty())
            {
                // Steal from the back of the FIFO queue to minimize contention with local pushes
                auto handle = _fifo_queue.back();
                _fifo_queue.pop_back();
                return handle;
            }

            return nullopt;
        }

      private:
        array<task_handle, RingSize> _ring;
        size_t _ring_head = 0;
        size_t _ring_count = 0;

        // FIFO queue - front is for local pushes, back is for stealing
        deque<task_handle> _fifo_queue;
        mutex _fifo_mutex;
    };
} // namespace tempest

#endif // tempest_tasks_task_queue_hpp
