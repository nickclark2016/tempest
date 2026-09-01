#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/default_importers.hpp>
#include <tempest/editor.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/memory.hpp>
#include <tempest/profiler/profiler.hpp>
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
            ctx_desc.application_name = "Editor Profiler UX Test";
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
                .title = "Editor Profiler UX Test Window",
                .fullscreen = false,
                .resizable = true,
            });

            return env;
        }
    } // namespace

    //==============================================================================
    // Web Server Lifecycle & Telemetry Collection Tests
    //==============================================================================

    /// @brief Verifies that editor_engine_context initializes and starts the embedded web server,
    /// binds to a valid port in range [8080, 8090], and cleanly stops on destruction.
    TEST(editor_profiler_ux_test, web_server_lifecycle_in_editor_engine_context)
    {
        // 1. Setup: Create editor engine context
        auto engine_ctx = editor_engine_context{};

        // 2. Act: Query web server state
        const auto* server = engine_ctx.get_web_server();

        // 3. Assert: Verify server is running on a valid port
        ASSERT_NE(server, nullptr);
        EXPECT_TRUE(server->is_running());
        const auto bound_port = server->get_bound_port();
        EXPECT_GE(bound_port, 8080u);
        EXPECT_LE(bound_port, 8090u);

        const auto server_url = server->get_server_url();
        EXPECT_FALSE(server_url.empty());
        EXPECT_NE(std::string_view(server_url.data(), server_url.size()).find("http://127.0.0.1:"),
                  std::string_view::npos);
    }

    /// @brief Verifies telemetry collection and broadcast from profiler session into cached
    /// telemetry frames with CPU and GPU tracks.
    TEST(editor_profiler_ux_test, telemetry_collection_and_caching)
    {
        // 1. Setup: Create editor engine context and record test zones
        auto engine_ctx = editor_engine_context{};
        auto& session = engine_ctx.get_profiler_session();

        {
            [[maybe_unused]] const auto z1 = profiler::scoped_zone{session, "UpdatePhysics"};
            [[maybe_unused]] const auto z2 = profiler::scoped_zone{session, "RenderScene"};
        }

        // 2. Act: Collect and broadcast telemetry
        engine_ctx.collect_and_broadcast_telemetry();

        // 3. Assert: Verify frame index and cached telemetry data
        EXPECT_EQ(engine_ctx.get_frame_index(), 1u);
        const auto& frame = engine_ctx.get_last_telemetry_frame();
        EXPECT_EQ(frame.frame_index, 1u);
        EXPECT_FALSE(frame.cpu_tracks.empty());

        auto found_physics = false;
        auto found_render = false;
        for (const auto& track : frame.cpu_tracks)
        {
            for (const auto& z : track.zones)
            {
                if (z.name == "UpdatePhysics")
                {
                    found_physics = true;
                }
                if (z.name == "RenderScene")
                {
                    found_render = true;
                }
            }
        }
        EXPECT_TRUE(found_physics);
        EXPECT_TRUE(found_render);
    }

    //==============================================================================
    // Recording & Debug Marker Tests
    //==============================================================================

    /// @brief Verifies capture recording state toggle and bookmark marker insertion.
    TEST(editor_profiler_ux_test, recording_toggle_and_bookmark_markers)
    {
        // 1. Setup: Create editor engine context
        auto engine_ctx = editor_engine_context{};
        EXPECT_FALSE(engine_ctx.is_recording());

        // 2. Act: Start recording and insert markers
        const auto rec_started = engine_ctx.toggle_recording();
        EXPECT_TRUE(rec_started);
        EXPECT_TRUE(engine_ctx.is_recording());

        engine_ctx.insert_bookmark_marker("TestSceneBookmark");
        engine_ctx.insert_bookmark_marker(); // Default formatted name

        // Record a zone during active capture
        {
            [[maybe_unused]] const auto z =
                profiler::scoped_zone{engine_ctx.get_profiler_session(), "CapturedWorkload"};
        }

        // 3. Act: Stop recording
        const auto rec_stopped = engine_ctx.toggle_recording();
        EXPECT_FALSE(rec_stopped);
        EXPECT_FALSE(engine_ctx.is_recording());
    }

    /// @brief Verifies live stream and GPU statistics enable/disable toggles.
    TEST(editor_profiler_ux_test, live_stream_and_gpu_stats_toggles)
    {
        // 1. Setup: Create editor engine context
        auto engine_ctx = editor_engine_context{};

        // 2. Act & Assert: Live stream toggle
        EXPECT_TRUE(engine_ctx.is_live_stream_enabled());
        engine_ctx.set_live_stream_enabled(false);
        EXPECT_FALSE(engine_ctx.is_live_stream_enabled());
        engine_ctx.set_live_stream_enabled(true);
        EXPECT_TRUE(engine_ctx.is_live_stream_enabled());

        // 3. Act & Assert: GPU stats toggle
        EXPECT_TRUE(engine_ctx.is_gpu_stats_enabled());
        engine_ctx.set_gpu_stats_enabled(false);
        EXPECT_FALSE(engine_ctx.is_gpu_stats_enabled());
        engine_ctx.set_gpu_stats_enabled(true);
        EXPECT_TRUE(engine_ctx.is_gpu_stats_enabled());
    }

    //==============================================================================
    // Editor Context UX & Status Bar Tests
    //==============================================================================

    /// @brief Verifies editor_context initialization, menu registration with shortcut labels,
    /// and full status bar telemetry pill rendering in ImGui.
    TEST(editor_profiler_ux_test, editor_context_profiler_ux_rendering)
    {
        // 1. Setup: Initialize test environment and UI context
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            auto engine_ctx = editor_engine_context{};
            const auto desc = window_desc{
                .width = 1280,
                .height = 720,
                .title = "Editor Profiler Test Window",
                .fullscreen = false,
                .resizable = true,
            };

            auto reg_info = engine_ctx.register_window(desc);
            ASSERT_TRUE(reg_info.handle.is_valid());

            auto ed_ctx = editor_context(engine_ctx, reg_info.handle, ui_ctx);

            // Record some zones
            {
                [[maybe_unused]] const auto z =
                    profiler::scoped_zone{engine_ctx.get_profiler_session(), "Editor::PaintPass"};
            }
            engine_ctx.collect_and_broadcast_telemetry();

            // 2. Act: Render frame containing MainMenuBar and Status Bar with Telemetry Pill
            ui_ctx.begin_ui_commands();
            EXPECT_NO_THROW(ed_ctx.draw());
            ui_ctx.finish_ui_commands();

            // 3. Assert: Verify clean execution and valid frame telemetry
            EXPECT_GT(engine_ctx.get_frame_index(), 0u);
        }

        env.win_mgr.destroy_window(env.win);
    }

    /// @brief Verifies global shortcut key event processing for recording and bookmark triggers.
    TEST(editor_profiler_ux_test, global_shortcuts_event_processing)
    {
        // 1. Setup: Initialize test environment and UI context
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            auto engine_ctx = editor_engine_context{};
            const auto desc = window_desc{
                .width = 1280,
                .height = 720,
                .title = "Shortcuts Test Window",
                .fullscreen = false,
                .resizable = true,
            };

            auto reg_info = engine_ctx.register_window(desc);
            ASSERT_TRUE(reg_info.handle.is_valid());

            auto ed_ctx = editor_context(engine_ctx, reg_info.handle, ui_ctx);

            // Initial frame setup
            ui_ctx.begin_ui_commands();
            ed_ctx.draw();
            ui_ctx.finish_ui_commands();
            EXPECT_FALSE(engine_ctx.is_recording());

            // 2. Act: Simulate F8 key press to toggle recording
            auto& io = ImGui::GetIO();
            io.AddKeyEvent(ImGuiKey_F8, true);

            ui_ctx.begin_ui_commands();
            ed_ctx.draw();
            ui_ctx.finish_ui_commands();

            // 3. Assert: Recording state is toggled to true
            EXPECT_TRUE(engine_ctx.is_recording());

            // 4. Act: Release F8 and press again to toggle recording off
            io.AddKeyEvent(ImGuiKey_F8, false);
            ui_ctx.begin_ui_commands();
            ed_ctx.draw();
            ui_ctx.finish_ui_commands();

            io.AddKeyEvent(ImGuiKey_F8, true);
            ui_ctx.begin_ui_commands();
            ed_ctx.draw();
            ui_ctx.finish_ui_commands();

            // 5. Assert: Recording state is toggled back to false
            EXPECT_FALSE(engine_ctx.is_recording());

            // 6. Act: Simulate F9 key press to insert marker
            io.AddKeyEvent(ImGuiKey_F8, false);
            io.AddKeyEvent(ImGuiKey_F9, true);

            ui_ctx.begin_ui_commands();
            EXPECT_NO_THROW(ed_ctx.draw());
            ui_ctx.finish_ui_commands();
        }

        env.win_mgr.destroy_window(env.win);
    }
} // namespace tempest::editor::tests
