#ifndef tempest_ecs_transform_history_system_hpp
#define tempest_ecs_transform_history_system_hpp

#include <tempest/archetype.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/quat.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>

namespace tempest::ecs
{
    inline auto step_transform_history(archetype_registry& registry) -> void
    {
        registry.each([](transform_history_component& hist) {
            hist.previous_position = hist.current_position;
            hist.previous_rotation = hist.current_rotation;
        });
    }

    inline auto interpolate_transform_history(archetype_registry& registry, float alpha) -> void
    {
        registry.each([alpha](transform_history_component& hist) {
            hist.render_position = math::lerp(hist.previous_position, hist.current_position, alpha);
            hist.render_rotation = math::slerp(hist.previous_rotation, hist.current_rotation, alpha);
        });

        registry.each([](const transform_history_component& hist, transform_component& tx) {
            tx.set(hist.render_position, hist.render_rotation);
        });
    }
} // namespace tempest::ecs

#endif // tempest_ecs_transform_history_system_hpp
