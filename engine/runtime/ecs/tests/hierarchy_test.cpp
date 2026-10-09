#include <tempest/archetype.hpp>
#include <tempest/ecs_events.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

namespace
{
    using entity_type = tempest::ecs::basic_archetype_registry::entity_type;
    using rel_comp_type = tempest::ecs::relationship_component<entity_type>;
    static constexpr auto tombstone_entity = static_cast<entity_type>(tempest::ecs::tombstone);
} // namespace

//=============================================================================
// Hierarchy Sibling Ordering Tests
//=============================================================================

/// @brief Verifies that calling create_parent_child_relationship repeatedly appends
/// children to the parent's sibling chain, preserving authored insertion order (A -> B -> C).
TEST(tempest_ecs_hierarchy, sibling_order_appends)
{
    // 1. Setup
    auto event_reg = tempest::event::event_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{event_reg};

    const auto parent = reg.create();
    const auto child_a = reg.create();
    const auto child_b = reg.create();
    const auto child_c = reg.create();

    // 2. Act
    tempest::ecs::create_parent_child_relationship(reg, parent, child_a);
    tempest::ecs::create_parent_child_relationship(reg, parent, child_b);
    tempest::ecs::create_parent_child_relationship(reg, parent, child_c);

    // 3. Assert
    ASSERT_TRUE(reg.has<rel_comp_type>(parent));
    ASSERT_TRUE(reg.has<rel_comp_type>(child_a));
    ASSERT_TRUE(reg.has<rel_comp_type>(child_b));
    ASSERT_TRUE(reg.has<rel_comp_type>(child_c));

    const auto parent_rel = reg.get<rel_comp_type>(parent);
    EXPECT_EQ(parent_rel.first_child, child_a);

    const auto rel_a = reg.get<rel_comp_type>(child_a);
    EXPECT_EQ(rel_a.parent, parent);
    EXPECT_EQ(rel_a.next_sibling, child_b);

    const auto rel_b = reg.get<rel_comp_type>(child_b);
    EXPECT_EQ(rel_b.parent, parent);
    EXPECT_EQ(rel_b.next_sibling, child_c);

    const auto rel_c = reg.get<rel_comp_type>(child_c);
    EXPECT_EQ(rel_c.parent, parent);
    EXPECT_EQ(rel_c.next_sibling, tombstone_entity);
}

//=============================================================================
// Destruction and Link Cleanup Tests
//=============================================================================

/// @brief Verifies that destroying a middle child correctly stitches sibling links,
/// destroying a parent entity orphans all its direct children, and destroyed entity names are erased.
TEST(tempest_ecs_hierarchy, destroy_cleans_links_and_names)
{
    // 1. Setup - Part A: Middle child destruction and sibling stitching
    auto event_reg = tempest::event::event_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{event_reg};

    const auto parent = reg.create();
    const auto child_a = reg.create();
    const auto child_b = reg.create();
    const auto child_c = reg.create();

    reg.name(child_b, "MiddleChild");
    tempest::ecs::create_parent_child_relationship(reg, parent, child_a);
    tempest::ecs::create_parent_child_relationship(reg, parent, child_b);
    tempest::ecs::create_parent_child_relationship(reg, parent, child_c);

    // 2. Act - Destroy middle child B
    reg.destroy(child_b);

    // 3. Assert - Sibling stitching and name cleanup
    EXPECT_FALSE(reg.is_valid(child_b));
    EXPECT_FALSE(reg.name(child_b).has_value());
    EXPECT_FALSE(reg.find_first_with_name("MiddleChild").has_value());

    const auto rel_a = reg.get<rel_comp_type>(child_a);
    EXPECT_EQ(rel_a.next_sibling, child_c);

    const auto rel_c = reg.get<rel_comp_type>(child_c);
    EXPECT_EQ(rel_c.parent, parent);
    EXPECT_EQ(rel_c.next_sibling, tombstone_entity);

    // 4. Setup - Part B: Destroying parent orphans direct children
    const auto parent2 = reg.create();
    const auto child_d = reg.create();
    const auto child_e = reg.create();
    reg.name(parent2, "ParentTwo");

    tempest::ecs::create_parent_child_relationship(reg, parent2, child_d);
    tempest::ecs::create_parent_child_relationship(reg, parent2, child_e);

    // 5. Act - Destroy parent2
    reg.destroy(parent2);

    // 6. Assert - Parent name is erased and children are orphaned
    EXPECT_FALSE(reg.is_valid(parent2));
    EXPECT_FALSE(reg.name(parent2).has_value());
    EXPECT_FALSE(reg.find_first_with_name("ParentTwo").has_value());

    ASSERT_TRUE(reg.is_valid(child_d));
    ASSERT_TRUE(reg.is_valid(child_e));

    const auto rel_d = reg.get<rel_comp_type>(child_d);
    EXPECT_EQ(rel_d.parent, tombstone_entity);

    const auto rel_e = reg.get<rel_comp_type>(child_e);
    EXPECT_EQ(rel_e.parent, tombstone_entity);
}

