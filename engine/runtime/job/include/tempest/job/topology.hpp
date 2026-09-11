#ifndef tempest_job_topology_hpp
#define tempest_job_topology_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/job/types.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

namespace tempest::job
{
    struct core_info
    {
        uint32_t core_id{0};
        uint32_t logical_core_index{0};
        core_class type{core_class::performance};
        uint32_t efficiency_class{0};
        uint64_t affinity_mask{0};
    };

    struct cpu_topology
    {
        vector<core_info> cores{};

        [[nodiscard]] auto total_core_count() const noexcept -> uint32_t
        {
            return static_cast<uint32_t>(cores.size());
        }

        [[nodiscard]] auto performance_core_count() const noexcept -> uint32_t
        {
            auto count = 0u;
            for (const auto& c : cores)
            {
                if (c.type == core_class::performance)
                {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] auto efficiency_core_count() const noexcept -> uint32_t
        {
            auto count = 0u;
            for (const auto& c : cores)
            {
                if (c.type == core_class::efficiency)
                {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] auto performance_mask() const noexcept -> uint64_t
        {
            auto mask = uint64_t{0};
            for (const auto& c : cores)
            {
                if (c.type == core_class::performance)
                {
                    mask |= c.affinity_mask;
                }
            }
            return mask;
        }

        [[nodiscard]] auto efficiency_mask() const noexcept -> uint64_t
        {
            auto mask = uint64_t{0};
            for (const auto& c : cores)
            {
                if (c.type == core_class::efficiency)
                {
                    mask |= c.affinity_mask;
                }
            }
            return mask;
        }
    };

    TEMPEST_API auto discover_cpu_topology() -> cpu_topology;
    TEMPEST_API auto set_thread_affinity(tempest::thread& t, uint64_t mask) -> bool;
    TEMPEST_API auto set_current_thread_affinity(uint64_t mask) -> bool;
} // namespace tempest::job

#endif // tempest_job_topology_hpp
