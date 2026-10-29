#include <gtest/gtest.h>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/checked.hpp>
#include <tempest/command_history.hpp>
#include <tempest/commands/entity_commands.hpp>
#include <tempest/dynamic_entity_id_component.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/int.hpp>
#include <tempest/material.hpp>
#include <tempest/math.hpp>
#include <tempest/memory.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>
#include <tempest/vertex.hpp>

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
        uint32_t point_light_idx = 0;
        uint32_t material_comp_idx = 0;
        uint32_t mesh_comp_idx = 0;

        entity_commands_fixture()
        {
            test_comp_idx = type_reg.ensure<test_component>();
            test_sec_comp_idx = type_reg.ensure<test_secondary_component>();
            transform_comp_idx = type_reg.ensure<ecs::transform_component>();
            point_light_idx = type_reg.ensure<render_system::point_light_component>();
            type_reg.ensure<scene::prefab_instance_component>();
            type_reg.ensure<scene::prefab_member_component>();
            type_reg.ensure<scene::scene_entity_component>();
            type_reg.ensure<dynamic_entity_id_component>();
            material_comp_idx = type_reg.ensure<core::material_component>();
            mesh_comp_idx = type_reg.ensure<core::mesh_component>();

            type_reg.add_alias("test_component", type_reg.type_info(test_comp_idx).name);
            type_reg.add_alias("test_secondary_component", type_reg.type_info(test_sec_comp_idx).name);
            type_reg.add_alias("transform_component", type_reg.type_info(transform_comp_idx).name);
            type_reg.add_alias("point_light_component", type_reg.type_info(point_light_idx).name);
            type_reg.add_alias("material_component", type_reg.type_info(material_comp_idx).name);
            type_reg.add_alias("mesh_component", type_reg.type_info(mesh_comp_idx).name);
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
    const auto* instance_comp = fix.registry.try_get<scene::prefab_instance_component>(root_ent);
    ASSERT_NE(instance_comp, nullptr);
    EXPECT_FALSE(instance_comp->overrides.field_overrides.empty());
    EXPECT_EQ(instance_comp->overrides.field_overrides[0].field_name, "val");

    // Act: Revert field mutation
    set_cmd.revert(fix.ctx);

    // Assert: Field restored and override removed
    EXPECT_EQ(fix.registry.get<test_component>(member_ent).val, 50);
    const auto* instance_comp_reverted = fix.registry.try_get<scene::prefab_instance_component>(root_ent);
    ASSERT_NE(instance_comp_reverted, nullptr);
    EXPECT_TRUE(instance_comp_reverted->overrides.field_overrides.empty());
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
    const auto* instance_comp = fix.registry.try_get<scene::prefab_instance_component>(root_ent);
    ASSERT_NE(instance_comp, nullptr);
    EXPECT_FALSE(instance_comp->overrides.added_components.empty());

    // Act: Revert addition
    add_cmd.revert(fix.ctx);
    EXPECT_FALSE(fix.registry.has<test_secondary_component>(member_ent));
    const auto* instance_comp_reverted = fix.registry.try_get<scene::prefab_instance_component>(root_ent);
    ASSERT_NE(instance_comp_reverted, nullptr);
    EXPECT_TRUE(instance_comp_reverted->overrides.added_components.empty());

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

//==============================================================================
// Event Propagation Tests
//==============================================================================

/// @brief Verifies that add_component_command emits component_added_event and revert emits component_removed_event.
TEST(entity_commands_test, add_component_command_publishes_event)
{
    // Setup: Scene with an entity
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneEventAdd");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto create_cmd = create_entity_command{scene, "EntityWithEvent"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto target_entity = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    ASSERT_NE(target_entity, tombstone_entity);

    auto added_event_count = size_t{0};
    auto added_entity = tombstone_entity;
    auto added_val = int32_t{0};
    auto added_sub = fix.event_reg.dispatcher<ecs::component_added_event<ecs::entity, test_component>>().subscribe(
        [&](const ecs::component_added_event<ecs::entity, test_component>& evt) {
            ++added_event_count;
            added_entity = evt.entity;
            added_val = evt.component.val;
        });

    auto removed_event_count = size_t{0};
    auto removed_entity = tombstone_entity;
    auto removed_sub = fix.event_reg.dispatcher<ecs::component_removed_event<ecs::entity, test_component>>().subscribe(
        [&](const ecs::component_removed_event<ecs::entity, test_component>& evt) {
            ++removed_event_count;
            removed_entity = evt.entity;
        });

    // Act: Apply add_component_command
    auto add_cmd = add_component_command{addr, fix.test_comp_idx};
    add_cmd.apply(fix.ctx);

    // Assert: component_added_event published with valid entity and default values
    EXPECT_EQ(added_event_count, 1U);
    EXPECT_EQ(added_entity, target_entity);
    EXPECT_EQ(added_val, test_component::default_val);
    EXPECT_EQ(removed_event_count, 0U);

    // Act: Revert add_component_command
    add_cmd.revert(fix.ctx);

    // Assert: component_removed_event published on revert
    EXPECT_EQ(removed_event_count, 1U);
    EXPECT_EQ(removed_entity, target_entity);
}

/// @brief Verifies that set_field_command emits component_replaced_event on both apply and revert.
TEST(entity_commands_test, set_field_command_publishes_event)
{
    // Setup: Scene with an entity containing test_component
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneEventSet");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto create_cmd = create_entity_command{scene, "EntityFieldSet"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto target_entity = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    ASSERT_NE(target_entity, tombstone_entity);

    auto add_cmd = add_component_command{addr, fix.test_comp_idx};
    add_cmd.apply(fix.ctx);

    auto replaced_event_count = size_t{0};
    auto replaced_entity = tombstone_entity;
    auto replaced_new_val = int32_t{0};
    auto replaced_sub = fix.event_reg.dispatcher<ecs::component_replaced_event<ecs::entity, test_component>>().subscribe(
        [&](const ecs::component_replaced_event<ecs::entity, test_component>& evt) {
            ++replaced_event_count;
            replaced_entity = evt.entity;
            replaced_new_val = evt.new_component.val;
        });

    // Act: Apply set_field_command mutating val
    const auto new_val = int32_t{42};
    auto val_bytes = vector<byte>(sizeof(int32_t));
    tempest::memcpy(val_bytes.data(), &new_val, sizeof(int32_t));
    auto set_cmd = set_field_command{addr, "test_component", "val", tempest::move(val_bytes)};
    set_cmd.apply(fix.ctx);

    // Assert: component_replaced_event published with new value
    EXPECT_EQ(replaced_event_count, 1U);
    EXPECT_EQ(replaced_entity, target_entity);
    EXPECT_EQ(replaced_new_val, 42);

    // Act: Revert set_field_command
    set_cmd.revert(fix.ctx);

    // Assert: component_replaced_event published with restored default value
    EXPECT_EQ(replaced_event_count, 2U);
    EXPECT_EQ(replaced_entity, target_entity);
    EXPECT_EQ(replaced_new_val, test_component::default_val);
}

/// @brief Verifies that remove_component_command emits component_removed_event and revert emits component_added_event.
TEST(entity_commands_test, remove_component_command_publishes_event)
{
    // Setup: Scene with an entity containing test_component
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneEventRem");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto create_cmd = create_entity_command{scene, "EntityCompRem"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto target_entity = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    ASSERT_NE(target_entity, tombstone_entity);

    auto add_cmd = add_component_command{addr, fix.test_comp_idx};
    add_cmd.apply(fix.ctx);

    // Mutate component field before removal to verify payload preserved on removal event
    const auto new_val = int32_t{99};
    auto val_bytes = vector<byte>(sizeof(int32_t));
    tempest::memcpy(val_bytes.data(), &new_val, sizeof(int32_t));
    auto set_cmd = set_field_command{addr, "test_component", "val", tempest::move(val_bytes)};
    set_cmd.apply(fix.ctx);

    auto removed_event_count = size_t{0};
    auto removed_entity = tombstone_entity;
    auto removed_val = int32_t{0};
    auto removed_sub = fix.event_reg.dispatcher<ecs::component_removed_event<ecs::entity, test_component>>().subscribe(
        [&](const ecs::component_removed_event<ecs::entity, test_component>& evt) {
            ++removed_event_count;
            removed_entity = evt.entity;
            removed_val = evt.component.val;
        });

    auto added_event_count = size_t{0};
    auto added_entity = tombstone_entity;
    auto added_sub = fix.event_reg.dispatcher<ecs::component_added_event<ecs::entity, test_component>>().subscribe(
        [&](const ecs::component_added_event<ecs::entity, test_component>& evt) {
            ++added_event_count;
            added_entity = evt.entity;
        });

    // Act: Apply remove_component_command
    auto rem_cmd = remove_component_command{addr, fix.test_comp_idx};
    rem_cmd.apply(fix.ctx);

    // Assert: component_removed_event published with the entity and value before removal
    EXPECT_EQ(removed_event_count, 1U);
    EXPECT_EQ(removed_entity, target_entity);
    EXPECT_EQ(removed_val, 99);
    EXPECT_EQ(added_event_count, 0U);

    // Act: Revert remove_component_command
    rem_cmd.revert(fix.ctx);

    // Assert: component_added_event published on revert
    EXPECT_EQ(added_event_count, 1U);
    EXPECT_EQ(added_entity, target_entity);
}

/// @brief Verifies that adding a point_light_component via add_component_command emits component_added_event,
/// mutating color/intensity/range via set_field_command emits component_replaced_event,
/// and remove_component_command emits component_removed_event and restore on revert emits component_added_event.
TEST(entity_commands_test, point_light_command_lifecycle_events)
{
    // Setup: Initialize command fixture and scene with a point light entity
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "ScenePointLightCmdTest");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto create_cmd = create_entity_command{scene, "PointLightEntity"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto target_entity = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    ASSERT_NE(target_entity, tombstone_entity);

    auto added_event_count = size_t{0};
    auto added_entity = tombstone_entity;
    auto added_sub =
        fix.event_reg.dispatcher<ecs::component_added_event<ecs::entity, render_system::point_light_component>>().subscribe(
            [&](const ecs::component_added_event<ecs::entity, render_system::point_light_component>& evt) {
                ++added_event_count;
                added_entity = evt.entity;
            });

    auto replaced_event_count = size_t{0};
    auto replaced_entity = tombstone_entity;
    render_system::point_light_component last_replaced_old{};
    render_system::point_light_component last_replaced_new{};
    auto replaced_sub =
        fix.event_reg.dispatcher<ecs::component_replaced_event<ecs::entity, render_system::point_light_component>>().subscribe(
            [&](const ecs::component_replaced_event<ecs::entity, render_system::point_light_component>& evt) {
                ++replaced_event_count;
                replaced_entity = evt.entity;
                last_replaced_old = evt.old_component;
                last_replaced_new = evt.new_component;
            });

    auto removed_event_count = size_t{0};
    auto removed_entity = tombstone_entity;
    auto removed_sub =
        fix.event_reg.dispatcher<ecs::component_removed_event<ecs::entity, render_system::point_light_component>>().subscribe(
            [&](const ecs::component_removed_event<ecs::entity, render_system::point_light_component>& evt) {
                ++removed_event_count;
                removed_entity = evt.entity;
            });

    // Act: Add point_light_component via add_component_command
    auto add_cmd = add_component_command{addr, fix.point_light_idx};
    add_cmd.apply(fix.ctx);

    // Assert: component_added_event published
    EXPECT_EQ(added_event_count, 1U);
    EXPECT_EQ(added_entity, target_entity);

    // Act: Mutate light color via set_field_command
    const auto new_color = math::vec3<float>{0.2F, 0.4F, 0.8F};
    auto color_bytes = vector<byte>(sizeof(new_color));
    tempest::memcpy(color_bytes.data(), &new_color, sizeof(new_color));
    auto set_color_cmd = set_field_command{addr, "point_light_component", "color", tempest::move(color_bytes)};
    set_color_cmd.apply(fix.ctx);

    // Assert: component_replaced_event published with updated color
    EXPECT_EQ(replaced_event_count, 1U);
    EXPECT_EQ(replaced_entity, target_entity);
    EXPECT_FLOAT_EQ(last_replaced_old.color.x, 1.0F);
    EXPECT_FLOAT_EQ(last_replaced_new.color.x, 0.2F);
    EXPECT_FLOAT_EQ(last_replaced_new.color.y, 0.4F);
    EXPECT_FLOAT_EQ(last_replaced_new.color.z, 0.8F);

    // Act: Mutate intensity via set_field_command
    const auto new_intensity = 25.0F;
    auto int_bytes = vector<byte>(sizeof(new_intensity));
    tempest::memcpy(int_bytes.data(), &new_intensity, sizeof(new_intensity));
    auto set_int_cmd = set_field_command{addr, "point_light_component", "intensity", tempest::move(int_bytes)};
    set_int_cmd.apply(fix.ctx);

    // Assert: component_replaced_event published with updated intensity
    EXPECT_EQ(replaced_event_count, 2U);
    EXPECT_FLOAT_EQ(last_replaced_new.intensity, 25.0F);

    // Act: Remove point_light_component via remove_component_command
    auto rem_cmd = remove_component_command{addr, fix.point_light_idx};
    rem_cmd.apply(fix.ctx);

    // Assert: component_removed_event published and entity no longer has component
    EXPECT_EQ(removed_event_count, 1U);
    EXPECT_EQ(removed_entity, target_entity);
    EXPECT_FALSE(fix.registry.has<render_system::point_light_component>(target_entity));

    // Act: Revert remove_component_command
    rem_cmd.revert(fix.ctx);

    // Assert: component_added_event published on restore and component restored
    EXPECT_EQ(added_event_count, 2U);
    EXPECT_TRUE(fix.registry.has<render_system::point_light_component>(target_entity));
    const auto& restored_pl = fix.registry.get<render_system::point_light_component>(target_entity);
    EXPECT_FLOAT_EQ(restored_pl.intensity, 25.0F);
}

//==============================================================================
// Prefab Instantiation & Material Assignment Commands Tests
//==============================================================================

/// @brief Verifies that instantiate_prefab_command creates scene entities, records stable address,
/// registers in scene roots, and supports lossless undo and redo operations.
TEST(entity_commands_test, instantiate_prefab_command_root_undo_redo)
{
    // Setup: Scene and registered multi-entity prefab template in cache
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneInstantiatePrefab");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto t_reg = ecs::archetype_registry{fix.event_reg, fix.type_reg};
    const auto tmpl_root = t_reg.create();
    t_reg.name(tmpl_root, "PrefabRoot");
    t_reg.assign(tmpl_root, test_component{.val = 42, .speed = 2.5F});

    const auto tmpl_child = t_reg.create();
    t_reg.name(tmpl_child, "PrefabChild");
    t_reg.assign(tmpl_child, test_secondary_component{.tag = 100});
    t_reg.reparent(tmpl_child, tmpl_root);

    auto tmpl = scene::prefab_template::from_entity(t_reg, tmpl_root);
    const auto prefab_guid = guid{array<byte, 16>{byte{11}, byte{22}, byte{33}, byte{44}}};
    ASSERT_TRUE(fix.prefab_cache.register_template(prefab_guid, tempest::move(tmpl)));

    // Act: Instantiate prefab as root entity in scene
    auto inst_cmd = instantiate_prefab_command{prefab_guid, scene};
    inst_cmd.apply(fix.ctx);

    // Assert: Verify command properties, created address, and entities
    EXPECT_EQ(inst_cmd.label(), "Instantiate Prefab");
    EXPECT_EQ(inst_cmd.target_scene(), scene);
    EXPECT_EQ(inst_cmd.prefab_guid(), prefab_guid);
    EXPECT_EQ(inst_cmd.instantiated_address().scene, scene);
    EXPECT_FALSE(inst_cmd.instantiated_address().id_path.empty());

    const auto root_ent = resolve_entity_address(fix.registry, fix.scene_mgr, inst_cmd.instantiated_address());
    ASSERT_NE(root_ent, tombstone_entity);
    EXPECT_TRUE(fix.registry.is_valid(root_ent));
    EXPECT_TRUE(fix.scene_mgr.is_dirty(scene));

    const auto roots_opt = fix.scene_mgr.get_root_entities(scene);
    ASSERT_TRUE(roots_opt.has_value());
    auto found_in_roots = false;
    for (const auto cand : *roots_opt)
    {
        if (cand == root_ent)
        {
            found_in_roots = true;
            break;
        }
    }
    EXPECT_TRUE(found_in_roots);

    const auto* comp = fix.registry.try_get<test_component>(root_ent);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->val, 42);

    const auto* rel = fix.registry.try_get<ecs::relationship_component<ecs::entity>>(root_ent);
    ASSERT_NE(rel, nullptr);
    const auto child_ent = rel->first_child;
    ASSERT_NE(child_ent, tombstone_entity);
    EXPECT_TRUE(fix.registry.is_valid(child_ent));

    const auto* child_comp = fix.registry.try_get<test_secondary_component>(child_ent);
    ASSERT_NE(child_comp, nullptr);
    EXPECT_EQ(child_comp->tag, 100);

    // Act: Revert instantiation command
    inst_cmd.revert(fix.ctx);

    // Assert: Root and child destroyed, root removed from scene roots
    EXPECT_EQ(resolve_entity_address(fix.registry, fix.scene_mgr, inst_cmd.instantiated_address()), tombstone_entity);
    EXPECT_FALSE(fix.registry.is_valid(root_ent));
    EXPECT_FALSE(fix.registry.is_valid(child_ent));

    const auto roots_after_undo = fix.scene_mgr.get_root_entities(scene);
    ASSERT_TRUE(roots_after_undo.has_value());
    auto found_after_undo = false;
    for (const auto cand : *roots_after_undo)
    {
        if (cand == root_ent)
        {
            found_after_undo = true;
            break;
        }
    }
    EXPECT_FALSE(found_after_undo);

    // Act: Reapply instantiation command (redo)
    inst_cmd.apply(fix.ctx);

    // Assert: Entities restored and registered in scene roots
    const auto restored_root = resolve_entity_address(fix.registry, fix.scene_mgr, inst_cmd.instantiated_address());
    ASSERT_NE(restored_root, tombstone_entity);
    EXPECT_TRUE(fix.registry.is_valid(restored_root));

    const auto* restored_comp = fix.registry.try_get<test_component>(restored_root);
    ASSERT_NE(restored_comp, nullptr);
    EXPECT_EQ(restored_comp->val, 42);

    const auto* restored_rel = fix.registry.try_get<ecs::relationship_component<ecs::entity>>(restored_root);
    ASSERT_NE(restored_rel, nullptr);
    const auto restored_child = restored_rel->first_child;
    ASSERT_NE(restored_child, tombstone_entity);
    EXPECT_TRUE(fix.registry.is_valid(restored_child));
    EXPECT_EQ(fix.registry.get<test_secondary_component>(restored_child).tag, 100);

    const auto roots_after_redo = fix.scene_mgr.get_root_entities(scene);
    ASSERT_TRUE(roots_after_redo.has_value());
    auto found_after_redo = false;
    for (const auto cand : *roots_after_redo)
    {
        if (cand == restored_root)
        {
            found_after_redo = true;
            break;
        }
    }
    EXPECT_TRUE(found_after_redo);
}

