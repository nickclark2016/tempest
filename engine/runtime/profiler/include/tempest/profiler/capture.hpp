#ifndef tempest_profiler_capture_hpp
#define tempest_profiler_capture_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/profiler/chunk_arena.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::profiler
{
    class profiler_session;

    enum class track_type : uint8_t
    {
        cpu_thread,
        gpu_queue,
        counter_metric,
    };

    struct track_data
    {
        uint64_t track_id{0};
        string name{};
        track_type type{track_type::cpu_thread};
        vector<zone_record> zones{};
        vector<marker_record> markers{};
    };

    struct metric_stream
    {
        string name{};
        metric_unit unit{metric_unit::raw};
        vector<metric_record> samples{};
    };

    struct capture_session_data
    {
        uint64_t start_time_ns{0};
        uint64_t end_time_ns{0};
        vector<track_data> tracks{};
        vector<metric_stream> metrics{};
        vector<string> string_table{};
    };

    TEMPEST_API auto create_capture_from_session(profiler_session& session) -> capture_session_data;
    TEMPEST_API auto create_capture_from_chunks(span<const unique_ptr<event_chunk>> chunks,
                                                const profiler_session* session = nullptr) -> capture_session_data;
} // namespace tempest::profiler

#endif // tempest_profiler_capture_hpp
