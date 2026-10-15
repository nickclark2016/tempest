#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/int.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/logger.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/physics/box_collider_component.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/character_controller_system.hpp>
#include <tempest/physics/character_movement_intent.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/physics/rigid_body_component.hpp>
#include <tempest/physics/velocity_component.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>

namespace tempest::physics::tests
{
    namespace
    {
        class physics_fixture : public ::testing::Test
        {
        public:
            physics_fixture() = default;
            physics_fixture(const physics_fixture&) = delete;
            physics_fixture(physics_fixture&&) noexcept = delete;
            ~physics_fixture() override = default;

            auto operator=(const physics_fixture&) -> physics_fixture& = delete;
            auto operator=(physics_fixture&&) noexcept -> physics_fixture& = delete;

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
    // SECTION: Character Lifecycle
    // =========================================================================

    /// @brief Verifies character spawning into physics_world, retrieving its virtual pointer,
    /// and ensures slot map generation invalidation upon despawning.
    TEST_F(physics_fixture, character_spawn_and_despawn)
    {
        // Setup: Instantiate physics_world with job system integration
        auto world = physics_world{make_description()};
        const auto config = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
        };

        // Act: Spawn a virtual character at initial position
        const auto character_id = world.spawn_character(config, math::vec3<float>{0.0F, 1.0F, 0.0F});

        // Assert: Character id is valid and pointer lookup succeeds
        ASSERT_NE(character_id, invalid_character_id);
        auto* character = world.get_character(character_id);
        ASSERT_NE(character, nullptr);
        EXPECT_FLOAT_EQ(character->get_position().y, 1.0F);

        // Act: Despawn character
        world.despawn_character(character_id);

        // Assert: Subsequent lookup returns nullptr due to slot invalidation
        EXPECT_EQ(world.get_character(character_id), nullptr);
    }

    // =========================================================================
    // SECTION: ECS Movement & Transform Synchronization
    // =========================================================================

    /// @brief Verifies that character_controller_system processes intent, advances horizontal
    /// position across simulation ticks, and propagates the new pose to transform_component.
    TEST_F(physics_fixture, character_movement_and_transform_sync)
    {
        // Setup: Physics world with static floor and ECS entity
        auto world = physics_world{make_description()};
        auto* const system = world.physics_system();
        ASSERT_NE(system, nullptr);

        const auto floor_body = create_static_box(system, jolt::shim::vec3{50.0F, 1.0F, 50.0F}, jolt::shim::vec3{0.0F, -1.0F, 0.0F});

        const auto entity = registry.create<character_controller_component, velocity_component, character_movement_intent, ecs::transform_component, ecs::transform_history_component>();
        auto controller = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
            .walk_speed = 5.0F,
            .acceleration_rate = 30.0F,
        };
        const auto initial_position = math::vec3<float>{0.0F, 1.0F, 0.0F};
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

        // Act: Advance simulation for 30 steps at 60 Hz (0.5 seconds)
        constexpr auto delta_time = 1.0F / 60.0F;
        constexpr auto gravity = math::vec3<float>{0.0F, -9.81F, 0.0F};
        for (auto step = 0; step < 30; ++step)
        {
            update_character_controllers(world, registry, delta_time, gravity);
            world.step(delta_time);
        }

        // Assert: Entity position moved forward along +Z axis and synced to transform & history
        const auto& updated_transform = registry.get<ecs::transform_component>(entity);
        const auto& updated_history = registry.get<ecs::transform_history_component>(entity);
        const auto& updated_velocity = registry.get<velocity_component>(entity);

        EXPECT_GT(updated_transform.position().z, 0.5F);
        EXPECT_FLOAT_EQ(updated_transform.position().z, updated_history.current_position.z);
        EXPECT_GT(updated_velocity.linear.z, 0.0F);

        // Teardown
        system->remove_body(floor_body);
        system->destroy_body(floor_body);
    }

    // =========================================================================
    // SECTION: Jumping & Ground Detection
    // =========================================================================

    /// @brief Verifies that a character resting on the floor transitions to grounded,
    /// and executing a jump intent applies upward velocity and leaves ground contact.
    TEST_F(physics_fixture, character_jumping_and_grounding)
    {
        // Setup: Physics world with floor and character spawned slightly above floor
        auto world = physics_world{make_description()};
        auto* const system = world.physics_system();
        ASSERT_NE(system, nullptr);

        const auto floor_body = create_static_box(system, jolt::shim::vec3{20.0F, 1.0F, 20.0F}, jolt::shim::vec3{0.0F, -1.0F, 0.0F});

        const auto entity = registry.create<character_controller_component, velocity_component, character_movement_intent, ecs::transform_component>();
        auto controller = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
            .jump_height = 1.5F,
        };
        const auto initial_position = math::vec3<float>{0.0F, 1.0F, 0.0F};
        controller.id = world.spawn_character(controller, initial_position);

