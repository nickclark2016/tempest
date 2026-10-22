#include <tempest/scene/scene_asset_scanner.hpp>

#include <tempest/component_type_registry.hpp>
#include <tempest/guid.hpp>
#include <tempest/reflection.hpp>
#include <tempest/scene/reverse_dependency_index.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

//=============================================================================
// Test Component Declarations & Reflection
//=============================================================================

namespace tempest_scanner_test
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

    struct test_light_component
    {
        tempest::guid texture_id = {};
        float intensity = 1.0F;
    };
} // namespace tempest_scanner_test

namespace tempest::ecs
{
    template <>
    struct reflect<tempest_scanner_test::test_mesh_component>
    {
        static void describe(type_builder<tempest_scanner_test::test_mesh_component>& builder)
        {
            builder.version(1)
                .asset_ref<&tempest_scanner_test::test_mesh_component::mesh_id>("mesh_id")
                .field<&tempest_scanner_test::test_mesh_component::submesh_index>("submesh_index");
        }
    };

    template <>
    struct reflect<tempest_scanner_test::test_material_component>
    {
        static void describe(type_builder<tempest_scanner_test::test_material_component>& builder)
        {
            builder.version(1).asset_ref<&tempest_scanner_test::test_material_component::material_id>("material_id");
        }
    };

    template <>
    struct reflect<tempest_scanner_test::test_light_component>
    {
        static void describe(type_builder<tempest_scanner_test::test_light_component>& builder)
        {
            builder.version(1)
                .asset_ref<&tempest_scanner_test::test_light_component::texture_id>("texture_id")
                .field<&tempest_scanner_test::test_light_component::intensity>("intensity");
        }
    };
} // namespace tempest::ecs

namespace
{
    auto create_test_registry() -> tempest::ecs::component_type_registry
    {
        auto registry = tempest::ecs::component_type_registry{};
        registry.register_type<tempest_scanner_test::test_mesh_component>();
        registry.register_type<tempest_scanner_test::test_material_component>();
        registry.register_type<tempest_scanner_test::test_light_component>();
        return registry;
    }
} // namespace

//=============================================================================
// Scene Asset Reference Scanning Tests
//=============================================================================

/// @brief Verifies that scene_asset_scanner detects reflected asset_ref fields across multiple root entities.
TEST(scene_asset_scanner_test, scan_scene_with_multiple_entities_and_reflected_components)
{
    // Setup: Component type registry with reflected mesh and material components
    const auto registry = create_test_registry();
    const auto scene_guid = *tempest::parse_guid("11111111-2222-3333-4444-555555555555");
    const auto mesh_guid_a = *tempest::parse_guid("aaaaaaaa-1111-2222-3333-444444444444");
    const auto mat_guid_a = *tempest::parse_guid("bbbbbbbb-1111-2222-3333-444444444444");
    const auto mesh_guid_b = *tempest::parse_guid("cccccccc-1111-2222-3333-444444444444");

    const auto scene_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            {
                "id": "0000000000000001",
                "name": "EntityOne",
                "components": {
                    "tempest_scanner_test::test_mesh_component": {
                        "mesh_id": "aaaaaaaa-1111-2222-3333-444444444444",
                        "submesh_index": 0
                    },
                    "tempest_scanner_test::test_material_component": {
                        "material_id": "bbbbbbbb-1111-2222-3333-444444444444"
                    }
                }
            },
            {
                "id": "0000000000000002",
                "name": "EntityTwo",
                "components": {
                    "tempest_scanner_test::test_mesh_component": {
                        "mesh_id": "cccccccc-1111-2222-3333-444444444444",
                        "submesh_index": 1
                    }
                }
            }
        ]
    })"};

    // Act: Scan document for asset references
    const auto refs = tempest::scene::scene_asset_scanner::scan_document(
        scene_json, registry, "assets/scenes/level.tscene", scene_guid);

    // Assert: Discovered 3 references targeting the two entities
    ASSERT_EQ(refs.size(), 3U);

    EXPECT_EQ(refs[0].asset_id, mesh_guid_a);
    EXPECT_EQ(refs[0].location.owner_guid, scene_guid);
    EXPECT_EQ(refs[0].location.owner_path, "assets/scenes/level.tscene");
    EXPECT_EQ(refs[0].location.field_name, "mesh_id");
    ASSERT_EQ(refs[0].location.target_path.size(), 1U);
    EXPECT_EQ(refs[0].location.target_path[0].value, 1ULL);

    EXPECT_EQ(refs[1].asset_id, mat_guid_a);
    EXPECT_EQ(refs[1].location.owner_guid, scene_guid);
    EXPECT_EQ(refs[1].location.field_name, "material_id");
    ASSERT_EQ(refs[1].location.target_path.size(), 1U);
    EXPECT_EQ(refs[1].location.target_path[0].value, 1ULL);

    EXPECT_EQ(refs[2].asset_id, mesh_guid_b);
    EXPECT_EQ(refs[2].location.field_name, "mesh_id");
    ASSERT_EQ(refs[2].location.target_path.size(), 1U);
    EXPECT_EQ(refs[2].location.target_path[0].value, 2ULL);
}

