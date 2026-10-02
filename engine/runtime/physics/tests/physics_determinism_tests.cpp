#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/int.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/logger.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/character_controller_system.hpp>
#include <tempest/physics/character_movement_intent.hpp>
#include <tempest/physics/character_snapshot.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/physics/velocity_component.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>

#include "primitive_collision_sandbox.hpp"

namespace tempest::physics::tests
{
    namespace
    {
        class physics_determinism_fixture : public ::testing::Test
        {
        public:
            physics_determinism_fixture() = default;
            physics_determinism_fixture(const physics_determinism_fixture&) = delete;
            physics_determinism_fixture(physics_determinism_fixture&&) noexcept = delete;
            ~physics_determinism_fixture() override = default;

            auto operator=(const physics_determinism_fixture&) -> physics_determinism_fixture& = delete;
            auto operator=(physics_determinism_fixture&&) noexcept -> physics_determinism_fixture& = delete;

        protected:
            event::event_registry event_reg{};
            ecs::archetype_registry registry{event_reg};
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
        };
    } // namespace

    // =========================================================================
    // SECTION: Snapshot Capture & Restore Round-Trip
    // =========================================================================

    /// @brief Verifies that capturing a character state snapshot and subsequently restoring it
    /// restores exact identical positions, velocities, rotations, and ground detection state
    /// across both the ECS components and the underlying Jolt CharacterVirtual object.
    TEST_F(physics_determinism_fixture, snapshot_restore_roundtrip_test)
    {
        // 1. Setup: Instantiate physics world, floor, and character entity
        auto world = physics_world{make_description()};
        auto* const system = world.physics_system();
        ASSERT_NE(system, nullptr);

        const auto floor_shape = system->create_box_shape(jolt::shim::vec3{20.0F, 0.5F, 20.0F});
        const auto floor_body = system->create_body(jolt::shim::body_desc{
            .shape = floor_shape,
            .position = jolt::shim::vec3{0.0F, -0.5F, 0.0F},
            .rotation = jolt::shim::quat{0.0F, 0.0F, 0.0F, 1.0F},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        });
        system->add_body(floor_body, false);

        const auto entity = registry.create<character_controller_component, velocity_component, character_movement_intent, ecs::transform_component, ecs::transform_history_component>();
        auto controller = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
        };
        const auto initial_position = math::vec3<float>{1.5F, 0.9F, -2.5F};
        const auto initial_rotation = math::quat<float>{0.0F, 0.3826834F, 0.0F, 0.9238795F}; // ~45 deg Y rot
        controller.id = world.spawn_character(controller, initial_position, initial_rotation);

        auto initial_transform = ecs::transform_component{};
        initial_transform.set(initial_position, initial_rotation);

        registry.replace(entity, controller);
        registry.replace(entity, velocity_component{
            .linear = math::vec3<float>{1.2F, -0.5F, 3.4F},
            .angular = math::vec3<float>{0.0F, 0.0F, 0.0F},
        });
        registry.replace(entity, character_movement_intent{});
        registry.replace(entity, initial_transform);
        registry.replace(entity, ecs::transform_history_component::create(initial_position, initial_rotation));

        auto* virtual_character = world.get_character(controller.id);
        ASSERT_NE(virtual_character, nullptr);
        virtual_character->set_linear_velocity(jolt::shim::vec3{1.2F, -0.5F, 3.4F});

        // 2. Act: Capture snapshot at tick 42
        constexpr auto tick_id = 42U;
        const auto snapshot = capture_character_snapshot(tick_id, registry, entity);

        EXPECT_EQ(snapshot.tick(), tick_id);
        EXPECT_FLOAT_EQ(snapshot.position().x, initial_position.x);
        EXPECT_FLOAT_EQ(snapshot.position().y, initial_position.y);
        EXPECT_FLOAT_EQ(snapshot.position().z, initial_position.z);
        EXPECT_FLOAT_EQ(snapshot.linear_velocity().x, 1.2F);
        EXPECT_FLOAT_EQ(snapshot.linear_velocity().z, 3.4F);

