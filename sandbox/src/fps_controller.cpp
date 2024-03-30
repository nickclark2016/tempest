#include "fps_controller.hpp"

#include <iostream>
#include <numbers>

void fps_controller::set_position(const tempest::math::vec3<float>& position)
{
    _position_xyz = position;
}

void fps_controller::set_rotation(const tempest::math::vec3<float>& rotation)
{
    _rotation_pyr = tempest::math::vec3<float>(tempest::math::as_radians(rotation.x),
                                               tempest::math::as_radians(rotation.y),
                                               tempest::math::as_radians(rotation.z));
}

void fps_controller::update(const tempest::core::keyboard& kb, const tempest::core::mouse& ms, float dt)
{
    if (!ms.is_disabled())
    {
        return;
    }

    // compute forward, right, up vectors
    float pitch = _rotation_pyr.x;
    float yaw = -_rotation_pyr.y;

    tempest::math::vec3<float> move_dir{};
    tempest::math::vec3<float> rot_dir{};

    tempest::math::vec3<float> up{0.0f, 1.0f, 0.0f};
    tempest::math::vec3<float> forward(std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                                       std::sin(yaw) * std::cos(pitch));
    tempest::math::vec3<float> right = tempest::math::cross(forward, up);

    if (kb.is_key_down(tempest::core::key::W))
    {
        move_dir += forward;
    }

    if (kb.is_key_down(tempest::core::key::S))
    {
        move_dir -= forward;
    }

    if (kb.is_key_down(tempest::core::key::D))
    {
        move_dir += right;
    }

    if (kb.is_key_down(tempest::core::key::A))
    {
        move_dir -= right;
    }

    if (kb.is_key_down(tempest::core::key::LEFT_SHIFT))
    {
        move_dir *= 3.0f;
    }

    float rot_speed = 360.0f / (std::numbers::pi_v<float> * 45.0f);

    rot_dir.y = -(rot_speed * ms.dx());
    rot_dir.x = -(rot_speed * ms.dy());

    _position_xyz += move_dir * dt;
    _rotation_pyr += rot_dir * dt;

    _forward = forward;
    _up = up;

    _rotation_pyr.x =
        tempest::math::clamp(_rotation_pyr.x, tempest::math::as_radians(-89.0f), tempest::math::as_radians(89.0f));

    tempest::math::mat4<float> view = tempest::math::look_at(_position_xyz, _position_xyz + forward, up);

    _view = view;
    _inv_view = tempest::math::inverse(_view);
}

const tempest::math::mat4<float>& fps_controller::view() const noexcept
{
    return _view;
}

const tempest::math::mat4<float>& fps_controller::inv_view() const noexcept
{
    return _inv_view;
}

tempest::math::vec3<float> fps_controller::eye_position() const noexcept
{
    return _position_xyz;
}

tempest::math::vec3<float> fps_controller::eye_direction() const noexcept
{
    return _forward;
}

tempest::math::vec3<float> fps_controller::up_direction() const noexcept
{
    return _up;
}