/// @brief Verifies that hierarchical entity traversal accurately records nested id_path segments in target_path.
TEST(scene_asset_scanner_test, scan_nested_child_entities_hierarchical_paths)
{
    // Setup: Registry and JSON document containing root, child, and grandchild entities
    const auto registry = create_test_registry();
    const auto child_mesh_guid = *tempest::parse_guid("aaaa0000-0000-0000-0000-000000000001");
    const auto grandchild_mat_guid = *tempest::parse_guid("bbbb0000-0000-0000-0000-000000000002");

    const auto scene_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            {
                "id": "000000000000000a",
                "name": "RootEntity",
                "components": {},
                "children": [
                    {
                        "id": "0000000000000014",
                        "name": "ChildEntity",
                        "components": {
                            "tempest_scanner_test::test_mesh_component": {
                                "mesh_id": "aaaa0000-0000-0000-0000-000000000001",
                                "submesh_index": 0
                            }
                        },
                        "children": [
                            {
                                "id": "000000000000001e",
                                "name": "GrandchildEntity",
                                "components": {
                                    "tempest_scanner_test::test_material_component": {
                                        "material_id": "bbbb0000-0000-0000-0000-000000000002"
                                    }
                                }
                            }
                        ]
                    }
                ]
            }
        ]
    })"};

    // Act: Scan document
    const auto refs = tempest::scene::scene_asset_scanner::scan_document(
        scene_json, registry, "assets/scenes/hierarchy.tscene");

    // Assert: Child entity path is [0xA, 0x14] and grandchild entity path is [0xA, 0x14, 0x1E]
    ASSERT_EQ(refs.size(), 2U);

    EXPECT_EQ(refs[0].asset_id, child_mesh_guid);
    ASSERT_EQ(refs[0].location.target_path.size(), 2U);
    EXPECT_EQ(refs[0].location.target_path[0].value, 0x0aULL);
    EXPECT_EQ(refs[0].location.target_path[1].value, 0x14ULL);

    EXPECT_EQ(refs[1].asset_id, grandchild_mat_guid);
    ASSERT_EQ(refs[1].location.target_path.size(), 3U);
    EXPECT_EQ(refs[1].location.target_path[0].value, 0x0aULL);
    EXPECT_EQ(refs[1].location.target_path[1].value, 0x14ULL);
    EXPECT_EQ(refs[1].location.target_path[2].value, 0x1eULL);
}

//=============================================================================
// Prefab Instance & Override Scanning Tests
//=============================================================================

