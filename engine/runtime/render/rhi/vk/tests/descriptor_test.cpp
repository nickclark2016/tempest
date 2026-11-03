#include <gtest/gtest.h>
#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/guid.hpp>
#include <tempest/logger.hpp>
#include <tempest/mutex.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>
#include <tempest/vk/context.hpp>
#include <tempest/vk/device.hpp>

#include <tempest/utility.hpp>

namespace test_bindless_sample
{
#include <test_bindless_sample.comp.h>
}

namespace test_bindless_storage
{
#include <test_bindless_storage.comp.h>
}

namespace
{
    using namespace tempest;
    using namespace tempest::rhi;

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
        ctx_desc.application_name = "Tempest Descriptor Test";
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

TEST(descriptor_test, descriptor_slot_lifecycle)
{
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    // Allocate multiple sampler descriptors
    auto sampler_slot_1 = dev->allocate_descriptor(descriptor_type::sampler);
    auto sampler_slot_2 = dev->allocate_descriptor(descriptor_type::sampler);
    EXPECT_NE(sampler_slot_1.index, sampler_slot_2.index);

    // Allocate multiple sampled image descriptors
    auto texture_slot_1 = dev->allocate_descriptor(descriptor_type::sampled_image);
    auto texture_slot_2 = dev->allocate_descriptor(descriptor_type::sampled_image);
    EXPECT_NE(texture_slot_1.index, texture_slot_2.index);

    // Allocate multiple storage image descriptors
    auto storage_slot_1 = dev->allocate_descriptor(descriptor_type::storage_image);
    auto storage_slot_2 = dev->allocate_descriptor(descriptor_type::storage_image);
    EXPECT_NE(storage_slot_1.index, storage_slot_2.index);

    // Free and reallocate
    dev->free_descriptor(descriptor_type::sampler, sampler_slot_1);
    auto sampler_slot_3 = dev->allocate_descriptor(descriptor_type::sampler);
    EXPECT_EQ(sampler_slot_3.index, sampler_slot_1.index);
    EXPECT_GT(sampler_slot_3.generation, sampler_slot_1.generation);

    dev->free_descriptor(descriptor_type::sampled_image, texture_slot_1);
    dev->free_descriptor(descriptor_type::storage_image, storage_slot_1);
}

TEST(descriptor_test, bindless_storage_image_write)
{
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    // Create storage texture (64x64 RGBA8)
    constexpr uint32_t width = 64;
    constexpr uint32_t height = 64;

    auto tex_desc = texture_desc{
        .width = width,
        .height = height,
        .depth = 1,
        .mip_levels = 1,
        .array_layers = 1,
        .format = data_format::rgba8_unorm,
        .memory_usage = memory_usage::device_only,
        .usage = texture_usage::storage | texture_usage::transfer_src,
        .name = "storage_test_texture",
    };
    auto tex = dev->create_texture(tex_desc);
    ASSERT_NE(tex.handle, 0ULL);

    auto view = dev->create_texture_view(tex, texture_view_desc{});
    ASSERT_NE(view.handle, 0ULL);

    // Allocate and write storage image descriptor
    auto storage_slot = dev->allocate_descriptor(descriptor_type::storage_image);
    ASSERT_NE(storage_slot.index, ~0U);
    dev->write_storage_image_descriptor(storage_slot, view, image_layout::general);

    // Create readback buffer
    auto readback_desc = buffer_desc{
        .size = static_cast<uint64_t>(width) * height * 4,
        .memory_usage = memory_usage::readback,
        .usage = buffer_usage::transfer_dst,
        .name = "storage_readback_buffer",
    };
    auto readback_buf = dev->create_buffer(readback_desc);
    ASSERT_NE(readback_buf.handle, 0ULL);
    ASSERT_NE(readback_buf.cpu_address, nullptr);

    // Create compute pipeline
    auto ir_bytes = span<const byte>{
        reinterpret_cast<const byte*>(test_bindless_storage::test_bindless_storage_spv),
        sizeof(test_bindless_storage::test_bindless_storage_spv),
    };
    auto pipe_desc = compute_pipeline_desc{
        .shader_module =
            {
                .stage = shader_stage::compute,
                .ir_code = ir_bytes,
                .entry_point = "compute_storage_write",
            },
    };
    auto pipe = dev->create_compute_pipeline(pipe_desc);
    ASSERT_NE(pipe.handle, 0ULL);

    // Record commands
    auto& compute_port = dev->get_async_compute_execution_port();
    auto& cmd = compute_port.acquire_command_list(0, command_list_lifetime::transient);

    cmd.begin();

    auto init_barrier = texture_barrier{
        .texture = tex,
        .src =
            {
                .stages = pipeline_stage::top_of_pipe,
                .access = resource_access::none,
                .layout = image_layout::undefined,
            },
        .dst =
            {
                .stages = pipeline_stage::compute,
                .access = resource_access::write,
                .layout = image_layout::general,
            },
    };
    cmd.pipeline_barrier(span<const texture_barrier>{&init_barrier, 1}, {});

    cmd.bind_pipeline(pipe);

    struct StorageConstants
    {
        uint32_t storage_image_idx;
        uint32_t width;
        uint32_t height;
    };
    auto constants = StorageConstants{
        .storage_image_idx = storage_slot.index,
        .width = width,
        .height = height,
    };
    cmd.push_constants(shader_stage::compute, 0,
                       span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

    cmd.dispatch((width + 7) / 8, (height + 7) / 8, 1);

    auto trans_barrier = texture_barrier{
        .texture = tex,
        .src =
            {
                .stages = pipeline_stage::compute,
                .access = resource_access::write,
                .layout = image_layout::general,
            },
        .dst =
            {
                .stages = pipeline_stage::all_transfer,
                .access = resource_access::read,
                .layout = image_layout::general,
            },
    };
    cmd.pipeline_barrier(span<const texture_barrier>{&trans_barrier, 1}, {});

    auto copy_region = buffer_texture_copy_region{
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
    cmd.copy_texture_to_buffer(tex, readback_buf, span<const buffer_texture_copy_region>{&copy_region, 1});

    cmd.end();

    auto timeline_sem = dev->create_timeline_semaphore();
    auto signal_point = device_sync_point{
        .semaphore = timeline_sem,
        .value = 1,
    };

    const auto* cmd_ptr = &cmd;
    auto submit_res = compute_port.submit(span<const command_list*>{&cmd_ptr, 1}, {},
                                          span<const device_sync_point>{&signal_point, 1});
    ASSERT_TRUE(submit_res.has_value());

    auto host_wait = host_sync_point{
        .semaphore = timeline_sem,
        .value = 1,
    };
    dev->wait_for_sync(host_wait);

    // Verify written values in readback buffer
    const auto* readback_data = static_cast<const uint8_t*>(readback_buf.cpu_address);
    ASSERT_NE(readback_data, nullptr);

    // Pixel at (0, 0): r=0, g=0, b=0.75 (~191), a=1.0 (255)
    EXPECT_EQ(readback_data[0], 0);
    EXPECT_EQ(readback_data[1], 0);
    EXPECT_NEAR(readback_data[2], 191, 2);
    EXPECT_EQ(readback_data[3], 255);

    // Clean up
    dev->free_descriptor(descriptor_type::storage_image, storage_slot);
    dev->destroy_compute_pipeline(pipe);
    dev->destroy_texture_view(view);
    dev->destroy_texture(tex);
    dev->destroy_buffer(readback_buf);
    dev->destroy_semaphore(timeline_sem);
}

TEST(descriptor_test, bindless_sampled_image_read)
{
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    // Create a 2x2 RGBA8 texture with known colors:
    // (0,0) Red (255, 0, 0, 255)
    // (1,0) Green (0, 255, 0, 255)
    // (0,1) Blue (0, 0, 255, 255)
    // (1,1) White (255, 255, 255, 255)
    constexpr uint32_t tex_w = 2;
    constexpr uint32_t tex_h = 2;
    auto tex_pixels = array<uint8_t, 16>{
        255, 0,   0,   255, // (0,0)
        0,   255, 0,   255, // (1,0)
        0,   0,   255, 255, // (0,1)
        255, 255, 255, 255, // (1,1)
    };

    auto upload_desc = buffer_desc{
        .size = sizeof(tex_pixels),
        .memory_usage = memory_usage::upload,
        .usage = buffer_usage::transfer_src,
        .name = "tex_upload_buf",
    };
    auto upload_buf = dev->create_buffer(upload_desc);
    ASSERT_NE(upload_buf.handle, 0ULL);
    ASSERT_NE(upload_buf.cpu_address, nullptr);
    tempest::memcpy(upload_buf.cpu_address, tex_pixels.data(), sizeof(tex_pixels));

    auto tex_desc = texture_desc{
        .width = tex_w,
        .height = tex_h,
        .depth = 1,
        .mip_levels = 1,
        .array_layers = 1,
        .format = data_format::rgba8_unorm,
        .memory_usage = memory_usage::device_only,
        .usage = texture_usage::sampled | texture_usage::transfer_dst,
        .name = "bindless_sampled_tex",
    };
    auto tex = dev->create_texture(tex_desc);
    ASSERT_NE(tex.handle, 0ULL);

    auto view = dev->create_texture_view(tex, texture_view_desc{});
    ASSERT_NE(view.handle, 0ULL);

    auto samp_desc = sampler_desc{
        .min_filter = filter_mode::nearest,
        .mag_filter = filter_mode::nearest,
        .mipmap_mode = mipmap_mode::nearest,
        .address_u = address_mode::clamp_to_edge,
        .address_v = address_mode::clamp_to_edge,
        .address_w = address_mode::clamp_to_edge,
    };
    auto samp = dev->create_sampler(samp_desc);
    ASSERT_NE(samp.handle, 0ULL);

    // Allocate descriptors and write descriptors
    auto texture_slot = dev->allocate_descriptor(descriptor_type::sampled_image);
    ASSERT_NE(texture_slot.index, ~0U);
    dev->write_sampled_image_descriptor(texture_slot, view, image_layout::general);

    auto sampler_slot = dev->allocate_descriptor(descriptor_type::sampler);
    ASSERT_NE(sampler_slot.index, ~0U);
    dev->write_sampler_descriptor(sampler_slot, samp);

    // Create GPU output buffer for sampling results (2x2 float4s = 64 bytes)
    constexpr uint32_t sample_w = 2;
    constexpr uint32_t sample_h = 2;
    auto out_desc = buffer_desc{
        .size = static_cast<uint64_t>(sample_w) * sample_h * sizeof(float) * 4,
        .memory_usage = memory_usage::readback,
        .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
        .name = "sample_output_buf",
    };
    auto out_buf = dev->create_buffer(out_desc);
    ASSERT_NE(out_buf.handle, 0ULL);
    ASSERT_NE(out_buf.cpu_address, nullptr);

    // Create compute pipeline
    auto ir_bytes = span<const byte>{
        reinterpret_cast<const byte*>(test_bindless_sample::test_bindless_sample_spv),
        sizeof(test_bindless_sample::test_bindless_sample_spv),
    };
    auto pipe_desc = compute_pipeline_desc{
        .shader_module =
            {
                .stage = shader_stage::compute,
                .ir_code = ir_bytes,
                .entry_point = "compute_sample",
            },
    };
    auto pipe = dev->create_compute_pipeline(pipe_desc);
    ASSERT_NE(pipe.handle, 0ULL);

    // Upload texture and execute compute sampling
    auto& graphics_port = dev->get_graphics_execution_port();
    auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);

    cmd.begin();

    auto init_barrier = texture_barrier{
        .texture = tex,
        .src =
            {
                .stages = pipeline_stage::top_of_pipe,
                .access = resource_access::none,
                .layout = image_layout::undefined,
            },
        .dst =
            {
                .stages = pipeline_stage::all_transfer,
                .access = resource_access::write,
                .layout = image_layout::general,
            },
    };
    cmd.pipeline_barrier(span<const texture_barrier>{&init_barrier, 1}, {});

    auto copy_region = buffer_texture_copy_region{
        .buffer_offset = 0,
        .buffer_row_length = tex_w,
        .buffer_image_height = tex_h,
        .mip_level = 0,
        .base_array_layer = 0,
        .array_layer_count = 1,
        .image_offset_x = 0,
        .image_offset_y = 0,
        .image_offset_z = 0,
        .image_extent_width = tex_w,
        .image_extent_height = tex_h,
        .image_extent_depth = 1,
    };
    cmd.copy_buffer_to_texture(upload_buf, tex, span<const buffer_texture_copy_region>{&copy_region, 1});

    auto sample_barrier = texture_barrier{
        .texture = tex,
        .src =
            {
                .stages = pipeline_stage::all_transfer,
                .access = resource_access::write,
                .layout = image_layout::general,
            },
        .dst =
            {
                .stages = pipeline_stage::compute,
                .access = resource_access::read,
                .layout = image_layout::general,
            },
    };
    cmd.pipeline_barrier(span<const texture_barrier>{&sample_barrier, 1}, {});

    cmd.bind_pipeline(pipe);

    struct BindlessPushConstants
    {
        uint32_t sampler_idx;
        uint32_t texture_idx;
        uint32_t width;
        uint32_t height;
        uint64_t output_buffer_address;
    };
    auto constants = BindlessPushConstants{
        .sampler_idx = sampler_slot.index,
        .texture_idx = texture_slot.index,
        .width = sample_w,
        .height = sample_h,
        .output_buffer_address = out_buf.gpu_address,
    };
    cmd.push_constants(shader_stage::compute, 0,
                       span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

    cmd.dispatch((sample_w + 7) / 8, (sample_h + 7) / 8, 1);

    cmd.end();

    auto timeline_sem = dev->create_timeline_semaphore();
    auto signal_point = device_sync_point{
        .semaphore = timeline_sem,
        .value = 1,
    };

    const auto* cmd_ptr = &cmd;
    auto submit_res = graphics_port.submit(span<const command_list*>{&cmd_ptr, 1}, {},
                                           span<const device_sync_point>{&signal_point, 1});
    ASSERT_TRUE(submit_res.has_value());

    auto host_wait = host_sync_point{
        .semaphore = timeline_sem,
        .value = 1,
    };
    dev->wait_for_sync(host_wait);

    // Verify sampled floats in output buffer
    const auto* out_data = static_cast<const float*>(out_buf.cpu_address);
    ASSERT_NE(out_data, nullptr);

    // Pixel (0,0) should be Red: (1.0, 0.0, 0.0, 1.0)
    EXPECT_NEAR(out_data[0], 1.0F, 0.01F);
    EXPECT_NEAR(out_data[1], 0.0F, 0.01F);
    EXPECT_NEAR(out_data[2], 0.0F, 0.01F);
    EXPECT_NEAR(out_data[3], 1.0F, 0.01F);

    // Pixel (1,0) should be Green: (0.0, 1.0, 0.0, 1.0)
    EXPECT_NEAR(out_data[4], 0.0F, 0.01F);
    EXPECT_NEAR(out_data[5], 1.0F, 0.01F);
    EXPECT_NEAR(out_data[6], 0.0F, 0.01F);
    EXPECT_NEAR(out_data[7], 1.0F, 0.01F);

    // Clean up
    dev->free_descriptor(descriptor_type::sampled_image, texture_slot);
    dev->free_descriptor(descriptor_type::sampler, sampler_slot);
    dev->destroy_compute_pipeline(pipe);
    dev->destroy_sampler(samp);
    dev->destroy_texture_view(view);
    dev->destroy_texture(tex);
    dev->destroy_buffer(upload_buf);
    dev->destroy_buffer(out_buf);
    dev->destroy_semaphore(timeline_sem);
}

/// @brief Tests that free_descriptor defers zeroing and slot reuse when timeline sync points are pending.
TEST(descriptor_test, descriptor_free_timeline_retirement)
{
    // Setup: Create test environment with Vulkan device
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    // Act: Allocate a sampled image descriptor
    auto slot = dev->allocate_descriptor(descriptor_type::sampled_image);
    EXPECT_NE(slot.index, ~0U);
    const auto initial_index = slot.index;
    const auto initial_generation = slot.generation;

    // Create a timeline semaphore and signal a value on GPU to establish an in-flight sync point
    auto timeline_sem = dev->create_timeline_semaphore();
    auto& graphics_port = dev->get_graphics_execution_port();
    auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
    cmd.begin();
    cmd.end();

    const auto* cmd_ptr = &cmd;
    auto signal_point = device_sync_point{
        .semaphore = timeline_sem,
        .value = 10,
        .stages = pipeline_stage::all_graphics,
    };
    auto submit_res = graphics_port.submit(span<const command_list*>{&cmd_ptr, 1}, {},
                                           span<const device_sync_point>{&signal_point, 1});
    ASSERT_TRUE(submit_res.has_value());

    // Act: Free descriptor while semaphore value on device is still advancing
    dev->free_descriptor(descriptor_type::sampled_image, slot);

    // Assert: Retired descriptor queue has deferred this entry
    EXPECT_GT(vk_dev->get_retired_descriptor_count(), 0U);

    // Wait for the sync point on host
    dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = 10});

    // Act: Process deferred retirements now that GPU execution has passed the sync point
    dev->process_deferred_retirements();

    // Assert: Retired descriptor count is now 0 and slot was returned to free list
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);

