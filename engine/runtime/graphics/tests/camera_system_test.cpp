#include <tempest/camera_system.hpp>

#include <tempest/archetype.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/transform_component.hpp>

#include <gtest/gtest.h>

namespace tempest::graphics::tests
{
    // =========================================================================
    // Camera System Discovery & Fallback Tests
    // =========================================================================

    /// @brief Verifies that camera_system automatically discovers and resolves the single
    /// active viewport camera present in the scene when no explicit possession is set.
    TEST(camera_system, single_camera_fallback)
    {
        // 1. Setup Registry & Camera System
        auto events = event::event_registry();
        auto registry = ecs::registry(events);
        camera_system sys(registry, events);

        // 2. Create Single Scene Camera
        const auto cam1 = registry.create();
        registry.assign(cam1, ecs::transform_component::identity());
        registry.assign(cam1, camera_component{
                                  .aspect_ratio = 16.0F / 9.0F,
                                  .vertical_fov = math::as_radians(60.0F),
                                  .near_plane = 0.1F,
                              });

        // 3. Assert Discovery
        const auto active_entity = sys.get_active_camera_entity();
        ASSERT_TRUE(active_entity.has_value());
        EXPECT_EQ(active_entity.value(), cam1);

        const auto render_cam_opt = sys.get_active_camera();
        ASSERT_TRUE(render_cam_opt.has_value());
    }

    /// @brief Verifies that camera_system switches active camera targets via explicit
    /// possession (set_active_camera) without archetype migration churn and reverts
    /// cleanly to fallback upon clear_active_camera().
    TEST(camera_system, camera_switching_via_set_active_camera)
    {
        // 1. Setup Registry & Three Cameras
        auto events = event::event_registry();
        auto registry = ecs::registry(events);
        camera_system sys(registry, events);

        const auto cam1 = registry.create();
        registry.assign(cam1, ecs::transform_component::identity());
        registry.assign(cam1, camera_component{
                                  .aspect_ratio = 1.0F,
                                  .vertical_fov = math::as_radians(45.0F),
                                  .near_plane = 0.1F,
                              });

        const auto cam2 = registry.create();
        registry.assign(cam2, ecs::transform_component::identity());
        registry.assign(cam2, camera_component{
                                  .aspect_ratio = 16.0F / 9.0F,
                                  .vertical_fov = math::as_radians(60.0F),
                                  .near_plane = 0.1F,
                              });

        const auto cam3 = registry.create();
        registry.assign(cam3, ecs::transform_component::identity());
        registry.assign(cam3, camera_component{
                                  .aspect_ratio = 4.0F / 3.0F,
                                  .vertical_fov = math::as_radians(90.0F),
                                  .near_plane = 0.5F,
                              });

        // 2. Default Fallback Resolves First Camera
        EXPECT_EQ(sys.get_active_camera_entity().value_or(ecs::entity{ecs::tombstone}), cam1);

        // 3. Explicitly Possess Camera 2
        sys.set_active_camera(cam2);
        EXPECT_EQ(sys.get_active_camera_entity().value_or(ecs::entity{ecs::tombstone}), cam2);

        // 4. Switch Possession to Camera 3
        sys.set_active_camera(cam3);
        EXPECT_EQ(sys.get_active_camera_entity().value_or(ecs::entity{ecs::tombstone}), cam3);

        // 5. Clear Possession -> Reverts to Fallback (Camera 1)
        sys.clear_active_camera();
        EXPECT_EQ(sys.get_active_camera_entity().value_or(ecs::entity{ecs::tombstone}), cam1);
    }

