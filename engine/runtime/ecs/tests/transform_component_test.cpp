#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/event.hpp>
#include <tempest/mat4.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

using tempest::ecs::archetype_registry;
using tempest::ecs::create_parent_child_relationship;
using tempest::ecs::relationship_component;
using tempest::ecs::transform_component;
using tempest::math::as_radians;
using tempest::math::fmat4;
using tempest::math::vec3;
using tempest::math::vec4;

// ============================================================================
// Section: Component Invariants & Initialization
// ============================================================================

/// @brief Tests that transform_component::identity() initializes to identity transform.
TEST(transform_component_test, identity_invariants)
{
    // 1. Setup & Act
    const auto tx = transform_component::identity();

    // 2. Assert
    EXPECT_FLOAT_EQ(tx.position().x, 0.0F);
    EXPECT_FLOAT_EQ(tx.position().y, 0.0F);
    EXPECT_FLOAT_EQ(tx.position().z, 0.0F);

    EXPECT_FLOAT_EQ(tx.rotation().x, 0.0F);
    EXPECT_FLOAT_EQ(tx.rotation().y, 0.0F);
    EXPECT_FLOAT_EQ(tx.rotation().z, 0.0F);

    EXPECT_FLOAT_EQ(tx.scale().x, 1.0F);
    EXPECT_FLOAT_EQ(tx.scale().y, 1.0F);
    EXPECT_FLOAT_EQ(tx.scale().z, 1.0F);

    const auto m = tx.matrix();
    const auto expected_identity = fmat4{1.0F};
    for (size_t col = 0; col < 4; ++col)
    {
        for (size_t row = 0; row < 4; ++row)
        {
            EXPECT_FLOAT_EQ(m[col][row], expected_identity[col][row]);
        }
    }
}

// ============================================================================
// Section: Mutator Matrix Updates
// ============================================================================

/// @brief Tests that mutating position, rotation, and scale properly updates the cached matrix.
TEST(transform_component_test, mutators_update_cached_matrix)
{
    // 1. Setup
    auto tx = transform_component::identity();
    const auto new_pos = vec3<float>{5.0F, 10.0F, -15.0F};
    const auto new_rot = vec3<float>{as_radians(30.0F), as_radians(45.0F), 0.0F};
    const auto new_scale = vec3<float>{2.0F, 2.0F, 2.0F};

    // 2. Act
    tx.position(new_pos);
    tx.rotation(new_rot);
    tx.scale(new_scale);

    // 3. Assert
    const auto expected_matrix = tempest::math::transform(new_pos, new_rot, new_scale);
    const auto actual_matrix = tx.matrix();

    for (size_t col = 0; col < 4; ++col)
    {
        for (size_t row = 0; row < 4; ++row)
        {
            EXPECT_NEAR(actual_matrix[col][row], expected_matrix[col][row], 1e-4F);
        }
    }
}

// ============================================================================
// Section: Registry Storage & Hierarchy Traversal
// ============================================================================

/// @brief Tests assigning transform_component in an archetype_registry and computing world matrix.
TEST(transform_component_test, hierarchy_world_matrix_computation)
{
    // 1. Setup
    auto events = tempest::event::event_registry{};
    auto registry = archetype_registry{events};

    const auto parent_ent = registry.create();
    auto parent_tx = transform_component::identity();
    parent_tx.position(vec3<float>{10.0F, 0.0F, 0.0F});
    registry.assign(parent_ent, parent_tx);

    const auto child_ent = registry.create();
    auto child_tx = transform_component::identity();
    child_tx.position(vec3<float>{0.0F, 5.0F, 0.0F});
    registry.assign(child_ent, child_tx);

    create_parent_child_relationship(registry, parent_ent, child_ent);

    // 2. Act - Compute world matrix by traversing hierarchy
    auto world = fmat4{1.0F};
    auto curr = child_ent;
    while (curr != tempest::ecs::tombstone)
    {
        if (const auto* const tx = registry.try_get<transform_component>(curr))
        {
            world = tx->matrix() * world;
        }
        if (const auto* const rel = registry.try_get<relationship_component<tempest::ecs::entity>>(curr))
        {
            curr = rel->parent;
        }
        else
        {
            break;
        }
    }

    // 3. Assert - Child at local (0, 5, 0) under parent at (10, 0, 0) should be at world (10, 5, 0)
    const auto origin = vec4<float>{0.0F, 0.0F, 0.0F, 1.0F};
    const auto world_pos = world * origin;

    EXPECT_NEAR(world_pos.x, 10.0F, 1e-4F);
    EXPECT_NEAR(world_pos.y, 5.0F, 1e-4F);
    EXPECT_NEAR(world_pos.z, 0.0F, 1e-4F);
    EXPECT_FLOAT_EQ(world_pos.w, 1.0F);
}
