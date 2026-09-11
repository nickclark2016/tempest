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

    enum class suspend_reason : uint8_t
    {
        none = 0,
        initial,
        completed,
        yield,
        mutex_contention,
        event_wait,
        channel_full,
        channel_empty,
        task_graph_dependency,
        timeline_wait,
        cancellation,
    };

    [[nodiscard]] constexpr auto to_string(suspend_reason reason) noexcept -> string_view
    {
        switch (reason)
        {
            case suspend_reason::none:
                return "none";
            case suspend_reason::initial:
                return "initial";
            case suspend_reason::completed:
                return "completed";
            case suspend_reason::yield:
                return "yield";
            case suspend_reason::mutex_contention:
                return "mutex_contention";
            case suspend_reason::event_wait:
                return "event_wait";
            case suspend_reason::channel_full:
                return "channel_full";
            case suspend_reason::channel_empty:
                return "channel_empty";
            case suspend_reason::task_graph_dependency:
                return "task_graph_dependency";
            case suspend_reason::timeline_wait:
                return "timeline_wait";
            case suspend_reason::cancellation:
                return "cancellation";
            default:
                return "unknown";
        }
    }

    struct zone_record
    {
        uint64_t start_ns{0};
        uint64_t end_ns{0};
        uint32_t depth{0};
        string_view name{};
        source_location location{};
        uint64_t task_id{0};
        uint64_t coroutine_id{0};
        uint32_t slice_index{0};
        suspend_reason reason{suspend_reason::none};
        inplace_vector<metric_record, 16> metrics{};
    };
} // namespace tempest::profiler

#endif // tempest_profiler_types_hpp
