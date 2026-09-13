#include <tempest/job/context.hpp>

#include <tempest/job/job_system.hpp>

namespace tempest::job
{
    auto job_context::schedule(coroutine_handle<> handle, task_priority priority, core_class affinity) const -> void
    {
        system->schedule(*this, handle, priority, affinity);
    }
} // namespace tempest::job
