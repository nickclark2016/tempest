#include <tempest/editor_camera.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/quat.hpp>
#include <tempest/transformations.hpp>

#include <tempest/math_utils.hpp>

namespace tempest::editor
{
    editor_camera::editor_camera() = default;

    auto editor_camera::get_forward_vector() const -> math::vec3<float>
    {
        const auto q = math::quat(math::vec3<float>{_pitch, _yaw, 0.0F});
        return math::extract_forward(q);
    }

    auto editor_camera::get_right_vector() const -> math::vec3<float>
    {
        const auto q = math::quat(math::vec3<float>{_pitch, _yaw, 0.0F});
        return math::extract_right(q);
    }

    auto editor_camera::get_up_vector() const -> math::vec3<float>
    {
        const auto q = math::quat(math::vec3<float>{_pitch, _yaw, 0.0F});
        return math::extract_up(q);
    }

    auto editor_camera::move(float forward_amount, float right_amount, float up_amount) -> void
    {
        if (!tempest::is_finite(forward_amount) || !tempest::is_finite(right_amount) || !tempest::is_finite(up_amount))
        {
            return;
        }

        const auto fwd = get_forward_vector();
        const auto rgt = get_right_vector();
        const auto world_up = math::vec3<float>{0.0F, 1.0F, 0.0F};
        const auto next_pos = _position + fwd * forward_amount + rgt * right_amount + world_up * up_amount;

        if (tempest::is_finite(next_pos.x) && tempest::is_finite(next_pos.y) && tempest::is_finite(next_pos.z))
        {
            _position = next_pos;
        }
    }

    auto editor_camera::rotate(float yaw_delta, float pitch_delta) -> void
    {
        if (!tempest::is_finite(yaw_delta) || !tempest::is_finite(pitch_delta))
        {
            return;
        }

        _yaw += yaw_delta;
        _pitch += pitch_delta;

        constexpr auto max_pitch = math::as_radians(89.0F);
        _pitch = math::clamp(_pitch, -max_pitch, max_pitch);
    }

    auto editor_camera::focus_on(math::vec3<float> target_pos, float distance) -> void
    {
        if (!tempest::is_finite(target_pos.x) || !tempest::is_finite(target_pos.y) ||
            !tempest::is_finite(target_pos.z) || !tempest::is_finite(distance))
        {
            return;
        }

        const auto fwd = get_forward_vector();
        _position = target_pos - fwd * distance;
    }

    auto editor_camera::set_aspect_ratio(float aspect) -> void
    {
        if (tempest::is_finite(aspect) && aspect > 0.0F)
        {
            _aspect_ratio = aspect;
        }
    }

    auto editor_camera::set_vertical_fov(float fov_radians) -> void
    {
        if (tempest::is_finite(fov_radians))
        {
            _vertical_fov = math::clamp(fov_radians, math::as_radians(1.0F), math::as_radians(179.0F));
        }
    }

    auto editor_camera::set_near_plane(float near_plane) -> void
    {
        if (tempest::is_finite(near_plane))
        {
            _near_plane = math::clamp(near_plane, 0.001F, 10000.0F);
        }
    }

    auto editor_camera::set_position(math::vec3<float> pos) -> void
    {
        if (tempest::is_finite(pos.x) && tempest::is_finite(pos.y) && tempest::is_finite(pos.z))
        {
            _position = pos;
        }
    }

    auto editor_camera::set_rotation(float yaw_radians, float pitch_radians) -> void
    {
        if (tempest::is_finite(yaw_radians) && tempest::is_finite(pitch_radians))
        {
            _yaw = yaw_radians;
            constexpr auto max_pitch = math::as_radians(89.0F);
            _pitch = math::clamp(pitch_radians, -max_pitch, max_pitch);
        }
    }

    auto editor_camera::set_move_speed(float speed) -> void
    {
        if (tempest::is_finite(speed))
        {
            _move_speed = math::clamp(speed, 0.01F, 1000.0F);
        }
    }

    auto editor_camera::adjust_move_speed(float delta) -> void
    {
        if (tempest::is_finite(delta))
        {
            _move_speed = math::clamp(_move_speed + delta, 0.1F, 100.0F);
        }
    }

    auto editor_camera::get_render_camera() const -> render_system::render_camera
    {
        const auto q = math::quat(math::vec3<float>{_pitch, _yaw, 0.0F});
        const auto f = math::extract_forward(q);
        const auto u = math::extract_up(q);

        const auto proj = math::perspective(_aspect_ratio, _vertical_fov, _near_plane);
        const auto view = math::look_at(_position, _position + f, u);
        const auto inv_proj = math::inverse(proj);
        const auto inv_view = math::inverse(view);
        const auto eye_pos = math::vec4<float>(_position.x, _position.y, _position.z, 1.0F);

        return render_system::render_camera{
            .proj = proj,
            .inv_proj = inv_proj,
            .view = view,
            .inv_view = inv_view,
            .eye_position = eye_pos,
        };
    }
} // namespace tempest::editor
