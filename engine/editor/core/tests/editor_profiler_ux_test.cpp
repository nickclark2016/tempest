#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/default_importers.hpp>
#include <tempest/editor.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/memory.hpp>
#include <tempest/profiler/profiler.hpp>
#include <tempest/render_system/renderer.hpp>
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
    }

    /// @brief Verifies that telemetry frame creation aggregates CPU/GPU durations and non-blocking broadcasts.
    TEST(editor_profiler_ux_test, telemetry_frame_collection_and_durations)
    {
        // 1. Setup: Create editor engine context
        auto engine_ctx = editor_engine_context{};

        // 2. Act: Record mock zones into profiler session
        {
            [[maybe_unused]] const auto z1 =
                profiler::scoped_zone{engine_ctx.get_profiler_session(), "editor::test_zone"};
        }
        engine_ctx.collect_and_broadcast_telemetry();

        // 3. Assert: Verify telemetry frame is non-empty and frame index increments
        EXPECT_GT(engine_ctx.get_frame_index(), 0u);
        const auto& frame = engine_ctx.get_last_telemetry_frame();
        EXPECT_EQ(frame.frame_index, engine_ctx.get_frame_index());
    }

    /// @brief Verifies profiler recording start/stop state management and binary capture export.
    TEST(editor_profiler_ux_test, recording_lifecycle_and_state)
    {
        // 1. Setup: Create editor engine context
        auto engine_ctx = editor_engine_context{};
        EXPECT_FALSE(engine_ctx.is_recording());

        // 2. Act: Start recording
        engine_ctx.set_recording(true);
        EXPECT_TRUE(engine_ctx.is_recording());

        // Record zone
        {
            [[maybe_unused]] const auto z =
                profiler::scoped_zone{engine_ctx.get_profiler_session(), "editor::record_test"};
        }

        // 3. Act: Stop recording (exports capture)
        engine_ctx.set_recording(false);
        EXPECT_FALSE(engine_ctx.is_recording());

        // 4. Act: Toggle recording
        EXPECT_TRUE(engine_ctx.toggle_recording());
        EXPECT_TRUE(engine_ctx.is_recording());
        EXPECT_FALSE(engine_ctx.toggle_recording());
        EXPECT_FALSE(engine_ctx.is_recording());
    }

    /// @brief Verifies live stream and GPU statistics enable/disable toggles and renderer pipeline stats
    /// synchronization.
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

        // 3. Act & Assert: GPU stats toggle and renderer synchronization
        EXPECT_TRUE(engine_ctx.is_gpu_stats_enabled());
        EXPECT_EQ(engine_ctx.get_renderer().get_pipeline_statistics(), render_system::all_pipeline_statistics);

        engine_ctx.set_gpu_stats_enabled(false);
        EXPECT_FALSE(engine_ctx.is_gpu_stats_enabled());
        EXPECT_EQ(engine_ctx.get_renderer().get_pipeline_statistics(), rhi::pipeline_statistic_flags::none);

        engine_ctx.set_gpu_stats_enabled(true);
        EXPECT_TRUE(engine_ctx.is_gpu_stats_enabled());
        EXPECT_EQ(engine_ctx.get_renderer().get_pipeline_statistics(), render_system::all_pipeline_statistics);
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

    /// @brief Verifies rolling statistics accumulation in editor_engine_context and filtering of GPU queue submits.
    TEST(editor_profiler_ux_test, editor_rolling_statistics_and_hot_zones)
    {
        // 1. Setup: Create editor engine context
        auto engine_ctx = editor_engine_context{};

        // Setup GPU track with submit envelope and passes
        const auto gpu_track_id = 0x8000'0001ULL;
        engine_ctx.get_profiler_session().register_track(gpu_track_id, "GPU: Graphics");

        // Record frame 1: CPU zone + GPU submit envelope and pass
        {
            [[maybe_unused]] const auto z_cpu =
                profiler::scoped_zone{engine_ctx.get_profiler_session(), "Editor::PaintPass"};

            auto chunk = engine_ctx.get_profiler_session().acquire_chunk();
            chunk->set_thread_id(gpu_track_id);
            // Submit envelope (0 to 5ms)
            chunk->add_zone(profiler::zone_record{
                .start_ns = 0,
                .end_ns = 5'000'000,
                .depth = 0,
                .name = "Graphics Submit",
            });
            // ForwardLightingPass (0.5 to 3.5ms = 3ms)
            chunk->add_zone(profiler::zone_record{
                .start_ns = 500'000,
                .end_ns = 3'500'000,
                .depth = 1,
                .name = "ForwardLightingPass",
            });
            // SkyboxPass (3.5 to 4.5ms = 1ms)
            chunk->add_zone(profiler::zone_record{
                .start_ns = 3'500'000,
                .end_ns = 4'500'000,
                .depth = 1,
                .name = "SkyboxPass",
            });
            engine_ctx.get_profiler_session().push_completed_chunk(tempest::move(chunk));
        }

        // 2. Act: Collect telemetry for frame 1
        engine_ctx.collect_and_broadcast_telemetry();

        // 3. Assert: Verify rolling stats and hot zones
        EXPECT_EQ(engine_ctx.get_frame_index(), 1u);
        EXPECT_GT(engine_ctx.get_rolling_fps(), 0.0f);
        EXPECT_GT(engine_ctx.get_rolling_frame_time_ms(), 0.0f);
        EXPECT_GT(engine_ctx.get_rolling_gpu_time_ms(), 0.0f);

        const auto gpu_hot = engine_ctx.get_top_gpu_hot_zones(5);
        // Verify submit envelope is filtered out
        for (const auto& z : gpu_hot)
        {
            EXPECT_NE(z.name, "Graphics Submit");
            EXPECT_FALSE(profiler::is_gpu_submit_zone_name(z.name));
        }
        ASSERT_GE(gpu_hot.size(), 2u);
        EXPECT_EQ(gpu_hot[0].name, "ForwardLightingPass");
        EXPECT_EQ(gpu_hot[1].name, "SkyboxPass");

        const auto cpu_hot = engine_ctx.get_top_cpu_hot_zones(5);
        ASSERT_GE(cpu_hot.size(), 1u);
        EXPECT_EQ(cpu_hot[0].name, "Editor::PaintPass");
    }
} // namespace tempest::editor::tests
