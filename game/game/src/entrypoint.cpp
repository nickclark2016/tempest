#include <tempest/math_utils.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define GAME_API __declspec(dllexport)
#elif defined(TEMPEST_PLATFORM_LINUX)
#define GAME_API __attribute__((visibility("default")))
#else
#error "Unsupported platform"
#endif

namespace
{
    auto create_cube_mesh(float half_size) -> tempest::core::mesh
    {
        auto m = tempest::core::mesh{};

        auto add_face = [&](tempest::math::vec3<float> normal, tempest::math::vec3<float> tangent,
                            tempest::math::vec3<float> v0, tempest::math::vec3<float> v1,
                            tempest::math::vec3<float> v2, tempest::math::vec3<float> v3,
                            tempest::math::vec4<float> color) -> void {
            const auto base_idx = static_cast<tempest::uint32_t>(m.vertices.size());
            m.vertices.push_back(tempest::core::vertex{.position = v0,
                                                       .uv = {0.0F, 0.0F},
                                                       .normal = normal,
                                                       .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                       .color = color});
            m.vertices.push_back(tempest::core::vertex{.position = v1,
                                                       .uv = {1.0F, 0.0F},
                                                       .normal = normal,
                                                       .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                       .color = color});
            m.vertices.push_back(tempest::core::vertex{.position = v2,
                                                       .uv = {1.0F, 1.0F},
                                                       .normal = normal,
                                                       .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                       .color = color});
            m.vertices.push_back(tempest::core::vertex{.position = v3,
                                                       .uv = {0.0F, 1.0F},
                                                       .normal = normal,
                                                       .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                       .color = color});

            m.indices.push_back(base_idx + 0);
            m.indices.push_back(base_idx + 1);
            m.indices.push_back(base_idx + 2);
            m.indices.push_back(base_idx + 2);
            m.indices.push_back(base_idx + 3);
            m.indices.push_back(base_idx + 0);
        };

        const auto s = half_size;
        // Front (+Z)
        add_face({0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F}, {-s, -s, s}, {s, -s, s}, {s, s, s}, {-s, s, s},
                 {1.0F, 0.55F, 0.2F, 1.0F});
        // Back (-Z)
        add_face({0.0F, 0.0F, -1.0F}, {-1.0F, 0.0F, 0.0F}, {s, -s, -s}, {-s, -s, -s}, {-s, s, -s}, {s, s, -s},
                 {1.0F, 0.55F, 0.2F, 1.0F});
        // Left (-X)
        add_face({-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {-s, -s, -s}, {-s, -s, s}, {-s, s, s}, {-s, s, -s},
                 {0.2F, 0.8F, 1.0F, 1.0F});
        // Right (+X)
        add_face({1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, {s, -s, s}, {s, -s, -s}, {s, s, -s}, {s, s, s},
                 {0.2F, 0.8F, 1.0F, 1.0F});
        // Top (+Y)
        add_face({0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {-s, s, s}, {s, s, s}, {s, s, -s}, {-s, s, -s},
                 {1.0F, 0.9F, 0.25F, 1.0F});
        // Bottom (-Y)
        add_face({0.0F, -1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {-s, -s, -s}, {s, -s, -s}, {s, -s, s}, {-s, -s, s},
                 {0.3F, 0.3F, 0.3F, 1.0F});

        return m;
    }

    struct patrol_component
    {
        float min_x;
        float max_x;
        float speed;
        float direction;
        float yaw_speed;
        float pitch_speed;
    };
} // namespace

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
                .vertical_fov = tempest::math::as_radians(60.0F),
                .near_plane = 0.05F,
            };
            registry.assign(camera, camera_data);
            auto camera_tx = tempest::ecs::transform_component::identity();
            camera_tx.position({0.0F, 0.6F, -1.8F});
            camera_tx.rotation({tempest::math::as_radians(-10.0F), 0.0F, 0.0F});
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
                .color = {1.0F, 0.98F, 0.92F},
                .intensity = 5.0F,
            };

            auto sun_shadows = tempest::render_system::shadow_caster_component{
                .resolution = 4096,
                .num_cascades = 4,
                .split_lambda = 0.5F,
                .max_shadow_distance = 100.0F,
                .normal_bias = 0.02F,
                .depth_bias = 0.005F,
            };

            auto sun_tx = tempest::ecs::transform_component::identity();
            sun_tx.rotation({tempest::math::as_radians(85.0F), tempest::math::as_radians(10.0F), 0.0F});

            registry.assign_or_replace(sun, sun_shadows);
            registry.assign_or_replace(sun, sun_data);
            registry.assign_or_replace(sun, sun_tx);
            registry.name(sun, "Sun");

            // Register procedural cube mesh and material
            const auto cube_mesh_id = engine_ctx.get_meshes().register_mesh(create_cube_mesh(0.18F));

            auto cube_mat = tempest::core::material{};
            cube_mat.set_vec4(tempest::core::material::base_color_factor_name, {0.95F, 0.6F, 0.15F, 1.0F});
            cube_mat.set_vec3(tempest::core::material::emissive_factor_name, {0.5F, 0.25F, 0.05F});
            cube_mat.set_scalar(tempest::core::material::metallic_factor_name, 0.1F);
            cube_mat.set_scalar(tempest::core::material::roughness_factor_name, 0.35F);
            const auto cube_mat_id = engine_ctx.get_materials().register_material(tempest::move(cube_mat));

            // Moving test entity with transform_history_component and renderable components
            auto test_cube = registry.create();
            registry.name(test_cube, "InterpolationTestEntity");

            const auto initial_position = tempest::math::vec3<float>{-2.5F, 0.35F, 0.0F};
            auto cube_hist = tempest::ecs::transform_history_component::create(initial_position);
            auto cube_tx = tempest::ecs::transform_component::identity();
            cube_tx.position(initial_position);

            auto cube_light = tempest::render_system::point_light_component{
                .color = {1.0F, 0.75F, 0.3F},
                .intensity = 25.0F,
                .range = 6.0F,
            };

            auto cube_patrol = patrol_component{
                .min_x = -2.5F,
                .max_x = 2.5F,
                .speed = 1.5F,
                .direction = 1.0F,
                .yaw_speed = 60.0F,
                .pitch_speed = 30.0F,
            };

            registry.assign(test_cube, cube_hist);
            registry.assign(test_cube, cube_tx);
            registry.assign(test_cube, cube_light);
            registry.assign(test_cube, cube_patrol);
            registry.assign(test_cube, tempest::core::mesh_component{.mesh_id = cube_mesh_id});
            registry.assign(test_cube, tempest::core::material_component{.material_id = cube_mat_id});
        });

        ctx->register_on_fixed_update_callback([](auto& engine_ctx, auto delta_time) -> void {
            auto& reg = engine_ctx.get_entities();
            reg.each([dt = delta_time.count()](tempest::ecs::transform_history_component& hist,
                                               patrol_component& patrol) {
                // 1. Move horizontally along wide track across courtyard
                hist.current_position.x += patrol.direction * patrol.speed * dt;
                if (hist.current_position.x >= patrol.max_x)
                {
                    hist.current_position.x = patrol.max_x;
                    patrol.direction = -1.0F;
                }
                else if (hist.current_position.x <= patrol.min_x)
                {
                    hist.current_position.x = patrol.min_x;
                    patrol.direction = 1.0F;
                }

                // 2. Continuous rotation via incremental delta quaternion composition
                const auto delta_rot = tempest::math::quat<float>(
                    tempest::math::vec3<float>{
                        tempest::math::as_radians(patrol.pitch_speed * dt),
                        tempest::math::as_radians(patrol.yaw_speed * dt),
                        0.0F,
                    });
                hist.current_rotation = tempest::math::normalize(hist.current_rotation * delta_rot);
            });
        });
    }

    GAME_API void on_unload()
    {
        // Cleanup code for the game goes here
    }
}