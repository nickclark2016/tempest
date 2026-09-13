#include <tempest/job/topology.hpp>

#include <tempest/array.hpp>
#include <tempest/algorithm.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/vector.hpp>

#if defined(__linux__)
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace tempest::job
{
    namespace
    {
#if defined(__linux__)
        auto read_core_capacity(uint32_t cpu_id) -> optional<uint32_t>
        {
            auto path = array<char, 64>{};
            auto len = 0;
            auto temp = cpu_id;
            auto num_buf = array<char, 16>{};
            auto num_digits = 0;
            if (temp == 0)
            {
                num_buf[num_digits++] = '0';
            }
            else
            {
                while (temp > 0)
                {
                    num_buf[num_digits++] = static_cast<char>('0' + (temp % 10));
                    temp /= 10;
                }
            }

            const auto* const prefix = "/sys/devices/system/cpu/cpu";
            for (auto i = 0; prefix[i] != '\0'; ++i)
            {
                path[len++] = prefix[i];
            }
            for (auto i = num_digits; i > 0; --i)
            {
                path[len++] = num_buf[i - 1];
            }
            const auto* const suffix = "/cpu_capacity";
            for (auto i = 0; suffix[i] != '\0'; ++i)
            {
                path[len++] = suffix[i];
            }
            path[len] = '\0';

            auto file_desc = open(path.data(), O_RDONLY);
            if (file_desc < 0)
            {
                return nullopt;
            }

            auto buf = array<char, 32>{};
            auto bytes_read = read(file_desc, buf.data(), sizeof(buf) - 1);
            close(file_desc);

            if (bytes_read <= 0)
            {
                return nullopt;
            }

            auto val = 0U;
            for (auto i = 0; i < bytes_read; ++i)
            {
                if (buf[i] >= '0' && buf[i] <= '9')
                {
                    val = val * 10 + static_cast<uint32_t>(buf[i] - '0');
                }
                else if (buf[i] == '\n' || buf[i] == '\0')
                {
                    break;
                }
            }
            return val;
        }
#endif
    } // namespace

    auto discover_cpu_topology() -> cpu_topology
    {
        auto topo = cpu_topology{};

#if defined(__linux__)
        auto nprocs = sysconf(_SC_NPROCESSORS_ONLN);
        auto count = (nprocs > 0) ? static_cast<uint32_t>(nprocs) : tempest::thread::hardware_concurrency();
        if (count == 0)
        {
            count = 1;
        }

        constexpr auto default_capability = 1024U;
        auto capacities = vector<uint32_t>{};
        capacities.resize(count, default_capability);
        auto has_heterogeneous = false;
        auto min_cap = default_capability;
        auto max_cap = default_capability;
        auto read_any = false;

        for (auto i = 0U; i < count; ++i)
        {
            auto cap = read_core_capacity(i);
            if (cap.has_value())
            {
                capacities[i] = *cap;
                if (!read_any)
                {
                    min_cap = *cap;
                    max_cap = *cap;
                    read_any = true;
                }
                else
                {
                    min_cap = tempest::min(min_cap, *cap);
                    max_cap = tempest::max(max_cap, *cap);
                }
            }
        }

        if (read_any && min_cap < max_cap)
        {
            has_heterogeneous = true;
        }

        auto threshold = (min_cap + max_cap) / 2;

        for (auto i = 0U; i < count; ++i)
        {
            auto info = core_info{
                .core_id = i,
                .logical_core_index = i,
                .type = core_class::performance,
                .efficiency_class = 0,
                .affinity_mask = (i < 64) ? (1ULL << i) : 0,
            };

            if (has_heterogeneous)
            {
                if (capacities[i] <= threshold)
                {
                    info.type = core_class::efficiency;
                    info.efficiency_class = 0;
                }
                else
                {
                    info.type = core_class::performance;
                    info.efficiency_class = 1;
                }
            }

            topo.cores.push_back(info);
        }

        return topo;

#elif defined(_WIN32)
        auto length = DWORD(0);
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && length > 0)
        {
            auto buffer = vector<uint8_t>{};
            buffer.resize(length);

            if (GetLogicalProcessorInformationEx(RelationProcessorCore,
                                                 reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),
                                                 &length))
            {
                auto offset = 0U;
                auto max_efficiency_class = 0U;
                auto min_efficiency_class = ~0U;
                auto core_count = 0U;

                // First pass: inspect efficiency classes
                while (offset < length)
                {
                    auto* info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
                    if (info->Relationship == RelationProcessorCore)
                    {
                        auto eff = static_cast<uint32_t>(info->Processor.EfficiencyClass);
                        max_efficiency_class = tempest::max(max_efficiency_class, eff);
                        min_efficiency_class = tempest::min(min_efficiency_class, eff);
                        ++core_count;
                    }
                    offset += info->Size;
                }

                auto is_hetero = (min_efficiency_class < max_efficiency_class);

                // Second pass: construct core_info
                offset = 0U;
                auto core_index = 0U;
                while (offset < length)
                {
                    auto* info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
                    if (info->Relationship == RelationProcessorCore)
                    {
                        auto eff = static_cast<uint32_t>(info->Processor.EfficiencyClass);
                        auto affinity = uint64_t{0};
                        if (info->Processor.GroupCount > 0)
                        {
                            affinity = static_cast<uint64_t>(info->Processor.GroupMask[0].Mask);
                        }
                        else
                        {
                            affinity = (core_index < 64) ? (1ULL << core_index) : 0;
                        }

                        auto c_type = core_class::performance;
                        if (is_hetero && eff == min_efficiency_class)
                        {
                            c_type = core_class::efficiency;
                        }

                        topo.cores.push_back(core_info{
                            .core_id = core_index,
                            .logical_core_index = core_index,
                            .type = c_type,
                            .efficiency_class = eff,
                            .affinity_mask = affinity,
                        });
                        ++core_index;
                    }
                    offset += info->Size;
                }

                if (!topo.cores.empty())
                {
                    return topo;
                }
            }
        }

        // Fallback for Windows if GetLogicalProcessorInformationEx failed
        auto hw = tempest::thread::hardware_concurrency();
        if (hw == 0)
        {
            hw = 1;
        }
        for (auto i = 0U; i < hw; ++i)
        {
            topo.cores.push_back(core_info{
                .core_id = i,
                .logical_core_index = i,
                .type = core_class::performance,
                .efficiency_class = 0,
                .affinity_mask = (i < 64) ? (1ULL << i) : 0,
            });
        }
        return topo;

