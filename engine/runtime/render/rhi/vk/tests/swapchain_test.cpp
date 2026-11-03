#include <gtest/gtest.h>
#include <tempest/atomic.hpp>
#include <tempest/guid.hpp>
#include <tempest/logger.hpp>
#include <tempest/rhi.hpp>
#include <tempest/thread.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>
#include <tempest/vk/context.hpp>
#include <tempest/vk/render_surface.hpp>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifdef TEMPEST_PLATFORM_WINDOWS
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
        static auto test_sink = stdout_log_sink{};
        static auto test_log = logger{test_sink};

        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Swapchain Test";
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

#ifdef TEMPEST_PLATFORM_WINDOWS
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

#ifdef TEMPEST_PLATFORM_WINDOWS
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

    // Acquire swapchain image
    auto acquire_res = surf->acquire_next_image(device_sync_point{
        .semaphore = acquire_sem,
        .value = 0,
        .stages = pipeline_stage::attachment_output,
    });
    ASSERT_TRUE(acquire_res.has_value());
    auto sc_img = acquire_res.value();

    // Clear swapchain image to solid blue and transition to present layout
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
                .r = 0.0F,
                .g = 0.4F,
                .b = 0.8F,
                .a = 1.0F,
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

    // Submit command buffer waiting on acquire_sem and signaling render_sem
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

    // Present image
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

#ifdef TEMPEST_PLATFORM_WINDOWS
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

    // Create Surface A (640x480)
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

    // Resize window and create Surface B (800x600) handing over Surface A as old_surface
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

    // Destroy Surface A (simulating higher-level deletion queue retirement)
    dev->destroy_render_surface(tempest::move(surf_a));

    // Acquire, render, and present with Surface B
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
                .r = 0.1F,
                .g = 0.9F,
                .b = 0.2F,
                .a = 1.0F,
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

