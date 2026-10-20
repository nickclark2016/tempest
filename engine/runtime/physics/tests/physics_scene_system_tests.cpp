#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/int.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/logger.hpp>
#include <tempest/physics/box_collider_component.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/character_controller_system.hpp>
#include <tempest/physics/character_movement_intent.hpp>
#include <tempest/physics/character_prediction.hpp>
#include <tempest/physics/physics_scene_system.hpp>
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
        class physics_scene_fixture : public ::testing::Test
        {
        public:
            physics_scene_fixture() = default;
            physics_scene_fixture(const physics_scene_fixture&) = delete;
            physics_scene_fixture(physics_scene_fixture&&) noexcept = delete;
            ~physics_scene_fixture() override = default;

            auto operator=(const physics_scene_fixture&) -> physics_scene_fixture& = delete;
            auto operator=(physics_scene_fixture&&) noexcept -> physics_scene_fixture& = delete;

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
        };
    } // namespace

    // =========================================================================
    // SECTION: Body Lifecycle & Event Reactions
    // =========================================================================

    /// @brief Verifies that adding box_collider_component and rigid_body_component
    /// automatically spawns a Jolt body, associates its identifier, and tracks the entity.
    TEST_F(physics_scene_fixture, physics_scene_system_spawns_body_on_component_added)
    {
        // Setup: Instantiate physics world, scene system, and an entity with transform
        auto world = physics_world{make_description()};
        auto scene_system = physics_scene_system{world, registry, event_reg};

        auto initial_transform = ecs::transform_component{};
        initial_transform.position(math::vec3<float>{0.0F, 5.0F, 0.0F});

        const auto target_entity = registry.create<ecs::transform_component>();
        registry.assign(target_entity, initial_transform);

        // Act: Assign box collider and dynamic rigid body components
        const auto collider = box_collider_component{
            .half_extents = math::vec3<float>{0.5F, 0.5F, 0.5F},
            .offset = math::vec3<float>{0.0F, 0.0F, 0.0F},
        };
        const auto body_comp = rigid_body_component{
            .motion = jolt::shim::motion_type::dynamic,
            .layer = jolt::shim::object_layer::moving,
            .mass = 10.0F,
        };

        registry.assign(target_entity, collider);
        registry.assign(target_entity, body_comp);

        // Assert: Scene system tracked body and assigned identifier to component
        EXPECT_TRUE(scene_system.has_body(target_entity));
        EXPECT_EQ(scene_system.body_count(), 1U);

        const auto body_id_opt = scene_system.get_body_id(target_entity);
        ASSERT_TRUE(body_id_opt.has_value());
        EXPECT_NE(*body_id_opt, jolt::shim::invalid_body_id);

        const auto* assigned_rb = registry.try_get<rigid_body_component>(target_entity);
        ASSERT_NE(assigned_rb, nullptr);
        EXPECT_EQ(assigned_rb->id, *body_id_opt);

        auto* const shim_system = world.physics_system();
        ASSERT_NE(shim_system, nullptr);
        const auto jolt_pos = shim_system->get_body_position(*body_id_opt);
        EXPECT_FLOAT_EQ(jolt_pos.x, 0.0F);
        EXPECT_FLOAT_EQ(jolt_pos.y, 5.0F);
        EXPECT_FLOAT_EQ(jolt_pos.z, 0.0F);
    }

    /// @brief Verifies that removing box_collider_component despawns the corresponding
    /// Jolt body and removes tracking from the scene system.
    TEST_F(physics_scene_fixture, physics_scene_system_despawns_body_on_component_removed)
    {
        // Setup: Create entity with transform, box collider, and rigid body
        auto world = physics_world{make_description()};
        auto scene_system = physics_scene_system{world, registry, event_reg};

        auto initial_transform = ecs::transform_component{};
        initial_transform.position(math::vec3<float>{0.0F, 2.0F, 0.0F});

        const auto target_entity = registry.create<ecs::transform_component>();
        registry.assign(target_entity, initial_transform);
        registry.assign(target_entity, box_collider_component{});
        registry.assign(target_entity, rigid_body_component{});

        ASSERT_TRUE(scene_system.has_body(target_entity));
        EXPECT_EQ(scene_system.body_count(), 1U);

        // Act: Remove collider component from entity
        registry.remove<box_collider_component>(target_entity);

        // Assert: Body is despawned and tracking is cleared
        EXPECT_FALSE(scene_system.has_body(target_entity));
        EXPECT_EQ(scene_system.body_count(), 0U);
        EXPECT_FALSE(scene_system.get_body_id(target_entity).has_value());
    }

    /// @brief Verifies that destroying an entity despawns the corresponding Jolt body
    /// and unregisters tracking from both forward and reverse lookups.
    TEST_F(physics_scene_fixture, physics_scene_system_despawns_body_on_entity_destroyed)
    {
        // Setup: Create entity with active collider and dynamic rigid body
        auto world = physics_world{make_description()};
        auto scene_system = physics_scene_system{world, registry, event_reg};

        auto initial_transform = ecs::transform_component{};
        initial_transform.position(math::vec3<float>{1.0F, 2.0F, 3.0F});

        const auto target_entity = registry.create<ecs::transform_component>();
        registry.assign(target_entity, initial_transform);
        registry.assign(target_entity, box_collider_component{});
        registry.assign(target_entity, rigid_body_component{});

        const auto body_id_opt = scene_system.get_body_id(target_entity);
        ASSERT_TRUE(body_id_opt.has_value());
        const auto tracked_body_id = *body_id_opt;

        // Act: Destroy the entity through the ECS registry
        registry.destroy(target_entity);

        // Assert: Tracking is removed and reverse lookup fails
        EXPECT_FALSE(scene_system.has_body(target_entity));
        EXPECT_EQ(scene_system.body_count(), 0U);
        EXPECT_FALSE(scene_system.get_entity(tracked_body_id).has_value());
    }

    // =========================================================================
    // SECTION: Simulation Advance & Pose Synchronization Loop
    // =========================================================================

    /// @brief Verifies that stepping simulation advances a dynamic rigid body under gravity
    /// and synchronizes the translated world pose to transform_component and history.
    TEST_F(physics_scene_fixture, physics_scene_system_dynamic_body_moves_transform)
    {
        // Setup: Create dynamic body elevated in free fall with transform history component
        auto world = physics_world{make_description()};
        auto scene_system = physics_scene_system{world, registry, event_reg};

        auto initial_transform = ecs::transform_component{};
        initial_transform.position(math::vec3<float>{0.0F, 10.0F, 0.0F});

        const auto target_entity = registry.create<ecs::transform_component>();
        registry.assign(target_entity, initial_transform);
        registry.assign(target_entity, box_collider_component{});
        registry.assign(target_entity, rigid_body_component{
            .motion = jolt::shim::motion_type::dynamic,
            .layer = jolt::shim::object_layer::moving,
            .mass = 5.0F,
        });
        registry.assign(target_entity, ecs::transform_history_component::create(
            math::vec3<float>{0.0F, 10.0F, 0.0F}, math::quat<float>{0.0F, 0.0F, 0.0F, 1.0F}));

        // Act: Advance simulation across multiple fixed delta time ticks
        constexpr auto fixed_dt = 1.0F / 60.0F;
        for (auto tick_index = 0; tick_index < 10; ++tick_index)
        {
            scene_system.step(fixed_dt);
        }

        // Assert: Vertical position fell downward under gravity and synchronized to ECS components
        const auto* updated_transform = registry.try_get<ecs::transform_component>(target_entity);
        ASSERT_NE(updated_transform, nullptr);
        EXPECT_LT(updated_transform->position().y, 10.0F);

        const auto* updated_history = registry.try_get<ecs::transform_history_component>(target_entity);
        ASSERT_NE(updated_history, nullptr);
        EXPECT_FLOAT_EQ(updated_history->current_position.y, updated_transform->position().y);
    }

    /// @brief Verifies that external transform modification triggers Jolt body pose update
    /// without entering an infinite recursion loop or stack overflow.
    TEST_F(physics_scene_fixture, physics_scene_system_transform_event_no_infinite_loop)
    {
        // Setup: Entity with dynamic body at initial origin position
        auto world = physics_world{make_description()};
        auto scene_system = physics_scene_system{world, registry, event_reg};

        auto initial_transform = ecs::transform_component{};
        initial_transform.position(math::vec3<float>{0.0F, 0.0F, 0.0F});

        const auto target_entity = registry.create<ecs::transform_component>();
        registry.assign(target_entity, initial_transform);
        registry.assign(target_entity, box_collider_component{});
        registry.assign(target_entity, rigid_body_component{
            .motion = jolt::shim::motion_type::dynamic,
            .layer = jolt::shim::object_layer::moving,
            .mass = 1.0F,
        });

        const auto body_id_opt = scene_system.get_body_id(target_entity);
        ASSERT_TRUE(body_id_opt.has_value());

        // Act: Explicitly replace transform component with new target pose
        auto new_transform = ecs::transform_component{};
        new_transform.position(math::vec3<float>{5.0F, 10.0F, 15.0F});
        registry.replace(target_entity, new_transform);

        // Assert: Jolt body position immediately updated to matching coordinates without recursion
        auto* const shim_system = world.physics_system();
        ASSERT_NE(shim_system, nullptr);
        const auto jolt_pos = shim_system->get_body_position(*body_id_opt);
        EXPECT_FLOAT_EQ(jolt_pos.x, 5.0F);
        EXPECT_FLOAT_EQ(jolt_pos.y, 10.0F);
        EXPECT_FLOAT_EQ(jolt_pos.z, 15.0F);
    }

    /// @brief Verifies that kinematic bodies follow explicit transform changes
    /// flushed during pre-step prior to advancing the physics world.
    TEST_F(physics_scene_fixture, physics_scene_system_kinematic_body_follows_transform)
    {
        // Setup: Entity with kinematic body
        auto world = physics_world{make_description()};
        auto scene_system = physics_scene_system{world, registry, event_reg};

        auto initial_transform = ecs::transform_component{};
        initial_transform.position(math::vec3<float>{0.0F, 0.0F, 0.0F});

        const auto target_entity = registry.create<ecs::transform_component>();
        registry.assign(target_entity, initial_transform);
        registry.assign(target_entity, box_collider_component{});
        registry.assign(target_entity, rigid_body_component{
            .motion = jolt::shim::motion_type::kinematic,
            .layer = jolt::shim::object_layer::moving,
            .mass = 0.0F,
        });

        const auto body_id_opt = scene_system.get_body_id(target_entity);
        ASSERT_TRUE(body_id_opt.has_value());

        // Act: Modify transform on component and step the scene system
        auto* transform = registry.try_get<ecs::transform_component>(target_entity);
        ASSERT_NE(transform, nullptr);
        transform->position(math::vec3<float>{3.0F, 4.0F, 5.0F});

        scene_system.step(1.0F / 60.0F);

        // Assert: Pre-step flushed the updated kinematic position into Jolt
        auto* const shim_system = world.physics_system();
        ASSERT_NE(shim_system, nullptr);
        const auto jolt_pos = shim_system->get_body_position(*body_id_opt);
        EXPECT_FLOAT_EQ(jolt_pos.x, 3.0F);
        EXPECT_FLOAT_EQ(jolt_pos.y, 4.0F);
        EXPECT_FLOAT_EQ(jolt_pos.z, 5.0F);
    }

    /// @brief Verifies that pre_simulation_flush and post_simulation_sync can be invoked
    /// decoupled from step(), correctly updating kinematic inputs and synchronizing dynamic outputs.
    TEST_F(physics_scene_fixture, physics_scene_system_manual_flush_and_sync_lifecycle)
    {
        // Setup: Create dynamic body falling under gravity and kinematic body to be repositioned
        auto world = physics_world{make_description()};
        auto scene_system = physics_scene_system{world, registry, event_reg};

        const auto dynamic_entity = registry.create<ecs::transform_component>();
        auto dynamic_transform = ecs::transform_component{};
        dynamic_transform.position(math::vec3<float>{0.0F, 10.0F, 0.0F});
        registry.assign(dynamic_entity, dynamic_transform);
        registry.assign(dynamic_entity, box_collider_component{});
        registry.assign(dynamic_entity, rigid_body_component{
            .motion = jolt::shim::motion_type::dynamic,
            .layer = jolt::shim::object_layer::moving,
            .mass = 1.0F,
        });

        const auto kinematic_entity = registry.create<ecs::transform_component>();
        auto kinematic_transform = ecs::transform_component{};
        kinematic_transform.position(math::vec3<float>{0.0F, 0.0F, 0.0F});
        registry.assign(kinematic_entity, kinematic_transform);
        registry.assign(kinematic_entity, box_collider_component{});
        registry.assign(kinematic_entity, rigid_body_component{
            .motion = jolt::shim::motion_type::kinematic,
            .layer = jolt::shim::object_layer::moving,
            .mass = 0.0F,
        });

        // Act: Update kinematic transform directly, flush pre-step, advance world independently, then post-sync
        auto* kin_tx = registry.try_get<ecs::transform_component>(kinematic_entity);
        ASSERT_NE(kin_tx, nullptr);
        kin_tx->position(math::vec3<float>{10.0F, 20.0F, 30.0F});

        scene_system.pre_simulation_flush();

        const auto kin_body = scene_system.get_body_id(kinematic_entity);
        ASSERT_TRUE(kin_body.has_value());
        auto* const shim_system = world.physics_system();
        ASSERT_NE(shim_system, nullptr);
        const auto flushed_kin_pos = shim_system->get_body_position(*kin_body);
        EXPECT_FLOAT_EQ(flushed_kin_pos.x, 10.0F);
        EXPECT_FLOAT_EQ(flushed_kin_pos.y, 20.0F);
        EXPECT_FLOAT_EQ(flushed_kin_pos.z, 30.0F);

        world.step(1.0F / 60.0F);

        scene_system.post_simulation_sync();

        // Assert: Dynamic entity transform has fallen under gravity after post-sync
        const auto* dyn_tx = registry.try_get<ecs::transform_component>(dynamic_entity);
        ASSERT_NE(dyn_tx, nullptr);
        EXPECT_LT(dyn_tx->position().y, 10.0F);
    }

    /// @brief Verifies that removing box_collider_component from a floor while a character
    /// is standing on it does not cause a crash or segfault, and character falls through.
    TEST_F(physics_scene_fixture, physics_scene_system_remove_floor_collider_under_character)
    {
        // Setup: World, scene system, static floor plane, and character controller
        auto world = physics_world{make_description()};
        auto scene_system = physics_scene_system{world, registry, event_reg};

        const auto floor_entity = registry.create<ecs::transform_component>();
        auto floor_tx = ecs::transform_component{};
        floor_tx.position(math::vec3<float>{0.0F, 0.0F, 0.0F});
        registry.assign(floor_entity, floor_tx);
        registry.assign(floor_entity, box_collider_component{
            .half_extents = {20.0F, 0.5F, 20.0F},
            .offset = {0.0F, -0.5F, 0.0F},
        });
        registry.assign(floor_entity, rigid_body_component{
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        });

        const auto player_entity = registry.create<character_controller_component, velocity_component,
                                                   character_movement_intent, ecs::transform_component,
                                                   ecs::transform_history_component>();
        auto controller = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
        };
        const auto initial_position = math::vec3<float>{0.0F, 1.0F, 0.0F};
        controller.id = world.spawn_character(controller, initial_position);

        auto player_tx = ecs::transform_component{};
        player_tx.position(initial_position);

        registry.replace(player_entity, controller);
        registry.replace(player_entity, velocity_component{});
        registry.replace(player_entity, character_movement_intent{});
        registry.replace(player_entity, player_tx);
        registry.replace(player_entity, ecs::transform_history_component::create(initial_position));

        // Act: Step character simulation for 10 frames until grounded on floor
        constexpr auto delta_time = chrono::duration<double>{1.0 / 60.0};
        const auto cmd = network::user_cmd{};
        for (auto i = 0; i < 10; ++i)
        {
            step_character_simulation(world, registry, player_entity, cmd, delta_time);
        }

        const auto* post_ground_controller = registry.try_get<character_controller_component>(player_entity);
        ASSERT_NE(post_ground_controller, nullptr);
        EXPECT_TRUE(post_ground_controller->is_grounded);

        // Act: Remove the box collider component from the floor
        registry.remove<box_collider_component>(floor_entity);

        // Act: Step character simulation again - character should now fall under gravity without crashing
        for (auto i = 0; i < 10; ++i)
        {
            step_character_simulation(world, registry, player_entity, cmd, delta_time);
        }

        // Assert: Character fell below the previous floor surface (Y < 0.9)
        const auto* falling_tx = registry.try_get<ecs::transform_component>(player_entity);
        ASSERT_NE(falling_tx, nullptr);
        EXPECT_LT(falling_tx->position().y, 0.9F);
    }

    /// @brief Verifies that init_from_registry updates existing bodies when collider extents have changed.
    TEST_F(physics_scene_fixture, init_from_registry_updates_existing_bodies_with_new_collider_extents)
    {
        // Setup: World and registry with scene system attached
        auto world = physics_world{make_description()};
        auto scene_system = physics_scene_system{world, registry, event_reg};

        const auto floor_entity = registry.create<ecs::transform_component>();
        auto floor_tx = ecs::transform_component{};
        floor_tx.position(math::vec3<float>{0.0F, 0.0F, 0.0F});
        registry.assign(floor_entity, floor_tx);

        // Assign initial small 0.5m box collider
        const auto small_collider = box_collider_component{
            .half_extents = math::vec3<float>{0.5F, 0.5F, 0.5F},
        };
        const auto static_body = rigid_body_component{
            .motion = jolt::shim::motion_type::static_motion,
            .layer = jolt::shim::object_layer::non_moving,
        };
        registry.assign(floor_entity, small_collider);
        registry.assign(floor_entity, static_body);

        EXPECT_TRUE(scene_system.has_body(floor_entity));

        // Mutate collider to 20m half-extents (simulating post-load deserialization)
        auto* mutable_collider = registry.try_get<box_collider_component>(floor_entity);
        ASSERT_NE(mutable_collider, nullptr);
        mutable_collider->half_extents = math::vec3<float>{20.0F, 0.5F, 20.0F};

        // Act: Re-initialize from registry (e.g. at play begin)
        scene_system.init_from_registry();

        // Spawn character at (5.0, 1.0, 5.0) which is outside 0.5m box but inside 20m box
        const auto player_entity = registry.create<ecs::transform_component>();
        auto player_tx = ecs::transform_component{};
        const auto initial_position = math::vec3<float>{5.0F, 1.0F, 5.0F};
        player_tx.position(initial_position);
        registry.assign(player_entity, player_tx);

        auto controller = character_controller_component{
            .character_height = 1.8F,
            .character_radius = 0.4F,
        };
        const auto char_id = world.spawn_character(controller, initial_position);
        controller.id = char_id;
        registry.assign(player_entity, controller);
        registry.assign(player_entity, velocity_component{});
        registry.assign(player_entity, character_movement_intent{});

        // Act: Step character simulation for 10 frames
        constexpr auto delta_time = chrono::duration<double>{1.0 / 60.0};
        const auto cmd = network::user_cmd{};
        for (auto frame_idx = 0; frame_idx < 10; ++frame_idx)
        {
            step_character_simulation(world, registry, player_entity, cmd, delta_time);
        }

        // Assert: Character remains supported by the 20m floor and is grounded
        const auto* post_ground_controller = registry.try_get<character_controller_component>(player_entity);
        ASSERT_NE(post_ground_controller, nullptr);
        EXPECT_TRUE(post_ground_controller->is_grounded);

        const auto* final_tx = registry.try_get<ecs::transform_component>(player_entity);
        ASSERT_NE(final_tx, nullptr);
        EXPECT_GT(final_tx->position().y, 0.5F);
    }
} // namespace tempest::physics::tests
