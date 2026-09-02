#include <tempest/algorithm.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/profiler/statistics.hpp>
#include <tempest/utility.hpp>

#include <algorithm>

namespace tempest::profiler
{
    namespace
    {
        auto compute_percentile(span<const double> sorted_durations, double p) noexcept -> double
        {
            if (sorted_durations.empty())
            {
                return 0.0;
            }
            if (sorted_durations.size() == 1)
            {
                return sorted_durations[0];
            }

            const auto rank = (p / 100.0) * static_cast<double>(sorted_durations.size() - 1);
            const auto k = static_cast<size_t>(rank);
            const auto frac = rank - static_cast<double>(k);

            if (k + 1 >= sorted_durations.size())
            {
                return sorted_durations.back();
            }

            return sorted_durations[k] + frac * (sorted_durations[k + 1] - sorted_durations[k]);
        }
    } // namespace

    auto compute_zone_statistics(span<const zone_record> zones) -> zone_statistics
    {
        if (zones.empty())
        {
            return zone_statistics{};
        }

        auto stats = zone_statistics{};
        stats.zone_name = zones[0].name;
        stats.count = static_cast<uint64_t>(zones.size());

        auto durations = vector<double>{};
        durations.reserve(zones.size());

        auto total_sum = 0.0;
        for (const auto& z : zones)
        {
            const auto dur = static_cast<double>(z.end_ns >= z.start_ns ? (z.end_ns - z.start_ns) : 0);
            durations.push_back(dur);
            total_sum += dur;
        }

        std::sort(durations.begin(), durations.end());

        stats.min_ns = durations.front();
        stats.max_ns = durations.back();
        stats.mean_ns = total_sum / static_cast<double>(durations.size());

        auto variance_sum = 0.0;
        for (const auto d : durations)
        {
            const auto diff = d - stats.mean_ns;
            variance_sum += diff * diff;
        }
        const auto variance = variance_sum / static_cast<double>(durations.size());
        stats.std_deviation_ns = tempest::math::sqrt(variance);

        const auto sorted_span = span<const double>{durations.data(), durations.size()};
        stats.p50_ns = compute_percentile(sorted_span, 50.0);
        stats.p90_ns = compute_percentile(sorted_span, 90.0);
        stats.p95_ns = compute_percentile(sorted_span, 95.0);
        stats.p99_ns = compute_percentile(sorted_span, 99.0);

        return stats;
    }

    auto compute_all_zone_statistics(const capture_session_data& capture) -> vector<zone_statistics>
    {
        struct zone_group
        {
            string_view name{};
            vector<zone_record> zones{};
        };

        auto groups = vector<zone_group>{};

        for (const auto& track : capture.tracks)
        {
            for (const auto& z : track.zones)
            {
                auto* found = static_cast<zone_group*>(nullptr);
                for (auto& g : groups)
                {
                    if (g.name == z.name)
                    {
                        found = &g;
                        break;
                    }
                }

                if (!found)
                {
                    groups.push_back(zone_group{
                        .name = z.name,
                        .zones = {},
                    });
                    found = &groups.back();
                }

                found->zones.push_back(z);
            }
        }

        auto result = vector<zone_statistics>{};
        result.reserve(groups.size());

        for (const auto& g : groups)
        {
            const auto z_span = span<const zone_record>{g.zones.data(), g.zones.size()};
            result.push_back(compute_zone_statistics(z_span));
        }

        std::sort(result.begin(), result.end(),
                  [](const zone_statistics& a, const zone_statistics& b) { return a.zone_name < b.zone_name; });

        return result;
    }

    auto compute_rolling_averages(span<const double> values, size_t window_size) -> vector<double>
    {
        if (values.empty() || window_size == 0)
        {
            return {};
        }

        auto result = vector<double>{};
        result.reserve(values.size());

        auto current_sum = 0.0;
        for (auto i = size_t{0}; i < values.size(); ++i)
        {
            current_sum += values[i];
            if (i >= window_size)
            {
                current_sum -= values[i - window_size];
            }
            const auto count = tempest::min(i + 1, window_size);
            result.push_back(current_sum / static_cast<double>(count));
        }

        return result;
    }

    auto is_gpu_submit_zone_name(string_view name) noexcept -> bool
    {
        if (name.empty())
        {
            return false;
        }
        return ends_with(name, "Submit") || ends_with(name, "submit") || ends_with(name, "SubmitEnvelope") ||
               name == "Queue Submit";
    }

