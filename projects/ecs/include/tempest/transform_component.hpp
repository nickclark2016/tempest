#ifndef tempest_ecs_transform_component_hpp
#define tempest_ecs_transform_component_hpp

#include <tempest/mat4.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vec3.hpp>

namespace tempest::ecs
{
    class transform_component
    {
      public:
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
        math::vec3<float> _position{0.0f};
        math::vec3<float> _rotation{0.0f};
        math::vec3<float> _scale{1.0f};
        math::mat4<float> _transform{1.0f};

        void _build_transform()
        {
            _transform = math::transform(_position, _rotation, _scale);
        }
    };

    static_assert(is_trivial<math::mat4<float>>::value);
} // namespace tempest::ecs

#endif // tempest_ecs_transform_component_hpp