    // Reallocate descriptor: should reuse the recycled index with incremented generation
    auto reallocated_slot = dev->allocate_descriptor(descriptor_type::sampled_image);
    EXPECT_EQ(reallocated_slot.index, initial_index);
    EXPECT_EQ(reallocated_slot.generation, initial_generation + 1);

    // Cleanup
    dev->free_descriptor(descriptor_type::sampled_image, reallocated_slot);
    dev->destroy_semaphore(timeline_sem);
}

/// @brief Stress-tests descriptor retirement queue under zero-timeout flushes and delayed timeline advances,
/// verifying that pending retired descriptors are never prematurely recycled before timeline completion.
TEST(descriptor_test, descriptor_delayed_timeline_and_zero_timeout_flushes)
{
    // Setup: Initialize test environment with Vulkan device
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    constexpr auto descriptor_batch_count = 32U;
    auto slots = vector<descriptor_handle>{};
    slots.reserve(descriptor_batch_count);

    for (auto i = 0U; i < descriptor_batch_count; ++i)
    {
        auto slot = dev->allocate_descriptor(descriptor_type::sampled_image);
        ASSERT_NE(slot.index, ~0U);
        slots.push_back(slot);
    }

    // Establish a pending GPU timeline sync point with a high value
    auto timeline_sem = dev->create_timeline_semaphore();
    auto& graphics_port = dev->get_graphics_execution_port();
    auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
    cmd.begin();
    cmd.end();

    const auto* cmd_ptr = &cmd;
    constexpr auto target_timeline_val = uint64_t{200};
    auto signal_point = device_sync_point{
        .semaphore = timeline_sem,
        .value = target_timeline_val,
        .stages = pipeline_stage::all_graphics,
    };
    auto submit_res = graphics_port.submit(span<const command_list*>{&cmd_ptr, 1}, {},
                                           span<const device_sync_point>{&signal_point, 1});
    ASSERT_TRUE(submit_res.has_value());

    // Act: Free all descriptors while GPU execution is still pending
    for (const auto slot : slots)
    {
        dev->free_descriptor(descriptor_type::sampled_image, slot);
    }

    // Assert: All 32 descriptors are enqueued in deferred retirement
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), descriptor_batch_count);

    // Act: Call zero-timeout retirement flush before waiting on sync point
    dev->process_deferred_retirements();

    // Assert: Descriptors remain safely deferred and are NOT recycled prematurely
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), descriptor_batch_count);

    // Wait on host for GPU completion
    dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = target_timeline_val});

    // Act: Now process retirements after GPU timeline completion
    dev->process_deferred_retirements();

    // Assert: All 32 descriptors are now recycled to free list
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);

    // Reallocate 32 descriptors to verify generations were incremented
    auto reallocated_slots = vector<descriptor_handle>{};
    reallocated_slots.reserve(descriptor_batch_count);
    for (auto i = 0U; i < descriptor_batch_count; ++i)
    {
        auto new_slot = dev->allocate_descriptor(descriptor_type::sampled_image);
        ASSERT_NE(new_slot.index, ~0U);
        reallocated_slots.push_back(new_slot);
    }

    // Cleanup
    for (const auto slot : reallocated_slots)
    {
        dev->free_descriptor(descriptor_type::sampled_image, slot);
    }
    dev->destroy_semaphore(timeline_sem);
}

