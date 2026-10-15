#ifndef TEMPEST_PHYSICS_VELOCITY_COMPONENT_HPP
#define TEMPEST_PHYSICS_VELOCITY_COMPONENT_HPP

#include <tempest/reflection.hpp>
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

namespace tempest::ecs
{
    template <>
    struct is_authorable<physics::velocity_component> : false_type
    {
    };

    template <>
    struct reflect<physics::velocity_component>
    {
        static void describe(type_builder<physics::velocity_component>& builder)
        {
            builder.version(1)
                .field<&physics::velocity_component::linear>("linear", field_flags::read_only,
                    field_ui_hints{.display_name = "Linear Velocity"})
                .field<&physics::velocity_component::angular>("angular", field_flags::read_only,
                    field_ui_hints{.display_name = "Angular Velocity"});
        }
    };
} // namespace tempest::ecs

#endif // TEMPEST_PHYSICS_VELOCITY_COMPONENT_HPP
