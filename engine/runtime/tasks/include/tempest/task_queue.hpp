#ifndef tempest_tasks_task_queue_hpp
#define tempest_tasks_task_queue_hpp

#include <tempest/coroutine.hpp>
#include <tempest/int.hpp>
#include <tempest/mutex.hpp>
#include <tempest/optional.hpp>

namespace tempest
{
    template <typename T = void>
    using task_handle = coroutine_handle<>;

    template <size_t RingSize = 4>
    class task_queue
    {
      public:
        void push_local(task_handle handle);
        optional<task_handle> try_pop_local();

        optional<task_handle> try_steal_fifo();

      private:
    };
}

#endif // tempest_tasks_task_queue_hpp