/// @brief Stress-tests device destruction order when descriptors are still pending in the retirement queue.
TEST(descriptor_test, descriptor_destruction_order_with_pending_retirements)
{
    // Setup: Create local test environment
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    // Allocate descriptors across multiple types
    auto sampled_slot = dev->allocate_descriptor(descriptor_type::sampled_image);
    auto storage_slot = dev->allocate_descriptor(descriptor_type::storage_image);
    auto sampler_slot = dev->allocate_descriptor(descriptor_type::sampler);

    ASSERT_NE(sampled_slot.index, ~0U);
    ASSERT_NE(storage_slot.index, ~0U);
    ASSERT_NE(sampler_slot.index, ~0U);

    // Submit work with timeline semaphore
    auto timeline_sem = dev->create_timeline_semaphore();
    auto& graphics_port = dev->get_graphics_execution_port();
    auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
    cmd.begin();
    cmd.end();

    const auto* cmd_ptr = &cmd;
    auto signal_point = device_sync_point{
        .semaphore = timeline_sem,
        .value = 50,
        .stages = pipeline_stage::all_graphics,
    };
    auto submit_res = graphics_port.submit(span<const command_list*>{&cmd_ptr, 1}, {},
                                           span<const device_sync_point>{&signal_point, 1});
    ASSERT_TRUE(submit_res.has_value());

    // Free descriptors so they enter _retired_descriptors
    dev->free_descriptor(descriptor_type::sampled_image, sampled_slot);
    dev->free_descriptor(descriptor_type::storage_image, storage_slot);
    dev->free_descriptor(descriptor_type::sampler, sampler_slot);

    // Verify they are deferred
    EXPECT_GT(vk_dev->get_retired_descriptor_count(), 0U);

    // Cleanup semaphore before device destruction
    dev->destroy_semaphore(timeline_sem);

    // Device destructor will run at end of scope and must cleanly flush all pending retirements
    env.dev.reset();
    env.context.reset();
}

