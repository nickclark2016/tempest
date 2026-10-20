#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/ecs_events.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/hierarchy_cache.hpp>
#include <tempest/play_mode_controller.hpp>
#include <tempest/play_session_diff.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_manager.hpp>

namespace tempest::editor::tests
{
    // =========================================================================
    // Play Session Diff Unit Tests
    // =========================================================================

    /// @brief Verifies that play_session_diff tracks newly spawned entities when entity_created_event is published.
    TEST(play_session_diff_test, tracks_spawned_entities_on_creation_event)
    {
        // Setup: Event registry and play session diff tracker
        auto events = event::event_registry();
        auto diff = play_session_diff();
        diff.attach(events);

        const auto entity_alpha = static_cast<ecs::entity>(10);
        const auto entity_beta = static_cast<ecs::entity>(20);

        // Act: Publish entity creation events
        events.dispatcher<ecs::entity_created_event<ecs::entity>>().publish({
            .entity = entity_alpha,
        });
        events.dispatcher<ecs::entity_created_event<ecs::entity>>().publish({
            .entity = entity_beta,
        });

        // Assert: Both entities are tracked as spawned
        EXPECT_TRUE(diff.is_spawned(entity_alpha));
        EXPECT_TRUE(diff.is_spawned(entity_beta));
        EXPECT_FALSE(diff.is_spawned(static_cast<ecs::entity>(30)));

        const auto spawned = diff.get_spawned_entities();
        EXPECT_EQ(spawned.size(), 2);
    }

    /// @brief Verifies that play_session_diff tracks component additions and removals accurately.
    TEST(play_session_diff_test, tracks_component_additions_and_removals)
    {
        // Setup: Event registry and play session diff tracker
        auto events = event::event_registry();
        auto diff = play_session_diff();
        diff.attach(events);

        const auto entity = static_cast<ecs::entity>(42);
        const auto component_type_a = 100u;
        const auto component_type_b = 200u;

        // Act: Publish component added events
        events.dispatcher<ecs::entity_component_added_event<ecs::entity>>().publish({
            .entity = entity,
            .component_type_index = component_type_a,
        });
        events.dispatcher<ecs::entity_component_added_event<ecs::entity>>().publish({
            .entity = entity,
            .component_type_index = component_type_b,
        });

        // Assert: Components recorded in added set
        EXPECT_TRUE(diff.is_component_added(entity, component_type_a));
        EXPECT_TRUE(diff.is_component_added(entity, component_type_b));
        EXPECT_FALSE(diff.is_component_added(entity, 300u));

        const auto added = diff.get_added_components(entity);
        EXPECT_EQ(added.size(), 2);

        // Act: Remove one component
        events.dispatcher<ecs::entity_component_removed_event<ecs::entity>>().publish({
            .entity = entity,
            .component_type_index = component_type_a,
        });

        // Assert: Removed component is no longer reported as added
        EXPECT_FALSE(diff.is_component_added(entity, component_type_a));
        EXPECT_TRUE(diff.is_component_added(entity, component_type_b));
        EXPECT_EQ(diff.get_added_components(entity).size(), 1);
    }

    /// @brief Verifies that destroying an entity clears its spawned state and component diff entries.
    TEST(play_session_diff_test, entity_destroyed_cleans_up_tracking)
    {
        // Setup: Track entity and its component additions
        auto events = event::event_registry();
        auto diff = play_session_diff();
        diff.attach(events);

        const auto entity = static_cast<ecs::entity>(77);
        const auto comp_type = 5u;

        events.dispatcher<ecs::entity_created_event<ecs::entity>>().publish({
            .entity = entity,
        });
        events.dispatcher<ecs::entity_component_added_event<ecs::entity>>().publish({
            .entity = entity,
            .component_type_index = comp_type,
        });

        ASSERT_TRUE(diff.is_spawned(entity));
        ASSERT_TRUE(diff.is_component_added(entity, comp_type));

        // Act: Destroy the entity
        events.dispatcher<ecs::entity_destroyed_event<ecs::entity>>().publish({
            .entity = entity,
        });

        // Assert: All tracking for the destroyed entity is purged
        EXPECT_FALSE(diff.is_spawned(entity));
        EXPECT_TRUE(diff.get_spawned_entities().empty());
        EXPECT_FALSE(diff.is_component_added(entity, comp_type));
        EXPECT_TRUE(diff.get_added_components(entity).empty());
    }

