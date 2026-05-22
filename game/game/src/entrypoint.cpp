#include <tempest/graphics_components.hpp>
#include <tempest/tempest.hpp>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define GAME_API __declspec(dllexport)
#elif defined(TEMPEST_PLATFORM_LINUX)
#define GAME_API __attribute__((visibility("default")))
#else
#error "Unsupported platform"
#endif

extern "C"
{
    GAME_API void on_load(tempest::engine_context* ctx, tempest::span<tempest::string_view> /*args*/)
    {
        auto& logger = ctx->get_logger();
        logger.info("Game loaded successfully!");

        [[maybe_unused]] auto window = ctx->register_window(
            {
                .width = 1920,
                .height = 1080,
                .name = "Tempest Game",
                .fullscreen = false,
            },
            true);

        ctx->register_on_close_callback([](auto& engine_ctx) -> void {
            auto& logger = engine_ctx.get_logger();
            logger.info("Game is closing...");
        });

        ctx->register_on_initialize_callback([](auto& engine_ctx) -> void {
            // Create a camera
            auto& registry = engine_ctx.get_registry();

            auto camera = registry.create();
            tempest::graphics::camera_component camera_data = {
                .aspect_ratio = 16.0F / 9.0F,
                .vertical_fov = 100.0F,
                .near_plane = 0.01F,
            };
            registry.assign(camera, camera_data);
            auto camera_tx = tempest::ecs::transform_component::identity();
            camera_tx.position({0.0F, 15.0F, -1.0F});
            camera_tx.rotation({0.0F, tempest::math::as_radians(90.0F), 0.0F});
            registry.assign(camera, camera_tx);

            // Load Sponza
            auto& asset_database = engine_ctx.get_asset_database();
            asset_database.open("game.tassetdb");

            const auto sponza_prefab =
                asset_database.load("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf", registry);

            const auto sponza_instance = engine_ctx.load_entity(sponza_prefab);
            auto sponza_transform = tempest::ecs::transform_component{};
            sponza_transform.scale({0.125F});
            registry.assign_or_replace(sponza_instance, sponza_transform);

            // Load Sun
            auto sun = registry.create();
            auto sun_data = tempest::graphics::directional_light_component{
                .color = {1.0F, 1.0F, 1.0F},
                .intensity = 1.0F,
            };

            auto sun_shadows = tempest::graphics::shadow_map_component{
                .shadow_distance = 2048.0F,
                .split_lambda = 0.9F,
                .blend_fraction = 0.1F,
                .cascade_count = 4,
            };

            auto sun_tx = tempest::ecs::transform_component::identity();
            sun_tx.rotation({tempest::math::as_radians(90.0F), 0.0F, 0.0F});

            registry.assign_or_replace(sun, sun_shadows);
            registry.assign_or_replace(sun, sun_data);
            registry.assign_or_replace(sun, sun_tx);
            registry.name(sun, "Sun");
        });

        ctx->register_on_close_callback([](auto& engine_ctx) -> void { (void)engine_ctx.get_asset_database().save(); });
    }

    GAME_API void on_unload()
    {
        // Cleanup code for the game goes here
    }
}