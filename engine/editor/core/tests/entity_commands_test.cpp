#include <gtest/gtest.h>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/checked.hpp>
#include <tempest/commands/entity_commands.hpp>
#include <tempest/dynamic_entity_id_component.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/int.hpp>
#include <tempest/math.hpp>
#include <tempest/memory.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

#if defined(_MSC_VER)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

namespace tempest_entity_cmd_test
{
    using namespace tempest;
    using namespace tempest::editor;

    constexpr auto empty_scene_json = string_view{R"({"version": 1, "entities": []})"};

    struct test_component
    {
        static constexpr int32_t default_val = 0;
        static constexpr float default_speed = 1.0F;

        int32_t val = default_val;
        float speed = default_speed;
    };

    struct test_secondary_component
    {
        static constexpr int32_t default_tag = 100;
        int32_t tag = default_tag;
    };

    static_assert(ecs::component<test_component>);
    static_assert(ecs::component<test_secondary_component>);

    struct entity_commands_fixture
    {
        event::event_registry event_reg{};
        ecs::component_type_registry type_reg{};
        ecs::archetype_registry registry{event_reg, type_reg};
        scene::scene_manager scene_mgr{registry};
        scene::prefab_cache prefab_cache{};
        command_context ctx{
            .registry = registry,
            .scene_manager = scene_mgr,
            .prefab_cache = &prefab_cache,
            .events = &event_reg,
        };

        uint32_t test_comp_idx = 0;
        uint32_t test_sec_comp_idx = 0;
        uint32_t transform_comp_idx = 0;

        entity_commands_fixture()
        {
            test_comp_idx = type_reg.ensure<test_component>();
            test_sec_comp_idx = type_reg.ensure<test_secondary_component>();
            transform_comp_idx = type_reg.ensure<ecs::transform_component>();
            type_reg.ensure<scene::prefab_instance_component>();
            type_reg.ensure<scene::prefab_member_component>();
            type_reg.ensure<scene::scene_entity_component>();
            type_reg.ensure<dynamic_entity_id_component>();

            type_reg.add_alias("test_component", type_reg.type_info(test_comp_idx).name);
            type_reg.add_alias("test_secondary_component", type_reg.type_info(test_sec_comp_idx).name);
            type_reg.add_alias("transform_component", type_reg.type_info(transform_comp_idx).name);
        }
    };
} // namespace tempest_entity_cmd_test

namespace tempest::ecs
{
    template <>
    struct reflect<tempest_entity_cmd_test::test_component>
    {
        static void describe(type_builder<tempest_entity_cmd_test::test_component>& builder)
        {
            builder.version(1)
                .field<&tempest_entity_cmd_test::test_component::val>("val")
                .field<&tempest_entity_cmd_test::test_component::speed>("speed");
        }
    };

    template <>
    struct reflect<tempest_entity_cmd_test::test_secondary_component>
    {
        static void describe(type_builder<tempest_entity_cmd_test::test_secondary_component>& builder)
        {
            builder.version(1).field<&tempest_entity_cmd_test::test_secondary_component::tag>("tag");
        }
    };
} // namespace tempest::ecs

namespace
{
    using namespace tempest;
    using namespace tempest::editor;
    using namespace tempest_entity_cmd_test;

    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
} // namespace

//==============================================================================
// Entity Lifecycle Command Tests
//==============================================================================

/// @brief Verifies that create_entity_command creates an entity in a scene, registers it as a root,
/// undo removes and destroys it, and redo restores it with identical addressing.
TEST(entity_commands_test, create_entity_in_scene_undo_redo)
{
    // Setup: Initialize test fixture and empty scene
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneA");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    // Act: Create entity via command
    auto cmd = create_entity_command{scene, "Player"};
    cmd.apply(fix.ctx);

    // Assert: Entity created and addressed
    const auto& created_addr = cmd.created_address();
    EXPECT_EQ(created_addr.scene, scene);
    ASSERT_FALSE(created_addr.id_path.empty());

    const auto live_ent = resolve_entity_address(fix.registry, fix.scene_mgr, created_addr);
    ASSERT_NE(live_ent, tombstone_entity);
    EXPECT_TRUE(fix.registry.is_valid(live_ent));
    const auto name_opt = fix.registry.name(live_ent);
    ASSERT_TRUE(name_opt.has_value());
    EXPECT_EQ(*name_opt, "Player");
    EXPECT_TRUE(fix.scene_mgr.is_dirty(scene));

    // Act: Revert creation
    cmd.revert(fix.ctx);

    // Assert: Entity destroyed and address resolves to tombstone
    const auto dead_ent = resolve_entity_address(fix.registry, fix.scene_mgr, created_addr);
    EXPECT_EQ(dead_ent, tombstone_entity);

    // Act: Redo creation
    cmd.apply(fix.ctx);

    // Assert: Entity recreated with same address and name
    const auto recreated_ent = resolve_entity_address(fix.registry, fix.scene_mgr, created_addr);
    ASSERT_NE(recreated_ent, tombstone_entity);
    EXPECT_TRUE(fix.registry.is_valid(recreated_ent));
    const auto recreated_name = fix.registry.name(recreated_ent);
    ASSERT_TRUE(recreated_name.has_value());
    EXPECT_EQ(*recreated_name, "Player");
}

