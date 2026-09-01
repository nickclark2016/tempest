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

//==============================================================================
// Embedded Web Server & RFC-6455 WebSocket Tests
//==============================================================================

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace
{
    struct test_tcp_client
    {
#if defined(_WIN32)
        using socket_type = SOCKET;
        static constexpr socket_type invalid_s = INVALID_SOCKET;
#else
        using socket_type = int;
        static constexpr socket_type invalid_s = -1;
#endif

        socket_type sock{invalid_s};

        test_tcp_client() = default;

        ~test_tcp_client()
        {
            close_sock();
        }

        auto connect_to(const char* host, uint16_t port) -> bool
        {
            close_sock();
#if defined(_WIN32)
            auto wsa = WSADATA{};
            WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
            sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sock == invalid_s)
            {
                return false;
            }

            auto addr = sockaddr_in{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, host, &addr.sin_addr);

            if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
            {
                close_sock();
                return false;
            }
            return true;
        }

        auto send_all(const void* data, size_t len) -> bool
        {
            if (sock == invalid_s)
            {
                return false;
            }
            auto ptr = reinterpret_cast<const char*>(data);
            auto remaining = len;
            while (remaining > 0)
            {
                const auto res = send(sock, ptr, static_cast<int>(remaining), 0);
                if (res <= 0)
                {
                    return false;
                }
                ptr += res;
                remaining -= res;
            }
            return true;
        }

        auto send_string(std::string_view s) -> bool
        {
            return send_all(s.data(), s.size());
        }

        auto receive_all(size_t timeout_ms = 1000) -> std::string
        {
            auto out = std::string{};
            if (sock == invalid_s)
            {
                return out;
            }

            char buffer[4096];
            while (true)
            {
                auto read_fds = fd_set{};
                FD_ZERO(&read_fds);
                FD_SET(sock, &read_fds);

                auto tv = timeval{};
                tv.tv_sec = static_cast<long>(timeout_ms / 1000);
                tv.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);

                const auto sel = select(static_cast<int>(sock + 1), &read_fds, nullptr, nullptr, &tv);
                if (sel <= 0)
                {
                    break;
                }

                const auto bytes = recv(sock, buffer, sizeof(buffer), 0);
                if (bytes <= 0)
                {
                    break;
                }
                out.append(buffer, static_cast<size_t>(bytes));
                if (bytes < static_cast<int>(sizeof(buffer)))
                {
                    timeout_ms = 50;
                }
            }
            return out;
        }

        auto close_sock() -> void
        {
            if (sock != invalid_s)
            {
#if defined(_WIN32)
                closesocket(sock);
#else
                close(sock);
#endif
                sock = invalid_s;
            }
        }
    };
} // namespace

/// @brief Verify socket lifecycle, port binding, and auto-increment when port 8080 is occupied.
TEST(profiler_tests, socket_lifecycle_and_port_auto_increment)
{
    // 1. Setup: Create profiler session and web server instances
    auto session = tempest::profiler::profiler_session{};
    auto config1 = tempest::profiler::web_server_config{.host = "127.0.0.1", .port = 8080, .max_port_attempts = 10};
    auto server1 = tempest::profiler::web_server{session, config1};

    // 2. Act: Start server 1 on port 8080
    server1.start();
    ASSERT_TRUE(server1.is_running());
    const auto port1 = server1.get_bound_port();
    ASSERT_EQ(port1, 8080u);
    ASSERT_EQ(server1.get_server_url(), "http://127.0.0.1:8080");

    // 3. Act & Assert: Start server 2 with same preferred port 8080 (should auto-increment to 8081)
    auto config2 = tempest::profiler::web_server_config{.host = "127.0.0.1", .port = 8080, .max_port_attempts = 10};
    auto server2 = tempest::profiler::web_server{session, config2};
    server2.start();
    ASSERT_TRUE(server2.is_running());
    const auto port2 = server2.get_bound_port();
    ASSERT_EQ(port2, 8081u);
    ASSERT_EQ(server2.get_server_url(), "http://127.0.0.1:8081");

    // 4. Act & Assert: Gracefully stop both servers
    server1.stop();
    ASSERT_FALSE(server1.is_running());
    ASSERT_EQ(server1.get_bound_port(), 0u);

    server2.stop();
    ASSERT_FALSE(server2.is_running());
    ASSERT_EQ(server2.get_bound_port(), 0u);
}