/// @brief Verifies that instantiate_prefab_command parented under an existing entity establishes hierarchy
/// without registering as a scene root, and preserves hierarchy through undo and redo.
TEST(entity_commands_test, instantiate_prefab_command_parented)
{
    // Setup: Scene with an existing parent entity and registered prefab template
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneInstantiateParented");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto create_parent = create_entity_command{scene, "ParentEntity"};
    create_parent.apply(fix.ctx);
    const auto parent_addr = create_parent.created_address();
    const auto parent_ent = resolve_entity_address(fix.registry, fix.scene_mgr, parent_addr);
    ASSERT_NE(parent_ent, tombstone_entity);

    auto t_reg = ecs::archetype_registry{fix.event_reg, fix.type_reg};
    const auto tmpl_root = t_reg.create();
    t_reg.name(tmpl_root, "ChildPrefabRoot");
    t_reg.assign(tmpl_root, test_component{.val = 77});

    auto tmpl = scene::prefab_template::from_entity(t_reg, tmpl_root);
    const auto prefab_guid = guid{array<byte, 16>{byte{55}, byte{66}, byte{77}, byte{88}}};
    ASSERT_TRUE(fix.prefab_cache.register_template(prefab_guid, tempest::move(tmpl)));

    // Act: Instantiate prefab parented under existing entity
    auto inst_cmd = instantiate_prefab_command{prefab_guid, parent_addr};
    inst_cmd.apply(fix.ctx);

    // Assert: Instantiated entity parented under parent_ent and not in scene roots
    const auto inst_ent = resolve_entity_address(fix.registry, fix.scene_mgr, inst_cmd.instantiated_address());
    ASSERT_NE(inst_ent, tombstone_entity);

    const auto* rel = fix.registry.try_get<ecs::relationship_component<ecs::entity>>(inst_ent);
    ASSERT_NE(rel, nullptr);
    EXPECT_EQ(rel->parent, parent_ent);

    const auto roots_opt = fix.scene_mgr.get_root_entities(scene);
    ASSERT_TRUE(roots_opt.has_value());
    for (const auto cand : *roots_opt)
    {
        EXPECT_NE(cand, inst_ent);
    }

    // Act: Revert command
    inst_cmd.revert(fix.ctx);

    // Assert: Instantiated entity destroyed, parent entity still intact
    EXPECT_EQ(resolve_entity_address(fix.registry, fix.scene_mgr, inst_cmd.instantiated_address()), tombstone_entity);
    EXPECT_TRUE(fix.registry.is_valid(parent_ent));

    // Act: Redo command
    inst_cmd.apply(fix.ctx);

    // Assert: Instantiated entity restored and reparented under parent_ent
    const auto restored_ent = resolve_entity_address(fix.registry, fix.scene_mgr, inst_cmd.instantiated_address());
    ASSERT_NE(restored_ent, tombstone_entity);
    const auto* restored_rel = fix.registry.try_get<ecs::relationship_component<ecs::entity>>(restored_ent);
    ASSERT_NE(restored_rel, nullptr);
    EXPECT_EQ(restored_rel->parent, parent_ent);
}

