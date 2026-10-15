#ifndef TEMPEST_PHYSICS_RIGID_BODY_COMPONENT_HPP
#define TEMPEST_PHYSICS_RIGID_BODY_COMPONENT_HPP

#include <tempest/int.hpp>
#include <tempest/physics/shim/jolt_shim.hpp>
#include <tempest/reflection.hpp>
#include <tempest/traits.hpp>

namespace tempest::physics
{
    struct rigid_body_component
    {
        static constexpr jolt::shim::motion_type default_motion = jolt::shim::motion_type::static_motion;
        static constexpr jolt::shim::object_layer default_layer = jolt::shim::object_layer::non_moving;
        static constexpr float default_mass = 0.0F;

        jolt::shim::motion_type motion = default_motion;
        jolt::shim::object_layer layer = default_layer;
        float mass = default_mass;
        jolt::shim::body_id id = jolt::shim::invalid_body_id;
    };

    static_assert(tempest::ecs::component<rigid_body_component>);
} // namespace tempest::physics

namespace tempest::ecs
{
    inline constexpr enum_entry motion_type_entries[] = {
        {.name = "Static", .value = static_cast<int64_t>(jolt::shim::motion_type::static_motion)},
        {.name = "Kinematic", .value = static_cast<int64_t>(jolt::shim::motion_type::kinematic)},
        {.name = "Dynamic", .value = static_cast<int64_t>(jolt::shim::motion_type::dynamic)},
    };

    inline constexpr enum_entry object_layer_entries[] = {
        {.name = "Non-Moving", .value = static_cast<int64_t>(jolt::shim::object_layer::non_moving)},
        {.name = "Moving", .value = static_cast<int64_t>(jolt::shim::object_layer::moving)},
    };

    template <>
    struct reflect<physics::rigid_body_component>
    {
        static void describe(type_builder<physics::rigid_body_component>& builder)
        {
            builder.version(1)
                .field<&physics::rigid_body_component::motion>("motion", motion_type_entries, field_flags::none,
                    field_ui_hints{.display_name = "Motion"})
                .field<&physics::rigid_body_component::layer>("layer", object_layer_entries, field_flags::none,
                    field_ui_hints{.display_name = "Layer"})
                .field<&physics::rigid_body_component::mass>("mass", field_flags::none,
                    field_ui_hints{.min_value = 0.0, .drag_speed = 0.1F, .display_name = "Mass"});
        }
    };
} // namespace tempest::ecs

#endif // TEMPEST_PHYSICS_RIGID_BODY_COMPONENT_HPP