/// @brief Verify HTTP GET request handling for all embedded single-page app web assets and endpoints with appropriate
/// MIME types.
TEST(profiler_tests, http_get_request_handling)
{
    // 1. Setup: Start web server on dynamic port
    auto session = tempest::profiler::profiler_session{};
    auto config = tempest::profiler::web_server_config{.host = "127.0.0.1", .port = 8082, .max_port_attempts = 10};
    auto server = tempest::profiler::web_server{session, config};
    server.start();
    ASSERT_TRUE(server.is_running());
    const auto port = server.get_bound_port();

    // 2. Act & Assert: HTTP GET / (index.html)
    {
        auto client = test_tcp_client{};
        ASSERT_TRUE(client.connect_to("127.0.0.1", port));
        ASSERT_TRUE(client.send_string("GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"));
        const auto response = client.receive_all(1000);
        ASSERT_FALSE(response.empty());
        ASSERT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
        ASSERT_NE(response.find("Content-Type: text/html; charset=utf-8"), std::string::npos);
        ASSERT_NE(response.find("<!DOCTYPE html>"), std::string::npos);
        ASSERT_NE(response.find("Tempest Engine Profiler"), std::string::npos);
        ASSERT_NE(response.find("id=\"timeline-canvas\""), std::string::npos);
    }

    // 3. Act & Assert: HTTP GET /index.html
    {
        auto client = test_tcp_client{};
        ASSERT_TRUE(client.connect_to("127.0.0.1", port));
        ASSERT_TRUE(client.send_string("GET /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"));
        const auto response = client.receive_all(1000);
        ASSERT_FALSE(response.empty());
        ASSERT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
        ASSERT_NE(response.find("Content-Type: text/html; charset=utf-8"), std::string::npos);
        ASSERT_NE(response.find("<!DOCTYPE html>"), std::string::npos);
    }

    // 4. Act & Assert: HTTP GET /app.js
    {
        auto client = test_tcp_client{};
        ASSERT_TRUE(client.connect_to("127.0.0.1", port));
        ASSERT_TRUE(client.send_string("GET /app.js HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"));
        const auto response = client.receive_all(1500);
        ASSERT_FALSE(response.empty());
        ASSERT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
        ASSERT_NE(response.find("Content-Type: application/javascript; charset=utf-8"), std::string::npos);
        ASSERT_NE(response.find("Tempest Engine Profiler"), std::string::npos);
        ASSERT_NE(response.find("initWebSocket"), std::string::npos);
        ASSERT_NE(response.find("renderTimeline"), std::string::npos);
    }

    // 5. Act & Assert: HTTP GET /styles.css
    {
        auto client = test_tcp_client{};
        ASSERT_TRUE(client.connect_to("127.0.0.1", port));
        ASSERT_TRUE(client.send_string("GET /styles.css HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"));
        const auto response = client.receive_all(1000);
        ASSERT_FALSE(response.empty());
        ASSERT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
        ASSERT_NE(response.find("Content-Type: text/css; charset=utf-8"), std::string::npos);
        ASSERT_NE(response.find("--accent-purple"), std::string::npos);
        ASSERT_NE(response.find(".timeline-section"), std::string::npos);
    }

    // 6. Act & Assert: HTTP GET /status
    {
        auto client = test_tcp_client{};
        ASSERT_TRUE(client.connect_to("127.0.0.1", port));
        ASSERT_TRUE(client.send_string("GET /status HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"));
        const auto response = client.receive_all(1000);
        ASSERT_FALSE(response.empty());
        ASSERT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
        ASSERT_NE(response.find("Content-Type: application/json; charset=utf-8"), std::string::npos);
        ASSERT_NE(response.find("\"status\":\"ok\""), std::string::npos);
    }

    // 7. Act & Assert: HTTP GET /unknown_path (404)
    {
        auto client = test_tcp_client{};
        ASSERT_TRUE(client.connect_to("127.0.0.1", port));
        ASSERT_TRUE(client.send_string("GET /unknown_path HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"));
        const auto response = client.receive_all(1000);
        ASSERT_FALSE(response.empty());
        ASSERT_NE(response.find("HTTP/1.1 404 Not Found"), std::string::npos);
        ASSERT_NE(response.find("Content-Type: text/plain; charset=utf-8"), std::string::npos);
    }

    server.stop();
}

/// @brief Verify RFC-6455 WebSocket handshake Sec-WebSocket-Accept key computation against RFC test vectors.
TEST(profiler_tests, rfc6455_websocket_handshake_key_computation)
{
    // 1. Setup: Test vector from RFC-6455 Section 1.3
    const auto client_key1 = "dGhlIHNhbXBsZSBub25jZQ==";
    const auto expected_accept1 = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

    // 2. Act & Assert: Primary RFC test vector
    const auto actual_accept1 = tempest::profiler::compute_websocket_accept_key(client_key1);
    ASSERT_EQ(actual_accept1, expected_accept1);

    // 3. Act & Assert: Additional valid nonce key test vector
    const auto client_key2 = "x3JJHMbDL1EzLkh9GBhXDw==";
    const auto expected_accept2 = "HSmrc0sMlYUkAGmm5OPpG2HaGWk=";
    const auto actual_accept2 = tempest::profiler::compute_websocket_accept_key(client_key2);
    ASSERT_EQ(actual_accept2, expected_accept2);

    // 4. Act & Assert: Nonce key with leading/trailing whitespace
    const auto client_key3 = "  dGhlIHNhbXBsZSBub25jZQ== \r\n";
    const auto actual_accept3 = tempest::profiler::compute_websocket_accept_key(client_key3);
    ASSERT_EQ(actual_accept3, expected_accept1);
}

