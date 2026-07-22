#ifndef tempest_editor_core_component_view_hpp
#define tempest_editor_core_component_view_hpp

#include <tempest/archetype.hpp>
#include <tempest/cstring_view.hpp>
#include <tempest/transform_component.hpp>

namespace tempest::editor
{
    class TEMPEST_EDITOR_API component_view_provider
    {
      public:
        component_view_provider(const component_view_provider&) = delete;
        component_view_provider(component_view_provider&&) noexcept = delete;
        virtual ~component_view_provider() = default;

        component_view_provider& operator=(const component_view_provider&) = delete;
        component_view_provider& operator=(component_view_provider&&) noexcept = delete;

        virtual auto draw(ecs::archetype_registry* registry, ecs::entity target) -> void = 0;
        virtual auto name() const -> cstring_view = 0;
        virtual auto create_default(ecs::archetype_registry* registry, ecs::entity target) -> void = 0;

      protected:
        component_view_provider() = default;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_component_view_hpp