    auto compute_exclusive_durations(span<const zone_record> zones) -> vector<zone_exclusive_time>
    {
        if (zones.empty())
        {
            return {};
        }

        auto result = vector<zone_exclusive_time>{};
        result.reserve(zones.size());

        for (auto i = size_t{0}; i < zones.size(); ++i)
        {
            const auto& z = zones[i];
            const auto total_ns = z.end_ns >= z.start_ns ? (z.end_ns - z.start_ns) : uint64_t{0};

            auto child_dur_sum = uint64_t{0};
            for (auto j = size_t{0}; j < zones.size(); ++j)
            {
                if (i == j)
                {
                    continue;
                }
                const auto& other = zones[j];
                if (other.depth == z.depth + 1 && other.start_ns >= z.start_ns && other.end_ns <= z.end_ns)
                {
                    const auto child_dur =
                        other.end_ns >= other.start_ns ? (other.end_ns - other.start_ns) : uint64_t{0};
                    child_dur_sum += child_dur;
                }
            }

            const auto exclusive_ns = total_ns >= child_dur_sum ? (total_ns - child_dur_sum) : uint64_t{0};
            result.push_back(zone_exclusive_time{
                .name = z.name,
                .total_duration_ns = total_ns,
                .exclusive_duration_ns = exclusive_ns,
                .depth = z.depth,
            });
        }

        return result;
    }

    auto compute_exclusive_durations_telemetry(span<const telemetry_zone> zones) -> vector<zone_exclusive_time>
    {
        if (zones.empty())
        {
            return {};
        }

        auto result = vector<zone_exclusive_time>{};
        result.reserve(zones.size());

        for (auto i = size_t{0}; i < zones.size(); ++i)
        {
            const auto& z = zones[i];
            const auto total_ns = z.end_ns >= z.start_ns ? (z.end_ns - z.start_ns) : uint64_t{0};

            auto child_dur_sum = uint64_t{0};
            for (auto j = size_t{0}; j < zones.size(); ++j)
            {
                if (i == j)
                {
                    continue;
                }
                const auto& other = zones[j];
                if (other.depth == z.depth + 1 && other.start_ns >= z.start_ns && other.end_ns <= z.end_ns)
                {
                    const auto child_dur =
                        other.end_ns >= other.start_ns ? (other.end_ns - other.start_ns) : uint64_t{0};
                    child_dur_sum += child_dur;
                }
            }

            const auto exclusive_ns = total_ns >= child_dur_sum ? (total_ns - child_dur_sum) : uint64_t{0};
            result.push_back(zone_exclusive_time{
                .name = string_view{z.name.data(), z.name.size()},
                .total_duration_ns = total_ns,
                .exclusive_duration_ns = exclusive_ns,
                .depth = z.depth,
            });
        }

        return result;
    }

    auto extract_hot_zones(span<const telemetry_track> tracks, bool is_gpu, size_t max_results)
        -> vector<hot_zone_entry>
    {
        auto map_entries = vector<hot_zone_entry>{};

        for (const auto& track : tracks)
        {
            const auto z_span = span<const telemetry_zone>{track.zones.data(), track.zones.size()};
            const auto exclusives = compute_exclusive_durations_telemetry(z_span);

            for (const auto& ez : exclusives)
            {
                if (is_gpu && is_gpu_submit_zone_name(ez.name))
                {
                    continue;
                }

                const auto ex_ms = static_cast<double>(ez.exclusive_duration_ns) / 1000000.0;
                const auto tot_ms = static_cast<double>(ez.total_duration_ns) / 1000000.0;

                auto* found = static_cast<hot_zone_entry*>(nullptr);
                for (auto& entry : map_entries)
                {
                    if (entry.name == ez.name)
                    {
                        found = &entry;
                        break;
                    }
                }

                if (found)
                {
                    found->exclusive_duration_ms += ex_ms;
                    found->total_duration_ms += tot_ms;
                    found->count += 1;
                }
                else
                {
                    map_entries.push_back(hot_zone_entry{
                        .name = string{ez.name.data(), ez.name.size()},
                        .exclusive_duration_ms = ex_ms,
                        .total_duration_ms = tot_ms,
                        .count = 1,
                    });
                }
            }
        }

        std::sort(map_entries.begin(), map_entries.end(), [](const hot_zone_entry& a, const hot_zone_entry& b) {
            return a.exclusive_duration_ms > b.exclusive_duration_ms;
        });

        if (map_entries.size() > max_results)
        {
            map_entries.resize(max_results);
        }

        return map_entries;
    }

    frame_stats_accumulator::frame_stats_accumulator(size_t window_capacity)
        : _capacity(window_capacity > 0 ? window_capacity : 60),
          _history(vector<frame_sample>(_capacity, frame_sample{}))
    {
    }

    auto frame_stats_accumulator::record_frame(float fps, float frame_time_ms, float cpu_time_ms, float gpu_time_ms,
                                               const telemetry_frame& frame) -> void
    {
        auto cpu_hot = extract_hot_zones(span<const telemetry_track>{frame.cpu_tracks.data(), frame.cpu_tracks.size()},
                                         false, 100);
        auto gpu_hot =
            extract_hot_zones(span<const telemetry_track>{frame.gpu_tracks.data(), frame.gpu_tracks.size()}, true, 100);

        _history[_head] = frame_sample{
            .fps = fps,
            .frame_time_ms = frame_time_ms,
            .cpu_time_ms = cpu_time_ms,
            .gpu_time_ms = gpu_time_ms,
            .cpu_zones = tempest::move(cpu_hot),
            .gpu_zones = tempest::move(gpu_hot),
        };

        _head = (_head + 1) % _capacity;
        if (_count < _capacity)
        {
            ++_count;
        }
    }