/// @brief Verifies that instantiate_prefab_command with an initial position assigns transform_component
/// with the specified position and preserves it through undo and redo.
TEST(entity_commands_test, instantiate_prefab_command_with_initial_position)
{
    // Setup: Scene and registered prefab template
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneInstantiatePosition");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto t_reg = ecs::archetype_registry{fix.event_reg, fix.type_reg};
    const auto tmpl_root = t_reg.create();
    t_reg.name(tmpl_root, "PosPrefabRoot");

    auto tmpl = scene::prefab_template::from_entity(t_reg, tmpl_root);
    const auto prefab_guid = guid{array<byte, 16>{byte{10}, byte{20}, byte{30}, byte{40}}};
    ASSERT_TRUE(fix.prefab_cache.register_template(prefab_guid, tempest::move(tmpl)));

    // Act: Instantiate prefab with explicit initial position
    const auto init_pos = math::vec3<float>{10.0F, 20.0F, 30.0F};
    auto inst_cmd = instantiate_prefab_command{prefab_guid, scene, nullopt, init_pos};
    inst_cmd.apply(fix.ctx);

    // Assert: Instantiated root has transform_component with matching position
    const auto inst_ent = resolve_entity_address(fix.registry, fix.scene_mgr, inst_cmd.instantiated_address());
    ASSERT_NE(inst_ent, tombstone_entity);

    const auto* tf = fix.registry.try_get<ecs::transform_component>(inst_ent);
    ASSERT_NE(tf, nullptr);
    EXPECT_FLOAT_EQ(tf->position().x, 10.0F);
    EXPECT_FLOAT_EQ(tf->position().y, 20.0F);
    EXPECT_FLOAT_EQ(tf->position().z, 30.0F);

    // Act: Revert and redo
    inst_cmd.revert(fix.ctx);
    inst_cmd.apply(fix.ctx);

    // Assert: Position preserved on restored entity
    const auto restored_ent = resolve_entity_address(fix.registry, fix.scene_mgr, inst_cmd.instantiated_address());
    ASSERT_NE(restored_ent, tombstone_entity);
    const auto* restored_tf = fix.registry.try_get<ecs::transform_component>(restored_ent);
    ASSERT_NE(restored_tf, nullptr);
    EXPECT_FLOAT_EQ(restored_tf->position().x, 10.0F);
    EXPECT_FLOAT_EQ(restored_tf->position().y, 20.0F);
    EXPECT_FLOAT_EQ(restored_tf->position().z, 30.0F);
}

