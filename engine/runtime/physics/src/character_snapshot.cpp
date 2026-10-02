#include <tempest/physics/character_snapshot.hpp>

namespace tempest::physics
{
    auto capture_character_snapshot(uint32_t tick,
                                    const character_controller_component& controller,
                                    const ecs::transform_component& transform,
                                    const velocity_component& velocity) -> character_snapshot
    {
        auto snapshot = character_snapshot{};
        snapshot.motion.tick = tick;
        snapshot.motion.position = transform.position();
        snapshot.motion.rotation = transform.rotation_quat();
        snapshot.motion.linear_velocity = velocity.linear;
        snapshot.motion.angular_velocity = velocity.angular;
        snapshot.is_grounded = controller.is_grounded;
        snapshot.current_ground_state = controller.current_ground_state;
        snapshot.ground_normal = controller.ground_normal;
        return snapshot;
    }

    auto capture_character_snapshot(uint32_t tick,
                                    const ecs::archetype_registry& registry,
                                    ecs::entity entity) -> character_snapshot
    {
        const auto& controller = registry.get<character_controller_component>(entity);
        const auto& transform = registry.get<ecs::transform_component>(entity);
        if (const auto* vel = registry.try_get<velocity_component>(entity))
        {
            return capture_character_snapshot(tick, controller, transform, *vel);
        }

        const auto empty_velocity = velocity_component{};
        return capture_character_snapshot(tick, controller, transform, empty_velocity);
    }

    auto restore_character_snapshot(character_controller_component& controller,
                                    ecs::transform_component& transform,
                                    velocity_component& velocity,
                                    const character_snapshot& snapshot,
                                    physics_world* world,
                                    ecs::transform_history_component* history) -> void
    {
        controller.is_grounded = snapshot.is_grounded;
        controller.current_ground_state = snapshot.current_ground_state;
        controller.ground_normal = snapshot.ground_normal;

        velocity.linear = snapshot.motion.linear_velocity;
        velocity.angular = snapshot.motion.angular_velocity;

        transform.set(snapshot.motion.position, snapshot.motion.rotation);

        if (history != nullptr)
        {
            history->previous_position = snapshot.motion.position;
            history->previous_rotation = snapshot.motion.rotation;
            history->current_position = snapshot.motion.position;
            history->current_rotation = snapshot.motion.rotation;
            history->render_position = snapshot.motion.position;
            history->render_rotation = snapshot.motion.rotation;
        }

        if (world != nullptr && controller.id != invalid_character_id)
        {
            if (auto* character = world->get_character(controller.id))
            {
                character->set_position(jolt::shim::vec3{
                    .x = snapshot.motion.position.x,
                    .y = snapshot.motion.position.y,
                    .z = snapshot.motion.position.z,
                });
                character->set_rotation(jolt::shim::quat{
                    .x = snapshot.motion.rotation.x,
                    .y = snapshot.motion.rotation.y,
                    .z = snapshot.motion.rotation.z,
                    .w = snapshot.motion.rotation.w,
                });
                character->set_linear_velocity(jolt::shim::vec3{
                    .x = snapshot.motion.linear_velocity.x,
                    .y = snapshot.motion.linear_velocity.y,
                    .z = snapshot.motion.linear_velocity.z,
                });
            }
        }
    }

    auto restore_character_snapshot(ecs::archetype_registry& registry,
                                    ecs::entity entity,
                                    const character_snapshot& snapshot,
                                    physics_world* world) -> void
    {
        const auto* controller = registry.try_get<character_controller_component>(entity);
        const auto* transform = registry.try_get<ecs::transform_component>(entity);
        if (controller == nullptr || transform == nullptr)
        {
            return;
        }

        auto* mutable_controller = const_cast<character_controller_component*>(controller);
        auto* mutable_transform = const_cast<ecs::transform_component*>(transform);

        const auto* history = registry.try_get<ecs::transform_history_component>(entity);
        auto* mutable_history = history != nullptr ? const_cast<ecs::transform_history_component*>(history) : nullptr;

        if (const auto* vel = registry.try_get<velocity_component>(entity))
        {
            auto* const mutable_velocity = const_cast<velocity_component*>(vel);
            restore_character_snapshot(*mutable_controller, *mutable_transform, *mutable_velocity, snapshot, world, mutable_history);
        }
        else
        {
            auto dummy_velocity = velocity_component{};
            restore_character_snapshot(*mutable_controller, *mutable_transform, dummy_velocity, snapshot, world, mutable_history);
        }
    }

    auto capture_body_snapshot(uint32_t tick,
                               const jolt::shim::physics_system& system,
                               jolt::shim::body_id target_body) -> actor_motion_snapshot
    {
        const auto pos = system.get_body_position(target_body);
        const auto rot = system.get_body_rotation(target_body);
        const auto lin_vel = system.get_body_linear_velocity(target_body);
        const auto ang_vel = system.get_body_angular_velocity(target_body);

        return actor_motion_snapshot{
            .tick = tick,
            .position = math::vec3<float>{pos.x, pos.y, pos.z},
            .rotation = math::quat<float>{rot.x, rot.y, rot.z, rot.w},
            .linear_velocity = math::vec3<float>{lin_vel.x, lin_vel.y, lin_vel.z},
            .angular_velocity = math::vec3<float>{ang_vel.x, ang_vel.y, ang_vel.z},
        };
    }

    auto restore_body_snapshot(jolt::shim::physics_system& system,
                               jolt::shim::body_id target_body,
                               const actor_motion_snapshot& snapshot) -> void
    {
        system.set_body_position(target_body, jolt::shim::vec3{
            .x = snapshot.position.x,
            .y = snapshot.position.y,
            .z = snapshot.position.z,
        });
        system.set_body_rotation(target_body, jolt::shim::quat{
            .x = snapshot.rotation.x,
            .y = snapshot.rotation.y,
            .z = snapshot.rotation.z,
            .w = snapshot.rotation.w,
        });
        system.set_body_linear_velocity(target_body, jolt::shim::vec3{
            .x = snapshot.linear_velocity.x,
            .y = snapshot.linear_velocity.y,
            .z = snapshot.linear_velocity.z,
        });
        system.set_body_angular_velocity(target_body, jolt::shim::vec3{
            .x = snapshot.angular_velocity.x,
            .y = snapshot.angular_velocity.y,
            .z = snapshot.angular_velocity.z,
        });
    }
} // namespace tempest::physics
