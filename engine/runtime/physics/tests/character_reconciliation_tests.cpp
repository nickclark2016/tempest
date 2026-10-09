#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/int.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/logger.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/network/user_cmd.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/character_controller_system.hpp>
#include <tempest/physics/character_movement_intent.hpp>
#include <tempest/physics/character_prediction.hpp>
#include <tempest/physics/character_reconciliation.hpp>
#include <tempest/physics/character_snapshot.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/physics/velocity_component.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>

namespace tempest::physics::tests
{
    namespace
    {
        class character_reconciliation_fixture : public ::testing::Test
        {
        public:
            character_reconciliation_fixture() = default;
            character_reconciliation_fixture(const character_reconciliation_fixture&) = delete;
            character_reconciliation_fixture(character_reconciliation_fixture&&) noexcept = delete;
            ~character_reconciliation_fixture() override = default;

            auto operator=(const character_reconciliation_fixture&) -> character_reconciliation_fixture& = delete;
            auto operator=(character_reconciliation_fixture&&) noexcept -> character_reconciliation_fixture& = delete;

        protected:
            event::event_registry event_reg{};
            ecs::component_type_registry type_reg{};
            ecs::archetype_registry registry{event_reg, type_reg};
            logger test_logger{};
            profiler::profiler_session test_profiler{false};
            job::job_system test_job_system{test_logger, test_profiler, job::job_system_config{
                .performance_worker_count = 2,
                .efficiency_worker_count = 0,
            }};

            [[nodiscard]] auto make_description() -> jolt::shim::init_desc
            {
                auto description = jolt::shim::init_desc{};
                description.max_bodies = jolt::shim::default_max_bodies;
                description.max_body_pairs = jolt::shim::default_max_body_pairs;
                description.max_contact_constraints = jolt::shim::default_max_contact_constraints;

                description.job_dispatch = [](jolt::shim::job_execute_fn execution_callback, void* job_context, void* user_data) {
                    auto* const current_job_system = static_cast<job::job_system*>(user_data);
                    auto launch = [](job::job_allocator&, job::job_system&,
                                     jolt::shim::job_execute_fn execution_fn, void* job_argument) -> job::detail::detached_task {
                        execution_fn(job_argument);
                        co_return;
                    };
                    auto detached_job = launch(current_job_system->get_dispatch_allocator(), *current_job_system, execution_callback, job_context);
                    current_job_system->schedule(detached_job.handle, job::task_priority::normal, job::core_class::any);
                };
                description.job_dispatcher_user_data = &test_job_system;

                description.warn = [](const char* message, void* user_data) {
                    if (user_data != nullptr)
                    {
                        auto* const logger_instance = static_cast<logger*>(user_data);
                        logger_instance->warn(string_view{message});
                    }
                };
                description.error = [](const char* message, void* user_data) {
                    if (user_data != nullptr)
                    {
                        auto* const logger_instance = static_cast<logger*>(user_data);
                        logger_instance->error(string_view{message});
                    }
                };
                description.log_user_data = &test_logger;

                return description;
            }

            auto create_static_box(jolt::shim::physics_system* system,
                                   jolt::shim::vec3 half_extents,
                                   jolt::shim::vec3 position) -> jolt::shim::body_id
            {
                const auto shape = system->create_box_shape(half_extents);
                const auto desc = jolt::shim::body_desc{
                    .shape = shape,
                    .position = position,
                    .rotation = jolt::shim::quat{
                        .x = 0.0F,
                        .y = 0.0F,
                        .z = 0.0F,
                        .w = 1.0F,
                    },
                    .motion = jolt::shim::motion_type::static_motion,
                    .layer = jolt::shim::object_layer::non_moving,
                };
                const auto body = system->create_body(desc);
                system->add_body(body, false);
                return body;
            }

