#ifndef TEMPEST_PHYSICS_CHARACTER_PREDICTION_HPP
#define TEMPEST_PHYSICS_CHARACTER_PREDICTION_HPP

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/int.hpp>
#include <tempest/network/prediction_buffer.hpp>
#include <tempest/network/user_cmd.hpp>
#include <tempest/physics/character_movement_intent.hpp>
#include <tempest/physics/character_snapshot.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/vec3.hpp>

#if defined(_WIN32) && !defined(TEMPEST_PHYSICS_DYNAMIC)
#   undef TEMPEST_API
#   define TEMPEST_API
#endif

namespace tempest::physics
{
    using character_prediction_buffer = network::client_prediction_buffer<character_snapshot>;

    /// @brief Maps network user command input state (view yaw, movement axes, buttons) into character movement intent.
    /// @param cmd The incoming client input command.
    /// @return Newly synthesized character_movement_intent.
    [[nodiscard]] TEMPEST_API auto apply_user_cmd(const network::user_cmd& cmd) -> character_movement_intent;

    /// @brief Steps the character controller and physics world predictively for one client simulation tick,
    /// capturing the resulting state snapshot and recording both command and state into the prediction ring buffer.
    /// @param world The physics world simulation instance.
    /// @param registry Archetype registry containing the player entity and components.
    /// @param player_entity Entity handle of the character being predicted.
    /// @param cmd Input command driving this tick's movement.
    /// @param delta_time Simulation time delta in seconds.
    /// @param buffer Circular client prediction buffer storing predicted inputs and frames.
    /// @param gravity World gravity vector (defaults to Earth standard (0, -9.81, 0)).
    /// @return Reference to the predicted character snapshot stored within the prediction buffer.
    TEMPEST_API auto step_client_prediction(physics_world& world,
                                            ecs::archetype_registry& registry,
                                            ecs::entity player_entity,
                                            const network::user_cmd& cmd,
                                            chrono::duration<double> delta_time,
                                            character_prediction_buffer& buffer,
                                            math::vec3<float> gravity = math::vec3<float>{0.0F, -9.81F, 0.0F})
        -> const character_snapshot&;

    inline auto step_client_prediction(physics_world& world,
                                       ecs::archetype_registry& registry,
                                       ecs::entity player_entity,
                                       const network::user_cmd& cmd,
                                       float delta_time,
                                       character_prediction_buffer& buffer,
                                       math::vec3<float> gravity = math::vec3<float>{0.0F, -9.81F, 0.0F})
        -> const character_snapshot&
    {
        return step_client_prediction(world, registry, player_entity, cmd,
                                      chrono::duration<double>{static_cast<double>(delta_time)}, buffer, gravity);
    }
} // namespace tempest::physics

#endif // TEMPEST_PHYSICS_CHARACTER_PREDICTION_HPP