/// @brief Stress-tests rapid repeated surface resize with old_surface handover and deferred retirement destruction.
TEST(swapchain_test, swapchain_rapid_repeated_resize_retirement_stress)
{
    // Setup: Initialize Vulkan device and GLFW window
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    ASSERT_EQ(glfwInit(), GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto* window = glfwCreateWindow(640, 480, "Tempest Swapchain Stress", nullptr, nullptr);
    ASSERT_NE(window, nullptr);

#ifdef TEMPEST_PLATFORM_WINDOWS
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

    // Create initial surface
    auto current_desc = render_surface_desc{
        .raw_surface = raw_surf,
        .present_mode = caps.supported_present_modes[0],
        .format = caps.supported_formats[0],
        .width = 640,
        .height = 480,
        .min_image_count = caps.min_image_count,
        .preferred_image_count = caps.min_image_count + 1,
    };
    auto current_surf = dev->create_render_surface(current_desc);
    ASSERT_NE(current_surf, nullptr);

    // Act: Rapidly recreate surface 5 times in succession across changing dimensions
    auto retired_surfaces = vector<unique_ptr<render_surface>>{};
    constexpr auto resize_steps = 5U;
    const auto target_widths = vector<uint32_t>{init_list, 720U, 800U, 960U, 1024U, 1280U};
    const auto target_heights = vector<uint32_t>{init_list, 480U, 600U, 540U, 768U, 720U};

    for (auto i = 0U; i < resize_steps; ++i)
    {
        glfwSetWindowSize(window, static_cast<int>(target_widths[i]), static_cast<int>(target_heights[i]));
        glfwPollEvents();

        auto new_desc = render_surface_desc{
            .raw_surface = raw_surf,
            .present_mode = caps.supported_present_modes[0],
            .format = caps.supported_formats[0],
            .width = target_widths[i],
            .height = target_heights[i],
            .min_image_count = caps.min_image_count,
            .preferred_image_count = caps.min_image_count + 1,
            .old_surface = current_surf.get(),
        };

        auto next_surf = dev->create_render_surface(new_desc);
        ASSERT_NE(next_surf, nullptr);

        retired_surfaces.push_back(tempest::move(current_surf));
        current_surf = tempest::move(next_surf);
    }

    // Assert: All 5 predecessor surfaces are safely maintained in the retired collection
    EXPECT_EQ(retired_surfaces.size(), resize_steps);

    // Wait idle and flush all retired surfaces (simulating frame retirement queue processing)
    dev->wait_idle();
    for (auto& old_surf : retired_surfaces)
    {
        dev->destroy_render_surface(tempest::move(old_surf));
    }
    retired_surfaces.clear();

    // Verify final surface functions properly by acquiring an image
    auto acquire_sem = dev->create_binary_semaphore();
    auto acquire_res = current_surf->acquire_next_image(device_sync_point{
        .semaphore = acquire_sem,
        .value = 0,
        .stages = pipeline_stage::attachment_output,
    });
    EXPECT_TRUE(acquire_res.has_value());

    // Cleanup
    dev->wait_idle();
    dev->destroy_semaphore(acquire_sem);
    dev->destroy_render_surface(tempest::move(current_surf));
    dev->destroy_raw_surface(raw_surf);
    glfwDestroyWindow(window);
    glfwTerminate();
}

/// @brief Tests mutual exclusion and absence of validation races between concurrent execution_port submissions and render_surface presents.
TEST(swapchain_test, concurrent_submit_and_present_stress)
{
    // Setup: Create test environment with window and swapchain
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    ASSERT_EQ(glfwInit(), GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto* window = glfwCreateWindow(640, 480, "Tempest Concurrent Present Test", nullptr, nullptr);
    ASSERT_NE(window, nullptr);

#ifdef TEMPEST_PLATFORM_WINDOWS
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
        .width = 640,
        .height = 480,
        .min_image_count = caps.min_image_count,
        .preferred_image_count = caps.min_image_count + 1,
    };

    auto surf = dev->create_render_surface(surf_desc);
    ASSERT_NE(surf, nullptr);

    auto& graphics_port = dev->get_graphics_execution_port();

    auto keep_submitting = atomic<bool>{true};
    auto background_submits_completed = atomic<uint32_t>{0};

    // Act: Spawn a concurrent background thread continuously submitting commands to graphics_port
    auto background_worker = thread{[&graphics_port, &keep_submitting, &background_submits_completed]() -> void {
        while (keep_submitting.load())
        {
            auto& cmd = graphics_port.acquire_command_list(1, command_list_lifetime::transient);
            cmd.begin();
            cmd.end();
            const auto* cmd_ptr = &cmd;
            const auto res = graphics_port.submit(span<const command_list*>{&cmd_ptr, 1}, {}, {});
            if (res.has_value())
            {
                background_submits_completed.fetch_add(1);
            }
            this_thread::sleep_for_nanoseconds(1'000'000);
        }
    }};

    // Act: Main thread repeatedly acquires, renders, and presents frames
    constexpr size_t frame_count = 8;
    for (size_t frame_idx = 0; frame_idx < frame_count; ++frame_idx)
    {
        auto acquire_sem = dev->create_binary_semaphore();
        auto render_sem = dev->create_binary_semaphore();

        auto acquire_res = surf->acquire_next_image(device_sync_point{
            .semaphore = acquire_sem,
            .value = 0,
            .stages = pipeline_stage::attachment_output,
        });
        ASSERT_TRUE(acquire_res.has_value());
        auto sc_img = acquire_res.value();

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
            .clear_value = clear_color_value{.r = 0.1F, .g = 0.2F, .b = 0.3F, .a = 1.0F},
        };
        cmd.begin_render_pass(span<const color_attachment>{&color_att, 1}, nullopt, 640, 480);
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
        auto submit_res = graphics_port.submit(span<const command_list*>{&cmd_ptr, 1},
                                               span<const device_sync_point>{&wait_point, 1},
                                               span<const device_sync_point>{&signal_point, 1});
        ASSERT_TRUE(submit_res.has_value());

        auto present_res = surf->present(device_sync_point{
            .semaphore = render_sem,
            .value = 0,
            .stages = pipeline_stage::bottom_of_pipe,
        });
        EXPECT_TRUE(present_res.has_value());

        dev->wait_idle();
        dev->destroy_semaphore(acquire_sem);
        dev->destroy_semaphore(render_sem);
    }

    keep_submitting.store(false);
    background_worker.join();

    // Assert: Background thread successfully submitted commands concurrently with presentation
    EXPECT_GT(background_submits_completed.load(), 0U);

    // Cleanup
    dev->wait_idle();
    dev->destroy_render_surface(tempest::move(surf));
    dev->destroy_raw_surface(raw_surf);
    glfwDestroyWindow(window);
    glfwTerminate();
}

