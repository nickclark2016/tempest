#include <tempest/camera_system.hpp>

#include <tempest/archetype.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/transform_component.hpp>

#include <gtest/gtest.h>

namespace tempest::graphics::tests
{
    TEST(camera_system, single_active_camera_enforcement_via_events)
    {
        auto events = event::event_registry();
        auto registry = ecs::registry(events);

        camera_system sys(registry, events);

        const auto cam1 = registry.create();
        registry.assign(cam1, ecs::transform_component::identity());
        registry.assign(cam1, camera_component{.aspect_ratio = 16.0f / 9.0f, .vertical_fov = math::as_radians(60.0f), .near_plane = 0.1f});

        const auto cam2 = registry.create();
        registry.assign(cam2, ecs::transform_component::identity());
        registry.assign(cam2, camera_component{.aspect_ratio = 16.0f / 9.0f, .vertical_fov = math::as_radians(60.0f), .near_plane = 0.1f});

        // Assign active tag to cam1
        registry.assign(cam1, active_camera_component{});
        EXPECT_TRUE(registry.has<active_camera_component>(cam1));
        EXPECT_FALSE(registry.has<active_camera_component>(cam2));
        EXPECT_EQ(sys.get_active_camera_entity().value_or(ecs::entity{ecs::tombstone}), cam1);

        // Assign active tag to cam2 -> event listener should strip active_camera_component from cam1
        registry.assign(cam2, active_camera_component{});
        EXPECT_TRUE(registry.has<active_camera_component>(cam2));
        EXPECT_FALSE(registry.has<active_camera_component>(cam1));
        EXPECT_EQ(sys.get_active_camera_entity().value_or(ecs::entity{ecs::tombstone}), cam2);
    }

    TEST(camera_system, camera_switching_via_set_active_camera)
    {
        auto events = event::event_registry();
        auto registry = ecs::registry(events);

        camera_system sys(registry, events);

        const auto cam1 = registry.create();
        registry.assign(cam1, ecs::transform_component::identity());
        registry.assign(cam1, camera_component{.aspect_ratio = 1.0f, .vertical_fov = math::as_radians(45.0f), .near_plane = 0.1f});

        const auto cam2 = registry.create();
        registry.assign(cam2, ecs::transform_component::identity());
        registry.assign(cam2, camera_component{.aspect_ratio = 16.0f / 9.0f, .vertical_fov = math::as_radians(60.0f), .near_plane = 0.1f});

        const auto cam3 = registry.create();
        registry.assign(cam3, ecs::transform_component::identity());
        registry.assign(cam3, camera_component{.aspect_ratio = 4.0f / 3.0f, .vertical_fov = math::as_radians(90.0f), .near_plane = 0.5f});

        // Set active camera to cam1
        sys.set_active_camera(cam1);
        EXPECT_EQ(sys.get_active_camera_entity().value_or(ecs::entity{ecs::tombstone}), cam1);
        EXPECT_TRUE(registry.has<active_camera_component>(cam1));
        EXPECT_FALSE(registry.has<active_camera_component>(cam2));
        EXPECT_FALSE(registry.has<active_camera_component>(cam3));

        // Switch active camera to cam3
        sys.set_active_camera(cam3);
        EXPECT_EQ(sys.get_active_camera_entity().value_or(ecs::entity{ecs::tombstone}), cam3);
        EXPECT_FALSE(registry.has<active_camera_component>(cam1));
        EXPECT_FALSE(registry.has<active_camera_component>(cam2));
        EXPECT_TRUE(registry.has<active_camera_component>(cam3));
    }

    TEST(camera_system, render_camera_matrix_computation)
    {
        auto events = event::event_registry();
        auto registry = ecs::registry(events);

        camera_system sys(registry, events);

        const auto cam_entity = registry.create();
        auto transform = ecs::transform_component::identity();
        transform.position(math::vec3<float>(0.0f, 5.0f, -10.0f));
        transform.rotation(math::vec3<float>(0.0f, 0.0f, 0.0f));

        registry.assign(cam_entity, transform);
        registry.assign(cam_entity, camera_component{
            .aspect_ratio = 16.0f / 9.0f,
            .vertical_fov = math::as_radians(60.0f),
            .near_plane = 0.1f,
        });

        sys.set_active_camera(cam_entity);

        const auto render_cam_opt = sys.get_active_camera();
        ASSERT_TRUE(render_cam_opt.has_value());

        const auto& render_cam = render_cam_opt.value();

        // Eye position should match transform position with w = 1.0f
        EXPECT_FLOAT_EQ(render_cam.eye_position.x, 0.0f);
        EXPECT_FLOAT_EQ(render_cam.eye_position.y, 5.0f);
        EXPECT_FLOAT_EQ(render_cam.eye_position.z, -10.0f);
        EXPECT_FLOAT_EQ(render_cam.eye_position.w, 1.0f);

        // Verify proj * inv_proj is identity matrix (within epsilon)
        const auto proj_identity = render_cam.proj * render_cam.inv_proj;
        for (std::size_t i = 0; i < 4; ++i)
        {
            for (std::size_t j = 0; j < 4; ++j)
            {
                const float expected = (i == j) ? 1.0f : 0.0f;
                EXPECT_NEAR(proj_identity[i][j], expected, 1e-4f);
            }
        }

        // Verify view * inv_view is identity matrix (within epsilon)
        const auto view_identity = render_cam.view * render_cam.inv_view;
        for (std::size_t i = 0; i < 4; ++i)
        {
            for (std::size_t j = 0; j < 4; ++j)
            {
                const float expected = (i == j) ? 1.0f : 0.0f;
                EXPECT_NEAR(view_identity[i][j], expected, 1e-4f);
            }
        }
    }

