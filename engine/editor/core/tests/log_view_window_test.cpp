#include <tempest/windows/log_view_window.hpp>

#include <tempest/asset_database.hpp>
#include <tempest/default_importers.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/int.hpp>
#include <tempest/logger.hpp>
#include <tempest/memory.hpp>
#include <tempest/rhi.hpp>
#include <tempest/source_location.hpp>
#include <tempest/string_view.hpp>
#include <tempest/ui.hpp>
#include <tempest/vector.hpp>
#include <tempest/vk/context.hpp>
#include <tempest/window_manager.hpp>

#include <gtest/gtest.h>
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
            static auto test_sink = stdout_log_sink{};
            static auto test_log = logger{test_sink};

            auto ctx_desc = rhi::context_desc{};
            ctx_desc.application_name = "Tempest Log View Window Test";
            ctx_desc.api = rhi::graphics_api::vulkan;

            auto result = rhi::vk::create_context(ctx_desc, test_log);
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
                .title = "Log View Window Test Window",
                .fullscreen = false,
                .resizable = true,
            });

            return env;
        }
    } // namespace

    // ============================================================================
    // Window Properties & Lifecycle Tests
    // ============================================================================

    /// @brief Verifies log_view_window properties: initial dock location, window name, and active states.
    TEST(log_view_window_test, window_properties_and_docking)
    {
        // Setup: Create log_store and log_view_window
        auto store = log_store{};
        auto window = log_view_window{store};

        // Act: Query window properties
        const auto dock = window.desired_initial_dock();
        const auto name = window.window_name();
        const auto supported_edit = window.is_mode_supported(simulation_state::edit);
        const auto supported_paused = window.is_mode_supported(simulation_state::paused);
        const auto supported_playing = window.is_mode_supported(simulation_state::playing);

        // Assert: Verify all properties match architectural specification
        EXPECT_EQ(dock, editor_window::dock_location::bottom);
        EXPECT_EQ(name, "Log View");
        EXPECT_TRUE(supported_edit);
        EXPECT_TRUE(supported_paused);
        EXPECT_TRUE(supported_playing);
        EXPECT_TRUE(window.open);
        EXPECT_TRUE(window.auto_scroll);
        EXPECT_FALSE(window.request_scroll_to_bottom);
    }

    // ============================================================================
    // Status Pill & Telemetry Ingestion Tests
    // ============================================================================

    /// @brief Verifies that ingesting warnings and errors accurately updates filter counters for status pill display.
    TEST(log_view_window_test, status_pill_telemetry_reflection)
    {
        // Setup: Create log_store and editor_log_sink
        auto store = log_store{};
        auto sink = editor_log_sink{};

        EXPECT_EQ(store.filter().warn_count, 0U);
        EXPECT_EQ(store.filter().error_count, 0U);

        // Act: Ingest 3 warnings and 2 errors into the sink and drain into store
        sink.log(log_record{
            .level = log_level::warn,
            .timestamp_ns = 100'000'000ULL,
            .thread_id = 1,
            .message = "Warning message 1",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::warn,
            .timestamp_ns = 200'000'000ULL,
            .thread_id = 1,
            .message = "Warning message 2",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::warn,
            .timestamp_ns = 300'000'000ULL,
            .thread_id = 1,
            .message = "Warning message 3",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::error,
            .timestamp_ns = 400'000'000ULL,
            .thread_id = 2,
            .message = "Error message 1",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::error,
            .timestamp_ns = 500'000'000ULL,
            .thread_id = 2,
            .message = "Error message 2",
            .source = source_location::current(),
        });

        const auto drained = store.drain(sink);

        // Assert: Drain count matches, and warning/error counters reflect ingested counts
        EXPECT_EQ(drained, 5U);
        EXPECT_EQ(store.filter().warn_count, 3U);
        EXPECT_EQ(store.filter().error_count, 2U);
        EXPECT_EQ(store.filter().total_count, 5U);
    }

    // ============================================================================
    // Virtualized Table & Drawing Execution Tests
    // ============================================================================

    /// @brief Verifies that log_view_window executes clean ImGui drawing passes with various log entries and levels.
    TEST(log_view_window_test, window_draw_with_entries)
    {
        // Setup: Ingest diverse log records into store and initialize window with headless ImGui
        auto store = log_store{};
        auto sink = editor_log_sink{};

        sink.log(log_record{
            .level = log_level::trace,
            .timestamp_ns = 1'000'000'000ULL,
            .thread_id = 0x1001,
            .message = "Trace message",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::debug,
            .timestamp_ns = 2'000'000'000ULL,
            .thread_id = 0x1002,
            .message = "Debug message",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::info,
            .timestamp_ns = 3'000'000'000ULL,
            .thread_id = 0x1003,
            .message = "Info message",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::warn,
            .timestamp_ns = 4'000'000'000ULL,
            .thread_id = 0x1004,
            .message = "Warning message",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::error,
            .timestamp_ns = 5'000'000'000ULL,
            .thread_id = 0x1005,
            .message = "Error message",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::critical,
            .timestamp_ns = 6'000'000'000ULL,
            .thread_id = 0x1006,
            .message = "Critical message",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::fatal,
            .timestamp_ns = 7'000'000'000ULL,
            .thread_id = 0x1007,
            .message = "Fatal message",
            .source = source_location::current(),
        });

        store.drain(sink);
        auto window = log_view_window{store};

        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            // Act: Render frame 1 with all entries visible
            ui_ctx.begin_ui_commands();
            EXPECT_NO_THROW(window.draw());
            ui_ctx.finish_ui_commands();

            // Assert: Verify window remains open and auto_scroll state maintained
            EXPECT_TRUE(window.open);
            EXPECT_TRUE(window.auto_scroll);

            // Act: Render frame 2 with window closed (should early return without error)
            window.open = false;
            ui_ctx.begin_ui_commands();
            EXPECT_NO_THROW(window.draw());
            ui_ctx.finish_ui_commands();
            EXPECT_FALSE(window.open);
        }

        env.win_mgr.destroy_window(env.win);
    }

    // ============================================================================
    // Filter Interaction & Watermark Tests
    // ============================================================================

    /// @brief Verifies search query updates, level toggles, and clear watermark interactions.
    TEST(log_view_window_test, clear_and_search_interactions)
    {
        // Setup: Create store and populate with distinct messages
        auto store = log_store{};
        auto sink = editor_log_sink{};

        sink.log(log_record{
            .level = log_level::info,
            .timestamp_ns = 10'000'000ULL,
            .thread_id = 1,
            .message = "Vulkan render device created",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::warn,
            .timestamp_ns = 20'000'000ULL,
            .thread_id = 1,
            .message = "Swapchain recreation triggered",
            .source = source_location::current(),
        });
        sink.log(log_record{
            .level = log_level::error,
            .timestamp_ns = 30'000'000ULL,
            .thread_id = 2,
            .message = "Shader compilation failed",
            .source = source_location::current(),
        });

        store.drain(sink);
        auto window = log_view_window{store};

        // Act: Apply search filter for "Shader"
        store.set_filter_search("Shader");

        // Assert: Only the matching shader record is in matched sequences
        EXPECT_EQ(store.filter().matched_sequences.size(), 1U);
        const auto matched_seq = store.filter().matched_sequences[0];
        const auto* const matched_entry = store.get_entry(matched_seq);
        ASSERT_NE(matched_entry, nullptr);
        EXPECT_EQ(matched_entry->message(), "Shader compilation failed");

        // Act: Reset search and invoke clear
        store.set_filter_search("");
        EXPECT_EQ(store.filter().matched_sequences.size(), 3U);

        window.store->clear();

        // Assert: All visible matches cleared, watermark advanced
        EXPECT_EQ(store.visible_count(), 0U);
        EXPECT_TRUE(store.filter().matched_sequences.empty());
        EXPECT_EQ(store.filter().warn_count, 0U);
        EXPECT_EQ(store.filter().error_count, 0U);
    }

    // ============================================================================
    // Context & Engine Integration Tests
    // ============================================================================

    /// @brief Verifies that editor_engine_context automatically captures logs and drains them each frame.
    TEST(log_view_window_test, editor_engine_context_log_integration)
    {
        // Setup: Create editor_engine_context
        auto ctx = editor_engine_context{engine_config{.headless = true}};

        // Act: Log through engine logger
        ctx.get_logger().info("Engine started");
        ctx.get_logger().warn("Low memory warning");
        ctx.get_logger().error("Resource not found");

        // Drain into store (normally called during on_render_frame or on_frame_end)
        ctx.get_log_store().drain(ctx.get_log_sink());

        // Assert: Verify entries captured in context's log_store
        EXPECT_EQ(ctx.get_log_store().filter().warn_count, 1U);
        EXPECT_EQ(ctx.get_log_store().filter().error_count, 1U);
        EXPECT_GE(ctx.get_log_store().visible_count(), 3U);
    }
} // namespace tempest::editor::tests
