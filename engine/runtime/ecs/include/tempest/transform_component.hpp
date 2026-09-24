#ifndef tempest_ecs_transform_component_hpp
#define tempest_ecs_transform_component_hpp

#include <tempest/api.hpp>
#include <tempest/mat4.hpp>
#include <tempest/transformations.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/vec3.hpp>

namespace tempest::ecs
{
    class TEMPEST_API transform_component
    {
      public:
        static constexpr transform_component identity() noexcept
        {
            transform_component tx;
            tx._position = math::vec3<float>(0.0f);
            tx._rotation = math::quat<float>(0.0f, 0.0f, 0.0f, 1.0f);
            tx._scale = math::vec3<float>(1.0f);
            tx._transform = math::mat4<float>(1.0f);

            return tx;
        }

        math::vec3<float> position() const noexcept
        {
            return _position;
        }

        void position(math::vec3<float> t)
        {
            _position = t;
            _build_transform();
        }

        math::vec3<float> rotation() const noexcept
        {
            return math::euler(_rotation);
        }

        void rotation(math::vec3<float> r)
        {
            _rotation = math::quat<float>(r);
            _build_transform();
        }

        math::quat<float> rotation_quat() const noexcept
        {
            return _rotation;
        }

        void rotation(math::quat<float> q)
        {
            _rotation = q;
            _build_transform();
        }

        math::vec3<float> scale() const noexcept
        {
            return _scale;
        }

        void scale(math::vec3<float> s)
        {
            _scale = s;
            _build_transform();
        }

        math::mat4<float> matrix() const noexcept
        {
            return _transform;
        }

        void set(math::vec3<float> position, math::quat<float> rotation)
        {
            _position = position;
            _rotation = rotation;
            _build_transform();
        }

      private:
        math::vec3<float> _position;
        math::quat<float> _rotation;
        math::vec3<float> _scale;
        math::mat4<float> _transform;

        void _build_transform()
        {
            _transform = math::transform(_position, _rotation, _scale);
        }
    };

    static_assert(is_trivial_v<transform_component>);
    static_assert(is_trivial_v<math::mat4<float>>);
} // namespace tempest::ecs

#endif // tempest_ecs_transform_component_hpp