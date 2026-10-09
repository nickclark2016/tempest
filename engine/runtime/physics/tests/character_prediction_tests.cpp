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
        class character_prediction_fixture : public ::testing::Test
        {
        public:
            character_prediction_fixture() = default;
            character_prediction_fixture(const character_prediction_fixture&) = delete;
            character_prediction_fixture(character_prediction_fixture&&) noexcept = delete;
            ~character_prediction_fixture() override = default;

            auto operator=(const character_prediction_fixture&) -> character_prediction_fixture& = delete;
            auto operator=(character_prediction_fixture&&) noexcept -> character_prediction_fixture& = delete;

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
        };
    } // namespace

    // =========================================================================
    // SECTION: Apply User Command Intent Mapping
    // =========================================================================

    /// @brief Verifies that apply_user_cmd accurately converts view yaw and movement axes into
    /// world-space wish direction vectors and sets button flags.
    TEST_F(character_prediction_fixture, apply_user_cmd_directional_mapping)
    {
        // 1. Act & Assert: Yaw = 0 (facing +Z)
        const auto cmd_forward = network::user_cmd{
            .tick = 1,
            .forward_move = 1.0F,
            .right_move = 0.0F,
            .view_yaw = 0.0F,
            .buttons = static_cast<uint16_t>(network::user_button_jump | network::user_button_sprint),
        };
        const auto intent_forward = apply_user_cmd(cmd_forward);

        EXPECT_NEAR(intent_forward.wish_direction.x, 0.0F, 1.0e-5F);
        EXPECT_NEAR(intent_forward.wish_direction.y, 0.0F, 1.0e-5F);
        EXPECT_NEAR(intent_forward.wish_direction.z, 1.0F, 1.0e-5F);
        EXPECT_TRUE(intent_forward.jump_requested);
        EXPECT_TRUE(intent_forward.sprint_requested);

        // 2. Act & Assert: Yaw = pi/2 (facing +X)
        constexpr auto half_pi = 1.57079632679F;
        const auto cmd_right = network::user_cmd{
            .tick = 2,
            .forward_move = 1.0F,
            .right_move = 0.0F,
            .view_yaw = half_pi,
            .buttons = network::user_button_none,
        };
        const auto intent_right = apply_user_cmd(cmd_right);

        EXPECT_NEAR(intent_right.wish_direction.x, 1.0F, 1.0e-5F);
        EXPECT_NEAR(intent_right.wish_direction.y, 0.0F, 1.0e-5F);
        EXPECT_NEAR(intent_right.wish_direction.z, 0.0F, 1.0e-5F);
        EXPECT_FALSE(intent_right.jump_requested);
        EXPECT_FALSE(intent_right.sprint_requested);
    }

    // =========================================================================
    // SECTION: Client Prediction Ring Buffer Recording
    // =========================================================================

    /// @brief Verifies that stepping client prediction across multiple ticks executes forward
    /// movement and jumping, updates physical state monotonically along +Z, and correctly records
    /// input commands and state snapshots into the circular prediction buffer.
    TEST_F(character_prediction_fixture, character_prediction_step_and_buffer_record)
    {
        // 1. Setup: Physics world with static floor and character entity
        auto world = physics_world{make_description()};
        auto* const system = world.physics_system();
        ASSERT_NE(system, nullptr);

        const auto floor_body = create_static_box(system, jolt::shim::vec3{20.0F, 1.0F, 20.0F}, jolt::shim::vec3{0.0F, -1.0F, 0.0F});

        const auto player_entity = registry.create<character_controller_component, velocity_component, character_movement_intent, ecs::transform_component, ecs::transform_history_component>();
        auto controller = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
            .jump_height = 1.5F,
        };
        const auto initial_position = math::vec3<float>{0.0F, 1.0F, 0.0F};
        controller.id = world.spawn_character(controller, initial_position);

        auto initial_transform = ecs::transform_component{};
        initial_transform.position(initial_position);

        registry.replace(player_entity, controller);
        registry.replace(player_entity, velocity_component{});
        registry.replace(player_entity, character_movement_intent{});
        registry.replace(player_entity, initial_transform);
        registry.replace(player_entity, ecs::transform_history_component::create(initial_position));

        auto buffer = character_prediction_buffer{};
        constexpr auto delta_time = 1.0F / 60.0F;

        // Settle character onto the floor before beginning predictive ticks
        for (auto settle_step = 0; settle_step < 10; ++settle_step)
        {
            update_character_controllers(world, registry, delta_time, math::vec3<float>{0.0F, -9.81F, 0.0F});
            world.step(delta_time);
        }

        const auto settled_snapshot = capture_character_snapshot(0, registry, player_entity);
        ASSERT_TRUE(settled_snapshot.is_grounded);

        // 2. Act: Execute 10 predictive ticks with forward movement and jump button pressed
        auto previous_z = settled_snapshot.position().z;
        for (uint32_t tick = 1; tick <= 10; ++tick)
        {
            const auto cmd = network::user_cmd{
                .tick = tick,
                .forward_move = 1.0F,
                .right_move = 0.0F,
                .view_yaw = 0.0F,
                .buttons = network::user_button_jump,
            };

            const auto& predicted_state = step_client_prediction(world, registry, player_entity, cmd, delta_time, buffer);

            // 3. Assert: Verify returned snapshot matches tick and exhibits monotonic forward displacement
            EXPECT_EQ(predicted_state.tick(), tick);
            EXPECT_GT(predicted_state.position().z, previous_z);
            previous_z = predicted_state.position().z;

            // Verify prediction buffer state at this tick
            const auto* const frame = buffer.get_frame(tick);
            ASSERT_NE(frame, nullptr);
            EXPECT_TRUE(frame->is_valid);
            EXPECT_EQ(frame->cmd.tick, tick);
            EXPECT_EQ(frame->cmd.forward_move, 1.0F);
            EXPECT_EQ(frame->cmd.buttons, network::user_button_jump);
            EXPECT_EQ(frame->predicted_state.tick(), tick);
            EXPECT_EQ(frame->predicted_state.position().z, predicted_state.position().z);
        }

        EXPECT_EQ(buffer.latest_tick().value(), 10U);

        // Teardown
        system->remove_body(floor_body);
        system->destroy_body(floor_body);
    }

    // =========================================================================
    // SECTION: Prediction Determinism vs Direct Stepping
    // =========================================================================

    /// @brief Verifies bit-exact determinism between client prediction stepping via step_client_prediction
    /// and direct manual stepping via update_character_controllers and world.step across 60 ticks.
    TEST_F(character_prediction_fixture, prediction_determinism_vs_direct_stepping)
    {
        // 1. Setup: World A (client prediction) and World B (direct stepping) with identical initial configurations
        constexpr auto delta_time = 1.0F / 60.0F;
        constexpr auto gravity = math::vec3<float>{0.0F, -9.81F, 0.0F};

        // --- World A Setup ---
        auto world_a = physics_world{make_description()};
        auto* const system_a = world_a.physics_system();
        ASSERT_NE(system_a, nullptr);
        const auto floor_a = create_static_box(system_a, jolt::shim::vec3{20.0F, 1.0F, 20.0F}, jolt::shim::vec3{0.0F, -1.0F, 0.0F});

        auto event_reg_a = event::event_registry{};
        auto type_reg_a = ecs::component_type_registry{};
        auto registry_a = ecs::archetype_registry{event_reg_a, type_reg_a};

        const auto entity_a = registry_a.create<character_controller_component, velocity_component, character_movement_intent, ecs::transform_component, ecs::transform_history_component>();
        auto controller_a = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
            .jump_height = 1.5F,
        };
        const auto initial_position = math::vec3<float>{0.0F, 1.0F, 0.0F};
        controller_a.id = world_a.spawn_character(controller_a, initial_position);

        auto initial_transform_a = ecs::transform_component{};
        initial_transform_a.position(initial_position);

        registry_a.replace(entity_a, controller_a);
        registry_a.replace(entity_a, velocity_component{});
        registry_a.replace(entity_a, character_movement_intent{});
        registry_a.replace(entity_a, initial_transform_a);
        registry_a.replace(entity_a, ecs::transform_history_component::create(initial_position));

        auto buffer_a = character_prediction_buffer{};

        // --- World B Setup ---
        auto world_b = physics_world{make_description()};
        auto* const system_b = world_b.physics_system();
        ASSERT_NE(system_b, nullptr);
        const auto floor_b = create_static_box(system_b, jolt::shim::vec3{20.0F, 1.0F, 20.0F}, jolt::shim::vec3{0.0F, -1.0F, 0.0F});

        auto event_reg_b = event::event_registry{};
        auto type_reg_b = ecs::component_type_registry{};
        auto registry_b = ecs::archetype_registry{event_reg_b, type_reg_b};

        const auto entity_b = registry_b.create<character_controller_component, velocity_component, character_movement_intent, ecs::transform_component, ecs::transform_history_component>();
        auto controller_b = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
            .jump_height = 1.5F,
        };
        controller_b.id = world_b.spawn_character(controller_b, initial_position);

        auto initial_transform_b = ecs::transform_component{};
        initial_transform_b.position(initial_position);

        registry_b.replace(entity_b, controller_b);
        registry_b.replace(entity_b, velocity_component{});
        registry_b.replace(entity_b, character_movement_intent{});
        registry_b.replace(entity_b, initial_transform_b);
        registry_b.replace(entity_b, ecs::transform_history_component::create(initial_position));

        // 2. Act: Step 60 ticks with varied inputs (turning yaw, moving, jumping)
        for (uint32_t tick = 1; tick <= 60; ++tick)
        {
            const auto yaw = static_cast<float>(tick) * 0.05F;
            const auto forward = (tick % 5 == 0) ? -0.5F : 1.0F;
            const auto right = (tick % 3 == 0) ? 0.5F : -0.2F;
            const auto jump = (tick == 15 || tick == 16) ? network::user_button_jump : network::user_button_none;
            const auto sprint = (tick > 30) ? network::user_button_sprint : network::user_button_none;

            const auto cmd = network::user_cmd{
                .tick = tick,
                .forward_move = forward,
                .right_move = right,
                .view_yaw = yaw,
                .buttons = static_cast<uint16_t>(jump | sprint),
            };

            // World A: via step_client_prediction
            const auto& snapshot_a = step_client_prediction(world_a, registry_a, entity_a, cmd, delta_time, buffer_a, gravity);

            // World B: directly via update_character_controllers and world.step
            registry_b.replace(entity_b, apply_user_cmd(cmd));
            update_character_controllers(world_b, registry_b, delta_time, gravity);
            world_b.step(delta_time);
            const auto snapshot_b = capture_character_snapshot(tick, registry_b, entity_b);

            // 3. Assert: Bit-for-bit / exact floating point match at every tick
            EXPECT_EQ(snapshot_a.tick(), snapshot_b.tick());
            EXPECT_EQ(snapshot_a.position().x, snapshot_b.position().x);
            EXPECT_EQ(snapshot_a.position().y, snapshot_b.position().y);
            EXPECT_EQ(snapshot_a.position().z, snapshot_b.position().z);

            EXPECT_EQ(snapshot_a.linear_velocity().x, snapshot_b.linear_velocity().x);
            EXPECT_EQ(snapshot_a.linear_velocity().y, snapshot_b.linear_velocity().y);
            EXPECT_EQ(snapshot_a.linear_velocity().z, snapshot_b.linear_velocity().z);

            EXPECT_EQ(snapshot_a.rotation().x, snapshot_b.rotation().x);
            EXPECT_EQ(snapshot_a.rotation().y, snapshot_b.rotation().y);
            EXPECT_EQ(snapshot_a.rotation().z, snapshot_b.rotation().z);
            EXPECT_EQ(snapshot_a.rotation().w, snapshot_b.rotation().w);

            EXPECT_EQ(snapshot_a.is_grounded, snapshot_b.is_grounded);
            EXPECT_EQ(snapshot_a.current_ground_state, snapshot_b.current_ground_state);
        }

        // Teardown
        system_a->remove_body(floor_a);
        system_a->destroy_body(floor_a);
        system_b->remove_body(floor_b);
        system_b->destroy_body(floor_b);
    }
} // namespace tempest::physics::tests