//=============================================================================
// Reparenting Tests
//=============================================================================

/// @brief Verifies inserting at head, middle, tail with insert_before, and reparenting across parents.
TEST(tempest_ecs_hierarchy, reparent_insert_before)
{
    // 1. Setup
    auto event_reg = tempest::event::event_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{event_reg};

    const auto parent1 = reg.create();
    const auto child_a = reg.create();
    const auto child_c = reg.create();

    tempest::ecs::create_parent_child_relationship(reg, parent1, child_a);
    tempest::ecs::create_parent_child_relationship(reg, parent1, child_c);

    // 2. Act - Insert B before C (middle insertion)
    const auto child_b = reg.create();
    auto reparent_b_ok = reg.reparent(child_b, parent1, child_c);
    EXPECT_TRUE(reparent_b_ok);

    // 3. Assert - Order is A -> B -> C
    auto parent1_rel = reg.get<rel_comp_type>(parent1);
    EXPECT_EQ(parent1_rel.first_child, child_a);
    EXPECT_EQ(reg.get<rel_comp_type>(child_a).next_sibling, child_b);
    EXPECT_EQ(reg.get<rel_comp_type>(child_b).next_sibling, child_c);
    EXPECT_EQ(reg.get<rel_comp_type>(child_c).next_sibling, tombstone_entity);

    // 4. Act - Insert X before A (head insertion)
    const auto child_x = reg.create();
    auto reparent_x_ok = reg.reparent(child_x, parent1, child_a);
    EXPECT_TRUE(reparent_x_ok);

    // 5. Assert - Order is X -> A -> B -> C
    parent1_rel = reg.get<rel_comp_type>(parent1);
    EXPECT_EQ(parent1_rel.first_child, child_x);
    EXPECT_EQ(reg.get<rel_comp_type>(child_x).next_sibling, child_a);

    // 6. Act - Insert Y at tail with tombstone
    const auto child_y = reg.create();
    auto reparent_y_ok = reg.reparent(child_y, parent1, tombstone_entity);
    EXPECT_TRUE(reparent_y_ok);

    // 7. Assert - Order is X -> A -> B -> C -> Y
    EXPECT_EQ(reg.get<rel_comp_type>(child_c).next_sibling, child_y);
    EXPECT_EQ(reg.get<rel_comp_type>(child_y).next_sibling, tombstone_entity);

    // 8. Act - Reparent B across parents to parent2 before Z
    const auto parent2 = reg.create();
    const auto child_z = reg.create();
    tempest::ecs::create_parent_child_relationship(reg, parent2, child_z);

    auto reparent_b_to_p2_ok = reg.reparent(child_b, parent2, child_z);
    EXPECT_TRUE(reparent_b_to_p2_ok);

    // 9. Assert - Parent1 order is X -> A -> C -> Y, Parent2 order is B -> Z
    EXPECT_EQ(reg.get<rel_comp_type>(child_a).next_sibling, child_c);
    EXPECT_EQ(reg.get<rel_comp_type>(parent2).first_child, child_b);
    EXPECT_EQ(reg.get<rel_comp_type>(child_b).parent, parent2);
    EXPECT_EQ(reg.get<rel_comp_type>(child_b).next_sibling, child_z);
    EXPECT_EQ(reg.get<rel_comp_type>(child_z).next_sibling, tombstone_entity);
}

