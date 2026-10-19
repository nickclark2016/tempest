#include <gtest/gtest.h>

#include <tempest/ecs_events.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/play_mode_controller.hpp>
#include <tempest/transform_component.hpp>

// ============================================================================
// Play Mode Controller Tests
// ============================================================================

/// @brief Verifies that play_mode_controller defaults to edit mode on initialization.
TEST(play_mode_controller_test, initial_state_is_edit)
{
    auto engine_ctx = tempest::editor::editor_engine_context{};
    auto& controller = engine_ctx.get_play_mode_controller();

    EXPECT_TRUE(controller.is_edit());
    EXPECT_FALSE(controller.is_playing());
    EXPECT_FALSE(controller.is_paused());
    EXPECT_EQ(controller.get_state(), tempest::editor::simulation_state::edit);
}

/// @brief Verifies state machine transitions between edit, playing, and paused states.
TEST(play_mode_controller_test, play_pause_resume_stop_state_transitions)
{
    // Setup: Initialize editor engine context
    auto engine_ctx = tempest::editor::editor_engine_context{};
    auto& controller = engine_ctx.get_play_mode_controller();

    // Act & Assert: edit -> playing
    EXPECT_TRUE(controller.play());
    EXPECT_TRUE(controller.is_playing());
    EXPECT_EQ(controller.get_state(), tempest::editor::simulation_state::playing);

    // Act & Assert: playing -> paused
    controller.pause();
    EXPECT_TRUE(controller.is_paused());
    EXPECT_EQ(controller.get_state(), tempest::editor::simulation_state::paused);

    // Act & Assert: paused -> playing (resume)
    EXPECT_TRUE(controller.play());
    EXPECT_TRUE(controller.is_playing());
    EXPECT_EQ(controller.get_state(), tempest::editor::simulation_state::playing);

    // Act & Assert: playing -> edit (stop)
    controller.stop();
    EXPECT_TRUE(controller.is_edit());
    EXPECT_EQ(controller.get_state(), tempest::editor::simulation_state::edit);
}

/// @brief Verifies that step is only permitted when paused, and is rejected in edit or playing states.
TEST(play_mode_controller_test, step_rejection_and_execution)
{
    // Setup: Initialize context
    auto engine_ctx = tempest::editor::editor_engine_context{};
    auto& controller = engine_ctx.get_play_mode_controller();

    // Assert: In edit state, step is rejected
    EXPECT_FALSE(controller.step());

    // Act: Transition to playing state
    EXPECT_TRUE(controller.play());

    // Assert: In playing state, step is rejected
    EXPECT_FALSE(controller.step());

    // Act: Transition to paused state
    controller.pause();

    // Assert: In paused state, step succeeds
    EXPECT_TRUE(controller.step());

    controller.stop();
}

/// @brief Verifies that stop restores the editor camera position and the ECS world state.
TEST(play_mode_controller_test, play_stop_restores_ecs_and_editor_camera_pose)
{
    // Setup: Initialize context and prepare pre-play world and camera
    auto engine_ctx = tempest::editor::editor_engine_context{};
    auto& controller = engine_ctx.get_play_mode_controller();
    auto& camera = engine_ctx.get_editor_camera();
    auto& entities = engine_ctx.get_entities();

    const auto pre_play_cam_pos = tempest::math::vec3<float>{12.0F, 34.0F, 56.0F};
    camera.set_position(pre_play_cam_pos);

    const auto pre_play_entity = entities.create();
    entities.name(pre_play_entity, "InitialEntity");

    // Act: Enter play mode
    EXPECT_TRUE(controller.play());

    // Mutate camera pose during play mode
    camera.move(10.0F, 20.0F, 30.0F);
    EXPECT_NE(camera.get_position(), pre_play_cam_pos);

    // Mutate ECS world during play mode
    const auto session_entity = entities.create();
    entities.name(session_entity, "SessionSpawned");
    entities.destroy(pre_play_entity);

    EXPECT_FALSE(entities.is_valid(pre_play_entity));
    EXPECT_TRUE(entities.is_valid(session_entity));

    // Act: Stop play mode
    controller.stop();

    // Assert: Camera pose is restored to pre-play values
    EXPECT_EQ(camera.get_position(), pre_play_cam_pos);

    // Assert: ECS world is restored to pre-play state
    EXPECT_TRUE(entities.is_valid(pre_play_entity));
    EXPECT_FALSE(entities.is_valid(session_entity));
    const auto found = entities.find_first_with_name("InitialEntity");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, pre_play_entity);
}

