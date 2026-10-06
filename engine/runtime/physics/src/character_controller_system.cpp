#include <tempest/algorithm.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/character_controller_system.hpp>
#include <tempest/physics/character_movement_intent.hpp>
#include <tempest/physics/velocity_component.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>

namespace tempest::physics
{
    auto update_character_controllers(physics_world& world,
                                      ecs::archetype_registry& registry,
                                      chrono::duration<double> delta_time,
                                      math::vec3<float> gravity) -> void
    {
        const auto dt = static_cast<float>(delta_time.count());

        registry.each([&](const ecs::self_component& self,
                          character_controller_component& controller,
                          const character_movement_intent& intent,
                          ecs::transform_component& transform) {
            if (controller.id == invalid_character_id)
            {
                controller.id = world.spawn_character(controller, transform.position(), transform.rotation_quat());
            }

            auto* character = world.get_character(controller.id);
            if (character == nullptr)
            {
                return;
            }

            // 1. Normalize horizontal wish direction.
            auto wish_direction = math::vec3<float>{intent.wish_direction.x, 0.0F, intent.wish_direction.z};
            const auto wish_length_squared = wish_direction.x * wish_direction.x + wish_direction.z * wish_direction.z;
            if (wish_length_squared > 1.0e-6F)
            {
                const auto inverse_length = 1.0F / math::sqrt(wish_length_squared);
                wish_direction.x *= inverse_length;
                wish_direction.z *= inverse_length;
            }

            // 2. Calculate target horizontal velocity (d_wish * speed).
            const auto speed = intent.sprint_requested ? controller.run_speed : controller.walk_speed;
            const auto target_horizontal_velocity = wish_direction * speed;

            // 3. Lerp current horizontal velocity toward target with acceleration_rate.
            const auto current_linear_velocity = character->get_linear_velocity();
            auto current_horizontal_velocity = math::vec3<float>{current_linear_velocity.x, 0.0F, current_linear_velocity.z};
            const auto alpha = math::clamp(controller.acceleration_rate * dt, 0.0F, 1.0F);
            current_horizontal_velocity = math::lerp(current_horizontal_velocity, target_horizontal_velocity, alpha);

            // 4. Manage vertical velocity.
            const auto ground_state = character->get_ground_state();
            const auto is_grounded = (ground_state == jolt::shim::ground_state::on_ground);

            auto vertical_velocity = current_linear_velocity.y;
            const auto moving_towards_ground = (vertical_velocity <= 0.1F);
            if (is_grounded && moving_towards_ground)
            {
                vertical_velocity = 0.0F;
                if (intent.jump_requested)
                {
                    const auto gravity_magnitude = math::max(math::abs(gravity.y), 1.0e-4F);
                    vertical_velocity = math::sqrt(2.0F * gravity_magnitude * controller.jump_height);
                }
            }

            vertical_velocity += gravity.y * dt;

            // 5. Check stairs or cancel velocity towards steep slopes for horizontal movement.
            auto horizontal_velocity = jolt::shim::vec3{
                .x = current_horizontal_velocity.x,
                .y = 0.0F,
                .z = current_horizontal_velocity.z,
            };
            if (!character->can_walk_stairs(horizontal_velocity))
            {
                horizontal_velocity = character->cancel_velocity_towards_steep_slopes(horizontal_velocity);
            }

            // 6. Set composite linear velocity.
            const auto desired_velocity = jolt::shim::vec3{
                .x = horizontal_velocity.x,
                .y = vertical_velocity,
                .z = horizontal_velocity.z,
            };
            character->set_linear_velocity(desired_velocity);

            // 7. Execute character->extended_update with stick_to_floor and walk_stairs settings.
            const auto update_settings = jolt::shim::extended_update_settings{
                .stick_to_floor_step_down = jolt::shim::vec3{
                    .x = 0.0F,
                    .y = -controller.stick_to_floor_step_down,
                    .z = 0.0F,
                },
                .walk_stairs_step_up = jolt::shim::vec3{
                    .x = 0.0F,
                    .y = controller.walk_stairs_step_up,
                    .z = 0.0F,
                },
                .walk_stairs_min_step_forward = jolt::shim::default_walk_stairs_min_step_forward,
                .walk_stairs_step_forward_test = jolt::shim::default_walk_stairs_step_forward_test,
                .walk_stairs_cos_angle_forward_contact = jolt::shim::default_walk_stairs_cos_angle_forward_contact,
                .walk_stairs_step_down_extra = jolt::shim::default_walk_stairs_step_down_extra,
            };
            const auto shim_gravity = jolt::shim::vec3{
                .x = gravity.x,
                .y = gravity.y,
                .z = gravity.z,
            };
            character->extended_update(dt, shim_gravity, update_settings);

            // 8. Update character_controller_component runtime state.
            const auto new_position = character->get_position();
            const auto new_rotation = character->get_rotation();
            const auto new_velocity = character->get_linear_velocity();
            const auto new_ground_state = character->get_ground_state();
            const auto new_ground_normal = character->get_ground_normal();

            controller.current_ground_state = new_ground_state;
            controller.is_grounded = (new_ground_state == jolt::shim::ground_state::on_ground);
            controller.ground_normal = math::vec3<float>{new_ground_normal.x, new_ground_normal.y, new_ground_normal.z};

            if (auto* velocity = registry.try_get<velocity_component>(self.entity))
            {
                auto* const mutable_velocity = const_cast<velocity_component*>(velocity);
                mutable_velocity->linear = math::vec3<float>{new_velocity.x, new_velocity.y, new_velocity.z};
            }

            // 9. Update transform_component with new position and rotation.
            const auto final_position = math::vec3<float>{new_position.x, new_position.y, new_position.z};
            const auto final_rotation = math::quat<float>{new_rotation.x, new_rotation.y, new_rotation.z, new_rotation.w};
            transform.set(final_position, final_rotation);

            // 10. If entity also has transform_history_component, update current_position and current_rotation.
            if (auto* history = registry.try_get<ecs::transform_history_component>(self.entity))
            {
                auto* mutable_history = const_cast<ecs::transform_history_component*>(history);
                mutable_history->current_position = final_position;
                mutable_history->current_rotation = final_rotation;
            }
        });
    }
} // namespace tempest::physics