            auto spawn_test_player(physics_world& world, math::vec3<float> initial_position) -> ecs::entity
            {
                const auto player_entity = registry.create<character_controller_component,
                                                           velocity_component,
                                                           character_movement_intent,
                                                           ecs::transform_component,
                                                           ecs::transform_history_component,
                                                           reconciliation_smoothing_component>();

                auto controller = character_controller_component{
                    .character_height = 1.8F,
                    .character_radius = 0.4F,
                    .jump_height = 1.5F,
                };
                controller.id = world.spawn_character(controller, initial_position);

                auto transform = ecs::transform_component{};
                transform.position(initial_position);

                registry.replace(player_entity, controller);
                registry.replace(player_entity, velocity_component{});
                registry.replace(player_entity, character_movement_intent{});
                registry.replace(player_entity, transform);
                registry.replace(player_entity, ecs::transform_history_component::create(initial_position));
                registry.replace(player_entity, reconciliation_smoothing_component{});

                return player_entity;
            }
        };
    }

    // =========================================================================
    // SECTION: In-Sync Acknowledgment & Buffer Discard
    // =========================================================================

    /// @brief Verifies that when an incoming server snapshot matches the predicted state
    /// within error_threshold, no rollback is performed (reconciled = false), visual smoothing
    /// offset remains zero, and acknowledged inputs up to that tick are discarded.
    TEST_F(character_reconciliation_fixture, reconciliation_in_sync_discards_acknowledged_frames)
    {
        // 1. Setup: Floor and character entity with circular prediction buffer
        auto world = physics_world{make_description()};
        auto* const system = world.physics_system();
        ASSERT_NE(system, nullptr);

        const auto floor_body = create_static_box(system, jolt::shim::vec3{20.0F, 1.0F, 20.0F}, jolt::shim::vec3{0.0F, -1.0F, 0.0F});
        const auto player_entity = spawn_test_player(world, math::vec3<float>{0.0F, 1.0F, 0.0F});
        auto buffer = character_prediction_buffer{};
        constexpr auto delta_time = 1.0F / 60.0F;

        // Execute 5 forward predictive ticks
        for (uint32_t tick = 1; tick <= 5; ++tick)
        {
            const auto cmd = network::user_cmd{
                .tick = tick,
                .forward_move = 1.0F,
                .right_move = 0.0F,
                .view_yaw = 0.0F,
                .buttons = network::user_button_none,
            };
            step_client_prediction(world, registry, player_entity, cmd, delta_time, buffer);
        }

        EXPECT_TRUE(buffer.has_frame(1));
        EXPECT_TRUE(buffer.has_frame(3));
        EXPECT_TRUE(buffer.has_frame(5));

        // 2. Act: Receive matching server state snapshot for tick 3
        const auto* const predicted_frame_3 = buffer.get_frame(3);
        ASSERT_NE(predicted_frame_3, nullptr);
        const auto server_snapshot = predicted_frame_3->predicted_state;

        const auto result = reconcile_client_character(world, registry, player_entity, buffer, server_snapshot);

        // 3. Assert: In-sync acknowledgment; no rollback or visual error accumulation
        EXPECT_FALSE(result.diverged);
        EXPECT_FALSE(result.reconciled);
        EXPECT_FALSE(result.snapped);
        EXPECT_EQ(result.resimulated_ticks, 0U);
        EXPECT_LE(result.error_distance, 0.001F);

        // Inputs 1..3 must now be discarded from prediction buffer; 4..5 remain valid
        EXPECT_FALSE(buffer.has_frame(1));
        EXPECT_FALSE(buffer.has_frame(2));
        EXPECT_FALSE(buffer.has_frame(3));
        EXPECT_TRUE(buffer.has_frame(4));
        EXPECT_TRUE(buffer.has_frame(5));

        const auto& smoothing = registry.get<reconciliation_smoothing_component>(player_entity);
        EXPECT_FLOAT_EQ(smoothing.position_error.x, 0.0F);
        EXPECT_FLOAT_EQ(smoothing.position_error.y, 0.0F);
        EXPECT_FLOAT_EQ(smoothing.position_error.z, 0.0F);

        // Teardown
        system->remove_body(floor_body);
        system->destroy_body(floor_body);
    }

    // =========================================================================
    // SECTION: Moderate Divergence Rollback & Resimulation
    // =========================================================================

