#include <gtest/gtest.h>

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
            auto ctx_desc = context_desc{};
            ctx_desc.application_name = "Tempest Execution Port Test";
            ctx_desc.api = graphics_api::vulkan;

            auto result = vk::create_context(ctx_desc);
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

    TEST(execution_port_test, get_execution_ports)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);

        auto& graphics_port = static_cast<vk::execution_port&>(env.dev->get_graphics_execution_port());
        EXPECT_NE(graphics_port.get_queue_family_index(), VK_QUEUE_FAMILY_IGNORED);

        auto& compute_port = static_cast<vk::execution_port&>(env.dev->get_async_compute_execution_port());
        EXPECT_NE(compute_port.get_queue_family_index(), VK_QUEUE_FAMILY_IGNORED);

        auto& transfer_port = static_cast<vk::execution_port&>(env.dev->get_async_transfer_execution_port());
        EXPECT_NE(transfer_port.get_queue_family_index(), VK_QUEUE_FAMILY_IGNORED);

        env.dev->wait_idle();
    }

    TEST(execution_port_test, acquire_and_record_commands)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);

        auto& graphics_port = env.dev->get_graphics_execution_port();
        auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);

        cmd.begin();
        cmd.pipeline_barrier({}, {});
        cmd.end();

        env.dev->wait_idle();
    }

    TEST(execution_port_test, submit_buffer_copy_and_sync)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto& dev = env.dev;

        constexpr size_t element_count = 256;
        constexpr size_t buffer_byte_size = element_count * sizeof(uint32_t);

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

        // Fill upload buffer
        auto* upload_ptr = static_cast<uint32_t*>(upload_buffer.cpu_address);
        for (size_t i = 0; i < element_count; ++i)
        {
            upload_ptr[i] = static_cast<uint32_t>(i * 1337 + 7);
        }

        // Initialize readback buffer to 0
        auto* readback_ptr = static_cast<uint32_t*>(readback_buffer.cpu_address);
        for (size_t i = 0; i < element_count; ++i)
        {
            readback_ptr[i] = 0;
        }

        // Record commands
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

        // Create timeline semaphore and submit
        auto timeline_sem = dev->create_timeline_semaphore();
        ASSERT_NE(timeline_sem.handle, 0ULL);

        const auto* cmd_ptr = &cmd;
        auto signal_sync = device_sync_point{
            .semaphore = timeline_sem,
            .value = 1,
            .stages = pipeline_stage::copy,
        };

        auto submit_result = graphics_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {},
                                                  span<const device_sync_point>{&signal_sync, 1});
        ASSERT_TRUE(submit_result.has_value());

        // Wait on host
        dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = 1});

        // Verify readback data
        for (size_t i = 0; i < element_count; ++i)
        {
            EXPECT_EQ(readback_ptr[i], static_cast<uint32_t>(i * 1337 + 7));
        }

        // Cleanup
        dev->destroy_semaphore(timeline_sem);
        dev->destroy_buffer(readback_buffer);
        dev->destroy_buffer(device_buffer);
        dev->destroy_buffer(upload_buffer);
    }

    TEST(execution_port_test, async_transfer_copy_and_sync)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto& dev = env.dev;

        constexpr size_t element_count = 128;
        constexpr size_t buffer_byte_size = element_count * sizeof(uint32_t);

        auto upload_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::transfer_src,
        };
        auto upload_buffer = dev->create_buffer(upload_desc);

        auto readback_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::readback,
            .usage = buffer_usage::transfer_dst,
        };
        auto readback_buffer = dev->create_buffer(readback_desc);

        auto* upload_ptr = static_cast<uint32_t*>(upload_buffer.cpu_address);
        for (size_t i = 0; i < element_count; ++i)
        {
            upload_ptr[i] = static_cast<uint32_t>(i * 31 + 101);
        }

        auto& transfer_port = dev->get_async_transfer_execution_port();
        auto& cmd = transfer_port.acquire_command_list(0, command_list_lifetime::transient);

        cmd.begin();
        auto copy_region = buffer_copy_region{
            .src_offset = 0,
            .dst_offset = 0,
            .size = buffer_byte_size,
        };
        cmd.copy_buffer(upload_buffer, readback_buffer, span<const buffer_copy_region>{&copy_region, 1});
        cmd.end();

        auto timeline_sem = dev->create_timeline_semaphore();
        const auto* cmd_ptr = &cmd;
        auto signal_sync = device_sync_point{
            .semaphore = timeline_sem,
            .value = 1,
            .stages = pipeline_stage::copy,
        };

        auto submit_result = transfer_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {},
                                                  span<const device_sync_point>{&signal_sync, 1});
        ASSERT_TRUE(submit_result.has_value());

        dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = 1});

        auto* readback_ptr = static_cast<uint32_t*>(readback_buffer.cpu_address);
        for (size_t i = 0; i < element_count; ++i)
        {
            EXPECT_EQ(readback_ptr[i], static_cast<uint32_t>(i * 31 + 101));
        }

        dev->destroy_semaphore(timeline_sem);
        dev->destroy_buffer(readback_buffer);
        dev->destroy_buffer(upload_buffer);
    }

    TEST(execution_port_test, multi_threaded_command_allocation)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto& dev = env.dev;

        constexpr size_t thread_count = 4;
        constexpr size_t element_count = 64;
        constexpr size_t buffer_byte_size = element_count * sizeof(uint32_t);

        auto threads = vector<thread>{};
        threads.reserve(thread_count);

        for (size_t t = 0; t < thread_count; ++t)
        {
            threads.emplace_back([t, &dev]() {
                auto upload_desc = buffer_desc{
                    .size = buffer_byte_size,
                    .memory_usage = memory_usage::upload,
                    .usage = buffer_usage::transfer_src,
                };
                auto upload_buffer = dev->create_buffer(upload_desc);

                auto readback_desc = buffer_desc{
                    .size = buffer_byte_size,
                    .memory_usage = memory_usage::readback,
                    .usage = buffer_usage::transfer_dst,
                };
                auto readback_buffer = dev->create_buffer(readback_desc);

                auto* upload_ptr = static_cast<uint32_t*>(upload_buffer.cpu_address);
                for (size_t i = 0; i < element_count; ++i)
                {
                    upload_ptr[i] = static_cast<uint32_t>((t + 1) * 1000 + i);
                }

                auto& graphics_port = dev->get_graphics_execution_port();
                auto& cmd =
                    graphics_port.acquire_command_list(static_cast<uint32_t>(t), command_list_lifetime::transient);

                cmd.begin();
                auto copy_region = buffer_copy_region{
                    .src_offset = 0,
                    .dst_offset = 0,
                    .size = buffer_byte_size,
                };
                cmd.copy_buffer(upload_buffer, readback_buffer, span<const buffer_copy_region>{&copy_region, 1});
                cmd.end();

                auto timeline_sem = dev->create_timeline_semaphore();
                const auto* cmd_ptr = &cmd;
                auto signal_sync = device_sync_point{
                    .semaphore = timeline_sem,
                    .value = 1,
                    .stages = pipeline_stage::copy,
                };

                auto submit_result = graphics_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {},
                                                          span<const device_sync_point>{&signal_sync, 1});
                EXPECT_TRUE(submit_result.has_value());

                dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = 1});

                auto* readback_ptr = static_cast<uint32_t*>(readback_buffer.cpu_address);
                for (size_t i = 0; i < element_count; ++i)
                {
                    EXPECT_EQ(readback_ptr[i], static_cast<uint32_t>((t + 1) * 1000 + i));
                }

                dev->destroy_semaphore(timeline_sem);
                dev->destroy_buffer(readback_buffer);
                dev->destroy_buffer(upload_buffer);
            });
        }

        for (auto& t : threads)
        {
            t.join();
        }
    }

    TEST(execution_port_test, slab_allocator_recycling)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto& dev = env.dev;

        constexpr size_t iterations = 32; // Exceeds single slab capacity of 16
        constexpr size_t element_count = 16;
        constexpr size_t buffer_byte_size = element_count * sizeof(uint32_t);

        auto upload_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::transfer_src,
        };
        auto upload_buffer = dev->create_buffer(upload_desc);

        auto readback_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::readback,
            .usage = buffer_usage::transfer_dst,
        };
        auto readback_buffer = dev->create_buffer(readback_desc);

        auto timeline_sem = dev->create_timeline_semaphore();
        auto& graphics_port = dev->get_graphics_execution_port();

        for (size_t iter = 1; iter <= iterations; ++iter)
        {
            auto* upload_ptr = static_cast<uint32_t*>(upload_buffer.cpu_address);
            for (size_t i = 0; i < element_count; ++i)
            {
                upload_ptr[i] = static_cast<uint32_t>(iter * 100 + i);
            }

            auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
            cmd.begin();
            auto copy_region = buffer_copy_region{
                .src_offset = 0,
                .dst_offset = 0,
                .size = buffer_byte_size,
            };
            cmd.copy_buffer(upload_buffer, readback_buffer, span<const buffer_copy_region>{&copy_region, 1});
            cmd.end();

            const auto* cmd_ptr = &cmd;
            auto signal_sync = device_sync_point{
                .semaphore = timeline_sem,
                .value = static_cast<uint64_t>(iter),
                .stages = pipeline_stage::copy,
            };

            auto submit_result = graphics_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {},
                                                      span<const device_sync_point>{&signal_sync, 1});
            ASSERT_TRUE(submit_result.has_value());

            // Synchronize every 8 iterations to allow slabs to recycle
            if (iter % 8 == 0)
            {
                dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = static_cast<uint64_t>(iter)});
            }
        }

        // Final wait
        dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = static_cast<uint64_t>(iterations)});

        auto* readback_ptr = static_cast<uint32_t*>(readback_buffer.cpu_address);
        for (size_t i = 0; i < element_count; ++i)
        {
            EXPECT_EQ(readback_ptr[i], static_cast<uint32_t>(iterations * 100 + i));
        }

        dev->destroy_semaphore(timeline_sem);
        dev->destroy_buffer(readback_buffer);
        dev->destroy_buffer(upload_buffer);
    }
} // namespace tempest::rhi::vk
