#include <tempest/physics/character_prediction.hpp>

#include <tempest/math_utils.hpp>
#include <tempest/physics/character_controller_system.hpp>

namespace tempest::physics
{
    auto apply_user_cmd(const network::user_cmd& cmd) -> character_movement_intent
    {
        const auto sin_yaw = math::sin(cmd.view_yaw);
        const auto cos_yaw = math::cos(cmd.view_yaw);

        const auto forward = math::vec3<float>{sin_yaw, 0.0F, cos_yaw};
        const auto right = math::vec3<float>{cos_yaw, 0.0F, -sin_yaw};

        return character_movement_intent{
            .wish_direction = forward * cmd.forward_move + right * cmd.right_move,
            .jump_requested = (cmd.buttons & network::user_button_jump) != 0,
            .sprint_requested = (cmd.buttons & network::user_button_sprint) != 0,
        };
    }

    auto step_client_prediction(physics_world& world,
                                ecs::archetype_registry& registry,
                                ecs::entity player_entity,
                                const network::user_cmd& cmd,
                                chrono::duration<double> delta_time,
                                character_prediction_buffer& buffer,
                                math::vec3<float> gravity) -> const character_snapshot&
    {
        registry.assign_or_replace(player_entity, apply_user_cmd(cmd));

        update_character_controllers(world, registry, delta_time, gravity);
        world.step(delta_time);

        const auto snapshot = capture_character_snapshot(cmd.tick, registry, player_entity);
        const auto& recorded_frame = buffer.record_input(cmd, snapshot);
        return recorded_frame.predicted_state;
    }
} // namespace tempest::physics
