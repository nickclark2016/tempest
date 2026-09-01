#ifndef tempest_profiler_statistics_hpp
#define tempest_profiler_statistics_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/profiler/capture.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::profiler
{
    struct zone_statistics
    {
        string_view zone_name{};
        uint64_t count{0};
        double mean_ns{0.0};
        double min_ns{0.0};
        double max_ns{0.0};
        double p50_ns{0.0};
        double p90_ns{0.0};
        double p95_ns{0.0};
        double p99_ns{0.0};
        double std_deviation_ns{0.0};
    };

    TEMPEST_API auto compute_zone_statistics(span<const zone_record> zones) -> zone_statistics;
    TEMPEST_API auto compute_all_zone_statistics(const capture_session_data& capture) -> vector<zone_statistics>;
    TEMPEST_API auto compute_rolling_averages(span<const double> values, size_t window_size) -> vector<double>;
} // namespace tempest::profiler

#endif // tempest_profiler_statistics_hpp
