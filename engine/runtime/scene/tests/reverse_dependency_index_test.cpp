#include <gtest/gtest.h>

#include <tempest/filesystem.hpp>
#include <tempest/guid.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/reverse_dependency_index.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>

//==============================================================================
// Registration & Query Tests
//==============================================================================

/// @brief Verifies registering asset references, querying them by asset GUID, and verifying has_references.
TEST(reverse_dependency_index_test, add_and_query_references)
{
    // Setup
    auto index = tempest::scene::reverse_dependency_index{};
    EXPECT_TRUE(index.empty());
    EXPECT_EQ(index.size(), 0U);

    const auto asset_id = tempest::guid::generate_random_guid();
    const auto scene_guid = tempest::guid::generate_random_guid();

    EXPECT_FALSE(index.has_references(asset_id));
    EXPECT_TRUE(index.find_references(asset_id).empty());

    auto location = tempest::scene::asset_reference_location{
        .owner_guid = scene_guid,
        .target_path = {},
        .component_name = "tempest::render_system::mesh_component",
        .field_name = "mesh_id",
        .owner_path = "assets/scenes/main.tscene",
    };
    location.target_path.push_back(tempest::scene::entity_local_id{42});

    // Act
    index.add_reference(asset_id, location);

    // Assert
    EXPECT_FALSE(index.empty());
    EXPECT_EQ(index.size(), 1U);
    EXPECT_TRUE(index.has_references(asset_id));
    const auto references = index.find_references(asset_id);
    ASSERT_EQ(references.size(), 1U);
    EXPECT_EQ(references[0].owner_guid, scene_guid);
    EXPECT_EQ(references[0].owner_path, "assets/scenes/main.tscene");
    EXPECT_EQ(references[0].component_name, "tempest::render_system::mesh_component");
    EXPECT_EQ(references[0].field_name, "mesh_id");
    ASSERT_EQ(references[0].target_path.size(), 1U);
    EXPECT_EQ(references[0].target_path[0].value, 42ULL);
}

/// @brief Verifies that adding identical reference locations does not produce duplicate entries.
TEST(reverse_dependency_index_test, duplicate_reference_idempotency)
{
    // Setup
    auto index = tempest::scene::reverse_dependency_index{};
    const auto asset_id = tempest::guid::generate_random_guid();
    const auto owner_guid = tempest::guid::generate_random_guid();

    const auto location = tempest::scene::asset_reference_location{
        .owner_guid = owner_guid,
        .target_path = {},
        .component_name = "tempest::render_system::mesh_component",
        .field_name = "mesh_id",
        .owner_path = "assets/prefabs/prop.tprefab",
    };

    // Act
    index.add_reference(asset_id, location);
    index.add_reference(asset_id, location);

    // Assert
    const auto references = index.find_references(asset_id);
    EXPECT_EQ(references.size(), 1U);
}

/// @brief Verifies tracking multiple references from distinct entities across scenes and prefabs.
TEST(reverse_dependency_index_test, multiple_references_and_assets)
{
    // Setup
    auto index = tempest::scene::reverse_dependency_index{};
    const auto asset_a = tempest::guid::generate_random_guid();
    const auto asset_b = tempest::guid::generate_random_guid();

    const auto scene_guid = tempest::guid::generate_random_guid();
    const auto prefab_guid = tempest::guid::generate_random_guid();

    auto loc_a1 = tempest::scene::asset_reference_location{
        .owner_guid = scene_guid,
        .target_path = {},
        .component_name = "tempest::render_system::mesh_component",
        .field_name = "mesh_id",
        .owner_path = "assets/scenes/level1.tscene",
    };
    loc_a1.target_path.push_back(tempest::scene::entity_local_id{100});

    auto loc_a2 = tempest::scene::asset_reference_location{
        .owner_guid = prefab_guid,
        .target_path = {},
        .component_name = "tempest::render_system::material_component",
        .field_name = "material_id",
        .owner_path = "assets/prefabs/tree.tprefab",
    };
    loc_a2.target_path.push_back(tempest::scene::entity_local_id{200});

    auto loc_b = tempest::scene::asset_reference_location{
        .owner_guid = scene_guid,
        .target_path = {},
        .component_name = "tempest::audio::audio_source_component",
        .field_name = "clip_id",
        .owner_path = "assets/scenes/level1.tscene",
    };
    loc_b.target_path.push_back(tempest::scene::entity_local_id{300});

    // Act
    index.add_reference(asset_a, loc_a1);
    index.add_reference(asset_a, loc_a2);
    index.add_reference(asset_b, loc_b);

    // Assert
    EXPECT_FALSE(index.empty());
    EXPECT_EQ(index.size(), 2U);
    EXPECT_TRUE(index.has_references(asset_a));
    EXPECT_TRUE(index.has_references(asset_b));
    EXPECT_EQ(index.find_references(asset_a).size(), 2U);
    EXPECT_EQ(index.find_references(asset_b).size(), 1U);
    EXPECT_EQ(index.all_dependencies().size(), 2U);
}