#else
        // Fallback for generic/other OS
        auto hw = tempest::thread::hardware_concurrency();
        if (hw == 0)
        {
            hw = 1;
        }
        for (auto i = 0U; i < hw; ++i)
        {
            topo.cores.push_back(core_info{
                .core_id = i,
                .logical_core_index = i,
                .type = core_class::performance,
                .efficiency_class = 0,
                .affinity_mask = (i < 64) ? (1ULL << i) : 0,
            });
        }
        return topo;
#endif
    }

    auto set_thread_affinity(tempest::thread& t, uint64_t mask) -> bool
    {
#if defined(__linux__)
        auto cpuset = cpu_set_t{};
        CPU_ZERO(&cpuset);
        for (auto i = 0U; i < 64U; ++i)
        {
            if ((mask & (1ULL << i)) != 0)
            {
                CPU_SET(i, &cpuset);
            }
        }
        return pthread_setaffinity_np(static_cast<pthread_t>(t.native_handle()), sizeof(cpu_set_t), &cpuset) == 0;
#elif defined(_WIN32)
        return SetThreadAffinityMask(static_cast<HANDLE>(t.native_handle()), static_cast<DWORD_PTR>(mask)) != 0;
#else
        (void)t;
        (void)mask;
        return false;
#endif
    }

    auto set_current_thread_affinity(uint64_t mask) -> bool
    {
#if defined(__linux__)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (auto i = 0U; i < 64U; ++i)
        {
            if ((mask & (1ULL << i)) != 0)
            {
                CPU_SET(i, &cpuset);
            }
        }
        return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#elif defined(_WIN32)
        return SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(mask)) != 0;
#else
        (void)mask;
        return false;
#endif
    }
} // namespace tempest::job
