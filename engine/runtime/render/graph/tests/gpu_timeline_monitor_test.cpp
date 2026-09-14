#include <tempest/chrono.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/logger.hpp>
#include <tempest/mutex.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/render_graph/gpu_sync_point.hpp>
#include <tempest/render_graph/gpu_timeline_monitor.hpp>
#include <tempest/rhi.hpp>
#include <tempest/thread.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace tempest;
    using namespace tempest::render_graph;

    class mock_timeline_device final : public rhi::device
    {
      public:
        uint64_t next_handle{1};
        rhi::device_desc desc{};
        atomic<bool> device_lost_flag{false};

        mutable mutex sem_mutex{};
        flat_unordered_map<uint64_t, uint64_t> semaphore_values{};

        auto wait_idle() -> void override
        {
        }

        auto wait_for_sync([[maybe_unused]] rhi::host_sync_point sync_point) -> void override
        {
        }

        [[nodiscard]] auto get_device_desc() const noexcept -> const rhi::device_desc& override
        {
            return desc;
        }

        [[nodiscard]] auto is_ray_tracing_supported() const -> bool override
        {
            return false;
        }

        [[nodiscard]] auto is_mesh_shading_supported() const -> bool override
        {
            return false;
        }

        [[nodiscard]] auto is_ray_query_supported() const -> bool override
        {
            return false;
        }

        [[nodiscard]] auto create_raw_surface([[maybe_unused]] rhi::native_wsi_handle native_window_handle)
            -> expected<rhi::raw_surface_handle, rhi::raw_surface_creation_error> override
        {
            return rhi::raw_surface_handle{.handle = next_handle++};
        }

        [[nodiscard]] auto get_surface_capabilities([[maybe_unused]] rhi::raw_surface_handle surface)
            -> rhi::surface_capabilities override
        {
            return {};
        }

        [[nodiscard]] auto create_render_surface([[maybe_unused]] const rhi::render_surface_desc& surface_desc)
            -> unique_ptr<rhi::render_surface> override
        {
            return nullptr;
        }

        auto destroy_render_surface([[maybe_unused]] unique_ptr<rhi::render_surface> surface) -> void override
        {
        }

        auto destroy_raw_surface([[maybe_unused]] rhi::raw_surface_handle surface) -> void override
        {
        }

        [[nodiscard]] auto get_semaphore_value(rhi::semaphore_handle semaphore) const -> uint64_t override
        {
            auto guard = lock_guard{sem_mutex};
            auto iter = semaphore_values.find(semaphore.handle);
            if (iter != semaphore_values.end())
            {
                return iter->second;
            }
            return 0;
        }

        auto signal_semaphore(rhi::semaphore_handle semaphore, uint64_t value) -> void override
        {
            auto guard = lock_guard{sem_mutex};
            semaphore_values[semaphore.handle] = value;
        }

        auto wait_semaphores(span<const rhi::host_sync_point> sync_points, uint64_t timeout_ns,
                             bool wait_any) -> rhi::wait_status override
        {
            const auto start = chrono::steady_clock::now();
            while (true)
            {
                if (device_lost_flag.load(memory_order::acquire))
                {
                    return rhi::wait_status::device_lost;
                }

                auto any_met = false;
                auto all_met = true;
                {
                    auto guard = lock_guard{sem_mutex};
                    for (const auto& sp : sync_points)
                    {
                        auto iter = semaphore_values.find(sp.semaphore.handle);
                        const auto current = (iter != semaphore_values.end()) ? iter->second : 0ULL;
                        if (current >= sp.value)
                        {
                            any_met = true;
                        }
                        else
                        {
                            all_met = false;
                        }
                    }
                }

                if (wait_any ? any_met : all_met)
                {
                    return rhi::wait_status::success;
                }

                const auto elapsed =
                    chrono::duration_cast<chrono::nanoseconds>(chrono::steady_clock::now() - start).count();
                if (static_cast<uint64_t>(elapsed) >= timeout_ns)
                {
                    return rhi::wait_status::timeout;
                }

                this_thread::sleep_for(chrono::microseconds(100));
            }
        }

        [[nodiscard]] auto get_graphics_execution_port() -> rhi::execution_port& override
        {
            return *reinterpret_cast<rhi::execution_port*>(this);
        }

        [[nodiscard]] auto get_async_compute_execution_port() -> rhi::execution_port& override
        {
            return *reinterpret_cast<rhi::execution_port*>(this);
        }

        [[nodiscard]] auto get_async_transfer_execution_port() -> rhi::execution_port& override
        {
            return *reinterpret_cast<rhi::execution_port*>(this);
        }

        [[nodiscard]] auto create_buffer([[maybe_unused]] const rhi::buffer_desc& buffer_desc)
            -> rhi::buffer_handle override
        {
            return rhi::buffer_handle{.handle = next_handle++};
        }

        [[nodiscard]] auto create_texture([[maybe_unused]] const rhi::texture_desc& texture_desc)
            -> rhi::texture_handle override
        {
            return rhi::texture_handle{.handle = next_handle++};
        }

        [[nodiscard]] auto create_texture_view([[maybe_unused]] rhi::texture_handle texture,
                                               [[maybe_unused]] const rhi::texture_view_desc& view_desc)
            -> rhi::texture_view_handle override
        {
            return rhi::texture_view_handle{.handle = next_handle++};
        }

        [[nodiscard]] auto create_sampler([[maybe_unused]] const rhi::sampler_desc& sampler_desc)
            -> rhi::sampler_handle override
        {
            return rhi::sampler_handle{.handle = next_handle++};
        }

        [[nodiscard]] auto create_graphics_pipeline([[maybe_unused]] const rhi::graphics_pipeline_desc& pipeline_desc)
            -> rhi::graphics_pipeline_handle override
        {
            return rhi::graphics_pipeline_handle{.handle = next_handle++};
        }

        [[nodiscard]] auto create_compute_pipeline([[maybe_unused]] const rhi::compute_pipeline_desc& pipeline_desc)
            -> rhi::compute_pipeline_handle override
        {
            return rhi::compute_pipeline_handle{.handle = next_handle++};
        }

        [[nodiscard]] auto create_event() -> rhi::event_handle override
        {
            return rhi::event_handle{.handle = next_handle++};
        }

        [[nodiscard]] auto create_timeline_semaphore() -> rhi::semaphore_handle override
        {
            auto guard = lock_guard{sem_mutex};
            const auto h = next_handle++;
            semaphore_values[h] = 0;
            return rhi::semaphore_handle{.handle = h};
        }

        [[nodiscard]] auto create_binary_semaphore() -> rhi::semaphore_handle override
        {
            return rhi::semaphore_handle{.handle = next_handle++};
        }

        [[nodiscard]] auto create_query_pool([[maybe_unused]] const rhi::query_pool_desc& pool_desc)
            -> rhi::query_pool_handle override
        {
            return rhi::query_pool_handle{.handle = next_handle++};
        }

        auto destroy_buffer([[maybe_unused]] rhi::buffer_handle buffer) -> void override
        {
        }

        auto destroy_texture([[maybe_unused]] rhi::texture_handle texture) -> void override
        {
        }

        auto destroy_texture_view([[maybe_unused]] rhi::texture_view_handle view) -> void override
        {
        }

        auto destroy_sampler([[maybe_unused]] rhi::sampler_handle sampler) -> void override
        {
        }

        auto destroy_graphics_pipeline([[maybe_unused]] rhi::graphics_pipeline_handle pipeline) -> void override
        {
        }

        auto destroy_compute_pipeline([[maybe_unused]] rhi::compute_pipeline_handle pipeline) -> void override
        {
        }

        auto destroy_event([[maybe_unused]] rhi::event_handle event) -> void override
        {
        }

        auto destroy_semaphore(rhi::semaphore_handle semaphore) -> void override
        {
            auto guard = lock_guard{sem_mutex};
            semaphore_values.erase(semaphore.handle);
        }

        auto destroy_query_pool([[maybe_unused]] rhi::query_pool_handle pool) -> void override
        {
        }

        [[nodiscard]] auto get_query_pool_results([[maybe_unused]] rhi::query_pool_handle pool,
                                                  [[maybe_unused]] uint32_t first_query,
                                                  [[maybe_unused]] uint32_t query_count,
                                                  [[maybe_unused]] span<uint64_t> results,
                                                  [[maybe_unused]] bool wait = false) -> bool override
        {
            return false;
        }

        [[nodiscard]] auto get_timestamp_period_ns() const noexcept -> float override
        {
            return 1.0f;
        }

        [[nodiscard]] auto get_calibrated_gpu_timestamp_offset_ns() const noexcept -> uint64_t override
        {
            return 0;
        }

        [[nodiscard]] auto convert_gpu_timestamp_to_cpu_ns(uint64_t gpu_ticks) const noexcept -> uint64_t override
        {
            return gpu_ticks;
        }

        [[nodiscard]] auto allocate_descriptor([[maybe_unused]] rhi::descriptor_type descriptor_type)
            -> rhi::descriptor_handle override
        {
            return {};
        }

        auto free_descriptor([[maybe_unused]] rhi::descriptor_type descriptor_type,
                             [[maybe_unused]] rhi::descriptor_handle descriptor) -> void override
        {
        }

        auto write_sampler_descriptor([[maybe_unused]] rhi::descriptor_handle slot,
                                      [[maybe_unused]] rhi::sampler_handle sampler) -> void override
        {
        }

        auto write_sampled_image_descriptor([[maybe_unused]] rhi::descriptor_handle slot,
                                            [[maybe_unused]] rhi::texture_view_handle view,
                                            [[maybe_unused]] rhi::image_layout layout = rhi::image_layout::general)
            -> void override
        {
        }

        auto write_storage_image_descriptor([[maybe_unused]] rhi::descriptor_handle slot,
                                            [[maybe_unused]] rhi::texture_view_handle view,
                                            [[maybe_unused]] rhi::image_layout layout = rhi::image_layout::general)
            -> void override
        {
        }

        auto set_debug_name([[maybe_unused]] rhi::buffer_handle handle, [[maybe_unused]] cstring_view name)
            -> void override
        {
        }

        auto set_debug_name([[maybe_unused]] rhi::texture_handle handle, [[maybe_unused]] cstring_view name)
            -> void override
        {
        }

        auto set_debug_name([[maybe_unused]] rhi::texture_view_handle handle, [[maybe_unused]] cstring_view name)
            -> void override
        {
        }

        auto set_debug_name([[maybe_unused]] rhi::sampler_handle handle, [[maybe_unused]] cstring_view name)
            -> void override
        {
        }

        auto set_debug_name([[maybe_unused]] rhi::graphics_pipeline_handle handle, [[maybe_unused]] cstring_view name)
            -> void override
        {
        }

        auto set_debug_name([[maybe_unused]] rhi::compute_pipeline_handle handle, [[maybe_unused]] cstring_view name)
            -> void override
        {
        }

        auto set_debug_name([[maybe_unused]] rhi::event_handle handle, [[maybe_unused]] cstring_view name)
            -> void override
        {
        }

        auto set_debug_name([[maybe_unused]] rhi::semaphore_handle handle, [[maybe_unused]] cstring_view name)
            -> void override
        {
        }

        auto set_debug_name([[maybe_unused]] rhi::query_pool_handle handle, [[maybe_unused]] cstring_view name)
            -> void override
        {
        }
    };
} // namespace