/// @brief Verifies dynamic entity creation outside of any scene handle.
TEST(entity_commands_test, create_dynamic_entity_undo_redo)
{
    // Setup: Initialize test fixture without scene
    auto fix = entity_commands_fixture{};

    // Act: Create dynamic entity
    auto cmd = create_entity_command{scene::scene_handle{}, "Transient"};
    cmd.apply(fix.ctx);

    // Assert: Dynamic entity address generated and entity valid
    const auto& created_addr = cmd.created_address();
    EXPECT_FALSE(created_addr.scene.is_valid());
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, created_addr);
    ASSERT_NE(ent, tombstone_entity);
    EXPECT_TRUE(fix.registry.has<dynamic_entity_id_component>(ent));

    // Act: Undo and Redo
    cmd.revert(fix.ctx);
    EXPECT_EQ(resolve_entity_address(fix.registry, fix.scene_mgr, created_addr), tombstone_entity);

    cmd.apply(fix.ctx);
    const auto restored_ent = resolve_entity_address(fix.registry, fix.scene_mgr, created_addr);
    EXPECT_NE(restored_ent, tombstone_entity);
    EXPECT_TRUE(fix.registry.has<dynamic_entity_id_component>(restored_ent));
}

/// @brief Verifies recursive destruction of an entity hierarchy with components, and idempotent restoration.
TEST(entity_commands_test, destroy_entity_recursive_hierarchy_undo_redo)
{
    // Setup: Initialize scene, parent entity, and child entity
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneA");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto parent_cmd = create_entity_command{scene, "Parent"};
    parent_cmd.apply(fix.ctx);
    const auto parent_addr = parent_cmd.created_address();
    const auto parent_ent = resolve_entity_address(fix.registry, fix.scene_mgr, parent_addr);

    auto child_cmd = create_entity_command{scene, "Child"};
    child_cmd.apply(fix.ctx);
    const auto child_addr = child_cmd.created_address();
    const auto child_ent = resolve_entity_address(fix.registry, fix.scene_mgr, child_addr);

    fix.registry.reparent(child_ent, parent_ent);
    fix.registry.assign(parent_ent, test_component{.val = 111, .speed = 2.0F});
    fix.registry.assign(child_ent, test_component{.val = 222, .speed = 4.0F});

    // Act: Destroy parent with recursive mode
    auto destroy_cmd = destroy_entity_command{parent_addr, destroy_entity_command::mode::recursive};
    destroy_cmd.apply(fix.ctx);

    // Assert: Both parent and child destroyed
    EXPECT_EQ(resolve_entity_address(fix.registry, fix.scene_mgr, parent_addr), tombstone_entity);
    EXPECT_FALSE(fix.registry.is_valid(parent_ent));
    EXPECT_FALSE(fix.registry.is_valid(child_ent));

    // Act: Revert recursive destroy
    destroy_cmd.revert(fix.ctx);

    // Assert: Both restored with correct values and relationship
    const auto restored_parent = resolve_entity_address(fix.registry, fix.scene_mgr, parent_addr);
    ASSERT_NE(restored_parent, tombstone_entity);
    EXPECT_TRUE(fix.registry.is_valid(restored_parent));
    const auto* parent_comp = fix.registry.try_get<test_component>(restored_parent);
    ASSERT_NE(parent_comp, nullptr);
    EXPECT_EQ(parent_comp->val, 111);

    const auto* rel = fix.registry.try_get<ecs::relationship_component<ecs::entity>>(restored_parent);
    ASSERT_NE(rel, nullptr);
    const auto restored_child = rel->first_child;
    ASSERT_NE(restored_child, tombstone_entity);
    const auto* child_comp = fix.registry.try_get<test_component>(restored_child);
    ASSERT_NE(child_comp, nullptr);
    EXPECT_EQ(child_comp->val, 222);

    // Act: Redo destroy
    destroy_cmd.apply(fix.ctx);
    EXPECT_EQ(resolve_entity_address(fix.registry, fix.scene_mgr, parent_addr), tombstone_entity);
}

