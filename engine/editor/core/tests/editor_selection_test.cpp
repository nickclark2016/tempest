#include <gtest/gtest.h>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/commands/entity_commands.hpp>
#include <tempest/dynamic_entity_id_component.hpp>
#include <tempest/ecs_events.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/event.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest_selection_test
{
    using namespace tempest;
    using namespace tempest::editor;

    constexpr auto empty_scene_json = string_view{R"({"version": 1, "entities": []})"};
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);

    struct editor_selection_fixture
    {
        event::event_registry event_reg{};
        ecs::component_type_registry type_reg{};
        ecs::archetype_registry registry{event_reg, type_reg};
        scene::scene_manager scene_mgr{registry};

        auto create_test_scene(string_view name) -> scene::scene_handle
        {
            auto loaded = scene_mgr.load_from_string_sync(
                empty_scene_json, name, scene::scene_load_options{.mode = scene::scene_load_mode::additive});
            if (!loaded.has_value())
            {
                return {};
            }
            return loaded.value();
        }
    };

    //==============================================================================
    // Selection Service Unit Tests
    //==============================================================================

    /// @brief Verifies single selection assignment and primary target entity and address tracking.
    TEST(editor_selection_test, single_selection_and_primary_tracking)
    {
        // Setup scene and entities
        auto fix = editor_selection_fixture{};
        const auto scene_h = fix.create_test_scene("TestScene");
        ASSERT_TRUE(scene_h.is_valid());

        const auto ent1 = fix.scene_mgr.create_entity(scene_h, "Entity1");
        const auto ent2 = fix.scene_mgr.create_entity(scene_h, "Entity2");
        ASSERT_TRUE(fix.registry.is_valid(ent1));
        ASSERT_TRUE(fix.registry.is_valid(ent2));

        const auto addr1 = compute_entity_address(fix.registry, ent1);
        const auto addr2 = compute_entity_address(fix.registry, ent2);

        auto sel = editor_selection{};
        EXPECT_TRUE(sel.is_empty());
        EXPECT_EQ(sel.count(), 0U);
        EXPECT_EQ(sel.get_primary_entity(), tombstone_entity);
        EXPECT_FALSE(sel.get_primary_address().has_value());

        // Act - Select first entity
        sel.set_selection(addr1, ent1);

        // Assert - First entity is selected and marked as primary
        EXPECT_FALSE(sel.is_empty());
        EXPECT_EQ(sel.count(), 1U);
        EXPECT_TRUE(sel.is_selected(ent1));
        EXPECT_TRUE(sel.is_selected(addr1));
        EXPECT_FALSE(sel.is_selected(ent2));
        EXPECT_FALSE(sel.is_selected(addr2));
        EXPECT_EQ(sel.get_primary_entity(), ent1);
        ASSERT_TRUE(sel.get_primary_address().has_value());
        EXPECT_EQ(sel.get_primary_address().value(), addr1);

        const auto selected_entities = sel.get_selected_entities();
        ASSERT_EQ(selected_entities.size(), 1U);
        EXPECT_EQ(selected_entities[0], ent1);

        const auto selected_addresses = sel.get_selected_addresses();
        ASSERT_EQ(selected_addresses.size(), 1U);
        EXPECT_EQ(selected_addresses[0], addr1);

        // Act - Replace selection with second entity
        sel.set_selection(addr2, ent2);

        // Assert - Selection replaced and primary updated
        EXPECT_EQ(sel.count(), 1U);
        EXPECT_FALSE(sel.is_selected(ent1));
        EXPECT_TRUE(sel.is_selected(ent2));
        EXPECT_EQ(sel.get_primary_entity(), ent2);
        ASSERT_TRUE(sel.get_primary_address().has_value());
        EXPECT_EQ(sel.get_primary_address().value(), addr2);

        // Act - Select using registry helper overload
        sel.set_selection(ent1, fix.registry, &fix.scene_mgr);

        // Assert - Selection replaced via helper
        EXPECT_EQ(sel.count(), 1U);
        EXPECT_TRUE(sel.is_selected(ent1));
        EXPECT_FALSE(sel.is_selected(ent2));
        EXPECT_EQ(sel.get_primary_entity(), ent1);

        // Act - Clear selection
        sel.clear_selection();

        // Assert - Selection is empty
        EXPECT_TRUE(sel.is_empty());
        EXPECT_EQ(sel.count(), 0U);
        EXPECT_FALSE(sel.is_selected(ent1));
        EXPECT_EQ(sel.get_primary_entity(), tombstone_entity);
        EXPECT_FALSE(sel.get_primary_address().has_value());
    }

    /// @brief Verifies Ctrl+Click toggling entities in and out of selection and updating the primary target.
    TEST(editor_selection_test, multi_selection_ctrl_toggle)
    {
        // Setup scene with three entities
        auto fix = editor_selection_fixture{};
        const auto scene_h = fix.create_test_scene("TestScene");
        ASSERT_TRUE(scene_h.is_valid());

        const auto ent1 = fix.scene_mgr.create_entity(scene_h, "Entity1");
        const auto ent2 = fix.scene_mgr.create_entity(scene_h, "Entity2");
        const auto ent3 = fix.scene_mgr.create_entity(scene_h, "Entity3");

        const auto addr1 = compute_entity_address(fix.registry, ent1);
        const auto addr2 = compute_entity_address(fix.registry, ent2);
        const auto addr3 = compute_entity_address(fix.registry, ent3);

        auto sel = editor_selection{};

        // Act - Toggle ent1 into selection
        sel.toggle_selection(addr1, ent1);

        // Assert
        EXPECT_EQ(sel.count(), 1U);
        EXPECT_TRUE(sel.is_selected(ent1));
        EXPECT_EQ(sel.get_primary_entity(), ent1);

        // Act - Toggle ent2 into selection
        sel.toggle_selection(addr2, ent2);

        // Assert - Both selected, ent2 is primary
        EXPECT_EQ(sel.count(), 2U);
        EXPECT_TRUE(sel.is_selected(ent1));
        EXPECT_TRUE(sel.is_selected(ent2));
        EXPECT_EQ(sel.get_primary_entity(), ent2);

        // Act - Toggle ent3 into selection
        sel.toggle_selection(addr3, ent3);

        // Assert - All three selected, ent3 is primary
        EXPECT_EQ(sel.count(), 3U);
        EXPECT_TRUE(sel.is_selected(ent3));
        EXPECT_EQ(sel.get_primary_entity(), ent3);

        // Act - Toggle out ent3 (the current primary)
        sel.toggle_selection(addr3, ent3);

        // Assert - ent3 removed, primary updates to previous item (ent2)
        EXPECT_EQ(sel.count(), 2U);
        EXPECT_FALSE(sel.is_selected(ent3));
        EXPECT_TRUE(sel.is_selected(ent1));
        EXPECT_TRUE(sel.is_selected(ent2));
        EXPECT_EQ(sel.get_primary_entity(), ent2);
        ASSERT_TRUE(sel.get_primary_address().has_value());
        EXPECT_EQ(sel.get_primary_address().value(), addr2);

        // Act - Toggle out ent1 (non-primary item)
        sel.toggle_selection(addr1, ent1);

        // Assert - ent1 removed, primary remains ent2
        EXPECT_EQ(sel.count(), 1U);
        EXPECT_FALSE(sel.is_selected(ent1));
        EXPECT_TRUE(sel.is_selected(ent2));
        EXPECT_EQ(sel.get_primary_entity(), ent2);

        // Act - Toggle out last remaining entity
        sel.toggle_selection(addr2, ent2);

        // Assert - Selection is empty, primary reset to tombstone and nullopt
        EXPECT_TRUE(sel.is_empty());
        EXPECT_EQ(sel.count(), 0U);
        EXPECT_EQ(sel.get_primary_entity(), tombstone_entity);
        EXPECT_FALSE(sel.get_primary_address().has_value());
    }

    /// @brief Verifies Shift+Click range selection adding all entities and marking the last as primary.
    TEST(editor_selection_test, multi_selection_range)
    {
        // Setup four entities
        auto fix = editor_selection_fixture{};
        const auto scene_h = fix.create_test_scene("TestScene");
        ASSERT_TRUE(scene_h.is_valid());

        const auto ent1 = fix.scene_mgr.create_entity(scene_h, "Entity1");
        const auto ent2 = fix.scene_mgr.create_entity(scene_h, "Entity2");
        const auto ent3 = fix.scene_mgr.create_entity(scene_h, "Entity3");
        const auto ent4 = fix.scene_mgr.create_entity(scene_h, "Entity4");

        const auto addr1 = compute_entity_address(fix.registry, ent1);
        const auto addr2 = compute_entity_address(fix.registry, ent2);
        const auto addr3 = compute_entity_address(fix.registry, ent3);
        const auto addr4 = compute_entity_address(fix.registry, ent4);

        auto addrs = vector<entity_address>{};
        addrs.push_back(addr1);
        addrs.push_back(addr2);
        addrs.push_back(addr3);
        addrs.push_back(addr4);

        auto ents = vector<ecs::entity>{};
        ents.push_back(ent1);
        ents.push_back(ent2);
        ents.push_back(ent3);
        ents.push_back(ent4);

        auto sel = editor_selection{};

        // Act - Select range of 4 entities
        sel.select_range(span<const entity_address>{addrs.data(), addrs.size()},
                         span<const ecs::entity>{ents.data(), ents.size()});

        // Assert - All 4 selected, last entity (ent4) is primary
        EXPECT_EQ(sel.count(), 4U);
        EXPECT_TRUE(sel.is_selected(ent1));
        EXPECT_TRUE(sel.is_selected(ent2));
        EXPECT_TRUE(sel.is_selected(ent3));
        EXPECT_TRUE(sel.is_selected(ent4));
        EXPECT_EQ(sel.get_primary_entity(), ent4);
        ASSERT_TRUE(sel.get_primary_address().has_value());
        EXPECT_EQ(sel.get_primary_address().value(), addr4);

        // Act - Remove entity 4 from selection
        sel.remove_from_selection(addr4);

        // Assert - Primary falls back to previous in list (addr3)
        EXPECT_EQ(sel.count(), 3U);
        EXPECT_FALSE(sel.is_selected(ent4));
        EXPECT_EQ(sel.get_primary_entity(), ent3);
        ASSERT_TRUE(sel.get_primary_address().has_value());
        EXPECT_EQ(sel.get_primary_address().value(), addr3);
    }

    /// @brief Verifies that destroying an entity purges its live handle and resets primary if destroyed.
    TEST(editor_selection_test, selection_purges_destroyed_entities)
    {
        // Setup two entities and select both
        auto fix = editor_selection_fixture{};
        const auto scene_h = fix.create_test_scene("TestScene");
        ASSERT_TRUE(scene_h.is_valid());

        const auto ent1 = fix.scene_mgr.create_entity(scene_h, "Entity1");
        const auto ent2 = fix.scene_mgr.create_entity(scene_h, "Entity2");

        const auto addr1 = compute_entity_address(fix.registry, ent1);
        const auto addr2 = compute_entity_address(fix.registry, ent2);

        auto sel = editor_selection{};
        sel.add_to_selection(addr2, ent2);
        sel.add_to_selection(addr1, ent1); // ent1 is primary

        EXPECT_EQ(sel.count(), 2U);
        EXPECT_EQ(sel.get_primary_entity(), ent1);

        // Act - Destroy ent1 in registry and notify selection
        fix.registry.destroy(ent1);
        sel.on_entity_destroyed(ent1);

        // Assert - ent1 live handle purged, is_selected(ent1) false, primary entity reset to tombstone
        EXPECT_FALSE(sel.is_selected(ent1));
        EXPECT_EQ(sel.get_primary_entity(), tombstone_entity);
        EXPECT_TRUE(sel.is_selected(ent2));

        // Persistent address is retained for undo
        EXPECT_TRUE(sel.is_selected(addr1));
        EXPECT_EQ(sel.get_selected_entities().size(), 1U);
        EXPECT_EQ(sel.get_selected_addresses().size(), 2U);

        // Act - Explicitly purge addresses failing to resolve
        sel.purge_destroyed(fix.registry, fix.scene_mgr);

        // Assert - Dead address purged, surviving ent2 becomes primary
        EXPECT_EQ(sel.get_selected_addresses().size(), 1U);
        EXPECT_FALSE(sel.is_selected(addr1));
        EXPECT_TRUE(sel.is_selected(addr2));
        EXPECT_EQ(sel.get_primary_entity(), ent2);
    }

    /// @brief Verifies that entity deletion and re-creation via undo restores selection upon re_resolve.
    TEST(editor_selection_test, selection_survives_entity_recreation_by_address)
    {
        // Setup scene with an entity
        auto fix = editor_selection_fixture{};
        const auto scene_h = fix.create_test_scene("TestScene");
        ASSERT_TRUE(scene_h.is_valid());

        const auto original_ent = fix.scene_mgr.create_entity(scene_h, "OriginalEntity");
        const auto addr = compute_entity_address(fix.registry, original_ent);

        auto sel = editor_selection{};
        sel.set_selection(addr, original_ent);
        EXPECT_EQ(sel.get_primary_entity(), original_ent);

        // Act - Simulate deletion: destroy entity and notify selection
        const auto* sec = fix.registry.try_get<scene::scene_entity_component>(original_ent);
        ASSERT_NE(sec, nullptr);
        const auto preserved_local_id = sec->local_id;

        fix.registry.destroy(original_ent);
        sel.on_entity_destroyed(original_ent);

        // Assert - Live handle is dead, but address is retained
        EXPECT_FALSE(sel.is_selected(original_ent));
        EXPECT_EQ(sel.get_primary_entity(), tombstone_entity);
        EXPECT_TRUE(sel.is_selected(addr));

        // Act - Simulate undo: recreate entity with identical address
        const auto recreated_ent = fix.registry.create();
        fix.registry.assign(recreated_ent, scene::scene_entity_component{
                                               .scene = scene_h,
                                               .local_id = preserved_local_id,
                                           });
        fix.scene_mgr.add_root_entity(scene_h, recreated_ent);

        // Act - Re-resolve selection against updated registry
        sel.re_resolve(fix.registry, fix.scene_mgr);

        // Assert - Selection re-resolves to newly recreated entity
        EXPECT_TRUE(sel.is_selected(recreated_ent));
        EXPECT_EQ(sel.get_primary_entity(), recreated_ent);
        ASSERT_TRUE(sel.get_primary_address().has_value());
        EXPECT_EQ(sel.get_primary_address().value(), addr);
        EXPECT_EQ(sel.get_selected_entities().size(), 1U);
        EXPECT_EQ(sel.get_selected_entities()[0], recreated_ent);
    }

    /// @brief Verifies that unloading a scene evicts only its entities while leaving other scenes intact.
    TEST(editor_selection_test, scene_unload_clears_scene_selection)
    {
        // Setup two distinct scenes with one entity each
        auto fix = editor_selection_fixture{};
        const auto scene_a = fix.create_test_scene("SceneA");
        const auto scene_b = fix.create_test_scene("SceneB");
        ASSERT_TRUE(scene_a.is_valid());
        ASSERT_TRUE(scene_b.is_valid());

        const auto ent_a = fix.scene_mgr.create_entity(scene_a, "EntityA");
        const auto ent_b = fix.scene_mgr.create_entity(scene_b, "EntityB");

        const auto addr_a = compute_entity_address(fix.registry, ent_a);
        const auto addr_b = compute_entity_address(fix.registry, ent_b);

        auto sel = editor_selection{};
        sel.add_to_selection(addr_b, ent_b);
        sel.add_to_selection(addr_a, ent_a); // Scene A entity is primary

        EXPECT_EQ(sel.count(), 2U);
        EXPECT_EQ(sel.get_primary_entity(), ent_a);

        // Act - Unload Scene A
        sel.on_scene_unloaded(scene_a);

        // Assert - Scene A selection completely evicted; Scene B selection preserved
        EXPECT_EQ(sel.count(), 1U);
        EXPECT_FALSE(sel.is_selected(ent_a));
        EXPECT_FALSE(sel.is_selected(addr_a));
        EXPECT_TRUE(sel.is_selected(ent_b));
        EXPECT_TRUE(sel.is_selected(addr_b));
        EXPECT_EQ(sel.get_primary_entity(), ent_b);
        ASSERT_TRUE(sel.get_primary_address().has_value());
        EXPECT_EQ(sel.get_primary_address().value(), addr_b);
    }

    /// @brief Verifies that attaching an event registry automatically purges live handles on destruction events.
    TEST(editor_selection_test, attach_event_registry_auto_purges_live_handle)
    {
        // Setup scene and event registry
        auto fix = editor_selection_fixture{};
        const auto scene_h = fix.create_test_scene("TestScene");
        ASSERT_TRUE(scene_h.is_valid());

        const auto ent = fix.scene_mgr.create_entity(scene_h, "AutoPurgeEntity");
        const auto addr = compute_entity_address(fix.registry, ent);

        auto sel = editor_selection{};
        sel.attach_event_registry(fix.event_reg);
        sel.set_selection(addr, ent);

        EXPECT_TRUE(sel.is_selected(ent));
        EXPECT_EQ(sel.get_primary_entity(), ent);

        // Act - Publish entity_destroyed_event through event registry
        fix.event_reg.dispatcher<ecs::entity_destroyed_event<ecs::entity>>().publish(
            ecs::entity_destroyed_event<ecs::entity>{
                .entity = ent,
            });

        // Assert - Selection automatically evicted dead handle
        EXPECT_FALSE(sel.is_selected(ent));
        EXPECT_EQ(sel.get_primary_entity(), tombstone_entity);
        EXPECT_TRUE(sel.is_selected(addr)); // Address preserved

        // Cleanup
        sel.detach_event_registry();
    }

    /// @brief Verifies copy and move semantics of editor_selection adhering to Rule of 5.
    TEST(editor_selection_test, copy_and_move_semantics)
    {
        // Setup selection with two entities
        auto fix = editor_selection_fixture{};
        const auto scene_h = fix.create_test_scene("TestScene");
        ASSERT_TRUE(scene_h.is_valid());

        const auto ent1 = fix.scene_mgr.create_entity(scene_h, "Entity1");
        const auto ent2 = fix.scene_mgr.create_entity(scene_h, "Entity2");
        const auto addr1 = compute_entity_address(fix.registry, ent1);
        const auto addr2 = compute_entity_address(fix.registry, ent2);

        auto orig = editor_selection{};
        orig.attach_event_registry(fix.event_reg);
        orig.add_to_selection(addr1, ent1);
        orig.add_to_selection(addr2, ent2);

        // Act - Copy constructor
        auto copied = editor_selection{orig};

        // Assert copied state
        EXPECT_EQ(copied.count(), 2U);
        EXPECT_TRUE(copied.is_selected(ent1));
        EXPECT_TRUE(copied.is_selected(ent2));
        EXPECT_EQ(copied.get_primary_entity(), ent2);

        // Act - Move constructor
        auto moved = editor_selection{tempest::move(copied)};

        // Assert moved state
        EXPECT_EQ(moved.count(), 2U);
        EXPECT_TRUE(moved.is_selected(ent1));
        EXPECT_TRUE(moved.is_selected(ent2));
        EXPECT_EQ(moved.get_primary_entity(), ent2);

        // Act - Copy assignment
        auto assigned = editor_selection{};
        assigned = orig;

        // Assert assigned state
        EXPECT_EQ(assigned.count(), 2U);
        EXPECT_TRUE(assigned.is_selected(ent1));
        EXPECT_EQ(assigned.get_primary_entity(), ent2);

        // Act - Move assignment
        auto move_assigned = editor_selection{};
        move_assigned = tempest::move(assigned);

        // Assert move assigned state
        EXPECT_EQ(move_assigned.count(), 2U);
        EXPECT_TRUE(move_assigned.is_selected(ent1));
        EXPECT_EQ(move_assigned.get_primary_entity(), ent2);
    }
} // namespace tempest_selection_test
