#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/builtin_assets.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/engine_types.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/physics/box_collider_component.hpp>
#include <tempest/physics/rigid_body_component.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/transform_history_system.hpp>

// =========================================================================
// SECTION: Main Scene Integration Tests
// =========================================================================

/// @brief Verifies that main.tscene loads correctly and contains all five sandbox root entities and components.
TEST(main_scene_test, load_main_scene_and_verify_entities)
{
    // Setup scene file path candidate check
    auto scene_path = tempest::filesystem::path{"game/assets/scenes/main.tscene"};
    if (!tempest::filesystem::exists(scene_path))
    {
        scene_path = tempest::filesystem::path{"assets/scenes/main.tscene"};
    }
    ASSERT_TRUE(tempest::filesystem::exists(scene_path));

    // Setup ECS registry and scene manager
    auto event_reg = tempest::event::event_registry{};
    auto type_reg = tempest::ecs::component_type_registry{};
    tempest::register_engine_types(type_reg);

    auto registry = tempest::ecs::basic_archetype_registry{event_reg, type_reg};
    auto manager = tempest::scene::scene_manager{registry};

    // Act: Load main.tscene synchronously
    const auto load_result = manager.load_sync(scene_path);
    ASSERT_TRUE(load_result.has_value());

    // Assert: Verify 5 entities exist
    const auto camera_opt = registry.find_first_with_name("Camera");
    const auto sun_opt = registry.find_first_with_name("Sun");
    const auto floor_opt = registry.find_first_with_name("FloorPlane");
    const auto step_opt = registry.find_first_with_name("StepObstacle");
    const auto capsule_opt = registry.find_first_with_name("CapsuleProxyTarget");

    ASSERT_TRUE(camera_opt.has_value());
    ASSERT_TRUE(sun_opt.has_value());
    ASSERT_TRUE(floor_opt.has_value());
    ASSERT_TRUE(step_opt.has_value());
    ASSERT_TRUE(capsule_opt.has_value());

    const auto camera_entity = *camera_opt;
    const auto sun_entity = *sun_opt;
    const auto floor_entity = *floor_opt;
    const auto step_entity = *step_opt;
    const auto capsule_entity = *capsule_opt;

    // Verify Camera entity components
    {
        const auto* const tx = registry.try_get<tempest::ecs::transform_component>(camera_entity);
        ASSERT_NE(tx, nullptr);
        EXPECT_NEAR(tx->position().x, 0.0F, 0.001F);
        EXPECT_NEAR(tx->position().y, 1.6F, 0.001F);
        EXPECT_NEAR(tx->position().z, 0.0F, 0.001F);

        const auto* const cam = registry.try_get<tempest::render_system::camera_component>(camera_entity);
        ASSERT_NE(cam, nullptr);
        EXPECT_EQ(cam->target, tempest::render_system::camera_target_type::viewport);
        EXPECT_NEAR(cam->near_plane, 0.05F, 0.001F);
        EXPECT_TRUE(cam->is_active);

        const auto* const active = registry.try_get<tempest::render_system::active_camera_component>(camera_entity);
        EXPECT_NE(active, nullptr);
        EXPECT_TRUE(registry.has<tempest::render_system::active_camera_component>(camera_entity));
    }

    // Verify Sun entity components
    {
        const auto* const tx = registry.try_get<tempest::ecs::transform_component>(sun_entity);
        ASSERT_NE(tx, nullptr);

        const auto* const sun = registry.try_get<tempest::render_system::directional_light_component>(sun_entity);
        ASSERT_NE(sun, nullptr);
        EXPECT_NEAR(sun->intensity, 2.0F, 0.001F);
        EXPECT_NEAR(sun->color.x, 1.0F, 0.001F);
        EXPECT_NEAR(sun->color.y, 0.98F, 0.001F);
        EXPECT_NEAR(sun->color.z, 0.95F, 0.001F);

        const auto* const shadows = registry.try_get<tempest::render_system::shadow_caster_component>(sun_entity);
        ASSERT_NE(shadows, nullptr);
        EXPECT_EQ(shadows->resolution, 2048U);
        EXPECT_EQ(shadows->num_cascades, 4U);
        EXPECT_NEAR(shadows->split_lambda, 0.5F, 0.001F);
        EXPECT_NEAR(shadows->max_shadow_distance, 100.0F, 0.001F);
    }

    // Verify FloorPlane entity components
    {
        const auto* const tx = registry.try_get<tempest::ecs::transform_component>(floor_entity);
        ASSERT_NE(tx, nullptr);
        EXPECT_NEAR(tx->position().x, 0.0F, 0.001F);
        EXPECT_NEAR(tx->position().y, 0.0F, 0.001F);
        EXPECT_NEAR(tx->position().z, 0.0F, 0.001F);
        EXPECT_NEAR(tx->scale().x, 40.0F, 0.001F);
        EXPECT_NEAR(tx->scale().y, 1.0F, 0.001F);
        EXPECT_NEAR(tx->scale().z, 40.0F, 0.001F);

        const auto* const mesh = registry.try_get<tempest::core::mesh_component>(floor_entity);
        ASSERT_NE(mesh, nullptr);
        EXPECT_EQ(mesh->mesh_id, tempest::assets::builtin::plane_mesh_guid);

        const auto* const mat = registry.try_get<tempest::core::material_component>(floor_entity);
        ASSERT_NE(mat, nullptr);
        EXPECT_EQ(mat->material_id, tempest::assets::builtin::default_material_guid);

        const auto* const rb = registry.try_get<tempest::physics::rigid_body_component>(floor_entity);
        ASSERT_NE(rb, nullptr);
        EXPECT_EQ(rb->motion, jolt::shim::motion_type::static_motion);
        EXPECT_EQ(rb->layer, jolt::shim::object_layer::non_moving);

        const auto* const box = registry.try_get<tempest::physics::box_collider_component>(floor_entity);
        ASSERT_NE(box, nullptr);
        EXPECT_NEAR(box->half_extents.x, 20.0F, 0.001F);
        EXPECT_NEAR(box->half_extents.y, 0.5F, 0.001F);
        EXPECT_NEAR(box->half_extents.z, 20.0F, 0.001F);
        EXPECT_NEAR(box->offset.y, -0.5F, 0.001F);
    }

    // Verify StepObstacle entity components
    {
        const auto* const tx = registry.try_get<tempest::ecs::transform_component>(step_entity);
        ASSERT_NE(tx, nullptr);
        EXPECT_NEAR(tx->position().x, 0.0F, 0.001F);
        EXPECT_NEAR(tx->position().y, 0.1F, 0.001F);
        EXPECT_NEAR(tx->position().z, 5.0F, 0.001F);
        EXPECT_NEAR(tx->scale().x, 2.0F, 0.001F);
        EXPECT_NEAR(tx->scale().y, 0.2F, 0.001F);
        EXPECT_NEAR(tx->scale().z, 2.0F, 0.001F);

        const auto* const mesh = registry.try_get<tempest::core::mesh_component>(step_entity);
        ASSERT_NE(mesh, nullptr);
        EXPECT_EQ(mesh->mesh_id, tempest::assets::builtin::box_mesh_guid);

        const auto* const mat = registry.try_get<tempest::core::material_component>(step_entity);
        ASSERT_NE(mat, nullptr);
        EXPECT_EQ(mat->material_id, tempest::assets::builtin::default_material_guid);

        const auto* const rb = registry.try_get<tempest::physics::rigid_body_component>(step_entity);
        ASSERT_NE(rb, nullptr);
        EXPECT_EQ(rb->motion, jolt::shim::motion_type::static_motion);
        EXPECT_EQ(rb->layer, jolt::shim::object_layer::non_moving);

        const auto* const box = registry.try_get<tempest::physics::box_collider_component>(step_entity);
        ASSERT_NE(box, nullptr);
        EXPECT_NEAR(box->half_extents.x, 1.0F, 0.001F);
        EXPECT_NEAR(box->half_extents.y, 0.1F, 0.001F);
        EXPECT_NEAR(box->half_extents.z, 1.0F, 0.001F);
        EXPECT_NEAR(box->offset.y, 0.0F, 0.001F);
    }

    // Verify CapsuleProxyTarget entity components
    {
        const auto* const tx = registry.try_get<tempest::ecs::transform_component>(capsule_entity);
        ASSERT_NE(tx, nullptr);
        EXPECT_NEAR(tx->position().x, 0.0F, 0.001F);
        EXPECT_NEAR(tx->position().y, 0.92F, 0.001F);
        EXPECT_NEAR(tx->position().z, 2.5F, 0.001F);

        const auto* const mesh = registry.try_get<tempest::core::mesh_component>(capsule_entity);
        ASSERT_NE(mesh, nullptr);
        EXPECT_EQ(mesh->mesh_id, tempest::assets::builtin::capsule_mesh_guid);

        const auto* const mat = registry.try_get<tempest::core::material_component>(capsule_entity);
        ASSERT_NE(mat, nullptr);
        EXPECT_EQ(mat->material_id, tempest::assets::builtin::default_material_guid);

        const auto* const hist = registry.try_get<tempest::ecs::transform_history_component>(capsule_entity);
        ASSERT_NE(hist, nullptr);
        EXPECT_NEAR(hist->current_position.x, 0.0F, 0.001F);
        EXPECT_NEAR(hist->current_position.y, 0.92F, 0.001F);
        EXPECT_NEAR(hist->current_position.z, 2.5F, 0.001F);

        // Act: Run interpolation (as happens during play mode frames)
        tempest::ecs::interpolate_transform_history(registry, 0.5F);

        // Assert: Transform position is preserved and not reset to origin
        const auto* const interpolated_tx = registry.try_get<tempest::ecs::transform_component>(capsule_entity);
        ASSERT_NE(interpolated_tx, nullptr);
        EXPECT_NEAR(interpolated_tx->position().x, 0.0F, 0.001F);
        EXPECT_NEAR(interpolated_tx->position().y, 0.92F, 0.001F);
        EXPECT_NEAR(interpolated_tx->position().z, 2.5F, 0.001F);
    }
}

