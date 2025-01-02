#ifndef tempest_editor_lighting_component_view_hpp
#define tempest_editor_lighting_component_view_hpp

#include <tempest/component_view.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/imgui_context.hpp>

namespace tempest::editor
{
    struct directional_lighting_component_view : component_view_factory
    {
        bool create_view(ecs::registry& registry, ecs::entity ent) const override
        {
            if (auto dir_light = registry.try_get<graphics::directional_light_component>(ent))
            {
                using imgui = graphics::imgui_context;

                auto color = dir_light->color;
                auto intensity = dir_light->intensity;

                imgui::create_header("Directional Light Component", [&]() {
                    imgui::label("Color");
                    color = imgui::input_color("Color", color);

                    imgui::label("Intensity");
                    intensity = imgui::float_slider("Intensity", 0, 1, intensity);
                });

                if (color != dir_light->color || intensity != dir_light->intensity)
                {
                    dir_light->color = color;
                    dir_light->intensity = intensity;

                    return true;
                }
            }

            return false;
        }
    };

    struct point_light_component_view : component_view_factory
    {
        bool create_view(ecs::registry& registry, ecs::entity ent) const override
        {
            if (auto point_light = registry.try_get<graphics::point_light_component>(ent))
            {
                using imgui = graphics::imgui_context;

                auto color = point_light->color;
                auto intensity = point_light->intensity;
                auto range = point_light->range;

                imgui::create_header("Point Light Component", [&]() {
                    imgui::label("Color");
                    color = imgui::input_color("Color", color);

                    imgui::create_table("##point light props", 2, [&]() {
                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Intensity");

                        imgui::next_column();
                        intensity = imgui::input_float("##Intensity", intensity);
                        // Clamp intensity to be positive
                        intensity = std::max(intensity, 0.01f);

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Falloff Radius");

                        imgui::next_column();
                        range = imgui::input_float("##Falloff Radius", range);
                        // Clamp range to be positive
                        range = std::max(range, 0.01f);
                    });
                });

                if (color != point_light->color || intensity != point_light->intensity || range != point_light->range)
                {
                    point_light->color = color;
                    point_light->intensity = intensity;
                    point_light->range = range;

                    return true;
                }
            }

            return false;
        }
    };

    struct shadow_map_component_view : component_view_factory
    {
        bool create_view(ecs::registry& registry, ecs::entity ent) const override
        {
            if (auto shadows = registry.try_get<graphics::shadow_map_component>(ent))
            {
                using imgui = graphics::imgui_context;
                auto size = shadows->size;
                auto cascade_count = shadows->cascade_count;

                imgui::create_header("Shadow Map Component", [&]() {
                    imgui::label("Size");

                    string_view size_labels[] = {
                        "1024x1024", // index 0
                        "2048x2048", // index 1
                        "4096x4096", // index 2
                    };

                    // Get current index with bit manipulation
                    // 1024 >> x = 0
                    // 2048 >> x = 1
                    // 4096 >> x = 2

                    auto size_index = size.x >> 11;
                    size_index = imgui::combo_box("##Size", size_index, size_labels);

                    // Set size based on index
                    size.x = 1024 << size_index;
                    size.y = 1024 << size_index;

                    imgui::label("Cascade Count");
                    cascade_count = imgui::int_slider("Cascade Count", 1, 6, cascade_count);
                });

                if (size != shadows->size || cascade_count != shadows->cascade_count)
                {
                    shadows->size = size;
                    shadows->cascade_count = cascade_count;
                    return true;
                }
            }

            return false;
        }
    };
} // namespace tempest::editor

#endif // tempest_editor_lighting_component_view_hpp