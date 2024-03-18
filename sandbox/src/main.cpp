#include <tempest/tempest.hpp>

#include "fps_controller.hpp"

int main()
{
    auto eng = tempest::engine::initialize();

    auto input_group = eng.add_window(tempest::graphics::window_factory::create({
        .title = "Tempest",
        .width = 1920,
        .height = 1080,
    }));

    auto camera = eng.get_registry().acquire_entity();
    eng.get_registry().assign(camera, tempest::graphics::camera_component{});

    eng.on_initialize([](tempest::engine& eng) {
        auto sponza = eng.load_asset("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf");
    });

    fps_controller fps_ctrl;
    fps_ctrl.set_position({5.0f, 6.0f, 0.0f});

    eng.on_update([&](tempest::engine& eng, float dt) {
        fps_ctrl.update(*input_group.kb, dt);
        auto& camera_data = eng.get_registry().get<tempest::graphics::camera_component>(camera);
        camera_data.forward = fps_ctrl.eye_direction();
        camera_data.position = fps_ctrl.eye_position();
        camera_data.up = fps_ctrl.up_direction();
    });

    eng.run();

    return 0;
}