#ifndef tempest_editor_core_editor_camera_hpp
#define tempest_editor_core_editor_camera_hpp

#include <tempest/mat4.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/vec3.hpp>

namespace tempest::editor
{
    class TEMPEST_EDITOR_API editor_camera
    {
      public:
        editor_camera();
        ~editor_camera() = default;

        editor_camera(const editor_camera&) = default;
        editor_camera(editor_camera&&) noexcept = default;
        auto operator=(const editor_camera&) -> editor_camera& = default;
        auto operator=(editor_camera&&) noexcept -> editor_camera& = default;

        /// @brief Constructs the render_camera struct containing projection, view, and inverse matrices.
        [[nodiscard]] auto get_render_camera() const -> render_system::render_camera;

        /// @brief Moves the camera relative to its current orientation along forward, right, and world up axes.
        auto move(float forward_amount, float right_amount, float up_amount) -> void;

        /// @brief Rotates the camera by the specified yaw and pitch deltas (in radians).
        auto rotate(float yaw_delta, float pitch_delta) -> void;

        /// @brief Focuses the camera on a target world position from a specified distance.
        auto focus_on(math::vec3<float> target_pos, float distance = 5.0F) -> void;

        /// @brief Sets the viewport aspect ratio.
        auto set_aspect_ratio(float aspect) -> void;

        /// @brief Sets the camera's field of view in radians.
        auto set_vertical_fov(float fov_radians) -> void;

        /// @brief Sets the camera near plane distance.
        auto set_near_plane(float near_plane) -> void;

        /// @brief Sets the camera world position.
        auto set_position(math::vec3<float> pos) -> void;

        /// @brief Sets the camera yaw and pitch angles in radians.
        auto set_rotation(float yaw_radians, float pitch_radians) -> void;

        /// @brief Sets the camera movement speed in units/second.
        auto set_move_speed(float speed) -> void;

        /// @brief Adjusts the move speed by a delta factor (e.g. from scroll wheel).
        auto adjust_move_speed(float delta) -> void;

        [[nodiscard]] auto get_position() const noexcept -> math::vec3<float>
        {
            return _position;
        }

        [[nodiscard]] auto get_yaw() const noexcept -> float
        {
            return _yaw;
        }

        [[nodiscard]] auto get_pitch() const noexcept -> float
        {
            return _pitch;
        }

        [[nodiscard]] auto get_aspect_ratio() const noexcept -> float
        {
            return _aspect_ratio;
        }

        [[nodiscard]] auto get_vertical_fov() const noexcept -> float
        {
            return _vertical_fov;
        }

        [[nodiscard]] auto get_near_plane() const noexcept -> float
        {
            return _near_plane;
        }

        [[nodiscard]] auto get_move_speed() const noexcept -> float
        {
            return _move_speed;
        }

        [[nodiscard]] auto get_forward_vector() const -> math::vec3<float>;
        [[nodiscard]] auto get_right_vector() const -> math::vec3<float>;
        [[nodiscard]] auto get_up_vector() const -> math::vec3<float>;

      private:
        math::vec3<float> _position{0.0F, 2.0F, -5.0F};
        float _yaw{0.0F};
        float _pitch{0.0F};
        float _aspect_ratio{16.0F / 9.0F};
        float _vertical_fov{1.04719755F}; // 60 degrees in radians
        float _near_plane{0.1F};
        float _move_speed{5.0F};
    };
} // namespace tempest::editor

#endif // tempest_editor_core_editor_camera_hpp
