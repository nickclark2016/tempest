#ifndef TEMPEST_PHYSICS_VELOCITY_COMPONENT_HPP
#define TEMPEST_PHYSICS_VELOCITY_COMPONENT_HPP

#include <tempest/traits.hpp>
#include <tempest/vec3.hpp>

namespace tempest::physics
{
    struct velocity_component
    {
        math::vec3<float> linear{0.0F, 0.0F, 0.0F};
        math::vec3<float> angular{0.0F, 0.0F, 0.0F};
    };

    static_assert(tempest::ecs::component<velocity_component>);
} // namespace tempest::physics

#endif // TEMPEST_PHYSICS_VELOCITY_COMPONENT_HPP