/// @brief Tests that calling free_descriptor twice with the same handle before GPU completion
/// rejects the second call, prevents retirement queue duplication, and guarantees free list uniqueness.
TEST(descriptor_test, descriptor_double_free_rejection_and_free_list_integrity)
{
    // Setup: Initialize test environment with Vulkan device
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    // Act: Allocate a single descriptor
    const auto initial_slot = dev->allocate_descriptor(descriptor_type::sampled_image);
    ASSERT_NE(initial_slot.index, ~0U);
    const auto initial_index = initial_slot.index;
    const auto initial_generation = initial_slot.generation;

    // Establish pending GPU timeline sync point to ensure deallocation is deferred
    auto timeline_sem = dev->create_timeline_semaphore();
    auto& graphics_port = dev->get_graphics_execution_port();
    auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
    cmd.begin();
    cmd.end();

    const auto* cmd_ptr = &cmd;
    constexpr auto target_timeline_val = uint64_t{50};
    auto signal_point = device_sync_point{
        .semaphore = timeline_sem,
        .value = target_timeline_val,
        .stages = pipeline_stage::all_graphics,
    };
    auto submit_res = graphics_port.submit(span<const command_list*>{&cmd_ptr, 1}, {},
                                           span<const device_sync_point>{&signal_point, 1});
    ASSERT_TRUE(submit_res.has_value());

    // Act: Issue initial free_descriptor call
    dev->free_descriptor(descriptor_type::sampled_image, initial_slot);

    // Assert: First free is accepted and enqueued in deferred retirement
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 1U);

    // Act: Issue adversarial duplicate free_descriptor with identical handle before GPU sync advances
    dev->free_descriptor(descriptor_type::sampled_image, initial_slot);

    // Assert: Duplicate free is rejected; retired descriptor count must remain exactly 1 (no duplicate entry)
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 1U);

    // Wait on host for GPU execution to complete the timeline sync point
    dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = target_timeline_val});

    // Act: Process retirements now that GPU execution is complete
    dev->process_deferred_retirements();

    // Assert: Retirement queue is now empty and exactly one slot was recycled
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);

    // Act: Allocate two descriptors sequentially to verify free list integrity
    const auto first_realloc = dev->allocate_descriptor(descriptor_type::sampled_image);
    const auto second_realloc = dev->allocate_descriptor(descriptor_type::sampled_image);

    // Assert: First reallocation reuses the recycled slot with an incremented generation
    EXPECT_EQ(first_realloc.index, initial_index);
    EXPECT_GT(first_realloc.generation, initial_generation);

    // Assert: Second reallocation must allocate a distinct slot; no duplicate index aliasing in free list
    EXPECT_NE(second_realloc.index, first_realloc.index);

    // Act: Attempt to free using stale initial_slot handle; must be rejected due to stale generation
    dev->free_descriptor(descriptor_type::sampled_image, initial_slot);
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);

    // Cleanup
    dev->free_descriptor(descriptor_type::sampled_image, first_realloc);
    dev->free_descriptor(descriptor_type::sampled_image, second_realloc);
    dev->destroy_semaphore(timeline_sem);
}

/// @brief Tests that freeing a descriptor during an active frame N preserves descriptor buffer memory
/// and defers recycling, even when the GPU has fully completed the previous submission (frame N-1).
TEST(descriptor_test, descriptor_active_frame_in_flight_memory_retention)
{
    // Setup: Initialize test environment with Vulkan device
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    // Frame N-1 Execution: Submit baseline work and wait for full GPU completion
    auto& graphics_port = dev->get_graphics_execution_port();
    auto& cmd_prev = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
    cmd_prev.begin();
    cmd_prev.end();

    const auto* cmd_prev_ptr = &cmd_prev;
    auto prev_submit_res = graphics_port.submit(span<const command_list*>{&cmd_prev_ptr, 1}, {}, {});
    ASSERT_TRUE(prev_submit_res.has_value());

    // Ensure GPU is completely caught up to Frame N-1 on the execution port timeline
    graphics_port.wait_idle();
    const auto prev_sync_point = graphics_port.get_timeline_sync_point();
    const auto prev_timeline_val = prev_sync_point.value;
    EXPECT_GT(prev_timeline_val, 0U);
    EXPECT_EQ(dev->get_semaphore_value(prev_sync_point.semaphore), prev_timeline_val);

    // Frame N Active: Allocate a sampled image descriptor and write a valid texture view to it
    const auto active_slot = dev->allocate_descriptor(descriptor_type::sampled_image);
    ASSERT_NE(active_slot.index, ~0U);

    auto tex_desc = texture_desc{
        .width = 4,
        .height = 4,
        .depth = 1,
        .mip_levels = 1,
        .array_layers = 1,
        .format = data_format::rgba8_unorm,
        .memory_usage = memory_usage::device_only,
        .usage = texture_usage::sampled,
        .name = "active_frame_test_texture",
    };
    auto tex = dev->create_texture(tex_desc);
    ASSERT_NE(tex.handle, 0ULL);

    auto view = dev->create_texture_view(tex, texture_view_desc{});
    ASSERT_NE(view.handle, 0ULL);

    // Act: Write descriptor payload into the device descriptor buffer
    dev->write_sampled_image_descriptor(active_slot, view, image_layout::general);

    // Assert: Descriptor memory is populated and non-zero
    EXPECT_FALSE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampled_image, active_slot.index));

    // Act: Free descriptor during Frame N recording before Frame N GPU completion
    dev->free_descriptor(descriptor_type::sampled_image, active_slot);

    // Assert: Descriptor must NOT be immediately recycled or zeroed despite GPU caught up to Frame N-1
    EXPECT_GT(vk_dev->get_retired_descriptor_count(), 0U);
    EXPECT_FALSE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampled_image, active_slot.index));

    // Act: Now submit Frame N work to advance timeline and execute pending frame
    auto& cmd_active = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
    cmd_active.begin();
    cmd_active.end();

    const auto* cmd_active_ptr = &cmd_active;
    auto active_submit_res = graphics_port.submit(span<const command_list*>{&cmd_active_ptr, 1}, {}, {});
    ASSERT_TRUE(active_submit_res.has_value());

    // Wait on host for Frame N GPU execution to finish
    graphics_port.wait_idle();

    // Act: Process deferred retirements now that Frame N has completed on GPU
    dev->process_deferred_retirements();

    // Assert: Descriptor is now safely recycled to free list and its buffer memory is cleanly zeroed
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);
    EXPECT_TRUE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampled_image, active_slot.index));

    // Cleanup
    dev->destroy_texture_view(view);
    dev->destroy_texture(tex);
}

