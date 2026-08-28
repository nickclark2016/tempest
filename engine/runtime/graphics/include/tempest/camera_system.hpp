#ifndef tempest_graphics_camera_system_hpp
#define tempest_graphics_camera_system_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/ecs_events.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/optional.hpp>
#include <tempest/render_camera.hpp>

namespace tempest::graphics
{
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

        /// @brief Query the current active camera entity in the registry.
        /// Returns the explicitly possessed camera if valid, or falls back to the first active viewport camera
        /// with camera_component and transform_component.
        [[nodiscard]] auto get_active_camera_entity() const -> tempest::optional<ecs::entity>;

        /// @brief Constructs and returns the render_camera data (projection, view, inverse matrices, eye position)
        /// for the active camera entity.
        [[nodiscard]] auto get_active_camera() const -> tempest::optional<render_camera>;

        /// @brief Sets the given camera_entity as the explicitly possessed active camera in the registry.
        auto set_active_camera(tempest::optional<ecs::entity> camera_entity) -> void;

        /// @brief Clears the explicitly possessed camera, falling back to scene camera resolution.
        auto clear_active_camera() -> void;

      private:
        ecs::registry* _registry = nullptr;
        mutable tempest::optional<ecs::entity> _active_camera_entity = tempest::nullopt;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_camera_system_hpp
