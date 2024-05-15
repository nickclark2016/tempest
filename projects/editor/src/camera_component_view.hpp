#ifndef tempest_editor_camera_component_view_hpp
#define tempest_editor_camera_component_view_hpp

#include <tempest/component_view.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/imgui_context.hpp>
#include <tempest/registry.hpp>

namespace tempest::editor
{
    class camera_component_view : public component_view_factory
    {
      public:
        bool create_view(tempest::ecs::registry& reg, tempest::ecs::entity ent) const override
        {
            if (graphics::camera_component* comp = reg.try_get<graphics::camera_component>(ent))
            {
                using imgui = tempest::graphics::imgui_context;

                imgui::create_header("Camera Component", [&]() {
                    imgui::create_table("##camera_component_container", 2, [&]() {
                        imgui::next_row();

                        imgui::next_column();
                        imgui::label("Field of View");
                        imgui::next_column();
                        comp->vertical_fov = imgui::input_float("##field_of_view", comp->vertical_fov);

                        imgui::next_row();

                        imgui::next_column();
                        imgui::label("Aspect Ratio");
                        imgui::next_column();
                        comp->aspect_ratio = imgui::input_float("##aspect_ratio", comp->aspect_ratio);

                        imgui::next_row();

                        imgui::next_column();
                        imgui::label("Near Clip Plane");
                        imgui::next_column();
                        comp->near_plane = imgui::input_float("##camera_near_plane", comp->near_plane);
                    });
                });
            }
            return false;
        }
    };
} // namespace tempest::editor

#endif // tempest_editor_camera_component_view_hpp