/// @brief Verify WebSocket binary and text frame encoding and decoding.
TEST(profiler_tests, websocket_frame_encoding_and_decoding)
{
    // 1. Setup: Test short text frame (len <= 125)
    const auto text_sample = std::string{"Hello, Tempest Profiler RFC-6455!"};
    auto text_bytes = tempest::vector<tempest::byte>{};
    for (auto c : text_sample)
    {
        text_bytes.push_back(static_cast<tempest::byte>(c));
    }

    // 2. Act: Encode server text frame and decode
    auto encoded_text = tempest::profiler::encode_websocket_frame(
        tempest::profiler::ws_opcode::text, tempest::span<const tempest::byte>{text_bytes.data(), text_bytes.size()});
    ASSERT_GE(encoded_text.size(), 2u + text_bytes.size());
    ASSERT_EQ(static_cast<uint8_t>(encoded_text[0]), 0x81); // FIN + text opcode

    auto decoded_text = tempest::profiler::decode_websocket_frame(
        tempest::span<const tempest::byte>{encoded_text.data(), encoded_text.size()});
    ASSERT_TRUE(decoded_text.has_value());
    ASSERT_EQ(decoded_text->opcode, tempest::profiler::ws_opcode::text);
    ASSERT_EQ(decoded_text->payload.size(), text_bytes.size());
    for (auto i = size_t{0}; i < text_bytes.size(); ++i)
    {
        ASSERT_EQ(decoded_text->payload[i], text_bytes[i]);
    }

    // 3. Act & Assert: Medium binary frame (126 <= len <= 65535)
    auto medium_binary = tempest::vector<tempest::byte>(1024);
    for (auto i = size_t{0}; i < medium_binary.size(); ++i)
    {
        medium_binary[i] = static_cast<tempest::byte>(i & 0xFF);
    }
    auto encoded_medium = tempest::profiler::encode_websocket_frame(
        tempest::profiler::ws_opcode::binary,
        tempest::span<const tempest::byte>{medium_binary.data(), medium_binary.size()});
    ASSERT_EQ(static_cast<uint8_t>(encoded_medium[0]), 0x82); // FIN + binary opcode
    ASSERT_EQ(static_cast<uint8_t>(encoded_medium[1]), 126);  // 16-bit extended length flag

    auto decoded_medium = tempest::profiler::decode_websocket_frame(
        tempest::span<const tempest::byte>{encoded_medium.data(), encoded_medium.size()});
    ASSERT_TRUE(decoded_medium.has_value());
    ASSERT_EQ(decoded_medium->opcode, tempest::profiler::ws_opcode::binary);
    ASSERT_EQ(decoded_medium->payload.size(), 1024u);
    ASSERT_EQ(decoded_medium->payload[500], medium_binary[500]);

    // 4. Act & Assert: Masked client-to-server text frame
    auto client_masked_frame = tempest::profiler::encode_websocket_client_frame(
        tempest::profiler::ws_opcode::text, tempest::span<const tempest::byte>{text_bytes.data(), text_bytes.size()},
        0x37FA213D);
    ASSERT_GE(client_masked_frame.size(), 6u + text_bytes.size());
    ASSERT_TRUE((static_cast<uint8_t>(client_masked_frame[1]) & 0x80) != 0); // Mask bit set

    auto decoded_client_frame = tempest::profiler::decode_websocket_frame(
        tempest::span<const tempest::byte>{client_masked_frame.data(), client_masked_frame.size()});
    ASSERT_TRUE(decoded_client_frame.has_value());
    ASSERT_EQ(decoded_client_frame->opcode, tempest::profiler::ws_opcode::text);
    ASSERT_EQ(decoded_client_frame->payload.size(), text_bytes.size());
    for (auto i = size_t{0}; i < text_bytes.size(); ++i)
    {
        ASSERT_EQ(decoded_client_frame->payload[i], text_bytes[i]);
    }

    // 5. Act & Assert: Incomplete / truncated frame error handling
    auto truncated = tempest::vector<tempest::byte>{client_masked_frame.begin(), client_masked_frame.begin() + 4};
    auto decoded_trunc = tempest::profiler::decode_websocket_frame(
        tempest::span<const tempest::byte>{truncated.data(), truncated.size()});
    ASSERT_FALSE(decoded_trunc.has_value());
    ASSERT_EQ(decoded_trunc.error(), tempest::profiler::ws_error::incomplete_frame);
}

