#ifndef tempest_ecs_transform_component_hpp
#define tempest_ecs_transform_component_hpp

#include <tempest/mat4.hpp>
#include <tempest/transformations.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/vec3.hpp>

namespace tempest::ecs
{
    class transform_component
    {
      public:
        static constexpr transform_component identity() noexcept
        {
            transform_component tx;
            tx._position = math::vec3<float>(0.0f);
            tx._rotation = math::vec3<float>(0.0f);
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
            return _rotation;
        }

        void rotation(math::vec3<float> r)
        {
            _rotation = r;
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

      private:
        math::vec3<float> _position;
        math::vec3<float> _rotation;
        math::vec3<float> _scale;
        math::mat4<float> _transform;

        void _build_transform()
        {
            _transform = math::transform(_position, _rotation, _scale);
        }
    };

    static_assert(is_trivial<math::mat4<float>>::value);
} // namespace tempest::ecs

#endif // tempest_ecs_transform_component_hpp