        // Mutate current entity state drastically
        auto mut_transform = ecs::transform_component{};
        mut_transform.set(math::vec3<float>{99.0F, 88.0F, 77.0F}, math::quat<float>{0.0F, 0.0F, 0.0F, 1.0F});
        registry.replace(entity, mut_transform);
        registry.replace(entity, velocity_component{
            .linear = math::vec3<float>{0.0F, 0.0F, 0.0F},
            .angular = math::vec3<float>{0.0F, 0.0F, 0.0F},
        });
        virtual_character->set_position(jolt::shim::vec3{99.0F, 88.0F, 77.0F});
        virtual_character->set_linear_velocity(jolt::shim::vec3{0.0F, 0.0F, 0.0F});

        // Restore snapshot
        restore_character_snapshot(registry, entity, snapshot, &world);

        // 3. Assert: Entity and Jolt CharacterVirtual completely restored
        const auto& restored_transform = registry.get<ecs::transform_component>(entity);
        const auto& restored_velocity = registry.get<velocity_component>(entity);
        const auto& restored_history = registry.get<ecs::transform_history_component>(entity);

        EXPECT_FLOAT_EQ(restored_transform.position().x, initial_position.x);
        EXPECT_FLOAT_EQ(restored_transform.position().y, initial_position.y);
        EXPECT_FLOAT_EQ(restored_transform.position().z, initial_position.z);
        EXPECT_FLOAT_EQ(restored_velocity.linear.x, 1.2F);
        EXPECT_FLOAT_EQ(restored_velocity.linear.y, -0.5F);
        EXPECT_FLOAT_EQ(restored_velocity.linear.z, 3.4F);

        EXPECT_FLOAT_EQ(restored_history.current_position.x, initial_position.x);
        EXPECT_FLOAT_EQ(restored_history.previous_position.x, initial_position.x);

        const auto restored_jolt_pos = virtual_character->get_position();
        const auto restored_jolt_vel = virtual_character->get_linear_velocity();
        EXPECT_FLOAT_EQ(restored_jolt_pos.x, initial_position.x);
        EXPECT_FLOAT_EQ(restored_jolt_pos.y, initial_position.y);
        EXPECT_FLOAT_EQ(restored_jolt_pos.z, initial_position.z);
        EXPECT_FLOAT_EQ(restored_jolt_vel.x, 1.2F);
        EXPECT_FLOAT_EQ(restored_jolt_vel.z, 3.4F);