    /// @brief Verifies that hierarchy_cache marks spawned entities as dynamic during cache rebuild.
    TEST(play_session_diff_test, hierarchy_cache_marks_spawned_entities_dynamic)
    {
        // Setup: Registry, scene manager, event bus, and hierarchy cache
        auto events = event::event_registry();
        auto types = ecs::component_type_registry();
        auto reg = ecs::archetype_registry(events, types);
        auto scene_mgr = scene::scene_manager(reg);

        const auto scene_opt = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "Test Scene");
        ASSERT_TRUE(scene_opt.has_value());
        const auto scn = *scene_opt;

        const auto persistent_ent = scene_mgr.create_entity(scn, "PersistentEntity");

        auto diff = play_session_diff();
        diff.attach(events);

        // Act: Spawn dynamic entity during simulated play session
        const auto spawned_ent = reg.create();
        reg.name(spawned_ent, "SpawnedEntity");

        auto cache = hierarchy_cache();
        cache.attach_event_registry(events);
        cache.rebuild(reg, scene_mgr, &diff);

        // Assert: Rows reflect dynamic status
        const auto rows = cache.get_visible_rows();
        auto found_persistent = false;
        auto found_spawned = false;

        for (const auto& row : rows)
        {
            if (row.type == row_type::entity_node)
            {
                if (row.entity == persistent_ent)
                {
                    found_persistent = true;
                    EXPECT_FALSE(row.is_dynamic);
                }
                else if (row.entity == spawned_ent)
                {
                    found_spawned = true;
                    EXPECT_TRUE(row.is_dynamic);
                }
            }
        }