/// @brief Verifies that prefab instance GUIDs, field overrides, and added components are all scanned.
TEST(scene_asset_scanner_test, scan_prefab_instances_and_overrides)
{
    // Setup: Registry and scene with prefab instance block
    const auto registry = create_test_registry();
    const auto prefab_guid = *tempest::parse_guid("99999999-8888-7777-6666-555555555555");
    const auto override_mat_guid = *tempest::parse_guid("88888888-7777-6666-5555-444444444444");
    const auto added_light_texture_guid = *tempest::parse_guid("77777777-6666-5555-4444-333333333333");

    const auto scene_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            {
                "id": "0000000000000050",
                "name": "HouseInstance",
                "prefab": {
                    "guid": "99999999-8888-7777-6666-555555555555",
                    "instance_id": "0000000000000050",
                    "overrides": [
                        {
                            "target": ["0000000000000050", "0000000000000051"],
                            "component": "tempest_scanner_test::test_material_component",
                            "field": "material_id",
                            "value": "88888888-7777-6666-5555-444444444444"
                        }
                    ],
                    "added_components": [
                        {
                            "target": ["0000000000000050"],
                            "component": "tempest_scanner_test::test_light_component",
                            "properties": {
                                "texture_id": "77777777-6666-5555-4444-333333333333",
                                "intensity": 5.0
                            }
                        }
                    ]
                }
            }
        ]
    })"};

    // Act: Scan document
    const auto refs = tempest::scene::scene_asset_scanner::scan_document(
        scene_json, registry, "assets/scenes/village.tscene");

    // Assert: Discovered prefab instance reference, override reference, and added component reference
    ASSERT_EQ(refs.size(), 3U);

    // Prefab asset GUID reference
    EXPECT_EQ(refs[0].asset_id, prefab_guid);
    EXPECT_EQ(refs[0].location.component_name, "tempest::scene::prefab_instance_component");
    EXPECT_EQ(refs[0].location.field_name, "prefab_guid");
    ASSERT_EQ(refs[0].location.target_path.size(), 1U);
    EXPECT_EQ(refs[0].location.target_path[0].value, 0x50ULL);

    // Field override on material_id targeting child entity [0x50, 0x51]
    EXPECT_EQ(refs[1].asset_id, override_mat_guid);
    EXPECT_EQ(refs[1].location.field_name, "material_id");
    ASSERT_EQ(refs[1].location.target_path.size(), 2U);
    EXPECT_EQ(refs[1].location.target_path[0].value, 0x50ULL);
    EXPECT_EQ(refs[1].location.target_path[1].value, 0x51ULL);

    // Added component on root entity [0x50]
    EXPECT_EQ(refs[2].asset_id, added_light_texture_guid);
    EXPECT_EQ(refs[2].location.field_name, "texture_id");
    ASSERT_EQ(refs[2].location.target_path.size(), 1U);
    EXPECT_EQ(refs[2].location.target_path[0].value, 0x50ULL);
}

//=============================================================================
// Prefab Template Document Scanning Tests
//=============================================================================

/// @brief Verifies that a .tprefab document is correctly parsed and uses root "guid" as owner_guid.
TEST(scene_asset_scanner_test, scan_prefab_document)
{
    // Setup: Prefab template JSON with root GUID and template entities
    const auto registry = create_test_registry();
    const auto prefab_guid = *tempest::parse_guid("12345678-1234-1234-1234-123456789abc");
    const auto mesh_guid = *tempest::parse_guid("face0000-0000-0000-0000-000000000001");
    const auto child_mat_guid = *tempest::parse_guid("face0000-0000-0000-0000-000000000002");

    const auto prefab_json = tempest::string_view{R"({
        "format": "tempest.prefab",
        "format_version": 1,
        "guid": "12345678-1234-1234-1234-123456789abc",
        "entities": [
            {
                "id": "0000000000000100",
                "name": "TemplateRoot",
                "components": {
                    "tempest_scanner_test::test_mesh_component": {
                        "mesh_id": "face0000-0000-0000-0000-000000000001",
                        "submesh_index": 0
                    }
                },
                "children": [
                    {
                        "id": "0000000000000101",
                        "name": "TemplateChild",
                        "components": {
                            "tempest_scanner_test::test_material_component": {
                                "material_id": "face0000-0000-0000-0000-000000000002"
                            }
                        }
                    }
                ]
            }
        ]
    })"};

    // Act: Scan document without explicitly passing owner_guid
    const auto refs = tempest::scene::scene_asset_scanner::scan_document(
        prefab_json, registry, "assets/prefabs/prop.tprefab");

    // Assert: Discovered 2 references and owner_guid automatically matched root "guid"
    ASSERT_EQ(refs.size(), 2U);

    EXPECT_EQ(refs[0].asset_id, mesh_guid);
    EXPECT_EQ(refs[0].location.owner_guid, prefab_guid);
    EXPECT_EQ(refs[0].location.owner_path, "assets/prefabs/prop.tprefab");
    ASSERT_EQ(refs[0].location.target_path.size(), 1U);
    EXPECT_EQ(refs[0].location.target_path[0].value, 0x100ULL);

    EXPECT_EQ(refs[1].asset_id, child_mat_guid);
    EXPECT_EQ(refs[1].location.owner_guid, prefab_guid);
    ASSERT_EQ(refs[1].location.target_path.size(), 2U);
    EXPECT_EQ(refs[1].location.target_path[0].value, 0x100ULL);
    EXPECT_EQ(refs[1].location.target_path[1].value, 0x101ULL);
}

