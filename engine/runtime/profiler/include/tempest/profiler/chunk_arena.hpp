#ifndef tempest_profiler_chunk_arena_hpp
#define tempest_profiler_chunk_arena_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/mutex.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest::profiler
{
    class TEMPEST_API event_chunk
    {
      public:
        static constexpr size_t chunk_capacity_bytes = 64 * 1024; // 64 KB

        event_chunk();
        ~event_chunk() = default;

        event_chunk(const event_chunk&) = delete;
        event_chunk& operator=(const event_chunk&) = delete;
        event_chunk(event_chunk&&) noexcept = default;
        event_chunk& operator=(event_chunk&&) noexcept = default;

        auto add_zone(const zone_record& zone) -> bool;
        auto add_marker(const marker_record& marker) -> bool;
        auto add_metric(const metric_record& metric) -> bool;

        [[nodiscard]] auto zones() const noexcept -> span<const zone_record>;
        [[nodiscard]] auto markers() const noexcept -> span<const marker_record>;
        [[nodiscard]] auto metrics() const noexcept -> span<const metric_record>;

        [[nodiscard]] auto zones() noexcept -> span<zone_record>;
        [[nodiscard]] auto markers() noexcept -> span<marker_record>;
        [[nodiscard]] auto metrics() noexcept -> span<metric_record>;

        [[nodiscard]] auto empty() const noexcept -> bool;
        [[nodiscard]] auto used_bytes() const noexcept -> size_t;
        [[nodiscard]] auto capacity_bytes() const noexcept -> size_t;
        [[nodiscard]] auto get_thread_id() const noexcept -> uint64_t;
        auto set_thread_id(uint64_t tid) noexcept -> void;

        auto reset() noexcept -> void;

      private:
        vector<zone_record> _zones;
        vector<marker_record> _markers;
        vector<metric_record> _metrics;
        uint64_t _thread_id{0};
    };

    class TEMPEST_API chunk_pool
    {
      public:
        chunk_pool() = default;
        ~chunk_pool() = default;

        chunk_pool(const chunk_pool&) = delete;
        chunk_pool& operator=(const chunk_pool&) = delete;
        chunk_pool(chunk_pool&&) noexcept = delete;
        chunk_pool& operator=(chunk_pool&&) noexcept = delete;

        auto acquire() -> unique_ptr<event_chunk>;
        auto release(unique_ptr<event_chunk> chunk) -> void;
        auto clear() -> void;
        [[nodiscard]] auto pool_size() const noexcept -> size_t;

      private:
        mutable mutex _mutex;
        vector<unique_ptr<event_chunk>> _available_chunks;
    };
} // namespace tempest::profiler

#endif // tempest_profiler_chunk_arena_hpp