// ============================================================================
// Section: GPU Timeline Monitor Tests
// ============================================================================

/// @brief Verify that awaiting a timeline semaphore whose milestone is already reached
/// takes the synchronous fast-path (await_ready returns true) without suspending.
TEST(gpu_timeline_monitor_test, immediate_fast_path_already_signaled)
{
    // 1. Setup
    auto dev = mock_timeline_device{};
    auto log = logger{};
    auto prof = profiler::profiler_session{false};
    auto config = job::job_system_config{
        .performance_worker_count = 1,
        .efficiency_worker_count = 0,
    };
    auto js = job::job_system{log, prof, config};
    auto monitor = gpu_timeline_monitor{dev, js, log};

    const auto sem = dev.create_timeline_semaphore();
    dev.signal_semaphore(sem, 100);

    // 2. Act
    auto executed = atomic<bool>{false};
    auto t = js.async([&]() -> job::task<void> {
        auto wait_res = co_await monitor.wait(rhi::host_sync_point{.semaphore = sem, .value = 100});
        if (wait_res)
        {
            executed.store(true, memory_order::release);
        }
        co_return;
    });

    js.wait_idle();

    // 3. Assert
    EXPECT_TRUE(executed.load(memory_order::acquire));
}

/// @brief Verify that awaiting an invalid semaphore handle immediately produces
/// gpu_sync_error::invalid_semaphore.
TEST(gpu_timeline_monitor_test, invalid_semaphore_error)
{
    // 1. Setup
    auto dev = mock_timeline_device{};
    auto log = logger{};
    auto prof = profiler::profiler_session{false};
    auto config = job::job_system_config{
        .performance_worker_count = 1,
        .efficiency_worker_count = 0,
    };
    auto js = job::job_system{log, prof, config};
    auto monitor = gpu_timeline_monitor{dev, js, log};

    // 2. Act
    auto received_error = atomic<gpu_sync_error>{gpu_sync_error::none};
    auto t = js.async([&]() -> job::task<void> {
        auto wait_res = co_await monitor.wait(rhi::host_sync_point{.semaphore = rhi::semaphore_handle{0}, .value = 1});
        if (!wait_res)
        {
            received_error.store(wait_res.error(), memory_order::release);
        }
        co_return;
    });

    js.wait_idle();

    // 3. Assert
    EXPECT_EQ(received_error.load(memory_order::acquire), gpu_sync_error::invalid_semaphore);
}

