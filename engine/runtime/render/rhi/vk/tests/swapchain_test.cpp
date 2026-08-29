#include <gtest/gtest.h>
#include <tempest/guid.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>
#include <tempest/vk/context.hpp>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif defined(TEMPEST_PLATFORM_LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

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
        ctx_desc.application_name = "Tempest Swapchain Test";
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

TEST(swapchain_test, create_and_acquire)
{
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    ASSERT_EQ(glfwInit(), GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto* window = glfwCreateWindow(640, 480, "Tempest Swapchain Test", nullptr, nullptr);
    ASSERT_NE(window, nullptr);

#if defined(TEMPEST_PLATFORM_WINDOWS)
    auto native_handle = native_wsi_handle{
        .display = GetModuleHandle(nullptr),
        .window = glfwGetWin32Window(window),
    };
#elif defined(TEMPEST_PLATFORM_LINUX)
    auto native_handle = native_wsi_handle{
        .display = static_cast<void*>(glfwGetX11Display()),
        .window = reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(window))),
    };
#else
    auto native_handle = native_wsi_handle{};
#endif

    auto raw_res = dev->create_raw_surface(native_handle);
    ASSERT_TRUE(raw_res.has_value());
    auto raw_surf = raw_res.value();

    auto caps = dev->get_surface_capabilities(raw_surf);
    ASSERT_FALSE(caps.supported_formats.empty());
    ASSERT_FALSE(caps.supported_present_modes.empty());

    auto surf_desc = render_surface_desc{
        .raw_surface = raw_surf,
        .present_mode = caps.supported_present_modes[0],
        .format = caps.supported_formats[0],
        .width = 640,
        .height = 480,
        .min_image_count = caps.min_image_count,
        .preferred_image_count = caps.min_image_count + 1,
    };

    auto surf = dev->create_render_surface(surf_desc);
    ASSERT_NE(surf, nullptr);
    EXPECT_EQ(surf->get_width(), 640);
    EXPECT_EQ(surf->get_height(), 480);

    auto acquire_sem = dev->create_binary_semaphore();
    ASSERT_NE(acquire_sem.handle, 0ULL);

    auto acquire_res = surf->acquire_next_image(device_sync_point{
        .semaphore = acquire_sem,
        .value = 0,
        .stages = pipeline_stage::all_graphics,
    });
    ASSERT_TRUE(acquire_res.has_value());

    auto sc_img = acquire_res.value();
    EXPECT_NE(sc_img.texture.handle, 0ULL);
    EXPECT_NE(sc_img.view.handle, 0ULL);

    dev->wait_idle();
    dev->destroy_semaphore(acquire_sem);
    dev->destroy_render_surface(tempest::move(surf));
    dev->destroy_raw_surface(raw_surf);
    glfwDestroyWindow(window);
    glfwTerminate();
}

TEST(swapchain_test, acquire_render_and_present)
{
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    ASSERT_EQ(glfwInit(), GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto* window = glfwCreateWindow(800, 600, "Tempest Present Test", nullptr, nullptr);
    ASSERT_NE(window, nullptr);

#if defined(TEMPEST_PLATFORM_WINDOWS)
    auto native_handle = native_wsi_handle{
        .display = GetModuleHandle(nullptr),
        .window = glfwGetWin32Window(window),
    };
#elif defined(TEMPEST_PLATFORM_LINUX)
    auto native_handle = native_wsi_handle{
        .display = static_cast<void*>(glfwGetX11Display()),
        .window = reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(window))),
    };
#else
    auto native_handle = native_wsi_handle{};