/// @brief Verifies that self-parenting and deep cyclic reparenting are rejected.
TEST(tempest_ecs_hierarchy, reparent_cycle_rejection)
{
    // 1. Setup - Linear chain: A -> B -> C -> D
    auto event_reg = tempest::event::event_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{event_reg};

    const auto node_a = reg.create();
    const auto node_b = reg.create();
    const auto node_c = reg.create();
    const auto node_d = reg.create();

    tempest::ecs::create_parent_child_relationship(reg, node_a, node_b);
    tempest::ecs::create_parent_child_relationship(reg, node_b, node_c);
    tempest::ecs::create_parent_child_relationship(reg, node_c, node_d);

    // 2. Act & Assert - Self-parenting
    EXPECT_FALSE(reg.reparent(node_a, node_a));
    EXPECT_FALSE(reg.reparent(node_b, node_b));

    // 3. Act & Assert - Deep cyclic reparenting (reparenting ancestor under descendant)
    EXPECT_FALSE(reg.reparent(node_a, node_d));
    EXPECT_FALSE(reg.reparent(node_a, node_c));
    EXPECT_FALSE(reg.reparent(node_b, node_d));

    // 4. Act & Assert - Invalid entities
    EXPECT_FALSE(reg.reparent(tombstone_entity, node_a));
    const auto invalid_ent = static_cast<entity_type>(99999ULL);
    EXPECT_FALSE(reg.reparent(node_a, invalid_ent));

    // 5. Assert - Hierarchy remains valid and unchanged
    EXPECT_EQ(reg.get<rel_comp_type>(node_a).first_child, node_b);
    EXPECT_EQ(reg.get<rel_comp_type>(node_b).parent, node_a);
    EXPECT_EQ(reg.get<rel_comp_type>(node_b).first_child, node_c);
    EXPECT_EQ(reg.get<rel_comp_type>(node_c).parent, node_b);
    EXPECT_EQ(reg.get<rel_comp_type>(node_c).first_child, node_d);
    EXPECT_EQ(reg.get<rel_comp_type>(node_d).parent, node_c);
}

//=============================================================================
// Unlinking Tests
//=============================================================================

/// @brief Verifies that unlinking middle and head children safely detaches them
/// from their parent and sibling linked list without destroying the entities.
TEST(tempest_ecs_hierarchy, unlink_detaches_from_parent_and_siblings)
{
    // 1. Setup
    auto event_reg = tempest::event::event_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{event_reg};

    const auto parent = reg.create();
    const auto child_a = reg.create();
    const auto child_b = reg.create();
    const auto child_c = reg.create();

    tempest::ecs::create_parent_child_relationship(reg, parent, child_a);
    tempest::ecs::create_parent_child_relationship(reg, parent, child_b);
    tempest::ecs::create_parent_child_relationship(reg, parent, child_c);

    // 2. Act - Unlink middle child B
    reg.unlink(child_b);

    // 3. Assert - Parent has A -> C; B is detached
    EXPECT_EQ(reg.get<rel_comp_type>(parent).first_child, child_a);
    EXPECT_EQ(reg.get<rel_comp_type>(child_a).next_sibling, child_c);
    EXPECT_EQ(reg.get<rel_comp_type>(child_b).parent, tombstone_entity);
    EXPECT_EQ(reg.get<rel_comp_type>(child_b).next_sibling, tombstone_entity);
    EXPECT_TRUE(reg.is_valid(child_b));

    // 4. Act - Unlink head child A
    reg.unlink(child_a);

    // 5. Assert - Parent has C; A is detached
    EXPECT_EQ(reg.get<rel_comp_type>(parent).first_child, child_c);
    EXPECT_EQ(reg.get<rel_comp_type>(child_a).parent, tombstone_entity);
    EXPECT_EQ(reg.get<rel_comp_type>(child_a).next_sibling, tombstone_entity);
    EXPECT_TRUE(reg.is_valid(child_a));
}

