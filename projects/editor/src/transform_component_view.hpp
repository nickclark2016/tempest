#ifndef tempest_editor_transform_component_view_hpp
#define tempest_editor_transform_component_view_hpp

#include <tempest/component_view.hpp>
#include <tempest/imgui_context.hpp>
#include <tempest/transform_component.hpp>

namespace tempest::editor
{
    class transform_component_view : public component_view_factory
    {
      public:
        bool create_view(ecs::registry& reg, ecs::entity ent) const override
        {
            if (ecs::transform_component* tc = reg.try_get<ecs::transform_component>(ent))
            {
                auto position = tc->position();
                auto rotation = tc->rotation();
                auto scale = tc->scale();

                using imgui = tempest::graphics::imgui_context;

                imgui::create_header("Transform Component", [&]() {
                    imgui::create_table("##transform_component_container", 4, [&]() {
                        imgui::next_row();

                        imgui::next_column();
                        imgui::label("Position");
                        imgui::next_column();
                        position.x = imgui::input_float("##position_x", position.x);
                        imgui::next_column();
                        position.y = imgui::input_float("##position_y", position.y);
                        imgui::next_column();
                        position.z = imgui::input_float("##position_z", position.z);

                        imgui::next_row();

                        imgui::next_column();
                        imgui::label("Rotation");
                        imgui::next_column();
                        rotation.x = imgui::input_float("##rotation_x", math::as_degrees(rotation.x));
                        imgui::next_column();
                        rotation.y = imgui::input_float("##rotation_y", math::as_degrees(rotation.y));
                        imgui::next_column();
                        rotation.z = imgui::input_float("##rotation_z", math::as_degrees(rotation.z));

                        rotation = math::as_radians(rotation);

                        imgui::next_row();

                        imgui::next_column();
                        imgui::label("Scale");
                        imgui::next_column();
                        scale.x = imgui::input_float("##scale_x", scale.x);
                        imgui::next_column();
                        scale.y = imgui::input_float("##scale_y", scale.y);
                        imgui::next_column();
                        scale.z = imgui::input_float("##scale_z", scale.z);
                    });
                });

                // Update the component
                if (tc->position() != position || tc->rotation() != rotation || tc->scale() != scale)
                {
                    tc->position(position);
                    tc->rotation(rotation);
                    tc->scale(scale);

                    return true;
                }
            }
            return false;
        }
    };
} // namespace tempest::editor

#endif // tempest_editor_transform_component_view_hpp