#endif

    auto raw_res = dev->create_raw_surface(native_handle);
    ASSERT_TRUE(raw_res.has_value());
    auto raw_surf = raw_res.value();

    auto caps = dev->get_surface_capabilities(raw_surf);
    auto surf_desc = render_surface_desc{
        .raw_surface = raw_surf,
        .present_mode = caps.supported_present_modes[0],
        .format = caps.supported_formats[0],
        .width = 800,
        .height = 600,
        .min_image_count = caps.min_image_count,
        .preferred_image_count = caps.min_image_count + 1,
    };

    auto surf = dev->create_render_surface(surf_desc);
    ASSERT_NE(surf, nullptr);

    auto acquire_sem = dev->create_binary_semaphore();
    auto render_sem = dev->create_binary_semaphore();

    // 1. Acquire swapchain image
    auto acquire_res = surf->acquire_next_image(device_sync_point{
        .semaphore = acquire_sem,
        .value = 0,
        .stages = pipeline_stage::attachment_output,
    });
    ASSERT_TRUE(acquire_res.has_value());
    auto sc_img = acquire_res.value();

    // 2. Clear swapchain image to solid blue and transition to present layout
    auto& graphics_port = dev->get_graphics_execution_port();
    auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);

    cmd.begin();

    auto init_barrier = texture_barrier{
        .texture = sc_img.texture,
        .src =
            {
                .stages = pipeline_stage::attachment_output,
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
        .view = sc_img.view,
        .load_op = load_op::clear,
        .store_op = store_op::store,
        .clear_value =
            clear_color_value{
                .r = 0.0f,
                .g = 0.4f,
                .b = 0.8f,
                .a = 1.0f,
            },
    };
    cmd.begin_render_pass(span<const color_attachment>{&color_att, 1}, nullopt, 800, 600);
    cmd.end_render_pass();

    auto present_barrier = texture_barrier{
        .texture = sc_img.texture,
        .src =
            {
                .stages = pipeline_stage::attachment_output,
                .access = resource_access::write,
                .layout = image_layout::general,
            },
        .dst =
            {
                .stages = pipeline_stage::bottom_of_pipe,
                .access = resource_access::none,
                .layout = image_layout::present,
            },
    };
    cmd.pipeline_barrier(span<const texture_barrier>{&present_barrier, 1}, {});

    cmd.end();

    // 3. Submit command buffer waiting on acquire_sem and signaling render_sem
    auto wait_point = device_sync_point{
        .semaphore = acquire_sem,
        .value = 0,
        .stages = pipeline_stage::attachment_output,
    };
    auto signal_point = device_sync_point{
        .semaphore = render_sem,
        .value = 0,
        .stages = pipeline_stage::bottom_of_pipe,
    };

    const auto* cmd_ptr = &cmd;
    auto submit_res =
        graphics_port.submit(span<const command_list*>{&cmd_ptr, 1}, span<const device_sync_point>{&wait_point, 1},
                             span<const device_sync_point>{&signal_point, 1});
    ASSERT_TRUE(submit_res.has_value());

    // 4. Present image
    auto present_res = surf->present(device_sync_point{
        .semaphore = render_sem,
        .value = 0,
    });
    EXPECT_TRUE(present_res.has_value() || present_res.error() == swapchain_error::suboptimal);

    dev->wait_idle();
    dev->destroy_semaphore(acquire_sem);
    dev->destroy_semaphore(render_sem);
    dev->destroy_render_surface(tempest::move(surf));
    dev->destroy_raw_surface(raw_surf);
    glfwDestroyWindow(window);
    glfwTerminate();
}

TEST(swapchain_test, swapchain_recreation_handover)
{
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    ASSERT_EQ(glfwInit(), GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto* window = glfwCreateWindow(640, 480, "Tempest Recreate Test", nullptr, nullptr);
    ASSERT_NE(window, nullptr);

#if defined(TEMPEST_PLATFORM_WINDOWS)
    auto native_handle = native_wsi_handle{
        .display = GetModuleHandle(nullptr),
        .window = glfwGetWin32Window(window),
    };
#elif defined(TEMPEST_PLATFORM_LINUX)
    auto native_handle = native_wsi_handle{
        .display = static_cast<void*>(glfwGetX11Display()),
        .window = reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(window))),
    };
#else
    auto native_handle = native_wsi_handle{};
