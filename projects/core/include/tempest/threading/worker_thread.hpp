#ifndef tempest_core_threading_worker_thread_hpp
#define tempest_core_threading_worker_thread_hpp

#include "work_steal_queue.hpp";

#include <concepts>
#include <cstddef>
#include <exception>
#include <thread>

namespace tempest::core::threading
{
    class task_node;
    class scheduler;
    class worker_thread_view;

    class worker_thread
    {
      public:
        [[nodiscard]] inline std::size_t worker_id() const noexcept
        {
            return _thread_id;
        }

        [[nodiscard]] inline std::thread* thread() const noexcept
        {
            return _thread;
        }

        [[nodiscard]] inline std::size_t work_steal_queue_size() const noexcept
        {
            return _wsq.size();
        }

        [[nodiscard]] std::size_t work_steal_queue_capacity() const noexcept
        {
            return _wsq.capacity();
        }

        [[nodiscard]] std::size_t pinned_queue_size() const noexcept
        {
            return static_cast<std::size_t>(_pinned.size());
        }

        [[nodiscard]] std::size_t pinned_queue_capacity() const noexcept
        {
            return static_cast<std::size_t>(_pinned.capacity());
        }

      private:
        friend class scheduler;
        friend class worker_thread_view;

        std::size_t _thread_id;
        scheduler* _owner;
        std::thread* _thread;
        work_steal_queue<task_node*> _wsq;
        work_steal_queue<task_node*> _pinned;
        task_node* _cached_node;
    };

    class worker_thread_view
    {
      public:
        explicit worker_thread_view(const worker_thread& worker);

        [[nodiscard]] inline std::size_t id() const noexcept
        {
            return _worker.worker_id();
        }

        [[nodiscard]] inline std::size_t work_steal_queue_size() const noexcept
        {
            return _worker.work_steal_queue_size();
        }

        [[nodiscard]] inline std::size_t work_steal_queue_capacity() const noexcept
        {
            return _worker.work_steal_queue_capacity();
        }

        [[nodiscard]] inline std::size_t pinned_queue_size() const noexcept
        {
            return _worker.pinned_queue_size();
        }

        [[nodiscard]] inline std::size_t pinned_queue_capacity() const noexcept
        {
            return _worker.pinned_queue_capacity();
        }

      private:
        friend class scheduler;

        const worker_thread& _worker;
    };

    class base_worker_thread
    {
      public:
        virtual ~base_worker_thread() = default;

        virtual void pre_schedule(worker_thread& worker) = 0;
        virtual void post_scheduler(worker_thread& worker, std::exception_ptr error) = 0;
    };

    template <std::derived_from<base_worker_thread> T, typename... Args>
    inline std::shared_ptr<T> make_worker(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
} // namespace tempest::core::threading

#endif // tempest_core_threading_worker_thread_hpp