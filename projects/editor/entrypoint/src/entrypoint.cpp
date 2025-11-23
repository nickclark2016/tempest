#include <tempest/archetype.hpp>
#include <tempest/pipelines/pbr_pipeline.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/ui.hpp>

namespace tempest::editor
{
    void run()
    {
        auto engine = tempest::engine_context{};
        auto [win_surface, inputs] = engine.register_window({
            .width = 1920,
            .height = 1080,
            .name = "Tempest Editor",
            .fullscreen = false,
        });

        auto camera = static_cast<ecs::archetype_entity>(ecs::null);

        engine.register_on_initialize_callback([&](engine_context& ctx) {
            auto sponza_prefab = ctx.get_asset_database().import(
                "assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf", ctx.get_registry());
            auto sponza_instance = ctx.load_entity(sponza_prefab);
            ctx.get_registry().get<tempest::ecs::transform_component>(sponza_instance).scale({0.125f});
            ctx.get_registry().name(sponza_instance, "Sponza");

            camera = ctx.get_registry().create();
            ctx.get_registry().name(camera, "Main Camera");

            auto camera_data = tempest::graphics::camera_component{
                .aspect_ratio = 16.0f / 9.0f,
                .vertical_fov = 100.0f,
                .near_plane = 0.01f,
                .far_shadow_plane = 64.0f,
            };

            ctx.get_registry().assign(camera, camera_data);

            auto camera_tx = tempest::ecs::transform_component::identity();
            camera_tx.position({0.0f, 15.0f, -1.0f});
            camera_tx.rotation({0.0f, tempest::math::as_radians(90.0f), 0.0f});
            ctx.get_registry().assign(camera, camera_tx);

            auto sun = ctx.get_registry().create();
            auto sun_data = tempest::graphics::directional_light_component{
                .color = {1.0f, 1.0f, 1.0f},
                .intensity = 1.0f,
            };

            auto sun_shadows = tempest::graphics::shadow_map_component{
                .size = {4096, 4096},
                .cascade_count = 4,
            };

            ctx.get_registry().assign_or_replace(sun, sun_shadows);
            ctx.get_registry().assign_or_replace(sun, sun_data);
            ctx.get_registry().name(sun, "Sun");

            auto sun_tx = tempest::ecs::transform_component::identity();
            sun_tx.rotation({tempest::math::as_radians(90.0f), 0.0f, 0.0f});
            ctx.get_registry().assign_or_replace(sun, sun_tx);
        });

        engine.register_on_variable_update_callback([&camera, &win_surface](engine_context& ctx, auto dt) mutable {
            const auto window_width = win_surface->framebuffer_width();
            const auto window_height = win_surface->framebuffer_height();
            auto& cam_data = ctx.get_registry().get<graphics::camera_component>(camera);
            cam_data.aspect_ratio = static_cast<float>(window_width) / window_height;
        });

        engine.run();
    }
} // namespace tempest::editor

#if _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine,
                     _In_ int nCmdShow)
{
    tempest::editor::run();
    return 0;
}

#else

int main(int argc, char* argv[])
{
    tempest::editor::run();
    return 0;
}

#endif
