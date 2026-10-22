#include <gtest/gtest.h>

#include <tempest/scene/scene_reference_stripper.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/guid.hpp>
#include <tempest/json.hpp>
#include <tempest/reflection.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/reverse_dependency_index.hpp>
#include <tempest/scene/scene_asset_scanner.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

//=============================================================================
// Test Components & Reflection Registrations
//=============================================================================

namespace tempest_stripper_test
{
    struct test_mesh_component
    {
        tempest::guid mesh_id = {};
        uint32_t submesh_index = 0;
    };

    struct test_material_component
    {
        tempest::guid material_id = {};
    };
} // namespace tempest_stripper_test

namespace tempest::ecs
{
    template <>
    struct reflect<tempest_stripper_test::test_mesh_component>
    {
        static void describe(type_builder<tempest_stripper_test::test_mesh_component>& builder)
        {
            builder.version(1)
                .asset_ref<&tempest_stripper_test::test_mesh_component::mesh_id>("mesh_id")
                .field<&tempest_stripper_test::test_mesh_component::submesh_index>("submesh_index");
        }
    };

    template <>
    struct reflect<tempest_stripper_test::test_material_component>
    {
        static void describe(type_builder<tempest_stripper_test::test_material_component>& builder)
        {
            builder.version(1).asset_ref<&tempest_stripper_test::test_material_component::material_id>("material_id");
        }
    };
} // namespace tempest::ecs

namespace
{
    auto create_test_registry() -> tempest::ecs::component_type_registry
    {
        auto registry = tempest::ecs::component_type_registry{};
        registry.register_type<tempest_stripper_test::test_mesh_component>();
        registry.register_type<tempest_stripper_test::test_material_component>();
        return registry;
    }

    struct scoped_temp_file
    {
        tempest::filesystem::path path;

        explicit scoped_temp_file(tempest::string_view filename, tempest::string_view content)
        {
            path = tempest::filesystem::temp_directory_path() / filename;
            const auto span_bytes =
                tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(content.data()), content.size()};
            [[maybe_unused]] const auto res = tempest::write_file_from_bytes(path, span_bytes);
        }

        ~scoped_temp_file()
        {
            if (tempest::filesystem::exists(path))
            {
                tempest::filesystem::remove(path);
            }
        }

        scoped_temp_file(const scoped_temp_file&) = delete;
        scoped_temp_file(scoped_temp_file&&) = delete;
        auto operator=(const scoped_temp_file&) -> scoped_temp_file& = delete;
        auto operator=(scoped_temp_file&&) -> scoped_temp_file& = delete;
    };
} // namespace

//=============================================================================
// Scene File Reference Stripping Tests
//=============================================================================

