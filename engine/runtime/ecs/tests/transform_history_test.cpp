#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/event.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/quat.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/transform_history_system.hpp>
#include <tempest/vec3.hpp>

using tempest::ecs::archetype_registry;
using tempest::ecs::interpolate_transform_history;
using tempest::ecs::step_transform_history;
using tempest::ecs::transform_component;
using tempest::ecs::transform_history_component;
using tempest::math::as_radians;
using tempest::math::fquat;
using tempest::math::vec3;

// ============================================================================
// Section: Component Invariants & Initialization
// ============================================================================

/// @brief Tests default initialization of transform_history_component fields.
TEST(transform_history_test, default_initialization)
{
    // 1. Setup & Act
    const auto history = transform_history_component::create();

    // 2. Assert
    EXPECT_FLOAT_EQ(history.previous_position.x, 0.0F);
    EXPECT_FLOAT_EQ(history.previous_position.y, 0.0F);
    EXPECT_FLOAT_EQ(history.previous_position.z, 0.0F);

    EXPECT_FLOAT_EQ(history.previous_rotation.x, 0.0F);
    EXPECT_FLOAT_EQ(history.previous_rotation.y, 0.0F);
    EXPECT_FLOAT_EQ(history.previous_rotation.z, 0.0F);
    EXPECT_FLOAT_EQ(history.previous_rotation.w, 1.0F);

    EXPECT_FLOAT_EQ(history.current_position.x, 0.0F);
    EXPECT_FLOAT_EQ(history.current_position.y, 0.0F);
    EXPECT_FLOAT_EQ(history.current_position.z, 0.0F);

    EXPECT_FLOAT_EQ(history.current_rotation.x, 0.0F);
    EXPECT_FLOAT_EQ(history.current_rotation.y, 0.0F);
    EXPECT_FLOAT_EQ(history.current_rotation.z, 0.0F);
    EXPECT_FLOAT_EQ(history.current_rotation.w, 1.0F);

    EXPECT_FLOAT_EQ(history.render_position.x, 0.0F);
    EXPECT_FLOAT_EQ(history.render_position.y, 0.0F);
    EXPECT_FLOAT_EQ(history.render_position.z, 0.0F);

    EXPECT_FLOAT_EQ(history.render_rotation.x, 0.0F);
    EXPECT_FLOAT_EQ(history.render_rotation.y, 0.0F);
    EXPECT_FLOAT_EQ(history.render_rotation.z, 0.0F);
    EXPECT_FLOAT_EQ(history.render_rotation.w, 1.0F);
}

// ============================================================================
// Section: History Stepping & Snapshotting
// ============================================================================

/// @brief Tests that step_transform_history snapshots current position and rotation to previous.
TEST(transform_history_test, step_history_snapshots_current_pose)
{
    // 1. Setup
    auto events = tempest::event::event_registry{};
    auto registry = archetype_registry{events};

    const auto entity = registry.create();
    auto initial_history = transform_history_component{
        .previous_position = vec3<float>{0.0F, 0.0F, 0.0F},
        .previous_rotation = fquat{0.0F, 0.0F, 0.0F, 1.0F},
        .current_position = vec3<float>{12.0F, -4.0F, 8.5F},
        .current_rotation = fquat{as_radians(vec3<float>{30.0F, 45.0F, 60.0F})},
    };
    registry.assign(entity, initial_history);

    // 2. Act
    step_transform_history(registry);

    // 3. Assert
    const auto* const updated_history = registry.try_get<transform_history_component>(entity);
    ASSERT_NE(updated_history, nullptr);

    EXPECT_FLOAT_EQ(updated_history->previous_position.x, 12.0F);
    EXPECT_FLOAT_EQ(updated_history->previous_position.y, -4.0F);
    EXPECT_FLOAT_EQ(updated_history->previous_position.z, 8.5F);

    EXPECT_NEAR(updated_history->previous_rotation.x, initial_history.current_rotation.x, 1e-5F);
    EXPECT_NEAR(updated_history->previous_rotation.y, initial_history.current_rotation.y, 1e-5F);
    EXPECT_NEAR(updated_history->previous_rotation.z, initial_history.current_rotation.z, 1e-5F);
    EXPECT_NEAR(updated_history->previous_rotation.w, initial_history.current_rotation.w, 1e-5F);
}

// ============================================================================
// Section: Render Interpolation & Monotonicity
// ============================================================================

