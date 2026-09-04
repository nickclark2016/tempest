#include <tempest/profiler/session.hpp>
#include <tempest/thread.hpp>

#include <cstdio>
#include <tempest/int.hpp>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#include <time.h>
#else
#include <chrono>
#endif

namespace
{
    auto get_timestamp_ns() noexcept -> tempest::uint64_t
    {
#if defined(_WIN32)
        static const auto frequency = []() {
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            return static_cast<tempest::uint64_t>(freq.QuadPart);
        }();
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        auto count = static_cast<tempest::uint64_t>(counter.QuadPart);
        return (count / frequency) * 1'000'000'000ULL + ((count % frequency) * 1'000'000'000ULL) / frequency;
#elif defined(__linux__)
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<tempest::uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<tempest::uint64_t>(ts.tv_nsec);
#else
        return static_cast<tempest::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count());
#endif
    }
} // namespace

namespace tempest::profiler
{
    event_chunk::event_chunk()
    {
        _zones.reserve(chunk_capacity_bytes / sizeof(zone_record));
    }

    auto event_chunk::add_zone(const zone_record& zone) -> bool
    {
        if (used_bytes() + sizeof(zone_record) > chunk_capacity_bytes)
        {
            return false;
        }
        _zones.push_back(zone);
        return true;
    }

    auto event_chunk::add_marker(const marker_record& marker) -> bool
    {
        if (used_bytes() + sizeof(marker_record) > chunk_capacity_bytes)
        {
            return false;
        }
        _markers.push_back(marker);
        return true;
    }

    auto event_chunk::add_metric(const metric_record& metric) -> bool
    {
        if (used_bytes() + sizeof(metric_record) > chunk_capacity_bytes)
        {
            return false;
        }
        _metrics.push_back(metric);
        return true;
    }

    auto event_chunk::zones() const noexcept -> span<const zone_record>
    {
        return span<const zone_record>(_zones.data(), _zones.size());
    }

    auto event_chunk::markers() const noexcept -> span<const marker_record>
    {
        return span<const marker_record>(_markers.data(), _markers.size());
    }

    auto event_chunk::metrics() const noexcept -> span<const metric_record>
    {
        return span<const metric_record>(_metrics.data(), _metrics.size());
    }

    auto event_chunk::zones() noexcept -> span<zone_record>
    {
        return span<zone_record>(_zones.data(), _zones.size());
    }

    auto event_chunk::markers() noexcept -> span<marker_record>
    {
        return span<marker_record>(_markers.data(), _markers.size());
    }

    auto event_chunk::metrics() noexcept -> span<metric_record>
    {
        return span<metric_record>(_metrics.data(), _metrics.size());
    }

    auto event_chunk::empty() const noexcept -> bool
    {
        return _zones.empty() && _markers.empty() && _metrics.empty();
    }

    auto event_chunk::used_bytes() const noexcept -> size_t
    {
        return (_zones.size() * sizeof(zone_record)) + (_markers.size() * sizeof(marker_record)) +
               (_metrics.size() * sizeof(metric_record));
    }

    auto event_chunk::capacity_bytes() const noexcept -> size_t
    {
        return chunk_capacity_bytes;
    }

    auto event_chunk::get_thread_id() const noexcept -> uint64_t
    {
        return _thread_id;
    }

    auto event_chunk::set_thread_id(uint64_t tid) noexcept -> void
    {
        _thread_id = tid;
    }

    auto event_chunk::reset() noexcept -> void
    {
        _zones.clear();
        _markers.clear();
        _metrics.clear();
        _thread_id = 0;
    }

    auto chunk_pool::acquire() -> unique_ptr<event_chunk>
    {
        lock_guard guard(_mutex);
        if (!_available_chunks.empty())
        {
            auto chunk = tempest::move(_available_chunks.back());
            _available_chunks.pop_back();
            chunk->reset();
            return chunk;
        }
        return make_unique<event_chunk>();
    }

    auto chunk_pool::release(unique_ptr<event_chunk> chunk) -> void
    {
        if (!chunk)
        {
            return;
        }
        chunk->reset();
        lock_guard guard(_mutex);
        _available_chunks.push_back(tempest::move(chunk));
    }

    auto chunk_pool::clear() -> void
    {
        lock_guard guard(_mutex);
        _available_chunks.clear();
    }

    auto chunk_pool::pool_size() const noexcept -> size_t
    {
        lock_guard guard(_mutex);
        return _available_chunks.size();
    }