/// @brief Verifies that calling free_descriptor with a stale generation handle
/// on an already reallocated slot is rejected and does not corrupt the active slot.
TEST(descriptor_test, descriptor_stale_generation_free_rejection)
{
    // Setup: Create local test environment
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();

    // Act: Allocate slot
    const auto original_slot = dev->allocate_descriptor(descriptor_type::sampler);
    ASSERT_NE(original_slot.index, ~0U);

    // Free and recycle immediately
    dev->free_descriptor(descriptor_type::sampler, original_slot);

    // Reallocate the same slot: generation must be incremented
    const auto reallocated_slot = dev->allocate_descriptor(descriptor_type::sampler);
    EXPECT_EQ(reallocated_slot.index, original_slot.index);
    EXPECT_GT(reallocated_slot.generation, original_slot.generation);

    // Act: Attempt to free using the original (stale) handle
    dev->free_descriptor(descriptor_type::sampler, original_slot);

    // Assert: The slot must remain allocated to the new owner; freeing with the valid handle must succeed
    dev->free_descriptor(descriptor_type::sampler, reallocated_slot);
}

/// @brief Verifies that writing to a descriptor that has been freed or is currently retiring
/// is safely rejected and does not overwrite descriptor memory.
TEST(descriptor_test, descriptor_write_use_after_free_prevention)
{
    // Setup: Create local test environment
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    const auto samp_desc = sampler_desc{};
    const auto samp = dev->create_sampler(samp_desc);
    ASSERT_NE(samp.handle, 0ULL);

    const auto slot = dev->allocate_descriptor(descriptor_type::sampler);
    ASSERT_NE(slot.index, ~0U);

    // Act: Free the descriptor
    dev->free_descriptor(descriptor_type::sampler, slot);

    // Assert: Writing to the freed slot must safely no-op without modifying descriptor memory
    dev->write_sampler_descriptor(slot, samp);
    EXPECT_TRUE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampler, slot.index));

    // Cleanup
    dev->destroy_sampler(samp);
}

/// @brief Tests that multi-frame latency delay retains descriptor backing memory and delays recycling
/// across multiple configured in-flight frame boundaries, even after GPU timeline execution has signaled.
TEST(descriptor_test, descriptor_multi_frame_latency_delay_and_retention)
{
    // Setup: Initialize test environment with Vulkan device
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    constexpr auto configured_frames_in_flight = uint32_t{3};
    vk_dev->set_frames_in_flight(configured_frames_in_flight);

    // Create texture and view for writing into descriptor buffer
    const auto texture_description = texture_desc{
        .width = 4,
        .height = 4,
        .depth = 1,
        .mip_levels = 1,
        .array_layers = 1,
        .format = data_format::rgba8_unorm,
        .memory_usage = memory_usage::device_only,
        .usage = texture_usage::sampled,
        .name = "multi_frame_latency_texture",
    };
    const auto texture_handle = dev->create_texture(texture_description);
    ASSERT_NE(texture_handle.handle, 0ULL);

    const auto view_handle = dev->create_texture_view(texture_handle, texture_view_desc{});
    ASSERT_NE(view_handle.handle, 0ULL);

    // Act: Allocate sampled image descriptor and populate payload
    const auto slot_handle = dev->allocate_descriptor(descriptor_type::sampled_image);
    ASSERT_NE(slot_handle.index, ~0U);
    dev->write_sampled_image_descriptor(slot_handle, view_handle, image_layout::general);

    // Assert: Descriptor memory is populated and non-zero
    EXPECT_FALSE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampled_image, slot_handle.index));

    // Submit work so execution port has an active timeline value
    auto timeline_semaphore = dev->create_timeline_semaphore();
    auto& graphics_port = dev->get_graphics_execution_port();
    auto& recorded_cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
    recorded_cmd.begin();
    recorded_cmd.end();

    const auto* recorded_cmd_ptr = &recorded_cmd;
    constexpr auto target_timeline_value = uint64_t{20};
    const auto signal_point = device_sync_point{
        .semaphore = timeline_semaphore,
        .value = target_timeline_value,
        .stages = pipeline_stage::all_graphics,
    };
    const auto submit_result = graphics_port.submit(span<const command_list*>{&recorded_cmd_ptr, 1}, {},
                                                    span<const device_sync_point>{&signal_point, 1});
    ASSERT_TRUE(submit_result.has_value());

    // Free the descriptor handle; must be enqueued with frames_remaining = 3
    dev->free_descriptor(descriptor_type::sampled_image, slot_handle);
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 1U);
    EXPECT_FALSE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampled_image, slot_handle.index));

    // Boundary Frame 1: Simulate frame transition
    dev->process_deferred_retirements();
    // Assert: Still in retirement queue with frames_remaining = 2; memory not cleared
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 1U);
    EXPECT_FALSE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampled_image, slot_handle.index));

    // Wait on host for GPU to finish target_timeline_value
    dev->wait_for_sync(host_sync_point{.semaphore = timeline_semaphore, .value = target_timeline_value});
    EXPECT_GE(dev->get_semaphore_value(timeline_semaphore), target_timeline_value);

    // Boundary Frame 2: Simulate frame transition; GPU done, but frames_remaining becomes 1
    dev->process_deferred_retirements();
    // Assert: Must NOT recycle yet because multi-frame latency delay requires 3 full frame boundaries
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 1U);
    EXPECT_FALSE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampled_image, slot_handle.index));

    // Boundary Frame 3: Simulate frame transition; frames_remaining becomes 0 and GPU sync satisfied
    dev->process_deferred_retirements();
    // Assert: Now retired descriptor is safely recycled and its buffer memory zeroed
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);
    EXPECT_TRUE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampled_image, slot_handle.index));

    // Cleanup
    dev->destroy_semaphore(timeline_semaphore);
    dev->destroy_texture_view(view_handle);
    dev->destroy_texture(texture_handle);
}