/// @brief Tests that interpolate_transform_history yields monotonic linear progression for position.
TEST(transform_history_test, interpolate_monotonic_position_progression)
{
    // 1. Setup
    auto events = tempest::event::event_registry{};
    auto registry = archetype_registry{events};

    const auto entity = registry.create();
    auto history = transform_history_component{
        .previous_position = vec3<float>{0.0F, 0.0F, 0.0F},
        .current_position = vec3<float>{10.0F, 20.0F, 30.0F},
    };
    registry.assign(entity, history);

    // 2. Act & Assert across varying alpha values
    const auto alpha_steps = {0.0F, 0.25F, 0.5F, 0.75F, 1.0F};
    auto last_x = -1.0F;

    for (const auto alpha : alpha_steps)
    {
        interpolate_transform_history(registry, alpha);

        const auto* const comp = registry.try_get<transform_history_component>(entity);
        ASSERT_NE(comp, nullptr);

        EXPECT_FLOAT_EQ(comp->render_position.x, 10.0F * alpha);
        EXPECT_FLOAT_EQ(comp->render_position.y, 20.0F * alpha);
        EXPECT_FLOAT_EQ(comp->render_position.z, 30.0F * alpha);

        EXPECT_GT(comp->render_position.x, last_x);
        last_x = comp->render_position.x;
    }
}

/// @brief Tests that interpolate_transform_history performs spherical linear interpolation on rotation.
TEST(transform_history_test, interpolate_spherical_rotation)
{
    // 1. Setup - 0 deg rotation to 90 deg rotation around Y
    auto events = tempest::event::event_registry{};
    auto registry = archetype_registry{events};

    const auto entity = registry.create();
    const auto q_prev = tempest::math::normalize(fquat{as_radians(vec3<float>{0.0F, 0.0F, 0.0F})});
    const auto q_curr = tempest::math::normalize(fquat{as_radians(vec3<float>{0.0F, 90.0F, 0.0F})});

    auto history = transform_history_component{
        .previous_rotation = q_prev,
        .current_rotation = q_curr,
    };
    registry.assign(entity, history);

    // 2. Act - Interpolate halfway
    interpolate_transform_history(registry, 0.5F);

    // 3. Assert - Intermediate rotation must rotate forward (+Z) by 45 degrees
    const auto* const comp = registry.try_get<transform_history_component>(entity);
    ASSERT_NE(comp, nullptr);

    const auto forward = vec3<float>{0.0F, 0.0F, 1.0F};
    const auto rotated = comp->render_rotation * forward;
    const auto expected_rad = as_radians(45.0F);

    EXPECT_NEAR(rotated.x, tempest::math::sin(expected_rad), 1e-4F);
    EXPECT_NEAR(rotated.y, 0.0F, 1e-4F);
    EXPECT_NEAR(rotated.z, tempest::math::cos(expected_rad), 1e-4F);
}

/// @brief Tests that interpolate_transform_history synchronizes transform_component on the same entity.
TEST(transform_history_test, interpolate_synchronizes_transform_component)
{
    // 1. Setup
    auto events = tempest::event::event_registry{};
    auto registry = archetype_registry{events};

    const auto entity = registry.create();
    auto history = transform_history_component{
        .previous_position = vec3<float>{0.0F, 0.0F, 0.0F},
        .previous_rotation = fquat{0.0F, 0.0F, 0.0F, 1.0F},
        .current_position = vec3<float>{10.0F, 0.0F, 0.0F},
        .current_rotation = fquat{0.0F, 0.0F, 0.0F, 1.0F},
    };
    auto tx = transform_component::identity();

    registry.assign(entity, history);
    registry.assign(entity, tx);

    // 2. Act - Interpolate with alpha = 0.5
    interpolate_transform_history(registry, 0.5F);

    // 3. Assert - transform_component position and matrix must reflect (5, 0, 0)
    const auto* const synced_tx = registry.try_get<transform_component>(entity);
    ASSERT_NE(synced_tx, nullptr);

    EXPECT_FLOAT_EQ(synced_tx->position().x, 5.0F);
    EXPECT_FLOAT_EQ(synced_tx->position().y, 0.0F);
    EXPECT_FLOAT_EQ(synced_tx->position().z, 0.0F);

    const auto mat = synced_tx->matrix();
    EXPECT_FLOAT_EQ(mat[3][0], 5.0F);
    EXPECT_FLOAT_EQ(mat[3][1], 0.0F);
    EXPECT_FLOAT_EQ(mat[3][2], 0.0F);
}
