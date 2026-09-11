#ifndef tempest_job_parallel_for_hpp
#define tempest_job_parallel_for_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/job/async_event.hpp>
#include <tempest/job/task.hpp>
#include <tempest/job/types.hpp>
#include <tempest/memory.hpp>

namespace tempest::job
{
    class job_system;

    template <typename T = size_t>
    struct range
    {
        T first{0};
        T last{0};

        constexpr range() noexcept = default;
        constexpr range(T f, T l) noexcept : first{f}, last{l}
        {
        }

        [[nodiscard]] constexpr auto begin() const noexcept -> T
        {
            return first;
        }

        [[nodiscard]] constexpr auto end() const noexcept -> T
        {
            return last;
        }

        [[nodiscard]] constexpr auto size() const noexcept -> size_t
        {
            return (last > first) ? static_cast<size_t>(last - first) : 0;
        }

        [[nodiscard]] constexpr auto empty() const noexcept -> bool
        {
            return first >= last;
        }
    };

    namespace partitioner
    {
        struct static_chunk
        {
            size_t chunk_size{0};
        };

        struct guided
        {
            size_t min_chunk_size{1};
        };
    } // namespace partitioner
} // namespace tempest::job

#endif // tempest_job_parallel_for_hpp