//==============================================================================
// Removal & Clear Tests
//==============================================================================

/// @brief Verifies removing references individually and pruning empty asset keys.
TEST(reverse_dependency_index_test, remove_reference)
{
    // Setup
    auto index = tempest::scene::reverse_dependency_index{};
    const auto asset_id = tempest::guid::generate_random_guid();
    const auto owner_guid = tempest::guid::generate_random_guid();

    auto loc1 = tempest::scene::asset_reference_location{
        .owner_guid = owner_guid,
        .target_path = {},
        .component_name = "tempest::render_system::mesh_component",
        .field_name = "mesh_id",
        .owner_path = "assets/scenes/main.tscene",
    };
    loc1.target_path.push_back(tempest::scene::entity_local_id{1});

    auto loc2 = tempest::scene::asset_reference_location{
        .owner_guid = owner_guid,
        .target_path = {},
        .component_name = "tempest::render_system::mesh_component",
        .field_name = "mesh_id",
        .owner_path = "assets/scenes/main.tscene",
    };
    loc2.target_path.push_back(tempest::scene::entity_local_id{2});

    index.add_reference(asset_id, loc1);
    index.add_reference(asset_id, loc2);
    ASSERT_EQ(index.find_references(asset_id).size(), 2U);

    // Act: Remove non-existent reference returns false
    auto non_existent_loc = loc1;
    non_existent_loc.field_name = "non_existent";
    EXPECT_FALSE(index.remove_reference(asset_id, non_existent_loc));

    // Act: Remove non-existent asset ID returns false
    const auto non_existent_asset = tempest::guid::generate_random_guid();
    EXPECT_FALSE(index.remove_reference(non_existent_asset, loc1));

    // Act: Remove first reference
    EXPECT_TRUE(index.remove_reference(asset_id, loc1));
    EXPECT_EQ(index.find_references(asset_id).size(), 1U);
    EXPECT_TRUE(index.has_references(asset_id));
    EXPECT_EQ(index.size(), 1U);

    // Act: Remove second reference (asset key should be erased)
    EXPECT_TRUE(index.remove_reference(asset_id, loc2));

    // Assert
    EXPECT_FALSE(index.has_references(asset_id));
    EXPECT_TRUE(index.find_references(asset_id).empty());
    EXPECT_TRUE(index.all_dependencies().empty());
    EXPECT_TRUE(index.empty());
    EXPECT_EQ(index.size(), 0U);
}

/// @brief Verifies that clear purges all indexed dependency data.
TEST(reverse_dependency_index_test, clear_purges_all_dependencies)
{
    // Setup
    auto index = tempest::scene::reverse_dependency_index{};
    const auto asset_id = tempest::guid::generate_random_guid();
    index.add_reference(asset_id, tempest::scene::asset_reference_location{
                                      .owner_guid = tempest::guid::generate_random_guid(),
                                      .target_path = {},
                                      .component_name = "mesh_component",
                                      .field_name = "mesh",
                                      .owner_path = "scene.tscene",
                                  });
    ASSERT_TRUE(index.has_references(asset_id));
    EXPECT_FALSE(index.empty());
    EXPECT_EQ(index.size(), 1U);

    // Act
    index.clear();

    // Assert
    EXPECT_FALSE(index.has_references(asset_id));
    EXPECT_TRUE(index.all_dependencies().empty());
    EXPECT_TRUE(index.empty());
    EXPECT_EQ(index.size(), 0U);
}

//==============================================================================
// JSON Codec Tests
//==============================================================================

