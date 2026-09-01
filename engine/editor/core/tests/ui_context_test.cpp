#include <gtest/gtest.h>

#include <tempest/default_importers.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/rhi.hpp>
#include <tempest/ui.hpp>
#include <tempest/vk/context.hpp>
#include <tempest/window_manager.hpp>

#include <imgui.h>

namespace tempest::editor::tests
{
    namespace
    {
        struct test_env
        {
            unique_ptr<rhi::context> context;
            unique_ptr<rhi::device> dev;
            assets::asset_database asset_db{};
            window_manager win_mgr;
            window_handle win{null_window_handle};
        };

        auto create_test_env() -> test_env
        {
            auto ctx_desc = rhi::context_desc{};
            ctx_desc.application_name = "Tempest UI Context Test";
            ctx_desc.api = rhi::graphics_api::vulkan;

            auto result = rhi::vk::create_context(ctx_desc);
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
            if (!dev)
            {
                return {};
            }

            auto asset_db = assets::asset_database{};
            assets::mount_default_shader_roots(asset_db);
            asset_db.scan_and_index();

            auto env = test_env{
                .context = tempest::move(context),
                .dev = tempest::move(dev),
                .asset_db = tempest::move(asset_db),
                .win_mgr = window_manager{},
            };

            env.win = env.win_mgr.create_window({
                .width = 1280,
                .height = 720,
                .title = "UI Context Test Window",
                .fullscreen = false,
                .resizable = true,
            });

            return env;
        }
    } // namespace

    TEST(ui_context_test, lifecycle)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
        }

        env.win_mgr.destroy_window(env.win);
    }

    TEST(ui_context_test, frame_lifecycle_and_rendering)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            ui_ctx.begin_ui_commands();

            ImGui::SetNextWindowPos(ImVec2(10, 10));
            ImGui::SetNextWindowSize(ImVec2(400, 300));
            if (ImGui::Begin("Test Window"))
            {
                ImGui::Text("Hello Tempest Editor");
                auto s = ui::scalar("Scalar", 42.0f);
                EXPECT_EQ(s, 42.0f);
                auto f3 = ui::float3("Float3", math::float3(1.0f, 2.0f, 3.0f));
                EXPECT_EQ(f3.x, 1.0f);
                auto c3 = ui::color3("Color3", math::float3(0.5f, 0.5f, 0.5f));
                EXPECT_EQ(c3.x, 0.5f);
                auto di = ui::drag_integral("Integral", 10, 0, 100);
                EXPECT_EQ(di, 10);
                auto ds = ui::drag_scalar("DragScalar", 5.0f, 0.0f, 10.0f);
                EXPECT_EQ(ds, 5.0f);
                ui::centered_button("Button");
            }
            ImGui::End();

            ui_ctx.finish_ui_commands();

            auto tex_desc = rhi::texture_desc{
                .width = 1280,
                .height = 720,
                .depth = 1,
                .mip_levels = 1,
                .array_layers = 1,
                .format = rhi::data_format::rgba8_unorm,
                .memory_usage = rhi::memory_usage::device_only,
                .usage = rhi::texture_usage::color_attachment,
                .name = "UI Test Color Target",
            };
            auto tex = env.dev->create_texture(tex_desc);
            auto view = env.dev->create_texture_view(tex, rhi::texture_view_desc{});

            auto color_att = rhi::color_attachment{
                .view = view,
                .load_op = rhi::load_op::clear,
                .store_op = rhi::store_op::store,
                .clear_value = rhi::clear_color_value{0.0F, 0.0F, 0.0F, 1.0F},
            };

            auto& graphics_port = env.dev->get_graphics_execution_port();
            auto& cmd = graphics_port.acquire_command_list(0, rhi::command_list_lifetime::transient);

            cmd.begin();
            cmd.begin_render_pass(span<const rhi::color_attachment>{&color_att, 1}, nullopt, 1280, 720);
            ui_ctx.render_ui_commands(cmd, 1280, 720);
            cmd.end_render_pass();
            cmd.end();

            env.dev->destroy_texture_view(view);
            env.dev->destroy_texture(tex);
        }

        env.win_mgr.destroy_window(env.win);
    }

    TEST(ui_context_test, image_helper)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        auto tex_desc = rhi::texture_desc{
            .width = 64,
            .height = 64,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .format = rhi::data_format::rgba8_unorm,
            .memory_usage = rhi::memory_usage::device_only,
            .usage = rhi::texture_usage::sampled,
            .name = "Test Image",
        };
        auto tex = env.dev->create_texture(tex_desc);
        auto view = env.dev->create_texture_view(tex, rhi::texture_view_desc{});
        auto descriptor = env.dev->allocate_descriptor(rhi::descriptor_type::sampled_image);
        env.dev->write_sampled_image_descriptor(descriptor, view, rhi::image_layout::general);

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            ui_ctx.begin_ui_commands();
            ImGui::Begin("Image Test");
            ui::image(descriptor, 64, 64);
            ImGui::End();
            ui_ctx.finish_ui_commands();
        }

        env.dev->free_descriptor(rhi::descriptor_type::sampled_image, descriptor);
        env.dev->destroy_texture_view(view);
        env.dev->destroy_texture(tex);
        env.win_mgr.destroy_window(env.win);
    }

    TEST(ui_context_test, input_text_helpers)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            auto str1 = string{"Initial String"};
            auto str2 = string{};

            ui_ctx.begin_ui_commands();
            ImGui::Begin("Input Text Test");
            EXPECT_NO_THROW({
                ui::input_text("Label 1", str1);
                ui::input_text_with_hint("Label 2", "Hint", str2);
            });
            ImGui::End();
            ui_ctx.finish_ui_commands();

            EXPECT_EQ(str1, "Initial String");
            EXPECT_TRUE(str2.empty());
        }

        env.win_mgr.destroy_window(env.win);
    }
} // namespace tempest::editor::tests