/// @brief Verifies reparent_children mode preserves children under the grandparent when destroying parent.
TEST(entity_commands_test, destroy_entity_reparent_children_mode)
{
    // Setup: Hierarchy grandparent -> parent -> child
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneA");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto gp_cmd = create_entity_command{scene, "Grandparent"};
    gp_cmd.apply(fix.ctx);
    const auto gp_addr = gp_cmd.created_address();
    const auto gp_ent = resolve_entity_address(fix.registry, fix.scene_mgr, gp_addr);

    auto p_cmd = create_entity_command{scene, "Parent"};
    p_cmd.apply(fix.ctx);
    const auto p_addr = p_cmd.created_address();
    const auto p_ent = resolve_entity_address(fix.registry, fix.scene_mgr, p_addr);

    auto c_cmd = create_entity_command{scene, "Child"};
    c_cmd.apply(fix.ctx);
    const auto c_addr = c_cmd.created_address();
    const auto c_ent = resolve_entity_address(fix.registry, fix.scene_mgr, c_addr);
    fix.registry.reparent(p_ent, gp_ent);
    fix.registry.reparent(c_ent, p_ent);

    // Act: Destroy parent with reparent_children mode
    auto destroy_cmd = destroy_entity_command{p_addr, destroy_entity_command::mode::reparent_children};
    destroy_cmd.apply(fix.ctx);

    // Assert: Parent destroyed, child now reparented under grandparent
    EXPECT_EQ(resolve_entity_address(fix.registry, fix.scene_mgr, p_addr), tombstone_entity);
    const auto live_child = resolve_entity_address(fix.registry, fix.scene_mgr, c_addr);
    ASSERT_NE(live_child, tombstone_entity);
    const auto* child_rel = fix.registry.try_get<ecs::relationship_component<ecs::entity>>(live_child);
    ASSERT_NE(child_rel, nullptr);
    EXPECT_EQ(child_rel->parent, gp_ent);

    // Act: Revert destroy
    destroy_cmd.revert(fix.ctx);

    // Assert: Parent restored, child reparented back to parent
    const auto restored_p = resolve_entity_address(fix.registry, fix.scene_mgr, p_addr);
    ASSERT_NE(restored_p, tombstone_entity);
    const auto* restored_child_rel = fix.registry.try_get<ecs::relationship_component<ecs::entity>>(live_child);
    ASSERT_NE(restored_child_rel, nullptr);
    EXPECT_EQ(restored_child_rel->parent, restored_p);
}

/// @brief Verifies duplicating an entity and its hierarchy creates siblings with Copy suffix and new IDs.
TEST(entity_commands_test, duplicate_entity_hierarchy_undo_redo)
{
    // Setup: Create parent entity with child entity
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneA");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto p_cmd = create_entity_command{scene, "MeshActor"};
    p_cmd.apply(fix.ctx);
    const auto p_addr = p_cmd.created_address();
    const auto p_ent = resolve_entity_address(fix.registry, fix.scene_mgr, p_addr);

    auto c_cmd = create_entity_command{scene, "Submesh"};
    c_cmd.apply(fix.ctx);
    const auto c_addr = c_cmd.created_address();
    const auto c_ent = resolve_entity_address(fix.registry, fix.scene_mgr, c_addr);
    fix.registry.reparent(c_ent, p_ent);

    fix.registry.assign(p_ent, test_component{.val = 55, .speed = 1.5F});

    // Act: Duplicate parent
    auto dup_cmd = duplicate_entity_command{p_addr};
    dup_cmd.apply(fix.ctx);

    // Assert: Cloned entity exists, has "MeshActor Copy" name, and duplicated components
    const auto clone_addr = dup_cmd.cloned_address();
    const auto clone_ent = resolve_entity_address(fix.registry, fix.scene_mgr, clone_addr);
    ASSERT_NE(clone_ent, tombstone_entity);
    EXPECT_NE(clone_ent, p_ent);

    const auto clone_name = fix.registry.name(clone_ent);
    ASSERT_TRUE(clone_name.has_value());
    EXPECT_EQ(*clone_name, "MeshActor Copy");

    const auto* comp = fix.registry.try_get<test_component>(clone_ent);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->val, 55);

    // Assert: Cloned child exists
    const auto* clone_rel = fix.registry.try_get<ecs::relationship_component<ecs::entity>>(clone_ent);
    ASSERT_NE(clone_rel, nullptr);
    EXPECT_NE(clone_rel->first_child, tombstone_entity);
    EXPECT_NE(clone_rel->first_child, c_ent);

    // Act: Revert duplicate
    dup_cmd.revert(fix.ctx);

    // Assert: Cloned entity destroyed
    EXPECT_EQ(resolve_entity_address(fix.registry, fix.scene_mgr, clone_addr), tombstone_entity);

    // Act: Redo duplicate
    dup_cmd.apply(fix.ctx);
    EXPECT_NE(resolve_entity_address(fix.registry, fix.scene_mgr, clone_addr), tombstone_entity);
}

