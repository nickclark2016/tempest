#include <tempest/archetype.hpp>

#include <gtest/gtest.h>
#include <tempest/component_type_registry.hpp>
#include <tempest/ecs_events.hpp>
#include <tempest/utility.hpp>

namespace
{
    struct position_component
    {
        float x{0.0F};
        float y{0.0F};
        float z{0.0F};
    };

    struct velocity_component
    {
        float vx{0.0F};
        float vy{0.0F};
        float vz{0.0F};
    };

    struct health_component
    {
        int max_hp{100};
        int current_hp{100};
    };
} // namespace

// ============================================================================
// ECS Registry Snapshot & Restore Tests
// ============================================================================

/// @brief Verifies that taking an ECS snapshot and restoring it returns all entity handles,
/// versions, components, and free list state to their exact pre-snapshot state.
TEST(registry_snapshot_test, snapshot_restore_bit_identical_entities_and_versions)
{
    // Setup: Initialize registries and populate entities
    auto events = tempest::event::event_registry{};
    auto types = tempest::ecs::component_type_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{events, types};

    const auto e1 = reg.create<position_component, velocity_component>();
    reg.assign_or_replace(e1, position_component{.x = 10.0F, .y = 20.0F, .z = 30.0F});
    reg.assign_or_replace(e1, velocity_component{.vx = 1.0F, .vy = 2.0F, .vz = 3.0F});

    const auto e2 = reg.create<position_component, health_component>();
    reg.assign_or_replace(e2, position_component{.x = 100.0F, .y = 200.0F, .z = 300.0F});
    reg.assign_or_replace(e2, health_component{.max_hp = 50, .current_hp = 42});

    const auto e3_to_destroy = reg.create<position_component>();
    reg.destroy(e3_to_destroy);

    const auto e4 = reg.create<health_component>();
    reg.assign_or_replace(e4, health_component{.max_hp = 200, .current_hp = 180});

    // Act: Capture snapshot
    auto snap = reg.snapshot();

    // Mutate world: create new entities, modify existing, destroy existing
    const auto session_e1 = reg.create<position_component>();
    reg.assign_or_replace(session_e1, position_component{.x = 999.0F, .y = 999.0F, .z = 999.0F});

    reg.assign_or_replace(e1, position_component{.x = 0.0F, .y = 0.0F, .z = 0.0F});
    reg.destroy(e2);

    // Restore snapshot
    reg.restore(tempest::move(snap));

    // Assert: Verify pre-snapshot state is completely restored
    EXPECT_TRUE(reg.is_valid(e1));
    EXPECT_TRUE(reg.is_valid(e2));
    EXPECT_FALSE(reg.is_valid(session_e1));

    const auto* pos1 = reg.try_get<position_component>(e1);
    ASSERT_NE(pos1, nullptr);
    EXPECT_FLOAT_EQ(pos1->x, 10.0F);
    EXPECT_FLOAT_EQ(pos1->y, 20.0F);
    EXPECT_FLOAT_EQ(pos1->z, 30.0F);

    const auto* vel1 = reg.try_get<velocity_component>(e1);
    ASSERT_NE(vel1, nullptr);
    EXPECT_FLOAT_EQ(vel1->vx, 1.0F);
    EXPECT_FLOAT_EQ(vel1->vy, 2.0F);
    EXPECT_FLOAT_EQ(vel1->vz, 3.0F);

    const auto* pos2 = reg.try_get<position_component>(e2);
    ASSERT_NE(pos2, nullptr);
    EXPECT_FLOAT_EQ(pos2->x, 100.0F);
    EXPECT_FLOAT_EQ(pos2->y, 200.0F);
    EXPECT_FLOAT_EQ(pos2->z, 300.0F);

    const auto* hp2 = reg.try_get<health_component>(e2);
    ASSERT_NE(hp2, nullptr);
    EXPECT_EQ(hp2->max_hp, 50);
    EXPECT_EQ(hp2->current_hp, 42);

    const auto* hp4 = reg.try_get<health_component>(e4);
    ASSERT_NE(hp4, nullptr);
    EXPECT_EQ(hp4->max_hp, 200);
    EXPECT_EQ(hp4->current_hp, 180);
}