//=============================================================================
// Incremental Update Tests
//=============================================================================

/// @brief Verifies that remove_references_for_file prunes file entries and unreferenced asset keys.
TEST(scene_asset_scanner_test, remove_references_for_file_prunes_empty_keys)
{
    // Setup: Index populated with references from two separate files
    auto index = tempest::scene::reverse_dependency_index{};
    const auto asset_a = *tempest::parse_guid("aaaa1111-0000-0000-0000-000000000000");
    const auto asset_shared = *tempest::parse_guid("bbbb2222-0000-0000-0000-000000000000");
    const auto asset_c = *tempest::parse_guid("cccc3333-0000-0000-0000-000000000000");

    auto loc_file1_a = tempest::scene::asset_reference_location{
        .owner_guid = {},
        .target_path = {},
        .component_name = "test_comp",
        .field_name = "ref_a",
        .owner_path = "scenes/file1.tscene",
    };
    loc_file1_a.target_path.push_back(tempest::scene::entity_local_id{1});
    index.add_reference(asset_a, loc_file1_a);

    auto loc_file1_shared = tempest::scene::asset_reference_location{
        .owner_guid = {},
        .target_path = {},
        .component_name = "test_comp",
        .field_name = "ref_shared",
        .owner_path = "scenes/file1.tscene",
    };
    loc_file1_shared.target_path.push_back(tempest::scene::entity_local_id{1});
    index.add_reference(asset_shared, loc_file1_shared);

    auto loc_file2_shared = tempest::scene::asset_reference_location{
        .owner_guid = {},
        .target_path = {},
        .component_name = "test_comp",
        .field_name = "ref_shared",
        .owner_path = "scenes/file2.tscene",
    };
    loc_file2_shared.target_path.push_back(tempest::scene::entity_local_id{2});
    index.add_reference(asset_shared, loc_file2_shared);

    auto loc_file2_c = tempest::scene::asset_reference_location{
        .owner_guid = {},
        .target_path = {},
        .component_name = "test_comp",
        .field_name = "ref_c",
        .owner_path = "scenes/file2.tscene",
    };
    loc_file2_c.target_path.push_back(tempest::scene::entity_local_id{2});
    index.add_reference(asset_c, loc_file2_c);

    ASSERT_EQ(index.size(), 3U);
    EXPECT_TRUE(index.has_references(asset_a));
    EXPECT_TRUE(index.has_references(asset_shared));
    EXPECT_TRUE(index.has_references(asset_c));

    // Act: Remove all references for file1.tscene
    const auto removed_count = tempest::scene::remove_references_for_file(index, "scenes/file1.tscene");

    // Assert: Exactly 2 references removed; asset_a pruned; asset_shared and asset_c retained
    EXPECT_EQ(removed_count, 2U);
    EXPECT_FALSE(index.has_references(asset_a));
    EXPECT_TRUE(index.has_references(asset_shared));
    EXPECT_TRUE(index.has_references(asset_c));
    EXPECT_EQ(index.size(), 2U);

    const auto shared_refs = index.find_references(asset_shared);
    ASSERT_EQ(shared_refs.size(), 1U);
    EXPECT_EQ(shared_refs[0].owner_path, "scenes/file2.tscene");
}

