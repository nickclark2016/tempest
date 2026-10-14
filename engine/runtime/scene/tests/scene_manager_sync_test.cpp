#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/scene_codec.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

#if defined(_MSC_VER)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

namespace tempest_test
{
    struct test_sync_component
    {
        int32_t value = 42;
        float scale = 1.0F;
    };

    static_assert(tempest::ecs::component<test_sync_component>);
} // namespace tempest_test

namespace tempest::ecs
{
    template <>
    struct reflect<tempest_test::test_sync_component>
    {
        static void describe(type_builder<tempest_test::test_sync_component>& builder)
        {
            builder.version(1)
                .field<&tempest_test::test_sync_component::value>("value")
                .field<&tempest_test::test_sync_component::scale>("scale");
        }
    };
} // namespace tempest::ecs

namespace
{
    struct test_context
    {
        tempest::event::event_registry event_reg;
        tempest::ecs::component_type_registry type_reg;
        tempest::ecs::basic_archetype_registry registry{event_reg, type_reg};
        tempest::scene::scene_manager manager{registry};

        test_context()
        {
            const auto tc_idx = type_reg.ensure<tempest_test::test_sync_component>();
            type_reg.add_alias("test_sync_component", type_reg.type_info(tc_idx).name);

            const auto tf_idx = type_reg.ensure<tempest::ecs::transform_component>();
            type_reg.add_alias("transform_component", type_reg.type_info(tf_idx).name);
        }
    };
} // namespace

//==============================================================================
// Scene Manager Synchronous Lifecycle & Hierarchy Tests
//==============================================================================

/// @brief Verifies that multiple scenes can be loaded additively and an individual scene can be unloaded without affecting others.
TEST(scene_manager_sync_test, additive_loading_and_selective_unload)
{
    // Setup: Test context with registry and scene manager
    auto ctx = test_context{};
    const auto scene_a_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            { "id": "0x0000000000000001", "name": "EntityA1", "components": { "test_sync_component": { "value": 10, "scale": 1.0 } } },
            { "id": "0x0000000000000002", "name": "EntityA2", "components": { "test_sync_component": { "value": 20, "scale": 2.0 } } }
        ]
    })"};
    const auto scene_b_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            { "id": "0x0000000000000003", "name": "EntityB1", "components": { "test_sync_component": { "value": 30, "scale": 3.0 } } }
        ]
    })"};

    // Act: Load Scene A and Scene B additively
    const auto opts = tempest::scene::scene_load_options{
        .mode = tempest::scene::scene_load_mode::additive,
        .clear_first = false,
    };

    const auto handle_a_res = ctx.manager.load_from_string_sync(scene_a_json, "SceneA", opts);
    ASSERT_TRUE(handle_a_res.has_value());
    const auto handle_a = *handle_a_res;

    const auto handle_b_res = ctx.manager.load_from_string_sync(scene_b_json, "SceneB", opts);
    ASSERT_TRUE(handle_b_res.has_value());
    const auto handle_b = *handle_b_res;

    EXPECT_EQ(ctx.manager.get_scene_count(), 2U);
    EXPECT_TRUE(ctx.manager.is_valid(handle_a));
    EXPECT_TRUE(ctx.manager.is_valid(handle_b));

    const auto roots_a_opt = ctx.manager.get_root_entities(handle_a);
    ASSERT_TRUE(roots_a_opt.has_value());
    EXPECT_EQ(roots_a_opt->size(), 2U);
    const auto ent_a1 = (*roots_a_opt)[0];
    const auto ent_a2 = (*roots_a_opt)[1];

    const auto roots_b_opt = ctx.manager.get_root_entities(handle_b);
    ASSERT_TRUE(roots_b_opt.has_value());
    EXPECT_EQ(roots_b_opt->size(), 1U);
    const auto ent_b1 = (*roots_b_opt)[0];

    EXPECT_TRUE(ctx.registry.is_valid(ent_a1));
    EXPECT_TRUE(ctx.registry.is_valid(ent_a2));
    EXPECT_TRUE(ctx.registry.is_valid(ent_b1));

    // Act: Unload Scene A
    const auto unload_ok = ctx.manager.unload(handle_a);
    EXPECT_TRUE(unload_ok);

    // Assert: Scene A is invalid and its entities destroyed; Scene B remains valid and intact
    EXPECT_EQ(ctx.manager.get_scene_count(), 1U);
    EXPECT_FALSE(ctx.manager.is_valid(handle_a));
    EXPECT_TRUE(ctx.manager.is_valid(handle_b));

    EXPECT_FALSE(ctx.registry.is_valid(ent_a1));
    EXPECT_FALSE(ctx.registry.is_valid(ent_a2));
    EXPECT_TRUE(ctx.registry.is_valid(ent_b1));
}