    thread_profiler_context::thread_profiler_context(profiler_session& session, uint64_t thread_id,
                                                     string_view thread_name)
        : _session(session), _thread_id(thread_id), _thread_name(thread_name)
    {
    }

    thread_profiler_context::~thread_profiler_context()
    {
        flush_active_chunk();
    }

    auto thread_profiler_context::begin_zone(string_view name, source_location loc) -> void
    {
        auto now_ns = get_timestamp_ns();
        lock_guard guard(_mutex);
        auto depth = static_cast<uint32_t>(_open_zones.size());
        _open_zones.push_back(open_zone_state{
            .start_ns = now_ns,
            .depth = depth,
            .name = name,
            .location = loc,
            .task_id = 0,
            .metrics = {},
        });
    }

    auto thread_profiler_context::end_zone() -> void
    {
        auto end_ns = get_timestamp_ns();
        lock_guard guard(_mutex);
        if (_open_zones.empty())
        {
            return;
        }
        auto state = tempest::move(_open_zones.back());
        _open_zones.pop_back();

        auto record = zone_record{
            .start_ns = state.start_ns,
            .end_ns = end_ns,
            .depth = state.depth,
            .name = state.name,
            .location = state.location,
            .task_id = state.task_id,
            .metrics = tempest::move(state.metrics),
        };

        _ensure_active_chunk_locked();
        if (!_current_chunk->add_zone(record))
        {
            _flush_chunk_locked();
            _ensure_active_chunk_locked();
            _current_chunk->add_zone(record);
        }
    }

    auto thread_profiler_context::add_marker(string_view name, source_location loc) -> void
    {
        auto now_ns = get_timestamp_ns();
        lock_guard guard(_mutex);
        auto record = marker_record{
            .timestamp_ns = now_ns,
            .name = name,
            .location = loc,
        };

        _ensure_active_chunk_locked();
        if (!_current_chunk->add_marker(record))
        {
            _flush_chunk_locked();
            _ensure_active_chunk_locked();
            _current_chunk->add_marker(record);
        }
    }

    auto thread_profiler_context::add_metric(string_view name, double val, metric_unit unit) -> void
    {
        auto now_ns = get_timestamp_ns();
        lock_guard guard(_mutex);
        auto metric = metric_record{
            .timestamp_ns = now_ns,
            .name = name,
            .value = val,
            .unit = unit,
        };

        if (!_open_zones.empty())
        {
            if (!_open_zones.back().metrics.try_push_back(metric))
            {
                _ensure_active_chunk_locked();
                if (!_current_chunk->add_metric(metric))
                {
                    _flush_chunk_locked();
                    _ensure_active_chunk_locked();
                    _current_chunk->add_metric(metric);
                }
            }
        }
        else
        {
            _ensure_active_chunk_locked();
            if (!_current_chunk->add_metric(metric))
            {
                _flush_chunk_locked();
                _ensure_active_chunk_locked();
                _current_chunk->add_metric(metric);
            }
        }
    }

    auto thread_profiler_context::set_current_zone_task_id(uint64_t task_id) -> void
    {
        lock_guard guard(_mutex);
        if (!_open_zones.empty())
        {
            _open_zones.back().task_id = task_id;
        }
    }

    auto thread_profiler_context::set_thread_name(string_view name) -> void
    {
        lock_guard guard(_mutex);
        _thread_name = string(name);
    }

    auto thread_profiler_context::get_thread_name() const noexcept -> string_view
    {
        lock_guard guard(_mutex);
        return string_view(_thread_name.data(), _thread_name.size());
    }

    auto thread_profiler_context::get_thread_id() const noexcept -> uint64_t
    {
        return _thread_id;
    }

    auto thread_profiler_context::get_session() const noexcept -> const profiler_session&
    {
        return _session;
    }

    auto thread_profiler_context::get_session() noexcept -> profiler_session&
    {
        return _session;
    }

    auto thread_profiler_context::current_depth() const noexcept -> uint32_t
    {
        lock_guard guard(_mutex);
        return static_cast<uint32_t>(_open_zones.size());
    }

    auto thread_profiler_context::_ensure_active_chunk_locked() -> void
    {
        if (!_current_chunk)
        {
            _current_chunk = _session.acquire_chunk();
            _current_chunk->set_thread_id(_thread_id);
        }
    }

    auto thread_profiler_context::_flush_chunk_locked() -> void
    {
        if (_current_chunk && !_current_chunk->empty())
        {
            _current_chunk->set_thread_id(_thread_id);
            _session.push_completed_chunk(tempest::move(_current_chunk));
            _current_chunk = nullptr;
        }
    }

