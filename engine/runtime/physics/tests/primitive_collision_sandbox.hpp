#ifndef TEMPEST_PHYSICS_TESTS_PRIMITIVE_COLLISION_SANDBOX_HPP
#define TEMPEST_PHYSICS_TESTS_PRIMITIVE_COLLISION_SANDBOX_HPP

#include <tempest/int.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/physics/character_snapshot.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/physics/shim/jolt_shim.hpp>
#include <tempest/quat.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vector.hpp>

namespace tempest::physics::tests
{
    class primitive_collision_sandbox
    {
    public:
        explicit primitive_collision_sandbox(jolt::shim::physics_system* system)
            : _system(system)
        {
            _build_environment();
        }

        primitive_collision_sandbox(const primitive_collision_sandbox&) = delete;
        primitive_collision_sandbox(primitive_collision_sandbox&& other) noexcept
            : _system(other._system),
              _shapes(tempest::move(other._shapes)),
              _bodies(tempest::move(other._bodies)),
              _box_20kg_id(other._box_20kg_id),
              _box_200kg_id(other._box_200kg_id),
              _initial_box_20kg(other._initial_box_20kg),
              _initial_box_200kg(other._initial_box_200kg)
        {
            other._system = nullptr;
            other._box_20kg_id = jolt::shim::invalid_body_id;
            other._box_200kg_id = jolt::shim::invalid_body_id;
        }

        ~primitive_collision_sandbox()
        {
            _teardown();
        }

        auto operator=(const primitive_collision_sandbox&) -> primitive_collision_sandbox& = delete;
        auto operator=(primitive_collision_sandbox&& other) noexcept -> primitive_collision_sandbox&
        {
            if (this != &other)
            {
                _teardown();
                _system = other._system;
                _shapes = tempest::move(other._shapes);
                _bodies = tempest::move(other._bodies);
                _box_20kg_id = other._box_20kg_id;
                _box_200kg_id = other._box_200kg_id;
                _initial_box_20kg = other._initial_box_20kg;
                _initial_box_200kg = other._initial_box_200kg;

                other._system = nullptr;
                other._box_20kg_id = jolt::shim::invalid_body_id;
                other._box_200kg_id = jolt::shim::invalid_body_id;
            }
            return *this;
        }

        [[nodiscard]] auto box_20kg_id() const noexcept -> jolt::shim::body_id
        {
            return _box_20kg_id;
        }

        [[nodiscard]] auto box_200kg_id() const noexcept -> jolt::shim::body_id
        {
            return _box_200kg_id;
        }

        auto reset_dynamic_boxes() -> void
        {
            if (_system == nullptr)
            {
                return;
            }

            if (_box_20kg_id != jolt::shim::invalid_body_id)
            {
                restore_body_snapshot(*_system, _box_20kg_id, _initial_box_20kg);
            }

            if (_box_200kg_id != jolt::shim::invalid_body_id)
            {
                restore_body_snapshot(*_system, _box_200kg_id, _initial_box_200kg);
            }
        }

        [[nodiscard]] auto capture_box_20kg_snapshot(uint32_t tick) const -> actor_motion_snapshot
        {
            return capture_body_snapshot(tick, *_system, _box_20kg_id);
        }

        [[nodiscard]] auto capture_box_200kg_snapshot(uint32_t tick) const -> actor_motion_snapshot
        {
            return capture_body_snapshot(tick, *_system, _box_200kg_id);
        }

    private:
        jolt::shim::physics_system* _system = nullptr;
        tempest::vector<jolt::shim::shape_handle> _shapes{};
        tempest::vector<jolt::shim::body_id> _bodies{};

        jolt::shim::body_id _box_20kg_id = jolt::shim::invalid_body_id;
        jolt::shim::body_id _box_200kg_id = jolt::shim::invalid_body_id;
        actor_motion_snapshot _initial_box_20kg{};
        actor_motion_snapshot _initial_box_200kg{};