// =========================================================================
// SECTION: Fallback Scene Active Camera Component Verification
// =========================================================================

/// @brief Verifies that active_camera_component is properly registered via register_engine_types()
/// and correctly instantiated into the archetype store during scene deserialization from fallback JSON,
/// evaluating to true for both registry.has and registry.try_get != nullptr.
TEST(main_scene_test, fallback_scene_active_camera_component_instantiation_and_query)
{
    // Setup ECS registry using only register_engine_types (no manual type registrations)
    auto event_reg = tempest::event::event_registry{};
    auto type_reg = tempest::ecs::component_type_registry{};
    tempest::register_engine_types(type_reg);

    auto registry = tempest::ecs::basic_archetype_registry{event_reg, type_reg};
    auto manager = tempest::scene::scene_manager{registry};

    static constexpr tempest::string_view fallback_scene_json =
        "{\n"
        "  \"version\": 1,\n"
        "  \"entities\": [\n"
        "    {\n"
        "      \"id\": \"0x0000000000000001\",\n"
        "      \"name\": \"Main Camera\",\n"
        "      \"components\": {\n"
        "        \"transform_component\": {\n"
        "          \"position\": [0.0, 1.6, -5.0],\n"
        "          \"rotation\": [0.0, 0.0, 0.0, 1.0],\n"
        "          \"scale\": [1.0, 1.0, 1.0]\n"
        "        },\n"
        "        \"camera_component\": {\n"
        "          \"aspect_ratio\": 1.7777778,\n"
        "          \"vertical_fov\": 1.0471976,\n"
        "          \"near_plane\": 0.05,\n"
        "          \"far_plane\": 1000.0,\n"
        "          \"target\": \"Viewport\",\n"
        "          \"is_active\": true\n"
        "        },\n"
        "        \"active_camera_component\": {}\n"
        "      }\n"
        "    },\n"
        "    {\n"
        "      \"id\": \"0x0000000000000002\",\n"
        "      \"name\": \"Sun Light\",\n"
        "      \"components\": {\n"
        "        \"transform_component\": {\n"
        "          \"position\": [0.0, 5.0, 0.0],\n"
        "          \"rotation\": [0.52456346, 0.18254332, -0.11629292, 0.8233997],\n"
        "          \"scale\": [1.0, 1.0, 1.0]\n"
        "        },\n"
        "        \"directional_light_component\": {\n"
        "          \"color\": [1.0, 0.98, 0.95],\n"
        "          \"intensity\": 2.0\n"
        "        }\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    // Act: Deserialization from JSON string
    const auto load_result = manager.load_from_string_sync(fallback_scene_json, "FallbackScene");
    ASSERT_TRUE(load_result.has_value());

    // Assert: Camera entity discovery and active_camera_component validation
    const auto camera_opt = registry.find_first_with_name("Main Camera");
    ASSERT_TRUE(camera_opt.has_value());
    const auto camera_entity = *camera_opt;

    // Verify entity validity
    EXPECT_TRUE(registry.is_valid(camera_entity));

    // Verify active_camera_component presence via has<T>
    EXPECT_TRUE(registry.has<tempest::render_system::active_camera_component>(camera_entity));

    // Verify active_camera_component pointer via try_get<T> != nullptr
    const auto* const active_cam_ptr = registry.try_get<tempest::render_system::active_camera_component>(camera_entity);
    EXPECT_NE(active_cam_ptr, nullptr);

    // Verify transform and camera components are also properly instantiated
    EXPECT_TRUE(registry.has<tempest::ecs::transform_component>(camera_entity));
    EXPECT_TRUE(registry.has<tempest::render_system::camera_component>(camera_entity));

    const auto* const cam_comp = registry.try_get<tempest::render_system::camera_component>(camera_entity);
    ASSERT_NE(cam_comp, nullptr);
    EXPECT_TRUE(cam_comp->is_active);
    EXPECT_EQ(cam_comp->target, tempest::render_system::camera_target_type::viewport);
    EXPECT_NEAR(cam_comp->aspect_ratio, 1.7777778F, 0.0001F);
    EXPECT_NEAR(cam_comp->vertical_fov, 1.0471976F, 0.0001F);
    EXPECT_NEAR(cam_comp->near_plane, 0.05F, 0.0001F);
    EXPECT_NEAR(cam_comp->far_plane, 1000.0F, 0.0001F);
}

