#include <gtest/gtest.h>

#include <tempest/vk/context.hpp>
#include <tempest/vk/device.hpp>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#error "Unsupported platform for this test"
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace tempest::rhi::vk
{
    TEST(device_test, create_surface)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_TRUE(device != nullptr);

        // Create a GLFW window
        if (glfwInit() != GLFW_TRUE)
        {
            FAIL() << "Failed to initialize GLFW";
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        auto* window = glfwCreateWindow(800, 600, "Tempest Test Window", nullptr, nullptr);
        ASSERT_NE(window, nullptr);

#ifdef TEMPEST_PLATFORM_WINDOWS
        auto native_window_handle = native_wsi_handle{
            .display = GetModuleHandle(nullptr),
            .window = glfwGetWin32Window(window),
        };
#else
        auto native_window_handle = native_wsi_handle{};
#endif

        auto surface_result = device->create_raw_surface(native_window_handle);
        ASSERT_TRUE(surface_result.has_value());

        auto surface_handle = tempest::move(surface_result).value();
        // We can't make any assertions about the surface handle itself

        // Clean up
        device->destroy_raw_surface(surface_handle);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    TEST(device_test, query_surface_capabilities)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_TRUE(device != nullptr);

        // Create a GLFW window
        if (glfwInit() != GLFW_TRUE)
        {
            FAIL() << "Failed to initialize GLFW";
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        auto* window = glfwCreateWindow(800, 600, "Tempest Test Window", nullptr, nullptr);
        ASSERT_NE(window, nullptr);

#ifdef TEMPEST_PLATFORM_WINDOWS
        auto native_window_handle = native_wsi_handle{
            .display = GetModuleHandle(nullptr),
            .window = glfwGetWin32Window(window),
        };
#else
        auto native_window_handle = native_wsi_handle{};
#endif

        auto surface_result = device->create_raw_surface(native_window_handle);
        ASSERT_TRUE(surface_result.has_value());

        auto surface_handle = tempest::move(surface_result).value();

        auto capabilities = device->get_surface_capabilities(surface_handle);
        // We can't make any assertions about the specific capabilities, but we can check that the returned structure is
        // valid
        ASSERT_GT(capabilities.min_image_count, 0);
        ASSERT_GE(capabilities.max_image_count, capabilities.min_image_count);
        ASSERT_GT(capabilities.max_image_array_layers, 0);
        ASSERT_FALSE(capabilities.supported_formats.empty());

        // Clean up
        device->destroy_raw_surface(surface_handle);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    TEST(device_test, create_render_surface)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_TRUE(device != nullptr);

        // Create a GLFW window
        if (glfwInit() != GLFW_TRUE)
        {
            FAIL() << "Failed to initialize GLFW";
        }

        const auto window_width = 800;
        const auto window_height = 600;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        auto* window = glfwCreateWindow(window_width, window_height, "Tempest Test Window", nullptr, nullptr);
        ASSERT_NE(window, nullptr);

#ifdef TEMPEST_PLATFORM_WINDOWS
        auto native_window_handle = native_wsi_handle{
            .display = GetModuleHandle(nullptr),
            .window = glfwGetWin32Window(window),
        };
#else
        auto native_window_handle = native_wsi_handle{};
#endif
        auto surface_result = device->create_raw_surface(native_window_handle);
        ASSERT_TRUE(surface_result.has_value());

        auto surface_handle = tempest::move(surface_result).value();
        auto capabilities = device->get_surface_capabilities(surface_handle);

        // Just pick the first supported format and present mode for testing
        auto format = capabilities.supported_formats[0];
        auto present_mode = capabilities.supported_present_modes[0];

        // Get the framebuffer size of the window
        auto framebuffer_width = 0;
        auto framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

        auto render_surface_desc = rhi::render_surface_desc{
            .raw_surface = surface_handle,
            .present_mode = present_mode,
            .format = format,
            .width = static_cast<uint32_t>(framebuffer_width),
            .height = static_cast<uint32_t>(framebuffer_height),
            .min_image_count = capabilities.min_image_count,
            .preferred_image_count = capabilities.min_image_count + 1,
        };

        auto render_surface = device->create_render_surface(render_surface_desc);
        // Note: swapchain wrapper will be implemented in presentation phase
        if (render_surface != nullptr)
        {
            device->destroy_render_surface(tempest::move(render_surface));
        }

        // Clean up
        device->destroy_raw_surface(surface_handle);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    TEST(device_test, create_buffer_device_only)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_NE(device, nullptr);

        auto buf_desc = buffer_desc{
            .size = 1024,
            .memory_usage = memory_usage::device_only,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
        };

        auto buf = device->create_buffer(buf_desc);
        EXPECT_NE(buf.handle, 0ULL);
        EXPECT_NE(buf.gpu_address, 0ULL);
        EXPECT_EQ(buf.cpu_address, nullptr);

        device->destroy_buffer(buf);
    }

    TEST(device_test, create_buffer_upload)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_NE(device, nullptr);

        auto buf_desc = buffer_desc{
            .size = 1024,
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
        };

        auto buf = device->create_buffer(buf_desc);
        EXPECT_NE(buf.handle, 0ULL);
        EXPECT_NE(buf.gpu_address, 0ULL);
        EXPECT_NE(buf.cpu_address, nullptr);

        // Write test data to mapped memory
        auto* data_ptr = static_cast<uint32_t*>(buf.cpu_address);
        for (auto i = 0U; i < 256; ++i)
        {
            data_ptr[i] = i * 42;
        }

        // Verify read back from mapped pointer
        for (auto i = 0U; i < 256; ++i)
        {
            EXPECT_EQ(data_ptr[i], i * 42);
        }

        device->destroy_buffer(buf);
    }

    TEST(device_test, create_buffer_readback)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_NE(device, nullptr);

        auto buf_desc = buffer_desc{
            .size = 512,
            .memory_usage = memory_usage::readback,
            .usage = buffer_usage::transfer_dst,
        };

        auto buf = device->create_buffer(buf_desc);
        EXPECT_NE(buf.handle, 0ULL);
        EXPECT_NE(buf.cpu_address, nullptr);

        device->destroy_buffer(buf);
    }

    TEST(device_test, create_texture_and_view)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_NE(device, nullptr);

        auto tex_desc = texture_desc{
            .width = 256,
            .height = 256,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .format = data_format::rgba8_unorm,
            .memory_usage = memory_usage::device_only,
            .usage = texture_usage::color_attachment | texture_usage::sampled,
        };

        auto tex = device->create_texture(tex_desc);
        EXPECT_NE(tex.handle, 0ULL);

        auto view_desc = texture_view_desc{
            .override_format = nullopt,
            .base_mip_level = 0,
            .mip_level_count = 1,
            .base_array_layer = 0,
            .array_layer_count = 1,
        };

        auto view = device->create_texture_view(tex, view_desc);
        EXPECT_NE(view.handle, 0ULL);

        device->destroy_texture_view(view);
        device->destroy_texture(tex);
    }

    TEST(device_test, create_sampler)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_NE(device, nullptr);

        auto samp_desc = sampler_desc{
            .min_filter = filter_mode::linear,
            .mag_filter = filter_mode::linear,
            .mipmap_mode = mipmap_mode::linear,
            .address_u = address_mode::repeat,
            .address_v = address_mode::repeat,
            .address_w = address_mode::repeat,
            .mip_lod_bias = 0.0F,
            .min_lod = 0.0F,
            .max_lod = 1000.0F,
            .max_anisotropy = 16.0F,
            .compare_op = nullopt,
        };

        auto samp = device->create_sampler(samp_desc);
        EXPECT_NE(samp.handle, 0ULL);

        device->destroy_sampler(samp);
    }

    TEST(device_test, create_semaphores_and_event)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_NE(device, nullptr);

        auto timeline_sem = device->create_timeline_semaphore();
        EXPECT_NE(timeline_sem.handle, 0ULL);

        auto binary_sem = device->create_binary_semaphore();
        EXPECT_NE(binary_sem.handle, 0ULL);

        auto evt = device->create_event();
        EXPECT_NE(evt.handle, 0ULL);

        device->destroy_semaphore(timeline_sem);
        device->destroy_semaphore(binary_sem);
        device->destroy_event(evt);
    }

    TEST(device_test, create_depth_texture_and_view)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_NE(device, nullptr);

        auto tex_desc = texture_desc{
            .width = 1920,
            .height = 1080,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .format = data_format::depth32_float,
            .memory_usage = memory_usage::device_only,
            .usage = texture_usage::depth_stencil_attachment | texture_usage::sampled,
        };

        auto tex = device->create_texture(tex_desc);
        EXPECT_NE(tex.handle, 0ULL);

        auto view_desc = texture_view_desc{
            .override_format = nullopt,
            .base_mip_level = 0,
            .mip_level_count = 1,
            .base_array_layer = 0,
            .array_layer_count = 1,
        };

        auto view = device->create_texture_view(tex, view_desc);
        EXPECT_NE(view.handle, 0ULL);

        device->destroy_texture_view(view);
        device->destroy_texture(tex);
    }

    TEST(device_test, multiple_resource_allocations)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_NE(device, nullptr);

        auto buffers = vector<buffer_handle>{};
        buffers.reserve(64);
        for (auto i = 0U; i < 64; ++i)
        {
            auto buf = device->create_buffer(buffer_desc{
                .size = 256 * (i + 1),
                .memory_usage = (i % 2 == 0) ? memory_usage::device_only : memory_usage::upload,
                .usage = buffer_usage::storage_buffer | buffer_usage::device_address,
            });
            EXPECT_NE(buf.handle, 0ULL);
            buffers.push_back(buf);
        }

        for (auto buf : buffers)
        {
            device->destroy_buffer(buf);
        }
    }

    TEST(device_test, create_compute_storage_texture)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_NE(device, nullptr);

        auto tex_desc = texture_desc{
            .width = 512,
            .height = 512,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .format = data_format::rgba16_float,
            .memory_usage = memory_usage::device_only,
            .usage = texture_usage::storage | texture_usage::sampled,
        };

        auto tex = device->create_texture(tex_desc);
        EXPECT_NE(tex.handle, 0ULL);

        auto view = device->create_texture_view(tex, texture_view_desc{});
        EXPECT_NE(view.handle, 0ULL);

        device->destroy_texture_view(view);
        device->destroy_texture(tex);
    }

    TEST(device_test, object_debug_naming_and_markers)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device = context->create_device(devices[0].device_uuid);
        ASSERT_NE(device, nullptr);

        // Test buffer with initial name and dynamic rename
        auto buf = device->create_buffer(buffer_desc{
            .size = 1024,
            .memory_usage = memory_usage::upload,
            .usage = buffer_usage::storage_buffer,
            .name = "InitialBufferName",
        });
        EXPECT_NE(buf.handle, 0ULL);
        device->set_debug_name(buf, "RenamedBuffer");

        // Test texture with initial name and dynamic rename
        auto tex = device->create_texture(texture_desc{
            .width = 64,
            .height = 64,
            .format = data_format::rgba8_unorm,
            .name = "InitialTextureName",
        });
        EXPECT_NE(tex.handle, 0ULL);
        device->set_debug_name(tex, "RenamedTexture");

        auto view = device->create_texture_view(tex, texture_view_desc{});
        EXPECT_NE(view.handle, 0ULL);
        device->set_debug_name(view, "TextureView");

        auto samp = device->create_sampler(sampler_desc{
            .min_filter = filter_mode::linear,
            .mag_filter = filter_mode::linear,
            .mipmap_mode = mipmap_mode::linear,
            .address_u = address_mode::repeat,
            .address_v = address_mode::repeat,
            .address_w = address_mode::repeat,
            .name = "LinearSampler",
        });
        EXPECT_NE(samp.handle, 0ULL);
        device->set_debug_name(samp, "RenamedSampler");

        auto evt = device->create_event();
        EXPECT_NE(evt.handle, 0ULL);
        device->set_debug_name(evt, "TestEvent");

        auto sem = device->create_timeline_semaphore();
        EXPECT_NE(sem.handle, 0ULL);
        device->set_debug_name(sem, "TestSemaphore");

        // Test command list and queue debug regions and markers
        auto& port = device->get_graphics_execution_port();
        port.begin_debug_region(debug_label{.name = "QueueTestRegion"});
        port.insert_debug_marker(debug_label{.name = "QueueMarker"});
        port.end_debug_region();

        auto& cmd = port.acquire_command_list(0);
        cmd.begin();
        cmd.begin_debug_region(debug_label{.name = "CmdTestRegion", .color = {1.0F, 0.0F, 0.0F, 1.0F}});
        cmd.insert_debug_marker(debug_label{.name = "CmdMarker", .color = {0.0F, 1.0F, 0.0F, 1.0F}});
        cmd.end_debug_region();
        cmd.end();

        // Clean up
        device->destroy_sampler(samp);
        device->destroy_texture_view(view);
        device->destroy_texture(tex);
        device->destroy_buffer(buf);
        device->destroy_event(evt);
        device->destroy_semaphore(sem);
    }

    TEST(device_test, get_device_desc)
    {
        auto ctx_desc = context_desc{
            .application_name = "Tempest Device Desc Test",
            .api = graphics_api::vulkan,
        };

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        const auto& enumerated_desc = devices[0];
        EXPECT_FALSE(enumerated_desc.name.empty());
        EXPECT_GE(enumerated_desc.limits.max_image_dimension_2d, 4096U);
        EXPECT_GT(enumerated_desc.limits.max_uniform_buffer_range, 0U);
        EXPECT_GT(enumerated_desc.limits.max_storage_buffer_range, 0U);

        auto device = context->create_device(enumerated_desc.device_uuid);
        ASSERT_NE(device, nullptr);

        const auto& dev_desc = device->get_device_desc();
        EXPECT_EQ(dev_desc.name, enumerated_desc.name);
        EXPECT_EQ(dev_desc.vendor, enumerated_desc.vendor);
        EXPECT_EQ(dev_desc.type, enumerated_desc.type);
        EXPECT_EQ(dev_desc.limits.max_image_dimension_2d, enumerated_desc.limits.max_image_dimension_2d);
        EXPECT_EQ(dev_desc.limits.max_storage_buffer_range, enumerated_desc.limits.max_storage_buffer_range);
        EXPECT_GE(dev_desc.limits.max_image_dimension_2d, 4096U);
    }
} // namespace tempest::rhi::vk