/// @brief Verify that a suspended coroutine awaiting a future GPU milestone
/// wakes up and resumes when the semaphore is signaled by another thread.
TEST(gpu_timeline_monitor_test, cross_thread_gpu_signal_wakeup)
{
    // 1. Setup
    auto dev = mock_timeline_device{};
    auto log = logger{};
    auto prof = profiler::profiler_session{false};
    auto config = job::job_system_config{
        .performance_worker_count = 2,
        .efficiency_worker_count = 0,
    };
    auto js = job::job_system{log, prof, config};
    auto monitor = gpu_timeline_monitor{dev, js, log};

    const auto sem = dev.create_timeline_semaphore();
    auto completed = atomic<bool>{false};

    // 2. Act: launch coroutine awaiting milestone 50
    auto t = js.async([&]() -> job::task<void> {
        auto wait_res = co_await monitor.wait(rhi::host_sync_point{.semaphore = sem, .value = 50});
        if (wait_res)
        {
            completed.store(true, memory_order::release);
        }
        co_return;
    });

    // Coroutine is suspended waiting for milestone 50
    this_thread::sleep_for(chrono::milliseconds(5));
    EXPECT_FALSE(completed.load(memory_order::acquire));

    // Signal semaphore from separate thread
    auto signaler = thread{[&]() {
        this_thread::sleep_for(chrono::milliseconds(10));
        dev.signal_semaphore(sem, 50);
    }};
    signaler.join();

    // Wait for coroutine completion
    const auto deadline = chrono::steady_clock::now() + chrono::seconds(2);
    while (!completed.load(memory_order::acquire) && chrono::steady_clock::now() < deadline)
    {
        this_thread::sleep_for(chrono::milliseconds(1));
    }

    js.wait_idle();

    // 3. Assert
    EXPECT_TRUE(completed.load(memory_order::acquire));
}

