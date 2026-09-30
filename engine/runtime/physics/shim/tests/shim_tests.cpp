#include <gtest/gtest.h>

#include <tempest/array.hpp>
#include <tempest/int.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/logger.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/memory.hpp>
#include <tempest/physics/shim/jolt_shim.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/string_view.hpp>

namespace tempest::physics::shim::tests
{
    namespace
    {
        struct physics_system_deleter
        {
            void operator()(jolt::shim::physics_system* ptr) const noexcept
            {
                jolt::shim::destroy_physics_system(ptr);
            }
        };

        using physics_system_ptr = tempest::unique_ptr<jolt::shim::physics_system, physics_system_deleter>;

        class jolt_shim_fixture : public ::testing::Test
        {
        public:
            jolt_shim_fixture() = default;
            jolt_shim_fixture(const jolt_shim_fixture&) = delete;
            jolt_shim_fixture(jolt_shim_fixture&&) noexcept = delete;
            ~jolt_shim_fixture() override = default;

            auto operator=(const jolt_shim_fixture&) -> jolt_shim_fixture& = delete;
            auto operator=(jolt_shim_fixture&&) noexcept -> jolt_shim_fixture& = delete;

        protected:
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

        [[nodiscard]] auto quat_from_axis_angle(jolt::shim::vec3 axis, float angle_rad) -> jolt::shim::quat
        {
            const auto half_angle = angle_rad * 0.5f;
            const auto s = tempest::math::sin(half_angle);
            return jolt::shim::quat{
                .x = axis.x * s,
                .y = axis.y * s,
                .z = axis.z * s,
                .w = tempest::math::cos(half_angle),
            };
        }
    } // namespace

    // =========================================================================
    // SECTION: System Lifecycle
    // =========================================================================

    /// @brief Verifies creation, stepping, and destruction of the Jolt physics system.
    TEST_F(jolt_shim_fixture, system_lifecycle)
    {
        // 1. Setup: Initialize init descriptor and construct physics system
        const auto description = make_description();
        auto physics = physics_system_ptr{jolt::shim::create_physics_system(description)};
        ASSERT_NE(physics, nullptr);

        // 2. Act: Step the simulation for 10 frames
        constexpr auto delta_time = 1.0f / 60.0f;
        for (auto frame = 0; frame < 10; ++frame)
        {
            physics->step(delta_time);
        }

        // 3. Assert: Clean destruction without assertion or leak
        physics.reset();
        EXPECT_EQ(physics, nullptr);
    }

    // =========================================================================
    // SECTION: Rigid Body Simulation & Collision
    // =========================================================================