/// @brief Verifies that record_references_for_file replaces existing file references atomically.
TEST(scene_asset_scanner_test, record_references_for_file_incremental_update)
{
    // Setup: Registry and index with initial document content referencing mesh A
    const auto registry = create_test_registry();
    auto index = tempest::scene::reverse_dependency_index{};

    const auto mesh_a = *tempest::parse_guid("aaaa0000-1111-2222-3333-444455556666");
    const auto mesh_b = *tempest::parse_guid("bbbb0000-1111-2222-3333-444455556666");

    const auto initial_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            {
                "id": "0000000000000001",
                "components": {
                    "tempest_scanner_test::test_mesh_component": {
                        "mesh_id": "aaaa0000-1111-2222-3333-444455556666"
                    }
                }
            }
        ]
    })"};

    // Initial record
    const auto initial_recorded = tempest::scene::record_references_for_file(
        index, "scenes/dyn.tscene", initial_json, registry);
    EXPECT_EQ(initial_recorded, 1U);
    EXPECT_TRUE(index.has_references(mesh_a));
    EXPECT_FALSE(index.has_references(mesh_b));

    // Act: Update document content to swap mesh A for mesh B
    const auto updated_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            {
                "id": "0000000000000001",
                "components": {
                    "tempest_scanner_test::test_mesh_component": {
                        "mesh_id": "bbbb0000-1111-2222-3333-444455556666"
                    }
                }
            }
        ]
    })"};

    const auto updated_recorded = tempest::scene::record_references_for_file(
        index, "scenes/dyn.tscene", updated_json, registry);

    // Assert: Old mesh_a reference pruned, new mesh_b reference registered
    EXPECT_EQ(updated_recorded, 1U);
    EXPECT_FALSE(index.has_references(mesh_a));
    EXPECT_TRUE(index.has_references(mesh_b));
    EXPECT_EQ(index.size(), 1U);
}

//=============================================================================
// Robustness & Edge Cases
//=============================================================================

/// @brief Verifies that malformed JSON, empty entities, or nil GUIDs are safely handled without errors.
TEST(scene_asset_scanner_test, malformed_and_edge_case_handling)
{
    // Setup: Registry and malformed or edge case documents
    const auto registry = create_test_registry();

    // Malformed JSON returns empty
    const auto malformed_refs = tempest::scene::scene_asset_scanner::scan_document(
        "{ bad_json", registry, "file.tscene");
    EXPECT_TRUE(malformed_refs.empty());

    // Non-object root returns empty
    const auto non_obj_refs = tempest::scene::scene_asset_scanner::scan_document(
        "[1, 2, 3]", registry, "file.tscene");
    EXPECT_TRUE(non_obj_refs.empty());

    // Empty entities array returns empty
    const auto empty_entities_refs = tempest::scene::scene_asset_scanner::scan_document(
        R"({ "version": 1, "entities": [] })", registry, "file.tscene");
    EXPECT_TRUE(empty_entities_refs.empty());

    // Nil GUID (all zeroes) is not registered as an asset reference
    const auto nil_guid_json = tempest::string_view{R"({
        "version": 1,
        "entities": [
            {
                "id": "0000000000000001",
                "components": {
                    "tempest_scanner_test::test_mesh_component": {
                        "mesh_id": "00000000-0000-0000-0000-000000000000"
                    }
                }
            }
        ]
    })"};
    const auto nil_refs = tempest::scene::scene_asset_scanner::scan_document(
        nil_guid_json, registry, "file.tscene");
    EXPECT_TRUE(nil_refs.empty());
}
