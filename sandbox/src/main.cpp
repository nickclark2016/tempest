#include <tempest/pipelines/pbr_pipeline.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>

int main()
{
    auto engine = tempest::engine_context();
    auto surface_desc = tempest::rhi::window_surface_desc{
        .width = 1920,
        .height = 1080,
        .name = "Tempest Engine Sandbox",
        .fullscreen = false,
    };

    auto window_and_inputs = engine.register_window(surface_desc);
    auto& [surface, inputs] = window_and_inputs;
    auto pipeline =
        engine.register_pipeline<tempest::graphics::pbr_pipeline>(surface, 1920, 1080, engine.get_registry());

    engine.register_on_initialize_callback([&](tempest::engine_context& ctx) {
        auto sponza_prefab = ctx.get_asset_database().import("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf",
                                                             ctx.get_registry());
        auto sponza_instance = ctx.load_entity(sponza_prefab);
        ctx.get_registry().get<tempest::ecs::transform_component>(sponza_instance).scale({0.125f});

        auto skybox_texture_prefab =
            ctx.get_asset_database().import("assets/polyhaven/hdri/autumn_field_puresky.exr", ctx.get_registry());
        auto skybox_texture =
            ctx.get_registry().get<tempest::core::texture_component>(skybox_texture_prefab).texture_id;
        pipeline->set_skybox_texture(ctx.get_renderer().get_device(), skybox_texture, ctx.get_texture_registry());

        auto camera = ctx.get_registry().create();
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
            .size = {2048, 2048},
            .cascade_count = 3,
        };

        surface->register_resize_callback([&ctx, camera](uint32_t width, uint32_t height) {
            auto camera_data = ctx.get_registry().get<tempest::graphics::camera_component>(camera);
            camera_data.aspect_ratio = static_cast<float>(width) / height;
            ctx.get_registry().assign_or_replace(camera, camera_data);
        });

        ctx.get_registry().assign_or_replace(sun, sun_shadows);
        ctx.get_registry().assign_or_replace(sun, sun_data);
        ctx.get_registry().name(sun, "Sun");

        tempest::ecs::transform_component sun_tx = tempest::ecs::transform_component::identity();
        sun_tx.rotation({tempest::math::as_radians(90.0f), 0.0f, 0.0f});

        ctx.get_registry().assign_or_replace(sun, sun_tx);
    });

    engine.run();
}