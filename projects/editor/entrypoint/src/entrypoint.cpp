#include "asset_explorer.hpp"
#include "editor.hpp"
#include "entity_panes.hpp"
#include "viewport.hpp"

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

        auto ui_context = tempest::make_unique<ui::ui_context>(win_surface, &engine.get_renderer().get_device(),
                                                               rhi::image_format::bgra8_srgb, 3);

        auto ui_pipeline = engine.register_pipeline<ui::ui_pipeline>(win_surface, ui_context.get());
        ui_pipeline->set_viewport(win_surface->framebuffer_width(), win_surface->framebuffer_height());

        auto pbr_pipeline_handle = ui_pipeline->register_viewport_pipeline(
            tempest::make_unique<tempest::graphics::pbr_pipeline>(1920, 1080, engine.get_registry()));
        auto pbr_pipeline =
            static_cast<tempest::graphics::pbr_pipeline*>(ui_pipeline->get_viewport_pipeline(pbr_pipeline_handle));

        auto editor_ctx = editor_context{};

        auto viewport_pane = editor_ctx.register_pane(tempest::make_unique<viewport>(pbr_pipeline),
                                                      editor_context::dock_location::center);
        [[maybe_unused]] auto entity_pane = editor_ctx.register_pane(
            tempest::make_unique<entity_inspector>(engine.get_registry()), editor_context::dock_location::right);
        [[maybe_unused]] auto hierarchy_pane = editor_ctx.register_pane(
            tempest::make_unique<scene_hierarchy>(engine.get_registry()), editor_context::dock_location::left);
        editor_ctx.register_pane(tempest::make_unique<asset_explorer>(), editor_context::dock_location::bottom);

        auto camera = engine.get_registry().create();
        engine.get_registry().name(camera, "Main Camera");

        // Create a camera
        engine.register_on_initialize_callback([pbr_pipeline, camera](engine_context& ctx) {
            auto sponza_prefab = ctx.get_asset_database().import(
                "assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf", ctx.get_registry());
            auto sponza_instance = ctx.load_entity(sponza_prefab);
            ctx.get_registry().get<tempest::ecs::transform_component>(sponza_instance).scale({0.125f});
            ctx.get_registry().name(sponza_instance, "Sponza");

            auto skybox_texture_prefab =
                ctx.get_asset_database().import("assets/polyhaven/hdri/autumn_field_puresky.exr", ctx.get_registry());
            auto skybox_texture =
                ctx.get_registry().get<tempest::core::texture_component>(skybox_texture_prefab).texture_id;
            pbr_pipeline->set_skybox_texture(ctx.get_renderer().get_device(), skybox_texture,
                                             ctx.get_texture_registry());

            tempest::graphics::camera_component camera_data = {
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
            tempest::graphics::directional_light_component sun_data = {
                .color = {1.0f, 1.0f, 1.0f},
                .intensity = 1.0f,
            };

            tempest::graphics::shadow_map_component sun_shadows = {
                .size = {4096, 4096},
                .cascade_count = 4,
            };

            ctx.get_registry().assign_or_replace(sun, sun_shadows);
            ctx.get_registry().assign_or_replace(sun, sun_data);
            ctx.get_registry().name(sun, "Sun");

            tempest::ecs::transform_component sun_tx = tempest::ecs::transform_component::identity();
            sun_tx.rotation({tempest::math::as_radians(90.0f), 0.0f, 0.0f});

            ctx.get_registry().assign_or_replace(sun, sun_tx);
        });

        engine.register_on_variable_update_callback([&ui_context, &editor_ctx, &viewport_pane, camera, &entity_pane,
                                                     &hierarchy_pane](engine_context& ctx, auto dt) mutable {
            auto vp_size = viewport_pane->window_size();
            auto& cam_data = ctx.get_registry().get<graphics::camera_component>(camera);
            cam_data.aspect_ratio = static_cast<float>(vp_size.x) / vp_size.y;

            entity_pane->set_selected_entity(hierarchy_pane->get_selected_entity());

            editor_ctx.draw(ctx, *ui_context);
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
