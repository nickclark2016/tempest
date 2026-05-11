#ifndef tempest_tasks_scheduler_hpp
#define tempest_tasks_scheduler_hpp

#include <tempest/int.hpp>

namespace tempest
{
    enum class worker_lane : uint8_t
    {
        performance,
        efficiency,
    };

    struct hardware_topology
    {
        uint64_t p_core_mask;
        uint64_t e_core_mask;
        uint32_t p_core_count;
        uint32_t e_core_count;

        static auto query() -> hardware_topology;
    };

    class task_scheduler;

    class worker
    {
      public:
        worker(task_scheduler& sched, worker_lane assignment, uint32_t tid, hardware_topology topology);

      private:
        void _run();

        hardware_topology _topology;
    };
} // namespace tempest

#endif // tempest_tasks_scheduler_hpp