/// @brief Verifies that strip_references_from_file removes referencing entities while retaining
/// non-referencing entities and leaving a well-formed JSON document on disk.
TEST(scene_reference_stripper_test, strip_scene_file_removes_matching_entity_and_preserves_others)
{
    // Setup: Type registry and a .tscene file containing two entities on disk
    const auto registry = create_test_registry();
    const auto target_mesh_guid = *tempest::parse_guid("aaaaaaaa-1111-2222-3333-444444444444");
    const auto other_mesh_guid = *tempest::parse_guid("bbbbbbbb-1111-2222-3333-444444444444");

    const auto scene_json = tempest::string_view{R"({
        "version": 1,
        "guid": "99999999-9999-9999-9999-999999999999",
        "entities": [
            {
                "id": "0000000000000001",
                "name": "TargetEntity",
                "components": {
                    "tempest_stripper_test::test_mesh_component": {
                        "mesh_id": "aaaaaaaa-1111-2222-3333-444444444444",
                        "submesh_index": 0
                    }
                }
            },
            {
                "id": "0000000000000002",
                "name": "KeptEntity",
                "components": {
                    "tempest_stripper_test::test_mesh_component": {
                        "mesh_id": "bbbbbbbb-1111-2222-3333-444444444444",
                        "submesh_index": 1
                    }
                }
            }
        ]
    })"};

    const auto temp_file = scoped_temp_file{"test_strip_scene_file.tscene", scene_json};

    // Act: Strip target asset references from the scene file
    const auto strip_res =
        tempest::scene::strip_references_from_file(temp_file.path, target_mesh_guid, registry);

    // Assert: Exactly 1 entity reference stripped and file rewritten cleanly
    ASSERT_TRUE(strip_res.has_value());
    EXPECT_EQ(*strip_res, 1U);

    const auto read_res = tempest::read_file_to_string(temp_file.path);
    ASSERT_TRUE(read_res.has_value());

    auto alloc = tempest::system_allocator{};
    auto doc_res = tempest::json_document::from_string(*read_res, alloc);
    ASSERT_TRUE(doc_res.has_value());

    const auto root = doc_res->root();
    ASSERT_TRUE(root.is_object());
    const auto root_obj = *root.as_object();

    const auto entities_val = root_obj["entities"];
    ASSERT_TRUE(entities_val.is_array());
    const auto entities_arr = *entities_val.as_array();
    ASSERT_EQ(entities_arr.size(), 1U);

    const auto remaining_ent = *entities_arr[0].as_object();
    EXPECT_EQ(remaining_ent["name"].as_string().value(), "KeptEntity");
    EXPECT_EQ(remaining_ent["id"].as_string().value(), "0000000000000002");

    const auto comps_obj = *remaining_ent["components"].as_object();
    const auto mesh_comp_obj = *comps_obj["tempest_stripper_test::test_mesh_component"].as_object();
    EXPECT_EQ(*tempest::parse_guid(mesh_comp_obj["mesh_id"].as_string().value()), other_mesh_guid);
}

//=============================================================================
// Prefab File Reference Stripping Tests
//=============================================================================

/// @brief Verifies that strip_references_from_file removes referencing child entities in a .tprefab file.
TEST(scene_reference_stripper_test, strip_prefab_file_removes_matching_child_entity)
{
    // Setup: Type registry and a .tprefab file with a root entity and a referencing child
    const auto registry = create_test_registry();
    const auto target_mat_guid = *tempest::parse_guid("cccccccc-1111-2222-3333-444444444444");

    const auto prefab_json = tempest::string_view{R"({
        "version": 1,
        "guid": "88888888-8888-8888-8888-888888888888",
        "entities": [
            {
                "id": "0000000000000001",
                "name": "PrefabRoot",
                "components": {},
                "children": [
                    {
                        "id": "0000000000000002",
                        "name": "ChildReferencingMaterial",
                        "components": {
                            "tempest_stripper_test::test_material_component": {
                                "material_id": "cccccccc-1111-2222-3333-444444444444"
                            }
                        }
                    }
                ]
            }
        ]
    })"};

    const auto temp_file = scoped_temp_file{"test_strip_prefab_file.tprefab", prefab_json};

    // Act: Strip target material references from the prefab file
    const auto strip_res =
        tempest::scene::strip_references_from_file(temp_file.path, target_mat_guid, registry);

    // Assert: 1 reference removed and root entity survives with child removed
    ASSERT_TRUE(strip_res.has_value());
    EXPECT_EQ(*strip_res, 1U);

    const auto read_res = tempest::read_file_to_string(temp_file.path);
    ASSERT_TRUE(read_res.has_value());

    auto alloc = tempest::system_allocator{};
    auto doc_res = tempest::json_document::from_string(*read_res, alloc);
    ASSERT_TRUE(doc_res.has_value());

    const auto root_obj = *doc_res->root().as_object();
    const auto entities_arr = *root_obj["entities"].as_array();
    ASSERT_EQ(entities_arr.size(), 1U);

    const auto root_ent = *entities_arr[0].as_object();
    EXPECT_EQ(root_ent["name"].as_string().value(), "PrefabRoot");

    const auto children_val = root_ent["children"];
    ASSERT_TRUE(children_val.is_array());
    EXPECT_EQ(children_val.as_array()->size(), 0U);
}

