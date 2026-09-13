#ifndef tempest_job_context_hpp
#define tempest_job_context_hpp

#include <tempest/checked.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/int.hpp>
#include <tempest/job/types.hpp>

namespace tempest::job
{
    class job_system;
    class job_allocator;

    template <typename T, typename E>
    class task;

    struct job_context
    {
        non_null<job_system> system;
        non_null<job_allocator> allocator;
        uint32_t worker_index = 0;
        core_class core_type = core_class::performance;

        auto schedule(coroutine_handle<> handle, task_priority priority = task_priority::normal,
                      core_class affinity = core_class::any) const -> void;

        template <typename T, typename E>
        auto schedule(task<T, E>& job, task_priority priority = task_priority::normal,
                      core_class affinity = core_class::any) const -> void;
    };
} // namespace tempest::job

#endif // tempest_job_context_hpp