        EXPECT_TRUE(found_persistent);
        EXPECT_TRUE(found_spawned);
    }

    /// @brief Verifies that play_mode_controller attaches play_session_diff on play and clears on stop.
    TEST(play_session_diff_test, play_mode_controller_lifecycle_attaches_and_clears)
    {
        // Setup: Editor engine context and play mode controller
        auto engine_ctx = editor_engine_context();
        auto& controller = engine_ctx.get_play_mode_controller();

        EXPECT_TRUE(controller.get_session_diff().get_spawned_entities().empty());

        // Act: Enter play mode
        ASSERT_TRUE(controller.play());
        EXPECT_TRUE(controller.is_playing());

        // Spawn entity during play
        const auto spawned = engine_ctx.get_entities().create();
        engine_ctx.get_entities().name(spawned, "RuntimeSpawn");

        // Assert: Entity tracked in session diff
        EXPECT_TRUE(controller.get_session_diff().is_spawned(spawned));
        EXPECT_EQ(controller.get_session_diff().get_spawned_entities().size(), 1);

        // Act: Stop play mode
        controller.stop();
        EXPECT_TRUE(controller.is_edit());

        // Assert: Session diff detached and cleared
        EXPECT_FALSE(controller.get_session_diff().is_spawned(spawned));
        EXPECT_TRUE(controller.get_session_diff().get_spawned_entities().empty());
    }

    /// @brief Verifies that removing a baseline component and re-adding it cancels out in play_session_diff.
    TEST(play_session_diff_test, baseline_component_removed_then_readded_cancels_out)
    {
        // Setup: Event registry and play session diff tracker
        auto events = event::event_registry();
        auto diff = play_session_diff();
        diff.attach(events);

        const auto entity = static_cast<ecs::entity>(10);
        const auto comp_type = 42u;

        // Act: Baseline entity removes a component during play
        events.dispatcher<ecs::entity_component_removed_event<ecs::entity>>().publish({
            .entity = entity,
            .component_type_index = comp_type,
        });

        // Assert intermediate state: component is tracked as removed
        EXPECT_TRUE(diff.is_component_removed(entity, comp_type));
        EXPECT_FALSE(diff.is_component_added(entity, comp_type));

        // Act: Component is re-added back to the baseline entity during the same play session
        events.dispatcher<ecs::entity_component_added_event<ecs::entity>>().publish({
            .entity = entity,
            .component_type_index = comp_type,
        });

        // Assert: Re-adding cancels out the removal; component is neither added nor removed
        EXPECT_FALSE(diff.is_component_removed(entity, comp_type));
        EXPECT_FALSE(diff.is_component_added(entity, comp_type));
        EXPECT_TRUE(diff.get_added_components(entity).empty());
        EXPECT_TRUE(diff.get_removed_components(entity).empty());
    }

    /// @brief Verifies that adding a component and subsequently removing it cancels out completely.
    TEST(play_session_diff_test, component_added_then_removed_cancels_out)
    {
        // Setup: Event registry and play session diff tracker
        auto events = event::event_registry();
        auto diff = play_session_diff();
        diff.attach(events);

        const auto entity = static_cast<ecs::entity>(15);
        const auto comp_type = 99u;

        // Act: Add component during play
        events.dispatcher<ecs::entity_component_added_event<ecs::entity>>().publish({
            .entity = entity,
            .component_type_index = comp_type,
        });

        // Assert intermediate state: component is tracked as added
        EXPECT_TRUE(diff.is_component_added(entity, comp_type));
        EXPECT_FALSE(diff.is_component_removed(entity, comp_type));

        // Act: Remove the previously added component during the same play session
        events.dispatcher<ecs::entity_component_removed_event<ecs::entity>>().publish({
            .entity = entity,
            .component_type_index = comp_type,
        });

        // Assert: Removing cancels out the addition; component is neither added nor removed
        EXPECT_FALSE(diff.is_component_added(entity, comp_type));
        EXPECT_FALSE(diff.is_component_removed(entity, comp_type));
        EXPECT_TRUE(diff.get_added_components(entity).empty());
        EXPECT_TRUE(diff.get_removed_components(entity).empty());
    }

    /// @brief Verifies that removing a component from an entity spawned during play is not tracked as a removed component.
    TEST(play_session_diff_test, spawned_entity_component_removed_not_tracked_as_removed)
    {
        // Setup: Event registry and play session diff tracker
        auto events = event::event_registry();
        auto diff = play_session_diff();
        diff.attach(events);

        const auto spawned_entity = static_cast<ecs::entity>(55);
        const auto comp_type = 12u;

        // Act: Spawn entity during play
        events.dispatcher<ecs::entity_created_event<ecs::entity>>().publish({
            .entity = spawned_entity,
        });

        ASSERT_TRUE(diff.is_spawned(spawned_entity));

        // Act: Add and then remove component on the spawned entity
        events.dispatcher<ecs::entity_component_added_event<ecs::entity>>().publish({
            .entity = spawned_entity,
            .component_type_index = comp_type,
        });
        events.dispatcher<ecs::entity_component_removed_event<ecs::entity>>().publish({
            .entity = spawned_entity,
            .component_type_index = comp_type,
        });

        // Assert: Neither added nor removed
        EXPECT_FALSE(diff.is_component_added(spawned_entity, comp_type));
        EXPECT_FALSE(diff.is_component_removed(spawned_entity, comp_type));
        EXPECT_TRUE(diff.get_removed_components(spawned_entity).empty());

        // Act: Remove another component directly on the spawned entity without prior addition
        const auto other_comp_type = 77u;
        events.dispatcher<ecs::entity_component_removed_event<ecs::entity>>().publish({
            .entity = spawned_entity,
            .component_type_index = other_comp_type,
        });

        // Assert: Still not tracked as removed because the entity was spawned during this session
        EXPECT_FALSE(diff.is_component_removed(spawned_entity, other_comp_type));
        EXPECT_TRUE(diff.get_removed_components(spawned_entity).empty());
    }

    /// @brief Verifies that hierarchy_cache marks dirty when world_restored_event or simulation lifecycle events fire.
    TEST(play_session_diff_test, hierarchy_cache_marks_dirty_on_world_restored_event)
    {
        // Setup: Event registry, archetype registry, scene manager, and hierarchy cache
        auto events = event::event_registry();
        auto types = ecs::component_type_registry();
        auto reg = ecs::archetype_registry(events, types);
        auto scene_mgr = scene::scene_manager(reg);

        auto cache = hierarchy_cache();
        cache.attach_event_registry(events);

        // Rebuild cache to clean state
        cache.rebuild(reg, scene_mgr);
        EXPECT_FALSE(cache.is_dirty());

        // Act: Publish world_restored_event
        events.dispatcher<ecs::world_restored_event>().publish({});

        // Assert: Cache marked dirty
        EXPECT_TRUE(cache.is_dirty());

        // Rebuild again to clean state
        cache.rebuild(reg, scene_mgr);
        EXPECT_FALSE(cache.is_dirty());

        // Act: Publish simulation_started_event
        events.dispatcher<ecs::simulation_started_event>().publish({});

        // Assert: Cache marked dirty
        EXPECT_TRUE(cache.is_dirty());

        // Rebuild again to clean state
        cache.rebuild(reg, scene_mgr);
        EXPECT_FALSE(cache.is_dirty());

        // Act: Publish simulation_stopped_event
        events.dispatcher<ecs::simulation_stopped_event>().publish({});

        // Assert: Cache marked dirty
        EXPECT_TRUE(cache.is_dirty());
    }
} // namespace tempest::editor::tests