/// @brief Verifies JSON serialization and deserialization preserves all metadata fields accurately.
TEST(reverse_dependency_index_test, json_round_trip_preserves_all_fields)
{
    // Setup
    auto original = tempest::scene::reverse_dependency_index{};
    const auto asset_a = tempest::guid::generate_random_guid();
    const auto asset_b = tempest::guid::generate_random_guid();
    const auto owner_scene = tempest::guid::generate_random_guid();
    const auto owner_prefab = tempest::guid::generate_random_guid();

    auto loc1 = tempest::scene::asset_reference_location{
        .owner_guid = owner_scene,
        .target_path = {},
        .component_name = "tempest::render_system::mesh_component",
        .field_name = "mesh_id",
        .owner_path = "assets/scenes/dungeon.tscene",
    };
    loc1.target_path.push_back(tempest::scene::entity_local_id{10});
    loc1.target_path.push_back(tempest::scene::entity_local_id{20});

    auto loc2 = tempest::scene::asset_reference_location{
        .owner_guid = owner_prefab,
        .target_path = {},
        .component_name = "tempest::physics::collider_component",
        .field_name = "mesh_id",
        .owner_path = "assets/prefabs/chest.tprefab",
    };
    loc2.target_path.push_back(tempest::scene::entity_local_id{5});

    auto loc3 = tempest::scene::asset_reference_location{
        .owner_guid = owner_scene,
        .target_path = {},
        .component_name = "tempest::audio::ambient_component",
        .field_name = "sound_id",
        .owner_path = "assets/scenes/dungeon.tscene",
    };

    original.add_reference(asset_a, loc1);
    original.add_reference(asset_a, loc2);
    original.add_reference(asset_b, loc3);

    // Act: Serialize to JSON string via standalone codec
    const auto json_payload = tempest::scene::reverse_dependency_codec::serialize(original);
    EXPECT_FALSE(json_payload.empty());

    // Act: Deserialize into a fresh expected index via standalone codec
    const auto reload_result = tempest::scene::reverse_dependency_codec::deserialize(json_payload);
    ASSERT_TRUE(reload_result.has_value());
    const auto& reloaded = *reload_result;

    // Assert: Verify asset_a references
    ASSERT_TRUE(reloaded.has_references(asset_a));
    const auto refs_a = reloaded.find_references(asset_a);
    ASSERT_EQ(refs_a.size(), 2U);
    EXPECT_EQ(refs_a[0].owner_guid, loc1.owner_guid);
    EXPECT_EQ(refs_a[0].owner_path, loc1.owner_path);
    EXPECT_EQ(refs_a[0].component_name, loc1.component_name);
    EXPECT_EQ(refs_a[0].field_name, loc1.field_name);
    ASSERT_EQ(refs_a[0].target_path.size(), 2U);
    EXPECT_EQ(refs_a[0].target_path[0].value, 10ULL);
    EXPECT_EQ(refs_a[0].target_path[1].value, 20ULL);

    EXPECT_EQ(refs_a[1].owner_guid, loc2.owner_guid);
    EXPECT_EQ(refs_a[1].owner_path, loc2.owner_path);
    EXPECT_EQ(refs_a[1].component_name, loc2.component_name);
    EXPECT_EQ(refs_a[1].field_name, loc2.field_name);
    ASSERT_EQ(refs_a[1].target_path.size(), 1U);
    EXPECT_EQ(refs_a[1].target_path[0].value, 5ULL);

    // Assert: Verify asset_b references
    ASSERT_TRUE(reloaded.has_references(asset_b));
    const auto refs_b = reloaded.find_references(asset_b);
    ASSERT_EQ(refs_b.size(), 1U);
    EXPECT_EQ(refs_b[0].owner_guid, loc3.owner_guid);
    EXPECT_EQ(refs_b[0].owner_path, loc3.owner_path);
    EXPECT_EQ(refs_b[0].component_name, loc3.component_name);
    EXPECT_EQ(refs_b[0].field_name, loc3.field_name);
    EXPECT_TRUE(refs_b[0].target_path.empty());
}

