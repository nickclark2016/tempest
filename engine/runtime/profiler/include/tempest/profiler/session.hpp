#ifndef tempest_profiler_session_hpp
#define tempest_profiler_session_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/mutex.hpp>
#include <tempest/profiler/chunk_arena.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/source_location.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::profiler
{
    class profiler_session;

    struct open_zone_state
    {
        uint64_t start_ns{0};
        uint32_t depth{0};
        string_view name{};
        source_location location{};
        uint64_t task_id{0};
        inplace_vector<metric_record, 4> metrics{};
    };

    class TEMPEST_API thread_profiler_context
    {
      public:
        thread_profiler_context(profiler_session& session, uint64_t thread_id, string_view thread_name);
        ~thread_profiler_context();

        thread_profiler_context(const thread_profiler_context&) = delete;
        thread_profiler_context& operator=(const thread_profiler_context&) = delete;
        thread_profiler_context(thread_profiler_context&&) noexcept = delete;
        thread_profiler_context& operator=(thread_profiler_context&&) noexcept = delete;

        auto begin_zone(string_view name, source_location loc = source_location::current()) -> void;
        auto end_zone() -> void;
        auto add_marker(string_view name, source_location loc = source_location::current()) -> void;
        auto add_metric(string_view name, double val, metric_unit unit = metric_unit::raw) -> void;
        auto set_current_zone_task_id(uint64_t task_id) -> void;

        auto set_thread_name(string_view name) -> void;
        [[nodiscard]] auto get_thread_name() const noexcept -> string_view;
        [[nodiscard]] auto get_thread_id() const noexcept -> uint64_t;
        [[nodiscard]] auto get_session() const noexcept -> const profiler_session&;
        [[nodiscard]] auto get_session() noexcept -> profiler_session&;
        [[nodiscard]] auto current_depth() const noexcept -> uint32_t;

        auto flush_active_chunk() -> void;

      private:
        auto _ensure_active_chunk_locked() -> void;
        auto _flush_chunk_locked() -> void;

        profiler_session& _session;
        uint64_t _thread_id{0};
        string _thread_name{};
        vector<open_zone_state> _open_zones;
        unique_ptr<event_chunk> _current_chunk;
        mutable mutex _mutex;
    };

    struct thread_slot
    {
        atomic<uint64_t> thread_id{0};
        unique_ptr<thread_profiler_context> context{};
    };

    class TEMPEST_API profiler_session
    {
      public:
        static constexpr size_t max_thread_slots = 64;

        profiler_session() noexcept;
        explicit profiler_session(bool enabled) noexcept;
        ~profiler_session();

        profiler_session(const profiler_session&) = delete;
        profiler_session& operator=(const profiler_session&) = delete;
        profiler_session(profiler_session&&) noexcept = delete;
        profiler_session& operator=(profiler_session&&) noexcept = delete;

        [[nodiscard]] auto get_or_register_thread() -> thread_profiler_context&;
        auto register_track(uint64_t track_id, string_view track_name) -> thread_profiler_context&;
        auto set_thread_name(string_view name) -> void;

        [[nodiscard]] auto is_enabled() const noexcept -> bool;
        auto set_enabled(bool enabled) noexcept -> void;

        auto drain_completed_chunks() -> vector<unique_ptr<event_chunk>>;
        auto recycle_chunks(vector<unique_ptr<event_chunk>> chunks) -> void;

        auto acquire_chunk() -> unique_ptr<event_chunk>;
        auto release_chunk(unique_ptr<event_chunk> chunk) -> void;
        auto push_completed_chunk(unique_ptr<event_chunk> chunk) -> void;

        [[nodiscard]] auto get_chunk_pool() noexcept -> chunk_pool&;
        [[nodiscard]] auto get_chunk_pool() const noexcept -> const chunk_pool&;

        [[nodiscard]] auto registered_thread_count() const noexcept -> size_t;

      private:
        auto _register_thread_slow(uint64_t tid) -> thread_profiler_context&;
        static auto _query_native_thread_name(uint64_t tid) -> string;

        atomic<bool> _enabled{true};
        array<thread_slot, max_thread_slots> _slots{};
        vector<unique_ptr<thread_profiler_context>> _overflow_contexts;
        mutable mutex _registration_mutex;

        chunk_pool _pool;
        vector<unique_ptr<event_chunk>> _completed_chunks;
        mutable mutex _completed_mutex;
    };
} // namespace tempest::profiler

#endif // tempest_profiler_session_hpp