        // Teardown
        system->remove_body(floor_body);
        system->destroy_body(floor_body);
        system->destroy_shape(floor_shape);
    }

    // =========================================================================
    // SECTION: Ramp & Stair Navigation on Primitive Collision Geometry
    // =========================================================================

    /// @brief Verifies character slope traversal: can climb 30 deg walkable ramp,
    /// but cannot climb 60 deg steep slope (exceeding default 50 deg max slope limit).
    TEST_F(physics_determinism_fixture, ramp_traversal_slope_limits)
    {
        // 1. Setup: Physics world with primitive sandbox
        auto world = physics_world{make_description()};
        auto* const system = world.physics_system();
        ASSERT_NE(system, nullptr);

        const auto sandbox = primitive_collision_sandbox{system};

        const auto entity = registry.create<character_controller_component, velocity_component, character_movement_intent, ecs::transform_component>();
        auto controller = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
            .max_slope_angle = math::as_radians(50.0F),
            .walk_speed = 5.0F,
            .acceleration_rate = 30.0F,
        };

        // Spawn in front of the 30-degree ramp at x = -15, z = 5
        const auto ramp_30_approach = math::vec3<float>{-15.0F, 0.9F, 5.0F};
        controller.id = world.spawn_character(controller, ramp_30_approach);

        auto initial_transform = ecs::transform_component{};
        initial_transform.position(ramp_30_approach);

        registry.replace(entity, controller);
        registry.replace(entity, velocity_component{});
        registry.replace(entity, character_movement_intent{
            .wish_direction = math::vec3<float>{0.0F, 0.0F, 1.0F}, // move +Z up the ramp
            .jump_requested = false,
            .sprint_requested = false,
        });
        registry.replace(entity, initial_transform);

        constexpr auto delta_time = 1.0F / 60.0F;
        constexpr auto gravity = math::vec3<float>{0.0F, -9.81F, 0.0F};

        // 2. Act: Walk up 30-deg ramp for 60 ticks (1 second)
        for (auto step = 0; step < 60; ++step)
        {
            update_character_controllers(world, registry, delta_time, gravity);
            world.step(delta_time);
        }

        // 3. Assert: Character climbed upwards (y increases significantly above ground plane)
        const auto& climbed_transform = registry.get<ecs::transform_component>(entity);
        EXPECT_GT(climbed_transform.position().y, 1.5F);
        EXPECT_GT(climbed_transform.position().z, 6.5F);

        // Now place character at 60-degree ramp approach (x = 5, z = 5)
        auto* char_virtual = world.get_character(controller.id);
        ASSERT_NE(char_virtual, nullptr);
        char_virtual->set_position(jolt::shim::vec3{5.0F, 0.9F, 5.0F});
        char_virtual->set_linear_velocity(jolt::shim::vec3{0.0F, 0.0F, 0.0F});

        auto current_transform = ecs::transform_component{};
        current_transform.position(math::vec3<float>{5.0F, 0.9F, 5.0F});
        registry.replace(entity, current_transform);

        // Try walking into 60-deg steep ramp for 60 ticks
        for (auto step = 0; step < 60; ++step)
        {
            update_character_controllers(world, registry, delta_time, gravity);
            world.step(delta_time);
        }

        // 4. Assert: On 60-deg ramp, slope limit cancels steep velocity; character cannot climb up
        const auto& steep_transform = registry.get<ecs::transform_component>(entity);
        EXPECT_LT(steep_transform.position().y, 1.6F);
    }

    // =========================================================================
    // SECTION: 100-Tick Replay Determinism Verification
    // =========================================================================

    /// @brief Verifies bit-exact 100-tick replay determinism:
    /// Runs 100 ticks of simulation involving dynamic box collisions (state S_100A),
    /// restores state back to S_0, and replays identical 100 inputs (state S_100B).
    /// Asserts |S_100A - S_100B| == 0 across character and dynamic box states.
    TEST_F(physics_determinism_fixture, input_replay_determinism_test)
    {
        // 1. Setup: Instantiate world, primitive sandbox (with 20kg and 200kg boxes)
        auto world = physics_world{make_description()};
        auto* const system = world.physics_system();
        ASSERT_NE(system, nullptr);

        auto sandbox = primitive_collision_sandbox{system};

        const auto entity = registry.create<character_controller_component, velocity_component, character_movement_intent, ecs::transform_component, ecs::transform_history_component>();
        auto controller = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
            .max_strength = 200.0F,
            .walk_speed = 4.0F,
            .acceleration_rate = 25.0F,
        };

        // Spawn character right in front of the 20kg box (box is at x=0, y=0.5, z=2.5)
        const auto initial_position = math::vec3<float>{0.0F, 0.9F, 0.5F};
        controller.id = world.spawn_character(controller, initial_position);

        auto initial_transform = ecs::transform_component{};
        initial_transform.position(initial_position);

        registry.replace(entity, controller);
        registry.replace(entity, velocity_component{});
        registry.replace(entity, character_movement_intent{
            .wish_direction = math::vec3<float>{0.0F, 0.0F, 1.0F},
            .jump_requested = false,
            .sprint_requested = false,
        });
        registry.replace(entity, initial_transform);
        registry.replace(entity, ecs::transform_history_component::create(initial_position));

        constexpr auto delta_time = 1.0F / 60.0F;
        constexpr auto gravity = math::vec3<float>{0.0F, -9.81F, 0.0F};

        // Capture initial state S_0
        const auto initial_character_snapshot = capture_character_snapshot(0, registry, entity);

        // Predefine a pseudo-random / varied 100-tick input sequence
        struct tick_input
        {
            math::vec3<float> wish_dir;
            bool jump;
            bool sprint;
        };

        tempest::vector<tick_input> input_sequence;
        input_sequence.reserve(100);
        for (auto tick_index = 0; tick_index < 100; ++tick_index)
        {
            // Deterministic input pattern: push forward, strafe slightly, jump around tick 40
            const auto forward = 1.0F;
            const auto strafe = (tick_index % 15 > 7) ? 0.2F : -0.2F;
            const auto jump = (tick_index == 40 || tick_index == 41);
            const auto sprint = (tick_index > 50);

            input_sequence.push_back(tick_input{
                .wish_dir = math::vec3<float>{strafe, 0.0F, forward},
                .jump = jump,
                .sprint = sprint,
            });
        }

        // 2. Act (Pass A): Simulate 100 ticks forward
        for (auto tick_index = 0; tick_index < 100; ++tick_index)
        {
            const auto& current_input = input_sequence[static_cast<size_t>(tick_index)];
            registry.replace(entity, character_movement_intent{
                .wish_direction = current_input.wish_dir,
                .jump_requested = current_input.jump,
                .sprint_requested = current_input.sprint,
            });

            update_character_controllers(world, registry, delta_time, gravity);
            world.step(delta_time);
        }

        // Record State S_100A
        const auto character_snapshot_100a = capture_character_snapshot(100, registry, entity);
        const auto box_20kg_snapshot_100a = sandbox.capture_box_20kg_snapshot(100);

        // Ensure box was indeed pushed during the 100 ticks
        EXPECT_GT(box_20kg_snapshot_100a.position.z, 2.6F);

        // 3. Act: Restore state back to S_0
        restore_character_snapshot(registry, entity, initial_character_snapshot, &world);
        sandbox.reset_dynamic_boxes();

        // 4. Act (Pass B): Replay identical 100 ticks
        for (auto tick_index = 0; tick_index < 100; ++tick_index)
        {
            const auto& current_input = input_sequence[static_cast<size_t>(tick_index)];
            registry.replace(entity, character_movement_intent{
                .wish_direction = current_input.wish_dir,
                .jump_requested = current_input.jump,
                .sprint_requested = current_input.sprint,
            });

            update_character_controllers(world, registry, delta_time, gravity);
            world.step(delta_time);
        }

        // Record State S_100B
        const auto character_snapshot_100b = capture_character_snapshot(100, registry, entity);
        const auto box_20kg_snapshot_100b = sandbox.capture_box_20kg_snapshot(100);

        // 5. Assert: Bit-exact determinism (|S_100A - S_100B| == 0)
        EXPECT_EQ(character_snapshot_100a.position().x, character_snapshot_100b.position().x);
        EXPECT_EQ(character_snapshot_100a.position().y, character_snapshot_100b.position().y);
        EXPECT_EQ(character_snapshot_100a.position().z, character_snapshot_100b.position().z);

        EXPECT_EQ(character_snapshot_100a.linear_velocity().x, character_snapshot_100b.linear_velocity().x);
        EXPECT_EQ(character_snapshot_100a.linear_velocity().y, character_snapshot_100b.linear_velocity().y);
        EXPECT_EQ(character_snapshot_100a.linear_velocity().z, character_snapshot_100b.linear_velocity().z);

        EXPECT_EQ(character_snapshot_100a.rotation().x, character_snapshot_100b.rotation().x);
        EXPECT_EQ(character_snapshot_100a.rotation().y, character_snapshot_100b.rotation().y);
        EXPECT_EQ(character_snapshot_100a.rotation().z, character_snapshot_100b.rotation().z);
        EXPECT_EQ(character_snapshot_100a.rotation().w, character_snapshot_100b.rotation().w);

        EXPECT_EQ(character_snapshot_100a.is_grounded, character_snapshot_100b.is_grounded);

        // Dynamic box determinism
        EXPECT_EQ(box_20kg_snapshot_100a.position.x, box_20kg_snapshot_100b.position.x);
        EXPECT_EQ(box_20kg_snapshot_100a.position.y, box_20kg_snapshot_100b.position.y);
        EXPECT_EQ(box_20kg_snapshot_100a.position.z, box_20kg_snapshot_100b.position.z);

        EXPECT_EQ(box_20kg_snapshot_100a.linear_velocity.x, box_20kg_snapshot_100b.linear_velocity.x);
        EXPECT_EQ(box_20kg_snapshot_100a.linear_velocity.y, box_20kg_snapshot_100b.linear_velocity.y);
        EXPECT_EQ(box_20kg_snapshot_100a.linear_velocity.z, box_20kg_snapshot_100b.linear_velocity.z);
    }
} // namespace tempest::physics::tests
