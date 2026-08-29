#include <tempest/math_utils.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define GAME_API __declspec(dllexport)
#elif defined(TEMPEST_PLATFORM_LINUX)
#define GAME_API __attribute__((visibility("default")))
#else
#error "Unsupported platform"
#endif

extern "C"
{
    GAME_API void on_load(tempest::engine_context* ctx, [[maybe_unused]] tempest::span<tempest::string_view> args)
    {
        auto& logger = ctx->get_logger();
        logger.info("Game loaded successfully!");

        ctx->register_on_close_callback([](auto& engine_ctx) -> void {
            auto& log = engine_ctx.get_logger();
            log.info("Game is closing...");
            [[maybe_unused]] auto saved = engine_ctx.get_assets().save();
        });

        ctx->register_on_initialize_callback([](auto& engine_ctx) -> void {
            // Create a camera
            auto& registry = engine_ctx.get_entities();

            auto camera = registry.create();
            registry.name(camera, "Camera");
            auto camera_data = tempest::render_system::camera_component{
                .aspect_ratio = 16.0F / 9.0F,
                .vertical_fov = tempest::math::as_radians(100.0F),
                .near_plane = 0.01F,
            };
            registry.assign(camera, camera_data);
            auto camera_tx = tempest::ecs::transform_component::identity();
            camera_tx.position({0.0F, 15.0F, -1.0F});
            camera_tx.rotation({0.0F, tempest::math::as_radians(90.0F), 0.0F});
            registry.assign(camera, camera_tx);

            // Load Sponza
            auto& asset_database = engine_ctx.get_assets();
            asset_database.open("game.tassetdb");

            const auto sponza_prefab =
                asset_database.load("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf", registry);

            const auto sponza_instance = engine_ctx.load_entity(sponza_prefab);
            auto sponza_transform = tempest::ecs::transform_component::identity();
            sponza_transform.scale({0.125F});
            registry.assign_or_replace(sponza_instance, sponza_transform);
            registry.name(sponza_instance, "Sponza");

            // Load Sun
            auto sun = registry.create();
            auto sun_data = tempest::render_system::directional_light_component{
                .color = {1.0F, 0.95F, 0.88F},
                .intensity = 2.5F,
            };

            auto sun_shadows = tempest::render_system::shadow_caster_component{
                .resolution = 4096,
                .num_cascades = 4,
                .split_lambda = 0.9F,
                .max_shadow_distance = 1024.0F,
            };

            auto sun_tx = tempest::ecs::transform_component::identity();
            sun_tx.rotation({tempest::math::as_radians(90.0F), 0.0F, 0.0F});

            registry.assign_or_replace(sun, sun_shadows);
            registry.assign_or_replace(sun, sun_data);
            registry.assign_or_replace(sun, sun_tx);
            registry.name(sun, "Sun");
        });
    }

    GAME_API void on_unload()
    {
        // Cleanup code for the game goes here
    }
}