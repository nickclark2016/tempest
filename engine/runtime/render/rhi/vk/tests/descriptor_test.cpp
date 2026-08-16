#include <gtest/gtest.h>
#include <tempest/guid.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>
#include <tempest/vk/context.hpp>

#include <cstring>

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
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Descriptor Test";
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

TEST(descriptor_test, descriptor_slot_lifecycle)
{
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    // Allocate multiple sampler descriptors
    auto s1 = dev->allocate_descriptor(descriptor_type::sampler);
    auto s2 = dev->allocate_descriptor(descriptor_type::sampler);
    EXPECT_NE(s1.index, s2.index);

    // Allocate multiple sampled image descriptors
    auto t1 = dev->allocate_descriptor(descriptor_type::sampled_image);
    auto t2 = dev->allocate_descriptor(descriptor_type::sampled_image);
    EXPECT_NE(t1.index, t2.index);

    // Allocate multiple storage image descriptors
    auto u1 = dev->allocate_descriptor(descriptor_type::storage_image);
    auto u2 = dev->allocate_descriptor(descriptor_type::storage_image);
    EXPECT_NE(u1.index, u2.index);

    // Free and reallocate
    dev->free_descriptor(descriptor_type::sampler, s1);
    auto s3 = dev->allocate_descriptor(descriptor_type::sampler);
    EXPECT_EQ(s3.index, s1.index);
    EXPECT_GT(s3.generation, s1.generation);

    dev->free_descriptor(descriptor_type::sampled_image, t1);
    dev->free_descriptor(descriptor_type::storage_image, u1);
}

TEST(descriptor_test, bindless_storage_image_write)
{
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    // 1. Create storage texture (64x64 RGBA8)
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

    // 2. Allocate and write storage image descriptor
    auto storage_slot = dev->allocate_descriptor(descriptor_type::storage_image);
    ASSERT_NE(storage_slot.index, ~0U);
    dev->write_storage_image_descriptor(storage_slot, view, image_layout::general);

    // 3. Create readback buffer
    auto readback_desc = buffer_desc{
        .size = width * height * 4,
        .memory_usage = memory_usage::readback,
        .usage = buffer_usage::transfer_dst,
        .name = "storage_readback_buffer",
    };
    auto readback_buf = dev->create_buffer(readback_desc);
    ASSERT_NE(readback_buf.handle, 0ULL);
    ASSERT_NE(readback_buf.cpu_address, nullptr);

    // 4. Create compute pipeline
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

    // 5. Record commands
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

    // 6. Verify written values in readback buffer
    auto readback_data = static_cast<const uint8_t*>(readback_buf.cpu_address);
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

    // 1. Create a 2x2 RGBA8 texture with known colors:
    // (0,0) Red (255, 0, 0, 255)
    // (1,0) Green (0, 255, 0, 255)
    // (0,1) Blue (0, 0, 255, 255)
    // (1,1) White (255, 255, 255, 255)
    constexpr uint32_t tex_w = 2;
    constexpr uint32_t tex_h = 2;
    uint8_t tex_pixels[16] = {
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
    std::memcpy(upload_buf.cpu_address, tex_pixels, sizeof(tex_pixels));

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

    // 2. Allocate descriptors and write descriptors
    auto texture_slot = dev->allocate_descriptor(descriptor_type::sampled_image);
    ASSERT_NE(texture_slot.index, ~0U);
    dev->write_sampled_image_descriptor(texture_slot, view, image_layout::general);

    auto sampler_slot = dev->allocate_descriptor(descriptor_type::sampler);
    ASSERT_NE(sampler_slot.index, ~0U);
    dev->write_sampler_descriptor(sampler_slot, samp);

    // 3. Create GPU output buffer for sampling results (2x2 float4s = 64 bytes)
    constexpr uint32_t sample_w = 2;
    constexpr uint32_t sample_h = 2;
    auto out_desc = buffer_desc{
        .size = sample_w * sample_h * sizeof(float) * 4,
        .memory_usage = memory_usage::readback,
        .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
        .name = "sample_output_buf",
    };
    auto out_buf = dev->create_buffer(out_desc);
    ASSERT_NE(out_buf.handle, 0ULL);
    ASSERT_NE(out_buf.cpu_address, nullptr);

    // 4. Create compute pipeline
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

    // 5. Upload texture and execute compute sampling
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

    // 6. Verify sampled floats in output buffer
    auto out_data = static_cast<const float*>(out_buf.cpu_address);
    ASSERT_NE(out_data, nullptr);

    // Pixel (0,0) should be Red: (1.0, 0.0, 0.0, 1.0)
    EXPECT_NEAR(out_data[0], 1.0f, 0.01f);
    EXPECT_NEAR(out_data[1], 0.0f, 0.01f);
    EXPECT_NEAR(out_data[2], 0.0f, 0.01f);
    EXPECT_NEAR(out_data[3], 1.0f, 0.01f);

    // Pixel (1,0) should be Green: (0.0, 1.0, 0.0, 1.0)
    EXPECT_NEAR(out_data[4], 0.0f, 0.01f);
    EXPECT_NEAR(out_data[5], 1.0f, 0.01f);
    EXPECT_NEAR(out_data[6], 0.0f, 0.01f);
    EXPECT_NEAR(out_data[7], 1.0f, 0.01f);

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