/// @brief Verifies that malformed, corrupted, or schema-invalid JSON strings return typed errors.
TEST(reverse_dependency_index_test, invalid_json_error_handling)
{
    // Act & Assert: Empty string fails with invalid_json
    const auto empty_result = tempest::scene::reverse_dependency_codec::deserialize(tempest::string_view{});
    ASSERT_FALSE(empty_result.has_value());
    EXPECT_EQ(empty_result.error(), tempest::scene::reverse_dependency_codec_error::invalid_json);

    // Act & Assert: Corrupt JSON syntax fails with invalid_json
    const auto syntax_result =
        tempest::scene::reverse_dependency_codec::deserialize(tempest::string_view{"{ not valid json }"});
    ASSERT_FALSE(syntax_result.has_value());
    EXPECT_EQ(syntax_result.error(), tempest::scene::reverse_dependency_codec_error::invalid_json);

    // Act & Assert: Non-object root fails with invalid_root_object
    const auto non_obj_result = tempest::scene::reverse_dependency_codec::deserialize(tempest::string_view{"[1, 2, 3]"});
    ASSERT_FALSE(non_obj_result.has_value());
    EXPECT_EQ(non_obj_result.error(), tempest::scene::reverse_dependency_codec_error::invalid_root_object);

    // Act & Assert: Missing dependencies member fails with missing_dependencies
    const auto missing_deps_result =
        tempest::scene::reverse_dependency_codec::deserialize(tempest::string_view{R"({ "version": 1 })"});
    ASSERT_FALSE(missing_deps_result.has_value());
    EXPECT_EQ(missing_deps_result.error(), tempest::scene::reverse_dependency_codec_error::missing_dependencies);

    // Act & Assert: Unsupported format identifier fails with unsupported_format
    const auto unsupported_fmt = tempest::scene::reverse_dependency_codec::deserialize(
        tempest::string_view{R"({ "format": "wrong.format", "version": 1, "dependencies": {} })"});
    ASSERT_FALSE(unsupported_fmt.has_value());
    EXPECT_EQ(unsupported_fmt.error(), tempest::scene::reverse_dependency_codec_error::unsupported_format);

    // Act & Assert: Future schema version fails with version_mismatch
    const auto future_ver = tempest::scene::reverse_dependency_codec::deserialize(
        tempest::string_view{R"({ "format": "tempest.reverse_dependencies", "version": 9999, "dependencies": {} })"});
    ASSERT_FALSE(future_ver.has_value());
    EXPECT_EQ(future_ver.error(), tempest::scene::reverse_dependency_codec_error::version_mismatch);
}

/// @brief Verifies deserialization of the alternative array-of-entries schema format.
TEST(reverse_dependency_index_test, array_schema_deserialization)
{
    // Setup
    const auto asset_guid_str = "01234567-89ab-cdef-0123-456789abcdef";
    const auto owner_guid_str = "fedcba98-7654-3210-fedc-ba9876543210";
    const auto expected_asset = *tempest::parse_guid(asset_guid_str);
    const auto expected_owner = *tempest::parse_guid(owner_guid_str);

    auto expected_path = tempest::scene::id_path{};
    expected_path.push_back(tempest::scene::entity_local_id{42});
    expected_path.push_back(tempest::scene::entity_local_id{99});

    auto json_payload = tempest::string{
        "{\n"
        "    \"format\": \"tempest.reverse_dependencies\",\n"
        "    \"version\": 1,\n"
        "    \"dependencies\": [\n"
        "        {\n"
        "            \"asset_guid\": \"01234567-89ab-cdef-0123-456789abcdef\",\n"
        "            \"references\": [\n"
        "                {\n"
        "                    \"owner_guid\": \"fedcba98-7654-3210-fedc-ba9876543210\",\n"
        "                    \"owner_path\": \"assets/scenes/array_test.tscene\",\n"
        "                    \"target_path\": \""};
    json_payload += tempest::scene::to_string(expected_path);
    json_payload +=
        "\",\n"
        "                    \"component\": \"tempest::render_system::mesh_component\",\n"
        "                    \"field\": \"mesh_id\"\n"
        "                }\n"
        "            ]\n"
        "        }\n"
        "    ]\n"
        "}";

    // Act
    const auto load_result = tempest::scene::reverse_dependency_codec::deserialize(json_payload);

    // Assert
    ASSERT_TRUE(load_result.has_value());
    const auto& index = *load_result;
    EXPECT_FALSE(index.empty());
    EXPECT_EQ(index.size(), 1U);
    ASSERT_TRUE(index.has_references(expected_asset));

    const auto refs = index.find_references(expected_asset);
    ASSERT_EQ(refs.size(), 1U);
    EXPECT_EQ(refs[0].owner_guid, expected_owner);
    EXPECT_EQ(refs[0].owner_path, "assets/scenes/array_test.tscene");
    EXPECT_EQ(refs[0].component_name, "tempest::render_system::mesh_component");
    EXPECT_EQ(refs[0].field_name, "mesh_id");
    ASSERT_EQ(refs[0].target_path.size(), 2U);
    EXPECT_EQ(refs[0].target_path[0].value, 42ULL);
    EXPECT_EQ(refs[0].target_path[1].value, 99ULL);
}

