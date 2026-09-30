#ifndef tempest_ecs_transform_component_hpp
#define tempest_ecs_transform_component_hpp

#include <tempest/api.hpp>
#include <tempest/mat4.hpp>
#include <tempest/traits.hpp>
#include <tempest/transformations.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/vec3.hpp>

namespace tempest::ecs
{
    class TEMPEST_API transform_component
    {
      public:
        [[nodiscard]] auto position() const noexcept -> math::vec3<float>
        {
            return _position;
        }

        auto position(math::vec3<float> pos) -> void
        {
            _position = pos;
            _build_transform();
        }

        [[nodiscard]] auto rotation() const noexcept -> math::vec3<float>
        {
            return math::euler(_rotation);
        }

        auto rotation(math::vec3<float> rot) -> void
        {
            _rotation = math::quat<float>(rot);
            _build_transform();
        }

        [[nodiscard]] auto rotation_quat() const noexcept -> math::quat<float>
        {
            return _rotation;
        }

        auto rotation(math::quat<float> rot) -> void
        {
            _rotation = rot;
            _build_transform();
        }

        [[nodiscard]] auto scale() const noexcept -> math::vec3<float>
        {
            return _scale;
        }

        auto scale(math::vec3<float> sca) -> void
        {
            _scale = sca;
            _build_transform();
        }

        [[nodiscard]] auto matrix() const noexcept -> math::mat4<float>
        {
            return _transform;
        }

        auto set(math::vec3<float> position, math::quat<float> rotation) -> void
        {
            _position = position;
            _rotation = rotation;
            _build_transform();
        }

      private:
        math::vec3<float> _position = math::vec3<float>(0.0F);
        math::quat<float> _rotation = math::quat<float>(0.0F, 0.0F, 0.0F, 1.0F);
        math::vec3<float> _scale = math::vec3<float>(1.0F);
        math::mat4<float> _transform = math::mat4<float>(1.0F);

        void _build_transform()
        {
            _transform = math::transform(_position, _rotation, _scale);
        }
    };

    static_assert(component<transform_component>);
} // namespace tempest::ecs

#endif // tempest_ecs_transform_component_hpp