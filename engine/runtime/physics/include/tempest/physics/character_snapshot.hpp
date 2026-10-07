#ifndef TEMPEST_PHYSICS_CHARACTER_SNAPSHOT_HPP
#define TEMPEST_PHYSICS_CHARACTER_SNAPSHOT_HPP

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/int.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/physics/shim/jolt_shim.hpp>
#include <tempest/physics/velocity_component.hpp>
#include <tempest/quat.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/vec3.hpp>

namespace tempest::physics
{
    /// \brief Generic motion state for any non-static physics actor (dynamic bodies, characters, projectiles).
    struct alignas(16) actor_motion_snapshot
    {
        uint32_t tick = 0;
        math::vec3<float> position{0.0F, 0.0F, 0.0F};
        math::quat<float> rotation{0.0F, 0.0F, 0.0F, 1.0F};
        math::vec3<float> linear_velocity{0.0F, 0.0F, 0.0F};
        math::vec3<float> angular_velocity{0.0F, 0.0F, 0.0F};
    };

    /// \brief Specialized character snapshot extending motion snapshot with locomotion and ground state.
    struct alignas(16) character_snapshot
    {
        actor_motion_snapshot motion{};
        bool is_grounded = false;
        jolt::shim::ground_state current_ground_state = jolt::shim::ground_state::in_air;
        math::vec3<float> ground_normal{0.0F, 1.0F, 0.0F};

        [[nodiscard]] constexpr auto tick() const noexcept -> uint32_t
        {
            return motion.tick;
        }

        [[nodiscard]] constexpr auto position() const noexcept -> const math::vec3<float>&
        {
            return motion.position;
        }

        [[nodiscard]] constexpr auto linear_velocity() const noexcept -> const math::vec3<float>&
        {
            return motion.linear_velocity;
        }

        [[nodiscard]] constexpr auto rotation() const noexcept -> const math::quat<float>&
        {
            return motion.rotation;
        }
    };

    /// \brief Captures a character snapshot from component references.
    [[nodiscard]] TEMPEST_API auto capture_character_snapshot(uint32_t tick,
                                                              const character_controller_component& controller,
                                                              const ecs::transform_component& transform,
                                                              const velocity_component& velocity) -> character_snapshot;

    /// \brief Captures a character snapshot from an archetype registry entity.
    [[nodiscard]] TEMPEST_API auto capture_character_snapshot(uint32_t tick,
                                                              const ecs::archetype_registry& registry,
                                                              ecs::entity entity) -> character_snapshot;

    /// \brief Restores a character snapshot into component references, optional physics_world, and optional transform history.
    TEMPEST_API auto restore_character_snapshot(character_controller_component& controller,
                                                ecs::transform_component& transform,
                                                velocity_component& velocity,
                                                const character_snapshot& snapshot,
                                                physics_world* world = nullptr,
                                                ecs::transform_history_component* history = nullptr) -> void;

    /// \brief Restores a character snapshot into an archetype registry entity and optional physics_world.
    TEMPEST_API auto restore_character_snapshot(ecs::archetype_registry& registry,
                                                ecs::entity entity,
                                                const character_snapshot& snapshot,
                                                physics_world* world = nullptr) -> void;

    /// \brief Captures an actor motion snapshot from a Jolt physics system rigid body.
    [[nodiscard]] TEMPEST_API auto capture_body_snapshot(uint32_t tick,
                                                         const jolt::shim::physics_system& system,
                                                         jolt::shim::body_id target_body) -> actor_motion_snapshot;

    /// \brief Restores an actor motion snapshot to a Jolt physics system rigid body.
    TEMPEST_API auto restore_body_snapshot(jolt::shim::physics_system& system,
                                           jolt::shim::body_id target_body,
                                           const actor_motion_snapshot& snapshot) -> void;
} // namespace tempest::physics

#endif // TEMPEST_PHYSICS_CHARACTER_SNAPSHOT_HPP
