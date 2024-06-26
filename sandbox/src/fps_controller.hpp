#ifndef tempest_sandbox_fps_controller_hpp
#define tempest_sandbox_fps_controller_hpp

#include <tempest/keyboard.hpp>
#include <tempest/mouse.hpp>
#include <tempest/mat4.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vec3.hpp>

class fps_controller
{
  public:
    void set_position(const tempest::math::vec3<float>& position);
    void set_rotation(const tempest::math::vec3<float>& rotation);
    void update(const tempest::core::keyboard& kb, const tempest::core::mouse& ms, float dt);

    const tempest::math::mat4<float>& view() const noexcept;
    const tempest::math::mat4<float>& inv_view() const noexcept;

    tempest::math::vec3<float> eye_position() const noexcept;
    tempest::math::vec3<float> eye_direction() const noexcept;
    tempest::math::vec3<float> up_direction() const noexcept;

    tempest::math::vec3<float> eye_rotation() const noexcept;

  private:
    tempest::math::vec3<float> _position_xyz{0, 0, 0};
    tempest::math::vec3<float> _rotation_pyr{0, 0, 0};
    tempest::math::mat4<float> _view;
    tempest::math::mat4<float> _inv_view;

    tempest::math::vec3<float> _forward{0, 0, 1};
    tempest::math::vec3<float> _up{0, 1, 0};
};

#endif // tempest_sandbox_fps_controller_hpp