        auto _build_environment() -> void
        {
            if (_system == nullptr)
            {
                return;
            }

            // 1. Static Ground Plane (100m x 100m)
            const auto ground_shape = _system->create_box_shape(jolt::shim::vec3{50.0F, 0.5F, 50.0F});
            _shapes.push_back(ground_shape);

            const auto ground_desc = jolt::shim::body_desc{
                .shape = ground_shape,
                .position = jolt::shim::vec3{0.0F, -0.5F, 0.0F},
                .rotation = jolt::shim::quat{0.0F, 0.0F, 0.0F, 1.0F},
                .motion = jolt::shim::motion_type::static_motion,
                .layer = jolt::shim::object_layer::non_moving,
            };
            const auto ground_body = _system->create_body(ground_desc);
            _system->add_body(ground_body, false);
            _bodies.push_back(ground_body);

            // 2. Ramp Incline Sections (30 deg, 45 deg, 60 deg)
            const auto ramp_shape = _system->create_box_shape(jolt::shim::vec3{2.0F, 0.2F, 6.0F});
            _shapes.push_back(ramp_shape);

            // 30-degree ramp: rotated by -30 deg around X
            const auto rot_30 = math::quat<float>(math::vec3<float>{math::as_radians(-30.0F), 0.0F, 0.0F});
            const auto ramp_30_desc = jolt::shim::body_desc{
                .shape = ramp_shape,
                .position = jolt::shim::vec3{-15.0F, 2.5F, 10.0F},
                .rotation = jolt::shim::quat{rot_30.x, rot_30.y, rot_30.z, rot_30.w},
                .motion = jolt::shim::motion_type::static_motion,
                .layer = jolt::shim::object_layer::non_moving,
            };
            const auto ramp_30_body = _system->create_body(ramp_30_desc);
            _system->add_body(ramp_30_body, false);
            _bodies.push_back(ramp_30_body);

            // 45-degree ramp: rotated by -45 deg around X
            const auto rot_45 = math::quat<float>(math::vec3<float>{math::as_radians(-45.0F), 0.0F, 0.0F});
            const auto ramp_45_desc = jolt::shim::body_desc{
                .shape = ramp_shape,
                .position = jolt::shim::vec3{-5.0F, 3.5F, 10.0F},
                .rotation = jolt::shim::quat{rot_45.x, rot_45.y, rot_45.z, rot_45.w},
                .motion = jolt::shim::motion_type::static_motion,
                .layer = jolt::shim::object_layer::non_moving,
            };
            const auto ramp_45_body = _system->create_body(ramp_45_desc);
            _system->add_body(ramp_45_body, false);
            _bodies.push_back(ramp_45_body);

            // 60-degree ramp: rotated by -60 deg around X
            const auto rot_60 = math::quat<float>(math::vec3<float>{math::as_radians(-60.0F), 0.0F, 0.0F});
            const auto ramp_60_desc = jolt::shim::body_desc{
                .shape = ramp_shape,
                .position = jolt::shim::vec3{5.0F, 4.5F, 10.0F},
                .rotation = jolt::shim::quat{rot_60.x, rot_60.y, rot_60.z, rot_60.w},
                .motion = jolt::shim::motion_type::static_motion,
                .layer = jolt::shim::object_layer::non_moving,
            };
            const auto ramp_60_body = _system->create_body(ramp_60_desc);
            _system->add_body(ramp_60_body, false);
            _bodies.push_back(ramp_60_body);

            // 3. Stepped Terrain (0.15m, 0.25m, 0.40m)
            const auto step_015_shape = _system->create_box_shape(jolt::shim::vec3{2.0F, 0.075F, 1.0F});
            _shapes.push_back(step_015_shape);
            const auto step_015_desc = jolt::shim::body_desc{
                .shape = step_015_shape,
                .position = jolt::shim::vec3{15.0F, 0.075F, 1.0F},
                .rotation = jolt::shim::quat{0.0F, 0.0F, 0.0F, 1.0F},
                .motion = jolt::shim::motion_type::static_motion,
                .layer = jolt::shim::object_layer::non_moving,
            };
            const auto step_015_body = _system->create_body(step_015_desc);
            _system->add_body(step_015_body, false);
            _bodies.push_back(step_015_body);

            const auto step_025_shape = _system->create_box_shape(jolt::shim::vec3{2.0F, 0.125F, 1.0F});
            _shapes.push_back(step_025_shape);
            const auto step_025_desc = jolt::shim::body_desc{
                .shape = step_025_shape,
                .position = jolt::shim::vec3{15.0F, 0.125F, 4.0F},
                .rotation = jolt::shim::quat{0.0F, 0.0F, 0.0F, 1.0F},
                .motion = jolt::shim::motion_type::static_motion,
                .layer = jolt::shim::object_layer::non_moving,
            };
            const auto step_025_body = _system->create_body(step_025_desc);
            _system->add_body(step_025_body, false);
            _bodies.push_back(step_025_body);

            const auto step_040_shape = _system->create_box_shape(jolt::shim::vec3{2.0F, 0.200F, 1.0F});
            _shapes.push_back(step_040_shape);
            const auto step_040_desc = jolt::shim::body_desc{
                .shape = step_040_shape,
                .position = jolt::shim::vec3{15.0F, 0.200F, 7.0F},
                .rotation = jolt::shim::quat{0.0F, 0.0F, 0.0F, 1.0F},
                .motion = jolt::shim::motion_type::static_motion,
                .layer = jolt::shim::object_layer::non_moving,
            };
            const auto step_040_body = _system->create_body(step_040_desc);
            _system->add_body(step_040_body, false);
            _bodies.push_back(step_040_body);

            // 4. Dynamic Pushable Boxes (1m^3 box: half extents 0.5m)
            const auto dynamic_box_shape = _system->create_box_shape(jolt::shim::vec3{0.5F, 0.5F, 0.5F});
            _shapes.push_back(dynamic_box_shape);

            // 20 kg dynamic box
            const auto box_20kg_desc = jolt::shim::body_desc{
                .shape = dynamic_box_shape,
                .position = jolt::shim::vec3{0.0F, 0.5F, 2.5F},
                .rotation = jolt::shim::quat{0.0F, 0.0F, 0.0F, 1.0F},
                .motion = jolt::shim::motion_type::dynamic,
                .layer = jolt::shim::object_layer::moving,
                .mass = 20.0F,
            };
            _box_20kg_id = _system->create_body(box_20kg_desc);
            _system->add_body(_box_20kg_id, true);
            _bodies.push_back(_box_20kg_id);
            _initial_box_20kg = capture_body_snapshot(0, *_system, _box_20kg_id);

            // 200 kg dynamic box
            const auto box_200kg_desc = jolt::shim::body_desc{
                .shape = dynamic_box_shape,
                .position = jolt::shim::vec3{5.0F, 0.5F, 2.5F},
                .rotation = jolt::shim::quat{0.0F, 0.0F, 0.0F, 1.0F},
                .motion = jolt::shim::motion_type::dynamic,
                .layer = jolt::shim::object_layer::moving,
                .mass = 200.0F,
            };
            _box_200kg_id = _system->create_body(box_200kg_desc);
            _system->add_body(_box_200kg_id, true);
            _bodies.push_back(_box_200kg_id);
            _initial_box_200kg = capture_body_snapshot(0, *_system, _box_200kg_id);
        }

        auto _teardown() -> void
        {
            if (_system == nullptr)
            {
                return;
            }

            for (const auto body : _bodies)
            {
                _system->remove_body(body);
                _system->destroy_body(body);
            }
            _bodies.clear();

            for (const auto shape : _shapes)
            {
                _system->destroy_shape(shape);
            }
            _shapes.clear();

            _system = nullptr;
        }
    };
} // namespace tempest::physics::tests

#endif // TEMPEST_PHYSICS_TESTS_PRIMITIVE_COLLISION_SANDBOX_HPP
