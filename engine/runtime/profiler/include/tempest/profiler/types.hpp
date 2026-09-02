#ifndef tempest_profiler_types_hpp
#define tempest_profiler_types_hpp

#include <tempest/api.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/int.hpp>
#include <tempest/source_location.hpp>
#include <tempest/string_view.hpp>

namespace tempest::profiler
{
    enum class metric_unit : uint8_t
    {
        count,
        bytes,
        duration_ns,
        percentage,
        raw,
    };

    struct metric_record
    {
        uint64_t timestamp_ns{0};
        string_view name{};
        double value{0.0};
        metric_unit unit{metric_unit::raw};
    };

    struct marker_record
    {
        uint64_t timestamp_ns{0};
        string_view name{};
        source_location location{};
    };

    struct zone_record
    {
        uint64_t start_ns{0};
        uint64_t end_ns{0};
        uint32_t depth{0};
        string_view name{};
        source_location location{};
        uint64_t task_id{0};
        inplace_vector<metric_record, 16> metrics{};
    };
} // namespace tempest::profiler

#endif // tempest_profiler_types_hpp