//=============================================================================
// Prefab Instance Stripping Tests
//=============================================================================

/// @brief Verifies that strip_references_from_file strips prefab instance entities whose prefab_guid matches target.
TEST(scene_reference_stripper_test, strip_prefab_instances_whose_prefab_guid_matches_target)
{
    // Setup: Scene containing a prefab instance referencing a deleted prefab asset
    const auto registry = create_test_registry();
    const auto deleted_prefab_guid = *tempest::parse_guid("dddddddd-1111-2222-3333-444444444444");

    const auto scene_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            {
                "id": "0000000000000001",
                "name": "DeletedPrefabInstance",
                "prefab": {
                    "guid": "dddddddd-1111-2222-3333-444444444444",
                    "instance_id": "0000000000000099"
                }
            },
            {
                "id": "0000000000000002",
                "name": "StandardEntity",
                "components": {}
            }
        ]
    })"};

    const auto temp_file = scoped_temp_file{"test_strip_prefab_instance.tscene", scene_json};

    // Act: Strip references to the deleted prefab GUID
    const auto strip_res =
        tempest::scene::strip_references_from_file(temp_file.path, deleted_prefab_guid, registry);

    // Assert: Exactly 1 prefab instance entity removed
    ASSERT_TRUE(strip_res.has_value());
    EXPECT_EQ(*strip_res, 1U);

    const auto read_res = tempest::read_file_to_string(temp_file.path);
    ASSERT_TRUE(read_res.has_value());

    auto alloc = tempest::system_allocator{};
    auto doc_res = tempest::json_document::from_string(*read_res, alloc);
    ASSERT_TRUE(doc_res.has_value());

    const auto root_obj = *doc_res->root().as_object();
    const auto entities_arr = *root_obj["entities"].as_array();
    ASSERT_EQ(entities_arr.size(), 1U);
    EXPECT_EQ((*entities_arr[0].as_object())["name"].as_string().value(), "StandardEntity");
}

/// @brief Verifies that strip_references_from_file strips matching field_overrides in a prefab instance while retaining the entity.
TEST(scene_reference_stripper_test, strip_prefab_instance_field_overrides_removes_override_and_preserves_instance)
{
    // Setup: Scene with a prefab instance having two field overrides (one referencing deleted asset, one scalar override)
    const auto registry = create_test_registry();
    const auto target_mesh_guid = *tempest::parse_guid("aaaaaaaa-1111-2222-3333-444444444444");

    const auto scene_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            {
                "id": "0000000000000001",
                "name": "PrefabInstanceWithOverrides",
                "prefab": {
                    "guid": "11111111-2222-3333-4444-555555555555",
                    "instance_id": "0000000000000099",
                    "field_overrides": [
                        {
                            "component": "tempest_stripper_test::test_mesh_component",
                            "field": "mesh_id",
                            "value": "aaaaaaaa-1111-2222-3333-444444444444"
                        },
                        {
                            "component": "tempest_stripper_test::test_mesh_component",
                            "field": "submesh_index",
                            "value": "42"
                        }
                    ]
                }
            }
        ]
    })"};

    const auto temp_file = scoped_temp_file{"test_strip_prefab_overrides.tscene", scene_json};

    // Act: Strip target mesh references from the scene file
    const auto strip_res =
        tempest::scene::strip_references_from_file(temp_file.path, target_mesh_guid, registry);

    // Assert: Exactly 1 override removed; the entity itself remains in the file
    ASSERT_TRUE(strip_res.has_value());
    EXPECT_EQ(*strip_res, 1U);

    const auto read_res = tempest::read_file_to_string(temp_file.path);
    ASSERT_TRUE(read_res.has_value());

    auto alloc = tempest::system_allocator{};
    auto doc_res = tempest::json_document::from_string(*read_res, alloc);
    ASSERT_TRUE(doc_res.has_value());

    const auto root_obj = *doc_res->root().as_object();
    const auto entities_arr = *root_obj["entities"].as_array();
    ASSERT_EQ(entities_arr.size(), 1U);

    const auto ent_obj = *entities_arr[0].as_object();
    EXPECT_EQ(ent_obj["name"].as_string().value(), "PrefabInstanceWithOverrides");

    const auto prefab_obj = *ent_obj["prefab"].as_object();
    const auto overrides_arr = *prefab_obj["field_overrides"].as_array();
    ASSERT_EQ(overrides_arr.size(), 1U);

    const auto remaining_override = *overrides_arr[0].as_object();
    EXPECT_EQ(remaining_override["field"].as_string().value(), "submesh_index");
}