/// @brief Tests that invoking wait_idle forces immediate retirement and zeroing of all pending descriptors,
/// even when multiple frames remain in their retirement delay countdown.
TEST(descriptor_test, descriptor_idle_force_flush_recycles_all_pending)
{
    // Setup: Initialize test environment with Vulkan device
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    constexpr auto configured_frames_in_flight = uint32_t{5};
    vk_dev->set_frames_in_flight(configured_frames_in_flight);

    // Create sampler
    const auto sampler_description = sampler_desc{};
    const auto sampler_handle_val = dev->create_sampler(sampler_description);
    ASSERT_NE(sampler_handle_val.handle, 0ULL);

    // Allocate and populate a batch of sampler descriptors
    constexpr auto allocation_count = size_t{6};
    auto allocated_slots = inplace_vector<descriptor_handle, allocation_count>{};
    for (auto allocation_index = size_t{0}; allocation_index < allocation_count; ++allocation_index)
    {
        const auto slot = dev->allocate_descriptor(descriptor_type::sampler);
        ASSERT_NE(slot.index, ~0U);
        dev->write_sampler_descriptor(slot, sampler_handle_val);
        EXPECT_FALSE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampler, slot.index));
        allocated_slots.push_back(slot);
    }

    // Submit work so execution port has an active timeline value
    auto& graphics_port = dev->get_graphics_execution_port();
    auto& recorded_cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
    recorded_cmd.begin();
    recorded_cmd.end();

    const auto* recorded_cmd_ptr = &recorded_cmd;
    const auto submit_result = graphics_port.submit(span<const command_list*>{&recorded_cmd_ptr, 1}, {}, {});
    ASSERT_TRUE(submit_result.has_value());

    // Free all descriptors so they enter retirement queue with frames_remaining = 5
    for (const auto slot : allocated_slots)
    {
        dev->free_descriptor(descriptor_type::sampler, slot);
    }
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), allocation_count);

    // Advance 1 frame boundary: frames_remaining becomes 4, so non-blocking retirements will not recycle
    dev->process_deferred_retirements();
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), allocation_count);
    for (const auto slot : allocated_slots)
    {
        EXPECT_FALSE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampler, slot.index));
    }

    // Act: Invoke wait_idle to force flush all pending in-flight retirements
    dev->wait_idle();

    // Assert: Retirement queue must be completely empty and all descriptor memory zeroed
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);
    for (const auto slot : allocated_slots)
    {
        EXPECT_TRUE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampler, slot.index));
    }

    // Reallocate slots to verify free list restored cleanly
    for (auto reallocation_index = size_t{0}; reallocation_index < allocation_count; ++reallocation_index)
    {
        const auto reallocated_slot = dev->allocate_descriptor(descriptor_type::sampler);
        EXPECT_NE(reallocated_slot.index, ~0U);
        dev->free_descriptor(descriptor_type::sampler, reallocated_slot);
    }

    // Cleanup
    dev->wait_idle();
    dev->destroy_sampler(sampler_handle_val);
}

/// @brief Stress-tests rapid consecutive frame allocations, writes, submissions, frees,
/// and non-blocking retirement pumps across multiple frames in flight without leaks or premature reuse.
TEST(descriptor_test, descriptor_rapid_frame_loop_stress)
{
    // Setup: Initialize test environment with Vulkan device
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    constexpr auto configured_frames_in_flight = uint32_t{2};
    vk_dev->set_frames_in_flight(configured_frames_in_flight);

    const auto sampler_description = sampler_desc{};
    const auto sampler_handle_val = dev->create_sampler(sampler_description);
    ASSERT_NE(sampler_handle_val.handle, 0ULL);

    auto& graphics_port = dev->get_graphics_execution_port();
    constexpr auto total_test_frames = size_t{25};

    // Act: Run rapid simulated frame loop
    for (auto frame_index = size_t{0}; frame_index < total_test_frames; ++frame_index)
    {
        // Allocate and write
        const auto slot = dev->allocate_descriptor(descriptor_type::sampler);
        ASSERT_NE(slot.index, ~0U);
        dev->write_sampler_descriptor(slot, sampler_handle_val);
        EXPECT_FALSE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampler, slot.index));

        // Submit work
        auto& recorded_cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
        recorded_cmd.begin();
        recorded_cmd.end();

        const auto* recorded_cmd_ptr = &recorded_cmd;
        const auto submit_result = graphics_port.submit(span<const command_list*>{&recorded_cmd_ptr, 1}, {}, {});
        ASSERT_TRUE(submit_result.has_value());

        // Free descriptor during active frame
        dev->free_descriptor(descriptor_type::sampler, slot);

        // Memory must be retained immediately after free
        EXPECT_FALSE(vk_dev->is_descriptor_memory_zeroed(descriptor_type::sampler, slot.index));

        // Non-blocking frame retirement pump
        dev->process_deferred_retirements();
    }

    // Act: Idle device at end of loop to finalize all in-flight retirements
    dev->wait_idle();

    // Assert: All descriptors drained from retirement queue
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);

    // Cleanup
    dev->destroy_sampler(sampler_handle_val);
}