/// @brief Verifies that instantiate_prefab_command fails gracefully without crashing when executed with a null prefab cache.
TEST(entity_commands_test, instantiate_prefab_command_null_cache_fails_gracefully)
{
    // Setup: Scene and command context with null prefab cache
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneInstantiateNullCache");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    fix.ctx.prefab_cache = nullptr;
    const auto dummy_guid = guid{array<byte, 16>{byte{11}, byte{22}, byte{33}, byte{44}}};

    // Act: Apply instantiate_prefab_command with null prefab cache
    auto inst_cmd = instantiate_prefab_command{dummy_guid, scene};
    inst_cmd.apply(fix.ctx);

    // Assert: No entity was instantiated
    const auto inst_addr = inst_cmd.instantiated_address();
    EXPECT_EQ(resolve_entity_address(fix.registry, fix.scene_mgr, inst_addr), tombstone_entity);

    // Act: Revert command
    inst_cmd.revert(fix.ctx);

    // Assert: State remains consistent and no entity exists
    EXPECT_EQ(resolve_entity_address(fix.registry, fix.scene_mgr, inst_addr), tombstone_entity);
}

/// @brief Verifies that assign_asset_command assigns a material GUID to an entity lacking material_component,
/// replaces existing material GUIDs, and restores previous states upon undo and redo.
TEST(entity_commands_test, assign_asset_command_material_lifecycle)
{
    // Setup: Scene with an entity lacking material_component
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneAssignMaterial");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto create_cmd = create_entity_command{scene, "MaterialTargetEntity"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    ASSERT_NE(ent, tombstone_entity);

    const auto mat_a = guid{array<byte, 16>{byte{1}, byte{2}, byte{3}, byte{4}}};
    const auto mat_b = guid{array<byte, 16>{byte{5}, byte{6}, byte{7}, byte{8}}};

    // Act: Assign initial material
    auto cmd_a = assign_asset_command::for_material(addr, mat_a);
    cmd_a.apply(fix.ctx);

    // Assert: Component added with mat_a, prev_asset is nullopt
    EXPECT_EQ(cmd_a.label(), "Assign Asset");
    EXPECT_EQ(cmd_a.target_scene(), scene);
    EXPECT_EQ(cmd_a.target_address(), addr);
    EXPECT_EQ(cmd_a.asset_guid(), mat_a);
    EXPECT_FALSE(cmd_a.prev_asset_guid().has_value());
    ASSERT_TRUE(fix.registry.has<core::material_component>(ent));
    EXPECT_EQ(fix.registry.get<core::material_component>(ent).material_id, mat_a);

    // Act: Revert initial material assignment
    cmd_a.revert(fix.ctx);

    // Assert: Component removed
    EXPECT_FALSE(fix.registry.has<core::material_component>(ent));

    // Act: Redo initial material assignment
    cmd_a.apply(fix.ctx);

    // Assert: Component restored with mat_a
    ASSERT_TRUE(fix.registry.has<core::material_component>(ent));
    EXPECT_EQ(fix.registry.get<core::material_component>(ent).material_id, mat_a);

    // Act: Replace material with mat_b
    auto cmd_b = assign_asset_command::for_material(addr, mat_b);
    cmd_b.apply(fix.ctx);

    // Assert: Component replaced with mat_b, prev_asset records mat_a
    EXPECT_EQ(cmd_b.asset_guid(), mat_b);
    ASSERT_TRUE(cmd_b.prev_asset_guid().has_value());
    EXPECT_EQ(*cmd_b.prev_asset_guid(), mat_a);
    EXPECT_EQ(fix.registry.get<core::material_component>(ent).material_id, mat_b);

    // Act: Revert replacement
    cmd_b.revert(fix.ctx);

    // Assert: Previous material mat_a restored
    EXPECT_EQ(fix.registry.get<core::material_component>(ent).material_id, mat_a);

    // Act: Redo replacement
    cmd_b.apply(fix.ctx);

    // Assert: Replacement mat_b reapplied
    EXPECT_EQ(fix.registry.get<core::material_component>(ent).material_id, mat_b);
}

