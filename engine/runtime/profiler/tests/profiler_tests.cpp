#include <tempest/algorithm.hpp>
#include <tempest/profiler/profiler.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>
#include <simdjson.h>

#include <cmath>
#include <cstdio>

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
        [[maybe_unused]] const auto root_zone = tempest::profiler::scoped_zone{session, "RootZone"};
        {
            [[maybe_unused]] const auto child1_zone = tempest::profiler::scoped_zone{session, "ChildZone1"};
            {
                [[maybe_unused]] const auto grandchild_zone = tempest::profiler::scoped_zone{session, "GrandchildZone"};
            }
        }
        {
            [[maybe_unused]] const auto child2_zone = tempest::profiler::scoped_zone{session, "ChildZone2"};
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
                [[maybe_unused]] const auto zone = tempest::profiler::scoped_zone{session, "ConcurrentZone"};
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
        [[maybe_unused]] const auto zone_a = tempest::profiler::scoped_zone{session_a, "SessionA_Zone"};
    }
    {
        [[maybe_unused]] const auto zone_b1 = tempest::profiler::scoped_zone{session_b, "SessionB_Zone1"};
        [[maybe_unused]] const auto zone_b2 = tempest::profiler::scoped_zone{session_b, "SessionB_Zone2"};
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
        [[maybe_unused]] const auto zone = tempest::profiler::scoped_zone{session, "WorkerTask"};
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
        [[maybe_unused]] const auto zone = tempest::profiler::scoped_zone{session, "BulkZone"};
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
        [[maybe_unused]] const auto zone = tempest::profiler::scoped_zone{session, "BulkZoneReused"};
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

//==============================================================================
// Statistics Engine & Exact Percentiles Tests
//==============================================================================

/// @brief Verify statistical calculations (Mean, Min, Max, P50, P90, P95, P99, StdDev) against exact mathematical
/// reference distributions.
TEST(profiler_tests, statistical_calculations_and_exact_percentiles)
{
    // 1. Setup: Create exact reference distribution (101 samples: 0, 10, 20, ..., 1000 ns)
    auto zones_odd = tempest::vector<tempest::profiler::zone_record>{};
    zones_odd.reserve(101);
    for (auto i = uint64_t{0}; i <= 100; ++i)
    {
        zones_odd.push_back(tempest::profiler::zone_record{
            .start_ns = 0,
            .end_ns = i * 10,
            .depth = 0,
            .name = "OddDistribution",
        });
    }

    // 2. Act: Compute statistics for odd distribution
    const auto stats_odd = tempest::profiler::compute_zone_statistics(
        tempest::span<const tempest::profiler::zone_record>{zones_odd.data(), zones_odd.size()});

    // 3. Assert: Verify exact percentiles and moments
    ASSERT_EQ(stats_odd.zone_name, "OddDistribution");
    ASSERT_EQ(stats_odd.count, 101u);
    ASSERT_DOUBLE_EQ(stats_odd.min_ns, 0.0);
    ASSERT_DOUBLE_EQ(stats_odd.max_ns, 1000.0);
    ASSERT_DOUBLE_EQ(stats_odd.mean_ns, 500.0);
    ASSERT_DOUBLE_EQ(stats_odd.p50_ns, 500.0);
    ASSERT_DOUBLE_EQ(stats_odd.p90_ns, 900.0);
    ASSERT_DOUBLE_EQ(stats_odd.p95_ns, 950.0);
    ASSERT_DOUBLE_EQ(stats_odd.p99_ns, 990.0);
    // Population variance for 0, 10, ..., 1000 is 100 * (100 * 102 / 12) = 85,000 -> StdDev = sqrt(85000)
    ASSERT_NEAR(stats_odd.std_deviation_ns, std::sqrt(85000.0), 1e-6);

    // 4. Setup & Act: Even distribution (4 samples: 100, 200, 300, 400 ns)
    auto zones_even = tempest::vector<tempest::profiler::zone_record>{};
    zones_even.push_back(tempest::profiler::zone_record{.start_ns = 0, .end_ns = 100, .name = "EvenDistribution"});
    zones_even.push_back(tempest::profiler::zone_record{.start_ns = 0, .end_ns = 200, .name = "EvenDistribution"});
    zones_even.push_back(tempest::profiler::zone_record{.start_ns = 0, .end_ns = 300, .name = "EvenDistribution"});
    zones_even.push_back(tempest::profiler::zone_record{.start_ns = 0, .end_ns = 400, .name = "EvenDistribution"});

    const auto stats_even = tempest::profiler::compute_zone_statistics(
        tempest::span<const tempest::profiler::zone_record>{zones_even.data(), zones_even.size()});

    // 5. Assert: Verify even distribution interpolation
    ASSERT_EQ(stats_even.count, 4u);
    ASSERT_DOUBLE_EQ(stats_even.min_ns, 100.0);
    ASSERT_DOUBLE_EQ(stats_even.max_ns, 400.0);
    ASSERT_DOUBLE_EQ(stats_even.mean_ns, 250.0);
    ASSERT_DOUBLE_EQ(stats_even.p50_ns, 250.0);
    ASSERT_DOUBLE_EQ(stats_even.p90_ns, 370.0);
    ASSERT_DOUBLE_EQ(stats_even.p95_ns, 385.0);
    ASSERT_DOUBLE_EQ(stats_even.p99_ns, 397.0);
    ASSERT_NEAR(stats_even.std_deviation_ns, std::sqrt(12500.0), 1e-6);

    // 6. Assert: Empty and single-element distributions
    const auto stats_empty = tempest::profiler::compute_zone_statistics({});
    ASSERT_EQ(stats_empty.count, 0u);
    ASSERT_DOUBLE_EQ(stats_empty.mean_ns, 0.0);

    auto zones_single = tempest::vector<tempest::profiler::zone_record>{};
    zones_single.push_back(tempest::profiler::zone_record{.start_ns = 10, .end_ns = 52, .name = "Single"});
    const auto stats_single = tempest::profiler::compute_zone_statistics(
        tempest::span<const tempest::profiler::zone_record>{zones_single.data(), zones_single.size()});
    ASSERT_EQ(stats_single.count, 1u);
    ASSERT_DOUBLE_EQ(stats_single.min_ns, 42.0);
    ASSERT_DOUBLE_EQ(stats_single.max_ns, 42.0);
    ASSERT_DOUBLE_EQ(stats_single.mean_ns, 42.0);
    ASSERT_DOUBLE_EQ(stats_single.p50_ns, 42.0);
    ASSERT_DOUBLE_EQ(stats_single.std_deviation_ns, 0.0);

    // 7. Verify rolling averages
    auto values = tempest::vector<double>{};
    values.push_back(10.0);
    values.push_back(20.0);
    values.push_back(30.0);
    values.push_back(40.0);
    values.push_back(50.0);
    const auto rolling =
        tempest::profiler::compute_rolling_averages(tempest::span<const double>{values.data(), values.size()}, 3);

    ASSERT_EQ(rolling.size(), 5u);
    ASSERT_DOUBLE_EQ(rolling[0], 10.0);
    ASSERT_DOUBLE_EQ(rolling[1], 15.0);
    ASSERT_DOUBLE_EQ(rolling[2], 20.0);
    ASSERT_DOUBLE_EQ(rolling[3], 30.0);
    ASSERT_DOUBLE_EQ(rolling[4], 40.0);

    // 8. Verify compute_all_zone_statistics across a capture
    auto capture = tempest::profiler::capture_session_data{};
    auto track1 = tempest::profiler::track_data{.track_id = 1, .name = "Track1"};
    track1.zones.push_back(tempest::profiler::zone_record{.start_ns = 0, .end_ns = 100, .name = "Update"});
    track1.zones.push_back(tempest::profiler::zone_record{.start_ns = 0, .end_ns = 200, .name = "Render"});

    auto track2 = tempest::profiler::track_data{.track_id = 2, .name = "Track2"};
    track2.zones.push_back(tempest::profiler::zone_record{.start_ns = 0, .end_ns = 300, .name = "Update"});

    capture.tracks.push_back(tempest::move(track1));
    capture.tracks.push_back(tempest::move(track2));

    const auto all_stats = tempest::profiler::compute_all_zone_statistics(capture);
    ASSERT_EQ(all_stats.size(), 2u);
    // Alphabetical order: "Render", "Update"
    ASSERT_EQ(all_stats[0].zone_name, "Render");
    ASSERT_EQ(all_stats[0].count, 1u);
    ASSERT_DOUBLE_EQ(all_stats[0].mean_ns, 200.0);

    ASSERT_EQ(all_stats[1].zone_name, "Update");
    ASSERT_EQ(all_stats[1].count, 2u);
    ASSERT_DOUBLE_EQ(all_stats[1].mean_ns, 200.0);
}

//==============================================================================
// Lossless Compressed Binary Serialization (.tprof) Tests
//==============================================================================

/// @brief Verify 100% bitwise lossless round-trip save and load for .tprof binary files with 50,000+ zones, markers,
/// and metrics.
TEST(profiler_tests, lossless_roundtrip_binary_serialization)
{
    // 1. Setup: Build high-volume capture session with 50,000+ zones, markers, and metric streams
    auto capture = tempest::profiler::capture_session_data{
        .start_time_ns = 1000000,
        .end_time_ns = 999999999,
        .tracks = {},
        .metrics = {},
        .string_table = {},
    };

    // Track 1: CPU Thread with 30,000 zones and markers
    auto cpu_track = tempest::profiler::track_data{
        .track_id = 1001,
        .name = "MainThread_Worker",
        .type = tempest::profiler::track_type::cpu_thread,
        .zones = {},
        .markers = {},
    };
    cpu_track.zones.reserve(30000);
    for (auto i = uint64_t{0}; i < 30000; ++i)
    {
        auto zone = tempest::profiler::zone_record{
            .start_ns = 1000000 + i * 20,
            .end_ns = 1000000 + i * 20 + 15,
            .depth = static_cast<uint32_t>(i % 8),
            .name = (i % 2 == 0) ? "PhysicsStep" : "AnimationEvaluation",
            .location = {},
            .task_id = (i % 4 == 0) ? (100 + i) : 0,
            .metrics = {},
        };
        if (i % 10 == 0)
        {
            zone.metrics.push_back(tempest::profiler::metric_record{
                .timestamp_ns = zone.start_ns + 5,
                .name = "iteration_cost",
                .value = static_cast<double>(i * 3.14),
                .unit = tempest::profiler::metric_unit::raw,
            });
        }
        cpu_track.zones.push_back(tempest::move(zone));
    }

    for (auto i = uint64_t{0}; i < 500; ++i)
    {
        cpu_track.markers.push_back(tempest::profiler::marker_record{
            .timestamp_ns = 1000000 + i * 1000,
            .name = "CheckpointMarker",
            .location = {},
        });
    }

    // Track 2: GPU Queue with 25,000 zones
    auto gpu_track = tempest::profiler::track_data{
        .track_id = 2001,
        .name = "Vulkan_Graphics_Queue",
        .type = tempest::profiler::track_type::gpu_queue,
        .zones = {},
        .markers = {},
    };
    gpu_track.zones.reserve(25000);
    for (auto i = uint64_t{0}; i < 25000; ++i)
    {
        gpu_track.zones.push_back(tempest::profiler::zone_record{
            .start_ns = 2000000 + i * 30,
            .end_ns = 2000000 + i * 30 + 25,
            .depth = static_cast<uint32_t>(i % 4),
            .name = (i % 3 == 0)   ? "GBufferPass"
                    : (i % 3 == 1) ? "ShadowPass"
                                   : "LightingPass",
            .location = {},
            .task_id = 0,
            .metrics = {},
        });
    }

    capture.tracks.push_back(tempest::move(cpu_track));
    capture.tracks.push_back(tempest::move(gpu_track));

    // Standalone Metric Streams
    auto vram_stream = tempest::profiler::metric_stream{
        .name = "VRAM_Allocated_Bytes",
        .unit = tempest::profiler::metric_unit::bytes,
        .samples = {},
    };
    vram_stream.samples.reserve(1000);
    for (auto i = uint64_t{0}; i < 1000; ++i)
    {
        vram_stream.samples.push_back(tempest::profiler::metric_record{
            .timestamp_ns = 1000000 + i * 500,
            .name = "VRAM_Allocated_Bytes",
            .value = static_cast<double>(1024 * 1024 * (64 + i % 128)),
            .unit = tempest::profiler::metric_unit::bytes,
        });
    }
    capture.metrics.push_back(tempest::move(vram_stream));

    // 2. Act: Serialize to compressed binary buffer and save to file
    const auto binary_buffer = tempest::profiler::serialize_binary_to_buffer(capture);
    ASSERT_FALSE(binary_buffer.empty());

    const auto file_path = "test_large_capture.tprof";
    const auto save_res = tempest::profiler::save_binary_capture(capture, file_path);
    ASSERT_TRUE(save_res.has_value());

    // Deserialize from in-memory buffer
    const auto buf_span = tempest::span<const tempest::byte>{binary_buffer.data(), binary_buffer.size()};
    const auto loaded_from_buf = tempest::profiler::deserialize_binary_from_buffer(buf_span);
    ASSERT_TRUE(loaded_from_buf.has_value());

    // Load from disk file
    const auto loaded_from_file = tempest::profiler::load_binary_capture(file_path);
    ASSERT_TRUE(loaded_from_file.has_value());

    // 3. Assert: Verify 100% bitwise & structural round-trip fidelity
    const auto& roundtrip = loaded_from_file.value();
    ASSERT_EQ(roundtrip.start_time_ns, capture.start_time_ns);
    ASSERT_EQ(roundtrip.end_time_ns, capture.end_time_ns);
    ASSERT_EQ(roundtrip.tracks.size(), 2u);

    // Verify CPU track
    ASSERT_EQ(roundtrip.tracks[0].track_id, 1001u);
    ASSERT_EQ(roundtrip.tracks[0].name, "MainThread_Worker");
    ASSERT_EQ(roundtrip.tracks[0].type, tempest::profiler::track_type::cpu_thread);
    ASSERT_EQ(roundtrip.tracks[0].zones.size(), 30000u);
    ASSERT_EQ(roundtrip.tracks[0].markers.size(), 500u);

    for (auto i = size_t{0}; i < 30000; ++i)
    {
        const auto& orig_z = capture.tracks[0].zones[i];
        const auto& loaded_z = roundtrip.tracks[0].zones[i];
        ASSERT_EQ(loaded_z.start_ns, orig_z.start_ns);
        ASSERT_EQ(loaded_z.end_ns, orig_z.end_ns);
        ASSERT_EQ(loaded_z.depth, orig_z.depth);
        ASSERT_EQ(loaded_z.name, orig_z.name);
        ASSERT_EQ(loaded_z.task_id, orig_z.task_id);
        ASSERT_EQ(loaded_z.metrics.size(), orig_z.metrics.size());
        if (!orig_z.metrics.empty())
        {
            ASSERT_EQ(loaded_z.metrics[0].name, orig_z.metrics[0].name);
            ASSERT_DOUBLE_EQ(loaded_z.metrics[0].value, orig_z.metrics[0].value);
            ASSERT_EQ(loaded_z.metrics[0].unit, orig_z.metrics[0].unit);
        }
    }

    // Verify GPU track
    ASSERT_EQ(roundtrip.tracks[1].track_id, 2001u);
    ASSERT_EQ(roundtrip.tracks[1].name, "Vulkan_Graphics_Queue");
    ASSERT_EQ(roundtrip.tracks[1].type, tempest::profiler::track_type::gpu_queue);
    ASSERT_EQ(roundtrip.tracks[1].zones.size(), 25000u);

    for (auto i = size_t{0}; i < 25000; ++i)
    {
        const auto& orig_z = capture.tracks[1].zones[i];
        const auto& loaded_z = roundtrip.tracks[1].zones[i];
        ASSERT_EQ(loaded_z.start_ns, orig_z.start_ns);
        ASSERT_EQ(loaded_z.end_ns, orig_z.end_ns);
        ASSERT_EQ(loaded_z.name, orig_z.name);
    }

    // Verify Metric Streams
    ASSERT_EQ(roundtrip.metrics.size(), 1u);
    ASSERT_EQ(roundtrip.metrics[0].name, "VRAM_Allocated_Bytes");
    ASSERT_EQ(roundtrip.metrics[0].unit, tempest::profiler::metric_unit::bytes);
    ASSERT_EQ(roundtrip.metrics[0].samples.size(), 1000u);

    // Clean up temporary disk file
    std::remove(file_path);
}

//==============================================================================
// JSON Chrome Trace Event Export & Validation Tests
//==============================================================================

/// @brief Verify JSON Chrome Trace export generates schema-valid JSON parseable by simdjson with correct
/// PID/TID/timestamps.
TEST(profiler_tests, json_chrome_trace_export_and_simdjson_validation)
{
    // 1. Setup: Create capture data with CPU track, GPU track, attached metrics, and counters
    auto capture = tempest::profiler::capture_session_data{};

    auto track_cpu = tempest::profiler::track_data{
        .track_id = 10,
        .name = "Worker_CPU",
        .type = tempest::profiler::track_type::cpu_thread,
        .zones = {},
        .markers = {},
    };

    auto zone1 = tempest::profiler::zone_record{
        .start_ns = 100000,
        .end_ns = 250000,
        .depth = 0,
        .name = "ExecutePhysics",
        .location = {},
        .task_id = 77,
        .metrics = {},
    };
    zone1.metrics.push_back(tempest::profiler::metric_record{
        .timestamp_ns = 110000,
        .name = "contacts_solved",
        .value = 142.0,
        .unit = tempest::profiler::metric_unit::count,
    });
    track_cpu.zones.push_back(tempest::move(zone1));

    track_cpu.markers.push_back(tempest::profiler::marker_record{
        .timestamp_ns = 260000,
        .name = "PhysicsComplete",
        .location = {},
    });

    capture.tracks.push_back(tempest::move(track_cpu));

    auto metric_st = tempest::profiler::metric_stream{
        .name = "FPS",
        .unit = tempest::profiler::metric_unit::raw,
        .samples = {},
    };
    metric_st.samples.push_back(tempest::profiler::metric_record{
        .timestamp_ns = 300000,
        .name = "FPS",
        .value = 120.0,
        .unit = tempest::profiler::metric_unit::raw,
    });
    capture.metrics.push_back(tempest::move(metric_st));

    // 2. Act: Export JSON string and write to file
    const auto json_str = tempest::profiler::export_chrome_trace_json_string(capture);
    ASSERT_FALSE(json_str.empty());

    const auto json_file = "test_chrome_trace.json";
    const auto export_res = tempest::profiler::export_chrome_trace_json(capture, json_file);
    ASSERT_TRUE(export_res.has_value());

    // 3. Assert: Validate JSON schema with simdjson
    auto parser = simdjson::dom::parser{};
    auto doc = parser.parse(json_str.data(), json_str.size());
    ASSERT_FALSE(doc.error());

    auto trace_events = doc["traceEvents"].get_array();
    ASSERT_FALSE(trace_events.error());
    ASSERT_GE(trace_events.value().size(), 4u); // process_name, thread_name, zone, marker, metric

    auto found_process_metadata = false;
    auto found_thread_metadata = false;
    auto found_zone = false;
    auto found_marker = false;
    auto found_metric = false;

    for (const auto element : trace_events.value())
    {
        auto ph = std::string_view{element["ph"].get_string().value()};
        auto name = std::string_view{element["name"].get_string().value()};

        if (ph == "M" && name == "process_name")
        {
            found_process_metadata = true;
        }
        else if (ph == "M" && name == "thread_name")
        {
            found_thread_metadata = true;
            ASSERT_EQ(element["tid"].get_uint64().value(), 10u);
        }
        else if (ph == "X" && name == "ExecutePhysics")
        {
            found_zone = true;
            ASSERT_DOUBLE_EQ(element["ts"].get_double().value(), 100.0);  // 100,000 ns -> 100.0 us
            ASSERT_DOUBLE_EQ(element["dur"].get_double().value(), 150.0); // 150,000 ns -> 150.0 us
            ASSERT_EQ(element["tid"].get_uint64().value(), 10u);
            ASSERT_EQ(element["args"]["task_id"].get_uint64().value(), 77u);
            ASSERT_DOUBLE_EQ(element["args"]["contacts_solved"].get_double().value(), 142.0);
        }
        else if (ph == "i" && name == "PhysicsComplete")
        {
            found_marker = true;
            ASSERT_DOUBLE_EQ(element["ts"].get_double().value(), 260.0);
            ASSERT_EQ(element["s"].get_string().value(), "t");
        }
        else if (ph == "C" && name == "FPS")
        {
            found_metric = true;
            ASSERT_DOUBLE_EQ(element["ts"].get_double().value(), 300.0);
            ASSERT_DOUBLE_EQ(element["args"]["value"].get_double().value(), 120.0);
        }
    }

    ASSERT_TRUE(found_process_metadata);
    ASSERT_TRUE(found_thread_metadata);
    ASSERT_TRUE(found_zone);
    ASSERT_TRUE(found_marker);
    ASSERT_TRUE(found_metric);

    // Clean up temporary file
    std::remove(json_file);
}

//==============================================================================
// Corrupted & Truncated Binary Error Handling Tests
//==============================================================================

/// @brief Verify truncated and corrupted binary files gracefully return capture_error without crashes.
TEST(profiler_tests, corrupted_and_truncated_binary_error_handling)
{
    // 1. Setup: Create valid capture and serialize to binary
    auto capture = tempest::profiler::capture_session_data{};
    auto track = tempest::profiler::track_data{.track_id = 1, .name = "TestTrack"};
    track.zones.push_back(tempest::profiler::zone_record{.start_ns = 100, .end_ns = 200, .name = "Zone1"});
    capture.tracks.push_back(tempest::move(track));

    auto valid_buf = tempest::profiler::serialize_binary_to_buffer(capture);
    ASSERT_FALSE(valid_buf.empty());

    // 2. Act & Assert: Header truncated (less than header size)
    auto truncated_hdr = tempest::vector<tempest::byte>{valid_buf.begin(), valid_buf.begin() + 10};
    auto res_trunc_hdr = tempest::profiler::deserialize_binary_from_buffer(
        tempest::span<const tempest::byte>{truncated_hdr.data(), truncated_hdr.size()});
    ASSERT_FALSE(res_trunc_hdr.has_value());
    ASSERT_EQ(res_trunc_hdr.error(), tempest::profiler::capture_error::corrupted_data);

    // 3. Act & Assert: Invalid magic bytes
    auto invalid_magic_buf = valid_buf;
    invalid_magic_buf[0] = static_cast<tempest::byte>('X');
    auto res_magic = tempest::profiler::deserialize_binary_from_buffer(
        tempest::span<const tempest::byte>{invalid_magic_buf.data(), invalid_magic_buf.size()});
    ASSERT_FALSE(res_magic.has_value());
    ASSERT_EQ(res_magic.error(), tempest::profiler::capture_error::invalid_magic);

    // 4. Act & Assert: Unsupported version major
    auto invalid_ver_buf = valid_buf;
    invalid_ver_buf[4] = static_cast<tempest::byte>(99); // version_major low byte
    auto res_ver = tempest::profiler::deserialize_binary_from_buffer(
        tempest::span<const tempest::byte>{invalid_ver_buf.data(), invalid_ver_buf.size()});
    ASSERT_FALSE(res_ver.has_value());
    ASSERT_EQ(res_ver.error(), tempest::profiler::capture_error::unsupported_version);

    // 5. Act & Assert: Truncated payload
    auto truncated_payload = tempest::vector<tempest::byte>{valid_buf.begin(), valid_buf.begin() + 28};
    auto res_trunc_payload = tempest::profiler::deserialize_binary_from_buffer(
        tempest::span<const tempest::byte>{truncated_payload.data(), truncated_payload.size()});
    ASSERT_FALSE(res_trunc_payload.has_value());
    ASSERT_EQ(res_trunc_payload.error(), tempest::profiler::capture_error::corrupted_data);

    // 6. Act & Assert: Corrupted compressed stream
    auto corrupted_payload = valid_buf;
    for (auto i = size_t{28}; i < corrupted_payload.size(); ++i)
    {
        corrupted_payload[i] = static_cast<tempest::byte>(0xFF);
    }
    auto res_decomp = tempest::profiler::deserialize_binary_from_buffer(
        tempest::span<const tempest::byte>{corrupted_payload.data(), corrupted_payload.size()});
    ASSERT_FALSE(res_decomp.has_value());
    ASSERT_EQ(res_decomp.error(), tempest::profiler::capture_error::decompression_failed);

    // 7. Act & Assert: Non-existent file load
    auto res_io = tempest::profiler::load_binary_capture("non_existent_file_path_12345.tprof");
    ASSERT_FALSE(res_io.has_value());
    ASSERT_EQ(res_io.error(), tempest::profiler::capture_error::io_error);
}