    auto thread_profiler_context::flush_active_chunk() -> void
    {
        lock_guard guard(_mutex);
        _flush_chunk_locked();
    }

    profiler_session::profiler_session() noexcept : _enabled(true)
    {
    }

    profiler_session::profiler_session(bool enabled) noexcept : _enabled(enabled)
    {
    }

    profiler_session::~profiler_session()
    {
        // Flush all active chunks
        lock_guard reg_guard(_registration_mutex);
        for (auto& slot : _slots)
        {
            if (slot.context)
            {
                slot.context->flush_active_chunk();
            }
        }
        for (auto& ctx : _overflow_contexts)
        {
            if (ctx)
            {
                ctx->flush_active_chunk();
            }
        }
    }

    auto profiler_session::get_or_register_thread() -> thread_profiler_context&
    {
        auto tid = tempest::this_thread::get_id().to_uint64();
        auto hash_val = hash<uint64_t>{}(tid);
        auto slot_idx = hash_val % max_thread_slots;

        auto occupant = _slots[slot_idx].thread_id.load(memory_order::relaxed);
        if (occupant == tid && _slots[slot_idx].context) [[likely]]
        {
            return *_slots[slot_idx].context;
        }

        return _register_thread_slow(tid);
    }

    auto profiler_session::register_track(uint64_t track_id, string_view track_name) -> thread_profiler_context&
    {
        lock_guard guard(_registration_mutex);

        auto hash_val = hash<uint64_t>{}(track_id);
        auto slot_idx = hash_val % max_thread_slots;

        for (size_t i = 0; i < max_thread_slots; ++i)
        {
            auto idx = (slot_idx + i) % max_thread_slots;
            if (_slots[idx].thread_id.load(memory_order::relaxed) == track_id && _slots[idx].context)
            {
                if (!track_name.empty())
                {
                    _slots[idx].context->set_thread_name(track_name);
                }
                return *_slots[idx].context;
            }
        }

        for (const auto& ctx : _overflow_contexts)
        {
            if (ctx && ctx->get_thread_id() == track_id)
            {
                if (!track_name.empty())
                {
                    ctx->set_thread_name(track_name);
                }
                return *ctx;
            }
        }

        for (size_t i = 0; i < max_thread_slots; ++i)
        {
            auto idx = (slot_idx + i) % max_thread_slots;
            if (_slots[idx].thread_id.load(memory_order::relaxed) == 0)
            {
                auto ctx = make_unique<thread_profiler_context>(*this, track_id, track_name);
                _slots[idx].context = tempest::move(ctx);
                _slots[idx].thread_id.store(track_id, memory_order::release);
                return *_slots[idx].context;
            }
        }

        auto ctx = make_unique<thread_profiler_context>(*this, track_id, track_name);
        auto* ptr = ctx.get();
        _overflow_contexts.push_back(tempest::move(ctx));
        return *ptr;
    }

    auto profiler_session::_register_thread_slow(uint64_t tid) -> thread_profiler_context&
    {
        lock_guard guard(_registration_mutex);

        auto hash_val = hash<uint64_t>{}(tid);
        auto slot_idx = hash_val % max_thread_slots;

        // 1. Check if another thread registered this tid while we were waiting on the lock
        for (size_t i = 0; i < max_thread_slots; ++i)
        {
            auto idx = (slot_idx + i) % max_thread_slots;
            if (_slots[idx].thread_id.load(memory_order::relaxed) == tid && _slots[idx].context)
            {
                return *_slots[idx].context;
            }
        }

        for (const auto& ctx : _overflow_contexts)
        {
            if (ctx && ctx->get_thread_id() == tid)
            {
                return *ctx;
            }
        }

        // 2. Find an empty slot
        auto thread_name = _query_native_thread_name(tid);

        for (size_t i = 0; i < max_thread_slots; ++i)
        {
            auto idx = (slot_idx + i) % max_thread_slots;
            if (_slots[idx].thread_id.load(memory_order::relaxed) == 0)
            {
                auto ctx = make_unique<thread_profiler_context>(*this, tid, thread_name);
                _slots[idx].context = tempest::move(ctx);
                _slots[idx].thread_id.store(tid, memory_order::release);
                return *_slots[idx].context;
            }
        }

        // 3. If all 64 slots are occupied, use overflow
        auto ctx = make_unique<thread_profiler_context>(*this, tid, thread_name);
        auto* ptr = ctx.get();
        _overflow_contexts.push_back(tempest::move(ctx));
        return *ptr;
    }