    auto frame_stats_accumulator::get_rolling_fps() const noexcept -> float
    {
        if (_count == 0)
        {
            return 0.0f;
        }
        auto sum = 0.0;
        for (auto i = size_t{0}; i < _count; ++i)
        {
            sum += _history[i].fps;
        }
        return static_cast<float>(sum / static_cast<double>(_count));
    }

    auto frame_stats_accumulator::get_rolling_frame_time_ms() const noexcept -> float
    {
        if (_count == 0)
        {
            return 0.0f;
        }
        auto sum = 0.0;
        for (auto i = size_t{0}; i < _count; ++i)
        {
            sum += _history[i].frame_time_ms;
        }
        return static_cast<float>(sum / static_cast<double>(_count));
    }

    auto frame_stats_accumulator::get_rolling_cpu_time_ms() const noexcept -> float
    {
        if (_count == 0)
        {
            return 0.0f;
        }
        auto sum = 0.0;
        for (auto i = size_t{0}; i < _count; ++i)
        {
            sum += _history[i].cpu_time_ms;
        }
        return static_cast<float>(sum / static_cast<double>(_count));
    }

    auto frame_stats_accumulator::get_rolling_gpu_time_ms() const noexcept -> float
    {
        if (_count == 0)
        {
            return 0.0f;
        }
        auto sum = 0.0;
        for (auto i = size_t{0}; i < _count; ++i)
        {
            sum += _history[i].gpu_time_ms;
        }
        return static_cast<float>(sum / static_cast<double>(_count));
    }

    auto frame_stats_accumulator::get_top_cpu_hot_zones(size_t count) const -> vector<hot_zone_entry>
    {
        if (_count == 0 || count == 0)
        {
            return {};
        }

        auto aggregated = vector<hot_zone_entry>{};
        for (auto i = size_t{0}; i < _count; ++i)
        {
            for (const auto& z : _history[i].cpu_zones)
            {
                auto* found = static_cast<hot_zone_entry*>(nullptr);
                for (auto& entry : aggregated)
                {
                    if (entry.name == z.name)
                    {
                        found = &entry;
                        break;
                    }
                }

                if (found)
                {
                    found->exclusive_duration_ms += z.exclusive_duration_ms;
                    found->total_duration_ms += z.total_duration_ms;
                    found->count += 1;
                }
                else
                {
                    aggregated.push_back(hot_zone_entry{
                        .name = z.name,
                        .exclusive_duration_ms = z.exclusive_duration_ms,
                        .total_duration_ms = z.total_duration_ms,
                        .count = 1,
                    });
                }
            }
        }

        const auto divisor = static_cast<double>(_count);
        for (auto& entry : aggregated)
        {
            entry.exclusive_duration_ms /= divisor;
            entry.total_duration_ms /= divisor;
        }

        std::sort(aggregated.begin(), aggregated.end(), [](const hot_zone_entry& a, const hot_zone_entry& b) {
            return a.exclusive_duration_ms > b.exclusive_duration_ms;
        });

        if (aggregated.size() > count)
        {
            aggregated.resize(count);
        }

        return aggregated;
    }

    auto frame_stats_accumulator::get_top_gpu_hot_zones(size_t count) const -> vector<hot_zone_entry>
    {
        if (_count == 0 || count == 0)
        {
            return {};
        }

        auto aggregated = vector<hot_zone_entry>{};
        for (auto i = size_t{0}; i < _count; ++i)
        {
            for (const auto& z : _history[i].gpu_zones)
            {
                auto* found = static_cast<hot_zone_entry*>(nullptr);
                for (auto& entry : aggregated)
                {
                    if (entry.name == z.name)
                    {
                        found = &entry;
                        break;
                    }
                }

                if (found)
                {
                    found->exclusive_duration_ms += z.exclusive_duration_ms;
                    found->total_duration_ms += z.total_duration_ms;
                    found->count += 1;
                }
                else
                {
                    aggregated.push_back(hot_zone_entry{
                        .name = z.name,
                        .exclusive_duration_ms = z.exclusive_duration_ms,
                        .total_duration_ms = z.total_duration_ms,
                        .count = 1,
                    });
                }
            }
        }

        const auto divisor = static_cast<double>(_count);
        for (auto& entry : aggregated)
        {
            entry.exclusive_duration_ms /= divisor;
            entry.total_duration_ms /= divisor;
        }

        std::sort(aggregated.begin(), aggregated.end(), [](const hot_zone_entry& a, const hot_zone_entry& b) {
            return a.exclusive_duration_ms > b.exclusive_duration_ms;
        });

        if (aggregated.size() > count)
        {
            aggregated.resize(count);
        }

        return aggregated;
    }

    auto frame_stats_accumulator::window_capacity() const noexcept -> size_t
    {
        return _capacity;
    }

    auto frame_stats_accumulator::sample_count() const noexcept -> size_t
    {
        return _count;
    }

    auto frame_stats_accumulator::reset() noexcept -> void
    {
        _head = 0;
        _count = 0;
        for (auto& s : _history)
        {
            s = frame_sample{};
        }
    }
} // namespace tempest::profiler
