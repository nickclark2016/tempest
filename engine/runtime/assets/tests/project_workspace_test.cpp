#include <gtest/gtest.h>

#include <tempest/asset_database.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/guid.hpp>
#include <tempest/project_workspace.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace
{
    struct mock_mesh_asset
    {
        int32_t vertex_count{0};
    };

    struct mock_material_asset
    {
        int32_t shader_index{0};
    };

    struct mock_texture_asset
    {
        int32_t width{0};
        int32_t height{0};
    };

    inline void write_test_file(const tempest::filesystem::path& target_path,
                                 tempest::string_view file_content = {})
    {
        const auto byte_span = tempest::span<const tempest::byte>(
            reinterpret_cast<const tempest::byte*>(file_content.data()), file_content.size());
        (void)tempest::write_file_from_bytes(target_path, byte_span);
    }
} // namespace

// ============================================================================
// Section 1: Project File Serialization (.tproject)
// ============================================================================

/// @brief Verifies that a .tproject configuration file correctly preserves its project name,
/// root directory, mount table, startup scene (both GUID and relative path), and bake settings
/// across a disk save and load round-trip.
TEST(project_workspace_tests, project_file_round_trip)
{
    // Setup: Construct a comprehensive project workspace configuration
    const auto temp_dir = tempest::filesystem::path("tempest_test_project_roundtrip");
    tempest::filesystem::create_directories(temp_dir);

    const auto project_file_path = (temp_dir / "test_project.tproject").generic_string();

    auto original_workspace = tempest::assets::project_workspace{};
    original_workspace.name = "TempestTestProject";
    original_workspace.root_directory = temp_dir.generic_string();

    original_workspace.mounts.push_back(tempest::assets::mount_point{
        .path = "assets",
        .priority = 10,
        .alias = "content",
    });
    original_workspace.mounts.push_back(tempest::assets::mount_point{
        .path = "engine_core",
        .priority = 0,
        .alias = "engine",
    });

    const auto scene_guid = tempest::guid::generate_random_guid();
    original_workspace.startup_scene.scene_guid = scene_guid;
    original_workspace.startup_scene.scene_path = "scenes/main_stage.scene";

    original_workspace.settings.output_directory = "custom_cache/baked";
    original_workspace.settings.compress_textures = true;
    original_workspace.settings.custom_settings.insert({"target_platform", "vulkan"});
    original_workspace.settings.custom_settings.insert({"mip_filter", "kaiser"});

    // Act: Save workspace to .tproject file and reload it
    const auto save_result = original_workspace.save_to_file(project_file_path);
    ASSERT_TRUE(save_result);

    auto loaded_workspace = tempest::assets::project_workspace{};
    const auto load_result = loaded_workspace.load_from_file(project_file_path);
    ASSERT_TRUE(load_result);

    // Assert: All fields match original values exactly
    EXPECT_EQ(loaded_workspace.name, original_workspace.name);
    EXPECT_EQ(loaded_workspace.root_directory, original_workspace.root_directory);
    ASSERT_EQ(loaded_workspace.mounts.size(), original_workspace.mounts.size());
    EXPECT_EQ(loaded_workspace.mounts[0], original_workspace.mounts[0]);
    EXPECT_EQ(loaded_workspace.mounts[1], original_workspace.mounts[1]);
    EXPECT_EQ(loaded_workspace.startup_scene.scene_guid, original_workspace.startup_scene.scene_guid);
    EXPECT_EQ(loaded_workspace.startup_scene.scene_path, original_workspace.startup_scene.scene_path);
    EXPECT_EQ(loaded_workspace.settings.output_directory, original_workspace.settings.output_directory);
    EXPECT_EQ(loaded_workspace.settings.compress_textures, original_workspace.settings.compress_textures);
    EXPECT_EQ(loaded_workspace.settings.custom_settings, original_workspace.settings.custom_settings);
    EXPECT_EQ(loaded_workspace, original_workspace);

    // Teardown: Clean up temporary test files
    tempest::filesystem::remove_all(temp_dir);
}

// ============================================================================
// Section 2: Sidecar Metadata Serialization (.tmeta)
// ============================================================================