/// @brief Verifies entity renaming and undo/redo restoration.
TEST(entity_commands_test, rename_entity_undo_redo)
{
    // Setup: Create entity named "OldName"
    auto fix = entity_commands_fixture{};
    auto cmd = create_entity_command{scene::scene_handle{}, "OldName"};
    cmd.apply(fix.ctx);
    const auto addr = cmd.created_address();
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, addr);

    // Act: Rename to "NewName"
    auto rename_cmd = rename_entity_command{addr, "NewName"};
    rename_cmd.apply(fix.ctx);

    // Assert: Name updated
    EXPECT_EQ(fix.registry.name(ent).value_or(string_view{}), "NewName");

    // Act: Undo rename
    rename_cmd.revert(fix.ctx);
    EXPECT_EQ(fix.registry.name(ent).value_or(string_view{}), "OldName");

    // Act: Redo rename
    rename_cmd.apply(fix.ctx);
    EXPECT_EQ(fix.registry.name(ent).value_or(string_view{}), "NewName");
}

/// @brief Verifies reparent_entity_command preserves world transform when reparenting under a transformed parent.
TEST(entity_commands_test, reparent_entity_preserve_world_transform)
{
    // Setup: Create parent at (10, 0, 0) and target at (2, 0, 0) -> world position (12, 0, 0)
    // Create new parent at (5, 0, 0)
    auto fix = entity_commands_fixture{};
    auto p1_cmd = create_entity_command{scene::scene_handle{}, "Parent1"};
    p1_cmd.apply(fix.ctx);
    const auto p1_ent = resolve_entity_address(fix.registry, fix.scene_mgr, p1_cmd.created_address());
    auto p1_tf = ecs::transform_component{};
    p1_tf.position({10.0F, 0.0F, 0.0F});
    fix.registry.assign(p1_ent, p1_tf);

    auto target_cmd = create_entity_command{scene::scene_handle{}, "Target"};
    target_cmd.apply(fix.ctx);
    const auto target_addr = target_cmd.created_address();
    const auto target_ent = resolve_entity_address(fix.registry, fix.scene_mgr, target_addr);
    auto target_tf = ecs::transform_component{};
    target_tf.position({2.0F, 0.0F, 0.0F});
    fix.registry.assign(target_ent, target_tf);
    fix.registry.reparent(target_ent, p1_ent);

    auto p2_cmd = create_entity_command{scene::scene_handle{}, "Parent2"};
    p2_cmd.apply(fix.ctx);
    const auto p2_addr = p2_cmd.created_address();
    const auto p2_ent = resolve_entity_address(fix.registry, fix.scene_mgr, p2_addr);
    auto p2_tf = ecs::transform_component{};
    p2_tf.position({5.0F, 0.0F, 0.0F});
    fix.registry.assign(p2_ent, p2_tf);

    // Act: Reparent target to Parent2 with preserve_world_transform = true
    auto reparent_cmd = reparent_entity_command{target_addr, p2_addr, nullopt, true};
    reparent_cmd.apply(fix.ctx);

    // Assert: Local position is now (7, 0, 0) because Parent2 is at (5, 0, 0), so world pos remains (12, 0, 0)
    const auto* updated_tf = fix.registry.try_get<ecs::transform_component>(target_ent);
    ASSERT_NE(updated_tf, nullptr);
    EXPECT_NEAR(updated_tf->position().x, 7.0F, 1e-4F);
    EXPECT_NEAR(updated_tf->position().y, 0.0F, 1e-4F);
    EXPECT_NEAR(updated_tf->position().z, 0.0F, 1e-4F);

    // Act: Undo reparent
    reparent_cmd.revert(fix.ctx);

    // Assert: Restored to Parent1 and local position (2, 0, 0)
    const auto* reverted_tf = fix.registry.try_get<ecs::transform_component>(target_ent);
    ASSERT_NE(reverted_tf, nullptr);
    EXPECT_NEAR(reverted_tf->position().x, 2.0F, 1e-4F);
    const auto* rel = fix.registry.try_get<ecs::relationship_component<ecs::entity>>(target_ent);
    ASSERT_NE(rel, nullptr);
    EXPECT_EQ(rel->parent, p1_ent);
}