/// @brief Verifies that loading in single mode without clear_first keeps prior scene intact until new scene succeeds.
TEST(scene_manager_sync_test, single_mode_atomic_replacement)
{
    // Setup: Load initial Scene 1 in single mode
    auto ctx = test_context{};
    const auto scene_1_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            { "id": "0x0000000000000001", "name": "Node1", "components": { "test_sync_component": { "value": 1, "scale": 1.0 } } }
        ]
    })"};
    const auto scene_2_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            { "id": "0x0000000000000002", "name": "Node2", "components": { "test_sync_component": { "value": 2, "scale": 2.0 } } }
        ]
    })"};

    const auto handle_1_res = ctx.manager.load_from_string_sync(scene_1_json, "Scene1");
    ASSERT_TRUE(handle_1_res.has_value());
    const auto handle_1 = *handle_1_res;
    EXPECT_EQ(ctx.manager.get_active_scene(), handle_1);

    const auto roots_1 = *ctx.manager.get_root_entities(handle_1);
    const auto ent_1 = roots_1[0];
    EXPECT_TRUE(ctx.registry.is_valid(ent_1));

    // Act: Load Scene 2 in single mode (atomic replacement)
    const auto handle_2_res = ctx.manager.load_from_string_sync(scene_2_json, "Scene2");
    ASSERT_TRUE(handle_2_res.has_value());
    const auto handle_2 = *handle_2_res;

    // Assert: Scene 1 unloaded, Scene 2 is active, Scene 1 entities destroyed, Scene 2 entities valid
    EXPECT_EQ(ctx.manager.get_scene_count(), 1U);
    EXPECT_FALSE(ctx.manager.is_valid(handle_1));
    EXPECT_TRUE(ctx.manager.is_valid(handle_2));
    EXPECT_EQ(ctx.manager.get_active_scene(), handle_2);

    EXPECT_FALSE(ctx.registry.is_valid(ent_1));

    const auto roots_2 = *ctx.manager.get_root_entities(handle_2);
    ASSERT_EQ(roots_2.size(), 1U);
    const auto ent_2 = roots_2[0];
    EXPECT_TRUE(ctx.registry.is_valid(ent_2));
    ASSERT_TRUE(ctx.registry.name(ent_2).has_value());
    EXPECT_EQ(*ctx.registry.name(ent_2), "Node2");
}

/// @brief Verifies that corrupted or malformed scene JSON fails to load and leaves prior scenes undisturbed.
TEST(scene_manager_sync_test, load_failure_rollback)
{
    // Setup: Test context with valid Scene 1 loaded
    auto ctx = test_context{};
    const auto scene_1_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            { "id": "0x0000000000000001", "name": "Node1", "components": { "test_sync_component": { "value": 1, "scale": 1.0 } } }
        ]
    })"};
    const auto handle_1_res = ctx.manager.load_from_string_sync(scene_1_json, "Scene1");
    ASSERT_TRUE(handle_1_res.has_value());
    const auto handle_1 = *handle_1_res;
    const auto initial_entity_count = ctx.registry.size();

    // Act: Attempt to load invalid JSON
    const auto bad_json = tempest::string_view{R"({ "version": 1, "entities": "not_an_array" })"};
    const auto bad_res = ctx.manager.load_from_string_sync(bad_json, "CorruptScene");

    // Assert: Loading fails, error is deserialization_failed, Scene 1 is completely intact
    EXPECT_FALSE(bad_res.has_value());
    EXPECT_EQ(bad_res.error(), tempest::scene::scene_error::deserialization_failed);
    EXPECT_EQ(ctx.manager.get_scene_count(), 1U);
    EXPECT_TRUE(ctx.manager.is_valid(handle_1));
    EXPECT_EQ(ctx.manager.get_active_scene(), handle_1);
    EXPECT_EQ(ctx.registry.size(), initial_entity_count);
}

