#include <gtest/gtest.h>

#include <tempest/array.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>
#include <tempest/vk/calibration.hpp>
#include <tempest/vk/context.hpp>
#include <tempest/vk/device.hpp>
#include <tempest/vk/execution_port.hpp>

namespace shaders::compute
{
#include <test_compute.comp.h>
} // namespace shaders::compute

namespace shaders::raster
{
    namespace vs
    {
#include <test_raster.vert.h>
    } // namespace vs

    namespace fs
    {
#include <test_raster.frag.h>
    } // namespace fs
} // namespace shaders::raster

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
            ctx_desc.application_name = "Tempest Query Test";
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

        struct raster_push_constants
        {
            float r;
            float g;
            float b;
            float a;
        };
    } // namespace

    // =========================================================================
    // GPU Queries & Timestamp Monotonicity Tests
    // =========================================================================

    /// @brief Verify creation, reset, timestamp writing, and monotonic result readback.
    TEST(query_test, timestamp_query_lifecycle_and_monotonicity)
    {
        // 1. Setup test environment, query pool, and synchronization primitives
        const auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        const auto& dev = env.dev;

        const auto qp_desc = query_pool_desc{
            .type = query_type::timestamp,
            .query_count = 4,
            .pipeline_statistics = pipeline_statistic_flags::none,
            .name = "Test Timestamp Pool",
        };
        const auto pool = dev->create_query_pool(qp_desc);
        ASSERT_NE(pool.handle, 0ULL);

        const auto timeline_sem = dev->create_timeline_semaphore();
        ASSERT_NE(timeline_sem.handle, 0ULL);

        constexpr size_t buffer_size = 1024;
        const auto upload_desc = buffer_desc{
            .size = buffer_size,
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::transfer_src,
            .name = "Upload Buffer",
        };
        const auto upload_buf = dev->create_buffer(upload_desc);
        ASSERT_NE(upload_buf.handle, 0ULL);

        const auto dst_desc = buffer_desc{
            .size = buffer_size,
            .memory_usage = memory_usage::device_only,
            .usage = buffer_usage::transfer_dst | buffer_usage::transfer_src,
            .name = "Device Buffer",
        };
        const auto dst_buf = dev->create_buffer(dst_desc);
        ASSERT_NE(dst_buf.handle, 0ULL);

        auto& graphics_port = dev->get_graphics_execution_port();
        auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);

        // 2. Act: Record reset, top-of-pipe timestamp, work, transfer timestamp, and submit
        cmd.begin();
        cmd.reset_query_pool(pool, 0, 4);
        cmd.write_timestamp(pool, 0, pipeline_stage::top_of_pipe);

        const auto copy_reg = buffer_copy_region{
            .src_offset = 0,
            .dst_offset = 0,
            .size = buffer_size / 2,
        };
        cmd.copy_buffer(upload_buf, dst_buf, span<const buffer_copy_region>{&copy_reg, 1});
        cmd.write_timestamp(pool, 1, pipeline_stage::all_transfer);

        const auto copy_reg2 = buffer_copy_region{
            .src_offset = buffer_size / 2,
            .dst_offset = buffer_size / 2,
            .size = buffer_size / 2,
        };
        cmd.copy_buffer(upload_buf, dst_buf, span<const buffer_copy_region>{&copy_reg2, 1});
        cmd.write_timestamp(pool, 2, pipeline_stage::bottom_of_pipe);
        cmd.end();

        const auto* cmd_ptr = &cmd;
        const auto signal_sync = device_sync_point{
            .semaphore = timeline_sem,
            .value = 1,
            .stages = pipeline_stage::bottom_of_pipe,
        };

        const auto submit_res = graphics_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {},
                                                     span<const device_sync_point>{&signal_sync, 1});
        ASSERT_TRUE(submit_res.has_value());

        // Wait for GPU execution to complete before reading back results
        dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = 1});

        // 3. Assert: Results are successfully read and timestamps are strictly monotonic
        auto results = array<uint64_t, 4>{0, 0, 0, 0};
        const auto read_success = dev->get_query_pool_results(pool, 0, 3, span<uint64_t>{results.data(), 3}, false);
        EXPECT_TRUE(read_success);

        EXPECT_GT(results[0], 0ULL);
        EXPECT_GE(results[1], results[0]);
        EXPECT_GE(results[2], results[1]);

        const auto period = dev->get_timestamp_period_ns();
        EXPECT_GT(period, 0.0F);

        // Cleanup
        dev->destroy_buffer(upload_buf);
        dev->destroy_buffer(dst_buf);
        dev->destroy_semaphore(timeline_sem);
        dev->destroy_query_pool(pool);
    }

    // =========================================================================
    // Pipeline Statistics Query Tests
    // =========================================================================

    /// @brief Verify pipeline statistics query pool extracts vertex, primitive, and fragment counts during a triangle
    /// draw.
    TEST(query_test, pipeline_statistics_raster_draw)
    {
        // 1. Setup rendering target, graphics pipeline, and pipeline statistics query pool
        const auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        const auto& dev = env.dev;

        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;

        const auto tex_desc = texture_desc{
            .width = width,
            .height = height,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .format = data_format::rgba8_unorm,
            .usage = texture_usage::color_attachment | texture_usage::transfer_src,
        };
        const auto color_tex = dev->create_texture(tex_desc);
        ASSERT_NE(color_tex.handle, 0ULL);

        const auto view_desc = texture_view_desc{
            .base_mip_level = 0,
            .mip_level_count = 1,
            .base_array_layer = 0,
            .array_layer_count = 1,
        };
        const auto color_view = dev->create_texture_view(color_tex, view_desc);
        ASSERT_NE(color_view.handle, 0ULL);

        const auto vs_desc = shader_module_desc{
            .stage = shader_stage::vertex,
            .ir_code = span<const byte>{reinterpret_cast<const byte*>(shaders::raster::vs::test_raster_vs_spv),
                                        sizeof(shaders::raster::vs::test_raster_vs_spv)},
            .entry_point = "VSMain",
        };
        const auto fs_desc = shader_module_desc{
            .stage = shader_stage::fragment,
            .ir_code = span<const byte>{reinterpret_cast<const byte*>(shaders::raster::fs::test_raster_fs_spv),
                                        sizeof(shaders::raster::fs::test_raster_fs_spv)},
            .entry_point = "FSMain",
        };
        const auto stages = array{vs_desc, fs_desc};
        const auto color_formats = array{data_format::rgba8_unorm};

        const auto pipe_desc = graphics_pipeline_desc{
            .shader_modules = span<const shader_module_desc>{stages.data(), stages.size()},
            .color_attachment_formats = span<const data_format>{color_formats.data(), color_formats.size()},
            .primitive_topology = primitive_topology::triangle_list,
            .rasterization_state =
                {
                    .polygon_mode = polygon_mode::fill,
                    .cull_mode = cull_mode::none,
                    .front_face = vertex_winding_order::counter_clockwise,
                },
        };
        const auto pipe = dev->create_graphics_pipeline(pipe_desc);
        ASSERT_NE(pipe.handle, 0ULL);

        const auto stat_flags = enum_mask<pipeline_statistic_flags>{
            pipeline_statistic_flags::input_assembly_vertices | pipeline_statistic_flags::input_assembly_primitives |
            pipeline_statistic_flags::vertex_shader_invocations | pipeline_statistic_flags::clipping_input_primitives |
            pipeline_statistic_flags::clipping_output_primitives |
            pipeline_statistic_flags::fragment_shader_invocations};

        const auto qp_desc = query_pool_desc{
            .type = query_type::pipeline_statistics,
            .query_count = 1,
            .pipeline_statistics = stat_flags,
            .name = "Pipeline Stats Pool",
        };
        const auto pool = dev->create_query_pool(qp_desc);
        ASSERT_NE(pool.handle, 0ULL);

        const auto timeline_sem = dev->create_timeline_semaphore();
        ASSERT_NE(timeline_sem.handle, 0ULL);

        // 2. Act: Record begin_query / end_query around a single triangle draw pass
        auto& graphics_port = dev->get_graphics_execution_port();
        auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);

        cmd.begin();

        const auto init_barrier = texture_barrier{
            .texture = color_tex,
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
        };
        cmd.pipeline_barrier(span<const texture_barrier>{&init_barrier, 1}, {});

        cmd.reset_query_pool(pool, 0, 1);
        cmd.begin_query(pool, 0);

        const auto color_att = color_attachment{
            .view = color_view,
            .load_op = load_op::clear,
            .store_op = store_op::store,
            .clear_value =
                clear_color_value{
                    .r = 0.0f,
                    .g = 0.0f,
                    .b = 0.0f,
                    .a = 1.0f,
                },
        };

        cmd.begin_render_pass(span<const color_attachment>{&color_att, 1}, nullopt, width, height);
        cmd.set_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
        cmd.set_scissor(0, 0, width, height);
        cmd.bind_pipeline(pipe);

        const auto tint = raster_push_constants{
            .r = 1.0f,
            .g = 1.0f,
            .b = 1.0f,
            .a = 1.0f,
        };
        cmd.push_constants(shader_stage::vertex, 0,
                           span<const byte>{reinterpret_cast<const byte*>(&tint), sizeof(tint)});

        cmd.draw(3, 1, 0, 0);
        cmd.end_render_pass();

        cmd.end_query(pool, 0);
        cmd.end();

        const auto* cmd_ptr = &cmd;
        const auto signal_sync = device_sync_point{
            .semaphore = timeline_sem,
            .value = 1,
            .stages = pipeline_stage::bottom_of_pipe,
        };
        const auto submit_res = graphics_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {},
                                                     span<const device_sync_point>{&signal_sync, 1});
        ASSERT_TRUE(submit_res.has_value());

        dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = 1});

        // 3. Assert: Statistics query returns non-zero vertex, primitive, and fragment counts
        auto stats = array<uint64_t, 6>{0, 0, 0, 0, 0, 0};
        const auto read_success =
            dev->get_query_pool_results(pool, 0, 1, span<uint64_t>{stats.data(), stats.size()}, false);
        EXPECT_TRUE(read_success);

        // input_assembly_vertices (index 0) == 3 vertices
        EXPECT_EQ(stats[0], 3ULL);
        // input_assembly_primitives (index 1) == 1 primitive
        EXPECT_EQ(stats[1], 1ULL);
        // vertex_shader_invocations (index 2) >= 3
        EXPECT_GE(stats[2], 3ULL);
        // clipping_input_primitives (index 3) >= 1
        EXPECT_GE(stats[3], 1ULL);
        // clipping_output_primitives (index 4) >= 1
        EXPECT_GE(stats[4], 1ULL);
        // fragment_shader_invocations (index 5) > 0
        EXPECT_GT(stats[5], 0ULL);

        // Cleanup
        dev->destroy_semaphore(timeline_sem);
        dev->destroy_query_pool(pool);
        dev->destroy_graphics_pipeline(pipe);
        dev->destroy_texture_view(color_view);
        dev->destroy_texture(color_tex);
    }

    // =========================================================================
    // Multi-Queue Independence Tests
    // =========================================================================

    /// @brief Verify multi-queue query independence between graphics, compute, and transfer queues.
    TEST(query_test, multi_queue_query_independence)
    {
        // 1. Setup test environment and independent query pools per queue
        const auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        const auto& dev = env.dev;

        auto qp_desc = query_pool_desc{
            .type = query_type::timestamp,
            .query_count = 2,
            .pipeline_statistics = pipeline_statistic_flags::none,
        };

        qp_desc.name = "Graphics Timestamp Pool";
        const auto gfx_pool = dev->create_query_pool(qp_desc);
        ASSERT_NE(gfx_pool.handle, 0ULL);

        qp_desc.name = "Compute Timestamp Pool";
        const auto compute_pool = dev->create_query_pool(qp_desc);
        ASSERT_NE(compute_pool.handle, 0ULL);

        qp_desc.name = "Transfer Timestamp Pool";
        const auto transfer_pool = dev->create_query_pool(qp_desc);
        ASSERT_NE(transfer_pool.handle, 0ULL);

        const auto gfx_sem = dev->create_timeline_semaphore();
        const auto compute_sem = dev->create_timeline_semaphore();
        const auto transfer_sem = dev->create_timeline_semaphore();

        constexpr size_t buffer_size = 4096;
        const auto upload_desc = buffer_desc{
            .size = buffer_size,
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::transfer_src,
        };
        const auto upload_buf = dev->create_buffer(upload_desc);
        ASSERT_NE(upload_buf.handle, 0ULL);

        const auto dev_desc = buffer_desc{
            .size = buffer_size,
            .memory_usage = memory_usage::device_only,
            .usage = buffer_usage::transfer_dst | buffer_usage::transfer_src | buffer_usage::storage_buffer,
        };
        const auto test_buf = dev->create_buffer(dev_desc);
        ASSERT_NE(test_buf.handle, 0ULL);

        // 2. Act: Record and submit independent queries across all three execution ports
        auto& transfer_port = dev->get_async_transfer_execution_port();
        auto& transfer_cmd = transfer_port.acquire_command_list(0, command_list_lifetime::transient);
        transfer_cmd.begin();
        transfer_cmd.reset_query_pool(transfer_pool, 0, 2);
        transfer_cmd.write_timestamp(transfer_pool, 0, pipeline_stage::top_of_pipe);
        const auto reg1 = buffer_copy_region{.src_offset = 0, .dst_offset = 0, .size = 1024};
        transfer_cmd.copy_buffer(upload_buf, test_buf, span<const buffer_copy_region>{&reg1, 1});
        transfer_cmd.write_timestamp(transfer_pool, 1, pipeline_stage::all_transfer);
        transfer_cmd.end();

        const auto* transfer_cmd_ptr = &transfer_cmd;
        const auto transfer_sync = device_sync_point{
            .semaphore = transfer_sem,
            .value = 1,
            .stages = pipeline_stage::all_transfer,
        };
        const auto t_res = transfer_port.submit(span<const rhi::command_list*>{&transfer_cmd_ptr, 1}, {},
                                                span<const device_sync_point>{&transfer_sync, 1});
        ASSERT_TRUE(t_res.has_value());

        auto& compute_port = dev->get_async_compute_execution_port();
        auto& compute_cmd = compute_port.acquire_command_list(0, command_list_lifetime::transient);
        compute_cmd.begin();
        compute_cmd.reset_query_pool(compute_pool, 0, 2);
        compute_cmd.write_timestamp(compute_pool, 0, pipeline_stage::top_of_pipe);
        const auto reg2 = buffer_copy_region{.src_offset = 1024, .dst_offset = 1024, .size = 1024};
        compute_cmd.copy_buffer(upload_buf, test_buf, span<const buffer_copy_region>{&reg2, 1});
        compute_cmd.write_timestamp(compute_pool, 1, pipeline_stage::bottom_of_pipe);
        compute_cmd.end();

        const auto* compute_cmd_ptr = &compute_cmd;
        const auto compute_sync = device_sync_point{
            .semaphore = compute_sem,
            .value = 1,
            .stages = pipeline_stage::bottom_of_pipe,
        };
        const auto c_res = compute_port.submit(span<const rhi::command_list*>{&compute_cmd_ptr, 1}, {},
                                               span<const device_sync_point>{&compute_sync, 1});
        ASSERT_TRUE(c_res.has_value());

        auto& graphics_port = dev->get_graphics_execution_port();
        auto& gfx_cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
        gfx_cmd.begin();
        gfx_cmd.reset_query_pool(gfx_pool, 0, 2);
        gfx_cmd.write_timestamp(gfx_pool, 0, pipeline_stage::top_of_pipe);
        const auto reg3 = buffer_copy_region{.src_offset = 2048, .dst_offset = 2048, .size = 1024};
        gfx_cmd.copy_buffer(upload_buf, test_buf, span<const buffer_copy_region>{&reg3, 1});
        gfx_cmd.write_timestamp(gfx_pool, 1, pipeline_stage::bottom_of_pipe);
        gfx_cmd.end();

        const auto* gfx_cmd_ptr = &gfx_cmd;
        const auto gfx_sync = device_sync_point{
            .semaphore = gfx_sem,
            .value = 1,
            .stages = pipeline_stage::bottom_of_pipe,
        };
        const auto g_res = graphics_port.submit(span<const rhi::command_list*>{&gfx_cmd_ptr, 1}, {},
                                                span<const device_sync_point>{&gfx_sync, 1});
        ASSERT_TRUE(g_res.has_value());

        // Wait on all queues
        dev->wait_for_sync(host_sync_point{.semaphore = transfer_sem, .value = 1});
        dev->wait_for_sync(host_sync_point{.semaphore = compute_sem, .value = 1});
        dev->wait_for_sync(host_sync_point{.semaphore = gfx_sem, .value = 1});

        // 3. Assert: Read back timestamps from each queue independently and verify ordering
        auto transfer_res = array<uint64_t, 2>{0, 0};
        auto compute_res = array<uint64_t, 2>{0, 0};
        auto gfx_res = array<uint64_t, 2>{0, 0};

        EXPECT_TRUE(dev->get_query_pool_results(transfer_pool, 0, 2, span<uint64_t>{transfer_res.data(), 2}, false));
        EXPECT_TRUE(dev->get_query_pool_results(compute_pool, 0, 2, span<uint64_t>{compute_res.data(), 2}, false));
        EXPECT_TRUE(dev->get_query_pool_results(gfx_pool, 0, 2, span<uint64_t>{gfx_res.data(), 2}, false));

        EXPECT_GE(transfer_res[1], transfer_res[0]);
        EXPECT_GE(compute_res[1], compute_res[0]);
        EXPECT_GE(gfx_res[1], gfx_res[0]);

        // Cleanup
        dev->destroy_buffer(upload_buf);
        dev->destroy_buffer(test_buf);
        dev->destroy_semaphore(gfx_sem);
        dev->destroy_semaphore(compute_sem);
        dev->destroy_semaphore(transfer_sem);
        dev->destroy_query_pool(gfx_pool);
        dev->destroy_query_pool(compute_pool);
        dev->destroy_query_pool(transfer_pool);
    }

    // =========================================================================
    // Clock Calibration Tests
    // =========================================================================

    /// @brief Verify CPU-GPU clock calibration maps GPU timestamps accurately within the CPU submission bracket.
    TEST(query_test, clock_calibration_bracketing)
    {
        // 1. Setup test environment, timestamp query pool, and timeline semaphore
        const auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        const auto& dev = env.dev;

        const auto qp_desc = query_pool_desc{
            .type = query_type::timestamp,
            .query_count = 2,
            .pipeline_statistics = pipeline_statistic_flags::none,
            .name = "Calibration Test Pool",
        };
        const auto pool = dev->create_query_pool(qp_desc);
        ASSERT_NE(pool.handle, 0ULL);

        const auto timeline_sem = dev->create_timeline_semaphore();
        ASSERT_NE(timeline_sem.handle, 0ULL);

        auto& graphics_port = dev->get_graphics_execution_port();
        auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);

        cmd.begin();
        cmd.reset_query_pool(pool, 0, 2);
        cmd.write_timestamp(pool, 0, pipeline_stage::top_of_pipe);
        cmd.write_timestamp(pool, 1, pipeline_stage::bottom_of_pipe);
        cmd.end();

        // 2. Act: Measure CPU bracket around queue submission and completion
        const auto cpu_start_ns = timeline_calibrator::query_cpu_timestamp_ns();

        const auto* cmd_ptr = &cmd;
        const auto signal_sync = device_sync_point{
            .semaphore = timeline_sem,
            .value = 1,
            .stages = pipeline_stage::bottom_of_pipe,
        };
        const auto submit_res = graphics_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {},
                                                     span<const device_sync_point>{&signal_sync, 1});
        ASSERT_TRUE(submit_res.has_value());

        const auto cpu_submitted_ns = timeline_calibrator::query_cpu_timestamp_ns();

        dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = 1});

        const auto cpu_completed_ns = timeline_calibrator::query_cpu_timestamp_ns();

        auto gpu_ticks = array<uint64_t, 2>{0, 0};
        const auto read_success = dev->get_query_pool_results(pool, 0, 2, span<uint64_t>{gpu_ticks.data(), 2}, false);
        EXPECT_TRUE(read_success);

        // 3. Assert: Converted CPU timestamps are monotonic and within reasonable bounds
        const auto converted_cpu_0 = dev->convert_gpu_timestamp_to_cpu_ns(gpu_ticks[0]);
        const auto converted_cpu_1 = dev->convert_gpu_timestamp_to_cpu_ns(gpu_ticks[1]);

        EXPECT_GE(converted_cpu_1, converted_cpu_0);

        // Verify fallback calibrator manually
        auto calibrator = timeline_calibrator{dev->get_timestamp_period_ns()};
        calibrator.calibrate_fallback(cpu_start_ns, cpu_submitted_ns, gpu_ticks[0]);
        EXPECT_TRUE(calibrator.is_calibrated());
        EXPECT_FALSE(calibrator.is_hardware_calibrated());

        const auto fallback_converted_0 = calibrator.convert_gpu_timestamp_to_cpu_ns(gpu_ticks[0]);
        const auto fallback_converted_1 = calibrator.convert_gpu_timestamp_to_cpu_ns(gpu_ticks[1]);

        EXPECT_GE(fallback_converted_0, cpu_start_ns);
        EXPECT_LE(fallback_converted_0, cpu_completed_ns);
        EXPECT_GE(fallback_converted_1, fallback_converted_0);

        // Cleanup
        dev->destroy_semaphore(timeline_sem);
        dev->destroy_query_pool(pool);
    }
} // namespace tempest::rhi::vk