/// @brief Verifies that a .tmeta sidecar metadata file survives a complete serialization and
/// deserialization round-trip, preserving source GUID, importer identifier, importer version,
/// import settings, and sub-assets map bit-for-bit.
TEST(project_workspace_tests, sidecar_meta_round_trip)
{
    // Setup: Construct sidecar metadata with mock importer settings and sub-assets
    const auto temp_dir = tempest::filesystem::path("tempest_test_sidecar_roundtrip");
    tempest::filesystem::create_directories(temp_dir);

    const auto meta_file_path = (temp_dir / "character.model.tmeta").generic_string();

    auto original_meta = tempest::assets::sidecar_meta{};
    original_meta.source_guid = tempest::guid::generate_random_guid();
    original_meta.importer = "gltf";
    original_meta.importer_version = 2;

    original_meta.settings.insert({"generate_normals", "true"});
    original_meta.settings.insert({"tangent_algorithm", "mikktspace"});

    const auto mesh_guid = tempest::guid::generate_random_guid();
    const auto mat_guid = tempest::guid::generate_random_guid();
    const auto prefab_guid = tempest::guid::generate_random_guid();

    original_meta.sub_assets.insert({"mesh/0", mesh_guid});
    original_meta.sub_assets.insert({"material/base", mat_guid});
    original_meta.sub_assets.insert({"prefab", prefab_guid});

    const auto type_mesh = tempest::assets::asset_type_id::of<mock_mesh_asset>();
    const auto type_mat = tempest::assets::asset_type_id::of<mock_material_asset>();
    original_meta.sub_asset_types.insert({"mesh/0", type_mesh});
    original_meta.sub_asset_types.insert({"material/base", type_mat});

    // Act: Save sidecar meta to disk and load it back
    const auto save_result = original_meta.save_to_file(meta_file_path);
    ASSERT_TRUE(save_result);

    const auto loaded_meta_opt = tempest::assets::sidecar_meta::load_from_file(meta_file_path);
    ASSERT_TRUE(loaded_meta_opt.has_value());
    const auto& loaded_meta = *loaded_meta_opt;

    // Assert: All fields match
    EXPECT_EQ(loaded_meta.source_guid, original_meta.source_guid);
    EXPECT_EQ(loaded_meta.importer, original_meta.importer);
    EXPECT_EQ(loaded_meta.importer_version, original_meta.importer_version);
    EXPECT_EQ(loaded_meta.settings, original_meta.settings);
    ASSERT_EQ(loaded_meta.sub_assets.size(), original_meta.sub_assets.size());
    EXPECT_EQ(loaded_meta.sub_assets.find("mesh/0")->second, mesh_guid);
    EXPECT_EQ(loaded_meta.sub_assets.find("material/base")->second, mat_guid);
    EXPECT_EQ(loaded_meta.sub_assets.find("prefab")->second, prefab_guid);
    EXPECT_EQ(loaded_meta.sub_asset_types.find("mesh/0")->second, type_mesh);
    EXPECT_EQ(loaded_meta.sub_asset_types.find("material/base")->second, type_mat);
    EXPECT_EQ(loaded_meta, original_meta);

    // Teardown: Clean up temporary test files
    tempest::filesystem::remove_all(temp_dir);
}

// ============================================================================
// Section 3: Sub-Asset Resolution & Deterministic GUID Stability
// ============================================================================