//==============================================================================
// Component Mutation Command Tests
//==============================================================================

/// @brief Verifies add_component_command adds component with default values and undoes cleanly.
TEST(entity_commands_test, add_component_undo_redo)
{
    // Setup: Create bare entity
    auto fix = entity_commands_fixture{};
    auto create_cmd = create_entity_command{scene::scene_handle{}, "EntityA"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, addr);

    // Act: Add test_component
    auto add_cmd = add_component_command{addr, fix.test_comp_idx};
    add_cmd.apply(fix.ctx);

    // Assert: Component assigned with defaults
    const auto* comp = fix.registry.try_get<test_component>(ent);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->val, test_component::default_val);
    EXPECT_FLOAT_EQ(comp->speed, test_component::default_speed);

    // Act: Revert addition
    add_cmd.revert(fix.ctx);
    EXPECT_EQ(fix.registry.try_get<test_component>(ent), nullptr);

    // Act: Redo addition
    add_cmd.apply(fix.ctx);
    const auto* redone_comp = fix.registry.try_get<test_component>(ent);
    ASSERT_NE(redone_comp, nullptr);
    EXPECT_EQ(redone_comp->val, test_component::default_val);
}

/// @brief Verifies remove_component_command captures original component data and restores it on undo.
TEST(entity_commands_test, remove_component_undo_redo)
{
    // Setup: Create entity with component
    auto fix = entity_commands_fixture{};
    auto create_cmd = create_entity_command{scene::scene_handle{}, "EntityB"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    fix.registry.assign(ent, test_component{.val = 999, .speed = 5.0F});

    // Act: Remove component
    auto rem_cmd = remove_component_command{addr, fix.test_comp_idx};
    rem_cmd.apply(fix.ctx);

    // Assert: Removed
    EXPECT_EQ(fix.registry.try_get<test_component>(ent), nullptr);

    // Act: Revert removal
    rem_cmd.revert(fix.ctx);

    // Assert: Restored with original values
    const auto* restored = fix.registry.try_get<test_component>(ent);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->val, 999);
    EXPECT_FLOAT_EQ(restored->speed, 5.0F);
}

/// @brief Verifies reset_component_command restores default constructed state and undoes to previous values.
TEST(entity_commands_test, reset_component_undo_redo)
{
    // Setup: Create entity with non-default component
    auto fix = entity_commands_fixture{};
    auto create_cmd = create_entity_command{scene::scene_handle{}, "EntityC"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    fix.registry.assign(ent, test_component{.val = 777, .speed = 9.0F});

    // Act: Reset component
    auto reset_cmd = reset_component_command{addr, fix.test_comp_idx};
    reset_cmd.apply(fix.ctx);

    // Assert: Values reset to default (val = 0, speed = 1.0F)
    const auto* reset_val = fix.registry.try_get<test_component>(ent);
    ASSERT_NE(reset_val, nullptr);
    EXPECT_EQ(reset_val->val, test_component::default_val);
    EXPECT_FLOAT_EQ(reset_val->speed, test_component::default_speed);

    // Act: Revert reset
    reset_cmd.revert(fix.ctx);

    // Assert: Values restored to 777 and 9.0F
    const auto* restored = fix.registry.try_get<test_component>(ent);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->val, 777);
    EXPECT_FLOAT_EQ(restored->speed, 9.0F);
}