#endif

    auto raw_res = dev->create_raw_surface(native_handle);
    ASSERT_TRUE(raw_res.has_value());
    auto raw_surf = raw_res.value();

    auto caps = dev->get_surface_capabilities(raw_surf);

    // 1. Create Surface A (640x480)
    auto surf_desc_a = render_surface_desc{
        .raw_surface = raw_surf,
        .present_mode = caps.supported_present_modes[0],
        .format = caps.supported_formats[0],
        .width = 640,
        .height = 480,
        .min_image_count = caps.min_image_count,
        .preferred_image_count = caps.min_image_count + 1,
    };
    auto surf_a = dev->create_render_surface(surf_desc_a);
    ASSERT_NE(surf_a, nullptr);

    // 2. Resize window and create Surface B (800x600) handing over Surface A as old_surface
    glfwSetWindowSize(window, 800, 600);
    glfwPollEvents();

    // Get the new framebuffer size after resizing the window
    int fb_width = 0;
    int fb_height = 0;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);

    ASSERT_NE(fb_width, 0);
    ASSERT_NE(fb_height, 0);

    auto caps_b = dev->get_surface_capabilities(raw_surf);
    auto surf_desc_b = render_surface_desc{
        .raw_surface = raw_surf,
        .present_mode = caps_b.supported_present_modes[0],
        .format = caps_b.supported_formats[0],
        .width = static_cast<uint32_t>(fb_width),
        .height = static_cast<uint32_t>(fb_height),
        .min_image_count = caps_b.min_image_count,
        .preferred_image_count = caps_b.min_image_count + 1,
        .old_surface = surf_a.get(),
    };
    auto surf_b = dev->create_render_surface(surf_desc_b);
    ASSERT_NE(surf_b, nullptr);
    EXPECT_EQ(surf_b->get_width(), fb_width);
    EXPECT_EQ(surf_b->get_height(), fb_height);

    // 3. Destroy Surface A (simulating higher-level deletion queue retirement)
    dev->destroy_render_surface(tempest::move(surf_a));

    // 4. Acquire, render, and present with Surface B
    auto acquire_sem = dev->create_binary_semaphore();
    auto render_sem = dev->create_binary_semaphore();

    auto acquire_res = surf_b->acquire_next_image(device_sync_point{
        .semaphore = acquire_sem,
        .value = 0,
        .stages = pipeline_stage::attachment_output,
    });
    ASSERT_TRUE(acquire_res.has_value());
    auto sc_img = acquire_res.value();

    auto& graphics_port = dev->get_graphics_execution_port();
    auto& cmd = graphics_port.acquire_command_list(0, command_list_lifetime::transient);

    cmd.begin();

    auto init_barrier = texture_barrier{
        .texture = sc_img.texture,
        .src =
            {
                .stages = pipeline_stage::attachment_output,
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
        .view = sc_img.view,
        .load_op = load_op::clear,
        .store_op = store_op::store,
        .clear_value =
            clear_color_value{
                .r = 0.1f,
                .g = 0.9f,
                .b = 0.2f,
                .a = 1.0f,
            },
    };
    cmd.begin_render_pass(span<const color_attachment>{&color_att, 1}, nullopt, 800, 600);
    cmd.end_render_pass();

    auto present_barrier = texture_barrier{
        .texture = sc_img.texture,
        .src =
            {
                .stages = pipeline_stage::attachment_output,
                .access = resource_access::write,
                .layout = image_layout::general,
            },
        .dst =
            {
                .stages = pipeline_stage::bottom_of_pipe,
                .access = resource_access::none,
                .layout = image_layout::present,
            },
    };
    cmd.pipeline_barrier(span<const texture_barrier>{&present_barrier, 1}, {});

    cmd.end();

    auto wait_point = device_sync_point{
        .semaphore = acquire_sem,
        .value = 0,
        .stages = pipeline_stage::attachment_output,
    };
    auto signal_point = device_sync_point{
        .semaphore = render_sem,
        .value = 0,
        .stages = pipeline_stage::bottom_of_pipe,
    };

    const auto* cmd_ptr = &cmd;
    auto submit_res =
        graphics_port.submit(span<const command_list*>{&cmd_ptr, 1}, span<const device_sync_point>{&wait_point, 1},
                             span<const device_sync_point>{&signal_point, 1});
    ASSERT_TRUE(submit_res.has_value());

    // Test explicit execution_port overload
    auto present_res = surf_b->present(graphics_port, device_sync_point{
                                                          .semaphore = render_sem,
                                                          .value = 0,
                                                      });
    EXPECT_TRUE(present_res.has_value() || present_res.error() == swapchain_error::suboptimal);

    dev->wait_idle();
    dev->destroy_semaphore(acquire_sem);
    dev->destroy_semaphore(render_sem);
    dev->destroy_render_surface(tempest::move(surf_b));
    dev->destroy_raw_surface(raw_surf);
    glfwDestroyWindow(window);
    glfwTerminate();
}