        auto initial_transform = ecs::transform_component{};
        initial_transform.position(initial_position);

        registry.replace(entity, controller);
        registry.replace(entity, velocity_component{});
        registry.replace(entity, character_movement_intent{
            .wish_direction = math::vec3<float>{0.0F, 0.0F, 0.0F},
            .jump_requested = false,
            .sprint_requested = false,
        });
        registry.replace(entity, initial_transform);

        constexpr auto delta_time = 1.0F / 60.0F;
        constexpr auto gravity = math::vec3<float>{0.0F, -9.81F, 0.0F};

        // Settle character on the ground
        for (auto step = 0; step < 20; ++step)
        {
            update_character_controllers(world, registry, delta_time, gravity);
            world.step(delta_time);
        }

        const auto& grounded_controller = registry.get<character_controller_component>(entity);
        ASSERT_TRUE(grounded_controller.is_grounded);

        // Act: Trigger jump intent
        registry.replace(entity, character_movement_intent{
            .wish_direction = math::vec3<float>{0.0F, 0.0F, 0.0F},
            .jump_requested = true,
            .sprint_requested = false,
        });

        update_character_controllers(world, registry, delta_time, gravity);
        world.step(delta_time);

        // Assert: Upward velocity applied matching sqrt(2 * g * jump_height)
        const auto& jumping_velocity = registry.get<velocity_component>(entity);
        EXPECT_GT(jumping_velocity.linear.y, 4.0F);

        // Advance a few ticks into air
        registry.replace(entity, character_movement_intent{
            .wish_direction = math::vec3<float>{0.0F, 0.0F, 0.0F},
            .jump_requested = false,
            .sprint_requested = false,
        });

        for (auto step = 0; step < 10; ++step)
        {
            update_character_controllers(world, registry, delta_time, gravity);
            world.step(delta_time);
        }

        const auto& airborne_controller = registry.get<character_controller_component>(entity);
        const auto& airborne_transform = registry.get<ecs::transform_component>(entity);
        EXPECT_FALSE(airborne_controller.is_grounded);
        EXPECT_GT(airborne_transform.position().y, 1.0F);

