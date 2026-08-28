#ifndef tempest_editor_core_engine_component_view_providers_hpp
#define tempest_editor_core_engine_component_view_providers_hpp

#include <tempest/windows/component_view.hpp>

namespace tempest::editor
{
    class TEMPEST_EDITOR_API transform_component_view_provider final : public component_view_provider
    {
      public:
        transform_component_view_provider() = default;

        auto draw(ecs::archetype_registry* registry, ecs::entity target) -> void override;

        auto name() const -> cstring_view override
        {
            return "Transform Component";
        }

        auto create_default(ecs::archetype_registry* registry, ecs::entity target) -> void override;
    };

    class TEMPEST_EDITOR_API camera_component_view_provider final : public component_view_provider
    {
      public:
        camera_component_view_provider() = default;

        auto draw(ecs::archetype_registry* registry, ecs::entity target) -> void override;

        auto name() const -> cstring_view override
        {
            return "Camera Component";
        }

        auto create_default(ecs::archetype_registry* registry, ecs::entity target) -> void override;
    };

    class TEMPEST_EDITOR_API directional_light_component_view_provider final : public component_view_provider
    {
      public:
        directional_light_component_view_provider() = default;

        auto draw(ecs::archetype_registry* registry, ecs::entity target) -> void override;

        auto name() const -> cstring_view override
        {
            return "Directional Light Component";
        }

        auto create_default(ecs::archetype_registry* registry, ecs::entity target) -> void override;
    };

    class TEMPEST_EDITOR_API point_light_component_view_provider final : public component_view_provider
    {
      public:
        point_light_component_view_provider() = default;

        auto draw(ecs::archetype_registry* registry, ecs::entity target) -> void override;

        auto name() const -> cstring_view override
        {
            return "Point Light Component";
        }

        auto create_default(ecs::archetype_registry* registry, ecs::entity target) -> void override;
    };

    class TEMPEST_EDITOR_API shadow_caster_component_view_provider final : public component_view_provider
    {
      public:
        shadow_caster_component_view_provider() = default;

        auto draw(ecs::archetype_registry* registry, ecs::entity target) -> void override;

        auto name() const -> cstring_view override
        {
            return "Shadow Caster Component";
        }

        auto create_default(ecs::archetype_registry* registry, ecs::entity target) -> void override;
    };

    class editor_context;
    auto register_engine_component_view_providers(editor_context& ctx) -> void;
} // namespace tempest::editor

#endif // tempest_editor_core_engine_component_view_providers_hpp