/// @brief Verifies that modifying a source asset and re-importing preserves existing sub-asset
/// GUIDs (such as mesh/0 and material/0), while allocating consistent, deterministic GUIDs for
/// newly introduced sub-assets.
TEST(project_workspace_tests, reimport_preserves_subasset_guids)
{
    // Setup: Prepare workspace with mock model asset
    const auto temp_dir = tempest::filesystem::path("tempest_test_reimport");
    const auto assets_dir = temp_dir / "assets";
    tempest::filesystem::create_directories(assets_dir);

    const auto source_disk_path = assets_dir / "hero.custom";
    write_test_file(source_disk_path, "initial_mesh_and_material_data_v1");

    auto workspace = tempest::assets::project_workspace{};
    workspace.name = "ReimportTest";
    workspace.root_directory = temp_dir.generic_string();
    workspace.mounts.push_back(tempest::assets::mount_point{
        .path = "assets",
        .priority = 0,
        .alias = "content",
    });

    auto type_reg = tempest::assets::asset_type_registry{};
    type_reg.register_type<mock_mesh_asset>(nullptr, nullptr);
    type_reg.register_type<mock_material_asset>(nullptr, nullptr);

    auto database = tempest::assets::asset_database{&type_reg};
    const auto rebuild_res = database.rebuild_from_workspace(workspace);
    ASSERT_TRUE(rebuild_res);

    const auto canonical_source_key = tempest::string{"@content/hero.custom"};
    const auto type_mesh = tempest::assets::asset_type_id::of<mock_mesh_asset>();
    const auto type_mat = tempest::assets::asset_type_id::of<mock_material_asset>();

    // Act (Step 1): First import producing mesh/0 and material/0
    const auto mesh0_guid_first = database.resolve_sub_asset_guid(canonical_source_key, "mesh/0", type_mesh);
    const auto mat0_guid_first = database.resolve_sub_asset_guid(canonical_source_key, "material/0", type_mat);

    EXPECT_NE(mesh0_guid_first, tempest::guid{});
    EXPECT_NE(mat0_guid_first, tempest::guid{});
    EXPECT_NE(mesh0_guid_first, mat0_guid_first);

    // Act (Step 2): Modify source asset on disk and re-import, producing mesh/0, material/0, and mesh/1
    write_test_file(source_disk_path, "modified_mesh_data_with_additional_lod_mesh_v2");

    const auto mesh0_guid_second = database.resolve_sub_asset_guid(canonical_source_key, "mesh/0", type_mesh);
    const auto mat0_guid_second = database.resolve_sub_asset_guid(canonical_source_key, "material/0", type_mat);
    const auto mesh1_guid_second = database.resolve_sub_asset_guid(canonical_source_key, "mesh/1", type_mesh);

    // Assert: Existing sub-assets retain their committed GUIDs bit-for-bit
    EXPECT_EQ(mesh0_guid_second, mesh0_guid_first);
    EXPECT_EQ(mat0_guid_second, mat0_guid_first);

    EXPECT_NE(mesh1_guid_second, tempest::guid{});
    EXPECT_NE(mesh1_guid_second, mesh0_guid_first);
    EXPECT_NE(mesh1_guid_second, mat0_guid_first);

    // Act (Step 3): Re-importing a third time returns identical GUID for mesh/1
    const auto mesh1_guid_third = database.resolve_sub_asset_guid(canonical_source_key, "mesh/1", type_mesh);
    EXPECT_EQ(mesh1_guid_third, mesh1_guid_second);

    // Teardown: Clean up test files
    tempest::filesystem::remove_all(temp_dir);
}

// ============================================================================
// Section 4: Pure Cache Rebuild & GUID Preservation
// ============================================================================