/// @brief Verifies that invalid or corrupted GUID strings in dictionary keys and array entries are skipped safely.
TEST(reverse_dependency_index_test, corrupted_guid_entries_are_safely_skipped)
{
    // Setup: Object schema with one invalid GUID key and one valid GUID key
    const auto valid_asset_guid_str = "01234567-89ab-cdef-0123-456789abcdef";
    const auto expected_asset = *tempest::parse_guid(valid_asset_guid_str);

    const auto dict_json = tempest::string_view{
        R"({
            "format": "tempest.reverse_dependencies",
            "version": 1,
            "dependencies": {
                "not-a-valid-guid": [
                    {
                        "owner_path": "corrupt.tscene",
                        "component": "comp",
                        "field": "fld"
                    }
                ],
                "01234567-89ab-cdef-0123-456789abcdef": [
                    {
                        "owner_path": "valid.tscene",
                        "component": "comp",
                        "field": "fld"
                    }
                ]
            }
        })"};

    // Act
    const auto dict_load_result = tempest::scene::reverse_dependency_codec::deserialize(dict_json);

    // Assert: Valid entry is loaded, corrupt GUID key is skipped
    ASSERT_TRUE(dict_load_result.has_value());
    const auto& dict_index = *dict_load_result;
    EXPECT_EQ(dict_index.size(), 1U);
    EXPECT_TRUE(dict_index.has_references(expected_asset));

    // Setup: Array schema with one invalid asset GUID and one valid asset GUID
    const auto array_json = tempest::string_view{
        R"({
            "format": "tempest.reverse_dependencies",
            "version": 1,
            "dependencies": [
                {
                    "asset_guid": "corrupt-asset-guid-value",
                    "references": [
                        {
                            "owner_path": "corrupt.tscene",
                            "component": "comp",
                            "field": "fld"
                        }
                    ]
                },
                {
                    "asset_guid": "01234567-89ab-cdef-0123-456789abcdef",
                    "references": [
                        {
                            "owner_path": "valid.tscene",
                            "component": "comp",
                            "field": "fld"
                        }
                    ]
                }
            ]
        })"};

    // Act
    const auto array_load_result = tempest::scene::reverse_dependency_codec::deserialize(array_json);

    // Assert: Valid entry is loaded, corrupt entry is skipped
    ASSERT_TRUE(array_load_result.has_value());
    const auto& array_index = *array_load_result;
    EXPECT_EQ(array_index.size(), 1U);
    EXPECT_TRUE(array_index.has_references(expected_asset));
}

//==============================================================================
// Filesystem Codec Tests
//==============================================================================

/// @brief Verifies writing the reverse dependency index to disk and loading it back.
TEST(reverse_dependency_index_test, filesystem_save_and_load_round_trip)
{
    // Setup
    auto index = tempest::scene::reverse_dependency_index{};
    const auto asset_id = tempest::guid::generate_random_guid();
    const auto owner_guid = tempest::guid::generate_random_guid();

    auto loc = tempest::scene::asset_reference_location{
        .owner_guid = owner_guid,
        .target_path = {},
        .component_name = "mesh_component",
        .field_name = "mesh",
        .owner_path = "assets/scenes/main.tscene",
    };
    loc.target_path.push_back(tempest::scene::entity_local_id{7});
    index.add_reference(asset_id, loc);

    const auto temp_path = tempest::filesystem::path{"bin/Debug/test_reverse_dependencies.json"};

    // Act: Save to disk via standalone codec
    const auto save_result = tempest::scene::reverse_dependency_codec::save_to_file(index, temp_path);
    ASSERT_TRUE(save_result.has_value());

    // Act: Load from disk into fresh expected index via standalone codec
    const auto load_result = tempest::scene::reverse_dependency_codec::load_from_file(temp_path);
    ASSERT_TRUE(load_result.has_value());
    const auto& reloaded = *load_result;

    // Assert: Verify loaded data
    ASSERT_TRUE(reloaded.has_references(asset_id));
    const auto refs = reloaded.find_references(asset_id);
    ASSERT_EQ(refs.size(), 1U);
    EXPECT_EQ(refs[0].owner_guid, owner_guid);
    EXPECT_EQ(refs[0].owner_path, "assets/scenes/main.tscene");
    ASSERT_EQ(refs[0].target_path.size(), 1U);
    EXPECT_EQ(refs[0].target_path[0].value, 7ULL);

    // Cleanup
    tempest::filesystem::remove(temp_path);
}

/// @brief Verifies that loading from a non-existent file path returns file_not_found error code gracefully.
TEST(reverse_dependency_index_test, load_from_non_existent_file_fails_gracefully)
{
    // Act
    const auto load_result =
        tempest::scene::reverse_dependency_codec::load_from_file("non_existent_directory/missing_dependencies.json");

    // Assert
    ASSERT_FALSE(load_result.has_value());
    EXPECT_EQ(load_result.error(), tempest::scene::reverse_dependency_codec_error::file_not_found);
}