/// @brief Verifies that assign_asset_command on an entity in a prefab instance records
/// the corresponding prefab override, and restores or reverts the override upon undo and redo.
TEST(entity_commands_test, assign_asset_command_material_prefab_override)
{
    // Setup: Scene with an instantiated prefab containing a material_component
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneAssignMatPrefab");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    const auto orig_mat = guid{array<byte, 16>{byte{100}, byte{101}, byte{102}}};
    const auto new_mat = guid{array<byte, 16>{byte{200}, byte{201}, byte{202}}};

    auto t_reg = ecs::archetype_registry{fix.event_reg, fix.type_reg};
    const auto tmpl_root = t_reg.create();
    t_reg.name(tmpl_root, "PrefabWithMaterial");
    t_reg.assign(tmpl_root, core::material_component{.material_id = orig_mat});

    auto tmpl = scene::prefab_template::from_entity(t_reg, tmpl_root);
    const auto prefab_guid = guid{array<byte, 16>{byte{90}, byte{91}, byte{92}, byte{93}}};
    ASSERT_TRUE(fix.prefab_cache.register_template(prefab_guid, tempest::move(tmpl)));

    auto inst_cmd = instantiate_prefab_command{prefab_guid, scene};
    inst_cmd.apply(fix.ctx);
    const auto inst_addr = inst_cmd.instantiated_address();
    const auto inst_ent = resolve_entity_address(fix.registry, fix.scene_mgr, inst_addr);
    ASSERT_NE(inst_ent, tombstone_entity);

    // Act: Assign new material to prefab instance entity
    auto mat_cmd = assign_asset_command::for_material(inst_addr, new_mat);
    mat_cmd.apply(fix.ctx);

    // Assert: Entity has new material and prefab instance records field override
    EXPECT_EQ(fix.registry.get<core::material_component>(inst_ent).material_id, new_mat);
    const auto* instance_comp = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp, nullptr);
    EXPECT_FALSE(instance_comp->overrides.field_overrides.empty());
    auto found_override = false;
    for (const auto& field_override : instance_comp->overrides.field_overrides)
    {
        if (field_override.field_name == "material_id")
        {
            found_override = true;
            break;
        }
    }
    EXPECT_TRUE(found_override);

    // Act: Revert material assignment
    mat_cmd.revert(fix.ctx);

    // Assert: Original material restored and field override removed
    EXPECT_EQ(fix.registry.get<core::material_component>(inst_ent).material_id, orig_mat);
    const auto* instance_comp_undone = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp_undone, nullptr);
    auto found_after_undo = false;
    for (const auto& field_override : instance_comp_undone->overrides.field_overrides)
    {
        if (field_override.field_name == "material_id")
        {
            found_after_undo = true;
            break;
        }
    }
    EXPECT_FALSE(found_after_undo);

    // Act: Redo material assignment
    mat_cmd.apply(fix.ctx);

    // Assert: New material restored and field override restored
    EXPECT_EQ(fix.registry.get<core::material_component>(inst_ent).material_id, new_mat);
    const auto* instance_comp_redone = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp_redone, nullptr);
    auto found_after_redo = false;
    for (const auto& field_override : instance_comp_redone->overrides.field_overrides)
    {
        if (field_override.field_name == "material_id")
        {
            found_after_redo = true;
            break;
        }
    }
    EXPECT_TRUE(found_after_redo);
}

/// @brief Verifies that assign_asset_command on an entity in a prefab instance that initially lacked a material
/// correctly records an added_component override, removes it on undo, and restores it on redo.
TEST(entity_commands_test, assign_asset_command_material_prefab_override_added_component)
{
    // Setup: Scene with an instantiated prefab lacking material_component
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneAssignMatAddedComp");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    const auto new_mat = guid{array<byte, 16>{byte{201}, byte{202}, byte{203}, byte{204}}};

    auto t_reg = ecs::archetype_registry{fix.event_reg, fix.type_reg};
    const auto tmpl_root = t_reg.create();
    t_reg.name(tmpl_root, "PrefabWithoutMaterial");
    t_reg.assign(tmpl_root, test_component{.val = 55});

    auto tmpl = scene::prefab_template::from_entity(t_reg, tmpl_root);
    const auto prefab_guid = guid{array<byte, 16>{byte{88}, byte{77}, byte{66}, byte{55}}};
    ASSERT_TRUE(fix.prefab_cache.register_template(prefab_guid, tempest::move(tmpl)));

    auto inst_cmd = instantiate_prefab_command{prefab_guid, scene};
    inst_cmd.apply(fix.ctx);
    const auto inst_addr = inst_cmd.instantiated_address();
    const auto inst_ent = resolve_entity_address(fix.registry, fix.scene_mgr, inst_addr);
    ASSERT_NE(inst_ent, tombstone_entity);
    ASSERT_FALSE(fix.registry.has<core::material_component>(inst_ent));

    // Act: Assign material to prefab instance entity lacking material_component
    auto mat_cmd = assign_asset_command::for_material(inst_addr, new_mat);
    mat_cmd.apply(fix.ctx);

    // Assert: Entity has new material and prefab instance records added_component override
    ASSERT_TRUE(fix.registry.has<core::material_component>(inst_ent));
    EXPECT_EQ(fix.registry.get<core::material_component>(inst_ent).material_id, new_mat);
    const auto* instance_comp = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp, nullptr);
    EXPECT_FALSE(instance_comp->overrides.added_components.empty());
    auto found_added = false;
    for (const auto& added : instance_comp->overrides.added_components)
    {
        if (added.component.type_info != nullptr &&
            added.component.type_info->name == fix.type_reg.type_info(fix.material_comp_idx).name)
        {
            found_added = true;
            break;
        }
    }
    EXPECT_TRUE(found_added);

    // Act: Revert material assignment
    mat_cmd.revert(fix.ctx);

    // Assert: Material component removed and added_component override removed
    EXPECT_FALSE(fix.registry.has<core::material_component>(inst_ent));
    const auto* instance_comp_undone = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp_undone, nullptr);
    auto found_after_undo = false;
    for (const auto& added : instance_comp_undone->overrides.added_components)
    {
        if (added.component.type_info != nullptr &&
            added.component.type_info->name == fix.type_reg.type_info(fix.material_comp_idx).name)
        {
            found_after_undo = true;
            break;
        }
    }
    EXPECT_FALSE(found_after_undo);

    // Act: Redo material assignment
    mat_cmd.apply(fix.ctx);

    // Assert: Material component restored and added_component override restored
    ASSERT_TRUE(fix.registry.has<core::material_component>(inst_ent));
    EXPECT_EQ(fix.registry.get<core::material_component>(inst_ent).material_id, new_mat);
    const auto* instance_comp_redone = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp_redone, nullptr);
    auto found_after_redo = false;
    for (const auto& added : instance_comp_redone->overrides.added_components)
    {
        if (added.component.type_info != nullptr &&
            added.component.type_info->name == fix.type_reg.type_info(fix.material_comp_idx).name)
        {
            found_after_redo = true;
            break;
        }
    }
    EXPECT_TRUE(found_after_redo);
}

