#ifndef TEMPEST_PHYSICS_CHARACTER_CONTROLLER_SYSTEM_HPP
#define TEMPEST_PHYSICS_CHARACTER_CONTROLLER_SYSTEM_HPP

#include <tempest/archetype.hpp>
#include <tempest/chrono.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/vec3.hpp>

namespace tempest::physics
{
    auto update_character_controllers(physics_world& world,
                                      ecs::archetype_registry& registry,
                                      chrono::duration<double> delta_time,
                                      math::vec3<float> gravity) -> void;

    inline auto update_character_controllers(physics_world& world,
                                             ecs::archetype_registry& registry,
                                             float delta_time,
                                             math::vec3<float> gravity) -> void
    {
        update_character_controllers(world, registry, chrono::duration<double>{static_cast<double>(delta_time)}, gravity);
    }
} // namespace tempest::physics

#endif // TEMPEST_PHYSICS_CHARACTER_CONTROLLER_SYSTEM_HPP
