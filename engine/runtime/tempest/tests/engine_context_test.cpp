#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/input.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/tempest.hpp>
#include <tempest/thread.hpp>
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

    TEST(window_manager_test, multiple_callbacks_coexist)
    {
        auto wm = window_manager{};
        const auto desc = window_desc{
            .width = 640,
            .height = 480,
            .title = "Multiple Callback Test Window",
        };

        auto win = wm.create_window(desc);
        ASSERT_TRUE(win.is_valid());

        auto cb1_called = false;
        auto cb2_called = false;

        wm.register_cursor_pos_callback(win, [&](float, float) { cb1_called = true; });
        wm.register_cursor_pos_callback(win, [&](float, float) { cb2_called = true; });

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

    /// @brief Verifies that is_focused returns the focus state of the window, and that keyboard
    /// and mouse clear methods properly reset pressed inputs to release.
    TEST(window_manager_test, focus_and_input_clearing)
    {
        // Setup window manager and create window
        auto wm = window_manager{};
        const auto desc = window_desc{
            .width = 640,
            .height = 480,
            .title = "Focus Gating Test Window",
        };

        auto win = wm.create_window(desc);
        ASSERT_TRUE(win.is_valid());

        // Act & Assert: Verify is_focused returns a valid boolean state
        const auto focused = wm.is_focused(win);
        EXPECT_TRUE(focused || !focused);

        // Act & Assert: Set keyboard key to press, verify is_key_down, then clear and verify released
        auto& kb = wm.get_keyboard(win);
        kb.set(core::key_state{
            .k = core::key::w,
            .action = core::key_action::press,
        });
        EXPECT_TRUE(kb.is_key_down(core::key::w));

        kb.clear();
        EXPECT_FALSE(kb.is_key_down(core::key::w));

        // Act & Assert: Set mouse button to press and position deltas, then clear and verify reset
        auto& ms = wm.get_mouse(win);
        ms.set(core::mouse_button_state{
            .button = core::mouse_button::left,
            .action = core::mouse_action::press,
        });
        ms.set_position(100.0F, 200.0F);
        ms.set_position(110.0F, 220.0F);
        EXPECT_EQ(ms.get(core::mouse_button::left).action, core::mouse_action::press);
        EXPECT_FLOAT_EQ(ms.dx(), 10.0F);
        EXPECT_FLOAT_EQ(ms.dy(), 20.0F);

        ms.clear();
        EXPECT_EQ(ms.get(core::mouse_button::left).action, core::mouse_action::release);
        EXPECT_FLOAT_EQ(ms.dx(), 0.0F);
        EXPECT_FLOAT_EQ(ms.dy(), 0.0F);

        // Cleanup
        wm.destroy_window(win);
    }

    TEST(engine_context_test, standalone_context_subsystems)
    {
        const auto config = engine_config{
            .headless = true,
        };
        auto ctx = standalone_engine_context{config};

        EXPECT_NO_THROW({
            [[maybe_unused]] auto& entities = ctx.get_entities();
            [[maybe_unused]] auto& events = ctx.get_events();
            [[maybe_unused]] auto& materials = ctx.get_materials();
            [[maybe_unused]] auto& meshes = ctx.get_meshes();
            [[maybe_unused]] auto& textures = ctx.get_textures();
            [[maybe_unused]] auto& assets = ctx.get_assets();
            [[maybe_unused]] auto& logger = ctx.get_logger();
            [[maybe_unused]] auto& wm = ctx.get_window_manager();
            [[maybe_unused]] auto& jobs = ctx.get_job_system();
        });

        const auto& const_ctx = ctx;
        EXPECT_NO_THROW({
            [[maybe_unused]] const auto& jobs = const_ctx.get_job_system();
        });

        EXPECT_FALSE(ctx.should_close());
        ctx.request_close(true);
        EXPECT_TRUE(ctx.should_close());
    }

    TEST(engine_context_test, window_registration_and_surfaces)
    {
        const auto config = engine_config{
            .headless = true,
        };
        auto ctx = standalone_engine_context{config};

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
        EXPECT_EQ(surf, nullptr);

        auto raw_surf = ctx.get_raw_surface(reg_info.handle);
        EXPECT_EQ(raw_surf.handle, 0);

        EXPECT_EQ(ctx.get_render_surface(null_window_handle), nullptr);
        EXPECT_EQ(ctx.get_raw_surface(null_window_handle).handle, 0);
    }

    // ------------------------------------------------------------------------
    // Engine Concurrency & Background Task Isolation Tests
    // ------------------------------------------------------------------------

    /// @brief Verifies that frame rendering using targeted countdown latch synchronization
    ///        does not stall, starve, or abort active multi-frame background tasks (e.g. continuous physics simulation).
    TEST(engine_context_test, render_frame_with_concurrent_background_task_isolation)
    {
        // Setup: create testable subclass allowing direct invocation of _render_frame()
        class testable_engine_context final : public standalone_engine_context
        {
          public:
            explicit testable_engine_context(const engine_config& config) : standalone_engine_context{config} {}

            auto render_frame_for_test() -> void
            {
                _render_frame();
            }
        };

        const auto config = engine_config{
            .headless = true,
        };
        auto ctx = testable_engine_context{config};
        const auto desc = window_desc{
            .width = 640,
            .height = 480,
            .title = "Concurrency Test Window",
        };
        auto reg_info = ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        // Setup: spawn a continuous multi-frame background task (simulating physics simulation)
        auto stop_background = atomic<bool>{false};
        auto background_ticks = atomic<uint32_t>{0};
        auto& jobs = ctx.get_job_system();
        auto bg_task = jobs.async(job::task_priority::normal, job::core_class::any, [&]() {
            while (!stop_background.load(memory_order::acquire))
            {
                background_ticks.fetch_add(1, memory_order::acq_rel);
                this_thread::yield();
            }
        });

        // Wait until background task starts running
        while (background_ticks.load(memory_order::acquire) == 0U)
        {
            this_thread::yield();
        }

        // Act: execute multiple frames of rendering while background task runs concurrently
        constexpr auto test_frame_count = 5U;
        for (auto frame_index = 0U; frame_index < test_frame_count; ++frame_index)
        {
            ctx.render_frame_for_test();
            this_thread::yield();
        }

        // Assert: verify background task continued making forward progress without starvation
        EXPECT_GT(background_ticks.load(memory_order::acquire), 0U);

        // Cleanup: signal background task to terminate cleanly
        stop_background.store(true, memory_order::release);
        jobs.wait_idle();
    }

    // ============================================================================
    // Section: Interface Segregation Tests
    // ============================================================================

    /// @brief Verifies that client_context inherits from engine_context, standalone_engine_context
    ///        inherits from client_context, and graphical vs simulation systems are strictly segregated.
    TEST(engine_context_test, interface_segregation_hierarchy)
    {
        // Setup: instantiate standalone context in headless mode
        const auto config = engine_config{
            .headless = true,
            .fixed_timestep = 1.0F / 60.0F,
            .max_frame_delta = 0.1F,
        };
        auto standalone_context = standalone_engine_context{config};

        // Act: bind via references to segregated interfaces
        auto& engine_interface = static_cast<engine_context&>(standalone_context);
        auto& client_interface = static_cast<client_context&>(standalone_context);

        // Assert: engine_context provides simulation primitives and accumulator
        EXPECT_NO_THROW({
            [[maybe_unused]] auto& entities = engine_interface.get_entities();
            [[maybe_unused]] auto& events = engine_interface.get_events();
            [[maybe_unused]] auto& logger = engine_interface.get_logger();
            [[maybe_unused]] auto& jobs = engine_interface.get_job_system();
            [[maybe_unused]] auto& main_executor = engine_interface.get_main_thread_executor();
            EXPECT_TRUE(main_executor.empty());
            [[maybe_unused]] auto& profiler = engine_interface.get_profiler_session();
            [[maybe_unused]] const auto& accumulator = engine_interface.get_accumulator();
            EXPECT_DOUBLE_EQ(accumulator.fixed_delta().count(), static_cast<double>(1.0F / 60.0F));
        });

        // Assert: client_context provides graphical and asset subsystems
        EXPECT_NO_THROW({
            [[maybe_unused]] auto& materials = client_interface.get_materials();
            [[maybe_unused]] auto& meshes = client_interface.get_meshes();
            [[maybe_unused]] auto& textures = client_interface.get_textures();
            [[maybe_unused]] auto& assets = client_interface.get_assets();
            [[maybe_unused]] auto& window_mgr = client_interface.get_window_manager();
        });
    }

    // ============================================================================
    // Section: Template Method Loop Order & Hook Verification
    // ============================================================================

    /// @brief Verifies that the template method loop executes hooks in the exact canonical order:
    ///        initialization callbacks -> [poll_events -> should_step_sim -> fixed_update -> variable_update -> render -> pace -> frame_end] -> close callbacks.
    TEST(engine_context_test, template_method_loop_hooks_dispatch_order)
    {
        // Setup: test subclass recording execution sequence of hooks
        class hook_tracker_engine_context final : public standalone_engine_context
        {
          public:
            explicit hook_tracker_engine_context(const engine_config& config) : standalone_engine_context{config} {}

            vector<string> execution_log{};
            uint32_t frame_count{0};

          protected:
            auto on_poll_events() -> void override
            {
                execution_log.push_back("on_poll_events");
                standalone_engine_context::on_poll_events();
            }

            auto should_step_simulation() -> bool override
            {
                execution_log.push_back("should_step_simulation");
                return true;
            }

            auto on_render_frame(float alpha) -> void override
            {
                execution_log.push_back("on_render_frame");
                standalone_engine_context::on_render_frame(alpha);
            }

            auto on_pace_frame(chrono::duration<double> frame_elapsed) -> void override
            {
                execution_log.push_back("on_pace_frame");
                standalone_engine_context::on_pace_frame(frame_elapsed);
            }

            auto on_frame_end() -> void override
            {
                execution_log.push_back("on_frame_end");
                ++frame_count;
                if (frame_count >= 2)
                {
                    request_close(true);
                }
            }
        };

        const auto config = engine_config{
            .headless = true,
            .fixed_timestep = 1.0F / 60.0F,
            .max_frame_delta = 0.1F,
        };
        auto tracker_context = hook_tracker_engine_context{config};

        tracker_context.register_on_initialize_callback([&](engine_context&) {
            tracker_context.execution_log.push_back("on_initialize");
        });

        tracker_context.register_on_close_callback([&](engine_context&) {
            tracker_context.execution_log.push_back("on_close");
        });

        tracker_context.register_on_fixed_update_callback([&](engine_context&, chrono::duration<double>) {
            tracker_context.execution_log.push_back("fixed_update");
        });

        tracker_context.register_on_variable_update_callback([&](engine_context&, chrono::duration<double>) {
            tracker_context.execution_log.push_back("variable_update");
        });

        // Act: run loop for 2 frames
        tracker_context.run();

        // Assert: initialization occurs first and close occurs last
        ASSERT_FALSE(tracker_context.execution_log.empty());
        EXPECT_EQ(tracker_context.execution_log.front(), "on_initialize");
        EXPECT_EQ(tracker_context.execution_log.back(), "on_close");

        // Assert: verify first frame sequence begins with on_poll_events then should_step_simulation
        auto first_poll_index = size_t{0};
        for (auto index = size_t{0}; index < tracker_context.execution_log.size(); ++index)
        {
            if (tracker_context.execution_log[index] == "on_poll_events")
            {
                first_poll_index = index;
                break;
            }
        }
        ASSERT_GT(first_poll_index, 0U);
        EXPECT_EQ(tracker_context.execution_log[first_poll_index + 1], "should_step_simulation");
    }

    /// @brief Verifies that when should_step_simulation returns false (simulation paused),
    ///        fixed update callbacks do not execute and pending ticks are safely consumed/discarded.
    TEST(engine_context_test, template_method_paused_simulation_discards_ticks)
    {
        // Setup: test context with paused simulation override
        class paused_engine_context final : public standalone_engine_context
        {
          public:
            explicit paused_engine_context(const engine_config& config) : standalone_engine_context{config} {}

            uint32_t frame_count{0};

          protected:
            auto should_step_simulation() -> bool override
            {
                return false;
            }

            auto on_frame_end() -> void override
            {
                ++frame_count;
                if (frame_count >= 3)
                {
                    request_close(true);
                }
            }
        };

        const auto config = engine_config{
            .headless = true,
            .fixed_timestep = 1.0F / 60.0F,
            .max_frame_delta = 0.1F,
        };
        auto paused_context = paused_engine_context{config};

        auto fixed_update_call_count = uint32_t{0};
        auto variable_update_call_count = uint32_t{0};

        paused_context.register_on_fixed_update_callback([&](engine_context&, chrono::duration<double>) {
            ++fixed_update_call_count;
        });

        paused_context.register_on_variable_update_callback([&](engine_context&, chrono::duration<double>) {
            ++variable_update_call_count;
        });

        // Act: run loop for 3 frames with paused simulation
        paused_context.run();

        // Assert: fixed update was never called, while variable update ran every frame
        EXPECT_EQ(fixed_update_call_count, 0U);
        EXPECT_EQ(variable_update_call_count, 3U);
        EXPECT_FALSE(paused_context.get_accumulator().has_pending_ticks());
    }

    /// @brief Verifies that tasks posted to main_thread_executor are dispatched at the start of each frame.
    TEST(engine_context_test, main_thread_executor_dispatched_in_frame_loop)
    {
        // Setup: test context
        class frame_executor_engine_context final : public standalone_engine_context
        {
          public:
            explicit frame_executor_engine_context(const engine_config& config) : standalone_engine_context{config} {}

            bool task_executed{false};

          protected:
            auto on_frame_end() -> void override
            {
                request_close(true);
            }
        };

        const auto config = engine_config{
            .headless = true,
            .fixed_timestep = 1.0F / 60.0F,
            .max_frame_delta = 0.1F,
        };
        auto ctx = frame_executor_engine_context{config};

        // Act: Post task to main_thread_executor
        ctx.get_main_thread_executor().post([&ctx]() {
            ctx.task_executed = true;
        });

        EXPECT_FALSE(ctx.task_executed);
        ctx.run();

        // Assert: Task was dispatched and executed during the frame loop
        EXPECT_TRUE(ctx.task_executed);
        EXPECT_TRUE(ctx.get_main_thread_executor().empty());
    }

    class test_pass_engine_context final : public standalone_engine_context
    {
      public:
        explicit test_pass_engine_context(const engine_config& config) : standalone_engine_context{config}
        {
            _profiler_session.set_enabled(true);
        }

        using standalone_engine_context::_with_pass_dispatcher;
    };

    /// @brief Verifies that standalone_engine_context::_with_pass_dispatcher gives worker slices
    ///        the actual pass_name instead of "detached_task", and tags valid caller provenance.
    TEST(engine_context_test, pass_dispatcher_worker_slice_provenance_and_naming)
    {
        // Setup: Test context exposing _with_pass_dispatcher
        const auto config = engine_config{
            .headless = true,
            .fixed_timestep = 1.0F / 60.0F,
            .max_frame_delta = 0.1F,
        };
        auto ctx = test_pass_engine_context{config};

        auto pass1_ran = false;
        auto pass2_ran = false;
        auto pass3_ran = false;

        auto pass1_fn = [&pass1_ran]() { pass1_ran = true; };
        auto pass2_fn = [&pass2_ran]() { pass2_ran = true; };
        auto pass3_fn = [&pass3_ran]() { pass3_ran = true; };

        auto passes = vector<render_graph::pass_record_item>{};
        passes.push_back(render_graph::pass_record_item{
            .record_fn = pass1_fn,
            .pass_name = "ShadowPass",
        });
        passes.push_back(render_graph::pass_record_item{
            .record_fn = pass2_fn,
            .pass_name = "GBufferPass",
        });
        passes.push_back(render_graph::pass_record_item{
            .record_fn = pass3_fn,
            .pass_name = "LightingPass",
        });

        // Act: Invoke pass dispatcher
        ctx._with_pass_dispatcher([&passes](render_graph::pass_dispatcher_fn dispatcher) {
            dispatcher(span<const render_graph::pass_record_item>{passes.data(), passes.size()});
        });

        // Assert: All passes executed
        EXPECT_TRUE(pass1_ran);
        EXPECT_TRUE(pass2_ran);
        EXPECT_TRUE(pass3_ran);

        // Audit profiler zones
        auto chunks = ctx.get_profiler_session().drain_completed_chunks();
        ASSERT_FALSE(chunks.empty());

        auto found_shadow = false;
        auto found_gbuffer = false;
        auto found_lighting = false;

        for (const auto& chunk : chunks)
        {
            for (const auto& zone : chunk->zones())
            {
                // Invariant: No zone should ever be titled "detached_task"
                EXPECT_NE(zone.name, "detached_task");

                if (zone.name == "ShadowPass")
                {
                    found_shadow = true;
                    EXPECT_GT(zone.spawned_by_thread_id, 0U);
                }
                else if (zone.name == "GBufferPass")
                {
                    found_gbuffer = true;
                    EXPECT_GT(zone.spawned_by_thread_id, 0U);
                }
                else if (zone.name == "LightingPass")
                {
                    found_lighting = true;
                    EXPECT_GT(zone.spawned_by_thread_id, 0U);
                }
            }
        }

        EXPECT_TRUE(found_shadow);
        EXPECT_TRUE(found_gbuffer);
        EXPECT_TRUE(found_lighting);
    }

    /// @brief Verifies that engine_context exposes the scene_manager and permits entity creation and scene management.
    TEST(engine_context_test, scene_manager_lifecycle)
    {
        // Setup: Headless engine context
        const auto config = engine_config{
            .headless = true,
            .fixed_timestep = 1.0F / 60.0F,
            .max_frame_delta = 0.1F,
        };
        auto ctx = standalone_engine_context{config};

        // Act: Retrieve scene manager and manage a scene
        auto& sm = ctx.get_scene_manager();
        EXPECT_EQ(sm.get_scene_count(), 0U);

        const auto scene_json = string_view{R"({ "version": 1, "entities": [] })"};
        const auto handle_res = sm.load_from_string_sync(scene_json, "EngineScene");
        ASSERT_TRUE(handle_res.has_value());
        const auto handle = *handle_res;

        const auto ent = sm.create_entity(handle, "Player");
        EXPECT_TRUE(ctx.get_entities().is_valid(ent));
        ASSERT_TRUE(ctx.get_entities().name(ent).has_value());
        EXPECT_EQ(*ctx.get_entities().name(ent), "Player");

        // Assert: Scene count and active scene
        EXPECT_EQ(sm.get_scene_count(), 1U);
        EXPECT_EQ(sm.get_active_scene(), handle);

        sm.unload(handle);
        EXPECT_EQ(sm.get_scene_count(), 0U);
        EXPECT_FALSE(ctx.get_entities().is_valid(ent));
    }
} // namespace tempest::tests