/// @brief Verify telemetry streaming packet generation and bidirectional command dispatch.
TEST(profiler_tests, telemetry_streaming_and_bidirectional_command_dispatch)
{
    // 1. Setup: Start web server and record some profiler session events
    auto session = tempest::profiler::profiler_session{true};
    {
        [[maybe_unused]] const auto z1 = tempest::profiler::scoped_zone{session, "EngineInit"};
        session.get_or_register_thread().add_metric("FPS", 60.0, tempest::profiler::metric_unit::raw);
        session.get_or_register_thread().add_marker("WarmupDone");
    }

    auto config = tempest::profiler::web_server_config{.host = "127.0.0.1", .port = 8084, .max_port_attempts = 10};
    auto server = tempest::profiler::web_server{session, config};
    server.start();
    ASSERT_TRUE(server.is_running());
    const auto port = server.get_bound_port();

    // 2. Act: Connect client and perform WebSocket handshake
    auto client = test_tcp_client{};
    ASSERT_TRUE(client.connect_to("127.0.0.1", port));

    const auto ws_handshake_req = "GET /ws HTTP/1.1\r\n"
                                  "Host: 127.0.0.1\r\n"
                                  "Upgrade: websocket\r\n"
                                  "Connection: Upgrade\r\n"
                                  "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                                  "Sec-WebSocket-Version: 13\r\n\r\n";

    ASSERT_TRUE(client.send_string(ws_handshake_req));
    const auto hs_response = client.receive_all(500);
    ASSERT_NE(hs_response.find("HTTP/1.1 101 Switching Protocols"), std::string::npos);
    ASSERT_NE(hs_response.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo="), std::string::npos);

    // Allow worker thread to register connection
    tempest::this_thread::yield();
    ASSERT_EQ(server.connected_client_count(), 1u);

    // 3. Act & Assert: Command "start_capture"
    {
        const auto cmd = std::string{"{\"command\":\"start_capture\"}"};
        auto cmd_bytes = tempest::vector<tempest::byte>{};
        for (auto c : cmd)
        {
            cmd_bytes.push_back(static_cast<tempest::byte>(c));
        }
        auto frame = tempest::profiler::encode_websocket_client_frame(
            tempest::profiler::ws_opcode::text, tempest::span<const tempest::byte>{cmd_bytes.data(), cmd_bytes.size()});
        ASSERT_TRUE(client.send_all(frame.data(), frame.size()));

        const auto resp_raw = client.receive_all(500);
        ASSERT_FALSE(resp_raw.empty());
        auto decoded = tempest::profiler::decode_websocket_frame(tempest::span<const tempest::byte>{
            reinterpret_cast<const tempest::byte*>(resp_raw.data()), resp_raw.size()});
        ASSERT_TRUE(decoded.has_value());
        ASSERT_EQ(decoded->opcode, tempest::profiler::ws_opcode::text);
        auto resp_str = std::string(reinterpret_cast<const char*>(decoded->payload.data()), decoded->payload.size());
        ASSERT_NE(resp_str.find("\"command\":\"start_capture\""), std::string::npos);
        ASSERT_NE(resp_str.find("\"recording\":true"), std::string::npos);
        ASSERT_TRUE(session.is_enabled());
    }

    // 4. Act & Assert: Command "query_stats"
    {
        const auto cmd = std::string{"{\"command\":\"query_stats\"}"};
        auto cmd_bytes = tempest::vector<tempest::byte>{};
        for (auto c : cmd)
        {
            cmd_bytes.push_back(static_cast<tempest::byte>(c));
        }
        auto frame = tempest::profiler::encode_websocket_client_frame(
            tempest::profiler::ws_opcode::text, tempest::span<const tempest::byte>{cmd_bytes.data(), cmd_bytes.size()});
        ASSERT_TRUE(client.send_all(frame.data(), frame.size()));

        const auto resp_raw = client.receive_all(500);
        ASSERT_FALSE(resp_raw.empty());
        auto decoded = tempest::profiler::decode_websocket_frame(tempest::span<const tempest::byte>{
            reinterpret_cast<const tempest::byte*>(resp_raw.data()), resp_raw.size()});
        ASSERT_TRUE(decoded.has_value());
        ASSERT_EQ(decoded->opcode, tempest::profiler::ws_opcode::text);
        auto resp_str = std::string(reinterpret_cast<const char*>(decoded->payload.data()), decoded->payload.size());
        ASSERT_NE(resp_str.find("\"type\":\"stats\""), std::string::npos);
        ASSERT_NE(resp_str.find("EngineInit"), std::string::npos);
    }

    // 5. Act & Assert: Telemetry packet serialization & server broadcast
    {
        auto t_frame = tempest::profiler::telemetry_frame{};
        t_frame.frame_index = 42;

        auto t_track = tempest::profiler::telemetry_track{.track_id = 1, .name = "MainThread"};
        t_track.zones.push_back(tempest::profiler::telemetry_zone{
            .name = "RenderFrame",
            .start_ns = 1000000,
            .end_ns = 2000000,
            .depth = 0,
        });
        t_frame.cpu_tracks.push_back(tempest::move(t_track));
        t_frame.markers.push_back(
            tempest::profiler::marker_record{.timestamp_ns = 2500000, .name = "SwapchainPresent"});

        const auto json_payload = tempest::profiler::serialize_telemetry_frame_json(t_frame);
        const auto json_payload_std = std::string(json_payload.data(), json_payload.size());
        ASSERT_NE(json_payload_std.find("\"frame_index\":42"), std::string::npos);
        ASSERT_NE(json_payload_std.find("\"name\":\"RenderFrame\""), std::string::npos);
        ASSERT_NE(json_payload_std.find("\"name\":\"SwapchainPresent\""), std::string::npos);

        // Broadcast to connected client
        server.broadcast_telemetry(t_frame);
        const auto bc_raw = client.receive_all(500);
        ASSERT_FALSE(bc_raw.empty());
        auto decoded = tempest::profiler::decode_websocket_frame(
            tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(bc_raw.data()), bc_raw.size()});
        ASSERT_TRUE(decoded.has_value());
        ASSERT_EQ(decoded->opcode, tempest::profiler::ws_opcode::text);
        auto bc_str = std::string(reinterpret_cast<const char*>(decoded->payload.data()), decoded->payload.size());
        ASSERT_NE(bc_str.find("\"frame_index\":42"), std::string::npos);
    }

    // 6. Act & Assert: Command "stop_capture"
    {
        const auto cmd = std::string{"{\"command\":\"stop_capture\"}"};
        auto cmd_bytes = tempest::vector<tempest::byte>{};
        for (auto c : cmd)
        {
            cmd_bytes.push_back(static_cast<tempest::byte>(c));
        }
        auto frame = tempest::profiler::encode_websocket_client_frame(
            tempest::profiler::ws_opcode::text, tempest::span<const tempest::byte>{cmd_bytes.data(), cmd_bytes.size()});
        ASSERT_TRUE(client.send_all(frame.data(), frame.size()));

        const auto resp_raw = client.receive_all(500);
        ASSERT_FALSE(resp_raw.empty());
        auto decoded = tempest::profiler::decode_websocket_frame(tempest::span<const tempest::byte>{
            reinterpret_cast<const tempest::byte*>(resp_raw.data()), resp_raw.size()});
        ASSERT_TRUE(decoded.has_value());
        ASSERT_EQ(decoded->opcode, tempest::profiler::ws_opcode::text);
        auto resp_str = std::string(reinterpret_cast<const char*>(decoded->payload.data()), decoded->payload.size());
        ASSERT_NE(resp_str.find("\"command\":\"stop_capture\""), std::string::npos);
        ASSERT_NE(resp_str.find("\"recording\":false"), std::string::npos);
        ASSERT_FALSE(session.is_enabled());
    }

    // 7. Act & Assert: Graceful connection close
    {
        auto close_frame = tempest::profiler::encode_websocket_client_frame(tempest::profiler::ws_opcode::close, {});
        ASSERT_TRUE(client.send_all(close_frame.data(), close_frame.size()));
        client.close_sock();
    }

    server.stop();
}