/// @brief Verifies that deleting .cache/assets.tassetdb and invoking rebuild_from_workspace()
/// successfully reconstructs source entries and asset entries from committed .tmeta sidecars,
/// reproducing every sub-asset GUID, type hash, and source ID bit-for-bit.
TEST(project_workspace_tests, cache_rebuild_preserves_guids)
{
    // Setup: Prepare workspace with mock assets and committed .tmeta sidecars
    const auto temp_dir = tempest::filesystem::path("tempest_test_cache_rebuild");
    const auto assets_dir = temp_dir / "assets";
    tempest::filesystem::create_directories(assets_dir);

    const auto model_source_path = assets_dir / "prop.model";
    write_test_file(model_source_path, "mock_prop_source_bytes");

    const auto texture_source_path = assets_dir / "diffuse.tex";
    write_test_file(texture_source_path, "mock_texture_source_bytes");

    const auto expected_prop_source_guid = tempest::guid::generate_random_guid();
    const auto expected_mesh_guid = tempest::guid::generate_random_guid();
    const auto expected_mat_guid = tempest::guid::generate_random_guid();

    const auto type_mesh = tempest::assets::asset_type_id::of<mock_mesh_asset>();
    const auto type_mat = tempest::assets::asset_type_id::of<mock_material_asset>();
    const auto type_tex = tempest::assets::asset_type_id::of<mock_texture_asset>();

    auto prop_meta = tempest::assets::sidecar_meta{};
    prop_meta.source_guid = expected_prop_source_guid;
    prop_meta.importer = "mock_model";
    prop_meta.importer_version = 1;
    prop_meta.sub_assets.insert({"mesh/0", expected_mesh_guid});
    prop_meta.sub_assets.insert({"material/0", expected_mat_guid});
    prop_meta.sub_asset_types.insert({"mesh/0", type_mesh});
    prop_meta.sub_asset_types.insert({"material/0", type_mat});
    auto prop_meta_path = model_source_path.string();
    prop_meta_path += ".tmeta";
    ASSERT_TRUE(prop_meta.save_to_file(prop_meta_path));

    const auto expected_tex_source_guid = tempest::guid::generate_random_guid();
    const auto expected_tex_guid = tempest::guid::generate_random_guid();

    auto tex_meta = tempest::assets::sidecar_meta{};
    tex_meta.source_guid = expected_tex_source_guid;
    tex_meta.importer = "mock_texture";
    tex_meta.importer_version = 1;
    tex_meta.sub_assets.insert({"texture/0", expected_tex_guid});
    tex_meta.sub_asset_types.insert({"texture/0", type_tex});
    auto tex_meta_path = texture_source_path.string();
    tex_meta_path += ".tmeta";
    ASSERT_TRUE(tex_meta.save_to_file(tex_meta_path));

    auto workspace = tempest::assets::project_workspace{};
    workspace.name = "CacheRebuildTest";
    workspace.root_directory = temp_dir.generic_string();
    workspace.mounts.push_back(tempest::assets::mount_point{
        .path = "assets",
        .priority = 0,
        .alias = "content",
    });

    auto type_reg = tempest::assets::asset_type_registry{};
    type_reg.register_type<mock_mesh_asset>(nullptr, nullptr);
    type_reg.register_type<mock_material_asset>(nullptr, nullptr);
    type_reg.register_type<mock_texture_asset>(nullptr, nullptr);

    // Act (Step 1): Initial build creates cache
    auto initial_db = tempest::assets::asset_database{&type_reg};
    const auto initial_rebuild_res = initial_db.rebuild_from_workspace(workspace);
    ASSERT_TRUE(initial_rebuild_res);

    const auto cache_db_path = workspace.cache_db_path();
    ASSERT_TRUE(tempest::filesystem::exists(tempest::filesystem::path(cache_db_path)));

    // Verify initial database entries exist
    const auto* initial_mesh = initial_db.find_by_guid(expected_mesh_guid);
    ASSERT_NE(initial_mesh, nullptr);
    EXPECT_EQ(initial_mesh->type, type_mesh);
    EXPECT_EQ(initial_mesh->source_id, expected_prop_source_guid);

    const auto* initial_mat = initial_db.find_by_guid(expected_mat_guid);
    ASSERT_NE(initial_mat, nullptr);
    EXPECT_EQ(initial_mat->type, type_mat);
    EXPECT_EQ(initial_mat->source_id, expected_prop_source_guid);

    const auto* initial_tex = initial_db.find_by_guid(expected_tex_guid);
    ASSERT_NE(initial_tex, nullptr);
    EXPECT_EQ(initial_tex->type, type_tex);
    EXPECT_EQ(initial_tex->source_id, expected_tex_source_guid);

    // Act (Step 2): Delete .cache/assets.tassetdb
    const auto remove_cache_res = tempest::filesystem::remove(tempest::filesystem::path(cache_db_path));
    ASSERT_TRUE(remove_cache_res);
    ASSERT_FALSE(tempest::filesystem::exists(tempest::filesystem::path(cache_db_path)));

    // Act (Step 3): Rebuild database from workspace from scratch
    auto rebuilt_db = tempest::assets::asset_database{&type_reg};
    const auto rebuilt_res = rebuilt_db.rebuild_from_workspace(workspace);
    ASSERT_TRUE(rebuilt_res);

    // Assert: Every sub-asset GUID and type hash matches initial database bit-for-bit
    const auto* rebuilt_mesh = rebuilt_db.find_by_guid(expected_mesh_guid);
    ASSERT_NE(rebuilt_mesh, nullptr);
    EXPECT_EQ(rebuilt_mesh->id, expected_mesh_guid);
    EXPECT_EQ(rebuilt_mesh->type, type_mesh);
    EXPECT_EQ(rebuilt_mesh->source_id, expected_prop_source_guid);

    const auto* rebuilt_mat = rebuilt_db.find_by_guid(expected_mat_guid);
    ASSERT_NE(rebuilt_mat, nullptr);
    EXPECT_EQ(rebuilt_mat->id, expected_mat_guid);
    EXPECT_EQ(rebuilt_mat->type, type_mat);
    EXPECT_EQ(rebuilt_mat->source_id, expected_prop_source_guid);

    const auto* rebuilt_tex = rebuilt_db.find_by_guid(expected_tex_guid);
    ASSERT_NE(rebuilt_tex, nullptr);
    EXPECT_EQ(rebuilt_tex->id, expected_tex_guid);
    EXPECT_EQ(rebuilt_tex->type, type_tex);
    EXPECT_EQ(rebuilt_tex->source_id, expected_tex_source_guid);

    // Check that cache file was recreated
    EXPECT_TRUE(tempest::filesystem::exists(tempest::filesystem::path(cache_db_path)));

    // Teardown: Clean up test files
    tempest::filesystem::remove_all(temp_dir);
}
