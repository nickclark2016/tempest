#include <tempest/editor.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>

#include <iostream>

int main()
{
    tempest::engine engine = tempest::engine::initialize();

    auto [win, input_group] = engine.add_window(
        tempest::graphics::window_factory::create({.title = "Tempest Editor", .width = 1920, .height = 1080}));

    auto renderer_settings = engine.get_render_system().settings();
    renderer_settings.aa_mode = tempest::graphics::anti_aliasing_mode::NONE;
    renderer_settings.enable_imgui = true;
    renderer_settings.enable_profiling = true;
    engine.get_render_system().update_settings(renderer_settings);

    engine.on_initialize([](tempest::engine& engine) {
        auto lantern = engine.load_asset("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf");
        auto lantern_transform = tempest::ecs::transform_component{};
        lantern_transform.scale({0.1f});

        engine.get_registry().assign(lantern, lantern_transform);

        auto camera = engine.get_registry().acquire_entity();

        tempest::graphics::camera_component camera_data = {
            .aspect_ratio = 16.0f / 9.0f,
            .vertical_fov = 90.0f,
            .near_plane = 0.1f,
        };

        tempest::ecs::transform_component camera_transform;
        camera_transform.position({0.0f, 0.0f, 0.0f});

        engine.get_registry().name(camera, "Camera");

        engine.get_registry().assign(camera, camera_data);
        engine.get_registry().assign(camera, camera_transform);
    });

    tempest::editor::editor editor;

    engine.on_update([&editor](tempest::engine& engine, float dt) {
        engine.get_render_system().draw_imgui([&, dt]() {
            using imgui = tempest::graphics::imgui_context;

            imgui::create_window("Entities", [&]() { editor.update(engine); });
            imgui::create_window("Metrics", [&, dt]() { imgui::label(std::format("FPS: {:.2f}", 1.0f / dt)); });
        });
    });

    engine.run();
}