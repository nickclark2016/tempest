#ifndef tempest_profiler_statistics_hpp
#define tempest_profiler_statistics_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/profiler/capture.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/profiler/websocket.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
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

    struct zone_exclusive_time
    {
        string_view name{};
        uint64_t total_duration_ns{0};
        uint64_t exclusive_duration_ns{0};
        uint32_t depth{0};
    };

    struct hot_zone_entry
    {
        string name{};
        double exclusive_duration_ms{0.0};
        double total_duration_ms{0.0};
        uint32_t count{1};
    };

    TEMPEST_API auto compute_zone_statistics(span<const zone_record> zones) -> zone_statistics;
    TEMPEST_API auto compute_all_zone_statistics(const capture_session_data& capture) -> vector<zone_statistics>;
    TEMPEST_API auto compute_rolling_averages(span<const double> values, size_t window_size) -> vector<double>;

    TEMPEST_API auto is_gpu_submit_zone_name(string_view name) noexcept -> bool;
    TEMPEST_API auto compute_exclusive_durations(span<const zone_record> zones) -> vector<zone_exclusive_time>;
    TEMPEST_API auto compute_exclusive_durations_telemetry(span<const telemetry_zone> zones)
        -> vector<zone_exclusive_time>;
    TEMPEST_API auto extract_hot_zones(span<const telemetry_track> tracks, bool is_gpu, size_t max_results = 5)
        -> vector<hot_zone_entry>;

    class TEMPEST_API frame_stats_accumulator
    {
      public:
        explicit frame_stats_accumulator(size_t window_capacity = 60);

        auto record_frame(float fps, float frame_time_ms, float cpu_time_ms, float gpu_time_ms,
                          const telemetry_frame& frame) -> void;

        [[nodiscard]] auto get_rolling_fps() const noexcept -> float;
        [[nodiscard]] auto get_rolling_frame_time_ms() const noexcept -> float;
        [[nodiscard]] auto get_rolling_cpu_time_ms() const noexcept -> float;
        [[nodiscard]] auto get_rolling_gpu_time_ms() const noexcept -> float;

        [[nodiscard]] auto get_top_cpu_hot_zones(size_t count = 5) const -> vector<hot_zone_entry>;
        [[nodiscard]] auto get_top_gpu_hot_zones(size_t count = 5) const -> vector<hot_zone_entry>;

        [[nodiscard]] auto window_capacity() const noexcept -> size_t;
        [[nodiscard]] auto sample_count() const noexcept -> size_t;

        auto reset() noexcept -> void;

      private:
        struct frame_sample
        {
            float fps{0.0f};
            float frame_time_ms{0.0f};
            float cpu_time_ms{0.0f};
            float gpu_time_ms{0.0f};
            vector<hot_zone_entry> cpu_zones{};
            vector<hot_zone_entry> gpu_zones{};
        };

        size_t _capacity{60};
        size_t _head{0};
        size_t _count{0};
        vector<frame_sample> _history{};
    };
} // namespace tempest::profiler

#endif // tempest_profiler_statistics_hpp