/// @brief Verifies integration of instantiate_prefab_command and assign_asset_command
/// executed through command_history, ensuring end-to-end multi-step undo and redo.
TEST(entity_commands_test, command_history_integration_prefab_and_material)
{
    // Setup: Scene and command history with registered prefab template
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneHistoryIntegration");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    const auto orig_mat = guid{array<byte, 16>{byte{1}, byte{1}, byte{1}}};
    const auto new_mat = guid{array<byte, 16>{byte{2}, byte{2}, byte{2}}};

    auto t_reg = ecs::archetype_registry{fix.event_reg, fix.type_reg};
    const auto tmpl_root = t_reg.create();
    t_reg.name(tmpl_root, "HistoryPrefabRoot");
    t_reg.assign(tmpl_root, core::material_component{.material_id = orig_mat});

    auto tmpl = scene::prefab_template::from_entity(t_reg, tmpl_root);
    const auto prefab_guid = guid{array<byte, 16>{byte{71}, byte{72}, byte{73}, byte{74}}};
    ASSERT_TRUE(fix.prefab_cache.register_template(prefab_guid, tempest::move(tmpl)));

    auto history = command_history{};

    // Act: Execute instantiate_prefab_command via command_history
    auto inst_cmd = make_unique<instantiate_prefab_command>(prefab_guid, scene);
    const auto inst_ptr = inst_cmd.get();
    history.execute(tempest::move(inst_cmd), fix.ctx);

    const auto inst_addr = inst_ptr->instantiated_address();
    const auto inst_ent = resolve_entity_address(fix.registry, fix.scene_mgr, inst_addr);
    ASSERT_NE(inst_ent, tombstone_entity);
    EXPECT_EQ(fix.registry.get<core::material_component>(inst_ent).material_id, orig_mat);

    // Act: Execute assign_asset_command via command_history
    auto mat_cmd = make_unique<assign_asset_command>(assign_asset_command::for_material(inst_addr, new_mat));
    history.execute(tempest::move(mat_cmd), fix.ctx);

    // Assert: New material applied
    EXPECT_EQ(fix.registry.get<core::material_component>(inst_ent).material_id, new_mat);

    // Act: Undo material assignment
    EXPECT_TRUE(history.undo(fix.ctx, scene));

    // Assert: Material reverted to original
    EXPECT_EQ(fix.registry.get<core::material_component>(inst_ent).material_id, orig_mat);

    // Act: Undo prefab instantiation
    EXPECT_TRUE(history.undo(fix.ctx, scene));

    // Assert: Prefab instance destroyed
    EXPECT_EQ(resolve_entity_address(fix.registry, fix.scene_mgr, inst_addr), tombstone_entity);

    // Act: Redo prefab instantiation
    EXPECT_TRUE(history.redo(fix.ctx, scene));

    // Assert: Prefab instance restored with original material
    const auto restored_ent = resolve_entity_address(fix.registry, fix.scene_mgr, inst_addr);
    ASSERT_NE(restored_ent, tombstone_entity);
    EXPECT_EQ(fix.registry.get<core::material_component>(restored_ent).material_id, orig_mat);

    // Act: Redo material assignment
    EXPECT_TRUE(history.redo(fix.ctx, scene));

    // Assert: New material reapplied to restored entity
    EXPECT_EQ(fix.registry.get<core::material_component>(restored_ent).material_id, new_mat);
}

//==============================================================================
// assign_asset_command Tests
//==============================================================================

/// @brief Verifies that assign_asset_command assigns an asset GUID to an entity lacking the component,
/// replaces existing asset GUIDs, and restores previous states upon undo and redo.
TEST(entity_commands_test, assign_asset_command_lifecycle)
{
    // Setup: Scene with an entity lacking mesh_component
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneAssignAssetLifecycle");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto create_cmd = create_entity_command{scene, "MeshTargetEntity"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    ASSERT_NE(ent, tombstone_entity);

    const auto mesh_a = guid{array<byte, 16>{byte{10}, byte{11}, byte{12}, byte{13}}};
    const auto mesh_b = guid{array<byte, 16>{byte{20}, byte{21}, byte{22}, byte{23}}};

    // Act: Assign initial mesh asset
    auto cmd_a = assign_asset_command{addr, "mesh_component", "mesh_id", mesh_a};
    cmd_a.apply(fix.ctx);

    // Assert: Component added with mesh_a, prev_asset is nullopt, was_added_component is true
    EXPECT_EQ(cmd_a.label(), "Assign Asset");
    EXPECT_EQ(cmd_a.target_scene(), scene);
    EXPECT_EQ(cmd_a.target_address(), addr);
    EXPECT_EQ(cmd_a.component_name(), "mesh_component");
    EXPECT_EQ(cmd_a.field_name(), "mesh_id");
    EXPECT_EQ(cmd_a.asset_guid(), mesh_a);
    EXPECT_FALSE(cmd_a.prev_asset_guid().has_value());
    EXPECT_TRUE(cmd_a.was_added_component());
    EXPECT_FALSE(cmd_a.is_in_prefab());
    ASSERT_TRUE(fix.registry.has<core::mesh_component>(ent));
    EXPECT_EQ(fix.registry.get<core::mesh_component>(ent).mesh_id, mesh_a);

    // Act: Revert initial mesh assignment
    cmd_a.revert(fix.ctx);

    // Assert: Component removed
    EXPECT_FALSE(fix.registry.has<core::mesh_component>(ent));

    // Act: Redo initial mesh assignment
    cmd_a.apply(fix.ctx);

    // Assert: Component restored with mesh_a
    ASSERT_TRUE(fix.registry.has<core::mesh_component>(ent));
    EXPECT_EQ(fix.registry.get<core::mesh_component>(ent).mesh_id, mesh_a);

    // Act: Replace mesh with mesh_b
    auto cmd_b = assign_asset_command{addr, "mesh_component", "mesh_id", mesh_b};
    cmd_b.apply(fix.ctx);

    // Assert: Component replaced with mesh_b, prev_asset records mesh_a, was_added_component is false
    EXPECT_EQ(cmd_b.asset_guid(), mesh_b);
    ASSERT_TRUE(cmd_b.prev_asset_guid().has_value());
    EXPECT_EQ(*cmd_b.prev_asset_guid(), mesh_a);
    EXPECT_FALSE(cmd_b.was_added_component());
    EXPECT_EQ(fix.registry.get<core::mesh_component>(ent).mesh_id, mesh_b);

    // Act: Revert replacement
    cmd_b.revert(fix.ctx);

    // Assert: Previous mesh mesh_a restored
    EXPECT_EQ(fix.registry.get<core::mesh_component>(ent).mesh_id, mesh_a);

    // Act: Redo replacement
    cmd_b.apply(fix.ctx);

    // Assert: Replacement mesh_b reapplied
    EXPECT_EQ(fix.registry.get<core::mesh_component>(ent).mesh_id, mesh_b);
}