    /// @brief Verifies that a moderate divergence (e.g. 0.3m > 0.02m and <= 1.25m) triggers
    /// physical rollback to the server snapshot, replays unacknowledged inputs forward to the
    /// latest tick, updates prediction buffer snapshots, and stores the divergence error in
    /// reconciliation_smoothing_component for visual decay.
    TEST_F(character_reconciliation_fixture, reconciliation_moderate_divergence_rolls_back_and_resimulates)
    {
        // 1. Setup: Floor and character entity stepped predictively for 10 ticks
        auto world = physics_world{make_description()};
        auto* const system = world.physics_system();
        ASSERT_NE(system, nullptr);

        const auto floor_body = create_static_box(system, jolt::shim::vec3{20.0F, 1.0F, 20.0F}, jolt::shim::vec3{0.0F, -1.0F, 0.0F});
        const auto player_entity = spawn_test_player(world, math::vec3<float>{0.0F, 1.0F, 0.0F});
        auto buffer = character_prediction_buffer{};
        constexpr auto delta_time = 1.0F / 60.0F;

        for (uint32_t tick = 1; tick <= 10; ++tick)
        {
            const auto cmd = network::user_cmd{
                .tick = tick,
                .forward_move = 1.0F,
                .right_move = 0.0F,
                .view_yaw = 0.0F,
                .buttons = network::user_button_none,
            };
            step_client_prediction(world, registry, player_entity, cmd, delta_time, buffer);
        }

        const auto* const predicted_frame_5 = buffer.get_frame(5);
        ASSERT_NE(predicted_frame_5, nullptr);

        // 2. Act: Inject an authoritative server state at tick 5 shifted laterally by +0.3m along X
        auto server_snapshot = predicted_frame_5->predicted_state;
        const auto lateral_offset = math::vec3<float>{0.3F, 0.0F, 0.0F};
        server_snapshot.motion.position = server_snapshot.motion.position + lateral_offset;

        const auto result = reconcile_client_character(world, registry, player_entity, buffer, server_snapshot);

        // 3. Assert: Divergence detected, rollback and 5 ticks (6..10) resimulated
        EXPECT_TRUE(result.diverged);
        EXPECT_TRUE(result.reconciled);
        EXPECT_FALSE(result.snapped);
        EXPECT_EQ(result.resimulated_ticks, 5U);
        EXPECT_NEAR(result.error_distance, 0.3F, 1e-4F);

        // Acknowledged frames 1..5 discarded; 6..10 remain valid with updated predictions
        EXPECT_FALSE(buffer.has_frame(5));
        for (uint32_t tick = 6; tick <= 10; ++tick)
        {
            const auto* const frame = buffer.get_frame(tick);
            ASSERT_NE(frame, nullptr);
            EXPECT_TRUE(frame->is_valid);
            // The resimulated frames should reflect the lateral shift along X
            EXPECT_NEAR(frame->predicted_state.position().x, 0.3F, 1e-3F);
        }

        // Current physical transform of character must reflect the resimulated state
        const auto& current_transform = registry.get<ecs::transform_component>(player_entity);
        EXPECT_NEAR(current_transform.position().x, 0.3F, 1e-3F);

        // Visual smoothing offset must hold the delta vector (+0.3 along X) to prevent a visual snap
        const auto& smoothing = registry.get<reconciliation_smoothing_component>(player_entity);
        EXPECT_NEAR(smoothing.position_error.x, 0.3F, 1e-4F);
        EXPECT_NEAR(smoothing.position_error.y, 0.0F, 1e-4F);
        EXPECT_NEAR(smoothing.position_error.z, 0.0F, 1e-4F);

        // Teardown
        system->remove_body(floor_body);
        system->destroy_body(floor_body);
    }

    // =========================================================================
    // SECTION: Hard Snap on Teleport / Large Desync
    // =========================================================================

