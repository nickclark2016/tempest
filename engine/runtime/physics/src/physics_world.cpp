#include <tempest/algorithm.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/utility.hpp>

namespace tempest::physics
{
    physics_world::physics_world() : _owns_physics_system(true)
    {
        auto description = jolt::shim::init_desc{};
        description.job_dispatch = [](jolt::shim::job_execute_fn execution_callback, void* job_context,
                                      void* /*user_data*/) -> void {
            if (execution_callback != nullptr)
            {
                execution_callback(job_context);
            }
        };
        _physics_system = jolt::shim::create_physics_system(description);
    }

    physics_world::physics_world(jolt::shim::physics_system* system) : _physics_system(system)
    {
    }

    physics_world::physics_world(const jolt::shim::init_desc& description)
        : _physics_system(jolt::shim::create_physics_system(description)), _owns_physics_system(true)
    {
    }

    physics_world::physics_world(physics_world&& other) noexcept
        : _physics_system(tempest::exchange(other._physics_system, nullptr)),
          _owns_physics_system(tempest::exchange(other._owns_physics_system, false)),
          _characters(tempest::move(other._characters))
    {
    }

    physics_world::~physics_world()
    {
        _release();
    }

    auto physics_world::operator=(physics_world&& other) noexcept -> physics_world&
    {
        if (this == &other)
        {
            return *this;
        }

        _release();

        _physics_system = tempest::exchange(other._physics_system, nullptr);
        _owns_physics_system = tempest::exchange(other._owns_physics_system, false);
        _characters = tempest::move(other._characters);

        return *this;
    }

    auto physics_world::spawn_character(const character_controller_component& config, math::vec3<float> position,
                                        math::quat<float> rotation) -> character_id
    {
        if (_physics_system == nullptr)
        {
            return invalid_character_id;
        }

        const auto half_height =
            tempest::max(0.0F, (config.character_height - (2.0F * config.character_radius)) * 0.5F);
        auto* const shape = _physics_system->create_capsule_shape(half_height, config.character_radius);
        if (shape == nullptr)
        {
            return invalid_character_id;
        }

        const auto description = jolt::shim::character_virtual_desc{
            .shape = shape,
            .position =
                jolt::shim::vec3{
                    .x = position.x,
                    .y = position.y,
                    .z = position.z,
                },
            .rotation =
                jolt::shim::quat{
                    .x = rotation.x,
                    .y = rotation.y,
                    .z = rotation.z,
                    .w = rotation.w,
                },
            .up =
                jolt::shim::vec3{
                    .x = 0.0F,
                    .y = 1.0F,
                    .z = 0.0F,
                },
            .mass = config.mass,
            .max_strength = config.max_strength,
            .max_slope_angle = config.max_slope_angle,
            .shape_offset =
                jolt::shim::vec3{
                    .x = 0.0F,
                    .y = 0.0F,
                    .z = 0.0F,
                },
            .character_padding = jolt::shim::default_character_padding,
            .penetration_recovery_speed = jolt::shim::default_character_penetration_recovery_speed,
            .predictive_contact_distance = jolt::shim::default_character_predictive_contact_distance,
            .enhanced_internal_edge_removal = jolt::shim::default_character_enhanced_internal_edge_removal,
            .max_collision_iterations = jolt::shim::default_character_max_collision_iterations,
            .max_constraint_iterations = jolt::shim::default_character_max_constraint_iterations,
            .collision_tolerance = jolt::shim::default_character_collision_tolerance,
            .inner_body_shape = nullptr,
            .inner_body_layer = jolt::shim::object_layer::moving,
            .callbacks = {},
        };

        auto* const character = _physics_system->create_character_virtual(description);
        if (character == nullptr)
        {
            _physics_system->destroy_shape(shape);
            return invalid_character_id;
        }

        return _characters.insert(character_entry{
            .character = character,
            .shape = shape,
        });
    }

    auto physics_world::despawn_character(character_id char_id) -> void
    {
        if (char_id == invalid_character_id)
        {
            return;
        }

        const auto character_iterator = _characters.find(char_id);
        if (character_iterator != _characters.end())
        {
            const auto entry = *character_iterator;
            if (_physics_system != nullptr)
            {
                if (entry.character != nullptr)
                {
                    _physics_system->destroy_character_virtual(entry.character);
                }
                if (entry.shape != nullptr)
                {
                    _physics_system->destroy_shape(entry.shape);
                }
            }
            _characters.erase(char_id);
        }
    }

    auto physics_world::get_character(character_id char_id) const -> jolt::shim::character_virtual*
    {
        if (char_id == invalid_character_id)
        {
            return nullptr;
        }

        const auto character_iterator = _characters.find(char_id);
        if (character_iterator != _characters.end())
        {
            return character_iterator->character;
        }
        return nullptr;
    }

    auto physics_world::physics_system() const noexcept -> jolt::shim::physics_system*
    {
        return _physics_system;
    }

    auto physics_world::step(chrono::duration<double> delta_time) -> void
    {
        if (_physics_system != nullptr)
        {
            _physics_system->step(static_cast<float>(delta_time.count()));
        }
    }

    auto physics_world::step(float delta_time) -> void
    {
        step(chrono::duration<double>{static_cast<double>(delta_time)});
    }

    auto physics_world::_release() -> void
    {
        for (const auto& entry : _characters)
        {
            if (_physics_system != nullptr)
            {
                if (entry.character != nullptr)
                {
                    _physics_system->destroy_character_virtual(entry.character);
                }
                if (entry.shape != nullptr)
                {
                    _physics_system->destroy_shape(entry.shape);
                }
            }
        }
        _characters.clear();

        if (_owns_physics_system && _physics_system != nullptr)
        {
            jolt::shim::destroy_physics_system(_physics_system);
        }
        _physics_system = nullptr;
        _owns_physics_system = false;
    }
} // namespace tempest::physics
