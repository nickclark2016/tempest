#ifndef tempest_ecs_transform_history_component_hpp
#define tempest_ecs_transform_history_component_hpp

#include <tempest/quat.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/vec3.hpp>

namespace tempest::ecs
{
    struct transform_history_component
    {
        math::vec3<float> previous_position;
        math::quat<float> previous_rotation;
        math::vec3<float> current_position;
        math::quat<float> current_rotation;
        math::vec3<float> render_position;
        math::quat<float> render_rotation;

        static constexpr auto create(math::vec3<float> position = {0.0F, 0.0F, 0.0F},
                                     math::quat<float> rotation = {0.0F, 0.0F, 0.0F, 1.0F}) noexcept
            -> transform_history_component
        {
            transform_history_component component;
            component.previous_position = position;
            component.previous_rotation = rotation;
            component.current_position = position;
            component.current_rotation = rotation;
            component.render_position = position;
            component.render_rotation = rotation;
            return component;
        }
    };

    static_assert(is_trivial_v<transform_history_component>);
} // namespace tempest::ecs

#endif // tempest_ecs_transform_history_component_hpp