/// @brief Verifies that assign_asset_command on an entity in a prefab instance records
/// the corresponding field override, and restores or reverts the override upon undo and redo.
TEST(entity_commands_test, assign_asset_command_prefab_override)
{
    // Setup: Scene with an instantiated prefab containing a mesh_component
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneAssignAssetPrefab");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    const auto orig_mesh = guid{array<byte, 16>{byte{30}, byte{31}, byte{32}}};
    const auto new_mesh = guid{array<byte, 16>{byte{40}, byte{41}, byte{42}}};

    auto t_reg = ecs::archetype_registry{fix.event_reg, fix.type_reg};
    const auto tmpl_root = t_reg.create();
    t_reg.name(tmpl_root, "PrefabWithMesh");
    t_reg.assign(tmpl_root, core::mesh_component{.mesh_id = orig_mesh});

    auto tmpl = scene::prefab_template::from_entity(t_reg, tmpl_root);
    const auto prefab_guid = guid{array<byte, 16>{byte{60}, byte{61}, byte{62}, byte{63}}};
    ASSERT_TRUE(fix.prefab_cache.register_template(prefab_guid, tempest::move(tmpl)));

    auto inst_cmd = instantiate_prefab_command{prefab_guid, scene};
    inst_cmd.apply(fix.ctx);
    const auto inst_addr = inst_cmd.instantiated_address();
    const auto inst_ent = resolve_entity_address(fix.registry, fix.scene_mgr, inst_addr);
    ASSERT_NE(inst_ent, tombstone_entity);

    // Act: Assign new mesh to prefab instance entity
    auto mesh_cmd = assign_asset_command{inst_addr, "tempest::core::mesh_component", "mesh_id", new_mesh};
    mesh_cmd.apply(fix.ctx);

    // Assert: Entity has new mesh and prefab instance records field override
    EXPECT_TRUE(mesh_cmd.is_in_prefab());
    EXPECT_FALSE(mesh_cmd.was_added_component());
    EXPECT_EQ(fix.registry.get<core::mesh_component>(inst_ent).mesh_id, new_mesh);
    const auto* instance_comp = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp, nullptr);
    EXPECT_FALSE(instance_comp->overrides.field_overrides.empty());
    auto found_override = false;
    for (const auto& field_override : instance_comp->overrides.field_overrides)
    {
        if (field_override.field_name == "mesh_id")
        {
            found_override = true;
            break;
        }
    }
    EXPECT_TRUE(found_override);

    // Act: Revert mesh assignment
    mesh_cmd.revert(fix.ctx);

    // Assert: Original mesh restored and field override removed
    EXPECT_EQ(fix.registry.get<core::mesh_component>(inst_ent).mesh_id, orig_mesh);
    const auto* instance_comp_undone = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp_undone, nullptr);
    auto found_after_undo = false;
    for (const auto& field_override : instance_comp_undone->overrides.field_overrides)
    {
        if (field_override.field_name == "mesh_id")
        {
            found_after_undo = true;
            break;
        }
    }
    EXPECT_FALSE(found_after_undo);

    // Act: Redo mesh assignment
    mesh_cmd.apply(fix.ctx);

    // Assert: New mesh restored and field override restored
    EXPECT_EQ(fix.registry.get<core::mesh_component>(inst_ent).mesh_id, new_mesh);
    const auto* instance_comp_redone = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp_redone, nullptr);
    auto found_after_redo = false;
    for (const auto& field_override : instance_comp_redone->overrides.field_overrides)
    {
        if (field_override.field_name == "mesh_id")
        {
            found_after_redo = true;
            break;
        }
    }
    EXPECT_TRUE(found_after_redo);
}

/// @brief Verifies that assign_asset_command on an entity in a prefab instance that initially lacked a component
/// correctly records an added_component override, removes it on undo, and restores it on redo.
TEST(entity_commands_test, assign_asset_command_prefab_override_added_component)
{
    // Setup: Scene with an instantiated prefab lacking mesh_component
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneAssignAssetAddedComp");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    const auto new_mesh = guid{array<byte, 16>{byte{51}, byte{52}, byte{53}, byte{54}}};

    auto t_reg = ecs::archetype_registry{fix.event_reg, fix.type_reg};
    const auto tmpl_root = t_reg.create();
    t_reg.name(tmpl_root, "PrefabWithoutMesh");
    t_reg.assign(tmpl_root, test_component{.val = 77});

    auto tmpl = scene::prefab_template::from_entity(t_reg, tmpl_root);
    const auto prefab_guid = guid{array<byte, 16>{byte{81}, byte{82}, byte{83}, byte{84}}};
    ASSERT_TRUE(fix.prefab_cache.register_template(prefab_guid, tempest::move(tmpl)));

    auto inst_cmd = instantiate_prefab_command{prefab_guid, scene};
    inst_cmd.apply(fix.ctx);
    const auto inst_addr = inst_cmd.instantiated_address();
    const auto inst_ent = resolve_entity_address(fix.registry, fix.scene_mgr, inst_addr);
    ASSERT_NE(inst_ent, tombstone_entity);
    ASSERT_FALSE(fix.registry.has<core::mesh_component>(inst_ent));

    // Act: Assign mesh to prefab instance entity lacking mesh_component
    auto mesh_cmd = assign_asset_command{inst_addr, "mesh_component", "mesh_id", new_mesh};
    mesh_cmd.apply(fix.ctx);

    // Assert: Entity has new mesh, is_in_prefab is true, was_added_component is true, records added_component override
    EXPECT_TRUE(mesh_cmd.is_in_prefab());
    EXPECT_TRUE(mesh_cmd.was_added_component());
    ASSERT_TRUE(fix.registry.has<core::mesh_component>(inst_ent));
    EXPECT_EQ(fix.registry.get<core::mesh_component>(inst_ent).mesh_id, new_mesh);
    const auto* instance_comp = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp, nullptr);
    EXPECT_FALSE(instance_comp->overrides.added_components.empty());
    auto found_added = false;
    for (const auto& added : instance_comp->overrides.added_components)
    {
        if (added.component.type_info != nullptr &&
            added.component.type_info->name == fix.type_reg.type_info(fix.mesh_comp_idx).name)
        {
            found_added = true;
            break;
        }
    }
    EXPECT_TRUE(found_added);

    // Act: Revert mesh assignment
    mesh_cmd.revert(fix.ctx);

    // Assert: Mesh component removed and added_component override removed
    EXPECT_FALSE(fix.registry.has<core::mesh_component>(inst_ent));
    const auto* instance_comp_undone = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp_undone, nullptr);
    auto found_added_after_undo = false;
    for (const auto& added : instance_comp_undone->overrides.added_components)
    {
        if (added.component.type_info != nullptr &&
            added.component.type_info->name == fix.type_reg.type_info(fix.mesh_comp_idx).name)
        {
            found_added_after_undo = true;
            break;
        }
    }
    EXPECT_FALSE(found_added_after_undo);

    // Act: Redo mesh assignment
    mesh_cmd.apply(fix.ctx);

    // Assert: Mesh component restored and added_component override restored
    ASSERT_TRUE(fix.registry.has<core::mesh_component>(inst_ent));
    EXPECT_EQ(fix.registry.get<core::mesh_component>(inst_ent).mesh_id, new_mesh);
    const auto* instance_comp_redone = fix.registry.try_get<scene::prefab_instance_component>(inst_ent);
    ASSERT_NE(instance_comp_redone, nullptr);
    auto found_added_after_redo = false;
    for (const auto& added : instance_comp_redone->overrides.added_components)
    {
        if (added.component.type_info != nullptr &&
            added.component.type_info->name == fix.type_reg.type_info(fix.mesh_comp_idx).name)
        {
            found_added_after_redo = true;
            break;
        }
    }
    EXPECT_TRUE(found_added_after_redo);
}