/// @brief Validates that destroying a render_surface strictly precedes destroying its parent raw_surface,
///        guaranteeing zero Vulkan validation layer warnings or VUID-vkDestroySurfaceKHR-surface-01266 violations.
TEST(swapchain_test, swapchain_destruction_strictly_precedes_surface_destruction)
{
    // Setup: Initialize headless test environment and hidden test window
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    ASSERT_EQ(glfwInit(), GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto* window = glfwCreateWindow(640, 480, "Swapchain Destruction Order Test", nullptr, nullptr);
    ASSERT_NE(window, nullptr);

#ifdef TEMPEST_PLATFORM_WINDOWS
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
    const auto raw_surf = raw_res.value();

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

    // Act: Ensure GPU is idle, then tear down swapchain first, followed by surface, then window
    dev->wait_idle();
    dev->destroy_render_surface(tempest::move(surf));
    EXPECT_EQ(surf, nullptr);

    // Assert: Surface destruction must succeed cleanly with zero validation layer errors or assertion trips
    dev->destroy_raw_surface(raw_surf);

    glfwDestroyWindow(window);
    glfwTerminate();
}

/// @brief Validates that explicit release_swapchain on vk::render_surface is idempotent and safely zeroes handles.
TEST(swapchain_test, render_surface_release_swapchain_idempotent)
{
    // Setup: Initialize headless test environment and test window
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    ASSERT_EQ(glfwInit(), GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto* window = glfwCreateWindow(640, 480, "Swapchain Idempotence Test", nullptr, nullptr);
    ASSERT_NE(window, nullptr);

#ifdef TEMPEST_PLATFORM_WINDOWS
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
    const auto raw_surf = raw_res.value();

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

    // Act: Explicitly call release_swapchain ahead of destruction
    auto* vk_surf = static_cast<vk::render_surface*>(surf.get());
    EXPECT_NE(vk_surf->get_swapchain(), VK_NULL_HANDLE);
    vk_surf->release_swapchain();

    // Assert: Swapchain handle is now VK_NULL_HANDLE and acquired index is reset
    EXPECT_EQ(vk_surf->get_swapchain(), VK_NULL_HANDLE);
    EXPECT_EQ(vk_surf->get_last_acquired_index(), ~0U);

    // Act: Inverted guard verification - destroy_render_surface(nullptr) is a safe no-op
    dev->destroy_render_surface(nullptr);

    // Act: Second call must be a safe no-op
    vk_surf->release_swapchain();
    EXPECT_EQ(vk_surf->get_swapchain(), VK_NULL_HANDLE);
    EXPECT_EQ(vk_surf->get_last_acquired_index(), ~0U);

    // Cleanup: Subsequent destroy_render_surface and destroy_raw_surface succeed without double-free
    dev->destroy_render_surface(tempest::move(surf));
    dev->destroy_raw_surface(raw_surf);

    glfwDestroyWindow(window);
    glfwTerminate();
}

/// @brief Stress-tests rapid repeated creation and destruction cycles of render surfaces (swapchains)
///        on a raw surface, verifying active_swapchain_count tracking precision and zero leak.
TEST(swapchain_test, swapchain_rapid_lifecycle_stress)
{
    // Setup: Initialize headless test environment and test window
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    ASSERT_EQ(glfwInit(), GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto* const window = glfwCreateWindow(640, 480, "Swapchain Rapid Lifecycle Stress", nullptr, nullptr);
    ASSERT_NE(window, nullptr);

#ifdef TEMPEST_PLATFORM_WINDOWS
    const auto native_handle = native_wsi_handle{
        .display = GetModuleHandle(nullptr),
        .window = glfwGetWin32Window(window),
    };
#elif defined(TEMPEST_PLATFORM_LINUX)
    const auto native_handle = native_wsi_handle{
        .display = static_cast<void*>(glfwGetX11Display()),
        .window = reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(window))),
    };
#else
    const auto native_handle = native_wsi_handle{};
#endif

    const auto raw_res = dev->create_raw_surface(native_handle);
    ASSERT_TRUE(raw_res.has_value());
    const auto raw_surf = raw_res.value();

    const auto caps = dev->get_surface_capabilities(raw_surf);
    ASSERT_FALSE(caps.supported_formats.empty());
    ASSERT_FALSE(caps.supported_present_modes.empty());

    const auto surf_desc = render_surface_desc{
        .raw_surface = raw_surf,
        .present_mode = caps.supported_present_modes[0],
        .format = caps.supported_formats[0],
        .width = 640,
        .height = 480,
        .min_image_count = caps.min_image_count,
        .preferred_image_count = caps.min_image_count + 1,
    };

    auto* const vk_dev = static_cast<vk::device*>(dev.get());

    // Act & Assert: Execute rapid create/destroy cycles on the same raw surface
    constexpr auto cycle_count = 20u;
    for (auto iteration = 0u; iteration < cycle_count; ++iteration)
    {
        const auto pre_info = vk_dev->get_raw_surface(raw_surf);
        ASSERT_TRUE(pre_info.has_value());
        EXPECT_EQ(pre_info->active_swapchain_count, 0u);

        auto test_surface = dev->create_render_surface(surf_desc);
        ASSERT_NE(test_surface, nullptr);

        const auto active_info = vk_dev->get_raw_surface(raw_surf);
        ASSERT_TRUE(active_info.has_value());
        EXPECT_EQ(active_info->active_swapchain_count, 1u);

        dev->destroy_render_surface(tempest::move(test_surface));
        EXPECT_EQ(test_surface, nullptr);

        const auto post_info = vk_dev->get_raw_surface(raw_surf);
        ASSERT_TRUE(post_info.has_value());
        EXPECT_EQ(post_info->active_swapchain_count, 0u);
    }

    // Teardown: Final surface destruction must succeed with active_swapchain_count == 0
    dev->destroy_raw_surface(raw_surf);

    glfwDestroyWindow(window);
    glfwTerminate();
}

/// @brief Stress-tests multiple concurrent surfaces and out-of-order destruction to guarantee
///        cross-surface destruction isolation and active_swapchain_count accuracy.
TEST(swapchain_test, swapchain_multi_surface_out_of_order_destruction_stress)
{
    // Setup: Initialize headless test environment and 3 hidden windows
    auto env = create_test_env();
    ASSERT_NE(env.dev, nullptr);
    auto& dev = env.dev;

    ASSERT_EQ(glfwInit(), GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    auto* const window_alpha = glfwCreateWindow(640, 480, "Window Alpha", nullptr, nullptr);
    auto* const window_beta = glfwCreateWindow(640, 480, "Window Beta", nullptr, nullptr);
    auto* const window_gamma = glfwCreateWindow(640, 480, "Window Gamma", nullptr, nullptr);
    ASSERT_NE(window_alpha, nullptr);
    ASSERT_NE(window_beta, nullptr);
    ASSERT_NE(window_gamma, nullptr);

#ifdef TEMPEST_PLATFORM_WINDOWS
    const auto native_handle_alpha = native_wsi_handle{
        .display = GetModuleHandle(nullptr),
        .window = glfwGetWin32Window(window_alpha),
    };
    const auto native_handle_beta = native_wsi_handle{
        .display = GetModuleHandle(nullptr),
        .window = glfwGetWin32Window(window_beta),
    };
    const auto native_handle_gamma = native_wsi_handle{
        .display = GetModuleHandle(nullptr),
        .window = glfwGetWin32Window(window_gamma),
    };
#elif defined(TEMPEST_PLATFORM_LINUX)
    const auto native_handle_alpha = native_wsi_handle{
        .display = static_cast<void*>(glfwGetX11Display()),
        .window = reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(window_alpha))),
    };
    const auto native_handle_beta = native_wsi_handle{
        .display = static_cast<void*>(glfwGetX11Display()),
        .window = reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(window_beta))),
    };
    const auto native_handle_gamma = native_wsi_handle{
        .display = static_cast<void*>(glfwGetX11Display()),
        .window = reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(window_gamma))),
    };
