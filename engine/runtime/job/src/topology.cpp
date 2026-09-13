#include <tempest/job/topology.hpp>

#include <tempest/array.hpp>
#include <tempest/algorithm.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/utility.hpp>
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
            constexpr auto path_len = 64U;
            constexpr auto num_buf_len = 16U;
            constexpr auto decimal_base = 10U;

            auto path = array<char, path_len>{};
            auto len = 0;
            auto temp = cpu_id;
            auto num_buf = array<char, num_buf_len>{};
            auto num_digits = 0;
            if (temp == 0)
            {
                num_buf[num_digits++] = '0';
            }
            else
            {
                while (temp > 0)
                {
                    num_buf[num_digits++] = static_cast<char>('0' + (temp % decimal_base));
                    temp /= decimal_base;
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

            const auto buf_len = 32U;
            auto buf = array<char, buf_len>{};
            auto bytes_read = read(file_desc, buf.data(), buf_len - 1);
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
                    val = (val * decimal_base) + static_cast<uint32_t>(buf[i] - '0');
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
                .processor_group = 0,
                .affinity_mask = (1ULL << (i % (sizeof(uint64_t) * char_bit))),
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
                        auto c_type = core_class::performance;
                        if (is_hetero && eff == min_efficiency_class)
                        {
                            c_type = core_class::efficiency;
                        }

                        if (info->Processor.GroupCount > 0)
                        {
                            for (auto g = 0U; g < static_cast<uint32_t>(info->Processor.GroupCount); ++g)
                            {
                                const auto& gm = info->Processor.GroupMask[g];
                                for (auto bit = 0U; bit < 64U; ++bit)
                                {
                                    if ((gm.Mask & (1ULL << bit)) != 0)
                                    {
                                        auto global_idx = static_cast<uint32_t>(gm.Group) * 64U + bit;
                                        topo.cores.push_back(core_info{
                                            .core_id = core_index++,
                                            .logical_core_index = global_idx,
                                            .type = c_type,
                                            .efficiency_class = eff,
                                            .processor_group = gm.Group,
                                            .affinity_mask = 1ULL << bit,
                                        });
                                    }
                                }
                            }
                        }
                        else
                        {
                            topo.cores.push_back(core_info{
                                .core_id = core_index,
                                .logical_core_index = core_index,
                                .type = c_type,
                                .efficiency_class = eff,
                                .processor_group = static_cast<uint16_t>(core_index / 64),
                                .affinity_mask = 1ULL << (core_index % 64),
                            });
                            ++core_index;
                        }
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
                .processor_group = static_cast<uint16_t>(i / 64),
                .affinity_mask = 1ULL << (i % 64),
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
                .processor_group = static_cast<uint16_t>(i / 64),
                .affinity_mask = 1ULL << (i % 64),
            });
        }
        return topo;
#endif
    }

    auto set_thread_affinity(tempest::thread& thr, const core_info& core) -> bool
    {
#if defined(__linux__)
        auto cpuset = cpu_set_t{};
        CPU_ZERO(&cpuset);
        if (core.logical_core_index < CPU_SETSIZE)
        {
            CPU_SET(core.logical_core_index, &cpuset);
        }
        return pthread_setaffinity_np(static_cast<pthread_t>(thr.native_handle()), sizeof(cpu_set_t), &cpuset) == 0;
#elif defined(_WIN32)
        auto group_affinity = GROUP_AFFINITY{};
        group_affinity.Group = core.processor_group;
        group_affinity.Mask = static_cast<KAFFINITY>(core.affinity_mask);
        return SetThreadGroupAffinity(static_cast<HANDLE>(thr.native_handle()), &group_affinity, nullptr) != 0;
#else
        [[maybe_unused]] auto unused_t = &thr;
        [[maybe_unused]] auto unused_core = &core;
        return false;
#endif
    }

    auto set_thread_affinity(tempest::thread& thr, const cpu_mask& mask) -> bool
    {
#if defined(__linux__)
        auto cpuset = cpu_set_t{};
        CPU_ZERO(&cpuset);
        constexpr auto copy_bytes = tempest::min(sizeof(mask), sizeof(cpu_set_t));
        tempest::memcpy(&cpuset, mask.data(), copy_bytes);
        return pthread_setaffinity_np(static_cast<pthread_t>(thr.native_handle()), sizeof(cpu_set_t), &cpuset) == 0;
#elif defined(_WIN32)
        auto first_bit = mask.find_first();
        if (!first_bit.has_value())
        {
            return false;
        }
        auto group_idx = static_cast<uint16_t>(*first_bit / 64);
        auto group_affinity = GROUP_AFFINITY{};
        group_affinity.Group = group_idx;
        group_affinity.Mask = static_cast<KAFFINITY>(mask.word(group_idx));
        return SetThreadGroupAffinity(static_cast<HANDLE>(thr.native_handle()), &group_affinity, nullptr) != 0;
#else
        [[maybe_unused]] auto unused_t = &thr;
        [[maybe_unused]] auto unused_mask = &mask;
        return false;
#endif
    }

    auto set_current_thread_affinity(const core_info& core) -> bool
    {
#if defined(__linux__)
        auto cpuset = cpu_set_t{};
        CPU_ZERO(&cpuset);
        if (core.logical_core_index < CPU_SETSIZE)
        {
            CPU_SET(core.logical_core_index, &cpuset);
        }
        return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#elif defined(_WIN32)
        auto group_affinity = GROUP_AFFINITY{};
        group_affinity.Group = core.processor_group;
        group_affinity.Mask = static_cast<KAFFINITY>(core.affinity_mask);
        return SetThreadGroupAffinity(GetCurrentThread(), &group_affinity, nullptr) != 0;
#else
        [[maybe_unused]] auto unused_core = &core;
        return false;
#endif
    }

    auto set_current_thread_affinity(const cpu_mask& mask) -> bool
    {
#if defined(__linux__)
        auto cpuset = cpu_set_t{};
        CPU_ZERO(&cpuset);
        constexpr auto copy_bytes = tempest::min(sizeof(mask), sizeof(cpu_set_t));
        tempest::memcpy(&cpuset, mask.data(), copy_bytes);
        return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#elif defined(_WIN32)
        auto first_bit = mask.find_first();
        if (!first_bit.has_value())
        {
            return false;
        }
        auto group_idx = static_cast<uint16_t>(*first_bit / 64);
        auto group_affinity = GROUP_AFFINITY{};
        group_affinity.Group = group_idx;
        group_affinity.Mask = static_cast<KAFFINITY>(mask.word(group_idx));
        return SetThreadGroupAffinity(GetCurrentThread(), &group_affinity, nullptr) != 0;
#else
        [[maybe_unused]] auto unused_mask = &mask;
        return false;
#endif
    }
} // namespace tempest::job