        // Teardown
        system->remove_body(floor_body);
        system->destroy_body(floor_body);
    }

    // =========================================================================
    // SECTION: Stair Stepping Navigation
    // =========================================================================

    /// @brief Verifies that character_controller_system automatically climbs over a 0.25m stair
    /// step obstacle using walk_stairs settings during horizontal motion.
    TEST_F(physics_fixture, character_stair_stepping_ecs)
    {
        // Setup: Floor and a 0.25m obstacle step in front of the character
        auto world = physics_world{make_description()};
        auto* const system = world.physics_system();
        ASSERT_NE(system, nullptr);

        const auto floor_body = create_static_box(system, jolt::shim::vec3{20.0F, 1.0F, 20.0F}, jolt::shim::vec3{0.0F, -1.0F, 0.0F});

        // Step: width 4m (half 2m), height 0.25m (half 0.125m, center y = 0.125m), depth 2m (half 1m, center z = 1.0m)
        const auto step_body = create_static_box(system, jolt::shim::vec3{2.0F, 0.125F, 1.0F}, jolt::shim::vec3{0.0F, 0.125F, 1.0F});

        const auto entity = registry.create<character_controller_component, velocity_component, character_movement_intent, ecs::transform_component>();
        auto controller = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
            .walk_speed = 3.0F,
            .acceleration_rate = 25.0F,
            .stick_to_floor_step_down = 0.5F,
            .walk_stairs_step_up = 0.4F,
        };
        const auto initial_position = math::vec3<float>{0.0F, 0.9F, -1.0F};
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

        // Act: Advance simulation for 60 ticks (1 second) toward the stair step
        constexpr auto delta_time = 1.0F / 60.0F;
        constexpr auto gravity = math::vec3<float>{0.0F, -9.81F, 0.0F};
        for (auto step = 0; step < 60; ++step)
        {
            update_character_controllers(world, registry, delta_time, gravity);
            world.step(delta_time);
        }

        // Assert: Character successfully climbed up onto the step (y raised by >= 0.25m, z navigated past obstacle front)
        const auto& final_transform = registry.get<ecs::transform_component>(entity);
        const auto& final_controller = registry.get<character_controller_component>(entity);

        EXPECT_GT(final_transform.position().z, 0.2F);
        EXPECT_GE(final_transform.position().y, 1.1F);
        EXPECT_TRUE(final_controller.is_grounded);

        // Teardown
        system->remove_body(step_body);
        system->destroy_body(step_body);
        system->remove_body(floor_body);
        system->destroy_body(floor_body);
    }

    // =========================================================================
    // SECTION: Physics Component Reflection & Defaults
    // =========================================================================

    /// @brief Verifies rigid_body_component default field values match expected
    /// engine and physics engine invariants.
    TEST(physics_component_tests, rigid_body_component_defaults)
    {
        // Setup: instantiate default rigid body component.
        const auto body = rigid_body_component{};

        // Assert: default field values.
        EXPECT_EQ(body.motion, jolt::shim::motion_type::static_motion);
        EXPECT_EQ(body.layer, jolt::shim::object_layer::non_moving);
        EXPECT_EQ(body.mass, 0.0F);
        EXPECT_EQ(body.id, jolt::shim::invalid_body_id);
    }

    /// @brief Verifies rigid_body_component reflection metadata including version,
    /// field names, types, flags, and enum entries.
    TEST(physics_component_tests, rigid_body_component_reflection)
    {
        // Setup: construct type descriptor via reflection describe.
        auto builder = tempest::ecs::type_builder<rigid_body_component>{"rigid_body_component"};
        tempest::ecs::reflect<rigid_body_component>::describe(builder);

        // Act: build the type descriptor.
        const auto descriptor = builder.build();

        // Assert: type descriptor properties.
        EXPECT_EQ(descriptor.version, 1U);
        EXPECT_EQ(descriptor.fields.size(), 3U);

        const auto* motion_field = descriptor.find_field("motion");
        ASSERT_NE(motion_field, nullptr);
        EXPECT_EQ(motion_field->kind, tempest::ecs::field_kind::enumeration);
        EXPECT_EQ(motion_field->flags, tempest::ecs::field_flags::none);
        EXPECT_EQ(motion_field->size, sizeof(jolt::shim::motion_type));
        EXPECT_EQ(motion_field->enum_entries.size(), 3U);

        const auto* layer_field = descriptor.find_field("layer");
        ASSERT_NE(layer_field, nullptr);
        EXPECT_EQ(layer_field->kind, tempest::ecs::field_kind::enumeration);
        EXPECT_EQ(layer_field->flags, tempest::ecs::field_flags::none);
        EXPECT_EQ(layer_field->size, sizeof(jolt::shim::object_layer));
        EXPECT_EQ(layer_field->enum_entries.size(), 2U);

        const auto* mass_field = descriptor.find_field("mass");
        ASSERT_NE(mass_field, nullptr);
        EXPECT_EQ(mass_field->kind, tempest::ecs::field_kind::float32);
        EXPECT_EQ(mass_field->flags, tempest::ecs::field_flags::none);
        EXPECT_EQ(mass_field->size, sizeof(float));

        // Assert: unreflected runtime ID field is not exposed.
        EXPECT_EQ(descriptor.find_field("id"), nullptr);
    }

    /// @brief Verifies box_collider_component default field values match expected
    /// unit cube dimensions and zero center offset.
    TEST(physics_component_tests, box_collider_component_defaults)
    {
        // Setup: instantiate default box collider component.
        const auto box = box_collider_component{};

        // Assert: default half extents (0.5m on all axes) and offset (0.0m on all axes).
        EXPECT_FLOAT_EQ(box.half_extents.x, 0.5F);
        EXPECT_FLOAT_EQ(box.half_extents.y, 0.5F);
        EXPECT_FLOAT_EQ(box.half_extents.z, 0.5F);
        EXPECT_FLOAT_EQ(box.offset.x, 0.0F);
        EXPECT_FLOAT_EQ(box.offset.y, 0.0F);
        EXPECT_FLOAT_EQ(box.offset.z, 0.0F);
    }

    /// @brief Verifies box_collider_component reflection metadata including version,
    /// field names, types, and flags.
    TEST(physics_component_tests, box_collider_component_reflection)
    {
        // Setup: construct type descriptor via reflection describe.
        auto builder = tempest::ecs::type_builder<box_collider_component>{"box_collider_component"};
        tempest::ecs::reflect<box_collider_component>::describe(builder);

        // Act: build the type descriptor.
        const auto descriptor = builder.build();

        // Assert: type descriptor properties.
        EXPECT_EQ(descriptor.version, 1U);
        EXPECT_EQ(descriptor.fields.size(), 2U);

        const auto* half_extents_field = descriptor.find_field("half_extents");
        ASSERT_NE(half_extents_field, nullptr);
        EXPECT_EQ(half_extents_field->kind, tempest::ecs::field_kind::vec3);
        EXPECT_EQ(half_extents_field->flags, tempest::ecs::field_flags::none);
        EXPECT_EQ(half_extents_field->size, sizeof(math::vec3<float>));

        const auto* offset_field = descriptor.find_field("offset");
        ASSERT_NE(offset_field, nullptr);
        EXPECT_EQ(offset_field->kind, tempest::ecs::field_kind::vec3);
        EXPECT_EQ(offset_field->flags, tempest::ecs::field_flags::none);
        EXPECT_EQ(offset_field->size, sizeof(math::vec3<float>));
    }
} // namespace tempest::physics::tests

auto main(int argc, char** argv) -> int
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
