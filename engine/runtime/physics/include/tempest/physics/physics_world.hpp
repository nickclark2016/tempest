#ifndef TEMPEST_PHYSICS_PHYSICS_WORLD_HPP
#define TEMPEST_PHYSICS_PHYSICS_WORLD_HPP

#include <tempest/chrono.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/shim/jolt_shim.hpp>
#include <tempest/quat.hpp>
#include <tempest/slot_map.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vector.hpp>

namespace tempest::physics
{
    class physics_world
    {
      public:
        physics_world();
        explicit physics_world(jolt::shim::physics_system* system);
        explicit physics_world(const jolt::shim::init_desc& description);
        physics_world(const physics_world&) = delete;
        physics_world(physics_world&& other) noexcept;
        ~physics_world();

        auto operator=(const physics_world&) -> physics_world& = delete;
        auto operator=(physics_world&& other) noexcept -> physics_world&;

        [[nodiscard]] auto spawn_character(const character_controller_component& config, math::vec3<float> position,
                                           math::quat<float> rotation = {0.0F, 0.0F, 0.0F, 1.0F}) -> character_id;

        auto despawn_character(character_id char_id) -> void;

        [[nodiscard]] auto get_character(character_id char_id) const -> jolt::shim::character_virtual*;

        [[nodiscard]] auto physics_system() const noexcept -> jolt::shim::physics_system*
        {
            return _physics_system;
        }

        auto step(chrono::duration<double> delta_time) -> void;
        auto step(float delta_time) -> void
        {
            step(chrono::duration<double>{static_cast<double>(delta_time)});
        }

      private:
        jolt::shim::physics_system* _physics_system = nullptr;
        bool _owns_physics_system = false;
        tempest::slot_map<jolt::shim::character_virtual*> _characters;
        tempest::vector<jolt::shim::shape_handle> _allocated_shapes;

        auto _release() -> void;
    };
} // namespace tempest::physics

#endif // TEMPEST_PHYSICS_PHYSICS_WORLD_HPP
