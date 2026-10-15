#ifndef TEMPEST_PHYSICS_CHARACTER_MOVEMENT_INTENT_HPP
#define TEMPEST_PHYSICS_CHARACTER_MOVEMENT_INTENT_HPP

#include <tempest/traits.hpp>
#include <tempest/vec3.hpp>

namespace tempest::physics
{
    struct character_movement_intent
    {
        math::vec3<float> wish_direction{0.0F, 0.0F, 0.0F};
        bool jump_requested = false;
        bool sprint_requested = false;
    };

    static_assert(tempest::ecs::component<character_movement_intent>);
} // namespace tempest::physics

namespace tempest::ecs
{
    template <>
    struct is_authorable<physics::character_movement_intent> : false_type
    {
    };
} // namespace tempest::ecs

#endif // TEMPEST_PHYSICS_CHARACTER_MOVEMENT_INTENT_HPP