    /// @brief Verifies that inactive cameras (is_active = false) and offscreen render-to-texture
    /// cameras (target = render_texture) are excluded from viewport fallback resolution.
    TEST(camera_system, inactive_and_render_texture_cameras_ignored_in_fallback)
    {
        // 1. Setup Registry
        auto events = event::event_registry();
        auto registry = ecs::registry(events);
        camera_system sys(registry, events);

        // 2. Create Inactive Camera
        const auto cam_inactive = registry.create();
        registry.assign(cam_inactive, ecs::transform_component::identity());
        registry.assign(cam_inactive, camera_component{
                                          .aspect_ratio = 16.0F / 9.0F,
                                          .vertical_fov = math::as_radians(60.0F),
                                          .near_plane = 0.1F,
                                          .is_active = false,
                                      });

        // 3. Create Offscreen Render Texture Camera
        const auto cam_tex = registry.create();
        registry.assign(cam_tex, ecs::transform_component::identity());
        registry.assign(cam_tex, camera_component{
                                     .aspect_ratio = 1.0F,
                                     .vertical_fov = math::as_radians(90.0F),
                                     .near_plane = 0.1F,
                                     .target = camera_target_type::render_texture,
                                     .is_active = true,
                                 });

        // 4. Create Active Viewport Camera
        const auto cam_viewport = registry.create();
        registry.assign(cam_viewport, ecs::transform_component::identity());
        registry.assign(cam_viewport, camera_component{
                                          .aspect_ratio = 16.0F / 9.0F,
                                          .vertical_fov = math::as_radians(60.0F),
                                          .near_plane = 0.1F,
                                          .target = camera_target_type::viewport,
                                          .is_active = true,
                                      });

        // 5. Assert Viewport Fallback Correctly Selects the Active Viewport Camera
        const auto active_entity = sys.get_active_camera_entity();
        ASSERT_TRUE(active_entity.has_value());
        EXPECT_EQ(active_entity.value(), cam_viewport);
    }

    // =========================================================================
    // Matrix Computation Tests
    // =========================================================================

    /// @brief Verifies that camera_system generates valid perspective and view matrices
    /// reflecting the entity's transform_component position and rotation.
    TEST(camera_system, render_camera_matrix_computation)
    {
        // 1. Setup Camera Entity with Offset Position
        auto events = event::event_registry();
        auto registry = ecs::registry(events);
        camera_system sys(registry, events);

        const auto cam_entity = registry.create();
        auto transform = ecs::transform_component::identity();
        transform.position(math::vec3<float>(0.0F, 5.0F, -10.0F));
        transform.rotation(math::vec3<float>(0.0F, 0.0F, 0.0F));

        registry.assign(cam_entity, transform);
        registry.assign(cam_entity, camera_component{
                                        .aspect_ratio = 16.0F / 9.0F,
                                        .vertical_fov = math::as_radians(60.0F),
                                        .near_plane = 0.1F,
                                    });

        sys.set_active_camera(cam_entity);

        // 2. Query Render Camera Output
        const auto render_cam_opt = sys.get_active_camera();
        ASSERT_TRUE(render_cam_opt.has_value());

        const auto& render_cam = render_cam_opt.value();

        // 3. Eye Position Matches Transform Position
        EXPECT_FLOAT_EQ(render_cam.eye_position.x, 0.0F);
        EXPECT_FLOAT_EQ(render_cam.eye_position.y, 5.0F);
        EXPECT_FLOAT_EQ(render_cam.eye_position.z, -10.0F);
        EXPECT_FLOAT_EQ(render_cam.eye_position.w, 1.0F);

        // 4. Assert P * P^-1 == I
        const auto proj_identity = render_cam.proj * render_cam.inv_proj;
        for (std::size_t i = 0; i < 4; ++i)
        {
            for (std::size_t j = 0; j < 4; ++j)
            {
                const float expected = (i == j) ? 1.0F : 0.0F;
                EXPECT_NEAR(proj_identity[i][j], expected, 1e-4F);
            }
        }

        // 5. Assert V * V^-1 == I
        const auto view_identity = render_cam.view * render_cam.inv_view;
        for (std::size_t i = 0; i < 4; ++i)
        {
            for (std::size_t j = 0; j < 4; ++j)
            {
                const float expected = (i == j) ? 1.0F : 0.0F;
                EXPECT_NEAR(view_identity[i][j], expected, 1e-4F);
            }
        }
    }

    // =========================================================================
    // Edge Cases: Entity Destruction & Dynamic Mutation
    // =========================================================================