/// @brief Verifies that assign_asset_command factory helpers correctly configure commands for material and mesh.
TEST(entity_commands_test, assign_asset_command_factories)
{
    // Setup: Scene with target entity
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneAssignAssetFactories");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto create_cmd = create_entity_command{scene, "FactoryTargetEntity"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    ASSERT_NE(ent, tombstone_entity);

    const auto mat_guid = guid{array<byte, 16>{byte{101}, byte{102}}};
    const auto mesh_guid = guid{array<byte, 16>{byte{201}, byte{202}}};

    // Act: Create commands via factory helpers
    auto mat_cmd = assign_asset_command::for_material(addr, mat_guid);
    auto mesh_cmd = assign_asset_command::for_mesh(addr, mesh_guid);

    // Assert: Helper properties configured accurately
    EXPECT_EQ(mat_cmd.component_name(), "tempest::core::material_component");
    EXPECT_EQ(mat_cmd.field_name(), "material_id");
    EXPECT_EQ(mat_cmd.asset_guid(), mat_guid);

    EXPECT_EQ(mesh_cmd.component_name(), "tempest::core::mesh_component");
    EXPECT_EQ(mesh_cmd.field_name(), "mesh_id");
    EXPECT_EQ(mesh_cmd.asset_guid(), mesh_guid);

    // Act: Apply factory commands
    mat_cmd.apply(fix.ctx);
    mesh_cmd.apply(fix.ctx);

    // Assert: Both components assigned with expected asset GUIDs
    ASSERT_TRUE(fix.registry.has<core::material_component>(ent));
    EXPECT_EQ(fix.registry.get<core::material_component>(ent).material_id, mat_guid);
    ASSERT_TRUE(fix.registry.has<core::mesh_component>(ent));
    EXPECT_EQ(fix.registry.get<core::mesh_component>(ent).mesh_id, mesh_guid);

    // Act: Revert both
    mesh_cmd.revert(fix.ctx);
    mat_cmd.revert(fix.ctx);

    // Assert: Both components removed
    EXPECT_FALSE(fix.registry.has<core::mesh_component>(ent));
    EXPECT_FALSE(fix.registry.has<core::material_component>(ent));
}

/// @brief Verifies that assign_asset_command rejects fields whose kind is neither asset_ref
/// nor guid, or whose size does not match sizeof(guid), leaving the entity and component intact.
TEST(entity_commands_test, assign_asset_command_invalid_field_kind_rejected)
{
    // Setup: Scene with an entity containing test_component
    constexpr auto tombstone_entity = static_cast<ecs::entity>(ecs::tombstone);
    auto fix = entity_commands_fixture{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneAssignAssetInvalidKind");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto create_cmd = create_entity_command{scene, "InvalidKindTargetEntity"};
    create_cmd.apply(fix.ctx);
    const auto addr = create_cmd.created_address();
    const auto ent = resolve_entity_address(fix.registry, fix.scene_mgr, addr);
    ASSERT_NE(ent, tombstone_entity);

    // Setup: Assign test_component with known scalar values
    constexpr auto initial_val = int32_t{42};
    constexpr auto initial_speed = 3.5F;
    fix.registry.assign(ent, test_component{.val = initial_val, .speed = initial_speed});

    const auto dummy_guid = guid{array<byte, 16>{byte{1}, byte{2}, byte{3}, byte{4}}};

    // Act: Attempt to assign asset to non-asset integer field 'val'
    auto invalid_cmd = assign_asset_command{addr, "test_component", "val", dummy_guid};
    invalid_cmd.apply(fix.ctx);

    // Assert: Component is unchanged and memory is not corrupted
    ASSERT_TRUE(fix.registry.has<test_component>(ent));
    const auto& comp_after_apply = fix.registry.get<test_component>(ent);
    EXPECT_EQ(comp_after_apply.val, initial_val);
    EXPECT_FLOAT_EQ(comp_after_apply.speed, initial_speed);

    // Act: Revert the invalid command
    invalid_cmd.revert(fix.ctx);

    // Assert: Component remains intact and unchanged
    ASSERT_TRUE(fix.registry.has<test_component>(ent));
    const auto& comp_after_revert = fix.registry.get<test_component>(ent);
    EXPECT_EQ(comp_after_revert.val, initial_val);
    EXPECT_FLOAT_EQ(comp_after_revert.speed, initial_speed);

    // Setup: Second entity without test_component
    auto create_cmd_b = create_entity_command{scene, "InvalidKindTargetEntityB"};
    create_cmd_b.apply(fix.ctx);
    const auto addr_b = create_cmd_b.created_address();
    const auto ent_b = resolve_entity_address(fix.registry, fix.scene_mgr, addr_b);
    ASSERT_NE(ent_b, tombstone_entity);

    // Act: Attempt to assign asset to non-asset field on entity lacking the component
    auto invalid_cmd_b = assign_asset_command{addr_b, "test_component", "val", dummy_guid};
    invalid_cmd_b.apply(fix.ctx);

    // Assert: Component was not added
    EXPECT_FALSE(fix.registry.has<test_component>(ent_b));

    // Act: Revert
    invalid_cmd_b.revert(fix.ctx);

    // Assert: Component still not present
    EXPECT_FALSE(fix.registry.has<test_component>(ent_b));
}



