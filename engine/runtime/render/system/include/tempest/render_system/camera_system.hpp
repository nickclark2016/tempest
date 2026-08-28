#ifndef tempest_render_system_camera_system_hpp
#define tempest_render_system_camera_system_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/ecs_events.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/mat4.hpp>
#include <tempest/optional.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/vec4.hpp>

namespace tempest::render_system
{
    struct TEMPEST_API render_camera
    {
        math::mat4<float> proj;
        math::mat4<float> inv_proj;
        math::mat4<float> view;
        math::mat4<float> inv_view;
        math::vec4<float> eye_position;
    };

    class TEMPEST_API camera_system
    {
      public:
        explicit camera_system(ecs::registry& registry, event::event_registry& events);
        explicit camera_system(ecs::registry& registry);
        ~camera_system() = default;

        camera_system(const camera_system&) = delete;
        camera_system(camera_system&&) noexcept = delete;
        auto operator=(const camera_system&) -> camera_system& = delete;
        auto operator=(camera_system&&) noexcept -> camera_system& = delete;

        [[nodiscard]] auto get_active_camera_entity() const -> tempest::optional<ecs::entity>;
        [[nodiscard]] auto get_active_camera() const -> tempest::optional<render_camera>;
        auto set_active_camera(tempest::optional<ecs::entity> camera_entity) -> void;
        auto clear_active_camera() -> void;

      private:
        ecs::registry* _registry{nullptr};
        mutable tempest::optional<ecs::entity> _active_camera_entity{tempest::nullopt};
    };
} // namespace tempest::render_system

#endif // tempest_render_system_camera_system_hpp