/// @brief Verifies that dynamically spawned child entities parented to scene entities are destroyed when scene is unloaded.
TEST(scene_manager_sync_test, recursive_child_cleanup)
{
    // Setup: Load a scene with a root entity
    auto ctx = test_context{};
    const auto scene_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            { "id": "0x0000000000000001", "name": "SceneRoot", "components": { "test_sync_component": { "value": 1, "scale": 1.0 } } }
        ]
    })"};
    const auto handle_res = ctx.manager.load_from_string_sync(scene_json, "SceneWithChildren");
    ASSERT_TRUE(handle_res.has_value());
    const auto handle = *handle_res;

    const auto roots = *ctx.manager.get_root_entities(handle);
    const auto root_ent = roots[0];

    // Spawn dynamic child without scene_entity_component and attach to scene root
    const auto dynamic_child = ctx.registry.create();
    ctx.registry.name(dynamic_child, "DynamicChild");
    ctx.registry.assign(dynamic_child, tempest_test::test_sync_component{
                                           .value = 100,
                                           .scale = 2.0F,
                                       });
    ctx.registry.reparent(dynamic_child, root_ent);

    // Spawn dynamic grandchild attached to child
    const auto dynamic_grandchild = ctx.registry.create();
    ctx.registry.name(dynamic_grandchild, "DynamicGrandchild");
    ctx.registry.reparent(dynamic_grandchild, dynamic_child);

    EXPECT_TRUE(ctx.registry.is_valid(root_ent));
    EXPECT_TRUE(ctx.registry.is_valid(dynamic_child));
    EXPECT_TRUE(ctx.registry.is_valid(dynamic_grandchild));

    // Act: Unload scene
    const auto unload_ok = ctx.manager.unload(handle);
    EXPECT_TRUE(unload_ok);

    // Assert: Root, child, and grandchild are all destroyed
    EXPECT_FALSE(ctx.registry.is_valid(root_ent));
    EXPECT_FALSE(ctx.registry.is_valid(dynamic_child));
    EXPECT_FALSE(ctx.registry.is_valid(dynamic_grandchild));
    EXPECT_EQ(ctx.manager.get_scene_count(), 0U);
}

/// @brief Verifies active scene tracking, explicit clearing/setting, and entity creation with component tagging.
TEST(scene_manager_sync_test, active_scene_tracking_and_entity_creation)
{
    // Setup: Initially no scene is active
    auto ctx = test_context{};
    EXPECT_FALSE(ctx.manager.get_active_scene().is_valid());

    // Act & Assert: Load first scene and verify it becomes active
    const auto empty_scene_json = tempest::string_view{R"({ "version": 1, "entities": [] })"};
    const auto handle_res = ctx.manager.load_from_string_sync(empty_scene_json, "SceneA");
    ASSERT_TRUE(handle_res.has_value());
    const auto handle_a = *handle_res;
    EXPECT_EQ(ctx.manager.get_active_scene(), handle_a);

    // Create entity via manager
    const auto new_ent = ctx.manager.create_entity(handle_a, "PlayerNode");
    EXPECT_TRUE(ctx.registry.is_valid(new_ent));
    ASSERT_TRUE(ctx.registry.name(new_ent).has_value());
    EXPECT_EQ(*ctx.registry.name(new_ent), "PlayerNode");

    // Verify scene_entity_component
    const auto* comp = ctx.registry.try_get<tempest::scene::scene_entity_component>(new_ent);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->scene, handle_a);
    EXPECT_TRUE(comp->local_id.is_valid());

    // Verify entity is in root_entities
    const auto roots = ctx.manager.get_root_entities(handle_a);
    ASSERT_TRUE(roots.has_value());
    EXPECT_EQ(roots->size(), 1U);
    EXPECT_EQ((*roots)[0], new_ent);

    // Clear active scene
    const auto clear_ok = ctx.manager.set_active_scene(tempest::scene::scene_handle{});
    EXPECT_TRUE(clear_ok);
    EXPECT_FALSE(ctx.manager.get_active_scene().is_valid());

    // Restore active scene
    const auto set_ok = ctx.manager.set_active_scene(handle_a);
    EXPECT_TRUE(set_ok);
    EXPECT_EQ(ctx.manager.get_active_scene(), handle_a);

    // Unloading active scene clears active_scene
    ctx.manager.unload(handle_a);
    EXPECT_FALSE(ctx.manager.get_active_scene().is_valid());
}