/// @brief Verifies set_field_command mutates a reflected field and supports undo/redo.
TEST(entity_commands_test, set_field_undo_redo)
{
    // Setup: Create entity with test_component
    auto fix = entity_commands_fixture{};
    auto create_cmd = create_entity_command{scene::scene_handle{}, "EntityD"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    fix.registry.assign(ent, test_component{.val = 10, .speed = 1.0F});

    // Act: Mutate val to 25
    auto val_bytes = vector<byte>(sizeof(int32_t));
    const auto new_val = int32_t{25};
    tempest::memcpy(val_bytes.data(), &new_val, sizeof(int32_t));

    auto set_cmd = set_field_command{addr, "test_component", "val", val_bytes};
    set_cmd.apply(fix.ctx);

    // Assert: Mutation applied
    EXPECT_EQ(fix.registry.get<test_component>(ent).val, 25);

    // Act: Revert mutation
    set_cmd.revert(fix.ctx);
    EXPECT_EQ(fix.registry.get<test_component>(ent).val, 10);

    // Act: Redo mutation
    set_cmd.apply(fix.ctx);
    EXPECT_EQ(fix.registry.get<test_component>(ent).val, 25);
}

/// @brief Verifies set_field_command drag coalescing via merge_with during interactive slider adjustments.
TEST(entity_commands_test, set_field_drag_coalescing)
{
    // Setup: Create entity with test_component
    auto fix = entity_commands_fixture{};
    auto create_cmd = create_entity_command{scene::scene_handle{}, "EntityE"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    fix.registry.assign(ent, test_component{.val = 10, .speed = 1.0F});

    // Act: First in-flight command sets val to 20
    auto val1_bytes = vector<byte>(sizeof(int32_t));
    const auto val1 = int32_t{20};
    tempest::memcpy(val1_bytes.data(), &val1, sizeof(int32_t));
    auto cmd1 = set_field_command{addr, "test_component", "val", val1_bytes};
    cmd1.apply(fix.ctx);
    EXPECT_EQ(fix.registry.get<test_component>(ent).val, 20);

    // Act: Second in-flight command sets val to 30
    auto val2_bytes = vector<byte>(sizeof(int32_t));
    const auto val2 = int32_t{30};
    tempest::memcpy(val2_bytes.data(), &val2, sizeof(int32_t));
    auto cmd2 = set_field_command{addr, "test_component", "val", val2_bytes};

    // Assert: Merging cmd2 into cmd1 succeeds
    EXPECT_TRUE(cmd1.merge_with(cmd2));

    // Act: Apply merged cmd1
    cmd1.apply(fix.ctx);
    EXPECT_EQ(fix.registry.get<test_component>(ent).val, 30);

    // Act: Reverting merged cmd1 restores original value 10
    cmd1.revert(fix.ctx);
    EXPECT_EQ(fix.registry.get<test_component>(ent).val, 10);
}

//==============================================================================
// Prefab Override Auto-Tracking Tests
//==============================================================================

/// @brief Verifies that mutating a field on a prefab member entity automatically records a field override in the root.
TEST(entity_commands_test, set_field_records_prefab_override)
{
    // Setup: Set up prefab instance hierarchy with root and member
    auto fix = entity_commands_fixture{};
    const auto root_ent = fix.registry.create();
    const auto root_id = scene::entity_local_id{1001};
    fix.registry.assign(root_ent, scene::scene_entity_component{
                                      .scene = scene::scene_handle{},
                                      .local_id = root_id,
                                  });
    fix.registry.assign(root_ent, scene::prefab_instance_component{
                                      .prefab_guid = guid{},
                                      .instance_local_id = root_id,
                                  });

    const auto member_ent = fix.registry.create();
    const auto member_template_id = scene::entity_local_id{2002};
    fix.registry.assign(member_ent, scene::prefab_member_component{
                                        .template_local_id = member_template_id,
                                    });
    fix.registry.reparent(member_ent, root_ent);
    fix.registry.assign(member_ent, test_component{.val = 50, .speed = 1.0F});

    const auto member_addr = compute_entity_address(fix.registry, member_ent);

    // Act: Mutate val on prefab member via set_field_command
    auto val_bytes = vector<byte>(sizeof(int32_t));
    const auto new_val = int32_t{99};
    tempest::memcpy(val_bytes.data(), &new_val, sizeof(int32_t));

    auto set_cmd = set_field_command{member_addr, "test_component", "val", val_bytes};
    set_cmd.apply(fix.ctx);

    // Assert: Member field mutated and override recorded in root instance
    EXPECT_EQ(fix.registry.get<test_component>(member_ent).val, 99);
    const auto* pic = fix.registry.try_get<scene::prefab_instance_component>(root_ent);
    ASSERT_NE(pic, nullptr);
    EXPECT_FALSE(pic->overrides.field_overrides.empty());
    EXPECT_EQ(pic->overrides.field_overrides[0].field_name, "val");

    // Act: Revert field mutation
    set_cmd.revert(fix.ctx);

    // Assert: Field restored and override removed
    EXPECT_EQ(fix.registry.get<test_component>(member_ent).val, 50);
    const auto* pic_reverted = fix.registry.try_get<scene::prefab_instance_component>(root_ent);
    ASSERT_NE(pic_reverted, nullptr);
    EXPECT_TRUE(pic_reverted->overrides.field_overrides.empty());
}

/// @brief Verifies that adding and removing components on a prefab member records added/removed overrides.
TEST(entity_commands_test, add_and_remove_component_records_prefab_overrides)
{
    // Setup: Prefab instance root and member
    auto fix = entity_commands_fixture{};
    const auto root_ent = fix.registry.create();
    const auto root_id = scene::entity_local_id{5001};
    fix.registry.assign(root_ent, scene::scene_entity_component{
                                      .scene = scene::scene_handle{},
                                      .local_id = root_id,
                                  });
    fix.registry.assign(root_ent, scene::prefab_instance_component{
                                      .prefab_guid = guid{},
                                      .instance_local_id = root_id,
                                  });

    const auto member_ent = fix.registry.create();
    const auto member_template_id = scene::entity_local_id{5002};
    fix.registry.assign(member_ent, scene::prefab_member_component{
                                        .template_local_id = member_template_id,
                                    });
    fix.registry.reparent(member_ent, root_ent);
    const auto member_addr = compute_entity_address(fix.registry, member_ent);

    // Act: Add test_secondary_component to member
    auto add_cmd = add_component_command{member_addr, fix.test_sec_comp_idx};
    add_cmd.apply(fix.ctx);

    // Assert: Component added and added_component override tracked
    EXPECT_TRUE(fix.registry.has<test_secondary_component>(member_ent));
    const auto* pic = fix.registry.try_get<scene::prefab_instance_component>(root_ent);
    ASSERT_NE(pic, nullptr);
    EXPECT_FALSE(pic->overrides.added_components.empty());

    // Act: Revert addition
    add_cmd.revert(fix.ctx);
    EXPECT_FALSE(fix.registry.has<test_secondary_component>(member_ent));
    const auto* pic_reverted = fix.registry.try_get<scene::prefab_instance_component>(root_ent);
    ASSERT_NE(pic_reverted, nullptr);
    EXPECT_TRUE(pic_reverted->overrides.added_components.empty());

    // Act: Remove test_component from member (pre-assigned)
    fix.registry.assign(member_ent, test_component{.val = 123, .speed = 2.0F});
    auto rem_cmd = remove_component_command{member_addr, fix.test_comp_idx};
    rem_cmd.apply(fix.ctx);

    // Assert: Component removed and removed_component override tracked
    EXPECT_FALSE(fix.registry.has<test_component>(member_ent));
    const auto* pic_rem = fix.registry.try_get<scene::prefab_instance_component>(root_ent);
    ASSERT_NE(pic_rem, nullptr);
    EXPECT_FALSE(pic_rem->overrides.removed_components.empty());

    // Act: Revert removal
    rem_cmd.revert(fix.ctx);
    EXPECT_TRUE(fix.registry.has<test_component>(member_ent));
    const auto* pic_rem_reverted = fix.registry.try_get<scene::prefab_instance_component>(root_ent);
    ASSERT_NE(pic_rem_reverted, nullptr);
    EXPECT_TRUE(pic_rem_reverted->overrides.removed_components.empty());
}

//==============================================================================
// Prefab Authoring Command Tests
//==============================================================================

/// @brief Verifies unpack_prefab_command removes prefab tags and restores them on undo.
TEST(entity_commands_test, unpack_prefab_undo_redo)
{
    // Setup: Prefab instance root and member
    auto fix = entity_commands_fixture{};
    const auto root_ent = fix.registry.create();
    const auto root_id = scene::entity_local_id{7001};
    fix.registry.assign(root_ent, scene::scene_entity_component{
                                      .scene = scene::scene_handle{},
                                      .local_id = root_id,
                                  });
    fix.registry.assign(root_ent, scene::prefab_instance_component{
                                      .prefab_guid = guid{},
                                      .instance_local_id = root_id,
                                  });

    const auto member_ent = fix.registry.create();
    const auto member_template_id = scene::entity_local_id{7002};
    fix.registry.assign(member_ent, scene::prefab_member_component{
                                        .template_local_id = member_template_id,
                                    });
    fix.registry.reparent(member_ent, root_ent);
    const auto root_addr = compute_entity_address(fix.registry, root_ent);

    // Act: Unpack prefab
    auto unpack_cmd = unpack_prefab_command{root_addr};
    unpack_cmd.apply(fix.ctx);

    // Assert: Instance and member components removed
    EXPECT_FALSE(fix.registry.has<scene::prefab_instance_component>(root_ent));
    EXPECT_FALSE(fix.registry.has<scene::prefab_member_component>(member_ent));

    // Act: Revert unpack
    unpack_cmd.revert(fix.ctx);

    // Assert: Instance and member components restored
    EXPECT_TRUE(fix.registry.has<scene::prefab_instance_component>(root_ent));
    EXPECT_TRUE(fix.registry.has<scene::prefab_member_component>(member_ent));
}

//==============================================================================
// Schema Unknown Component Command Tests
//==============================================================================

/// @brief Verifies removing an unrecognized component payload and restoring it on undo.
TEST(entity_commands_test, remove_unknown_component_undo_redo)
{
    // Setup: Scene with unrecognized component payload
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneUnrec");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto* unrec_data = fix.scene_mgr.get_unrecognized_data(scene);
    ASSERT_NE(unrec_data, nullptr);

    auto unknown_comp = scene::unknown_component{
        .name = "legacy::old_component",
        .json_payload = R"({"param": 42})",
    };
    auto ent_data = scene::entity_unrecognized_data{};
    ent_data.unknown_components.push_back(tempest::move(unknown_comp));
    unrec_data->entities[9999] = tempest::move(ent_data);

    // Act: Remove unknown component
    auto rem_unknown_cmd = remove_unknown_component_command{scene, 9999, "legacy::old_component"};
    rem_unknown_cmd.apply(fix.ctx);

    // Assert: Payload removed
    EXPECT_TRUE(unrec_data->entities.empty() || unrec_data->entities.find(9999) == unrec_data->entities.end());

    // Act: Revert removal
    rem_unknown_cmd.revert(fix.ctx);

    // Assert: Payload restored
    ASSERT_FALSE(unrec_data->entities.empty());
    ASSERT_NE(unrec_data->entities.find(9999), unrec_data->entities.end());
    EXPECT_EQ(unrec_data->entities[9999].unknown_components[0].name, "legacy::old_component");
}

/// @brief Verifies mapping an unrecognized component payload to a registered component type and restoring it on undo.
TEST(entity_commands_test, map_unknown_component_undo_redo)
{
    // Setup: Scene with unrecognized component payload matching test_component fields
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneMap");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto create_cmd = create_entity_command{scene, "MappedEntity"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    const auto local_id = addr.id_path[0].value;

    auto* unrec_data = fix.scene_mgr.get_unrecognized_data(scene);
    ASSERT_NE(unrec_data, nullptr);

    auto unknown_comp = scene::unknown_component{
        .name = "custom::legacy_data",
        .json_payload = R"({"val": 77, "speed": 4.5})",
    };
    auto ent_data = scene::entity_unrecognized_data{};
    ent_data.unknown_components.push_back(tempest::move(unknown_comp));
    unrec_data->entities[local_id] = tempest::move(ent_data);

    // Act: Map unknown component to test_component
    auto map_cmd = map_unknown_component_command{addr, "custom::legacy_data", fix.test_comp_idx};
    map_cmd.apply(fix.ctx);

    // Assert: Unknown payload removed and component assigned with deserialized fields
    EXPECT_TRUE(unrec_data->entities.empty() || unrec_data->entities.find(local_id) == unrec_data->entities.end());
    const auto* comp = fix.registry.try_get<test_component>(ent);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->val, 77);
    EXPECT_FLOAT_EQ(comp->speed, 4.5F);

    // Act: Revert mapping
    map_cmd.revert(fix.ctx);

    // Assert: Component removed and unknown payload restored
    EXPECT_EQ(fix.registry.try_get<test_component>(ent), nullptr);
    ASSERT_FALSE(unrec_data->entities.empty());
    ASSERT_NE(unrec_data->entities.find(local_id), unrec_data->entities.end());
    EXPECT_EQ(unrec_data->entities[local_id].unknown_components[0].name, "custom::legacy_data");
}