/// @brief Verifies that strip_references_from_file cascades entity removal to children referencing a stripped parent in a flat hierarchy.
TEST(scene_reference_stripper_test, strip_scene_file_flat_hierarchy_cascades_deletion_to_children)
{
    // Setup: Scene with a flat hierarchy where entity 2 is child of entity 1, and entity 3 is child of entity 2
    const auto registry = create_test_registry();
    const auto target_mat_guid = *tempest::parse_guid("cccccccc-1111-2222-3333-444444444444");

    const auto scene_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            {
                "id": "0000000000000001",
                "name": "RootReferencingMaterial",
                "components": {
                    "tempest_stripper_test::test_material_component": {
                        "material_id": "cccccccc-1111-2222-3333-444444444444"
                    }
                }
            },
            {
                "id": "0000000000000002",
                "name": "ChildOfRoot",
                "parent": "0000000000000001",
                "components": {}
            },
            {
                "id": "0000000000000003",
                "name": "GrandchildOfRoot",
                "parent": "0000000000000002",
                "components": {}
            },
            {
                "id": "0000000000000004",
                "name": "IndependentEntity",
                "components": {}
            }
        ]
    })"};

    const auto temp_file = scoped_temp_file{"test_strip_flat_hierarchy.tscene", scene_json};

    // Act: Strip target material references from the scene file
    const auto strip_res =
        tempest::scene::strip_references_from_file(temp_file.path, target_mat_guid, registry);

    // Assert: Root and both descendant entities are stripped (total 3); independent entity survives
    ASSERT_TRUE(strip_res.has_value());
    EXPECT_EQ(*strip_res, 3U);

    const auto read_res = tempest::read_file_to_string(temp_file.path);
    ASSERT_TRUE(read_res.has_value());

    auto alloc = tempest::system_allocator{};
    auto doc_res = tempest::json_document::from_string(*read_res, alloc);
    ASSERT_TRUE(doc_res.has_value());

    const auto root_obj = *doc_res->root().as_object();
    const auto entities_arr = *root_obj["entities"].as_array();
    ASSERT_EQ(entities_arr.size(), 1U);

    const auto remaining_ent = *entities_arr[0].as_object();
    EXPECT_EQ(remaining_ent["name"].as_string().value(), "IndependentEntity");
    EXPECT_EQ(remaining_ent["id"].as_string().value(), "0000000000000004");
}

//=============================================================================
// Active Registry Stripping Tests
//=============================================================================

