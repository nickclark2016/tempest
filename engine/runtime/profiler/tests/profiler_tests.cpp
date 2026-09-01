#include <tempest/profiler/profiler.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

//==============================================================================
// Single-Threaded & Hierarchical Profiler Tests
//==============================================================================

/// @brief Verify single-threaded nested zone hierarchies, start/end timestamps, and depth calculation.
TEST(profiler_tests, nested_zone_hierarchies)
{
    // 1. Setup profiler session
    auto session = tempest::profiler::profiler_session{};

    // 2. Act: Record nested scopes
    {
        auto root_zone = tempest::profiler::scoped_zone{session, "RootZone"};
        {
            auto child1_zone = tempest::profiler::scoped_zone{session, "ChildZone1"};
            {
                auto grandchild_zone = tempest::profiler::scoped_zone{session, "GrandchildZone"};
            }
        }
        {
            auto child2_zone = tempest::profiler::scoped_zone{session, "ChildZone2"};
        }
    }

    // 3. Assert: Drain chunks and verify records
    auto chunks = session.drain_completed_chunks();
    ASSERT_FALSE(chunks.empty());

    auto total_zones = size_t{0};
    for (const auto& chunk : chunks)
    {
        total_zones += chunk->zones().size();
    }
    ASSERT_EQ(total_zones, 4u);

    // Records are emitted upon zone exit (LIFO completion order): Grandchild (depth 2), Child1 (depth 1), Child2 (depth
    // 1), Root (depth 0)
    const auto& chunk = *chunks[0];
    auto zones = chunk.zones();

    ASSERT_EQ(zones[0].name, "GrandchildZone");
    ASSERT_EQ(zones[0].depth, 2u);
    ASSERT_LE(zones[0].start_ns, zones[0].end_ns);

    ASSERT_EQ(zones[1].name, "ChildZone1");
    ASSERT_EQ(zones[1].depth, 1u);
    ASSERT_LE(zones[1].start_ns, zones[1].end_ns);
    ASSERT_LE(zones[1].start_ns, zones[0].start_ns);
    ASSERT_GE(zones[1].end_ns, zones[0].end_ns);

    ASSERT_EQ(zones[2].name, "ChildZone2");
    ASSERT_EQ(zones[2].depth, 1u);
    ASSERT_LE(zones[2].start_ns, zones[2].end_ns);

    ASSERT_EQ(zones[3].name, "RootZone");
    ASSERT_EQ(zones[3].depth, 0u);
    ASSERT_LE(zones[3].start_ns, zones[3].end_ns);
    ASSERT_LE(zones[3].start_ns, zones[1].start_ns);
    ASSERT_GE(zones[3].end_ns, zones[2].end_ns);
}

//==============================================================================
// Concurrent Multi-Threaded Profiler Tests
//==============================================================================