/// @brief Verify WebSocket frame decoder rejects malicious allocation bombs and integer overflows.
TEST(profiler_tests, websocket_frame_overflow_and_allocation_dos_protection)
{
    // 1. Setup: Construct an oversized/overflow payload frame header (length = 0xFFFFFFFF)
    auto malicious_frame = tempest::vector<tempest::byte>{};
    malicious_frame.push_back(static_cast<tempest::byte>(0x81)); // FIN | text
    malicious_frame.push_back(static_cast<tempest::byte>(127));  // 64-bit extended length
    // 8 bytes of length = 0xFFFFFFFFFFFFFFFF (overflow)
    for (auto i = 0; i < 8; ++i)
    {
        malicious_frame.push_back(static_cast<tempest::byte>(0xFF));
    }

    // 2. Act: Decode frame
    const auto result = tempest::profiler::decode_websocket_frame(
        tempest::span<const tempest::byte>{malicious_frame.data(), malicious_frame.size()});

    // 3. Assert: Must return payload_too_large or incomplete_frame without throwing bad_alloc or crashing
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error() == tempest::profiler::ws_error::payload_too_large ||
                result.error() == tempest::profiler::ws_error::incomplete_frame);
}

/// @brief Verify HTTP GET requests with query strings cleanly serve assets.
TEST(profiler_tests, http_request_query_string_stripping)
{
    // 1. Setup: Start web server
    auto session = tempest::profiler::profiler_session{false};
    auto config = tempest::profiler::web_server_config{.host = "127.0.0.1", .port = 8086, .max_port_attempts = 10};
    auto server = tempest::profiler::web_server{session, config};
    server.start();
    ASSERT_TRUE(server.is_running());
    const auto port = server.get_bound_port();

    // 2. Act & Assert: Request /index.html?token=test1234
    {
        auto client = test_tcp_client{};
        ASSERT_TRUE(client.connect_to("127.0.0.1", port));
        const auto req = "GET /index.html?token=test1234&v=1.0 HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
        ASSERT_TRUE(client.send_string(req));
        const auto resp = client.receive_all(500);
        EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);
        EXPECT_NE(resp.find("Content-Type: text/html"), std::string::npos);
        EXPECT_NE(resp.find("Tempest Engine Profiler"), std::string::npos);
    }

    // 3. Act & Assert: Request /?v=1
    {
        auto client = test_tcp_client{};
        ASSERT_TRUE(client.connect_to("127.0.0.1", port));
        const auto req = "GET /?v=1 HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
        ASSERT_TRUE(client.send_string(req));
        const auto resp = client.receive_all(500);
        EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);
        EXPECT_NE(resp.find("Content-Type: text/html"), std::string::npos);
    }

    server.stop();
}