    /// @brief Verifies that a dynamic sphere falling under gravity lands and comes
    ///        to rest atop a static box floor.
    TEST_F(jolt_shim_fixture, rigid_body_falling_and_rest)
    {
        // 1. Setup: Create physics system, static box floor at y = -1, and dynamic sphere at y = 5
        const auto description = make_description();
        auto physics = physics_system_ptr{jolt::shim::create_physics_system(description)};
        ASSERT_NE(physics, nullptr);

        // Static floor: box half-extents (50, 1, 50) positioned at (0, -1, 0) => top surface at y = 0
        auto* floor_shape = physics->create_box_shape(jolt::shim::vec3{50.0f, 1.0f, 50.0f});
        ASSERT_NE(floor_shape, nullptr);

        const auto floor_body_desc = jolt::shim::body_desc{
            .shape = floor_shape,
            .position = jolt::shim::vec3{0.0f, -1.0f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto floor_id = physics->create_body(floor_body_desc);
        ASSERT_NE(floor_id, jolt::shim::invalid_body_id);
        physics->add_body(floor_id, false);

        // Dynamic sphere: radius 1.0 positioned at (0, 5, 0)
        auto* sphere_shape = physics->create_sphere_shape(1.0f);
        ASSERT_NE(sphere_shape, nullptr);

        const auto sphere_body_desc = jolt::shim::body_desc{
            .shape = sphere_shape,
            .position = jolt::shim::vec3{0.0f, 5.0f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .motion = jolt::shim::motion_type::dynamic,
            .layer = jolt::shim::object_layer::moving,
        };
        const auto sphere_id = physics->create_body(sphere_body_desc);
        ASSERT_NE(sphere_id, jolt::shim::invalid_body_id);
        physics->add_body(sphere_id, true);

        // 2. Act: Step simulation for 120 frames (2.0 seconds) to allow settling
        constexpr auto delta_time = 1.0f / 60.0f;
        for (auto frame = 0; frame < 120; ++frame)
        {
            physics->step(delta_time);
        }

        // 3. Assert: Sphere came to rest on top of box floor (y approx 1.0, linear velocity ~ 0)
        const auto final_position = physics->get_body_position(sphere_id);
        const auto final_velocity = physics->get_body_linear_velocity(sphere_id);

        EXPECT_NEAR(final_position.y, 1.0f, 0.15f);
        EXPECT_NEAR(final_position.x, 0.0f, 0.1f);
        EXPECT_NEAR(final_position.z, 0.0f, 0.1f);
        EXPECT_NEAR(final_velocity.y, 0.0f, 0.2f);

        // Teardown
        physics->remove_body(sphere_id);
        physics->destroy_body(sphere_id);
        physics->remove_body(floor_id);
        physics->destroy_body(floor_id);
        physics->destroy_shape(sphere_shape);
        physics->destroy_shape(floor_shape);
    }

    // =========================================================================
    // SECTION: Raycasting
    // =========================================================================

    /// @brief Verifies that a downward raycast against a box correctly detects
    ///        the top surface hit position, surface normal, and body identifier.
    TEST_F(jolt_shim_fixture, raycast_closest_hit)
    {
        // 1. Setup: Create static box at origin (0, 0, 0) with half-extents (2, 2, 2)
        const auto description = make_description();
        auto physics = physics_system_ptr{jolt::shim::create_physics_system(description)};
        ASSERT_NE(physics, nullptr);

        auto* box_shape = physics->create_box_shape(jolt::shim::vec3{2.0f, 2.0f, 2.0f});
        ASSERT_NE(box_shape, nullptr);

        const auto box_body_desc = jolt::shim::body_desc{
            .shape = box_shape,
            .position = jolt::shim::vec3{0.0f, 0.0f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto box_id = physics->create_body(box_body_desc);
        ASSERT_NE(box_id, jolt::shim::invalid_body_id);
        physics->add_body(box_id, false);

        // 2. Act: Cast ray from (0, 10, 0) straight down along -Y
        const auto query = jolt::shim::raycast_query{
            .origin = jolt::shim::vec3{0.0f, 10.0f, 0.0f},
            .direction = jolt::shim::vec3{0.0f, -1.0f, 0.0f},
            .max_distance = 20.0f,
        };
        const auto hit = physics->cast_ray(query);

        // 3. Assert: Ray hits top face at y = 2.0 with upward normal (0, 1, 0) and distance = 8.0
        EXPECT_TRUE(hit.has_hit);
        EXPECT_EQ(hit.hit_body_id, box_id);
        EXPECT_NEAR(hit.distance, 8.0f, 0.01f);
        EXPECT_NEAR(hit.position.x, 0.0f, 0.01f);
        EXPECT_NEAR(hit.position.y, 2.0f, 0.01f);
        EXPECT_NEAR(hit.position.z, 0.0f, 0.01f);
        EXPECT_NEAR(hit.normal.x, 0.0f, 0.01f);
        EXPECT_NEAR(hit.normal.y, 1.0f, 0.01f);
        EXPECT_NEAR(hit.normal.z, 0.0f, 0.01f);

        // Teardown
        physics->remove_body(box_id);
        physics->destroy_body(box_id);
        physics->destroy_shape(box_shape);
    }

    /// @brief Verifies that a heightfield shape is created successfully and can be
    ///        accurately intersected by a raycast.
    TEST_F(jolt_shim_fixture, heightfield_raycast)
    {
        // 1. Setup: Create 4x4 heightfield terrain with constant elevation = 3.0
        const auto description = make_description();
        auto physics = physics_system_ptr{jolt::shim::create_physics_system(description)};
        ASSERT_NE(physics, nullptr);

        constexpr auto sample_count = 4u;
        auto heights = tempest::array<float, sample_count * sample_count>{};
        for (auto sample_index = 0u; sample_index < sample_count * sample_count; ++sample_index)
        {
            heights[sample_index] = 3.0f;
        }

        auto* const heightfield_shape = physics->create_heightfield_shape(
            heights.data(),
            sample_count,
            jolt::shim::vec3{-1.5f, 0.0f, -1.5f},
            jolt::shim::vec3{1.0f, 1.0f, 1.0f});
        ASSERT_NE(heightfield_shape, nullptr);

        const auto heightfield_body_desc = jolt::shim::body_desc{
            .shape = heightfield_shape,
            .position = jolt::shim::vec3{0.0f, 0.0f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto heightfield_body_id = physics->create_body(heightfield_body_desc);
        ASSERT_NE(heightfield_body_id, jolt::shim::invalid_body_id);
        physics->add_body(heightfield_body_id, false);

        // 2. Act: Cast downward ray from (0, 10, 0)
        const auto query = jolt::shim::raycast_query{
            .origin = jolt::shim::vec3{0.0f, 10.0f, 0.0f},
            .direction = jolt::shim::vec3{0.0f, -1.0f, 0.0f},
            .max_distance = 20.0f,
        };
        const auto hit = physics->cast_ray(query);

        // 3. Assert: Ray hits heightfield at y = 3.0 with upward normal
        EXPECT_TRUE(hit.has_hit);
        EXPECT_EQ(hit.hit_body_id, heightfield_body_id);
        EXPECT_NEAR(hit.position.x, 0.0f, 0.01f);
        EXPECT_NEAR(hit.position.y, 3.0f, 0.01f);
        EXPECT_NEAR(hit.position.z, 0.0f, 0.01f);
        EXPECT_NEAR(hit.normal.y, 1.0f, 0.01f);

        // Teardown
        physics->remove_body(heightfield_body_id);
        physics->destroy_body(heightfield_body_id);
        physics->destroy_shape(heightfield_shape);
    }

    // =========================================================================
    // SECTION: Character Virtual Movement & Interaction
    // =========================================================================

    /// @brief Verifies creation, velocity setting, step/update, position retrieval,
    ///        and destruction of a character_virtual instance.
    TEST_F(jolt_shim_fixture, character_lifecycle)
    {
        // 1. Setup: Construct physics system, capsule shape, and character virtual
        const auto description = make_description();
        auto physics = physics_system_ptr{jolt::shim::create_physics_system(description)};
        ASSERT_NE(physics, nullptr);

        constexpr auto half_height = 0.5f;
        constexpr auto radius = 0.3f;
        auto* const capsule_shape = physics->create_capsule_shape(half_height, radius);
        ASSERT_NE(capsule_shape, nullptr);

        const auto char_desc = jolt::shim::character_virtual_desc{
            .shape = capsule_shape,
            .position = jolt::shim::vec3{0.0f, 1.0f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .up = jolt::shim::vec3{0.0f, 1.0f, 0.0f},
            .mass = 80.0f,
            .max_strength = 100.0f,
            .max_slope_angle = 0.872665f,
            .shape_offset = jolt::shim::vec3{0.0f, half_height + radius, 0.0f},
        };
        auto* const character = physics->create_character_virtual(char_desc);
        ASSERT_NE(character, nullptr);

        // 2. Act: Verify initial position, apply velocity, step the character
        const auto initial_position = character->get_position();
        EXPECT_NEAR(initial_position.x, 0.0f, 1.0e-4f);
        EXPECT_NEAR(initial_position.y, 1.0f, 1.0e-4f);
        EXPECT_NEAR(initial_position.z, 0.0f, 1.0e-4f);

        constexpr auto delta_time = 1.0f / 60.0f;
        character->set_linear_velocity(jolt::shim::vec3{0.0f, 2.0f, 0.0f});
        character->update(delta_time, jolt::shim::vec3{0.0f, 0.0f, 0.0f});

        // 3. Assert: Position moved upward, properties match, and destruction succeeds cleanly
        const auto updated_position = character->get_position();
        EXPECT_NEAR(updated_position.y, 1.0f + 2.0f * delta_time, 1.0e-3f);
        EXPECT_NEAR(character->get_mass(), 80.0f, 1.0e-4f);
        EXPECT_NEAR(character->get_max_slope_angle(), 0.872665f, 1.0e-4f);

        // Teardown
        physics->destroy_character_virtual(character);
        physics->destroy_shape(capsule_shape);
    }

    /// @brief Verifies that a character virtual successfully climbs up a 40-degree walkable slope.
    TEST_F(jolt_shim_fixture, slope_climb_test)
    {
        // 1. Setup: Construct physics system, 40-degree inclined ramp, and character
        const auto description = make_description();
        auto physics = physics_system_ptr{jolt::shim::create_physics_system(description)};
        ASSERT_NE(physics, nullptr);

        // 40 degree ramp (< 50 degree max slope angle)
        constexpr auto angle_deg = 40.0f;
        constexpr auto angle_rad = angle_deg * (tempest::math::constants::pi<float> / 180.0f);
        auto* const ramp_shape = physics->create_box_shape(jolt::shim::vec3{10.0f, 0.5f, 10.0f});
        ASSERT_NE(ramp_shape, nullptr);

        const auto ramp_rotation = quat_from_axis_angle(jolt::shim::vec3{0.0f, 0.0f, 1.0f}, angle_rad);
        const auto ramp_body_desc = jolt::shim::body_desc{
            .shape = ramp_shape,
            .position = jolt::shim::vec3{0.0f, 0.0f, 0.0f},
            .rotation = ramp_rotation,
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto ramp_id = physics->create_body(ramp_body_desc);
        ASSERT_NE(ramp_id, jolt::shim::invalid_body_id);
        physics->add_body(ramp_id, false);

        // Capsule character
        constexpr auto half_height = 0.5f;
        constexpr auto radius = 0.3f;
        auto* const capsule_shape = physics->create_capsule_shape(half_height, radius);
        ASSERT_NE(capsule_shape, nullptr);

        const auto char_desc = jolt::shim::character_virtual_desc{
            .shape = capsule_shape,
            .position = jolt::shim::vec3{0.0f, 5.0f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .up = jolt::shim::vec3{0.0f, 1.0f, 0.0f},
            .mass = 80.0f,
            .max_strength = 100.0f,
            .max_slope_angle = 50.0f * (tempest::math::constants::pi<float> / 180.0f),
            .shape_offset = jolt::shim::vec3{0.0f, half_height + radius, 0.0f},
        };
        auto* const character = physics->create_character_virtual(char_desc);
        ASSERT_NE(character, nullptr);

        // Find surface elevation at x = 0
        const auto ray_hit = physics->cast_ray(jolt::shim::raycast_query{
            .origin = jolt::shim::vec3{0.0f, 10.0f, 0.0f},
            .direction = jolt::shim::vec3{0.0f, -1.0f, 0.0f},
            .max_distance = 20.0f,
        });
        ASSERT_TRUE(ray_hit.has_hit);
        character->set_position(ray_hit.position);

        constexpr auto delta_time = 1.0f / 60.0f;
        constexpr auto gravity = jolt::shim::vec3{0.0f, -9.81f, 0.0f};

        // Settle character on the slope
        for (auto step = 0; step < 10; ++step)
        {
            character->set_linear_velocity(jolt::shim::vec3{0.0f, -1.0f, 0.0f});
            character->update(delta_time, gravity);
        }
        EXPECT_EQ(character->get_ground_state(), jolt::shim::ground_state::on_ground);
        EXPECT_TRUE(character->is_supported());

        const auto start_pos = character->get_position();

        // 2. Act: Move horizontally in +X into the 40-degree slope for 30 steps
        for (auto step = 0; step < 30; ++step)
        {
            character->set_linear_velocity(jolt::shim::vec3{2.0f, 0.0f, 0.0f});
            character->update(delta_time, gravity);
        }

        // 3. Assert: Character climbed forward and upward along the slope
        const auto end_pos = character->get_position();
        EXPECT_GT(end_pos.x, start_pos.x + 0.3f);
        EXPECT_GT(end_pos.y, start_pos.y + 0.2f);
        EXPECT_EQ(character->get_ground_state(), jolt::shim::ground_state::on_ground);

        // Teardown
        physics->destroy_character_virtual(character);
        physics->remove_body(ramp_id);
        physics->destroy_body(ramp_id);
        physics->destroy_shape(capsule_shape);
        physics->destroy_shape(ramp_shape);
    }

    /// @brief Verifies that a character on a 65-degree slope is detected as on_steep_ground
    ///        and cancel_velocity_towards_steep_slopes prevents ascending.
    TEST_F(jolt_shim_fixture, slope_slide_test)
    {
        // 1. Setup: Construct physics system, 65-degree steep ramp, and character
        const auto description = make_description();
        auto physics = physics_system_ptr{jolt::shim::create_physics_system(description)};
        ASSERT_NE(physics, nullptr);

        // 65 degree ramp (> 50 degree max slope angle)
        constexpr auto angle_deg = 65.0f;
        constexpr auto angle_rad = angle_deg * (tempest::math::constants::pi<float> / 180.0f);
        auto* const ramp_shape = physics->create_box_shape(jolt::shim::vec3{10.0f, 0.5f, 10.0f});
        ASSERT_NE(ramp_shape, nullptr);

        const auto ramp_rotation = quat_from_axis_angle(jolt::shim::vec3{0.0f, 0.0f, 1.0f}, angle_rad);
        const auto ramp_body_desc = jolt::shim::body_desc{
            .shape = ramp_shape,
            .position = jolt::shim::vec3{0.0f, 0.0f, 0.0f},
            .rotation = ramp_rotation,
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto ramp_id = physics->create_body(ramp_body_desc);
        ASSERT_NE(ramp_id, jolt::shim::invalid_body_id);
        physics->add_body(ramp_id, false);

        // Capsule character
        constexpr auto half_height = 0.5f;
        constexpr auto radius = 0.3f;
        auto* const capsule_shape = physics->create_capsule_shape(half_height, radius);
        ASSERT_NE(capsule_shape, nullptr);

        const auto char_desc = jolt::shim::character_virtual_desc{
            .shape = capsule_shape,
            .position = jolt::shim::vec3{0.0f, 5.0f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .up = jolt::shim::vec3{0.0f, 1.0f, 0.0f},
            .mass = 80.0f,
            .max_strength = 100.0f,
            .max_slope_angle = 50.0f * (tempest::math::constants::pi<float> / 180.0f),
            .shape_offset = jolt::shim::vec3{0.0f, half_height + radius, 0.0f},
        };
        auto* const character = physics->create_character_virtual(char_desc);
        ASSERT_NE(character, nullptr);

        // Find surface elevation at x = 0
        const auto ray_hit = physics->cast_ray(jolt::shim::raycast_query{
            .origin = jolt::shim::vec3{0.0f, 10.0f, 0.0f},
            .direction = jolt::shim::vec3{0.0f, -1.0f, 0.0f},
            .max_distance = 20.0f,
        });
        ASSERT_TRUE(ray_hit.has_hit);
        character->set_position(ray_hit.position);

        constexpr auto delta_time = 1.0f / 60.0f;
        constexpr auto gravity = jolt::shim::vec3{0.0f, -9.81f, 0.0f};

        // Settle character on the slope
        for (auto step = 0; step < 10; ++step)
        {
            character->set_linear_velocity(jolt::shim::vec3{0.0f, -1.0f, 0.0f});
            character->update(delta_time, gravity);
        }

        // 2. Act: Assert steep slope detected, cancel desired velocity towards slope, simulate
        EXPECT_EQ(character->get_ground_state(), jolt::shim::ground_state::on_steep_ground);

        const auto desired_velocity = jolt::shim::vec3{2.0f, 0.0f, 0.0f};
        const auto clamped_velocity = character->cancel_velocity_towards_steep_slopes(desired_velocity);
        EXPECT_LE(clamped_velocity.x, 0.01f);

        const auto pos_before = character->get_position();
        for (auto step = 0; step < 30; ++step)
        {
            auto vel = character->cancel_velocity_towards_steep_slopes(jolt::shim::vec3{2.0f, 0.0f, 0.0f});
            vel.y -= 9.81f * delta_time;
            character->set_linear_velocity(vel);
            character->update(delta_time, gravity);
        }
        const auto pos_after = character->get_position();

        // 3. Assert: Character did not advance or climb up the steep slope
        EXPECT_LE(pos_after.x, pos_before.x + 0.01f);
        EXPECT_LE(pos_after.y, pos_before.y + 0.01f);

        // Teardown
        physics->destroy_character_virtual(character);
        physics->remove_body(ramp_id);
        physics->destroy_body(ramp_id);
        physics->destroy_shape(capsule_shape);
        physics->destroy_shape(ramp_shape);
    }

    /// @brief Verifies that a character virtual steps over a 0.25m stair box using extended_update.
    TEST_F(jolt_shim_fixture, step_climb_test)
    {
        // 1. Setup: Construct physics system, floor, 0.25m stair step, and character
        const auto description = make_description();
        auto physics = physics_system_ptr{jolt::shim::create_physics_system(description)};
        ASSERT_NE(physics, nullptr);

        // Floor: box surface at y = 0.0
        auto* const floor_shape = physics->create_box_shape(jolt::shim::vec3{20.0f, 0.5f, 20.0f});
        ASSERT_NE(floor_shape, nullptr);
        const auto floor_desc = jolt::shim::body_desc{
            .shape = floor_shape,
            .position = jolt::shim::vec3{0.0f, -0.5f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto floor_id = physics->create_body(floor_desc);
        ASSERT_NE(floor_id, jolt::shim::invalid_body_id);
        physics->add_body(floor_id, false);

        // Stair step: height 0.25m (half extents 2.0 x 0.125 x 2.0, positioned at x = 3.0, y = 0.125)
        // Leading edge at x = 3.0 - 2.0 = 1.0m, top surface at y = 0.25m
        auto* const stair_shape = physics->create_box_shape(jolt::shim::vec3{2.0f, 0.125f, 2.0f});
        ASSERT_NE(stair_shape, nullptr);
        const auto stair_desc = jolt::shim::body_desc{
            .shape = stair_shape,
            .position = jolt::shim::vec3{3.0f, 0.125f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto stair_id = physics->create_body(stair_desc);
        ASSERT_NE(stair_id, jolt::shim::invalid_body_id);
        physics->add_body(stair_id, false);

        // Capsule character at x = 0.0, y = 0.0 (1.0m in front of the stair)
        constexpr auto half_height = 0.5f;
        constexpr auto radius = 0.3f;
        auto* const capsule_shape = physics->create_capsule_shape(half_height, radius);
        ASSERT_NE(capsule_shape, nullptr);

        const auto char_desc = jolt::shim::character_virtual_desc{
            .shape = capsule_shape,
            .position = jolt::shim::vec3{0.0f, 0.0f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .up = jolt::shim::vec3{0.0f, 1.0f, 0.0f},
            .mass = 80.0f,
            .max_strength = 100.0f,
            .max_slope_angle = 50.0f * (tempest::math::constants::pi<float> / 180.0f),
            .shape_offset = jolt::shim::vec3{0.0f, half_height + radius, 0.0f},
        };
        auto* const character = physics->create_character_virtual(char_desc);
        ASSERT_NE(character, nullptr);

        constexpr auto delta_time = 1.0f / 60.0f;
        constexpr auto gravity = jolt::shim::vec3{0.0f, -9.81f, 0.0f};

        // Settle character on the floor
        for (auto step = 0; step < 5; ++step)
        {
            character->update(delta_time, gravity);
        }
        EXPECT_EQ(character->get_ground_state(), jolt::shim::ground_state::on_ground);

        // 2. Act: Advance character forward with extended_update settings allowing 0.4m stair step
        const auto settings = jolt::shim::extended_update_settings{
            .stick_to_floor_step_down = jolt::shim::vec3{0.0f, -0.5f, 0.0f},
            .walk_stairs_step_up = jolt::shim::vec3{0.0f, 0.4f, 0.0f},
            .walk_stairs_min_step_forward = 0.02f,
            .walk_stairs_step_forward_test = 0.15f,
            .walk_stairs_cos_angle_forward_contact = 0.258819f,
            .walk_stairs_step_down_extra = jolt::shim::vec3{0.0f, 0.0f, 0.0f},
        };

        for (auto step = 0; step < 60; ++step)
        {
            character->set_linear_velocity(jolt::shim::vec3{2.0f, 0.0f, 0.0f});
            character->extended_update(delta_time, gravity, settings);
        }

        // 3. Assert: Character climbed onto the 0.25m step surface and continued forward
        const auto pos = character->get_position();
        EXPECT_GT(pos.x, 1.2f);
        EXPECT_NEAR(pos.y, 0.25f, 0.05f);
        EXPECT_EQ(character->get_ground_state(), jolt::shim::ground_state::on_ground);

        // Teardown
        physics->destroy_character_virtual(character);
        physics->remove_body(stair_id);
        physics->destroy_body(stair_id);
        physics->remove_body(floor_id);
        physics->destroy_body(floor_id);
        physics->destroy_shape(capsule_shape);
        physics->destroy_shape(stair_shape);
        physics->destroy_shape(floor_shape);
    }

    /// @brief Verifies that a character jumping upward into an overhead static box stops upward motion.
    TEST_F(jolt_shim_fixture, ceiling_hit_test)
    {
        // 1. Setup: Construct physics system, floor at y = 0, ceiling at y = 2.5, and character (height 1.6)
        const auto description = make_description();
        auto physics = physics_system_ptr{jolt::shim::create_physics_system(description)};
        ASSERT_NE(physics, nullptr);

        // Floor: box surface at y = 0.0
        auto* const floor_shape = physics->create_box_shape(jolt::shim::vec3{10.0f, 0.5f, 10.0f});
        ASSERT_NE(floor_shape, nullptr);
        const auto floor_desc = jolt::shim::body_desc{
            .shape = floor_shape,
            .position = jolt::shim::vec3{0.0f, -0.5f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto floor_id = physics->create_body(floor_desc);
        ASSERT_NE(floor_id, jolt::shim::invalid_body_id);
        physics->add_body(floor_id, false);

        // Ceiling: bottom surface at y = 3.0 - 0.5 = 2.5m
        auto* const ceiling_shape = physics->create_box_shape(jolt::shim::vec3{5.0f, 0.5f, 5.0f});
        ASSERT_NE(ceiling_shape, nullptr);
        const auto ceiling_desc = jolt::shim::body_desc{
            .shape = ceiling_shape,
            .position = jolt::shim::vec3{0.0f, 3.0f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        const auto ceiling_id = physics->create_body(ceiling_desc);
        ASSERT_NE(ceiling_id, jolt::shim::invalid_body_id);
        physics->add_body(ceiling_id, false);

        // Capsule character of height 1.6m (top at y = 1.6m initially, 0.9m below ceiling)
        constexpr auto half_height = 0.5f;
        constexpr auto radius = 0.3f;
        auto* const capsule_shape = physics->create_capsule_shape(half_height, radius);
        ASSERT_NE(capsule_shape, nullptr);

        bool ceiling_contact_added = false;
        const auto callbacks = jolt::shim::character_contact_callbacks{
            .on_contact_added = [](jolt::shim::character_virtual*, jolt::shim::body_id, jolt::shim::vec3, jolt::shim::vec3 normal, void* user_data) {
                if (normal.y < -0.5f)
                {
                    *static_cast<bool*>(user_data) = true;
                }
            },
            .user_data = &ceiling_contact_added,
        };

        const auto char_desc = jolt::shim::character_virtual_desc{
            .shape = capsule_shape,
            .position = jolt::shim::vec3{0.0f, 0.0f, 0.0f},
            .rotation = jolt::shim::quat{0.0f, 0.0f, 0.0f, 1.0f},
            .up = jolt::shim::vec3{0.0f, 1.0f, 0.0f},
            .mass = 80.0f,
            .max_strength = 100.0f,
            .max_slope_angle = 50.0f * (tempest::math::constants::pi<float> / 180.0f),
            .shape_offset = jolt::shim::vec3{0.0f, half_height + radius, 0.0f},
            .callbacks = callbacks,
        };
        auto* const character = physics->create_character_virtual(char_desc);
        ASSERT_NE(character, nullptr);

        constexpr auto delta_time = 1.0f / 60.0f;
        constexpr auto gravity = jolt::shim::vec3{0.0f, -9.81f, 0.0f};

        // 2. Act: Apply a massive vertical jump velocity (10.0 m/s)
        character->set_linear_velocity(jolt::shim::vec3{0.0f, 10.0f, 0.0f});
        auto max_y = 0.0f;
        for (auto step = 0; step < 30; ++step)
        {
            auto vel = character->get_linear_velocity();
            vel.y -= 9.81f * delta_time;
            character->set_linear_velocity(vel);
            character->update(delta_time, gravity);
            if (character->get_position().y > max_y)
            {
                max_y = character->get_position().y;
            }
        }

        // 3. Assert: Character base y never exceeded ceiling limit (2.5m ceiling - 1.6m height = 0.9m)
        EXPECT_LE(max_y, 0.95f);
        EXPECT_TRUE(ceiling_contact_added);

        // Teardown
        physics->destroy_character_virtual(character);
        physics->remove_body(ceiling_id);
        physics->destroy_body(ceiling_id);
        physics->remove_body(floor_id);
        physics->destroy_body(floor_id);
        physics->destroy_shape(capsule_shape);
        physics->destroy_shape(ceiling_shape);
        physics->destroy_shape(floor_shape);
    }
} // namespace tempest::physics::shim::tests

auto main(int argc, char** argv) -> int
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