/// @brief Verifies creating entities, saving scene to string, unloading, reloading, and ensuring component and hierarchy fidelity.
TEST(scene_manager_sync_test, save_and_reload_scene)
{
    // Setup: Load empty scene and create entities with components
    auto ctx = test_context{};
    const auto empty_scene = tempest::string_view{R"({ "version": 1, "entities": [] })"};
    const auto handle_res = ctx.manager.load_from_string_sync(empty_scene, "SaveTestScene");
    ASSERT_TRUE(handle_res.has_value());
    const auto handle_1 = *handle_res;

    const auto root_ent = ctx.manager.create_entity(handle_1, "RootNode");
    ctx.registry.assign(root_ent, tempest_test::test_sync_component{
                                      .value = 123,
                                      .scale = 4.5F,
                                  });

    const auto child_ent = ctx.manager.create_entity(handle_1, "ChildNode");
    ctx.registry.assign(child_ent, tempest_test::test_sync_component{
                                       .value = 456,
                                       .scale = 9.0F,
                                   });
    ctx.registry.reparent(child_ent, root_ent);

    // Act: Save scene to string and unload
    const auto save_res = ctx.manager.save_to_string(handle_1);
    ASSERT_TRUE(save_res.has_value());
    const auto saved_json = *save_res;

    ctx.manager.unload(handle_1);
    EXPECT_EQ(ctx.manager.get_scene_count(), 0U);
    EXPECT_FALSE(ctx.registry.is_valid(root_ent));
    EXPECT_FALSE(ctx.registry.is_valid(child_ent));

    // Reload from saved string
    const auto reload_res = ctx.manager.load_from_string_sync(saved_json, "ReloadedScene");
    ASSERT_TRUE(reload_res.has_value());
    const auto handle_2 = *reload_res;

    // Assert: Reloaded scene has correct roots, children, and component data
    EXPECT_EQ(ctx.manager.get_scene_count(), 1U);
    const auto reloaded_roots = ctx.manager.get_root_entities(handle_2);
    ASSERT_TRUE(reloaded_roots.has_value());
    ASSERT_EQ(reloaded_roots->size(), 1U);

    const auto reloaded_root = (*reloaded_roots)[0];
    ASSERT_TRUE(ctx.registry.name(reloaded_root).has_value());
    EXPECT_EQ(*ctx.registry.name(reloaded_root), "RootNode");
    const auto* root_tc = ctx.registry.try_get<tempest_test::test_sync_component>(reloaded_root);
    ASSERT_NE(root_tc, nullptr);
    EXPECT_EQ(root_tc->value, 123);
    EXPECT_FLOAT_EQ(root_tc->scale, 4.5F);

    const auto* rel = ctx.registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(reloaded_root);
    ASSERT_NE(rel, nullptr);
    ASSERT_TRUE(ctx.registry.is_valid(rel->first_child));
    const auto reloaded_child = rel->first_child;
    ASSERT_TRUE(ctx.registry.name(reloaded_child).has_value());
    EXPECT_EQ(*ctx.registry.name(reloaded_child), "ChildNode");
    const auto* child_tc = ctx.registry.try_get<tempest_test::test_sync_component>(reloaded_child);
    ASSERT_NE(child_tc, nullptr);
    EXPECT_EQ(child_tc->value, 456);
    EXPECT_FLOAT_EQ(child_tc->scale, 9.0F);
}

/// @brief Verifies filesystem roundtrip saving and loading with path tracking.
TEST(scene_manager_sync_test, filesystem_save_and_load_sync)
{
    // Setup: Load empty scene and create entities
    auto ctx = test_context{};
    const auto empty_scene = tempest::string_view{R"({ "version": 1, "entities": [] })"};
    const auto handle_res = ctx.manager.load_from_string_sync(empty_scene, "DiskScene");
    ASSERT_TRUE(handle_res.has_value());
    const auto handle = *handle_res;

    const auto ent = ctx.manager.create_entity(handle, "DiskEntity");
    ctx.registry.assign(ent, tempest_test::test_sync_component{
                                 .value = 777,
                                 .scale = 7.7F,
                             });

    const auto temp_path = tempest::filesystem::path{"bin/Debug/test_sync_scene.tscene"};

    // Act: Save to filesystem path
    const auto save_res = ctx.manager.save_sync(handle, temp_path);
    ASSERT_TRUE(save_res.has_value());

    // Verify path tracking on handle
    const auto path_opt = ctx.manager.get_scene_path(handle);
    ASSERT_TRUE(path_opt.has_value());
    EXPECT_EQ(*path_opt, temp_path.generic_string());

    ctx.manager.unload(handle);
    EXPECT_EQ(ctx.manager.get_scene_count(), 0U);

    // Load from filesystem path
    const auto load_res = ctx.manager.load_sync(temp_path);
    ASSERT_TRUE(load_res.has_value());
    const auto reloaded_handle = *load_res;

    // Assert: Entity restored from disk with correct path and component values
    EXPECT_EQ(ctx.manager.get_scene_count(), 1U);
    const auto loaded_path_opt = ctx.manager.get_scene_path(reloaded_handle);
    ASSERT_TRUE(loaded_path_opt.has_value());
    EXPECT_EQ(*loaded_path_opt, temp_path.generic_string());

    const auto reloaded_roots = *ctx.manager.get_root_entities(reloaded_handle);
    ASSERT_EQ(reloaded_roots.size(), 1U);
    const auto loaded_ent = reloaded_roots[0];
    ASSERT_TRUE(ctx.registry.name(loaded_ent).has_value());
    EXPECT_EQ(*ctx.registry.name(loaded_ent), "DiskEntity");
    const auto* tc = ctx.registry.try_get<tempest_test::test_sync_component>(loaded_ent);
    ASSERT_NE(tc, nullptr);
    EXPECT_EQ(tc->value, 777);
    EXPECT_FLOAT_EQ(tc->scale, 7.7F);
}
