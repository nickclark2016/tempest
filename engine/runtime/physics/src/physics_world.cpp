#include <tempest/algorithm.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/utility.hpp>

namespace tempest::physics
{
    namespace
    {
        [[nodiscard]] auto to_slot_map_key(character_id char_id) -> slot_map<jolt::shim::character_virtual*>::key_type
        {
            if (char_id == invalid_character_id)
            {
                return slot_map_traits<slot_map<jolt::shim::character_virtual*>::key_type>::empty;
            }
#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
            const auto slot_index = static_cast<uint64_t>(char_id & 0x000FFFFF);
            const auto generation = static_cast<uint64_t>((char_id >> 20) & 0x0FFF);
            return (generation << 32) | slot_index; // NOLINT
#else
            return char_id;
#endif
        }

        [[nodiscard]] auto to_character_id(slot_map<jolt::shim::character_virtual*>::key_type key) -> character_id
        {
#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
            const auto slot_index = static_cast<uint32_t>(key & 0x000FFFFF);
            const auto generation = static_cast<uint32_t>((key >> 32) & 0x0FFF);
            return (generation << 20) | slot_index; // NOLINT
#else
            return key;
#endif
        }
    } // namespace

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
        : _physics_system(other._physics_system), _owns_physics_system(other._owns_physics_system),
          _characters(tempest::move(other._characters)), _allocated_shapes(tempest::move(other._allocated_shapes))
    {
        other._physics_system = nullptr;
        other._owns_physics_system = false;
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
        _allocated_shapes = tempest::move(other._allocated_shapes);

        return *this;
    }

    auto physics_world::spawn_character(const character_controller_component& config, math::vec3<float> position,
                                        math::quat<float> rotation) -> character_id
    {
        if (_physics_system == nullptr)
        {
            return invalid_character_id;
        }

        const auto half_height = tempest::max(0.0F, (config.character_height - (2.0F * config.character_radius)) * 0.5F);
        auto* const shape = _physics_system->create_capsule_shape(half_height, config.character_radius);
        _allocated_shapes.push_back(shape);

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

        auto* character = _physics_system->create_character_virtual(description);
        if (character == nullptr)
        {
            return invalid_character_id;
        }

        const auto key = _characters.insert(character);
        return to_character_id(key);
    }

    auto physics_world::despawn_character(character_id id) -> void
    {
        const auto key = to_slot_map_key(id);
        const auto it = _characters.find(key);
        if (it != _characters.end())
        {
            auto* character = *it;
            if (character != nullptr && _physics_system != nullptr)
            {
                _physics_system->destroy_character_virtual(character);
            }
            _characters.erase(key);
        }
    }

    auto physics_world::get_character(character_id id) const -> jolt::shim::character_virtual*
    {
        const auto key = to_slot_map_key(id);
        const auto it = _characters.find(key);
        if (it != _characters.end())
        {
            return *it;
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
        for (auto* character : _characters)
        {
            if (character != nullptr && _physics_system != nullptr)
            {
                _physics_system->destroy_character_virtual(character);
            }
        }
        _characters.clear();

        if (_physics_system != nullptr)
        {
            for (auto* shape : _allocated_shapes)
            {
                if (shape != nullptr)
                {
                    _physics_system->destroy_shape(shape);
                }
            }
            _allocated_shapes.clear();
        }
    }
} // namespace tempest::physics