/// @brief Verifies that strip_references_from_registry identifies and destroys all live entities referencing target_asset_id.
TEST(scene_reference_stripper_test, strip_references_from_registry_destroys_matching_entities)
{
    // Setup: Live ECS registry with registered types and 3 active entities
    auto type_reg = create_test_registry();
    auto event_reg = tempest::event::event_registry{};
    auto registry = tempest::ecs::archetype_registry{event_reg, type_reg};

    const auto target_mesh_guid = *tempest::parse_guid("eeeeeeee-1111-2222-3333-444444444444");
    const auto other_mesh_guid = *tempest::parse_guid("ffffffff-1111-2222-3333-444444444444");
    const auto target_prefab_guid = *tempest::parse_guid("12345678-1234-1234-1234-123456789abc");

    const auto ent_mesh_target = registry.create();
    registry.assign(ent_mesh_target, tempest_stripper_test::test_mesh_component{
                                         .mesh_id = target_mesh_guid,
                                         .submesh_index = 0,
                                     });

    const auto ent_mesh_other = registry.create();
    registry.assign(ent_mesh_other, tempest_stripper_test::test_mesh_component{
                                        .mesh_id = other_mesh_guid,
                                        .submesh_index = 1,
                                    });

    const auto ent_prefab_target = registry.create();
    registry.assign(ent_prefab_target, tempest::scene::prefab_instance_component{
                                           .prefab_guid = target_prefab_guid,
                                       });

    // Act: Strip target mesh references from the live registry
    const auto destroyed_mesh =
        tempest::scene::strip_references_from_registry(registry, target_mesh_guid);

    // Assert: ent_mesh_target is destroyed; ent_mesh_other and ent_prefab_target remain valid
    ASSERT_EQ(destroyed_mesh.size(), 1U);
    EXPECT_EQ(destroyed_mesh[0], ent_mesh_target);
    EXPECT_FALSE(registry.is_valid(ent_mesh_target));
    EXPECT_TRUE(registry.is_valid(ent_mesh_other));
    EXPECT_TRUE(registry.is_valid(ent_prefab_target));

    // Act: Strip target prefab references from the live registry
    const auto destroyed_prefab =
        tempest::scene::strip_references_from_registry(registry, target_prefab_guid);

    // Assert: ent_prefab_target is destroyed; ent_mesh_other still survives
    ASSERT_EQ(destroyed_prefab.size(), 1U);
    EXPECT_EQ(destroyed_prefab[0], ent_prefab_target);
    EXPECT_FALSE(registry.is_valid(ent_prefab_target));
    EXPECT_TRUE(registry.is_valid(ent_mesh_other));
}

//=============================================================================
// Coordinator Index Updates Tests
//=============================================================================

/// @brief Verifies that strip_asset_references clears references from reverse_dependency_index and disk files.
TEST(scene_reference_stripper_test, strip_asset_references_updates_index_and_clears_dependencies)
{
    // Setup: Populate a reverse_dependency_index from a scene file on disk
    const auto registry = create_test_registry();
    const auto target_asset_guid = *tempest::parse_guid("77777777-1111-2222-3333-444444444444");

    const auto scene_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            {
                "id": "0000000000000001",
                "name": "ReferencingEntity",
                "components": {
                    "tempest_stripper_test::test_mesh_component": {
                        "mesh_id": "77777777-1111-2222-3333-444444444444",
                        "submesh_index": 0
                    }
                }
            }
        ]
    })"};

    const auto temp_file = scoped_temp_file{"test_coordinator_strip.tscene", scene_json};

    auto index = tempest::scene::reverse_dependency_index{};
    const auto scanned_count = tempest::scene::scene_asset_scanner::record_references_for_file(
        index, temp_file.path.generic_string(), scene_json, registry);
    ASSERT_EQ(scanned_count, 1U);
    ASSERT_TRUE(index.has_references(target_asset_guid));

    // Act: Run strip_asset_references coordinator
    const auto stripped_count =
        tempest::scene::strip_asset_references(index, target_asset_guid, registry);

    // Assert: Reference stripped from file and index is cleared
    EXPECT_EQ(stripped_count, 1U);
    EXPECT_FALSE(index.has_references(target_asset_guid));
    EXPECT_TRUE(index.find_references(target_asset_guid).empty());
}

//=============================================================================
// Scene Manager Integration Hook Tests
//=============================================================================

