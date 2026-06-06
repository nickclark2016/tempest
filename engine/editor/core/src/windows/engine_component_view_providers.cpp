#include <tempest/windows/engine_component_view_providers.hpp>

#include <tempest/graphics_components.hpp>
#include <tempest/ui.hpp>

#include <imgui.h>

namespace tempest::editor
{
    auto transform_component_view_provider::draw(ecs::archetype_registry* registry, ecs::entity target) -> void
    {
        const auto* const transform = registry->try_get<ecs::transform_component>(target);
        if (transform == nullptr)
        {
            return;
        }

        if (ImGui::CollapsingHeader("Transform Component"))
        {
            const auto new_position = ui::float3("Position", transform->position());
            const auto new_rotation = math::as_radians(ui::float3("Rotation", math::as_degrees(transform->rotation())));
            const auto new_scale = ui::float3("Scale", transform->scale());

            const auto changed = new_position != transform->position() || new_rotation != transform->rotation() ||
                                 new_scale != transform->scale();

            if (changed)
            {
                auto new_transform = ecs::transform_component::identity();
                new_transform.position(new_position);
                new_transform.rotation(new_rotation);
                new_transform.scale(new_scale);

                registry->replace(target, new_transform);
            }
        }
    }

    auto directional_light_component_view_provider::draw(ecs::archetype_registry* registry, ecs::entity target) -> void
    {
        const auto* const existing_dir_light = registry->try_get<graphics::directional_light_component>(target);
        if (existing_dir_light == nullptr)
        {
            return;
        }

        if (ImGui::CollapsingHeader("Directional Light Component"))
        {
            const auto new_color = ui::color3("Color", existing_dir_light->color);
            const auto new_intensity = ui::scalar("Intensity", existing_dir_light->intensity);

            const auto changed =
                new_color != existing_dir_light->color || new_intensity != existing_dir_light->intensity;

            if (changed)
            {
                auto new_dir_light = graphics::directional_light_component{
                    .color = new_color,
                    .intensity = new_intensity,
                };

                registry->replace(target, new_dir_light);
            }
        }
    }
} // namespace tempest::editor