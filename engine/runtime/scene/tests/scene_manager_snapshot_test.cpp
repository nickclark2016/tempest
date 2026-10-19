#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/string_view.hpp>
#include <tempest/utility.hpp>

namespace
{
    struct test_context
    {
        tempest::event::event_registry event_reg;
        tempest::ecs::component_type_registry type_reg;
        tempest::ecs::basic_archetype_registry registry{event_reg, type_reg};
        tempest::scene::scene_manager manager{registry};
    };
} // namespace

// ============================================================================
// Scene Manager Snapshot & Restore Tests
// ============================================================================

/// @brief Verifies that taking a snapshot of scene_manager captures loaded scenes,
/// active scene handle, dirty states, and occupied count, and that restoring recovers them accurately.
TEST(scene_manager_snapshot_test, snapshot_and_restore_scenes_and_active_scene)
{
    // Setup: Initialize context and load two scenes
    auto ctx = test_context{};

    const auto scene_a_json = tempest::string_view{R"({
        "version": 1,
        "entities": []
    })"};

    const auto scene_b_json = tempest::string_view{R"({
        "version": 1,
        "entities": []
    })"};

    const auto opts = tempest::scene::scene_load_options{
        .mode = tempest::scene::scene_load_mode::additive,
        .clear_first = false,
    };

    const auto scene_a_opt = ctx.manager.load_from_string_sync(scene_a_json, "SceneA", opts);
    ASSERT_TRUE(scene_a_opt.has_value());
    const auto scene_a = *scene_a_opt;

    const auto scene_b_opt = ctx.manager.load_from_string_sync(scene_b_json, "SceneB", opts);
    ASSERT_TRUE(scene_b_opt.has_value());
    const auto scene_b = *scene_b_opt;

    ctx.manager.set_active_scene(scene_b);
    ctx.manager.set_dirty(scene_a, true);

    EXPECT_EQ(ctx.manager.get_scene_count(), 2U);
    EXPECT_EQ(ctx.manager.get_active_scene(), scene_b);
    EXPECT_TRUE(ctx.manager.is_dirty(scene_a));
    EXPECT_FALSE(ctx.manager.is_dirty(scene_b));

    // Act: Capture snapshot
    auto snap = ctx.manager.snapshot();

    // Mutate state during simulated session: unload SceneA, load SceneC, change active
    ctx.manager.unload(scene_a);
    EXPECT_FALSE(ctx.manager.is_valid(scene_a));

    const auto scene_c_opt = ctx.manager.load_from_string_sync(scene_a_json, "SceneC", opts);
    ASSERT_TRUE(scene_c_opt.has_value());
    const auto scene_c = *scene_c_opt;
    ctx.manager.set_active_scene(scene_c);

    EXPECT_EQ(ctx.manager.get_active_scene(), scene_c);

    // Restore snapshot
    ctx.manager.restore(tempest::move(snap));

    // Assert: Pre-snapshot scenes, active scene, and dirty flags are restored
    EXPECT_TRUE(ctx.manager.is_valid(scene_a));
    EXPECT_TRUE(ctx.manager.is_valid(scene_b));
    EXPECT_FALSE(ctx.manager.is_valid(scene_c));

    EXPECT_EQ(ctx.manager.get_scene_count(), 2U);
    EXPECT_EQ(ctx.manager.get_active_scene(), scene_b);
    EXPECT_TRUE(ctx.manager.is_dirty(scene_a));
    EXPECT_FALSE(ctx.manager.is_dirty(scene_b));
}

/// @brief Verifies that has_in_flight_loads accurately reports zero pending loads in synchronous usage.
TEST(scene_manager_snapshot_test, has_in_flight_loads_synchronous_reporting)
{
    // Setup: Initialize context
    auto ctx = test_context{};

    // Assert: Initially no loads in flight
    EXPECT_FALSE(ctx.manager.has_in_flight_loads());

    // Act: Load scene synchronously
    const auto scene_json = tempest::string_view{R"({
        "version": 1,
        "entities": []
    })"};
    const auto scene_opt = ctx.manager.load_from_string_sync(scene_json, "SyncScene");
    ASSERT_TRUE(scene_opt.has_value());

    // Assert: Synchronous load finishes immediately, so in-flight load count remains zero
    EXPECT_FALSE(ctx.manager.has_in_flight_loads());
}

/// @brief Verifies that restoring a snapshot cancels any in-flight asynchronous operations.
TEST(scene_manager_snapshot_test, restore_cancels_in_flight_async_ops)
{
    // Setup: Initialize context and capture initial snapshot
    auto ctx = test_context{};
    auto snap = ctx.manager.snapshot();

    // Act: Start an async load
    const auto scene_json = tempest::string_view{R"({
        "version": 1,
        "entities": []
    })"};
    const auto options = tempest::scene::scene_async_load_options{
        .entities_per_frame = 10,
        .mode = tempest::scene::scene_load_mode::additive,
    };
    const auto load_handle = ctx.manager.load_from_string_async(scene_json, "AsyncScene", options);
    ASSERT_TRUE(load_handle.is_valid());
    EXPECT_TRUE(ctx.manager.has_in_flight_loads());

    // Act: Restore snapshot
    ctx.manager.restore(tempest::move(snap));

    // Assert: In-flight load is cancelled and has_in_flight_loads returns false
    EXPECT_FALSE(ctx.manager.has_in_flight_loads());
    EXPECT_TRUE(load_handle.is_cancelled());
}

