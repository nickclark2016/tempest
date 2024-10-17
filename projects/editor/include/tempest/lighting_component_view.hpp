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

                imgui::create_header("Directional Light Component", [&]() {
                    imgui::label("Color");
                    dir_light->color = imgui::input_color("Color", dir_light->color);

                    imgui::label("Intensity");
                    dir_light->intensity = imgui::float_slider("Intensity", 0, 1, dir_light->intensity);
                });
            }

            return false;
        }
    };
} // namespace tempest::editor

#endif // tempest_editor_lighting_component_view_hpp