/// @brief Verifies that scene_manager::save_sync updates an attached reverse_dependency_index upon saving.
TEST(scene_reference_stripper_test, scene_manager_save_sync_updates_attached_reverse_dependency_index)
{
    // Setup: Registry and scene_manager with an attached reverse_dependency_index
    auto type_reg = create_test_registry();
    auto event_reg = tempest::event::event_registry{};
    auto registry = tempest::ecs::archetype_registry{event_reg, type_reg};

    auto manager = tempest::scene::scene_manager{registry};
    auto index = tempest::scene::reverse_dependency_index{};

    manager.set_reverse_dependency_index(&index);
    EXPECT_EQ(manager.get_reverse_dependency_index(), &index);

    // Act: Create an entity in the scene with a mesh component and save to disk
    const auto scene_res = manager.load_from_string_sync(R"({"version": 1, "entities": []})");
    ASSERT_TRUE(scene_res.has_value());
    const auto handle = *scene_res;

    const auto mesh_guid = *tempest::parse_guid("55555555-1111-2222-3333-444444444444");
    const auto ent = manager.create_entity(handle, "TestEntity");
    registry.assign(ent, tempest_stripper_test::test_mesh_component{
                             .mesh_id = mesh_guid,
                             .submesh_index = 0,
                         });

    const auto save_path = tempest::filesystem::temp_directory_path() / "test_manager_save_hook.tscene";
    const auto save_res = manager.save_sync(handle, save_path);

    // Assert: Save succeeded and attached index recorded the reference
    ASSERT_TRUE(save_res.has_value());
    EXPECT_TRUE(index.has_references(mesh_guid));

    const auto refs = index.find_references(mesh_guid);
    ASSERT_EQ(refs.size(), 1U);
    EXPECT_EQ(refs[0].owner_path, save_path.generic_string());
    EXPECT_EQ(refs[0].field_name, "mesh_id");

    if (tempest::filesystem::exists(save_path))
    {
        tempest::filesystem::remove(save_path);
    }
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/// @brief Verifies that strip_references_from_file returns file_not_found for a non-existent file path.
TEST(scene_reference_stripper_test, strip_references_from_file_returns_file_not_found_for_missing_file)
{
    // Setup: Missing path and test registry
    const auto registry = create_test_registry();
    const auto target_asset_guid = *tempest::parse_guid("11111111-2222-3333-4444-555555555555");
    const auto missing_path =
        tempest::filesystem::temp_directory_path() / "non_existent_file_for_strip_test_99999999.tscene";

    // Act: Attempt to strip references from non-existent file
    const auto strip_res =
        tempest::scene::strip_references_from_file(missing_path, target_asset_guid, registry);

    // Assert: Fails with scene_error::file_not_found
    ASSERT_FALSE(strip_res.has_value());
    EXPECT_EQ(strip_res.error(), tempest::scene::scene_error::file_not_found);
}

/// @brief Verifies that strip_references_from_file returns invalid_json for malformed or non-schema file content.
TEST(scene_reference_stripper_test, strip_references_from_file_returns_invalid_json_for_malformed_content)
{
    // Setup: Malformed JSON file on disk
    const auto registry = create_test_registry();
    const auto target_asset_guid = *tempest::parse_guid("11111111-2222-3333-4444-555555555555");
    const auto malformed_json = tempest::string_view{R"({ "version": 1, "entities": [ incomplete_json)"};

    const auto temp_file = scoped_temp_file{"test_strip_malformed.tscene", malformed_json};

    // Act: Attempt to strip references from invalid JSON
    const auto strip_res =
        tempest::scene::strip_references_from_file(temp_file.path, target_asset_guid, registry);

    // Assert: Fails with scene_error::invalid_json
    ASSERT_FALSE(strip_res.has_value());
    EXPECT_EQ(strip_res.error(), tempest::scene::scene_error::invalid_json);
}