    /// @brief Verifies that a massive divergence exceeding snap_threshold (e.g. 3.0m > 1.25m)
    /// causes a physical rollback and resimulation but hard-snaps visually (snapped = true),
    /// resetting smoothing position_error to (0, 0, 0) to avoid visual smearing.
    TEST_F(character_reconciliation_fixture, reconciliation_large_divergence_hard_snaps)
    {
        // 1. Setup: Floor and character entity stepped predictively for 6 ticks
        auto world = physics_world{make_description()};
        auto* const system = world.physics_system();
        ASSERT_NE(system, nullptr);

        const auto floor_body = create_static_box(system, jolt::shim::vec3{20.0F, 1.0F, 20.0F}, jolt::shim::vec3{0.0F, -1.0F, 0.0F});
        const auto player_entity = spawn_test_player(world, math::vec3<float>{0.0F, 1.0F, 0.0F});
        auto buffer = character_prediction_buffer{};
        constexpr auto delta_time = 1.0F / 60.0F;

        for (uint32_t tick = 1; tick <= 6; ++tick)
        {
            const auto cmd = network::user_cmd{
                .tick = tick,
                .forward_move = 1.0F,
                .right_move = 0.0F,
                .view_yaw = 0.0F,
                .buttons = network::user_button_none,
            };
            step_client_prediction(world, registry, player_entity, cmd, delta_time, buffer);
        }

        const auto* const predicted_frame_3 = buffer.get_frame(3);
        ASSERT_NE(predicted_frame_3, nullptr);

        // 2. Act: Inject an authoritative server state at tick 3 teleported by 3.0m along X
        auto server_snapshot = predicted_frame_3->predicted_state;
        server_snapshot.motion.position.x += 3.0F;

        const auto result = reconcile_client_character(world, registry, player_entity, buffer, server_snapshot);

        // 3. Assert: Large divergence detected, rollback and resimulate, but hard-snap triggered
        EXPECT_TRUE(result.diverged);
        EXPECT_TRUE(result.reconciled);
        EXPECT_TRUE(result.snapped);
        EXPECT_EQ(result.resimulated_ticks, 3U);
        EXPECT_NEAR(result.error_distance, 3.0F, 1e-4F);

        // Visual smoothing position error MUST be reset to zero (hard snap without visual smear)
        const auto& smoothing = registry.get<reconciliation_smoothing_component>(player_entity);
        EXPECT_FLOAT_EQ(smoothing.position_error.x, 0.0F);
        EXPECT_FLOAT_EQ(smoothing.position_error.y, 0.0F);
        EXPECT_FLOAT_EQ(smoothing.position_error.z, 0.0F);

        // Teardown
        system->remove_body(floor_body);
        system->destroy_body(floor_body);
    }

    // =========================================================================
    // SECTION: Visual Smoothing Exponential Decay
    // =========================================================================

    /// @brief Verifies that update_reconciliation_smoothing exponentially decays the visual
    /// error offset toward zero over successive render frames, adding the offset to transform_component
    /// without modifying physical controller or transform_history_component state.
    TEST_F(character_reconciliation_fixture, reconciliation_visual_smoothing_exponential_decay)
    {
        // 1. Setup: Entity with transform_component and reconciliation_smoothing_component
        const auto entity = registry.create<ecs::transform_component,
                                            ecs::transform_history_component,
                                            reconciliation_smoothing_component>();

        const auto base_position = math::vec3<float>{10.0F, 2.0F, 5.0F};
        auto transform = ecs::transform_component{};
        transform.position(base_position);

        auto smoothing = reconciliation_smoothing_component{
            .position_error = math::vec3<float>{0.4F, 0.0F, 0.0F},
            .rotation_error = math::quat<float>{0.0F, 0.0F, 0.0F, 1.0F},
            .decay_rate = 20.0F,
        };

        registry.replace(entity, transform);
        registry.replace(entity, ecs::transform_history_component::create(base_position));
        registry.replace(entity, smoothing);

        constexpr auto render_dt = 1.0F / 60.0F; // 60 FPS display refresh rate

        // 2. Act: Step smoothing update for 10 frames
        auto previous_error_x = 0.4F;
        for (auto frame = 0; frame < 10; ++frame)
        {
            update_reconciliation_smoothing(registry, render_dt);

            const auto& current_smoothing = registry.get<reconciliation_smoothing_component>(entity);
            const auto& current_transform = registry.get<ecs::transform_component>(entity);

            // 3. Assert: Error strictly decreases monotonically toward zero
            EXPECT_LT(current_smoothing.position_error.x, previous_error_x);
            EXPECT_GE(current_smoothing.position_error.x, 0.0F);
            previous_error_x = current_smoothing.position_error.x;

            // Transform position incorporates base_position + decaying visual error
            EXPECT_NEAR(current_transform.position().x, base_position.x + current_smoothing.position_error.x, 1e-4F);
        }

        // After 10 frames (~167ms with decay rate 20), error should have decayed by ~exp(-20 * 0.167) ~ 0.035
        const auto& final_smoothing = registry.get<reconciliation_smoothing_component>(entity);
        EXPECT_NEAR(final_smoothing.position_error.x, 0.4F * math::exp(-20.0F * (10.0F / 60.0F)), 1e-3F);

        // Transform history (physical simulation state) must remain completely untouched
        const auto& history = registry.get<ecs::transform_history_component>(entity);
        EXPECT_FLOAT_EQ(history.current_position.x, base_position.x);
        EXPECT_FLOAT_EQ(history.current_position.y, base_position.y);
        EXPECT_FLOAT_EQ(history.current_position.z, base_position.z);
    }

