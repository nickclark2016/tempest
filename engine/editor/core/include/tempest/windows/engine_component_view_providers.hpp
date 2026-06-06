#include <tempest/windows/component_view.hpp>

namespace tempest::editor
{
    class TEMPEST_EDITOR_API transform_component_view_provider final : public component_view_provider
    {
      public:
        transform_component_view_provider() = default;

        auto draw(ecs::archetype_registry* registry, ecs::entity target) -> void override;
    };

    class TEMPEST_EDITOR_API directional_light_component_view_provider final : public component_view_provider
    {
      public:
        directional_light_component_view_provider() = default;

        auto draw(ecs::archetype_registry* registry, ecs::entity target) -> void override;
    };

    class TEMPEST_EDITOR_API shadow_map_component_view_provider final : public component_view_provider
    {
      public:
        shadow_map_component_view_provider() = default;

        auto draw(ecs::archetype_registry* registry, ecs::entity target) -> void override;
    };
}