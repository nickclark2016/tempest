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
        ~camera_system();

        camera_system(const camera_system&) = delete;
        camera_system(camera_system&&) noexcept = delete;
        auto operator=(const camera_system&) -> camera_system& = delete;
        auto operator=(camera_system&&) noexcept -> camera_system& = delete;

        /// @brief Query the current active camera entity in the registry.
        /// Returns the entity tagged with active_camera_component, or falls back to the first entity
        /// with camera_component and transform_component if no entity is tagged.
        [[nodiscard]] auto get_active_camera_entity() const -> tempest::optional<ecs::entity>;

        /// @brief Constructs and returns the render_camera data (projection, view, inverse matrices, eye position)
        /// for the active camera entity.
        [[nodiscard]] auto get_active_camera() const -> tempest::optional<render_camera>;

        /// @brief Sets the given camera_entity as the active camera in the registry, assigning active_camera_component
        /// and removing it from any other camera entity.
        auto set_active_camera(ecs::entity camera_entity) -> void;

      private:
        ecs::registry* _registry = nullptr;
        event::event_registry* _events = nullptr;
        event::subscription_handle<ecs::component_added_event<ecs::entity, active_camera_component>> _subscription_handle{};
        // TODO: Investigate mutexes/atomicity to make active camera entity tracking thread-safe across concurrent threads.
        mutable tempest::optional<ecs::entity> _active_camera_entity = tempest::nullopt;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_camera_system_hpp