    auto profiler_session::_query_native_thread_name(uint64_t tid) -> string
    {
        auto name = string{};
#if defined(_WIN32)
        PWSTR desc = nullptr;
        auto hr = GetThreadDescription(GetCurrentThread(), &desc);
        if (SUCCEEDED(hr) && desc != nullptr && desc[0] != L'\0')
        {
            auto size_needed = WideCharToMultiByte(CP_UTF8, 0, desc, -1, nullptr, 0, nullptr, nullptr);
            if (size_needed > 1)
            {
                auto buf = vector<char>(static_cast<size_t>(size_needed));
                WideCharToMultiByte(CP_UTF8, 0, desc, -1, buf.data(), size_needed, nullptr, nullptr);
                name = string(buf.data(), static_cast<size_t>(size_needed - 1));
            }
            LocalFree(desc);
        }
#elif defined(__linux__)
        char buf[16] = {0};
        if (pthread_getname_np(pthread_self(), buf, sizeof(buf)) == 0 && buf[0] != '\0')
        {
            name = string(buf);
        }
#endif

        if (name.empty())
        {
            char id_buf[32];
            snprintf(id_buf, sizeof(id_buf), "Thread-%llu", static_cast<unsigned long long>(tid));
            name = string(id_buf);
        }

        return name;
    }

    auto profiler_session::set_thread_name(string_view name) -> void
    {
        auto& ctx = get_or_register_thread();
        ctx.set_thread_name(name);
    }

    auto profiler_session::is_enabled() const noexcept -> bool
    {
        return _enabled.load(memory_order::relaxed);
    }

    auto profiler_session::set_enabled(bool enabled) noexcept -> void
    {
        _enabled.store(enabled, memory_order::relaxed);
    }

    auto profiler_session::drain_completed_chunks() -> vector<unique_ptr<event_chunk>>
    {
        {
            lock_guard reg_guard(_registration_mutex);
            for (auto& slot : _slots)
            {
                if (slot.context)
                {
                    slot.context->flush_active_chunk();
                }
            }
            for (auto& ctx : _overflow_contexts)
            {
                if (ctx)
                {
                    ctx->flush_active_chunk();
                }
            }
        }

        lock_guard guard(_completed_mutex);
        auto result = tempest::move(_completed_chunks);
        _completed_chunks.clear();
        return result;
    }

    auto profiler_session::recycle_chunks(vector<unique_ptr<event_chunk>> chunks) -> void
    {
        for (auto& chunk : chunks)
        {
            if (chunk)
            {
                _pool.release(tempest::move(chunk));
            }
        }
    }

    auto profiler_session::acquire_chunk() -> unique_ptr<event_chunk>
    {
        return _pool.acquire();
    }

    auto profiler_session::release_chunk(unique_ptr<event_chunk> chunk) -> void
    {
        _pool.release(tempest::move(chunk));
    }

    auto profiler_session::push_completed_chunk(unique_ptr<event_chunk> chunk) -> void
    {
        if (!chunk || chunk->empty())
        {
            return;
        }
        lock_guard guard(_completed_mutex);
        _completed_chunks.push_back(tempest::move(chunk));
    }

    auto profiler_session::get_chunk_pool() noexcept -> chunk_pool&
    {
        return _pool;
    }

    auto profiler_session::get_chunk_pool() const noexcept -> const chunk_pool&
    {
        return _pool;
    }

    auto profiler_session::registered_thread_count() const noexcept -> size_t
    {
        lock_guard guard(_registration_mutex);
        auto count = size_t{0};
        for (const auto& slot : _slots)
        {
            if (slot.thread_id.load(memory_order::relaxed) != 0 && slot.context)
            {
                ++count;
            }
        }
        count += _overflow_contexts.size();
        return count;
    }

    auto profiler_session::get_track_name(uint64_t track_id) const -> string
    {
        lock_guard guard(_registration_mutex);
        for (const auto& slot : _slots)
        {
            if (slot.thread_id.load(memory_order::relaxed) == track_id && slot.context)
            {
                auto name_view = slot.context->get_thread_name();
                return string{name_view.data(), name_view.size()};
            }
        }
        for (const auto& ctx : _overflow_contexts)
        {
            if (ctx && ctx->get_thread_id() == track_id)
            {
                auto name_view = ctx->get_thread_name();
                return string{name_view.data(), name_view.size()};
            }
        }
        return {};
    }
} // namespace tempest::profiler