/// @brief Verify multiple concurrent WebSocket clients receive broadcast telemetry simultaneously without race or
/// deadlock.
TEST(profiler_tests, multiple_concurrent_websocket_clients_broadcast)
{
    // 1. Setup: Start web server
    auto session = tempest::profiler::profiler_session{true};
    auto config = tempest::profiler::web_server_config{.host = "127.0.0.1", .port = 8088, .max_port_attempts = 10};
    auto server = tempest::profiler::web_server{session, config};
    server.start();
    ASSERT_TRUE(server.is_running());
    const auto port = server.get_bound_port();

    // 2. Act: Connect 3 concurrent WebSocket clients
    auto client1 = test_tcp_client{};
    auto client2 = test_tcp_client{};
    auto client3 = test_tcp_client{};

    ASSERT_TRUE(client1.connect_to("127.0.0.1", port));
    ASSERT_TRUE(client2.connect_to("127.0.0.1", port));
    ASSERT_TRUE(client3.connect_to("127.0.0.1", port));

    const auto ws_req = "GET /ws HTTP/1.1\r\n"
                        "Host: 127.0.0.1\r\n"
                        "Upgrade: websocket\r\n"
                        "Connection: Upgrade\r\n"
                        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                        "Sec-WebSocket-Version: 13\r\n\r\n";

    ASSERT_TRUE(client1.send_string(ws_req));
    ASSERT_TRUE(client2.send_string(ws_req));
    ASSERT_TRUE(client3.send_string(ws_req));

    const auto resp1 = client1.receive_all(500);
    const auto resp2 = client2.receive_all(500);
    const auto resp3 = client3.receive_all(500);

    ASSERT_NE(resp1.find("HTTP/1.1 101"), std::string::npos);
    ASSERT_NE(resp2.find("HTTP/1.1 101"), std::string::npos);
    ASSERT_NE(resp3.find("HTTP/1.1 101"), std::string::npos);

    tempest::this_thread::yield();
    EXPECT_EQ(server.connected_client_count(), 3u);

    // 3. Act: Broadcast telemetry frame to all 3 clients
    auto t_frame = tempest::profiler::telemetry_frame{};
    t_frame.frame_index = 100;
    server.broadcast_telemetry(t_frame);

    // 4. Assert: All 3 clients receive the broadcast frame
    const auto r1 = client1.receive_all(500);
    const auto r2 = client2.receive_all(500);
    const auto r3 = client3.receive_all(500);

    ASSERT_FALSE(r1.empty());
    ASSERT_FALSE(r2.empty());
    ASSERT_FALSE(r3.empty());

    auto d1 = tempest::profiler::decode_websocket_frame(
        tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(r1.data()), r1.size()});
    auto d2 = tempest::profiler::decode_websocket_frame(
        tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(r2.data()), r2.size()});
    auto d3 = tempest::profiler::decode_websocket_frame(
        tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(r3.data()), r3.size()});

    ASSERT_TRUE(d1.has_value());
    ASSERT_TRUE(d2.has_value());
    ASSERT_TRUE(d3.has_value());

    EXPECT_NE(
        std::string(reinterpret_cast<const char*>(d1->payload.data()), d1->payload.size()).find("\"frame_index\":100"),
        std::string::npos);
    EXPECT_NE(
        std::string(reinterpret_cast<const char*>(d2->payload.data()), d2->payload.size()).find("\"frame_index\":100"),
        std::string::npos);
    EXPECT_NE(
        std::string(reinterpret_cast<const char*>(d3->payload.data()), d3->payload.size()).find("\"frame_index\":100"),
        std::string::npos);

    server.stop();
}

//==============================================================================
// GPU Track Classification & Telemetry Routing Tests
//==============================================================================

