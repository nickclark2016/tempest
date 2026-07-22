#include <tempest/windows/engine_component_view_providers.hpp>

#include <tempest/editor.hpp>
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

        if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen))
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

    auto transform_component_view_provider::create_default(ecs::archetype_registry* registry, ecs::entity target)
        -> void
    {
        registry->assign(target, ecs::transform_component::identity());
    }

    auto camera_component_view_provider::draw(ecs::archetype_registry* registry, ecs::entity target) -> void
    {
        const auto* const existing_camera = registry->try_get<graphics::camera_component>(target);
        if (existing_camera == nullptr)
        {
            return;
        }

        if (ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto new_fov = math::as_radians(ui::drag_scalar(
                "Field of View (Vertical)", math::as_degrees(existing_camera->vertical_fov), 0.0F, 180.0F));
            const auto new_near_plane =
                ui::drag_scalar("Near Plane", existing_camera->near_plane, 0.0F, numeric_limits<float>::max());

            const auto changed =
                new_fov != existing_camera->vertical_fov || new_near_plane != existing_camera->near_plane;

            if (changed)
            {
                const auto cam = graphics::camera_component{
                    .aspect_ratio = existing_camera->aspect_ratio,
                    .vertical_fov = new_fov,
                    .near_plane = new_near_plane,
                };

                registry->replace(target, cam);
            }
        }
    }

    auto camera_component_view_provider::create_default(ecs::archetype_registry* registry, ecs::entity target) -> void
    {
        registry->assign(target, graphics::camera_component{});
    }

    auto directional_light_component_view_provider::draw(ecs::archetype_registry* registry, ecs::entity target) -> void
    {
        const auto* const existing_dir_light = registry->try_get<graphics::directional_light_component>(target);
        if (existing_dir_light == nullptr)
        {
            return;
        }

        if (ImGui::CollapsingHeader("Directional Light Component", ImGuiTreeNodeFlags_DefaultOpen))
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

    auto directional_light_component_view_provider::create_default(ecs::archetype_registry* registry,
                                                                   ecs::entity target) -> void
    {
        registry->assign(target, graphics::directional_light_component{});
    }

    auto shadow_map_component_view_provider::draw(ecs::archetype_registry* registry, ecs::entity target) -> void
    {
        const auto* const existing_shadow_map_component = registry->try_get<graphics::shadow_map_component>(target);
        if (existing_shadow_map_component == nullptr)
        {
            return;
        }

        if (ImGui::CollapsingHeader("Shadow Map Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto new_cascade_count =
                ui::drag_integral("Cascades", static_cast<int>(existing_shadow_map_component->cascade_count), 1, 4);
            const auto new_split_lambda =
                ui::drag_scalar("Split Lambda", existing_shadow_map_component->split_lambda, 0.0F, 1.0F);
            const auto new_blend_fraction =
                ui::drag_scalar("Blend Fraction", existing_shadow_map_component->blend_fraction, 0.0F, 1.0F);
            const auto new_shadow_distance =
                ui::drag_scalar("Shadow Distance", existing_shadow_map_component->shadow_distance, 1.0F, 5000.0F);

            const auto changed = new_cascade_count != static_cast<int>(existing_shadow_map_component->cascade_count) ||
                                 new_split_lambda != existing_shadow_map_component->split_lambda ||
                                 new_blend_fraction != existing_shadow_map_component->blend_fraction ||
                                 new_shadow_distance != existing_shadow_map_component->shadow_distance;

            if (changed)
            {
                auto new_shadow_map = graphics::shadow_map_component{
                    .shadow_distance = new_shadow_distance,
                    .split_lambda = new_split_lambda,
                    .blend_fraction = new_blend_fraction,
                    .cascade_count = static_cast<uint32_t>(new_cascade_count),
                };

                registry->replace(target, new_shadow_map);
            }
        }
    }

    auto shadow_map_component_view_provider::create_default(ecs::archetype_registry* registry, ecs::entity target)
        -> void
    {
        registry->assign(target, graphics::shadow_map_component{});
    }

    auto register_engine_component_view_providers(editor_context& ctx) -> void
    {
        ctx.register_component_view_provider(make_unique<transform_component_view_provider>());
        ctx.register_component_view_provider(make_unique<camera_component_view_provider>());
        ctx.register_component_view_provider(make_unique<directional_light_component_view_provider>());
        ctx.register_component_view_provider(make_unique<shadow_map_component_view_provider>());
    }
} // namespace tempest::editor