//=============================================================================
// Recursive Teardown Tests
//=============================================================================

/// @brief Verifies that destroy_recursive tears down a deep hierarchy bottom-up
/// and completely erases all names and component storage.
TEST(tempest_ecs_hierarchy, destroy_recursive_teardown)
{
    // 1. Setup - Build a multi-level tree
    auto event_reg = tempest::event::event_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{event_reg};

    const auto root = reg.create();
    const auto branch1 = reg.create();
    const auto branch2 = reg.create();
    const auto leaf1a = reg.create();
    const auto leaf1b = reg.create();
    const auto leaf2a = reg.create();

    reg.name(root, "Root");
    reg.name(branch1, "Branch1");
    reg.name(branch2, "Branch2");
    reg.name(leaf1a, "Leaf1A");
    reg.name(leaf1b, "Leaf1B");
    reg.name(leaf2a, "Leaf2A");

    tempest::ecs::create_parent_child_relationship(reg, root, branch1);
    tempest::ecs::create_parent_child_relationship(reg, root, branch2);
    tempest::ecs::create_parent_child_relationship(reg, branch1, leaf1a);
    tempest::ecs::create_parent_child_relationship(reg, branch1, leaf1b);
    tempest::ecs::create_parent_child_relationship(reg, branch2, leaf2a);

    // 2. Act
    reg.destroy_recursive(root);

    // 3. Assert - All entities are invalid and all names erased
    EXPECT_FALSE(reg.is_valid(root));
    EXPECT_FALSE(reg.is_valid(branch1));
    EXPECT_FALSE(reg.is_valid(branch2));
    EXPECT_FALSE(reg.is_valid(leaf1a));
    EXPECT_FALSE(reg.is_valid(leaf1b));
    EXPECT_FALSE(reg.is_valid(leaf2a));

    EXPECT_FALSE(reg.find_first_with_name("Root").has_value());
    EXPECT_FALSE(reg.find_first_with_name("Branch1").has_value());
    EXPECT_FALSE(reg.find_first_with_name("Branch2").has_value());
    EXPECT_FALSE(reg.find_first_with_name("Leaf1A").has_value());
    EXPECT_FALSE(reg.find_first_with_name("Leaf1B").has_value());
    EXPECT_FALSE(reg.find_first_with_name("Leaf2A").has_value());
}

//=============================================================================
// In-Place Reparenting on Destruction Tests
//=============================================================================

