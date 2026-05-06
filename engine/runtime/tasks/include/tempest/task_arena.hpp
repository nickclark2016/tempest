#ifndef tempest_tasks_task_arena_hpp
#define tempest_tasks_task_arena_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/vector.hpp>

namespace tempest
{
    class TEMPEST_API task_memory_arena
    {
      public:
        [[nodiscard]] auto allocate(size_t size) noexcept -> void*;
        auto deallocate(void* ptr, size_t size) noexcept -> void;

        void reset();

      private:
        struct segment {};

        vector<segment> _segments;
    };
} // namespace tempest

#endif // tempest_tasks_task_arena_hpp
