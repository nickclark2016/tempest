#ifndef TEMPEST_PHYSICS_BOX_COLLIDER_COMPONENT_HPP
#define TEMPEST_PHYSICS_BOX_COLLIDER_COMPONENT_HPP

#include <tempest/reflection.hpp>
#include <tempest/traits.hpp>
#include <tempest/vec3.hpp>

namespace tempest::physics
{
    struct box_collider_component
    {
        static constexpr math::vec3<float> default_half_extents{0.5F, 0.5F, 0.5F};
        static constexpr math::vec3<float> default_offset{0.0F, 0.0F, 0.0F};

        math::vec3<float> half_extents = default_half_extents;
        math::vec3<float> offset = default_offset;
    };

    static_assert(tempest::ecs::component<box_collider_component>);
} // namespace tempest::physics

namespace tempest::ecs
{
    template <>
    struct reflect<physics::box_collider_component>
    {
        static void describe(type_builder<physics::box_collider_component>& builder)
        {
            builder.version(1)
                .field<&physics::box_collider_component::half_extents>("half_extents", field_flags::none,
                    field_ui_hints{.drag_speed = 0.05F, .display_name = "Half Extents"})
                .field<&physics::box_collider_component::offset>("offset", field_flags::none,
                    field_ui_hints{.drag_speed = 0.05F, .display_name = "Offset"});
        }
    };
} // namespace tempest::ecs

#endif // TEMPEST_PHYSICS_BOX_COLLIDER_COMPONENT_HPP
