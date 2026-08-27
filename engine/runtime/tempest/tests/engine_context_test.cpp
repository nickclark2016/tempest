#include <gtest/gtest.h>

#include <tempest/input.hpp>
#include <tempest/tempest.hpp>
#include <tempest/window_manager.hpp>

namespace tempest::tests
{
    TEST(window_manager_test, create_and_destroy_window)
    {
        auto wm = window_manager{};
        const auto desc = window_desc{
            .width = 800,
            .height = 600,
            .title = "Test Window",
            .fullscreen = false,
            .resizable = true,
        };

        auto win = wm.create_window(desc);
        ASSERT_TRUE(win.is_valid());

        EXPECT_EQ(wm.get_width(win), 800);
        EXPECT_EQ(wm.get_height(win), 600);
        EXPECT_GT(wm.get_framebuffer_width(win), 0);
        EXPECT_GT(wm.get_framebuffer_height(win), 0);

        auto wsi = wm.get_native_wsi_handle(win);
        EXPECT_NE(wsi.window, nullptr);

        wm.destroy_window(win);
        EXPECT_EQ(wm.get_width(win), 0);
        EXPECT_EQ(wm.get_height(win), 0);
    }

    TEST(window_manager_test, cursor_mode)
    {
        auto wm = window_manager{};
        const auto desc = window_desc{
            .width = 640,
            .height = 480,
            .title = "Cursor Test Window",
        };

        auto win = wm.create_window(desc);
        ASSERT_TRUE(win.is_valid());

        EXPECT_EQ(wm.get_cursor_mode(win), cursor_mode::normal);
        EXPECT_FALSE(wm.is_cursor_disabled(win));

        wm.set_cursor_mode(win, cursor_mode::disabled);
        EXPECT_EQ(wm.get_cursor_mode(win), cursor_mode::disabled);
        EXPECT_TRUE(wm.is_cursor_disabled(win));

        wm.set_cursor_mode(win, cursor_mode::normal);
        EXPECT_EQ(wm.get_cursor_mode(win), cursor_mode::normal);
        EXPECT_FALSE(wm.is_cursor_disabled(win));

        wm.destroy_window(win);
    }

    TEST(window_manager_test, input_and_callbacks)
    {
        auto wm = window_manager{};
        const auto desc = window_desc{
            .width = 640,
            .height = 480,
            .title = "Input Test Window",
        };

        auto win = wm.create_window(desc);
        ASSERT_TRUE(win.is_valid());

        auto& kb = wm.get_keyboard(win);
        auto& ms = wm.get_mouse(win);
        auto group = wm.get_input_group(win);

        EXPECT_EQ(group.kb, &kb);
        EXPECT_EQ(group.ms, &ms);

        auto resize_invoked = false;
        wm.register_resize_callback(win, [&]([[maybe_unused]] uint32_t w, [[maybe_unused]] uint32_t h) {
            resize_invoked = true;
        });

        auto close_invoked = false;
        wm.register_close_callback(win, [&]() {
            close_invoked = true;
        });

        wm.poll_events();

        wm.destroy_window(win);
    }

    TEST(window_manager_test, close_flag)
    {
        auto wm = window_manager{};
        const auto desc = window_desc{
            .width = 640,
            .height = 480,
            .title = "Close Flag Test Window",
        };

        auto win = wm.create_window(desc);
        ASSERT_TRUE(win.is_valid());

        EXPECT_FALSE(wm.should_close(win));
        wm.set_should_close(win, true);
        EXPECT_TRUE(wm.should_close(win));

        wm.destroy_window(win);
    }

    TEST(engine_context_test, standalone_context_subsystems)
    {
        auto ctx = standalone_engine_context{};

        EXPECT_NO_THROW({
            [[maybe_unused]] auto& entities = ctx.get_entities();
            [[maybe_unused]] auto& events = ctx.get_events();
            [[maybe_unused]] auto& materials = ctx.get_materials();
            [[maybe_unused]] auto& meshes = ctx.get_meshes();
            [[maybe_unused]] auto& textures = ctx.get_textures();
            [[maybe_unused]] auto& assets = ctx.get_assets();
            [[maybe_unused]] auto& logger = ctx.get_logger();
            [[maybe_unused]] auto& wm = ctx.get_window_manager();
            [[maybe_unused]] auto& dev = ctx.get_device();
            [[maybe_unused]] auto& renderer = ctx.get_renderer();
        });

        EXPECT_FALSE(ctx.should_close());
        ctx.request_close(true);
        EXPECT_TRUE(ctx.should_close());
    }

    TEST(engine_context_test, window_registration_and_surfaces)
    {
        auto ctx = standalone_engine_context{};

        const auto desc = window_desc{
            .width = 640,
            .height = 480,
            .title = "Registration Test Window",
        };

        auto reg_info = ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());
        EXPECT_NE(reg_info.inputs.kb, nullptr);
        EXPECT_NE(reg_info.inputs.ms, nullptr);

        auto* surf = ctx.get_render_surface(reg_info.handle);
        ASSERT_NE(surf, nullptr);
        EXPECT_GT(surf->get_width(), 0);
        EXPECT_GT(surf->get_height(), 0);

        auto raw_surf = ctx.get_raw_surface(reg_info.handle);
        EXPECT_NE(raw_surf.handle, 0);

        EXPECT_EQ(ctx.get_render_surface(null_window_handle), nullptr);
        EXPECT_EQ(ctx.get_raw_surface(null_window_handle).handle, 0);
    }
} // namespace tempest::tests