/// @brief Verifies that simulation lifecycle events and world_restored_event are published during transitions.
TEST(play_mode_controller_test, lifecycle_events_dispatched_correctly)
{
    // Setup: Track event counts
    auto engine_ctx = tempest::editor::editor_engine_context{};
    auto& controller = engine_ctx.get_play_mode_controller();
    auto& events = engine_ctx.get_events();

    auto started_count = 0;
    auto paused_count = 0;
    auto stopped_count = 0;
    auto restored_count = 0;
    auto observed_state_on_restore = tempest::editor::simulation_state::playing;
    auto observed_is_play_mode_on_restore = true;

    [[maybe_unused]] const auto h_started =
        events.dispatcher<tempest::ecs::simulation_started_event>().subscribe(
            [&](const auto&) { ++started_count; });

    [[maybe_unused]] const auto h_paused =
        events.dispatcher<tempest::ecs::simulation_paused_event>().subscribe(
            [&](const auto&) { ++paused_count; });

    [[maybe_unused]] const auto h_stopped =
        events.dispatcher<tempest::ecs::simulation_stopped_event>().subscribe(
            [&](const auto&) { ++stopped_count; });

    [[maybe_unused]] const auto h_restored =
        events.dispatcher<tempest::ecs::world_restored_event>().subscribe(
            [&](const auto&) {
                ++restored_count;
                observed_state_on_restore = controller.get_state();
                observed_is_play_mode_on_restore = engine_ctx.is_play_mode();
            });

    // Act: play -> pause -> stop
    EXPECT_TRUE(controller.play());
    EXPECT_EQ(started_count, 1);

    controller.pause();
    EXPECT_EQ(paused_count, 1);

    controller.stop();
    EXPECT_EQ(stopped_count, 1);
    EXPECT_EQ(restored_count, 1);
    EXPECT_EQ(observed_state_on_restore, tempest::editor::simulation_state::edit);
    EXPECT_FALSE(observed_is_play_mode_on_restore);
}

/// @brief Verifies that stepping while paused executes a single simulation tick.
TEST(play_mode_controller_test, step_executes_single_simulation_tick)
{
    // Setup: Register fixed update callback
    auto engine_ctx = tempest::editor::editor_engine_context{};
    auto& controller = engine_ctx.get_play_mode_controller();

    auto tick_count = 0;
    engine_ctx.register_on_fixed_update_callback(
        [&](tempest::engine_context&, tempest::chrono::duration<double>) {
            ++tick_count;
        });

    // Act: Play, Pause, then Step
    EXPECT_TRUE(controller.play());
    controller.pause();

    EXPECT_EQ(tick_count, 0);

    EXPECT_TRUE(controller.step());
    EXPECT_EQ(tick_count, 1);

    EXPECT_TRUE(controller.step());
    EXPECT_EQ(tick_count, 2);

    controller.stop();
}

/// @brief Verifies that play_mode_controller invokes registered on_play_begin and on_play_end callbacks across Play -> Stop transitions.
TEST(play_mode_controller_test, play_begin_and_play_end_callbacks)
{
    // Setup: Initialize context and controller
    auto engine_ctx = tempest::editor::editor_engine_context{};
    auto& controller = engine_ctx.get_play_mode_controller();

    auto begin_call_count = 0;
    auto end_call_count = 0;
    auto observed_mode = tempest::network_mode::embedded_server;
    auto observed_port = tempest::uint16_t{0};

    controller.set_on_play_begin([&](const tempest::play_settings& settings) {
        ++begin_call_count;
        observed_mode = settings.mode;
        observed_port = settings.port;
    });

    controller.set_on_play_end([&]() {
        ++end_call_count;
    });

    // Act: Enter play mode with custom settings
    const auto custom_settings = tempest::play_settings{
        .mode = tempest::network_mode::connect_remote,
        .remote_address = "192.168.1.100",
        .port = 8888,
        .start_embedded_server = false,
    };

    EXPECT_TRUE(controller.play(custom_settings));

    // Assert: on_play_begin was called with the provided settings
    EXPECT_EQ(begin_call_count, 1);
    EXPECT_EQ(end_call_count, 0);
    EXPECT_EQ(observed_mode, tempest::network_mode::connect_remote);
    EXPECT_EQ(observed_port, 8888);

    // Act: Stop play mode
    controller.stop();

    // Assert: on_play_end was called
    EXPECT_EQ(begin_call_count, 1);
    EXPECT_EQ(end_call_count, 1);
}