    /// @brief Verifies that destroying the currently possessed active camera entity
    /// is handled safely, falling back to remaining cameras without dangling pointer crashes.
    TEST(camera_system, camera_entity_destruction_fallback)
    {
        // 1. Setup Two Cameras
        auto events = event::event_registry();
        auto registry = ecs::registry(events);
        camera_system sys(registry, events);

        const auto cam1 = registry.create();
        registry.assign(cam1, ecs::transform_component::identity());
        registry.assign(cam1, camera_component{
                                  .aspect_ratio = 16.0F / 9.0F,
                                  .vertical_fov = math::as_radians(60.0F),
                                  .near_plane = 0.1F,
                              });

        const auto cam2 = registry.create();
        registry.assign(cam2, ecs::transform_component::identity());
        registry.assign(cam2, camera_component{
                                  .aspect_ratio = 4.0F / 3.0F,
                                  .vertical_fov = math::as_radians(45.0F),
                                  .near_plane = 0.1F,
                              });

        // 2. Possess Camera 2
        sys.set_active_camera(cam2);
        EXPECT_EQ(sys.get_active_camera_entity().value(), cam2);

        // 3. Destroy Possessed Entity (Camera 2)
        registry.destroy(cam2);

        // 4. Assert Fallback to Camera 1
        const auto active_opt = sys.get_active_camera_entity();
        ASSERT_TRUE(active_opt.has_value());
        EXPECT_EQ(active_opt.value(), cam1);

        // 5. Destroy Remaining Camera -> Safely Returns Nullopt
        registry.destroy(cam1);
        EXPECT_FALSE(sys.get_active_camera_entity().has_value());
        EXPECT_FALSE(sys.get_active_camera().has_value());
    }

    /// @brief Verifies that modifying camera is_active at runtime dynamically switches
    /// fallback resolution to the next available active camera.
    TEST(camera_system, multiple_viewport_cameras_dynamic_deactivation)
    {
        // 1. Setup Two Active Cameras
        auto events = event::event_registry();
        auto registry = ecs::registry(events);
        camera_system sys(registry, events);

        const auto cam1 = registry.create();
        registry.assign(cam1, ecs::transform_component::identity());
        registry.assign(cam1, camera_component{
                                  .aspect_ratio = 16.0F / 9.0F,
                                  .vertical_fov = math::as_radians(60.0F),
                                  .near_plane = 0.1F,
                                  .is_active = true,
                              });

        const auto cam2 = registry.create();
        registry.assign(cam2, ecs::transform_component::identity());
        registry.assign(cam2, camera_component{
                                  .aspect_ratio = 4.0F / 3.0F,
                                  .vertical_fov = math::as_radians(45.0F),
                                  .near_plane = 0.1F,
                                  .is_active = true,
                              });

        // 2. Initial Fallback Resolves Camera 1
        EXPECT_EQ(sys.get_active_camera_entity().value(), cam1);

        // 3. Deactivate Camera 1 -> Resolves Camera 2
        registry.replace(cam1, camera_component{
                                   .aspect_ratio = 16.0F / 9.0F,
                                   .vertical_fov = math::as_radians(60.0F),
                                   .near_plane = 0.1F,
                                   .is_active = false,
                               });
        EXPECT_EQ(sys.get_active_camera_entity().value(), cam2);

        // 4. Deactivate Camera 2 -> Resolves Nullopt
        registry.replace(cam2, camera_component{
                                   .aspect_ratio = 4.0F / 3.0F,
                                   .vertical_fov = math::as_radians(45.0F),
                                   .near_plane = 0.1F,
                                   .is_active = false,
                               });
        EXPECT_FALSE(sys.get_active_camera_entity().has_value());
    }

    /// @brief Verifies that when a possessed camera entity is destroyed and its raw slot index
    /// is recycled by a new entity without camera components, is_valid() protects against ABA false-positives.
    TEST(camera_system, recycled_slot_aba_protection)
    {
        // 1. Setup Camera System
        auto events = event::event_registry();
        auto registry = ecs::registry(events);
        camera_system sys(registry, events);

        // 2. Create and Possess Camera
        const auto cam = registry.create();
        registry.assign(cam, ecs::transform_component::identity());
        registry.assign(cam, camera_component{
                                 .aspect_ratio = 16.0F / 9.0F,
                                 .vertical_fov = math::as_radians(60.0F),
                                 .near_plane = 0.1F,
                             });

        sys.set_active_camera(cam);
        EXPECT_EQ(sys.get_active_camera_entity().value(), cam);

        // 3. Destroy Possessed Camera
        registry.destroy(cam);

        // 4. Allocate New Non-Camera Entity (May reuse slot index with new generation)
        const auto other_ent = registry.create();
        registry.assign(other_ent, ecs::transform_component::identity());

        // 5. Assert Old Possessed Handle Is Rejected (Returns Nullopt)
        EXPECT_FALSE(sys.get_active_camera_entity().has_value());
        EXPECT_FALSE(sys.get_active_camera().has_value());
    }
} // namespace tempest::graphics::tests