/// @brief Verify multiple coroutines awaiting differing milestone values on the same semaphore
/// complete in their correct sequence as milestones are signaled.
TEST(gpu_timeline_monitor_test, multiple_waiters_single_semaphore)
{
    // 1. Setup
    auto dev = mock_timeline_device{};
    auto log = logger{};
    auto prof = profiler::profiler_session{false};
    auto config = job::job_system_config{
        .performance_worker_count = 2,
        .efficiency_worker_count = 0,
    };
    auto js = job::job_system{log, prof, config};
    auto monitor = gpu_timeline_monitor{dev, js, log};

    const auto sem = dev.create_timeline_semaphore();
    auto coro1_done = atomic<bool>{false};
    auto coro2_done = atomic<bool>{false};

    // 2. Act: Coroutine 1 waits for 10, Coroutine 2 waits for 20
    auto t1 = js.async([&]() -> job::task<void> {
        auto wait_res = co_await monitor.wait(rhi::host_sync_point{.semaphore = sem, .value = 10});
        if (wait_res)
        {
            coro1_done.store(true, memory_order::release);
        }
        co_return;
    });

    auto t2 = js.async([&]() -> job::task<void> {
        auto wait_res = co_await monitor.wait(rhi::host_sync_point{.semaphore = sem, .value = 20});
        if (wait_res)
        {
            coro2_done.store(true, memory_order::release);
        }
        co_return;
    });

    this_thread::sleep_for(chrono::milliseconds(5));
    EXPECT_FALSE(coro1_done.load(memory_order::acquire));
    EXPECT_FALSE(coro2_done.load(memory_order::acquire));

    // Signal milestone 10
    dev.signal_semaphore(sem, 10);

    const auto d1 = chrono::steady_clock::now() + chrono::seconds(2);
    while (!coro1_done.load(memory_order::acquire) && chrono::steady_clock::now() < d1)
    {
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    EXPECT_TRUE(coro1_done.load(memory_order::acquire));
    EXPECT_FALSE(coro2_done.load(memory_order::acquire));

    // Signal milestone 20
    dev.signal_semaphore(sem, 20);

    const auto d2 = chrono::steady_clock::now() + chrono::seconds(2);
    while (!coro2_done.load(memory_order::acquire) && chrono::steady_clock::now() < d2)
    {
        this_thread::sleep_for(chrono::milliseconds(1));
    }

    js.wait_idle();

    // 3. Assert
    EXPECT_TRUE(coro2_done.load(memory_order::acquire));
}

/// @brief Verify that when the monitor is stopped/destructed, any unresolved awaiting coroutines
/// are resumed with gpu_sync_error::cancelled rather than being leaked.
TEST(gpu_timeline_monitor_test, monitor_destruction_cancellation)
{
    // 1. Setup
    auto dev = mock_timeline_device{};
    auto log = logger{};
    auto prof = profiler::profiler_session{false};
    auto config = job::job_system_config{
        .performance_worker_count = 2,
        .efficiency_worker_count = 0,
    };
    auto js = job::job_system{log, prof, config};

    const auto sem = dev.create_timeline_semaphore();
    auto error_received = atomic<gpu_sync_error>{gpu_sync_error::none};
    auto done = atomic<bool>{false};

    // 2. Act
    auto t = job::task<void>{};
    {
        auto monitor = gpu_timeline_monitor{dev, js, log};

        t = js.async([&]() -> job::task<void> {
            auto wait_res = co_await monitor.wait(rhi::host_sync_point{.semaphore = sem, .value = 999});
            if (!wait_res)
            {
                error_received.store(wait_res.error(), memory_order::release);
            }
            done.store(true, memory_order::release);
            co_return;
        });

        this_thread::sleep_for(chrono::milliseconds(10));
        EXPECT_FALSE(done.load(memory_order::acquire));

        // Monitor destroyed here
    }

    const auto deadline = chrono::steady_clock::now() + chrono::seconds(2);
    while (!done.load(memory_order::acquire) && chrono::steady_clock::now() < deadline)
    {
        this_thread::sleep_for(chrono::milliseconds(1));
    }

    js.wait_idle();

    // 3. Assert
    EXPECT_TRUE(done.load(memory_order::acquire));
    EXPECT_EQ(error_received.load(memory_order::acquire), gpu_sync_error::cancelled);
}

/// @brief Verify that device lost status during semaphore wait cancels pending coroutines
/// with gpu_sync_error::device_lost.
TEST(gpu_timeline_monitor_test, device_lost_cancellation)
{
    // 1. Setup
    auto dev = mock_timeline_device{};
    auto log = logger{};
    auto prof = profiler::profiler_session{false};
    auto config = job::job_system_config{
        .performance_worker_count = 2,
        .efficiency_worker_count = 0,
    };
    auto js = job::job_system{log, prof, config};
    auto monitor = gpu_timeline_monitor{dev, js, log};

    const auto sem = dev.create_timeline_semaphore();
    auto error_received = atomic<gpu_sync_error>{gpu_sync_error::none};
    auto done = atomic<bool>{false};

    // 2. Act
    auto t = js.async([&]() -> job::task<void> {
        auto wait_res = co_await monitor.wait(rhi::host_sync_point{.semaphore = sem, .value = 500});
        if (!wait_res)
        {
            error_received.store(wait_res.error(), memory_order::release);
        }
        done.store(true, memory_order::release);
        co_return;
    });

    this_thread::sleep_for(chrono::milliseconds(5));
    EXPECT_FALSE(done.load(memory_order::acquire));

    // Trigger device loss
    dev.device_lost_flag.store(true, memory_order::release);

    const auto deadline = chrono::steady_clock::now() + chrono::seconds(2);
    while (!done.load(memory_order::acquire) && chrono::steady_clock::now() < deadline)
    {
        this_thread::sleep_for(chrono::milliseconds(1));
    }

    js.wait_idle();

    // 3. Assert
    EXPECT_TRUE(done.load(memory_order::acquire));
    EXPECT_EQ(error_received.load(memory_order::acquire), gpu_sync_error::device_lost);
}

/// @brief Verify that dozens of concurrent coroutines can push wait requests into the
/// lock-free intrusive Treiber stack simultaneously across multiple worker threads,
/// and resume successfully when their GPU milestone is signaled.
TEST(gpu_timeline_monitor_test, high_contention_concurrent_waiters)
{
    // 1. Setup
    auto dev = mock_timeline_device{};
    auto log = logger{};
    auto prof = profiler::profiler_session{false};
    auto config = job::job_system_config{
        .performance_worker_count = 4,
        .efficiency_worker_count = 0,
    };
    auto js = job::job_system{log, prof, config};
    auto monitor = gpu_timeline_monitor{dev, js, log};

    const auto sem = dev.create_timeline_semaphore();
    constexpr auto waiter_count = uint32_t{48};
    auto completion_count = atomic<uint32_t>{0};

    // 2. Act: launch concurrent coroutines awaiting milestone 10
    auto tasks = vector<job::task<void>>{};
    tasks.reserve(waiter_count);

    for (auto index = uint32_t{0}; index < waiter_count; ++index)
    {
        tasks.push_back(js.async([&]() -> job::task<void> {
            auto wait_res = co_await monitor.wait(rhi::host_sync_point{.semaphore = sem, .value = 10});
            if (wait_res)
            {
                completion_count.fetch_add(1, memory_order::release);
            }
            co_return;
        }));
    }

    // Wait until workers have all had an opportunity to suspend
    this_thread::sleep_for(chrono::milliseconds(20));
    EXPECT_EQ(completion_count.load(memory_order::acquire), 0U);

    // Signal milestone 10 to wake all waiters concurrently
    dev.signal_semaphore(sem, 10);

    const auto deadline = chrono::steady_clock::now() + chrono::seconds(3);
    while (completion_count.load(memory_order::acquire) < waiter_count && chrono::steady_clock::now() < deadline)
    {
        this_thread::sleep_for(chrono::milliseconds(1));
    }

    js.wait_idle();

    // 3. Assert
    EXPECT_EQ(completion_count.load(memory_order::acquire), waiter_count);
}

/// @brief Verify that dozens of concurrent waiters registered via the intrusive stack
/// are all safely cancelled and resumed when the monitor is explicitly stopped.
TEST(gpu_timeline_monitor_test, concurrent_waiters_cancelled_on_stop)
{
    // 1. Setup
    auto dev = mock_timeline_device{};
    auto log = logger{};
    auto prof = profiler::profiler_session{false};
    auto config = job::job_system_config{
        .performance_worker_count = 4,
        .efficiency_worker_count = 0,
    };
    auto js = job::job_system{log, prof, config};
    auto monitor = gpu_timeline_monitor{dev, js, log};

    const auto sem = dev.create_timeline_semaphore();
    constexpr auto waiter_count = uint32_t{48};
    auto cancel_count = atomic<uint32_t>{0};

    // 2. Act: launch coroutines awaiting an unreachable milestone
    auto tasks = vector<job::task<void>>{};
    tasks.reserve(waiter_count);

    for (auto index = uint32_t{0}; index < waiter_count; ++index)
    {
        tasks.push_back(js.async([&]() -> job::task<void> {
            auto wait_res = co_await monitor.wait(rhi::host_sync_point{.semaphore = sem, .value = 1000});
            if (!wait_res && wait_res.error() == gpu_sync_error::cancelled)
            {
                cancel_count.fetch_add(1, memory_order::release);
            }
            co_return;
        }));
    }

    // Give time for coroutines to enqueue in the Treiber stack
    this_thread::sleep_for(chrono::milliseconds(20));
    EXPECT_EQ(cancel_count.load(memory_order::acquire), 0U);

    // Stop monitor: should cancel all pending and active entries
    monitor.stop();

    const auto deadline = chrono::steady_clock::now() + chrono::seconds(3);
    while (cancel_count.load(memory_order::acquire) < waiter_count && chrono::steady_clock::now() < deadline)
    {
        this_thread::sleep_for(chrono::milliseconds(1));
    }

    js.wait_idle();

    // 3. Assert
    EXPECT_EQ(cancel_count.load(memory_order::acquire), waiter_count);
}

/// @brief Verify that destroying a coroutine frame while suspended waiting on a GPU sync point
/// safely marks the embedded entry as cancelled and prevents UAF during monitor loop execution.
TEST(gpu_timeline_monitor_test, frame_destruction_cancels_waiter)
{
    // 1. Setup
    auto dev = mock_timeline_device{};
    auto log = logger{};
    auto prof = profiler::profiler_session{false};
    auto config = job::job_system_config{
        .performance_worker_count = 2,
        .efficiency_worker_count = 0,
    };
    auto js = job::job_system{log, prof, config};
    auto monitor = gpu_timeline_monitor{dev, js, log};

    const auto sem = dev.create_timeline_semaphore();

    // 2. Act: Spawn a task that suspends on monitor.wait(), then destroy the task object
    {
        auto coro = [&]() -> job::task<void> {
            co_await monitor.wait(rhi::host_sync_point{.semaphore = sem, .value = 5});
            co_return;
        };

        auto t = coro();
        t.resume(); // Suspends inside monitor.wait()
        // t goes out of scope and is destroyed here, invoking ~gpu_sync_point()
    }

    // Advance timeline semaphore past the wait threshold
    dev.signal_semaphore(sem, 10);

    // Give time for monitor to loop and observe completion of the now-cancelled entry
    this_thread::sleep_for(chrono::milliseconds(30));

    // 3. Assert: Monitor loop did not crash or dereference freed frame memory
    SUCCEED();
}

