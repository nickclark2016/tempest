#include <tempest/windows/engine_component_view_providers.hpp>

#include <tempest/editor.hpp>
#include <tempest/limits.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/transform_component.hpp>
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
        const auto* const existing_camera = registry->try_get<render_system::camera_component>(target);
        if (existing_camera == nullptr)
        {
            return;
        }

        if (ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool is_active = registry->has<render_system::active_camera_component>(target);
            if (ImGui::Checkbox("Is Active Camera", &is_active))
            {
                if (is_active)
                {
                    registry->assign(target, render_system::active_camera_component{});
                }
                else
                {
                    registry->remove<render_system::active_camera_component>(target);
                }
            }

            const auto new_fov = math::as_radians(ui::drag_scalar(
                "Field of View (Vertical)", math::as_degrees(existing_camera->vertical_fov), 0.0F, 180.0F));
            const auto new_near_plane = ui::drag_scalar("Near Plane", existing_camera->near_plane, 0.001F, 10000.0F);

            const auto changed =
                new_fov != existing_camera->vertical_fov || new_near_plane != existing_camera->near_plane;

            if (changed)
            {
                const auto cam = render_system::camera_component{
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
        registry->assign(target, render_system::camera_component{
                                     .aspect_ratio = 16.0F / 9.0F,
                                     .vertical_fov = math::as_radians(60.0F),
                                     .near_plane = 0.1F,
                                 });
    }

    auto active_camera_component_view_provider::draw(ecs::archetype_registry* registry, ecs::entity target) -> void
    {
        const auto* const active_cam = registry->try_get<render_system::active_camera_component>(target);
        if (active_cam == nullptr)
        {
            return;
        }

        if (ImGui::CollapsingHeader("Active Camera Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool is_active = true;
            if (ImGui::Checkbox("Is Active Camera##ActiveTag", &is_active))
            {
                if (!is_active)
                {
                    registry->remove<render_system::active_camera_component>(target);
                }
            }
        }
    }

    auto active_camera_component_view_provider::create_default(ecs::archetype_registry* registry, ecs::entity target)
        -> void
    {
        registry->assign(target, render_system::active_camera_component{});
    }

    auto directional_light_component_view_provider::draw(ecs::archetype_registry* registry, ecs::entity target) -> void
    {
        const auto* const existing_dir_light = registry->try_get<render_system::directional_light_component>(target);
        if (existing_dir_light == nullptr)
        {
            return;
        }

        if (ImGui::CollapsingHeader("Directional Light Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto new_color = ui::color3("Color", existing_dir_light->color);
            const auto new_intensity = ui::drag_scalar("Intensity", existing_dir_light->intensity, 0.0F, 100000.0F);

            const auto changed =
                new_color != existing_dir_light->color || new_intensity != existing_dir_light->intensity;

            if (changed)
            {
                auto new_dir_light = render_system::directional_light_component{
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
        registry->assign(target, render_system::directional_light_component{
                                     .color = math::float3{1.0F, 1.0F, 1.0F},
                                     .intensity = 1.0F,
                                 });
    }

    auto point_light_component_view_provider::draw(ecs::archetype_registry* registry, ecs::entity target) -> void
    {
        const auto* const existing_pt_light = registry->try_get<render_system::point_light_component>(target);
        if (existing_pt_light == nullptr)
        {
            return;
        }

        if (ImGui::CollapsingHeader("Point Light Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto new_color = ui::color3("Color", existing_pt_light->color);
            const auto new_intensity = ui::drag_scalar("Intensity", existing_pt_light->intensity, 0.0F, 100000.0F);
            const auto new_range = ui::drag_scalar("Range", existing_pt_light->range, 0.1F, 10000.0F);

            const auto changed = new_color != existing_pt_light->color ||
                                 new_intensity != existing_pt_light->intensity || new_range != existing_pt_light->range;

            if (changed)
            {
                auto new_pt_light = render_system::point_light_component{
                    .color = new_color,
                    .intensity = new_intensity,
                    .range = new_range,
                };

                registry->replace(target, new_pt_light);
            }
        }
    }

    auto point_light_component_view_provider::create_default(ecs::archetype_registry* registry, ecs::entity target)
        -> void
    {
        registry->assign(target, render_system::point_light_component{
                                     .color = math::float3{1.0F, 1.0F, 1.0F},
                                     .intensity = 10.0F,
                                     .range = 10.0F,
                                 });
    }

    auto shadow_caster_component_view_provider::draw(ecs::archetype_registry* registry, ecs::entity target) -> void
    {
        const auto* const existing_shadow_caster = registry->try_get<render_system::shadow_caster_component>(target);
        if (existing_shadow_caster == nullptr)
        {
            return;
        }

        if (ImGui::CollapsingHeader("Shadow Caster Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            constexpr const char* res_names[] = {"512", "1024", "2048", "4096"};
            constexpr uint32_t res_values[] = {512, 1024, 2048, 4096};
            auto current_res_idx = 2; // Default 2048
            for (auto i = 0; i < 4; ++i)
            {
                if (existing_shadow_caster->resolution == res_values[i])
                {
                    current_res_idx = i;
                    break;
                }
            }

            auto new_resolution = existing_shadow_caster->resolution;
            if (ImGui::Combo("Resolution", &current_res_idx, res_names, 4))
            {
                new_resolution = res_values[current_res_idx];
            }

            const auto new_cascade_count =
                ui::drag_integral("Cascades", static_cast<int>(existing_shadow_caster->num_cascades), 1, 4);
            const auto new_split_lambda =
                ui::drag_scalar("Split Lambda", existing_shadow_caster->split_lambda, 0.0F, 1.0F);
            const auto new_shadow_distance =
                ui::drag_scalar("Max Shadow Distance", existing_shadow_caster->max_shadow_distance, 1.0F, 5000.0F);
            const auto new_normal_bias =
                ui::drag_scalar("Normal Bias", existing_shadow_caster->normal_bias, 0.0F, 1.0F);
            const auto new_depth_bias = ui::drag_scalar("Depth Bias", existing_shadow_caster->depth_bias, 0.0F, 1.0F);

            constexpr const char* debug_names[] = {"None", "Cascades", "Shadow Factor", "Cascade & Shadow",
                                                   "Scene Cascade Tint"};
            auto current_debug_idx = static_cast<int>(existing_shadow_caster->debug_mode);
            if (current_debug_idx < 0 || current_debug_idx > 4)
            {
                current_debug_idx = 0;
            }

            auto new_debug_mode = existing_shadow_caster->debug_mode;
            if (ImGui::Combo("Debug Mode", &current_debug_idx, debug_names, 5))
            {
                new_debug_mode = static_cast<render_system::shadow_debug_mode>(current_debug_idx);
            }

            const auto changed = new_resolution != existing_shadow_caster->resolution ||
                                 new_cascade_count != static_cast<int>(existing_shadow_caster->num_cascades) ||
                                 new_split_lambda != existing_shadow_caster->split_lambda ||
                                 new_shadow_distance != existing_shadow_caster->max_shadow_distance ||
                                 new_normal_bias != existing_shadow_caster->normal_bias ||
                                 new_depth_bias != existing_shadow_caster->depth_bias ||
                                 new_debug_mode != existing_shadow_caster->debug_mode;

            if (changed)
            {
                auto new_shadow_caster = render_system::shadow_caster_component{
                    .resolution = new_resolution,
                    .num_cascades = static_cast<uint32_t>(new_cascade_count),
                    .split_lambda = new_split_lambda,
                    .max_shadow_distance = new_shadow_distance,
                    .normal_bias = new_normal_bias,
                    .depth_bias = new_depth_bias,
                    .priority = existing_shadow_caster->priority,
                    .debug_mode = new_debug_mode,
                };

                registry->replace(target, new_shadow_caster);
            }
        }
    }

    auto shadow_caster_component_view_provider::create_default(ecs::archetype_registry* registry, ecs::entity target)
        -> void
    {
        registry->assign(target, render_system::shadow_caster_component{});
    }

    auto register_engine_component_view_providers(editor_context& ctx) -> void
    {
        ctx.register_component_view_provider(make_unique<transform_component_view_provider>());
        ctx.register_component_view_provider(make_unique<camera_component_view_provider>());
        ctx.register_component_view_provider(make_unique<active_camera_component_view_provider>());
        ctx.register_component_view_provider(make_unique<directional_light_component_view_provider>());
        ctx.register_component_view_provider(make_unique<point_light_component_view_provider>());
        ctx.register_component_view_provider(make_unique<shadow_caster_component_view_provider>());
    }
} // namespace tempest::editor