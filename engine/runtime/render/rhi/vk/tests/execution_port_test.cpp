#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/logger.hpp>
#include <tempest/mutex.hpp>
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
            ctx_desc.application_name = "Tempest Execution Port Test";
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

    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

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
            upload_ptr[i] = static_cast<uint32_t>((i * 1337) + 7);
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
            EXPECT_EQ(readback_ptr[i], static_cast<uint32_t>((i * 1337) + 7));
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
            upload_ptr[i] = static_cast<uint32_t>((i * 31) + 101);
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
            EXPECT_EQ(readback_ptr[i], static_cast<uint32_t>((i * 31) + 101));
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

        for (size_t thread_idx = 0; thread_idx < thread_count; ++thread_idx)
        {
            threads.emplace_back([thread_idx, &dev]() -> void {
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
                    upload_ptr[i] = static_cast<uint32_t>(((thread_idx + 1) * 1000) + i);
                }

                auto& graphics_port = dev->get_graphics_execution_port();
                auto& cmd = graphics_port.acquire_command_list(static_cast<uint32_t>(thread_idx),
                                                               command_list_lifetime::transient);

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
                    EXPECT_EQ(readback_ptr[i], static_cast<uint32_t>(((thread_idx + 1) * 1000) + i));
                }

                dev->destroy_semaphore(timeline_sem);
                dev->destroy_buffer(readback_buffer);
                dev->destroy_buffer(upload_buffer);
            });
        }

        for (auto& worker_thread : threads)
        {
            worker_thread.join();
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
                upload_ptr[i] = static_cast<uint32_t>((iter * 100) + i);
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
            EXPECT_EQ(readback_ptr[i], static_cast<uint32_t>(((iterations * 100)) + i));
        }

        dev->destroy_semaphore(timeline_sem);
        dev->destroy_buffer(readback_buffer);
        dev->destroy_buffer(upload_buffer);
    }

    TEST(execution_port_test, blit_texture_execution)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto& dev = env.dev;

        constexpr uint32_t src_w = 16;
        constexpr uint32_t src_h = 16;
        constexpr uint32_t dst_w = 32;
        constexpr uint32_t dst_h = 32;

        auto src_tex = dev->create_texture(texture_desc{
            .width = src_w,
            .height = src_h,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .format = data_format::rgba8_unorm,
            .memory_usage = memory_usage::device_only,
            .usage = texture_usage::transfer_src | texture_usage::transfer_dst,
            .name = "BlitSrcTexture",
        });

        auto dst_tex = dev->create_texture(texture_desc{
            .width = dst_w,
            .height = dst_h,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .format = data_format::rgba8_unorm,
            .memory_usage = memory_usage::device_only,
            .usage = texture_usage::transfer_src | texture_usage::transfer_dst,
            .name = "BlitDstTexture",
        });

        auto upload_buf = dev->create_buffer(buffer_desc{
            .size = static_cast<uint64_t>(src_w) * src_h * 4,
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::transfer_src,
            .name = "BlitUploadBuffer",
        });

        auto* upload_ptr = static_cast<uint8_t*>(upload_buf.cpu_address);
        for (size_t i = 0; i < static_cast<size_t>(src_w) * src_h * 4; i += 4)
        {
            upload_ptr[i + 0] = 200;
            upload_ptr[i + 1] = 100;
            upload_ptr[i + 2] = 50;
            upload_ptr[i + 3] = 255;
        }

        auto readback_buf = dev->create_buffer(buffer_desc{
            .size = static_cast<uint64_t>(dst_w) * dst_h * 4,
            .memory_usage = memory_usage::readback,
            .usage = buffer_usage::transfer_dst,
            .name = "BlitReadbackBuffer",
        });

        auto& graphics_port = dev->get_graphics_execution_port();
        auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);

        cmd.begin();

        auto init_barriers = array<texture_barrier, 2>{
            texture_barrier{
                .texture = src_tex,
                .src = {.stages = pipeline_stage::top_of_pipe,
                        .access = resource_access::none,
                        .layout = image_layout::undefined},
                .dst = {.stages = pipeline_stage::copy,
                        .access = resource_access::write,
                        .layout = image_layout::general},
                .base_mip_level = 0,
                .mip_level_count = 1,
                .base_array_layer = 0,
                .array_layer_count = 1,
            },
            texture_barrier{
                .texture = dst_tex,
                .src = {.stages = pipeline_stage::top_of_pipe,
                        .access = resource_access::none,
                        .layout = image_layout::undefined},
                .dst = {.stages = pipeline_stage::blit,
                        .access = resource_access::write,
                        .layout = image_layout::general},
                .base_mip_level = 0,
                .mip_level_count = 1,
                .base_array_layer = 0,
                .array_layer_count = 1,
            },
        };
        cmd.pipeline_barrier(init_barriers, {});

        auto copy_to_src = buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = src_w,
            .image_extent_height = src_h,
            .image_extent_depth = 1,
        };
        cmd.copy_buffer_to_texture(upload_buf, src_tex, span<const buffer_texture_copy_region>{&copy_to_src, 1});

        auto blit_barrier = texture_barrier{
            .texture = src_tex,
            .src = {.stages = pipeline_stage::copy, .access = resource_access::write, .layout = image_layout::general},
            .dst = {.stages = pipeline_stage::blit, .access = resource_access::read, .layout = image_layout::general},
            .base_mip_level = 0,
            .mip_level_count = 1,
            .base_array_layer = 0,
            .array_layer_count = 1,
        };
        cmd.pipeline_barrier(span<const texture_barrier>{&blit_barrier, 1}, {});

        auto blit_reg = texture_blit_region{
            .src_subresource = {.mip_level = 0, .base_array_layer = 0, .array_layer_count = 1},
            .src_offsets = {offset_3d{.x = 0, .y = 0, .z = 0},
                            offset_3d{.x = static_cast<int32_t>(src_w), .y = static_cast<int32_t>(src_h), .z = 1}},
            .dst_subresource = {.mip_level = 0, .base_array_layer = 0, .array_layer_count = 1},
            .dst_offsets = {offset_3d{.x = 0, .y = 0, .z = 0},
                            offset_3d{.x = static_cast<int32_t>(dst_w), .y = static_cast<int32_t>(dst_h), .z = 1}},
        };
        cmd.blit_texture(src_tex, dst_tex, span<const texture_blit_region>{&blit_reg, 1}, filter_mode::linear);

        auto readback_barrier = texture_barrier{
            .texture = dst_tex,
            .src = {.stages = pipeline_stage::blit, .access = resource_access::write, .layout = image_layout::general},
            .dst = {.stages = pipeline_stage::copy, .access = resource_access::read, .layout = image_layout::general},
            .base_mip_level = 0,
            .mip_level_count = 1,
            .base_array_layer = 0,
            .array_layer_count = 1,
        };
        cmd.pipeline_barrier(span<const texture_barrier>{&readback_barrier, 1}, {});

        auto copy_from_dst = buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = dst_w,
            .image_extent_height = dst_h,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(dst_tex, readback_buf, span<const buffer_texture_copy_region>{&copy_from_dst, 1});

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
        ASSERT_TRUE(submit_result.has_value());

        dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = 1});

        const auto* readback_ptr = static_cast<const uint8_t*>(readback_buf.cpu_address);
        ASSERT_NE(readback_ptr, nullptr);
        EXPECT_NEAR(readback_ptr[0], 200, 5);
        EXPECT_NEAR(readback_ptr[1], 100, 5);
        EXPECT_NEAR(readback_ptr[2], 50, 5);
        EXPECT_EQ(readback_ptr[3], 255);

        dev->destroy_semaphore(timeline_sem);
        dev->destroy_buffer(readback_buf);
        dev->destroy_buffer(upload_buf);
        dev->destroy_texture(dst_tex);
        dev->destroy_texture(src_tex);
    }

    /// @brief Verifies that clear_depth_attachment accurately resets depth values in a specified sub-rectangle
    ///        within an active dynamic render pass without affecting outside regions.
    TEST(execution_port_test, clear_depth_attachment_scissor)
    {
        // Setup: Create device, graphics queue, depth texture, view, and readback buffer
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto* dev = env.dev.get();
        auto& graphics_port = static_cast<vk::execution_port&>(dev->get_graphics_execution_port());

        constexpr auto width = 32U;
        constexpr auto height = 32U;

        const auto depth_tex = dev->create_texture(texture_desc{
            .width = width,
            .height = height,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .format = data_format::depth32_float,
            .usage = texture_usage::depth_stencil_attachment | texture_usage::transfer_src,
        });
        ASSERT_NE(depth_tex.handle, 0);

        const auto depth_view = dev->create_texture_view(depth_tex, texture_view_desc{});
        ASSERT_NE(depth_view.handle, 0);

        auto readback_buf = dev->create_buffer(buffer_desc{
            .size = static_cast<uint64_t>(width) * height * sizeof(float),
            .memory_usage = memory_usage::readback,
            .usage = buffer_usage::transfer_dst,
            .name = "DepthReadbackBuffer",
        });
        ASSERT_NE(readback_buf.handle, 0);

        // Act: Record pass with initial clear to 0.1F, then clear sub-rect [8, 8, 16, 16] to 0.85F
        auto& cmd = graphics_port.acquire_command_list();
        cmd.begin();

        const auto to_attach = texture_barrier{
            .texture = depth_tex,
            .src =
                {
                    .stages = pipeline_stage::top_of_pipe,
                    .access = resource_access::none,
                    .layout = image_layout::undefined,
                },
            .dst =
                {
                    .stages = pipeline_stage::attachment_output,
                    .access = resource_access::write,
                    .layout = image_layout::general,
                },
            .base_mip_level = 0,
            .mip_level_count = 1,
            .base_array_layer = 0,
            .array_layer_count = 1,
        };
        cmd.pipeline_barrier(span<const texture_barrier>{&to_attach, 1}, {});

        cmd.begin_render_pass(
            {},
            depth_stencil_attachment{
                .view = depth_view,
                .depth_load_op = load_op::clear,
                .depth_store_op = store_op::store,
                .clear_value = {.depth = 0.1F, .stencil = 0},
            },
            width, height);

        // Clear sub-rect [8, 8, 16, 16] to 0.85F
        cmd.clear_depth_attachment(8, 8, 16, 16, 0.85F);

        cmd.end_render_pass();

        const auto to_readback = texture_barrier{
            .texture = depth_tex,
            .src =
                {
                    .stages = pipeline_stage::attachment_output,
                    .access = resource_access::write,
                    .layout = image_layout::general,
                },
            .dst =
                {
                    .stages = pipeline_stage::all_transfer,
                    .access = resource_access::read,
                    .layout = image_layout::general,
                },
            .base_mip_level = 0,
            .mip_level_count = 1,
            .base_array_layer = 0,
            .array_layer_count = 1,
        };
        cmd.pipeline_barrier(span<const texture_barrier>{&to_readback, 1}, {});

        const auto copy_reg = buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = width,
            .buffer_image_height = height,
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = width,
            .image_extent_height = height,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(depth_tex, readback_buf, span<const buffer_texture_copy_region>{&copy_reg, 1});

        cmd.end();

        auto timeline_sem = dev->create_timeline_semaphore();
        const auto* cmd_ptr = &cmd;
        auto signal_sync = device_sync_point{
            .semaphore = timeline_sem,
            .value = 1,
            .stages = pipeline_stage::all_transfer,
        };

        auto submit_result = graphics_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {},
                                                  span<const device_sync_point>{&signal_sync, 1});
        ASSERT_TRUE(submit_result.has_value());

        dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = 1});

        // Assert: Sub-rect pixels are 0.85F, outside pixels remain 0.1F
        const auto* depths = static_cast<const float*>(readback_buf.cpu_address);
        ASSERT_NE(depths, nullptr);

        // Outside sub-rect: (0, 0)
        EXPECT_NEAR(depths[0], 0.1F, 0.001F);
        // Inside sub-rect: (12, 12) -> y=12, x=12 -> 12 * 32 + 12
        EXPECT_NEAR(depths[12 * width + 12], 0.85F, 0.001F);
        // Inside sub-rect: (8, 8)
        EXPECT_NEAR(depths[8 * width + 8], 0.85F, 0.001F);
        // Outside sub-rect: (31, 31)
        EXPECT_NEAR(depths[31 * width + 31], 0.1F, 0.001F);

        dev->destroy_semaphore(timeline_sem);
        dev->destroy_buffer(readback_buf);
        dev->destroy_texture_view(depth_view);
        dev->destroy_texture(depth_tex);
    }

    /// @brief Tests that execution ports sharing the same VkQueue handle reference the exact same submit mutex.
    TEST(execution_port_test, shared_queue_mutex_identity)
    {
        // Setup: Create test environment with Vulkan device
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);

        auto& graphics_port = static_cast<vk::execution_port&>(env.dev->get_graphics_execution_port());
        auto& compute_port = static_cast<vk::execution_port&>(env.dev->get_async_compute_execution_port());
        auto& transfer_port = static_cast<vk::execution_port&>(env.dev->get_async_transfer_execution_port());

        // Assert: If ports share the identical VkQueue handle, their submit mutex pointers must be identical
        if (graphics_port.get_queue() == compute_port.get_queue())
        {
            EXPECT_EQ(&graphics_port.get_submit_mutex(), &compute_port.get_submit_mutex());
        }
        if (graphics_port.get_queue() == transfer_port.get_queue())
        {
            EXPECT_EQ(&graphics_port.get_submit_mutex(), &transfer_port.get_submit_mutex());
        }
        if (compute_port.get_queue() == transfer_port.get_queue())
        {
            EXPECT_EQ(&compute_port.get_submit_mutex(), &transfer_port.get_submit_mutex());
        }
    }

    /// @brief Stress-tests concurrent command recording and submission across multiple execution ports sharing the same VkQueue.
    TEST(execution_port_test, shared_queue_multi_port_concurrent_submit_stress)
    {
        // Setup: Create test environment with Vulkan device
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto* const vk_dev = static_cast<vk::device*>(env.dev.get());

        auto& graphics_port = static_cast<vk::execution_port&>(env.dev->get_graphics_execution_port());
        const auto shared_queue = graphics_port.get_queue();
        const auto queue_family = graphics_port.get_queue_family_index();
        auto& shared_mutex = graphics_port.get_submit_mutex();

        // Construct two separate execution ports intentionally bound to the identical VkQueue and shared mutex
        auto port_alpha = execution_port{*vk_dev, queue_family, queue_type::compute, shared_queue,
                                         vk_dev->get_dispatch_table(), shared_mutex};
        auto port_beta = execution_port{*vk_dev, queue_family, queue_type::transfer, shared_queue,
                                        vk_dev->get_dispatch_table(), shared_mutex};

        constexpr size_t thread_count = 6;
        constexpr size_t iterations_per_thread = 20;

        auto threads = vector<thread>{};
        threads.reserve(thread_count);

        auto failed_submissions = atomic<uint32_t>{0};

        // Act: Spawn threads concurrently submitting to port_alpha and port_beta
        for (size_t thread_idx = 0; thread_idx < thread_count; ++thread_idx)
        {
            threads.emplace_back([thread_idx, &port_alpha, &port_beta, &failed_submissions]() -> void {
                auto& target_port = (thread_idx % 2 == 0) ? port_alpha : port_beta;

                for (size_t iter = 0; iter < iterations_per_thread; ++iter)
                {
                    auto& cmd = target_port.acquire_command_list(static_cast<uint32_t>(thread_idx),
                                                                 command_list_lifetime::transient);
                    cmd.begin();
                    cmd.end();

                    const auto* cmd_ptr = &cmd;
                    const auto submit_res = target_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {}, {});
                    if (!submit_res.has_value())
                    {
                        failed_submissions.fetch_add(1);
                    }
                }
            });
        }

        for (auto& worker : threads)
        {
            worker.join();
        }

        port_alpha.wait_idle();
        port_beta.wait_idle();

        // Assert: All submissions must succeed with 0 failures under multi-threaded contention
        EXPECT_EQ(failed_submissions.load(), 0U);
        EXPECT_GT(port_alpha.get_timeline_sync_point().value, 0U);
        EXPECT_GT(port_beta.get_timeline_sync_point().value, 0U);
    }

    namespace
    {
        struct submission_oracle_state
        {
            atomic<uint32_t> active_in_flight{0};
            atomic<uint32_t> max_concurrent{0};
            atomic<uint32_t> total_invocations{0};
            PFN_vkQueueSubmit2 forward_target{nullptr};
        };

        submission_oracle_state g_submit_oracle{};

        auto VKAPI_CALL oracle_mock_queue_submit2(
            VkQueue queue, uint32_t submit_count, const VkSubmitInfo2* p_submits, VkFence fence) -> VkResult
        {
            const auto current = ++g_submit_oracle.active_in_flight;
            auto prev_max = g_submit_oracle.max_concurrent.load();
            while (current > prev_max && !g_submit_oracle.max_concurrent.compare_exchange_weak(prev_max, current))
            {
            }

            // Artificial delay to expand the window of potential concurrent execution
            this_thread::sleep_for_nanoseconds(50'000);

            const auto result = g_submit_oracle.forward_target
                                    ? g_submit_oracle.forward_target(queue, submit_count, p_submits, fence)
                                    : VK_SUCCESS;

            --g_submit_oracle.active_in_flight;
            ++g_submit_oracle.total_invocations;
            return result;
        }
    } // namespace

    /// @brief Proves mutual exclusion empirically: verifies that multiple execution ports sharing the submit mutex
    /// strictly serialize calls to vkQueueSubmit2 (max_concurrent == 1), and that separate unshared mutexes fail.
    TEST(execution_port_test, shared_queue_mutual_exclusion_oracle)
    {
        // Setup: Create test environment with Vulkan device
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto* const vk_dev = static_cast<vk::device*>(env.dev.get());

        auto& graphics_port = static_cast<vk::execution_port&>(env.dev->get_graphics_execution_port());
        const auto real_queue = graphics_port.get_queue();

        auto custom_dispatch = vk_dev->get_dispatch_table();
        custom_dispatch.fp_vkQueueSubmit2 = oracle_mock_queue_submit2;

        auto shared_mutex = mutex{};
        auto port1 = execution_port{*vk_dev, 0, queue_type::graphics, real_queue, custom_dispatch, shared_mutex};
        auto port2 = execution_port{*vk_dev, 0, queue_type::compute, real_queue, custom_dispatch, shared_mutex};

        g_submit_oracle.active_in_flight.store(0);
        g_submit_oracle.max_concurrent.store(0);
        g_submit_oracle.total_invocations.store(0);
        g_submit_oracle.forward_target = vk_dev->get_dispatch_table().fp_vkQueueSubmit2;

        constexpr size_t thread_count = 6;
        constexpr size_t iterations_per_thread = 15;

        // Act (Part A - Shared Mutex): Multi-threaded concurrent submissions across ports sharing the mutex
        auto threads = vector<thread>{};
        threads.reserve(thread_count);

        for (size_t thread_idx = 0; thread_idx < thread_count; ++thread_idx)
        {
            threads.emplace_back([thread_idx, &port1, &port2]() -> void {
                auto& port = (thread_idx % 2 == 0) ? port1 : port2;
                for (size_t iter = 0; iter < iterations_per_thread; ++iter)
                {
                    auto& cmd = port.acquire_command_list(static_cast<uint32_t>(thread_idx),
                                                          command_list_lifetime::transient);
                    cmd.begin();
                    cmd.end();
                    const auto* cmd_ptr = &cmd;
                    const auto res = port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {}, {});
                    EXPECT_TRUE(res.has_value());
                }
            });
        }

        for (auto& worker : threads)
        {
            worker.join();
        }

        port1.wait_idle();
        port2.wait_idle();

        // Assert (Part A): Exactly 1 concurrent submission at all times (strict mutual exclusion)
        EXPECT_EQ(g_submit_oracle.total_invocations.load(), thread_count * iterations_per_thread);
        EXPECT_EQ(g_submit_oracle.max_concurrent.load(), 1U);

        // Act (Part B - Negative Control / Oracle Sensitivity): Run with independent unshared mutexes (mocked, no real driver forward)
        auto separate_mutex1 = mutex{};
        auto separate_mutex2 = mutex{};
        auto unshared_port1 = execution_port{*vk_dev, 0, queue_type::graphics, real_queue,
                                             custom_dispatch, separate_mutex1};
        auto unshared_port2 = execution_port{*vk_dev, 0, queue_type::compute, real_queue,
                                             custom_dispatch, separate_mutex2};

        g_submit_oracle.active_in_flight.store(0);
        g_submit_oracle.max_concurrent.store(0);
        g_submit_oracle.total_invocations.store(0);
        g_submit_oracle.forward_target = nullptr; // Do not forward to avoid corrupting real driver state

        auto unshared_threads = vector<thread>{};
        unshared_threads.reserve(thread_count);

        for (size_t thread_idx = 0; thread_idx < thread_count; ++thread_idx)
        {
            unshared_threads.emplace_back([thread_idx, &unshared_port1, &unshared_port2]() -> void {
                auto& port = (thread_idx % 2 == 0) ? unshared_port1 : unshared_port2;
                for (size_t iter = 0; iter < iterations_per_thread; ++iter)
                {
                    auto& cmd = port.acquire_command_list(static_cast<uint32_t>(thread_idx),
                                                          command_list_lifetime::transient);
                    cmd.begin();
                    cmd.end();
                    const auto* cmd_ptr = &cmd;
                    const auto res = port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {}, {});
                    EXPECT_TRUE(res.has_value());
                }
            });
        }

        for (auto& worker : unshared_threads)
        {
            worker.join();
        }

        // Assert (Part B): Unshared mutexes allow concurrent execution (max_concurrent > 1), proving oracle sensitivity
        EXPECT_GT(g_submit_oracle.max_concurrent.load(), 1U);
    }

    /// @brief Verifies edge-case safety for execution ports constructed with VK_NULL_HANDLE.
    TEST(execution_port_test, null_queue_execution_port_safety)
    {
        // Setup: Create test environment with Vulkan device
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto* const vk_dev = static_cast<vk::device*>(env.dev.get());

        auto dummy_mutex = mutex{};
        auto null_port = execution_port{*vk_dev, 0, queue_type::compute, VK_NULL_HANDLE,
                                        vk_dev->get_dispatch_table(), dummy_mutex};

        // Act & Assert: Operations on a null-queue port must not crash or cause access violations
        null_port.wait_idle();
        null_port.begin_debug_region(debug_label{.name = "test_null_region"});
        null_port.insert_debug_marker(debug_label{.name = "test_null_marker"});
        null_port.end_debug_region();

        const auto sync = null_port.get_timeline_sync_point();
        EXPECT_NE(sync.semaphore.handle, 0ULL);
        EXPECT_EQ(sync.value, 0ULL);
    }

    /// @brief Tests mutual exclusion between render_surface::present and execution_port::submit.
    TEST(execution_port_test, shared_queue_present_submit_mutual_exclusion)
    {
        // Setup: Create test environment with Vulkan device
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto& graphics_port = static_cast<vk::execution_port&>(env.dev->get_graphics_execution_port());
        auto& submit_mutex = graphics_port.get_submit_mutex();

        auto lock_acquired = atomic<bool>{false};
        auto present_blocked = atomic<bool>{true};
        auto thread_completed = atomic<bool>{false};

        // Act: Hold the submit mutex in the main thread (simulating an active submit)
        auto lock = unique_lock{submit_mutex};

        // Launch a worker thread that attempts to acquire the mutex (simulating render_surface::present)
        auto worker = thread{[&submit_mutex, &lock_acquired, &present_blocked, &thread_completed]() -> void {
            lock_acquired.store(true);
            {
                auto worker_lock = lock_guard{submit_mutex};
                present_blocked.store(false);
            }
            thread_completed.store(true);
        }};

        // Wait until worker thread starts and attempts acquisition
        while (!lock_acquired.load())
        {
            this_thread::yield();
        }

        // Delay 20ms to confirm worker remains blocked while lock is held
        this_thread::sleep_for_nanoseconds(20'000'000);

        // Assert: Worker must still be blocked
        EXPECT_TRUE(present_blocked.load());
        EXPECT_FALSE(thread_completed.load());

        // Release the mutex
        lock.unlock();

        worker.join();

        // Assert: Once released, worker acquires the mutex and completes
        EXPECT_FALSE(present_blocked.load());
        EXPECT_TRUE(thread_completed.load());
    }

    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
} // namespace tempest::rhi::vk
