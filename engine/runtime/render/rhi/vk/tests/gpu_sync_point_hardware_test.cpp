#include <gtest/gtest.h>

#include <tempest/chrono.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/logger.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/render_graph/gpu_sync_point.hpp>
#include <tempest/render_graph/gpu_timeline_monitor.hpp>
#include <tempest/rhi.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>
#include <tempest/vk/context.hpp>
#include <tempest/vk/device.hpp>
#include <tempest/vk/execution_port.hpp>

namespace tempest::rhi::vk
{
    namespace
    {
        struct test_env
        {
            unique_ptr<rhi::context> context;
            unique_ptr<rhi::device> dev;
        };

        auto create_test_env() -> test_env
        {
            static auto test_sink = stdout_log_sink{};
            static auto test_log = logger{test_sink};

            auto ctx_desc = context_desc{};
            ctx_desc.application_name = "Tempest GPU Sync Point Hardware Test";
            ctx_desc.api = graphics_api::vulkan;

            auto result = vk::create_context(ctx_desc, test_log);
            if (!result.has_value())
            {
                return {};
            }

            auto context = tempest::move(result).value();
            auto devices = context->enumerate_devices();
            if (devices.empty())
            {
                return {};
            }

            auto dev = context->create_device(devices[0].device_uuid);
            return test_env{
                .context = tempest::move(context),
                .dev = tempest::move(dev),
            };
        }
    } // namespace

    // ============================================================================
    // Section: Hardware GPU-CPU Synchronization Bridge Tests
    // ============================================================================

    /// @brief Verify that a CPU coroutine can suspend on a Vulkan hardware timeline semaphore,
    /// and wake up with zero-CPU-spin latency when GPU hardware finishes execution, validating
    /// GPU buffer copy contents on the host.
    TEST(gpu_sync_point_hardware_test, coroutine_awaits_hardware_buffer_copy)
    {
        // 1. Setup
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto& dev = env.dev;

        auto test_sink = stdout_log_sink{};
        auto log = logger{test_sink};
        auto prof = profiler::profiler_session{false};
        auto config = job::job_system_config{
            .performance_worker_count = 2,
            .efficiency_worker_count = 0,
        };
        auto js = job::job_system{log, prof, config};
        auto monitor = render_graph::gpu_timeline_monitor{*dev, js, log};

        constexpr auto element_count = size_t{256};
        constexpr auto buffer_byte_size = element_count * sizeof(uint32_t);

        auto upload_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::transfer_src,
        };
        auto upload_buffer = dev->create_buffer(upload_desc);
        ASSERT_NE(upload_buffer.handle, 0ULL);
        ASSERT_NE(upload_buffer.cpu_address, nullptr);

        auto device_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::device_only,
            .usage = buffer_usage::transfer_src | buffer_usage::transfer_dst,
        };
        auto device_buffer = dev->create_buffer(device_desc);
        ASSERT_NE(device_buffer.handle, 0ULL);

        auto readback_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::readback,
            .usage = buffer_usage::transfer_dst,
        };
        auto readback_buffer = dev->create_buffer(readback_desc);
        ASSERT_NE(readback_buffer.handle, 0ULL);
        ASSERT_NE(readback_buffer.cpu_address, nullptr);

        // Fill upload buffer with deterministic pattern
        auto* upload_ptr = static_cast<uint32_t*>(upload_buffer.cpu_address);
        for (auto i = size_t{0}; i < element_count; ++i)
        {
            upload_ptr[i] = static_cast<uint32_t>((i * 4242) + 13);
        }

        auto* readback_ptr = static_cast<uint32_t*>(readback_buffer.cpu_address);
        for (auto i = size_t{0}; i < element_count; ++i)
        {
            readback_ptr[i] = 0;
        }

        // Record GPU copy commands (upload -> device -> readback)
        auto& graphics_port = dev->get_graphics_execution_port();
        auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);

        cmd.begin();

        auto copy_region = buffer_copy_region{
            .src_offset = 0,
            .dst_offset = 0,
            .size = buffer_byte_size,
        };
        cmd.copy_buffer(upload_buffer, device_buffer, span<const buffer_copy_region>{&copy_region, 1});

        auto barrier = buffer_barrier{
            .buffer = device_buffer,
            .src =
                {
                    .stages = pipeline_stage::copy,
                    .access = resource_access::write,
                },
            .dst =
                {
                    .stages = pipeline_stage::copy,
                    .access = resource_access::read,
                },
            .offset = 0,
            .size = buffer_byte_size,
        };
        cmd.pipeline_barrier({}, span<const buffer_barrier>{&barrier, 1});

        cmd.copy_buffer(device_buffer, readback_buffer, span<const buffer_copy_region>{&copy_region, 1});
        cmd.end();

        // Create GPU timeline semaphore and submit
        auto timeline_sem = dev->create_timeline_semaphore();
        ASSERT_NE(timeline_sem.handle, 0ULL);

        const auto target_value = uint64_t{100};
        const auto* cmd_ptr = &cmd;
        auto signal_sync = device_sync_point{
            .semaphore = timeline_sem,
            .value = target_value,
            .stages = pipeline_stage::copy,
        };

        // 2. Act
        auto completed = atomic<bool>{false};
        auto sync_success = atomic<bool>{false};

        // Submit GPU work to graphics port
        auto submit_res = graphics_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {},
                                               span<const device_sync_point>{&signal_sync, 1});
        ASSERT_TRUE(submit_res.has_value());

        // Coroutine awaits timeline semaphore milestone 100 via monitor
        auto t = js.async([&]() -> job::task<void> {
            auto wait_res = co_await monitor.wait(host_sync_point{
                .semaphore = timeline_sem,
                .value = target_value,
            });
            if (wait_res)
            {
                sync_success.store(true, memory_order::release);
            }
            completed.store(true, memory_order::release);
            co_return;
        });

        // Wait for coroutine completion
        const auto deadline = chrono::steady_clock::now() + chrono::seconds(5);
        while (!completed.load(memory_order::acquire) && chrono::steady_clock::now() < deadline)
        {
            this_thread::sleep_for(chrono::milliseconds(1));
        }

        // 3. Assert
        EXPECT_TRUE(completed.load(memory_order::acquire));
        EXPECT_TRUE(sync_success.load(memory_order::acquire));

        // Validate that readback buffer received data from upload buffer via GPU transfer
        for (auto i = size_t{0}; i < element_count; ++i)
        {
            EXPECT_EQ(readback_ptr[i], upload_ptr[i]);
        }

        // Cleanup
        dev->wait_idle();
        dev->destroy_buffer(upload_buffer);
        dev->destroy_buffer(device_buffer);
        dev->destroy_buffer(readback_buffer);
        dev->destroy_semaphore(timeline_sem);
    }
} // namespace tempest::rhi::vk