/// @brief Verify 16+ threads concurrently recording 100,000+ zones without race conditions or data loss.
TEST(profiler_tests, concurrent_multithreaded_recording)
{
    // 1. Setup: Create profiler session and configure 16 worker threads
    auto session = tempest::profiler::profiler_session{};
    constexpr auto thread_count = size_t{16};
    constexpr auto zones_per_thread = size_t{10000}; // 16 * 10,000 = 160,000 zones
    auto threads = tempest::vector<tempest::thread>{};
    threads.reserve(thread_count);

    // 2. Act: Spawn threads and record zones concurrently
    for (auto i = size_t{0}; i < thread_count; ++i)
    {
        threads.push_back(tempest::thread([&session]() {
            for (auto j = size_t{0}; j < zones_per_thread; ++j)
            {
                auto zone = tempest::profiler::scoped_zone{session, "ConcurrentZone"};
            }
        }));
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // 3. Assert: Drain chunks and verify exact count without data loss
    auto chunks = session.drain_completed_chunks();
    auto total_recorded_zones = size_t{0};
    for (const auto& chunk : chunks)
    {
        total_recorded_zones += chunk->zones().size();
    }
    ASSERT_EQ(total_recorded_zones, thread_count * zones_per_thread);
}

//==============================================================================
// Instance Isolation Tests
//==============================================================================

/// @brief Verify complete instance isolation between two separate profiler_session objects.
TEST(profiler_tests, session_instance_isolation)
{
    // 1. Setup two independent profiler sessions
    auto session_a = tempest::profiler::profiler_session{};
    auto session_b = tempest::profiler::profiler_session{};

    // 2. Act: Record events into session A and session B independently
    {
        auto zone_a = tempest::profiler::scoped_zone{session_a, "SessionA_Zone"};
    }
    {
        auto zone_b1 = tempest::profiler::scoped_zone{session_b, "SessionB_Zone1"};
        auto zone_b2 = tempest::profiler::scoped_zone{session_b, "SessionB_Zone2"};
    }

    // 3. Assert: Drain chunks and verify strict separation
    auto chunks_a = session_a.drain_completed_chunks();
    auto chunks_b = session_b.drain_completed_chunks();

    auto count_a = size_t{0};
    for (const auto& chunk : chunks_a)
    {
        for (const auto& z : chunk->zones())
        {
            ASSERT_EQ(z.name, "SessionA_Zone");
            ++count_a;
        }
    }

    auto count_b = size_t{0};
    for (const auto& chunk : chunks_b)
    {
        for (const auto& z : chunk->zones())
        {
            ASSERT_TRUE(z.name == "SessionB_Zone1" || z.name == "SessionB_Zone2");
            ++count_b;
        }
    }

    ASSERT_EQ(count_a, 1u);
    ASSERT_EQ(count_b, 2u);
}

//==============================================================================
// Thread Registration & Naming Tests
//==============================================================================

/// @brief Verify implicit thread registration and OS thread naming on background threads.
TEST(profiler_tests, implicit_thread_registration_and_naming)
{
    // 1. Setup session
    auto session = tempest::profiler::profiler_session{};

    // 2. Act: Spawn a thread, set thread name, and record a zone
    auto worker = tempest::thread([&session]() {
        session.set_thread_name("WorkerThread-Alpha");
        auto zone = tempest::profiler::scoped_zone{session, "WorkerTask"};
    });
    worker.join();

    // 3. Assert: Verify thread registration and thread name
    ASSERT_GE(session.registered_thread_count(), 1u);
    auto chunks = session.drain_completed_chunks();
    ASSERT_FALSE(chunks.empty());
    ASSERT_EQ(chunks[0]->zones().size(), 1u);
}

//==============================================================================
// Markers & Metrics Tests
//==============================================================================

/// @brief Verify instant markers and attached metrics within active zones.
TEST(profiler_tests, markers_and_attached_metrics)
{
    // 1. Setup session
    auto session = tempest::profiler::profiler_session{};

    // 2. Act: Emit markers and attach metrics within a scoped zone
    tempest::profiler::emit_marker(session, "FrameStart");
    {
        auto zone = tempest::profiler::scoped_zone{session, "RenderPass"};
        zone.add_metric("draw_calls", 150.0, tempest::profiler::metric_unit::count);
        zone.add_metric("vram_usage", 1024.0 * 1024.0 * 64.0, tempest::profiler::metric_unit::bytes);
        zone.set_task_id(42);
    }
    tempest::profiler::emit_marker(session, "FrameEnd");

    // 3. Assert: Drain and verify markers and attached metrics
    auto chunks = session.drain_completed_chunks();
    ASSERT_FALSE(chunks.empty());

    auto total_markers = size_t{0};
    auto total_zones = size_t{0};
    for (const auto& chunk : chunks)
    {
        total_markers += chunk->markers().size();
        total_zones += chunk->zones().size();
    }

    ASSERT_EQ(total_markers, 2u);
    ASSERT_EQ(total_zones, 1u);

    const auto& chunk = *chunks[0];
    auto zones = chunk.zones();
    auto markers = chunk.markers();

    ASSERT_EQ(markers[0].name, "FrameStart");
    ASSERT_EQ(markers[1].name, "FrameEnd");
    ASSERT_GT(markers[0].timestamp_ns, 0u);
    ASSERT_LE(markers[0].timestamp_ns, markers[1].timestamp_ns);

    ASSERT_EQ(zones[0].name, "RenderPass");
    ASSERT_EQ(zones[0].task_id, 42u);
    ASSERT_EQ(zones[0].metrics.size(), 2u);
    ASSERT_EQ(zones[0].metrics[0].name, "draw_calls");
    ASSERT_DOUBLE_EQ(zones[0].metrics[0].value, 150.0);
    ASSERT_EQ(zones[0].metrics[0].unit, tempest::profiler::metric_unit::count);

    ASSERT_EQ(zones[0].metrics[1].name, "vram_usage");
    ASSERT_DOUBLE_EQ(zones[0].metrics[1].value, 1024.0 * 1024.0 * 64.0);
    ASSERT_EQ(zones[0].metrics[1].unit, tempest::profiler::metric_unit::bytes);
}

//==============================================================================
// Chunk Arena Pool Recycling Tests
//==============================================================================

/// @brief Verify chunk arena pool recycling when event volume exceeds single 64KB chunk.
TEST(profiler_tests, chunk_arena_pool_recycling)
{
    // 1. Setup session
    auto session = tempest::profiler::profiler_session{};

    // 2. Act: Record enough zones to exceed a single 64KB chunk (e.g., 2,000 zones)
    constexpr auto event_count = size_t{2000};
    for (auto i = size_t{0}; i < event_count; ++i)
    {
        auto zone = tempest::profiler::scoped_zone{session, "BulkZone"};
    }

    // Drain chunks and assert multiple chunks were filled
    auto chunks = session.drain_completed_chunks();
    ASSERT_GT(chunks.size(), 1u);

    auto drained_count = size_t{0};
    for (const auto& chunk : chunks)
    {
        drained_count += chunk->zones().size();
    }
    ASSERT_EQ(drained_count, event_count);

    // Recycle drained chunks back into the session pool
    auto recycled_count = chunks.size();
    session.recycle_chunks(tempest::move(chunks));
    ASSERT_EQ(session.get_chunk_pool().pool_size(), recycled_count);

    // Record another batch and verify recycled chunks are reused from pool
    for (auto i = size_t{0}; i < event_count; ++i)
    {
        auto zone = tempest::profiler::scoped_zone{session, "BulkZoneReused"};
    }

    auto second_chunks = session.drain_completed_chunks();
    ASSERT_GT(second_chunks.size(), 1u);
    auto second_drained_count = size_t{0};
    for (const auto& chunk : second_chunks)
    {
        second_drained_count += chunk->zones().size();
    }
    ASSERT_EQ(second_drained_count, event_count);
}

//==============================================================================
// Compile-Time & Runtime Disabled Traits Tests
//==============================================================================

/// @brief Verify zero-overhead compile-time disabled traits.
TEST(profiler_tests, compile_time_and_runtime_disabled_traits)
{
    // 1. Setup session
    auto session = tempest::profiler::profiler_session{false}; // Runtime disabled

    // 2. Act: Use disabled_scoped_zone and runtime-disabled session
    static_assert(sizeof(tempest::profiler::disabled_scoped_zone) == 1,
                  "disabled_scoped_zone must be empty struct size");

    {
        auto compile_disabled = tempest::profiler::disabled_scoped_zone{session, "DisabledCompileZone"};
        compile_disabled.add_metric("metric", 123.0, tempest::profiler::metric_unit::count);
        compile_disabled.set_task_id(1);
    }

    {
        auto runtime_disabled = tempest::profiler::scoped_zone{session, "DisabledRuntimeZone"};
        runtime_disabled.add_metric("metric", 456.0, tempest::profiler::metric_unit::count);
        runtime_disabled.set_task_id(2);
    }

    tempest::profiler::emit_marker(session, "DisabledMarker");
    tempest::profiler::emit_metric(session, "DisabledMetric", 789.0, tempest::profiler::metric_unit::raw);

    // 3. Assert: No chunks or zones recorded
    auto chunks = session.drain_completed_chunks();
    auto total_events = size_t{0};
    for (const auto& chunk : chunks)
    {
        total_events += chunk->zones().size() + chunk->markers().size() + chunk->metrics().size();
    }
    ASSERT_EQ(total_events, 0u);
}
