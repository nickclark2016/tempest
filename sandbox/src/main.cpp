#include <tempest/tempest.hpp>

#include <tempest/transform_component.hpp>

#include "fps_controller.hpp"

int main()
{
    auto eng = tempest::engine::initialize();

    auto [win, input_group] = eng.add_window(tempest::graphics::window_factory::create({
        .title = "Tempest",
        .width = 1920,
        .height = 1080,
    }));

    win->disable_cursor();

    auto camera = eng.get_registry().acquire_entity();
    eng.get_registry().assign(camera, tempest::graphics::camera_component{});

    eng.on_initialize([](tempest::engine& eng) {
        auto sponza = eng.load_asset("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf");
        auto lantern = eng.load_asset("assets/glTF-Sample-Assets/Models/Lantern/glTF/Lantern.gltf");

        tempest::ecs::transform_component lantern_scalar;
        lantern_scalar.scale({0.1f});

        eng.get_registry().assign(lantern, lantern_scalar);
    });

    fps_controller fps_ctrl;
    fps_ctrl.set_position({10.0f, 4.0f, 0.0f});
    fps_ctrl.set_rotation({0.0f, 180.0f, 0.0f});

    bool was_escape_down_last_frame = false;

    eng.on_update([&](tempest::engine& eng, float dt) {
        fps_ctrl.update(*input_group.kb, *input_group.ms, dt);
        auto& camera_data = eng.get_registry().get<tempest::graphics::camera_component>(camera);
        camera_data.forward = fps_ctrl.eye_direction();
        camera_data.position = fps_ctrl.eye_position();
        camera_data.up = fps_ctrl.up_direction();
        camera_data.aspect_ratio = static_cast<float>(win->width()) / static_cast<float>(win->height());
        camera_data.vertical_fov = 90.0f;

        if (input_group.kb->is_key_down(tempest::core::key::ESCAPE))
        {
            if (!was_escape_down_last_frame)
            {
                win->disable_cursor(!win->is_cursor_disabled());
            }
            was_escape_down_last_frame = true;
        }
        else
        {
            was_escape_down_last_frame = false;
        }
    });

    eng.run();

    return 0;
}