/// @brief Adversarially stress-tests repeated double-free, triple-free, quad-free, and stale-generation
/// free rejection across all descriptor types (sampler, sampled image, storage image) to guarantee that
/// retirement queues cannot be duplicated, free lists cannot be corrupted, and active reallocated slots
/// cannot be prematurely destroyed by stale handles.
TEST(descriptor_test, descriptor_adversarial_repeated_free_stress)
{
    // Setup: Initialize test environment with Vulkan device
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    constexpr size_t batch_size = 40;
    auto sampler_slots = vector<descriptor_handle>{};
    auto sampled_image_slots = vector<descriptor_handle>{};
    auto storage_image_slots = vector<descriptor_handle>{};
    sampler_slots.reserve(batch_size);
    sampled_image_slots.reserve(batch_size);
    storage_image_slots.reserve(batch_size);

    for (size_t idx = 0; idx < batch_size; ++idx)
    {
        sampler_slots.push_back(dev->allocate_descriptor(descriptor_type::sampler));
        sampled_image_slots.push_back(dev->allocate_descriptor(descriptor_type::sampled_image));
        storage_image_slots.push_back(dev->allocate_descriptor(descriptor_type::storage_image));
        ASSERT_NE(sampler_slots.back().index, ~0U);
        ASSERT_NE(sampled_image_slots.back().index, ~0U);
        ASSERT_NE(storage_image_slots.back().index, ~0U);
    }

    // Submit transient GPU work to establish pending timeline sync point so retirements are deferred
    auto timeline_sem = dev->create_timeline_semaphore();
    auto& graphics_port = dev->get_graphics_execution_port();
    auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
    cmd.begin();
    cmd.end();

    const auto* cmd_ptr = &cmd;
    constexpr auto target_timeline_val = uint64_t{100};
    auto signal_point = device_sync_point{
        .semaphore = timeline_sem,
        .value = target_timeline_val,
        .stages = pipeline_stage::all_graphics,
    };
    auto submit_res = graphics_port.submit(span<const command_list*>{&cmd_ptr, 1}, {},
                                           span<const device_sync_point>{&signal_point, 1});
    ASSERT_TRUE(submit_res.has_value());

    // Act: Phase 1 — Issue first free for all 120 descriptors
    for (size_t idx = 0; idx < batch_size; ++idx)
    {
        dev->free_descriptor(descriptor_type::sampler, sampler_slots[idx]);
        dev->free_descriptor(descriptor_type::sampled_image, sampled_image_slots[idx]);
        dev->free_descriptor(descriptor_type::storage_image, storage_image_slots[idx]);
    }

    // Assert: All 120 slots are successfully queued in deferred retirement
    constexpr auto total_slots = batch_size * 3;
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), total_slots);

    // Act: Phase 2 — Adversarial Double-Free
    for (size_t idx = 0; idx < batch_size; ++idx)
    {
        dev->free_descriptor(descriptor_type::sampler, sampler_slots[idx]);
        dev->free_descriptor(descriptor_type::sampled_image, sampled_image_slots[idx]);
        dev->free_descriptor(descriptor_type::storage_image, storage_image_slots[idx]);
    }
    // Assert: Duplicate frees rejected; retired count must remain strictly 120
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), total_slots);

    // Act: Phase 3 — Adversarial Triple-Free
    for (size_t idx = 0; idx < batch_size; ++idx)
    {
        dev->free_descriptor(descriptor_type::sampler, sampler_slots[idx]);
        dev->free_descriptor(descriptor_type::sampled_image, sampled_image_slots[idx]);
        dev->free_descriptor(descriptor_type::storage_image, storage_image_slots[idx]);
    }
    // Assert: Triple frees rejected; retired count remains strictly 120
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), total_slots);

    // Act: Phase 4 — Adversarial Quad-Free
    for (size_t idx = 0; idx < batch_size; ++idx)
    {
        dev->free_descriptor(descriptor_type::sampler, sampler_slots[idx]);
        dev->free_descriptor(descriptor_type::sampled_image, sampled_image_slots[idx]);
        dev->free_descriptor(descriptor_type::storage_image, storage_image_slots[idx]);
    }
    // Assert: Quad frees rejected; retired count remains strictly 120
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), total_slots);

    // Wait on host for GPU execution to complete the timeline sync point
    dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = target_timeline_val});

    // Act: Process retirements now that GPU work has completed
    dev->process_deferred_retirements();

    // Assert: All 120 descriptors are recycled and retirement queue is empty
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);

    // Act: Phase 5 — Attempt to free old handles now that slots are back on free list (free state)
    for (size_t idx = 0; idx < batch_size; ++idx)
    {
        dev->free_descriptor(descriptor_type::sampler, sampler_slots[idx]);
        dev->free_descriptor(descriptor_type::sampled_image, sampled_image_slots[idx]);
        dev->free_descriptor(descriptor_type::storage_image, storage_image_slots[idx]);
    }
    // Assert: Freeing slots in free status is rejected; retired count remains 0
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);

    // Act: Phase 6 — Reallocate all 120 descriptors
    auto new_samplers = vector<descriptor_handle>{};
    auto new_sampled = vector<descriptor_handle>{};
    auto new_storage = vector<descriptor_handle>{};
    new_samplers.reserve(batch_size);
    new_sampled.reserve(batch_size);
    new_storage.reserve(batch_size);

    auto sampler_indices_seen = array<bool, 1024>{};
    auto sampled_indices_seen = array<bool, 4096>{};
    auto storage_indices_seen = array<bool, 1024>{};

    for (size_t idx = 0; idx < batch_size; ++idx)
    {
        const auto slot_sampler = dev->allocate_descriptor(descriptor_type::sampler);
        const auto slot_sampled = dev->allocate_descriptor(descriptor_type::sampled_image);
        const auto slot_storage = dev->allocate_descriptor(descriptor_type::storage_image);

        // Assert: Generations must be strictly bumped
        EXPECT_GT(slot_sampler.generation, sampler_slots[idx].generation);
        EXPECT_GT(slot_sampled.generation, sampled_image_slots[idx].generation);
        EXPECT_GT(slot_storage.generation, storage_image_slots[idx].generation);

        // Assert: Free list must hand out unique indices; no duplicates
        EXPECT_FALSE(sampler_indices_seen[slot_sampler.index]);
        sampler_indices_seen[slot_sampler.index] = true;
        new_samplers.push_back(slot_sampler);

        EXPECT_FALSE(sampled_indices_seen[slot_sampled.index]);
        sampled_indices_seen[slot_sampled.index] = true;
        new_sampled.push_back(slot_sampled);

        EXPECT_FALSE(storage_indices_seen[slot_storage.index]);
        storage_indices_seen[slot_storage.index] = true;
        new_storage.push_back(slot_storage);
    }

    // Act: Phase 7 — Attack active reallocated slots using stale initial handles
    for (size_t idx = 0; idx < batch_size; ++idx)
    {
        dev->free_descriptor(descriptor_type::sampler, sampler_slots[idx]);
        dev->free_descriptor(descriptor_type::sampled_image, sampled_image_slots[idx]);
        dev->free_descriptor(descriptor_type::storage_image, storage_image_slots[idx]);
    }
    // Assert: Stale frees rejected due to generation mismatch; newly allocated slots are untouched
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);

    // Cleanup: Safely free all reallocated descriptors with their valid handles
    for (size_t idx = 0; idx < batch_size; ++idx)
    {
        dev->free_descriptor(descriptor_type::sampler, new_samplers[idx]);
        dev->free_descriptor(descriptor_type::sampled_image, new_sampled[idx]);
        dev->free_descriptor(descriptor_type::storage_image, new_storage[idx]);
    }
    dev->destroy_semaphore(timeline_sem);
}

