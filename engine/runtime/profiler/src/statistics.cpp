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
} // namespace tempest::profiler