/// @brief Verifies that taking a snapshot preserves the free-list progression deterministically.
TEST(registry_snapshot_test, snapshot_restore_preserves_free_list_determinism)
{
    // Setup: Initialize registry and build free list with version advances
    auto events = tempest::event::event_registry{};
    auto types = tempest::ecs::component_type_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{events, types};

    const auto e1 = reg.create();
    const auto e2 = reg.create();
    const auto e3 = reg.create();

    reg.destroy(e2);

    // Snapshot captured when e2 is in free list
    auto snap = reg.snapshot();

    // Mutate: consume free list, destroy more, create new
    const auto recycled = reg.create();
    [[maybe_unused]] const auto fresh = reg.create();
    reg.destroy(e1);
    reg.destroy(e3);

    // Restore
    reg.restore(tempest::move(snap));

    // Assert: Next creation from restored state must recycle e2 with matching version advance
    const auto restored_recycled = reg.create();
    EXPECT_EQ(restored_recycled, recycled);
}

/// @brief Verifies that entity names are properly snapshotted and restored.
TEST(registry_snapshot_test, snapshot_restore_preserves_names_table)
{
    // Setup: Create named entities
    auto events = tempest::event::event_registry{};
    auto types = tempest::ecs::component_type_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{events, types};

    const auto player = reg.create();
    reg.name(player, "Player");

    const auto boss = reg.create();
    reg.name(boss, "Boss");

    // Act: Take snapshot
    auto snap = reg.snapshot();

    // Mutate during session: rename player, create enemy with name, destroy boss
    reg.name(player, "GhostPlayer");
    reg.destroy(boss);
    const auto enemy = reg.create();
    reg.name(enemy, "Minion");

    // Restore
    reg.restore(tempest::move(snap));

    // Assert: Names match pre-snapshot state exactly
    const auto found_player = reg.find_first_with_name("Player");
    ASSERT_TRUE(found_player.has_value());
    EXPECT_EQ(*found_player, player);

    const auto found_boss = reg.find_first_with_name("Boss");
    ASSERT_TRUE(found_boss.has_value());
    EXPECT_EQ(*found_boss, boss);

    const auto found_minion = reg.find_first_with_name("Minion");
    EXPECT_FALSE(found_minion.has_value());

    const auto found_ghost = reg.find_first_with_name("GhostPlayer");
    EXPECT_FALSE(found_ghost.has_value());
}

/// @brief Verifies that restore suppresses individual entity creation/destruction events
/// and dispatches exactly one world_restored_event.
TEST(registry_snapshot_test, restore_suppresses_individual_events_and_dispatches_world_restored)
{
    // Setup: Initialize registries and subscribe to lifecycle events
    auto events = tempest::event::event_registry{};
    auto types = tempest::ecs::component_type_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{events, types};

    auto world_restored_count = 0;
    auto entity_created_count = 0;
    auto entity_destroyed_count = 0;

    [[maybe_unused]] const auto h_restore =
        events.dispatcher<tempest::ecs::world_restored_event>().subscribe(
            [&](const auto&) { ++world_restored_count; });

    [[maybe_unused]] const auto h_create =
        events.dispatcher<tempest::ecs::entity_created_event<tempest::ecs::entity>>().subscribe(
            [&](const auto&) { ++entity_created_count; });

    [[maybe_unused]] const auto h_destroy =
        events.dispatcher<tempest::ecs::entity_destroyed_event<tempest::ecs::entity>>().subscribe(
            [&](const auto&) { ++entity_destroyed_count; });

    const auto ent1 = reg.create<position_component>();
    [[maybe_unused]] const auto ent2 = reg.create<health_component>();

    auto snap = reg.snapshot();

    // Mutate world
    [[maybe_unused]] const auto extra = reg.create<position_component>();
    reg.destroy(ent1);

    // Reset event tracking counters before restore
    world_restored_count = 0;
    entity_created_count = 0;
    entity_destroyed_count = 0;

    // Act: Restore snapshot
    reg.restore(tempest::move(snap));

    // Assert: Only world_restored_event was published; individual lifecycle events were suppressed
    EXPECT_EQ(world_restored_count, 1);
    EXPECT_EQ(entity_created_count, 0);
    EXPECT_EQ(entity_destroyed_count, 0);
}