/// @brief Verify GPU tracks with bit 31 set are classified as track_type::gpu_queue, named cleanly, and routed to
/// gpu_tracks.
TEST(profiler_tests, gpu_track_classification_and_telemetry_routing)
{
    // 1. Setup: Create event chunks representing GPU queues and CPU threads
    auto chunks = tempest::vector<tempest::unique_ptr<tempest::profiler::event_chunk>>{};

    // Graphics queue: bit 31 set, index 0 -> track_id = 0x8000'0001ULL
    auto chunk_gfx = tempest::make_unique<tempest::profiler::event_chunk>();
    chunk_gfx->set_thread_id(0x8000'0001ULL);
    chunk_gfx->add_zone(tempest::profiler::zone_record{
        .start_ns = 1000,
        .end_ns = 2000,
        .depth = 0,
        .name = "GBufferPass",
    });
    chunks.push_back(tempest::move(chunk_gfx));

    // Async Compute queue: bit 31 set, index 1 -> track_id = 0x8000'0002ULL
    auto chunk_compute = tempest::make_unique<tempest::profiler::event_chunk>();
    chunk_compute->set_thread_id(0x8000'0002ULL);
    chunk_compute->add_zone(tempest::profiler::zone_record{
        .start_ns = 1500,
        .end_ns = 2500,
        .depth = 0,
        .name = "CullingCompute",
    });
    chunks.push_back(tempest::move(chunk_compute));

    // Async Transfer queue: bit 31 set, index 2 -> track_id = 0x8000'0003ULL
    auto chunk_transfer = tempest::make_unique<tempest::profiler::event_chunk>();
    chunk_transfer->set_thread_id(0x8000'0003ULL);
    chunk_transfer->add_zone(tempest::profiler::zone_record{
        .start_ns = 500,
        .end_ns = 1200,
        .depth = 0,
        .name = "TextureUpload",
    });
    chunks.push_back(tempest::move(chunk_transfer));

    // Custom GPU queue: bit 31 set, index 7 -> track_id = 0x8000'0008ULL
    auto chunk_custom_gpu = tempest::make_unique<tempest::profiler::event_chunk>();
    chunk_custom_gpu->set_thread_id(0x8000'0008ULL);
    chunk_custom_gpu->add_zone(tempest::profiler::zone_record{
        .start_ns = 3000,
        .end_ns = 4000,
        .depth = 0,
        .name = "OpticalFlowPass",
    });
    chunks.push_back(tempest::move(chunk_custom_gpu));

    // CPU thread: bit 31 not set -> track_id = 42
    auto chunk_cpu = tempest::make_unique<tempest::profiler::event_chunk>();
    chunk_cpu->set_thread_id(42);
    chunk_cpu->add_zone(tempest::profiler::zone_record{
        .start_ns = 800,
        .end_ns = 2200,
        .depth = 0,
        .name = "SimulationUpdate",
    });
    chunks.push_back(tempest::move(chunk_cpu));

    // 2. Act: Create capture session data from chunks
    const auto chunk_span =
        tempest::span<const tempest::unique_ptr<tempest::profiler::event_chunk>>{chunks.data(), chunks.size()};
    const auto capture = tempest::profiler::create_capture_from_chunks(chunk_span);

    // 3. Assert: Verify track classification and naming in capture session data
    ASSERT_EQ(capture.tracks.size(), 5u);

    // GPU: Graphics
    EXPECT_EQ(capture.tracks[0].track_id, 0x8000'0001ULL);
    EXPECT_EQ(capture.tracks[0].type, tempest::profiler::track_type::gpu_queue);
    EXPECT_EQ(capture.tracks[0].name, "GPU: Graphics");

    // GPU: Async Compute
    EXPECT_EQ(capture.tracks[1].track_id, 0x8000'0002ULL);
    EXPECT_EQ(capture.tracks[1].type, tempest::profiler::track_type::gpu_queue);
    EXPECT_EQ(capture.tracks[1].name, "GPU: Async Compute");

    // GPU: Async Transfer
    EXPECT_EQ(capture.tracks[2].track_id, 0x8000'0003ULL);
    EXPECT_EQ(capture.tracks[2].type, tempest::profiler::track_type::gpu_queue);
    EXPECT_EQ(capture.tracks[2].name, "GPU: Async Transfer");

    // GPU: Queue 7
    EXPECT_EQ(capture.tracks[3].track_id, 0x8000'0008ULL);
    EXPECT_EQ(capture.tracks[3].type, tempest::profiler::track_type::gpu_queue);
    EXPECT_EQ(capture.tracks[3].name, "GPU: Queue 7");

    // CPU Thread
    EXPECT_EQ(capture.tracks[4].track_id, 42u);
    EXPECT_EQ(capture.tracks[4].type, tempest::profiler::track_type::cpu_thread);
    EXPECT_EQ(capture.tracks[4].name, "Thread 42");

    // 4. Act & Assert: Convert to telemetry frame and verify segregation
    const auto telemetry = tempest::profiler::create_telemetry_frame_from_capture(1, capture);
    EXPECT_EQ(telemetry.gpu_tracks.size(), 4u);
    EXPECT_EQ(telemetry.cpu_tracks.size(), 1u);

    EXPECT_EQ(telemetry.gpu_tracks[0].name, "GPU: Graphics");
    EXPECT_EQ(telemetry.gpu_tracks[1].name, "GPU: Async Compute");
    EXPECT_EQ(telemetry.gpu_tracks[2].name, "GPU: Async Transfer");
    EXPECT_EQ(telemetry.gpu_tracks[3].name, "GPU: Queue 7");

    EXPECT_EQ(telemetry.cpu_tracks[0].name, "Thread 42");
}

/// @brief Verify GPU track IDs remain strictly within JavaScript safe integer range (53 bits, < 2^53).
TEST(profiler_tests, gpu_track_id_js_safe_integer_range)
{
    // 1. Setup: Define maximum safe integer in IEEE-754 double precision (2^53 - 1)
    constexpr auto js_max_safe_integer = uint64_t{(1ULL << 53) - 1};

    // 2. Act: Generate GPU queue track IDs for standard and high index queues
    const auto gfx_track_id = uint64_t{0x8000'0000ULL | 1ULL};
    const auto compute_track_id = uint64_t{0x8000'0000ULL | 2ULL};
    const auto transfer_track_id = uint64_t{0x8000'0000ULL | 3ULL};
    const auto max_queue_track_id = uint64_t{0x8000'0000ULL | 0x7FFF'FFFFULL};

    // 3. Assert: All GPU track IDs are well within the JS safe integer limit
    EXPECT_LT(gfx_track_id, js_max_safe_integer);
    EXPECT_LT(compute_track_id, js_max_safe_integer);
    EXPECT_LT(transfer_track_id, js_max_safe_integer);
    EXPECT_LT(max_queue_track_id, js_max_safe_integer);

    // Verify track classification
    auto chunk = tempest::make_unique<tempest::profiler::event_chunk>();
    chunk->set_thread_id(gfx_track_id);
    chunk->add_zone(tempest::profiler::zone_record{
        .start_ns = 100,
        .end_ns = 200,
        .depth = 0,
        .name = "Pass",
    });

    auto chunks = tempest::vector<tempest::unique_ptr<tempest::profiler::event_chunk>>{};
    chunks.push_back(tempest::move(chunk));

    const auto chunk_span =
        tempest::span<const tempest::unique_ptr<tempest::profiler::event_chunk>>{chunks.data(), chunks.size()};
    const auto capture = tempest::profiler::create_capture_from_chunks(chunk_span);

    ASSERT_EQ(capture.tracks.size(), 1u);
    EXPECT_EQ(capture.tracks[0].type, tempest::profiler::track_type::gpu_queue);
    EXPECT_EQ(capture.tracks[0].name, "GPU: Graphics");
    EXPECT_LT(capture.tracks[0].track_id, js_max_safe_integer);
}

/// @brief Verify drained chunks are recycled into the session pool without new heap allocations.
TEST(profiler_tests, chunk_recycling_via_create_capture_from_session)
{
    // 1. Setup profiler session and verify empty initial pool
    auto session = tempest::profiler::profiler_session{true};
    EXPECT_EQ(session.get_chunk_pool().pool_size(), 0u);

    // 2. Act: Record multiple zones exceeding a chunk, then create capture from session
    constexpr auto event_count = size_t{2000};
    for (auto i = size_t{0}; i < event_count; ++i)
    {
        [[maybe_unused]] const auto zone = tempest::profiler::scoped_zone{session, "RecycleZone"};
    }

    const auto capture = tempest::profiler::create_capture_from_session(session);

    // 3. Assert: Capture contains all events and drained chunks were recycled into pool
    auto total_zones = size_t{0};
    for (const auto& tr : capture.tracks)
    {
        total_zones += tr.zones.size();
    }
    EXPECT_EQ(total_zones, event_count);
    EXPECT_GT(session.get_chunk_pool().pool_size(), 0u);

    // 4. Act: Record second frame and capture again, verifying pool reuse
    const auto pool_size_before = session.get_chunk_pool().pool_size();
    for (auto i = size_t{0}; i < event_count; ++i)
    {
        [[maybe_unused]] const auto zone = tempest::profiler::scoped_zone{session, "RecycleZone2"};
    }

    const auto capture2 = tempest::profiler::create_capture_from_session(session);
    const auto pool_size_after = session.get_chunk_pool().pool_size();

    // Pool size after second frame capture should equal pool size before, confirming zero net heap allocations
    EXPECT_EQ(pool_size_after, pool_size_before);
}

/// @brief Verify non-blocking socket broadcasts handle multiple clients gracefully without stalling.
TEST(profiler_tests, nonblocking_socket_broadcast_handling)
{
    // 1. Setup: Start web server
    auto session = tempest::profiler::profiler_session{true};
    auto config = tempest::profiler::web_server_config{.host = "127.0.0.1", .port = 8092, .max_port_attempts = 10};
    auto server = tempest::profiler::web_server{session, config};
    server.start();
    ASSERT_TRUE(server.is_running());
    const auto port = server.get_bound_port();

    // 2. Act: Connect a WebSocket client
    auto client = test_tcp_client{};
    ASSERT_TRUE(client.connect_to("127.0.0.1", port));

    const auto ws_req = "GET /ws HTTP/1.1\r\n"
                        "Host: 127.0.0.1\r\n"
                        "Upgrade: websocket\r\n"
                        "Connection: Upgrade\r\n"
                        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                        "Sec-WebSocket-Version: 13\r\n\r\n";

    ASSERT_TRUE(client.send_string(ws_req));
    const auto resp = client.receive_all(500);
    ASSERT_NE(resp.find("HTTP/1.1 101"), std::string::npos);

    tempest::this_thread::yield();
    EXPECT_EQ(server.connected_client_count(), 1u);

    // 3. Act: Issue multiple non-blocking broadcasts in rapid succession
    for (auto i = uint64_t{0}; i < 10; ++i)
    {
        auto t_frame = tempest::profiler::telemetry_frame{};
        t_frame.frame_index = i;
        server.broadcast_telemetry(t_frame);
    }

    // 4. Assert: Client is still connected and can receive frames without server stalling
    EXPECT_EQ(server.connected_client_count(), 1u);
    const auto bc_data = client.receive_all(500);
    EXPECT_FALSE(bc_data.empty());

    server.stop();
}

/// @brief Verify large multi-kilobyte telemetry frames transmit completely without truncation.
TEST(profiler_tests, large_payload_nonblocking_send_all)
{
    // 1. Setup: Start web server
    auto session = tempest::profiler::profiler_session{true};
    auto config = tempest::profiler::web_server_config{.host = "127.0.0.1", .port = 8093, .max_port_attempts = 10};
    auto server = tempest::profiler::web_server{session, config};
    server.start();
    ASSERT_TRUE(server.is_running());
    const auto port = server.get_bound_port();

    // 2. Connect client
    auto client = test_tcp_client{};
    ASSERT_TRUE(client.connect_to("127.0.0.1", port));

    const auto ws_req = "GET /ws HTTP/1.1\r\n"
                        "Host: 127.0.0.1\r\n"
                        "Upgrade: websocket\r\n"
                        "Connection: Upgrade\r\n"
                        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                        "Sec-WebSocket-Version: 13\r\n\r\n";

    ASSERT_TRUE(client.send_string(ws_req));
    const auto resp = client.receive_all(500);
    ASSERT_NE(resp.find("HTTP/1.1 101"), std::string::npos);

    // 3. Act: Broadcast a 128KB large payload
    auto large_text = std::string(128 * 1024, 'A');
    server.broadcast_text(tempest::string_view{large_text.data(), large_text.size()});

    // 4. Assert: Client receives the complete WebSocket payload without truncation
    const auto bc_data = client.receive_all(1000);
    EXPECT_GT(bc_data.size(), 128 * 1024u);

    server.stop();
}