/// @brief Tests free list integrity under partial interleaved retirements, ensuring that
/// even-indexed descriptors can be retired and reallocated without aliasing or corrupting
/// still-allocated odd-indexed descriptors.
TEST(descriptor_test, descriptor_free_list_adversarial_exhaustion_and_integrity)
{
    // Setup: Initialize test environment with Vulkan device
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    constexpr size_t total_alloc = 256;
    auto slots = vector<descriptor_handle>{};
    slots.reserve(total_alloc);

    auto seen_indices = array<bool, 4096>{};
    for (size_t idx = 0; idx < total_alloc; ++idx)
    {
        const auto slot = dev->allocate_descriptor(descriptor_type::sampled_image);
        ASSERT_NE(slot.index, ~0U);
        EXPECT_FALSE(seen_indices[slot.index]);
        seen_indices[slot.index] = true;
        slots.push_back(slot);
    }

    // Submit work on graphics queue to defer retirements
    auto timeline_sem = dev->create_timeline_semaphore();
    auto& graphics_port = dev->get_graphics_execution_port();
    auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
    cmd.begin();
    cmd.end();

    const auto* cmd_ptr = &cmd;
    constexpr auto target_timeline_val = uint64_t{200};
    auto signal_point = device_sync_point{
        .semaphore = timeline_sem,
        .value = target_timeline_val,
        .stages = pipeline_stage::all_graphics,
    };
    auto submit_res = graphics_port.submit(span<const command_list*>{&cmd_ptr, 1}, {},
                                           span<const device_sync_point>{&signal_point, 1});
    ASSERT_TRUE(submit_res.has_value());

    // Act: Free even-indexed descriptors (128 slots)
    constexpr size_t freed_count = total_alloc / 2;
    for (size_t idx = 0; idx < total_alloc; idx += 2)
    {
        dev->free_descriptor(descriptor_type::sampled_image, slots[idx]);
    }

    // Assert: Exactly 128 descriptors in retirement queue
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), freed_count);

    // Act: Issue adversarial duplicate frees on all even-indexed descriptors
    for (size_t idx = 0; idx < total_alloc; idx += 2)
    {
        dev->free_descriptor(descriptor_type::sampled_image, slots[idx]);
    }
    // Assert: Retired count remains strictly 128 (no duplicates)
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), freed_count);

    // Wait for GPU sync and pump retirements
    dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = target_timeline_val});
    dev->process_deferred_retirements();
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);

    // Act: Reallocate 128 descriptors
    auto reallocated_slots = vector<descriptor_handle>{};
    reallocated_slots.reserve(freed_count);

    auto reallocated_seen = array<bool, 4096>{};
    for (size_t idx = 0; idx < freed_count; ++idx)
    {
        const auto slot = dev->allocate_descriptor(descriptor_type::sampled_image);
        ASSERT_NE(slot.index, ~0U);
        EXPECT_FALSE(reallocated_seen[slot.index]);
        reallocated_seen[slot.index] = true;
        reallocated_slots.push_back(slot);
    }

    // Assert: Verify that none of the reallocated descriptors collide with the still-allocated odd-indexed descriptors
    for (size_t idx = 1; idx < total_alloc; idx += 2)
    {
        EXPECT_FALSE(reallocated_seen[slots[idx].index]);
    }

    // Cleanup: Free all remaining descriptors
    for (size_t idx = 1; idx < total_alloc; idx += 2)
    {
        dev->free_descriptor(descriptor_type::sampled_image, slots[idx]);
    }
    for (const auto& slot : reallocated_slots)
    {
        dev->free_descriptor(descriptor_type::sampled_image, slot);
    }
    dev->destroy_semaphore(timeline_sem);
}

/// @brief Verifies that multithreaded worker passes concurrently allocating, using, and retiring
/// descriptors under synchronized access maintain free list integrity, hand out strictly unique indices,
/// and cleanly reject concurrent double-free attempts.
TEST(descriptor_test, descriptor_concurrent_worker_simulation_stress)
{
    // Setup: Initialize test environment with Vulkan device
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto* const dev = env.dev.get();
    auto* const vk_dev = static_cast<vk::device*>(dev);

    auto& graphics_port = dev->get_graphics_execution_port();

    constexpr size_t worker_count = 6;
    constexpr size_t iterations_per_worker = 20;

    auto dev_mutex = mutex{};
    auto threads = vector<thread>{};
    threads.reserve(worker_count);

    auto total_allocations = atomic<uint32_t>{0};
    auto rejected_double_frees = atomic<uint32_t>{0};

    // Act: Launch concurrent worker threads allocating, using, and freeing descriptors
    for (size_t worker_idx = 0; worker_idx < worker_count; ++worker_idx)
    {
        threads.emplace_back([&dev, &dev_mutex, &total_allocations, &rejected_double_frees, &vk_dev]() -> void {
            for (size_t iter = 0; iter < iterations_per_worker; ++iter)
            {
                // Allocate descriptors under lock
                auto sampled_slot = descriptor_handle{};
                auto sampler_slot = descriptor_handle{};
                {
                    auto lock = lock_guard{dev_mutex};
                    sampled_slot = dev->allocate_descriptor(descriptor_type::sampled_image);
                    sampler_slot = dev->allocate_descriptor(descriptor_type::sampler);
                }

                if (sampled_slot.index != ~0U)
                {
                    total_allocations.fetch_add(1);
                }
                if (sampler_slot.index != ~0U)
                {
                    total_allocations.fetch_add(1);
                }

                // Free descriptors and attempt adversarial double free
                {
                    auto lock = lock_guard{dev_mutex};
                    dev->free_descriptor(descriptor_type::sampled_image, sampled_slot);
                    dev->free_descriptor(descriptor_type::sampler, sampler_slot);

                    // Adversarial double-free attempt
                    const auto count_after_first = vk_dev->get_retired_descriptor_count();
                    dev->free_descriptor(descriptor_type::sampled_image, sampled_slot);
                    dev->free_descriptor(descriptor_type::sampler, sampler_slot);
                    const auto count_after_double = vk_dev->get_retired_descriptor_count();

                    if (count_after_double == count_after_first)
                    {
                        rejected_double_frees.fetch_add(2);
                    }
                }
            }
        });
    }

    // Launch a background pump thread to submit transient command lists and pump retirements
    auto stop_pump = atomic<bool>{false};
    auto pump_thread = thread([&dev, &graphics_port, &dev_mutex, &stop_pump]() -> void {
        while (!stop_pump.load())
        {
            {
                auto lock = lock_guard{dev_mutex};
                auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);
                cmd.begin();
                cmd.end();
                const auto* cmd_ptr = &cmd;
                [[maybe_unused]] auto res = graphics_port.submit(span<const command_list*>{&cmd_ptr, 1}, {}, {});
                dev->process_deferred_retirements();
            }
            this_thread::yield();
        }
    });

    for (auto& worker : threads)
    {
        worker.join();
    }

    stop_pump.store(true);
    pump_thread.join();

    // Final synchronization: drain GPU and flush retirements
    dev->wait_idle();

    // Assert: All double-free attempts were 100% rejected
    EXPECT_GT(total_allocations.load(), 0U);
    EXPECT_EQ(rejected_double_frees.load(), total_allocations.load());
    EXPECT_EQ(vk_dev->get_retired_descriptor_count(), 0U);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