    // =========================================================================
    // SECTION: Successive Cumulative Corrections & Evicted Tick Handling
    // =========================================================================

    /// @brief Verifies that receiving successive corrections properly accumulates into
    /// reconciliation_smoothing_component without sudden discontinuity.
    TEST_F(character_reconciliation_fixture, reconciliation_multiple_accumulated_corrections)
    {
        // 1. Setup: Stepped character entity
        auto world = physics_world{make_description()};
        auto* const system = world.physics_system();
        ASSERT_NE(system, nullptr);

        const auto floor_body = create_static_box(system, jolt::shim::vec3{20.0F, 1.0F, 20.0F}, jolt::shim::vec3{0.0F, -1.0F, 0.0F});
        const auto player_entity = spawn_test_player(world, math::vec3<float>{0.0F, 1.0F, 0.0F});
        auto buffer = character_prediction_buffer{};
        constexpr auto delta_time = 1.0F / 60.0F;

        for (uint32_t tick = 1; tick <= 8; ++tick)
        {
            const auto cmd = network::user_cmd{
                .tick = tick,
                .forward_move = 1.0F,
                .right_move = 0.0F,
                .view_yaw = 0.0F,
                .buttons = network::user_button_none,
            };
            step_client_prediction(world, registry, player_entity, cmd, delta_time, buffer);
        }

        // 2. Act: First correction at tick 3 (+0.1m X)
        auto* frame_3 = buffer.get_frame(3);
        ASSERT_NE(frame_3, nullptr);
        auto snapshot_3 = frame_3->predicted_state;
        snapshot_3.motion.position.x += 0.1F;

        const auto result_1 = reconcile_client_character(world, registry, player_entity, buffer, snapshot_3);
        EXPECT_TRUE(result_1.reconciled);

        const auto smoothing_1 = registry.get<reconciliation_smoothing_component>(player_entity);
        EXPECT_NEAR(smoothing_1.position_error.x, 0.1F, 1e-4F);

        // Second correction at tick 6 (+0.15m X)
        auto* frame_6 = buffer.get_frame(6);
        ASSERT_NE(frame_6, nullptr);
        auto snapshot_6 = frame_6->predicted_state;
        snapshot_6.motion.position.x += 0.15F;

        const auto result_2 = reconcile_client_character(world, registry, player_entity, buffer, snapshot_6);
        EXPECT_TRUE(result_2.reconciled);

        // 3. Assert: Offsets accumulated smoothly
        const auto smoothing_2 = registry.get<reconciliation_smoothing_component>(player_entity);
        EXPECT_NEAR(smoothing_2.position_error.x, 0.25F, 1e-4F);

        // Teardown
        system->remove_body(floor_body);
        system->destroy_body(floor_body);
    }

    /// @brief Verifies that receiving an authoritative snapshot for a tick that is absent or
    /// already evicted from the prediction buffer is handled gracefully without false rollbacks.
    TEST_F(character_reconciliation_fixture, reconciliation_missing_tick_handled_gracefully)
    {
        // 1. Setup: Empty prediction buffer
        auto world = physics_world{make_description()};
        const auto player_entity = spawn_test_player(world, math::vec3<float>{0.0F, 1.0F, 0.0F});
        auto buffer = character_prediction_buffer{};

        auto snapshot = character_snapshot{};
        snapshot.motion.tick = 42;

        // 2. Act: Reconcile with no frames present
        const auto result = reconcile_client_character(world, registry, player_entity, buffer, snapshot);

        // 3. Assert: Handled safely
        EXPECT_FALSE(result.diverged);
        EXPECT_FALSE(result.reconciled);
        EXPECT_FALSE(result.snapped);
        EXPECT_EQ(result.resimulated_ticks, 0U);
    }
} // namespace tempest::physics::tests
