#include <gtest/gtest.h>

#include <tempest/array.hpp>
#include <tempest/int.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/logger.hpp>
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
} // namespace tempest::physics::shim::tests

auto main(int argc, char** argv) -> int
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