#else
    const auto native_handle_alpha = native_wsi_handle{};
    const auto native_handle_beta = native_wsi_handle{};
    const auto native_handle_gamma = native_wsi_handle{};
#endif

    const auto raw_res_alpha = dev->create_raw_surface(native_handle_alpha);
    const auto raw_res_beta = dev->create_raw_surface(native_handle_beta);
    const auto raw_res_gamma = dev->create_raw_surface(native_handle_gamma);
    ASSERT_TRUE(raw_res_alpha.has_value());
    ASSERT_TRUE(raw_res_beta.has_value());
    ASSERT_TRUE(raw_res_gamma.has_value());
    const auto raw_alpha = raw_res_alpha.value();
    const auto raw_beta = raw_res_beta.value();
    const auto raw_gamma = raw_res_gamma.value();

    const auto caps_alpha = dev->get_surface_capabilities(raw_alpha);
    const auto caps_beta = dev->get_surface_capabilities(raw_beta);
    const auto caps_gamma = dev->get_surface_capabilities(raw_gamma);

    auto surf_alpha = dev->create_render_surface(render_surface_desc{
        .raw_surface = raw_alpha,
        .present_mode = caps_alpha.supported_present_modes[0],
        .format = caps_alpha.supported_formats[0],
        .width = 640,
        .height = 480,
        .min_image_count = caps_alpha.min_image_count,
        .preferred_image_count = caps_alpha.min_image_count + 1,
    });
    auto surf_beta = dev->create_render_surface(render_surface_desc{
        .raw_surface = raw_beta,
        .present_mode = caps_beta.supported_present_modes[0],
        .format = caps_beta.supported_formats[0],
        .width = 640,
        .height = 480,
        .min_image_count = caps_beta.min_image_count,
        .preferred_image_count = caps_beta.min_image_count + 1,
    });
    auto surf_gamma = dev->create_render_surface(render_surface_desc{
        .raw_surface = raw_gamma,
        .present_mode = caps_gamma.supported_present_modes[0],
        .format = caps_gamma.supported_formats[0],
        .width = 640,
        .height = 480,
        .min_image_count = caps_gamma.min_image_count,
        .preferred_image_count = caps_gamma.min_image_count + 1,
    });

    ASSERT_NE(surf_alpha, nullptr);
    ASSERT_NE(surf_beta, nullptr);
    ASSERT_NE(surf_gamma, nullptr);

    auto* const vk_dev = static_cast<vk::device*>(dev.get());
    EXPECT_EQ(vk_dev->get_raw_surface(raw_alpha)->active_swapchain_count, 1u);
    EXPECT_EQ(vk_dev->get_raw_surface(raw_beta)->active_swapchain_count, 1u);
    EXPECT_EQ(vk_dev->get_raw_surface(raw_gamma)->active_swapchain_count, 1u);

    // Act: Out-of-order destruction: destroy surface Beta first
    dev->destroy_render_surface(tempest::move(surf_beta));
    EXPECT_EQ(surf_beta, nullptr);
    EXPECT_EQ(vk_dev->get_raw_surface(raw_alpha)->active_swapchain_count, 1u);
    EXPECT_EQ(vk_dev->get_raw_surface(raw_beta)->active_swapchain_count, 0u);
    EXPECT_EQ(vk_dev->get_raw_surface(raw_gamma)->active_swapchain_count, 1u);

    dev->destroy_raw_surface(raw_beta);
    EXPECT_FALSE(vk_dev->get_raw_surface(raw_beta).has_value());

    // Act: Out-of-order destruction: destroy surface Gamma second
    dev->destroy_render_surface(tempest::move(surf_gamma));
    EXPECT_EQ(surf_gamma, nullptr);
    EXPECT_EQ(vk_dev->get_raw_surface(raw_alpha)->active_swapchain_count, 1u);
    EXPECT_EQ(vk_dev->get_raw_surface(raw_gamma)->active_swapchain_count, 0u);

    dev->destroy_raw_surface(raw_gamma);
    EXPECT_FALSE(vk_dev->get_raw_surface(raw_gamma).has_value());

    // Act: Out-of-order destruction: destroy surface Alpha last
    dev->destroy_render_surface(tempest::move(surf_alpha));
    EXPECT_EQ(surf_alpha, nullptr);
    EXPECT_EQ(vk_dev->get_raw_surface(raw_alpha)->active_swapchain_count, 0u);

    dev->destroy_raw_surface(raw_alpha);
    EXPECT_FALSE(vk_dev->get_raw_surface(raw_alpha).has_value());

    // Teardown: Windows destroyed after all swapchains and raw surfaces are clean
    glfwDestroyWindow(window_gamma);
    glfwDestroyWindow(window_beta);
    glfwDestroyWindow(window_alpha);
    glfwTerminate();
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