    TEST(camera_system, fallback_to_first_camera_when_no_active_tag)
    {
        auto events = event::event_registry();
        auto registry = ecs::registry(events);

        camera_system sys(registry, events);

        const auto cam1 = registry.create();
        registry.assign(cam1, ecs::transform_component::identity());
        registry.assign(cam1, camera_component{.aspect_ratio = 16.0f / 9.0f, .vertical_fov = math::as_radians(60.0f), .near_plane = 0.1f});

        const auto cam2 = registry.create();
        registry.assign(cam2, ecs::transform_component::identity());
        registry.assign(cam2, camera_component{.aspect_ratio = 4.0f / 3.0f, .vertical_fov = math::as_radians(45.0f), .near_plane = 0.1f});

        // Neither camera has active_camera_component tag
        EXPECT_FALSE(registry.has<active_camera_component>(cam1));
        EXPECT_FALSE(registry.has<active_camera_component>(cam2));

        // get_active_camera_entity and get_active_camera should return the first camera entity (cam1)
        const auto active_entity = sys.get_active_camera_entity();
        ASSERT_TRUE(active_entity.has_value());
        EXPECT_EQ(active_entity.value(), cam1);

        const auto render_cam_opt = sys.get_active_camera();
        ASSERT_TRUE(render_cam_opt.has_value());
    }

    TEST(camera_system, fallback_when_active_entity_lacks_camera_component)
    {
        auto events = event::event_registry();
        auto registry = ecs::registry(events);

        camera_system sys(registry, events);

        // Create an entity tagged with active_camera_component but missing camera_component
        const auto dummy_entity = registry.create();
        registry.assign(dummy_entity, ecs::transform_component::identity());
        registry.assign(dummy_entity, active_camera_component{});

        // Create a valid camera entity (untagged)
        const auto valid_cam = registry.create();
        registry.assign(valid_cam, ecs::transform_component::identity());
        registry.assign(valid_cam, camera_component{.aspect_ratio = 16.0f / 9.0f, .vertical_fov = math::as_radians(60.0f), .near_plane = 0.1f});

        // get_active_camera_entity and get_active_camera should fall back to valid_cam
        const auto active_entity = sys.get_active_camera_entity();
        ASSERT_TRUE(active_entity.has_value());
        EXPECT_EQ(active_entity.value(), valid_cam);

        const auto render_cam_opt = sys.get_active_camera();
        ASSERT_TRUE(render_cam_opt.has_value());
    }

    TEST(camera_system, unregister_on_destruction)
    {
        auto events = event::event_registry();
        auto registry = ecs::registry(events);

        const auto cam1 = registry.create();
        registry.assign(cam1, ecs::transform_component::identity());
        registry.assign(cam1, camera_component{.aspect_ratio = 16.0f / 9.0f, .vertical_fov = math::as_radians(60.0f), .near_plane = 0.1f});

        const auto cam2 = registry.create();
        registry.assign(cam2, ecs::transform_component::identity());
        registry.assign(cam2, camera_component{.aspect_ratio = 16.0f / 9.0f, .vertical_fov = math::as_radians(60.0f), .near_plane = 0.1f});

        {
            camera_system sys(registry, events);
            registry.assign(cam1, active_camera_component{});
            EXPECT_TRUE(registry.has<active_camera_component>(cam1));
        }

        // After sys destruction, assigning active tag to cam2 should NOT strip active tag from cam1
        registry.assign(cam2, active_camera_component{});
        EXPECT_TRUE(registry.has<active_camera_component>(cam1));
        EXPECT_TRUE(registry.has<active_camera_component>(cam2));
    }
} // namespace tempest::graphics::tests
