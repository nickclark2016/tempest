#include <tempest/editor.hpp>
#include <tempest/logger.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>

#include <iostream>

void initialize_lights(tempest::ecs::archetype_registry& registry);
void initialize_models(tempest::engine& engine);

namespace
{
    auto logger = tempest::logger::logger_factory::create({.prefix = {"editor::main"}});
}

int main()
{
    tempest::engine engine = tempest::engine::initialize();

    auto [win, input_group] = engine.add_window(tempest::graphics::window_factory::create({
        .title = "Tempest Editor",
        .width = 1920,
        .height = 1080,
    }));
    (void)win;

    auto renderer_settings = engine.get_render_system().settings();
    renderer_settings.aa_mode = tempest::graphics::anti_aliasing_mode::NONE;
    renderer_settings.enable_imgui = true;
    renderer_settings.enable_profiling = true;
    engine.get_render_system().update_settings(renderer_settings);

    tempest::editor::editor editor(engine);

    engine.on_initialize([](tempest::engine& engine) {
        auto camera = engine.get_archetype_registry().create();

        tempest::graphics::camera_component camera_data = {
            .aspect_ratio = 16.0f / 9.0f,
            .vertical_fov = 90.0f,
            .near_plane = 0.01f,
            .far_shadow_plane = 64.0f,
        };

        tempest::ecs::transform_component camera_transform = tempest::ecs::transform_component::identity();
        camera_transform.position({0.0f, 10.0f, -25.0f});
        camera_transform.rotation({0.0f, 0.0f, 0.0f});

        engine.get_archetype_registry().assign(camera, camera_data);
        engine.get_archetype_registry().assign(camera, camera_transform);
        engine.get_archetype_registry().name(camera, "Camera");

        initialize_models(engine);
        initialize_lights(engine.get_archetype_registry());
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

void initialize_lights(tempest::ecs::archetype_registry& registry)
{
    auto sun = registry.create();
    tempest::graphics::directional_light_component sun_data = {
        .color = {1.0f, 1.0f, 1.0f},
        .intensity = 1.0f,
    };

    tempest::graphics::shadow_map_component sun_shadows = {
        .size = {2048, 2048},
        .cascade_count = 1,
    };

    registry.assign_or_replace(sun, sun_shadows);
    registry.assign_or_replace(sun, sun_data);
    registry.name(sun, "Sun");

    tempest::ecs::transform_component sun_tx = tempest::ecs::transform_component::identity();
    sun_tx.rotation({tempest::math::as_radians(90.0f), 0.0f, 0.0f});

    registry.assign_or_replace(sun, sun_tx);

    // Create a point light
    auto point_light = registry.create();
    tempest::graphics::point_light_component point_light_data = {
        .color = {1.0f, 1.0f, 1.0f},
        .intensity = 50.0f,
        .range = 10.0f,
    };

    auto point_light_tx = tempest::ecs::transform_component::identity();
    point_light_tx.position({0.0f, 10.0f, 15.0f});

    registry.assign_or_replace(point_light, point_light_data);
    registry.assign_or_replace(point_light, point_light_tx);
    registry.name(point_light, "Point Light");
}

void initialize_models(tempest::engine& engine)
{
    auto sponza_prefab = engine.get_asset_database().import("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf",
                                                            engine.get_archetype_registry());
    auto sponza_instance = engine.load_entity(sponza_prefab);
    auto sponza_transform = tempest::ecs::transform_component::identity();
    sponza_transform.scale({12.5f, 12.5f, 12.5f});
    engine.get_archetype_registry().assign_or_replace(sponza_instance, sponza_transform);

    // auto lantern_prefab = engine.get_asset_database().import(
    //     "assets/glTF-Sample-Assets/Models/Lantern/glTF/Lantern.gltf", engine.get_archetype_registry());
    // auto lantern_instance = engine.load_entity(lantern_prefab);
    // auto lantern_transform = tempest::ecs::transform_component{};
    // lantern_transform.position({0.0f, 0.0f, 2.0f});
    // lantern_transform.scale({0.1f, 0.1f, 0.1f});
    // engine.get_archetype_registry().assign(lantern_instance, lantern_transform);
}