/// @brief Verifies that destroy_and_reparent_children splices direct children into
/// the target's former position in the parent's sibling chain, and handles root targets.
TEST(tempest_ecs_hierarchy, destroy_and_reparent_children)
{
    // 1. Setup - Part A: Non-root target splicing
    auto event_reg = tempest::event::event_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{event_reg};

    const auto grandparent = reg.create();
    const auto child_a = reg.create();
    const auto target = reg.create();
    const auto child_b = reg.create();
    const auto grandchild1 = reg.create();
    const auto grandchild2 = reg.create();

    tempest::ecs::create_parent_child_relationship(reg, grandparent, child_a);
    tempest::ecs::create_parent_child_relationship(reg, grandparent, target);
    tempest::ecs::create_parent_child_relationship(reg, grandparent, child_b);
    tempest::ecs::create_parent_child_relationship(reg, target, grandchild1);
    tempest::ecs::create_parent_child_relationship(reg, target, grandchild2);

    // 2. Act - Destroy target and reparent grandchildren into grandparent
    reg.destroy_and_reparent_children(target);

    // 3. Assert - Grandchildren spliced into target's position: A -> grandchild1 -> grandchild2 -> B
    EXPECT_FALSE(reg.is_valid(target));

    EXPECT_EQ(reg.get<rel_comp_type>(child_a).next_sibling, grandchild1);
    EXPECT_EQ(reg.get<rel_comp_type>(grandchild1).parent, grandparent);
    EXPECT_EQ(reg.get<rel_comp_type>(grandchild1).next_sibling, grandchild2);
    EXPECT_EQ(reg.get<rel_comp_type>(grandchild2).parent, grandparent);
    EXPECT_EQ(reg.get<rel_comp_type>(grandchild2).next_sibling, child_b);
    EXPECT_EQ(reg.get<rel_comp_type>(child_b).next_sibling, tombstone_entity);

    // 4. Setup - Part B: Root target destruction
    const auto root_target = reg.create();
    const auto r_child1 = reg.create();
    const auto r_child2 = reg.create();

    tempest::ecs::create_parent_child_relationship(reg, root_target, r_child1);
    tempest::ecs::create_parent_child_relationship(reg, root_target, r_child2);

    // 5. Act - Destroy root target
    reg.destroy_and_reparent_children(root_target);

    // 6. Assert - Root target destroyed, children become root entities
    EXPECT_FALSE(reg.is_valid(root_target));
    ASSERT_TRUE(reg.is_valid(r_child1));
    ASSERT_TRUE(reg.is_valid(r_child2));

    EXPECT_EQ(reg.get<rel_comp_type>(r_child1).parent, tombstone_entity);
    EXPECT_EQ(reg.get<rel_comp_type>(r_child2).parent, tombstone_entity);
}

//=============================================================================
// Entity Renamed Event Tests
//=============================================================================

/// @brief Verifies that entity_renamed_event is published on name assignment and renaming,
/// and that duplicate name assignments do not publish spurious events.
TEST(tempest_ecs_hierarchy, entity_renamed_event_published)
{
    // 1. Setup
    auto event_reg = tempest::event::event_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{event_reg};

    struct rename_record
    {
        entity_type entity;
        tempest::string old_name;
        tempest::string new_name;
    };

    auto rename_events = tempest::vector<rename_record>{};

    [[maybe_unused]] const auto subscription_handle =
        event_reg.dispatcher<tempest::ecs::entity_renamed_event<entity_type>>().subscribe([&](const auto& evt) {
            rename_events.push_back({
                .entity = evt.entity,
                .old_name = evt.old_name,
                .new_name = evt.new_name,
            });
        });

    const auto ent = reg.create();

    // 2. Act - Initial name assignment
    reg.name(ent, "Alpha");

    // 3. Assert - Initial event published with empty old_name
    ASSERT_EQ(rename_events.size(), 1U);
    EXPECT_EQ(rename_events.front().entity, ent);
    EXPECT_EQ(rename_events.front().old_name, "");
    EXPECT_EQ(rename_events.front().new_name, "Alpha");

    // 4. Act - Rename entity
    reg.name(ent, "Beta");

    // 5. Assert - Rename event published with previous name
    ASSERT_EQ(rename_events.size(), 2U);
    EXPECT_EQ(rename_events.back().entity, ent);
    EXPECT_EQ(rename_events.back().old_name, "Alpha");
    EXPECT_EQ(rename_events.back().new_name, "Beta");

    // 6. Act - Duplicate name assignment (no-op)
    reg.name(ent, "Beta");

    // 7. Assert - No duplicate event dispatched
    EXPECT_EQ(rename_events.size(), 2U);
}
