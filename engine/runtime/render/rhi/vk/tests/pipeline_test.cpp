#include <gtest/gtest.h>

#include <tempest/array.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>
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
            ctx_desc.application_name = "Tempest Pipeline Test";
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

        struct compute_push_constants
        {
            uint64_t input_buffer;
            uint64_t output_buffer;
            uint32_t count;
        };

        struct raster_push_constants
        {
            float r;
            float g;
            float b;
            float a;
        };
    } // namespace

    TEST(pipeline_test, compute_pipeline_bda_execution)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto& dev = env.dev;

        constexpr size_t element_count = 64;
        constexpr size_t buffer_byte_size = element_count * sizeof(uint32_t);

        auto upload_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::transfer_src,
        };
        auto upload_buffer = dev->create_buffer(upload_desc);
        ASSERT_NE(upload_buffer.handle, 0ULL);
        ASSERT_NE(upload_buffer.cpu_address, nullptr);

        auto in_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::device_only,
            .usage = buffer_usage::transfer_dst | buffer_usage::storage_buffer | buffer_usage::device_address,
        };
        auto in_buffer = dev->create_buffer(in_desc);
        ASSERT_NE(in_buffer.handle, 0ULL);
        ASSERT_NE(in_buffer.gpu_address, 0ULL);

        auto out_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::device_only,
            .usage = buffer_usage::transfer_src | buffer_usage::storage_buffer | buffer_usage::device_address,
        };
        auto out_buffer = dev->create_buffer(out_desc);
        ASSERT_NE(out_buffer.handle, 0ULL);
        ASSERT_NE(out_buffer.gpu_address, 0ULL);

        auto readback_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::readback,
            .usage = buffer_usage::transfer_dst,
        };
        auto readback_buffer = dev->create_buffer(readback_desc);
        ASSERT_NE(readback_buffer.handle, 0ULL);
        ASSERT_NE(readback_buffer.cpu_address, nullptr);

        // Fill initial data
        auto* upload_ptr = static_cast<uint32_t*>(upload_buffer.cpu_address);
        for (size_t i = 0; i < element_count; ++i)
        {
            upload_ptr[i] = static_cast<uint32_t>(i + 2);
        }

        auto* readback_ptr = static_cast<uint32_t*>(readback_buffer.cpu_address);
        for (size_t i = 0; i < element_count; ++i)
        {
            readback_ptr[i] = 0;
        }

        // Create compute pipeline
        auto sm_desc = shader_module_desc{
            .stage = shader_stage::compute,
            .ir_code = span<const byte>{reinterpret_cast<const byte*>(shaders::compute::test_compute_spv),
                                        sizeof(shaders::compute::test_compute_spv)},
            .entry_point = "CSMain",
        };
        auto pipe = dev->create_compute_pipeline(compute_pipeline_desc{.shader_module = sm_desc});
        ASSERT_NE(pipe.handle, 0ULL);

        // Record commands
        auto& compute_port = dev->get_async_compute_execution_port();
        auto& cmd = compute_port.acquire_command_list(0, command_list_lifetime::transient);

        cmd.begin();

        auto copy_region = buffer_copy_region{
            .src_offset = 0,
            .dst_offset = 0,
            .size = buffer_byte_size,
        };
        cmd.copy_buffer(upload_buffer, in_buffer, span<const buffer_copy_region>{&copy_region, 1});

        auto upload_barrier = buffer_barrier{
            .buffer = in_buffer,
            .src =
                {
                    .stages = pipeline_stage::copy,
                    .access = resource_access::write,
                },
            .dst =
                {
                    .stages = pipeline_stage::compute,
                    .access = resource_access::read,
                },
            .offset = 0,
            .size = buffer_byte_size,
        };
        cmd.pipeline_barrier({}, span<const buffer_barrier>{&upload_barrier, 1});

        cmd.bind_pipeline(pipe);

        auto constants = compute_push_constants{
            .input_buffer = in_buffer.gpu_address,
            .output_buffer = out_buffer.gpu_address,
            .count = static_cast<uint32_t>(element_count),
        };
        cmd.push_constants(shader_stage::compute, 0,
                           span<const byte>{reinterpret_cast<const byte*>(&constants), sizeof(constants)});

        cmd.dispatch(1, 1, 1);

        auto compute_barrier = buffer_barrier{
            .buffer = out_buffer,
            .src =
                {
                    .stages = pipeline_stage::compute,
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
        cmd.pipeline_barrier({}, span<const buffer_barrier>{&compute_barrier, 1});

        cmd.copy_buffer(out_buffer, readback_buffer, span<const buffer_copy_region>{&copy_region, 1});
        cmd.end();

        auto timeline_sem = dev->create_timeline_semaphore();
        const auto* cmd_ptr = &cmd;
        auto signal_sync = device_sync_point{
            .semaphore = timeline_sem,
            .value = 1,
            .stages = pipeline_stage::copy,
        };

        auto submit_result = compute_port.submit(span<const rhi::command_list*>{&cmd_ptr, 1}, {},
                                                 span<const device_sync_point>{&signal_sync, 1});
        ASSERT_TRUE(submit_result.has_value());

        dev->wait_for_sync(host_sync_point{.semaphore = timeline_sem, .value = 1});

        // Verify out[i] == in[i] * in[i] + 1
        for (size_t i = 0; i < element_count; ++i)
        {
            auto in_val = static_cast<uint32_t>(i + 2);
            EXPECT_EQ(readback_ptr[i], in_val * in_val + 1);
        }

        // Cleanup
        dev->destroy_semaphore(timeline_sem);
        dev->destroy_compute_pipeline(pipe);
        dev->destroy_buffer(readback_buffer);
        dev->destroy_buffer(out_buffer);
        dev->destroy_buffer(in_buffer);
        dev->destroy_buffer(upload_buffer);
    }

    TEST(pipeline_test, graphics_pipeline_dynamic_rendering)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto& dev = env.dev;

        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;
        constexpr size_t buffer_byte_size = width * height * 4;

        // Create target texture
        auto tex_desc = texture_desc{
            .width = width,
            .height = height,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .format = data_format::rgba8_unorm,
            .usage = texture_usage::color_attachment | texture_usage::transfer_src,
        };
        auto color_tex = dev->create_texture(tex_desc);
        ASSERT_NE(color_tex.handle, 0ULL);

        auto view_desc = texture_view_desc{
            .base_mip_level = 0,
            .mip_level_count = 1,
            .base_array_layer = 0,
            .array_layer_count = 1,
        };
        auto color_view = dev->create_texture_view(color_tex, view_desc);
        ASSERT_NE(color_view.handle, 0ULL);

        auto readback_desc = buffer_desc{
            .size = buffer_byte_size,
            .memory_usage = memory_usage::readback,
            .usage = buffer_usage::transfer_dst,
        };
        auto readback_buffer = dev->create_buffer(readback_desc);
        ASSERT_NE(readback_buffer.handle, 0ULL);
        ASSERT_NE(readback_buffer.cpu_address, nullptr);

        // Create graphics pipeline
        auto vs_desc = shader_module_desc{
            .stage = shader_stage::vertex,
            .ir_code = span<const byte>{reinterpret_cast<const byte*>(shaders::raster::vs::test_raster_vs_spv),
                                        sizeof(shaders::raster::vs::test_raster_vs_spv)},
            .entry_point = "VSMain",
        };
        auto fs_desc = shader_module_desc{
            .stage = shader_stage::fragment,
            .ir_code = span<const byte>{reinterpret_cast<const byte*>(shaders::raster::fs::test_raster_fs_spv),
                                        sizeof(shaders::raster::fs::test_raster_fs_spv)},
            .entry_point = "FSMain",
        };
        auto stages = array{vs_desc, fs_desc};
        auto color_formats = array{data_format::rgba8_unorm};

        auto pipe_desc = graphics_pipeline_desc{
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
        auto pipe = dev->create_graphics_pipeline(pipe_desc);
        ASSERT_NE(pipe.handle, 0ULL);

        // Record rendering commands
        auto& graphics_port = dev->get_graphics_execution_port();
        auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);

        cmd.begin();

        auto init_barrier = texture_barrier{
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

        auto color_att = color_attachment{
            .view = color_view,
            .load_op = load_op::clear,
            .store_op = store_op::store,
            .clear_value =
                clear_color_value{
                    .r = 0.2f,
                    .g = 0.2f,
                    .b = 0.2f,
                    .a = 1.0f,
                },
        };

        cmd.begin_render_pass(span<const color_attachment>{&color_att, 1}, nullopt, width, height);
        cmd.set_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
        cmd.set_scissor(0, 0, width, height);
        cmd.bind_pipeline(pipe);

        auto tint = raster_push_constants{
            .r = 1.0f,
            .g = 1.0f,
            .b = 1.0f,
            .a = 1.0f,
        };
        cmd.push_constants(shader_stage::vertex, 0,
                           span<const byte>{reinterpret_cast<const byte*>(&tint), sizeof(tint)});

        cmd.draw(3, 1, 0, 0);
        cmd.end_render_pass();

        auto transfer_barrier = texture_barrier{
            .texture = color_tex,
            .src =
                {
                    .stages = pipeline_stage::attachment_output,
                    .access = resource_access::write,
                    .layout = image_layout::general,
                },
            .dst =
                {
                    .stages = pipeline_stage::copy,
                    .access = resource_access::read,
                    .layout = image_layout::general,
                },
        };
        cmd.pipeline_barrier(span<const texture_barrier>{&transfer_barrier, 1}, {});

        auto copy_reg = buffer_texture_copy_region{
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
        cmd.copy_texture_to_buffer(color_tex, readback_buffer, span<const buffer_texture_copy_region>{&copy_reg, 1});
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

        // Verify pixel data (center pixel around 32, 32 should have drawn triangle colors)
        auto* pixels = static_cast<const uint8_t*>(readback_buffer.cpu_address);
        size_t center_idx = (32 * width + 32) * 4;
        // Non-zero rendered color in the triangle interior
        EXPECT_GT(pixels[center_idx + 0] + pixels[center_idx + 1] + pixels[center_idx + 2], 0);

        // Cleanup
        dev->destroy_semaphore(timeline_sem);
        dev->destroy_graphics_pipeline(pipe);
        dev->destroy_buffer(readback_buffer);
        dev->destroy_texture_view(color_view);
        dev->destroy_texture(color_tex);
    }

    TEST(pipeline_test, pipeline_destruction_and_recreation)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        auto& dev = env.dev;

        auto sm_desc = shader_module_desc{
            .stage = shader_stage::compute,
            .ir_code = span<const byte>{reinterpret_cast<const byte*>(shaders::compute::test_compute_spv),
                                        sizeof(shaders::compute::test_compute_spv)},
            .entry_point = "CSMain",
        };

        for (size_t iter = 0; iter < 5; ++iter)
        {
            auto pipe = dev->create_compute_pipeline(compute_pipeline_desc{.shader_module = sm_desc});
            ASSERT_NE(pipe.handle, 0ULL);
            dev->destroy_compute_pipeline(pipe);
        }
    }
} // namespace tempest::rhi::vk
