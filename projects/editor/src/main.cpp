#include <tempest/editor.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>

#include <iostream>

int main()
{
    tempest::engine engine = tempest::engine::initialize();

    auto [win, input_group] = engine.add_window(tempest::graphics::window_factory::create({
        .title = "Tempest Editor",
        .width = 1920,
        .height = 1080,
    }));

    auto renderer_settings = engine.get_render_system().settings();
    renderer_settings.aa_mode = tempest::graphics::anti_aliasing_mode::NONE;
    renderer_settings.enable_imgui = true;
    renderer_settings.enable_profiling = true;
    engine.get_render_system().update_settings(renderer_settings);

    tempest::editor::editor editor(engine);

    engine.on_initialize([&editor](tempest::engine& engine) {
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

        auto sponza_prefab = engine.get_asset_database().import(
            "assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf", engine.get_registry());
        auto sponza_instance = engine.load_entity(sponza_prefab);
        auto sponza_transform = tempest::ecs::transform_component{};
        sponza_transform.scale({0.1f});
        engine.get_registry().assign(sponza_instance, sponza_transform);
    });

    engine.on_update([&editor](tempest::engine& engine, float dt) {
        engine.get_render_system().draw_imgui([&, dt]() {
            using imgui = tempest::graphics::imgui_context;

            imgui::create_window("Entities", [&]() { editor.update(engine); });
            imgui::create_window(
                "Metrics", [&, dt]() { imgui::label(tempest::string_view(std::format("FPS: {:.2f}", 1.0f / dt))); });

            if (engine.get_render_system().settings().enable_profiling)
            {
                engine.get_render_system().draw_profiler();
            }
        });
    });

    engine.run();
}