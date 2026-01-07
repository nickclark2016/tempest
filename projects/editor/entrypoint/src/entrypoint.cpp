#include <tempest/archetype.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>

#include "editor.hpp"

namespace tempest::editor
{
    static unique_ptr<editor> setup_render_graph(engine_context& ctx, rhi::window_surface* win_surface, ui::ui_context* ui_ctx)
    {
        return make_unique<editor>(ctx, win_surface, ui_ctx);
    }

    static void run()
    {
        auto engine = tempest::engine_context{};
        auto window_data = engine.register_window(
            {
                .width = 1920,
                .height = 1080,
                .name = "Tempest Editor",
                .fullscreen = false,
            },
            false);

        auto&& [win_surface, render_surface, inputs] = window_data;
        auto ui_ctx = ui::ui_context(win_surface, &engine.get_renderer().get_device(),
                                     engine.get_renderer().get_frame_graph().get_tonemapped_color_format(), 3);

        auto camera = static_cast<ecs::archetype_entity>(ecs::null);
        auto ui_editor = unique_ptr<editor>();

        engine.register_on_initialize_callback([&](engine_context& ctx) {
            ui_editor = setup_render_graph(ctx, win_surface, &ui_ctx);

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

        engine.register_on_variable_update_callback(
            [&camera, &win_surface, &ui_ctx, &ui_editor](engine_context& ctx, auto dt) mutable {
                ui_ctx.begin_ui_commands();
                ui_editor->draw({});
                ui_ctx.finish_ui_commands();
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
