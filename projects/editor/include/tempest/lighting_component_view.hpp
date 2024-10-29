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
} // namespace tempest::editor

#endif // tempest_editor_lighting_component_view_hpp