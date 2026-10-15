#include <tempest/engine_types.hpp>

#include <tempest/archetype.hpp>
#include <tempest/material.hpp>
#include <tempest/physics/box_collider_component.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/rigid_body_component.hpp>
#include <tempest/physics/velocity_component.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/vertex.hpp>

namespace tempest
{
    void register_engine_types(ecs::component_type_registry& types)
    {
        types.register_type<ecs::transform_component>();
        types.register_type<ecs::self_component>();
        types.register_type<ecs::relationship_component<ecs::entity>>();
        types.register_type<ecs::transform_history_component>();
        types.register_type<core::mesh_component>();
        types.register_type<core::material_component>();
        types.register_type<render_system::camera_component>();
        types.register_type<render_system::directional_light_component>();
        types.register_type<render_system::point_light_component>();
        types.register_type<render_system::shadow_caster_component>();
        types.register_type<physics::velocity_component>();
        types.register_type<physics::character_controller_component>();
        types.register_type<physics::rigid_body_component>();
        types.register_type<physics::box_collider_component>();
    }
} // namespace tempest
