#ifndef TEMPEST_PHYSICS_CHARACTER_CONTROLLER_COMPONENT_HPP
#define TEMPEST_PHYSICS_CHARACTER_CONTROLLER_COMPONENT_HPP

#include <tempest/int.hpp>
#include <tempest/physics/shim/jolt_shim.hpp>
#include <tempest/traits.hpp>
#include <tempest/vec3.hpp>

namespace tempest::physics
{
    using character_id = uint32_t;
    inline constexpr character_id invalid_character_id = 0xFFFFFFFF;

    // Locomotion & Sizing Defaults

    /// \brief Default total height in meters (1.8 m) of the character capsule.
    inline constexpr float default_character_height = 1.8F;

    /// \brief Default cylinder radius and hemispherical cap radius in meters (0.4 m) of the character capsule.
    inline constexpr float default_character_radius = 0.4F;

    /// \brief Default character mass in kilograms (80 kg). Sourced from jolt::shim defaults.
    inline constexpr float default_character_mass = jolt::shim::default_character_mass;

    /// \brief Default maximum walkable slope angle in radians (~50 degrees). Sourced from jolt::shim defaults.
    inline constexpr float default_character_max_slope_angle = jolt::shim::default_character_max_slope_angle;

    /// \brief Default maximum force in Newtons (100 N) that the character can push dynamic rigid bodies.
    inline constexpr float default_character_max_strength = jolt::shim::default_character_max_strength;

    /// \brief Default base walking speed in meters per second (5.0 m/s).
    inline constexpr float default_character_walk_speed = 5.0F;

    /// \brief Default sprinting speed in meters per second (8.0 m/s).
    inline constexpr float default_character_run_speed = 8.0F;

    /// \brief Default peak jump clearance height in meters (1.2 m) used to compute vertical takeoff impulse.
    inline constexpr float default_character_jump_height = 1.2F;

    /// \brief Default horizontal acceleration rate in 1/s (15.0) used for sharp velocity interpolation.
    inline constexpr float default_character_acceleration_rate = 15.0F;

    /// \brief Default downward probe distance in meters (0.5 m) to snap the character to the ground when descending.
    inline constexpr float default_character_stick_to_floor_step_down = 0.5F;

    /// \brief Default maximum vertical step height in meters (0.4 m) for automatic stair navigation.
    inline constexpr float default_character_walk_stairs_step_up = 0.4F;

    struct character_controller_component
    {
        // Configuration
        float character_height = default_character_height;
        float character_radius = default_character_radius;
        float mass = default_character_mass;
        float max_slope_angle = default_character_max_slope_angle;
        float max_strength = default_character_max_strength;
        float walk_speed = default_character_walk_speed;
        float run_speed = default_character_run_speed;
        float jump_height = default_character_jump_height;
        float acceleration_rate = default_character_acceleration_rate;
        float stick_to_floor_step_down = default_character_stick_to_floor_step_down;
        float walk_stairs_step_up = default_character_walk_stairs_step_up;

        // Runtime state
        character_id id = invalid_character_id;
        bool is_grounded = false;
        jolt::shim::ground_state current_ground_state = jolt::shim::ground_state::in_air;
        math::vec3<float> linear_velocity{0.0F, 0.0F, 0.0F};
        math::vec3<float> ground_normal{0.0F, 1.0F, 0.0F};
    };

    static_assert(tempest::ecs::component<character_controller_component>);
} // namespace tempest::physics

#endif // TEMPEST_PHYSICS_CHARACTER_CONTROLLER_COMPONENT_HPP
