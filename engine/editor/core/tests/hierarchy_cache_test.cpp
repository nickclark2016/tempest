#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/hierarchy_cache.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_manager.hpp>

namespace tempest::editor::tests
{
    // =========================================================================
    // Hierarchy Cache Unit Tests
    // =========================================================================

    /// @brief Verifies that the incremental cache maintains a flattened visible row model
    /// matching a full hierarchy tree walk across multiple scenes and dynamic entities.
    TEST(hierarchy_cache_test, incremental_cache_matches_full_rebuild)
    {
        // Setup ECS registry, event bus, and scene manager
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};

        const auto scene1_opt = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "World Alpha");
        ASSERT_TRUE(scene1_opt.has_value());
        const auto scene1 = *scene1_opt;

        const auto scene2_opt = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "World Beta",
            {.mode = scene::scene_load_mode::additive});
        ASSERT_TRUE(scene2_opt.has_value());
        const auto scene2 = *scene2_opt;

        // Populate Scene 1 entities: RootA -> ChildA1, and RootB
        const auto root_a = scene_mgr.create_entity(scene1, "RootA");
        const auto child_a1 = scene_mgr.create_entity(scene1, "ChildA1");
        reg.reparent(child_a1, root_a);
        const auto root_b = scene_mgr.create_entity(scene1, "RootB");

        // Populate Scene 2 entities: RootC
        const auto root_c = scene_mgr.create_entity(scene2, "RootC");

        // Populate Dynamic entities: DynRoot -> DynChild
        const auto dyn_root = reg.create();
        reg.name(dyn_root, "DynRoot");
        const auto dyn_child = reg.create();
        reg.name(dyn_child, "DynChild");
        reg.reparent(dyn_child, dyn_root);

        // Attach event registry to hierarchy cache
        auto cache = hierarchy_cache{};
        cache.attach_event_registry(events);

        // Act - Rebuild hierarchy cache
        cache.rebuild(reg, scene_mgr);
        auto rows = cache.get_visible_rows();

        // Assert - Verify full tree row layout
        ASSERT_EQ(rows.size(), 9U);

        // Scene 1 Header
        EXPECT_EQ(rows[0].type, row_type::scene_header);
        EXPECT_EQ(rows[0].depth, 0U);
        EXPECT_EQ(rows[0].scene, scene1);
        EXPECT_EQ(rows[0].name, "World Alpha");
        EXPECT_TRUE(rows[0].has_children);

        // RootA under Scene 1
        EXPECT_EQ(rows[1].type, row_type::entity_node);
        EXPECT_EQ(rows[1].depth, 1U);
        EXPECT_EQ(rows[1].entity, root_a);
        EXPECT_EQ(rows[1].name, "RootA");
        EXPECT_TRUE(rows[1].has_children);
        EXPECT_FALSE(rows[1].is_dynamic);

        // ChildA1 under RootA
        EXPECT_EQ(rows[2].type, row_type::entity_node);
        EXPECT_EQ(rows[2].depth, 2U);
        EXPECT_EQ(rows[2].entity, child_a1);
        EXPECT_EQ(rows[2].name, "ChildA1");
        EXPECT_FALSE(rows[2].has_children);

        // RootB under Scene 1
        EXPECT_EQ(rows[3].type, row_type::entity_node);
        EXPECT_EQ(rows[3].depth, 1U);
        EXPECT_EQ(rows[3].entity, root_b);
        EXPECT_EQ(rows[3].name, "RootB");

        // Scene 2 Header
        EXPECT_EQ(rows[4].type, row_type::scene_header);
        EXPECT_EQ(rows[4].depth, 0U);
        EXPECT_EQ(rows[4].scene, scene2);
        EXPECT_EQ(rows[4].name, "World Beta");

        // RootC under Scene 2
        EXPECT_EQ(rows[5].type, row_type::entity_node);
        EXPECT_EQ(rows[5].depth, 1U);
        EXPECT_EQ(rows[5].entity, root_c);
        EXPECT_EQ(rows[5].name, "RootC");

        // Dynamic Section Header
        EXPECT_EQ(rows[6].type, row_type::dynamic_header);
        EXPECT_EQ(rows[6].depth, 0U);
        EXPECT_EQ(rows[6].name, "Dynamic Entities");
        EXPECT_TRUE(rows[6].is_dynamic);

        // DynRoot under Dynamic Section
        EXPECT_EQ(rows[7].type, row_type::entity_node);
        EXPECT_EQ(rows[7].depth, 1U);
        EXPECT_EQ(rows[7].entity, dyn_root);
        EXPECT_EQ(rows[7].name, "DynRoot");
        EXPECT_TRUE(rows[7].is_dynamic);

        // DynChild under DynRoot
        EXPECT_EQ(rows[8].type, row_type::entity_node);
        EXPECT_EQ(rows[8].depth, 2U);
        EXPECT_EQ(rows[8].entity, dyn_child);
        EXPECT_EQ(rows[8].name, "DynChild");
        EXPECT_TRUE(rows[8].is_dynamic);

        // Act & Assert Incremental Event Updates: Destroy child_a1, rename dyn_root, add child to root_c
        reg.destroy(child_a1);
        reg.name(dyn_root, "RenamedDynRoot");
        const auto child_c1 = scene_mgr.create_entity(scene2, "ChildC1");
        reg.reparent(child_c1, root_c);

        EXPECT_TRUE(cache.is_dirty());
        cache.rebuild(reg, scene_mgr);
        rows = cache.get_visible_rows();

        // Verify updated count and modified nodes
        ASSERT_EQ(rows.size(), 9U);
        EXPECT_EQ(rows[1].entity, root_a);
        EXPECT_FALSE(rows[1].has_children);
        EXPECT_EQ(rows[2].entity, root_b);
        EXPECT_EQ(rows[4].entity, root_c);
        EXPECT_TRUE(rows[4].has_children);
        EXPECT_EQ(rows[5].entity, child_c1);
        EXPECT_EQ(rows[5].depth, 2U);
        EXPECT_EQ(rows[7].name, "RenamedDynRoot");
        EXPECT_EQ(rows[8].entity, dyn_child);
    }

    /// @brief Verifies that collapsing a parent node hides all of its descendants from visible rows,
    /// and expanding restores them.
    TEST(hierarchy_cache_test, collapse_and_expand_nodes)
    {
        // Setup hierarchy with Parent -> Child -> Grandchild
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};

        const auto scene_opt = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "Main Level");
        ASSERT_TRUE(scene_opt.has_value());
        const auto scene = *scene_opt;

        const auto parent = scene_mgr.create_entity(scene, "Parent");
        const auto child = scene_mgr.create_entity(scene, "Child");
        const auto grandchild = scene_mgr.create_entity(scene, "Grandchild");
        reg.reparent(child, parent);
        reg.reparent(grandchild, child);

        auto cache = hierarchy_cache{};
        cache.attach_event_registry(events);
        cache.rebuild(reg, scene_mgr);

        auto rows = cache.get_visible_rows();
        ASSERT_EQ(rows.size(), 4U);

        // Act - Collapse Child node
        cache.set_expanded(child, false);
        EXPECT_FALSE(cache.is_expanded(child));
        cache.rebuild(reg, scene_mgr);
        rows = cache.get_visible_rows();

        // Assert - Child is visible but Grandchild is hidden
        ASSERT_EQ(rows.size(), 3U);
        EXPECT_EQ(rows[1].entity, parent);
        EXPECT_EQ(rows[2].entity, child);
        EXPECT_FALSE(rows[2].is_expanded);

        // Act - Collapse Scene Header
        cache.set_expanded(scene, false);
        EXPECT_FALSE(cache.is_expanded(scene));
        cache.rebuild(reg, scene_mgr);
        rows = cache.get_visible_rows();

        // Assert - Only Scene Header is visible
        ASSERT_EQ(rows.size(), 1U);
        EXPECT_EQ(rows[0].type, row_type::scene_header);
        EXPECT_FALSE(rows[0].is_expanded);

        // Act - Expand Scene Header and Child
        cache.set_expanded(scene, true);
        cache.set_expanded(child, true);
        cache.rebuild(reg, scene_mgr);
        rows = cache.get_visible_rows();

        // Assert - All 4 rows restored
        ASSERT_EQ(rows.size(), 4U);
        EXPECT_EQ(rows[3].entity, grandchild);
        EXPECT_EQ(rows[3].depth, 3U);
    }

    /// @brief Verifies that entities are cleanly grouped under their respective scene headers,
    /// and that dirty and active scene flags are properly reflected on scene header rows.
    TEST(hierarchy_cache_test, scene_header_grouping_and_dirty_flag)
    {
        // Setup two scenes with entities
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};

        const auto scene_a_opt = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "Scene A");
        ASSERT_TRUE(scene_a_opt.has_value());
        const auto scene_a = *scene_a_opt;

        const auto scene_b_opt = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "Scene B",
            {.mode = scene::scene_load_mode::additive});
        ASSERT_TRUE(scene_b_opt.has_value());
        const auto scene_b = *scene_b_opt;

        const auto ent_a = scene_mgr.create_entity(scene_a, "Entity A");
        const auto ent_b = scene_mgr.create_entity(scene_b, "Entity B");

        auto cache = hierarchy_cache{};
        cache.attach_event_registry(events);
        cache.rebuild(reg, scene_mgr);

        auto rows = cache.get_visible_rows();
        ASSERT_EQ(rows.size(), 4U);
        EXPECT_FALSE(rows[0].is_dirty);
        EXPECT_FALSE(rows[2].is_dirty);

        // Act - Mark Scene A dirty and set Scene B active
        scene_mgr.set_dirty(scene_a, true);
        scene_mgr.set_active_scene(scene_b);

        cache.rebuild(reg, scene_mgr);
        rows = cache.get_visible_rows();

        // Assert - Scene A reflects dirty flag and non-active state
        EXPECT_TRUE(rows[0].is_dirty);
        EXPECT_FALSE(rows[0].is_active_scene);

        // Assert - Scene B reflects active state and non-dirty flag
        EXPECT_FALSE(rows[2].is_dirty);
        EXPECT_TRUE(rows[2].is_active_scene);
    }

    /// @brief Verifies that entities without scene_entity_component appear under the
    /// Dynamic / Unassigned section, and collapsing that section hides dynamic entities.
    TEST(hierarchy_cache_test, dynamic_entity_section)
    {
        // Setup ECS registry with only dynamic entities
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};

        const auto dyn1 = reg.create();
        reg.name(dyn1, "Dynamic Particle");
        const auto dyn2 = reg.create();
        reg.name(dyn2, "Spawned Projectile");

        auto cache = hierarchy_cache{};
        cache.attach_event_registry(events);
        cache.rebuild(reg, scene_mgr);

        auto rows = cache.get_visible_rows();
        ASSERT_EQ(rows.size(), 3U);

        // Assert - Header and dynamic items
        EXPECT_EQ(rows[0].type, row_type::dynamic_header);
        EXPECT_EQ(rows[0].name, "Dynamic Entities");
        EXPECT_TRUE(rows[0].is_dynamic);

        EXPECT_EQ(rows[1].type, row_type::entity_node);
        EXPECT_TRUE(rows[1].is_dynamic);
        EXPECT_EQ(rows[1].depth, 1U);

        EXPECT_EQ(rows[2].type, row_type::entity_node);
        EXPECT_TRUE(rows[2].is_dynamic);
        EXPECT_EQ(rows[2].depth, 1U);

        // Act - Collapse Dynamic Section
        cache.set_dynamic_section_expanded(false);
        EXPECT_FALSE(cache.is_dynamic_section_expanded());
        cache.rebuild(reg, scene_mgr);
        rows = cache.get_visible_rows();

        // Assert - Only Dynamic Header row is visible
        ASSERT_EQ(rows.size(), 1U);
        EXPECT_EQ(rows[0].type, row_type::dynamic_header);

        // Act - Expand Dynamic Section
        cache.set_dynamic_section_expanded(true);
        cache.rebuild(reg, scene_mgr);
        rows = cache.get_visible_rows();

        ASSERT_EQ(rows.size(), 3U);
    }

    /// @brief Verifies that deleting intermediate or root entities preserves the relative
    /// order of all remaining entities rather than shuffling them due to archetype table storage swap-remove.
    TEST(hierarchy_cache_test, entity_order_preserved_on_deletion)
    {
        // Setup ECS registry and scene manager
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};

        const auto scene_opt = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "Level");
        ASSERT_TRUE(scene_opt.has_value());
        const auto scene = *scene_opt;

        // Create scene root entities in sequence: Alpha, Beta, Gamma, Delta
        const auto ent_alpha = scene_mgr.create_entity(scene, "Alpha");
        const auto ent_beta = scene_mgr.create_entity(scene, "Beta");
        const auto ent_gamma = scene_mgr.create_entity(scene, "Gamma");
        const auto ent_delta = scene_mgr.create_entity(scene, "Delta");

        // Create dynamic root entities in sequence: Dyn1, Dyn2, Dyn3, Dyn4
        const auto dyn1 = reg.create();
        reg.name(dyn1, "Dyn1");
        const auto dyn2 = reg.create();
        reg.name(dyn2, "Dyn2");
        const auto dyn3 = reg.create();
        reg.name(dyn3, "Dyn3");
        const auto dyn4 = reg.create();
        reg.name(dyn4, "Dyn4");

        auto cache = hierarchy_cache{};
        cache.attach_event_registry(events);
        cache.rebuild(reg, scene_mgr);

        auto rows = cache.get_visible_rows();
        ASSERT_EQ(rows.size(), 10U);
        EXPECT_EQ(rows[1].entity, ent_alpha);
        EXPECT_EQ(rows[2].entity, ent_beta);
        EXPECT_EQ(rows[3].entity, ent_gamma);
        EXPECT_EQ(rows[4].entity, ent_delta);
        EXPECT_EQ(rows[6].entity, dyn1);
        EXPECT_EQ(rows[7].entity, dyn2);
        EXPECT_EQ(rows[8].entity, dyn3);
        EXPECT_EQ(rows[9].entity, dyn4);

        // Act - Delete intermediate scene entity Beta and intermediate dynamic entity Dyn2
        scene_mgr.remove_root_entity(scene, ent_beta);
        reg.destroy_recursive(ent_beta);

        reg.destroy_recursive(dyn2);

        cache.rebuild(reg, scene_mgr);
        rows = cache.get_visible_rows();

        // Assert - Remaining scene entities preserve relative order: Alpha, Gamma, Delta
        ASSERT_EQ(rows.size(), 8U);
        EXPECT_EQ(rows[1].entity, ent_alpha);
        EXPECT_EQ(rows[2].entity, ent_gamma);
        EXPECT_EQ(rows[3].entity, ent_delta);

        // Assert - Remaining dynamic entities preserve relative order: Dyn1, Dyn3, Dyn4
        EXPECT_EQ(rows[5].entity, dyn1);
        EXPECT_EQ(rows[6].entity, dyn3);
        EXPECT_EQ(rows[7].entity, dyn4);

        // Act - Delete first scene entity Alpha and first dynamic entity Dyn1
        scene_mgr.remove_root_entity(scene, ent_alpha);
        reg.destroy_recursive(ent_alpha);

        reg.destroy_recursive(dyn1);

        cache.rebuild(reg, scene_mgr);
        rows = cache.get_visible_rows();

        // Assert - Remaining entities still maintain exact relative order
        ASSERT_EQ(rows.size(), 6U);
        EXPECT_EQ(rows[1].entity, ent_gamma);
        EXPECT_EQ(rows[2].entity, ent_delta);
        EXPECT_EQ(rows[4].entity, dyn3);
        EXPECT_EQ(rows[5].entity, dyn4);
    }
} // namespace tempest::editor::tests

