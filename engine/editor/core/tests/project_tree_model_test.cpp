#include <gtest/gtest.h>

#include <tempest/asset_database.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/file_watcher.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/guid.hpp>
#include <tempest/memory.hpp>
#include <tempest/project_tree_model.hpp>
#include <tempest/project_workspace.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::editor::tests
{
    namespace fs = tempest::filesystem;

    namespace
    {
        auto write_dummy_file(const fs::path& target_path, string_view content = "dummy") -> bool
        {
            const auto span_bytes = span<const byte>{
                reinterpret_cast<const byte*>(content.data()),
                content.size(),
            };
            return write_file_from_bytes(target_path, span_bytes).has_value();
        }
    } // namespace

    // =========================================================================
    // project_tree_node Unit Tests
    // =========================================================================

    /// @brief Verifies default initialization and helper predicates on project tree nodes.
    TEST(project_tree_node_test, default_properties_and_predicates)
    {
        // Setup & Act
        const auto default_node = project_tree_node{};

        // Assert
        EXPECT_TRUE(default_node.name.empty());
        EXPECT_TRUE(default_node.path.empty());
        EXPECT_TRUE(default_node.disk_path.empty());
        EXPECT_EQ(default_node.kind, tree_node_kind::directory);
        EXPECT_EQ(default_node.asset_id, guid{});
        EXPECT_FALSE(default_node.is_expanded);
        EXPECT_EQ(default_node.parent, nullptr);
        EXPECT_FALSE(default_node.has_children());
        EXPECT_TRUE(default_node.is_directory_like());
    }

    /// @brief Verifies that add_child correctly reparents the added node and appends it to children.
    TEST(project_tree_node_test, add_child_reparents_and_appends)
    {
        // Setup
        auto parent_node = project_tree_node{};
        parent_node.name = "parent";
        parent_node.kind = tree_node_kind::mount_root;

        auto child_node = make_unique<project_tree_node>();
        child_node->name = "child";
        child_node->kind = tree_node_kind::directory;

        // Act
        auto* added_ptr = parent_node.add_child(tempest::move(child_node));

        // Assert
        ASSERT_NE(added_ptr, nullptr);
        EXPECT_EQ(added_ptr->name, "child");
        EXPECT_EQ(added_ptr->parent, &parent_node);
        EXPECT_TRUE(parent_node.has_children());
        ASSERT_EQ(parent_node.children.size(), 1u);
        EXPECT_EQ(parent_node.children.front().get(), added_ptr);
    }

    /// @brief Verifies that move operations transfer ownership and update children's parent pointers.
    TEST(project_tree_node_test, move_operations_update_child_parent_pointers)
    {
        // Setup
        auto original_node = project_tree_node{};
        original_node.name = "source_parent";
        original_node.kind = tree_node_kind::directory;

        auto child_one = make_unique<project_tree_node>();
        child_one->name = "child_one";
        auto* child_one_ptr = original_node.add_child(tempest::move(child_one));

        auto child_two = make_unique<project_tree_node>();
        child_two->name = "child_two";
        auto* child_two_ptr = original_node.add_child(tempest::move(child_two));

        // Act: Move construct
        auto moved_node = project_tree_node{tempest::move(original_node)};

        // Assert move construct
        EXPECT_EQ(moved_node.name, "source_parent");
        ASSERT_EQ(moved_node.children.size(), 2u);
        EXPECT_EQ(child_one_ptr->parent, &moved_node);
        EXPECT_EQ(child_two_ptr->parent, &moved_node);

        // Act: Move assign
        auto assigned_node = project_tree_node{};
        assigned_node = tempest::move(moved_node);

        // Assert move assign
        EXPECT_EQ(assigned_node.name, "source_parent");
        ASSERT_EQ(assigned_node.children.size(), 2u);
        EXPECT_EQ(child_one_ptr->parent, &assigned_node);
        EXPECT_EQ(child_two_ptr->parent, &assigned_node);
    }

    // =========================================================================
    // project_tree_row Unit Tests
    // =========================================================================

    /// @brief Verifies default initialization and default properties on project tree rows.
    TEST(project_tree_row_test, default_properties)
    {
        // Setup & Act
        const auto default_row = project_tree_row{};

        // Assert
        EXPECT_EQ(default_row.node, nullptr);
        EXPECT_EQ(default_row.depth, 0u);
        EXPECT_TRUE(default_row.display_label.empty());
        EXPECT_TRUE(default_row.relative_path.empty());
        EXPECT_EQ(default_row.kind, tree_node_kind::directory);
        EXPECT_EQ(default_row.asset_id, guid{});
        EXPECT_FALSE(default_row.is_expanded);
        EXPECT_FALSE(default_row.has_children);
    }

    // =========================================================================
    // project_tree_model Hierarchy Building & Classification Tests
    // =========================================================================

    /// @brief Verifies that directory traversal builds a tree with deterministic ordering:
    /// directories first, then files, with sidecars suppressed.
    TEST(project_tree_model_test, deterministic_traversal_and_sidecar_suppression)
    {
        // Setup: Create test folder layout
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_tree_traversal";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "textures"));
        ASSERT_TRUE(fs::create_directories(temp_dir / "models"));

        ASSERT_TRUE(write_dummy_file(temp_dir / "textures" / "albedo.png"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "models" / "character.fbx"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "models" / "character.fbx.tmeta"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "models" / "animation.anim"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "root_readme.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        // Act
        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        // Assert: Root mount node
        ASSERT_EQ(model.roots().size(), 1u);
        const auto* root = model.roots().front().get();
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->name, "Assets");
        EXPECT_EQ(root->kind, tree_node_kind::mount_root);
        EXPECT_TRUE(root->is_directory_like());

        // Root children: "models" and "textures" first, then "root_readme.txt"
        ASSERT_EQ(root->children.size(), 3u);
        EXPECT_EQ(root->children[0]->name, "models");
        EXPECT_EQ(root->children[0]->kind, tree_node_kind::directory);
        EXPECT_EQ(root->children[1]->name, "textures");
        EXPECT_EQ(root->children[1]->kind, tree_node_kind::directory);
        EXPECT_EQ(root->children[2]->name, "root_readme.txt");
        EXPECT_EQ(root->children[2]->kind, tree_node_kind::loose_file);

        // Check models children: "animation.anim", "character.fbx". Notice .tmeta is suppressed!
        const auto* models_dir = root->children[0].get();
        ASSERT_EQ(models_dir->children.size(), 2u);
        EXPECT_EQ(models_dir->children[0]->name, "animation.anim");
        EXPECT_EQ(models_dir->children[1]->name, "character.fbx");

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that imported files with valid .tmeta files are classified as imported_file
    /// with source GUID and sub-assets extracted as children, while files without .tmeta are loose_file.
    TEST(project_tree_model_test, file_classification_and_sub_asset_extraction)
    {
        // Setup: Create test files and sidecar
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_sub_assets";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir));

        const auto model_file = temp_dir / "hero.model";
        const auto meta_file = temp_dir / "hero.model.tmeta";
        const auto loose_file = temp_dir / "notes.txt";

        ASSERT_TRUE(write_dummy_file(model_file));
        ASSERT_TRUE(write_dummy_file(loose_file));

        const auto hero_source_guid = guid::generate_random_guid();
        const auto mesh_sub_guid = guid::generate_random_guid();
        const auto mat_sub_guid = guid::generate_random_guid();
        const auto prefab_sub_guid = guid::generate_random_guid();

        auto meta = assets::sidecar_meta{};
        meta.source_guid = hero_source_guid;
        meta.importer = "model_importer";
        meta.sub_assets["mesh/0"] = mesh_sub_guid;
        meta.sub_assets["material/0"] = mat_sub_guid;
        meta.sub_assets["prefab"] = prefab_sub_guid;
        meta.sub_asset_types["mesh/0"] = assets::asset_type_id::from_hash(0x1111);
        meta.sub_asset_types["material/0"] = assets::asset_type_id::from_hash(0x2222);
        meta.sub_asset_types["prefab"] = assets::asset_type_id::from_hash(0x3333);
        ASSERT_TRUE(meta.save_to_file(meta_file.generic_string()));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Content",
        });

        // Act
        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        // Assert: Verify loose file classification
        const auto* loose_node = model.find_node_by_path("notes.txt");
        ASSERT_NE(loose_node, nullptr);
        EXPECT_EQ(loose_node->name, "notes.txt");
        EXPECT_EQ(loose_node->kind, tree_node_kind::loose_file);
        EXPECT_EQ(loose_node->asset_id, guid{});
        EXPECT_FALSE(loose_node->has_children());

        // Assert: Verify imported file classification
        const auto* hero_node = model.find_node_by_path("hero.model");
        ASSERT_NE(hero_node, nullptr);
        EXPECT_EQ(hero_node->name, "hero.model");
        EXPECT_EQ(hero_node->kind, tree_node_kind::imported_file);
        EXPECT_EQ(hero_node->asset_id, hero_source_guid);
        EXPECT_TRUE(hero_node->has_children());
        ASSERT_EQ(hero_node->children.size(), 3u);

        // Sub-assets are sorted alphabetically by name:
        // "material/0" < "mesh/0" < "prefab"
        const auto* sub0 = hero_node->children[0].get();
        EXPECT_EQ(sub0->name, "material/0");
        EXPECT_EQ(sub0->kind, tree_node_kind::sub_asset);
        EXPECT_EQ(sub0->asset_id, mat_sub_guid);
        EXPECT_EQ(sub0->sub_asset_key, "material/0");
        EXPECT_EQ(sub0->path, "hero.model:material/0");
        EXPECT_EQ(sub0->disk_path, hero_node->disk_path);
        EXPECT_EQ(sub0->type, assets::asset_type_id::from_hash(0x2222));
        EXPECT_EQ(sub0->parent, hero_node);

        const auto* sub1 = hero_node->children[1].get();
        EXPECT_EQ(sub1->name, "mesh/0");
        EXPECT_EQ(sub1->kind, tree_node_kind::sub_asset);
        EXPECT_EQ(sub1->asset_id, mesh_sub_guid);
        EXPECT_EQ(sub1->sub_asset_key, "mesh/0");
        EXPECT_EQ(sub1->path, "hero.model:mesh/0");
        EXPECT_EQ(sub1->disk_path, hero_node->disk_path);
        EXPECT_EQ(sub1->type, assets::asset_type_id::from_hash(0x1111));
        EXPECT_EQ(sub1->parent, hero_node);

        const auto* sub2 = hero_node->children[2].get();
        EXPECT_EQ(sub2->name, "prefab");
        EXPECT_EQ(sub2->kind, tree_node_kind::sub_asset);
        EXPECT_EQ(sub2->asset_id, prefab_sub_guid);
        EXPECT_EQ(sub2->sub_asset_key, "prefab");
        EXPECT_EQ(sub2->path, "hero.model:prefab");
        EXPECT_EQ(sub2->disk_path, hero_node->disk_path);
        EXPECT_EQ(sub2->type, assets::asset_type_id::from_hash(0x3333));
        EXPECT_EQ(sub2->parent, hero_node);

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies find_node_by_path and find_node_by_guid lookups.
    TEST(project_tree_model_test, path_and_guid_lookups)
    {
        // Setup: Create test folder layout
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_lookups";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "characters"));

        const auto character_file = temp_dir / "characters" / "warrior.asset";
        const auto meta_file = temp_dir / "characters" / "warrior.asset.tmeta";
        ASSERT_TRUE(write_dummy_file(character_file));

        const auto source_guid = guid::generate_random_guid();
        const auto sub_mesh_guid = guid::generate_random_guid();

        auto meta = assets::sidecar_meta{};
        meta.source_guid = source_guid;
        meta.sub_assets["mesh/0"] = sub_mesh_guid;
        ASSERT_TRUE(meta.save_to_file(meta_file.generic_string()));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "GameAssets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        // Act & Assert: find_node_by_path relative
        const auto* node_by_rel = model.find_node_by_path("characters/warrior.asset");
        ASSERT_NE(node_by_rel, nullptr);
        EXPECT_EQ(node_by_rel->name, "warrior.asset");

        // Act & Assert: find_node_by_path with mount prefix
        const auto* node_by_mount_pref = model.find_node_by_path("GameAssets/characters/warrior.asset");
        EXPECT_EQ(node_by_mount_pref, node_by_rel);

        // Act & Assert: find_node_by_path with backslashes
        const auto* node_by_backslash = model.find_node_by_path("characters\\warrior.asset");
        EXPECT_EQ(node_by_backslash, node_by_rel);

        // Act & Assert: find_node_by_path for sub-asset
        const auto* sub_node = model.find_node_by_path("characters/warrior.asset:mesh/0");
        ASSERT_NE(sub_node, nullptr);
        EXPECT_EQ(sub_node->name, "mesh/0");
        EXPECT_EQ(sub_node->asset_id, sub_mesh_guid);

        // Act & Assert: find_node_by_guid
        const auto* found_by_source_guid = model.find_node_by_guid(source_guid);
        EXPECT_EQ(found_by_source_guid, node_by_rel);

        const auto* found_by_sub_guid = model.find_node_by_guid(sub_mesh_guid);
        EXPECT_EQ(found_by_sub_guid, sub_node);

        // Act & Assert: Nil GUID and non-existent queries
        EXPECT_EQ(model.find_node_by_guid(guid{}), nullptr);
        EXPECT_EQ(model.find_node_by_guid(guid::generate_random_guid()), nullptr);
        EXPECT_EQ(model.find_node_by_path("non_existent/path.txt"), nullptr);
        EXPECT_EQ(model.find_node_by_path(""), nullptr);

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies workspace rebuild and clear functionality.
    TEST(project_tree_model_test, workspace_rebuild_and_clear)
    {
        // Setup: Create workspace with relative mount
        const auto base_dir = fs::temp_directory_path() / "tempest_test_workspace_tree";
        static_cast<void>(fs::remove_all(base_dir));
        const auto content_dir = base_dir / "Content";
        ASSERT_TRUE(fs::create_directories(content_dir));
        ASSERT_TRUE(write_dummy_file(content_dir / "sample.txt"));

        auto workspace = assets::project_workspace{};
        workspace.name = "TestProject";
        workspace.root_directory = base_dir.generic_string();
        workspace.mounts.push_back(assets::mount_point{
            .path = "Content",
            .alias = "ProjectContent",
        });

        // Act
        auto model = project_tree_model{};
        model.rebuild_from_workspace(workspace);

        // Assert
        ASSERT_EQ(model.roots().size(), 1u);
        EXPECT_EQ(model.roots().front()->name, "ProjectContent");
        ASSERT_EQ(model.roots().front()->children.size(), 1u);
        EXPECT_EQ(model.roots().front()->children.front()->name, "sample.txt");

        // Act: Clear model
        model.clear();

        // Assert: Model is cleared
        EXPECT_TRUE(model.roots().empty());
        EXPECT_EQ(model.find_node_by_path("sample.txt"), nullptr);

        // Teardown
        static_cast<void>(fs::remove_all(base_dir));
    }

    /// @brief Verifies that rebuild_from_workspace correctly discovers and populates nested subdirectories
    /// and files under mount roots.
    TEST(project_tree_model_test, rebuild_from_workspace_populates_subdirectories)
    {
        // Setup: Create a directory structure with subdirectories and scene files
        const auto base_dir = fs::temp_directory_path() / "tempest_test_tree_subdirs";
        static_cast<void>(fs::remove_all(base_dir));
        const auto assets_dir = base_dir / "assets";
        const auto scenes_dir = assets_dir / "scenes";
        ASSERT_TRUE(fs::create_directories(scenes_dir));
        ASSERT_TRUE(write_dummy_file(scenes_dir / "main.tscene"));

        auto workspace = assets::project_workspace{};
        workspace.name = "GameProject";
        workspace.root_directory = base_dir.generic_string();
        workspace.mounts.push_back(assets::mount_point{
            .path = "assets",
            .alias = "assets",
        });

        // Act: Rebuild model from workspace
        auto model = project_tree_model{};
        model.rebuild_from_workspace(workspace);

        // Assert: Mount root has scenes subdirectory as child
        ASSERT_EQ(model.roots().size(), 1u);
        const auto* root = model.roots().front().get();
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->name, "assets");
        ASSERT_EQ(root->children.size(), 1u);

        const auto* scenes_node = root->children.front().get();
        ASSERT_NE(scenes_node, nullptr);
        EXPECT_EQ(scenes_node->name, "scenes");
        EXPECT_EQ(scenes_node->kind, tree_node_kind::directory);
        ASSERT_EQ(scenes_node->children.size(), 1u);

        const auto* scene_file_node = scenes_node->children.front().get();
        ASSERT_NE(scene_file_node, nullptr);
        EXPECT_EQ(scene_file_node->name, "main.tscene");

        // Assert: scenes is in visible rows because root is expanded by default
        const auto rows = model.visible_rows();
        ASSERT_GE(rows.size(), 2u);
        EXPECT_EQ(rows[0].display_label, "assets");
        EXPECT_EQ(rows[1].display_label, "scenes");
        EXPECT_TRUE(rows[1].has_children);

        // Teardown
        static_cast<void>(fs::remove_all(base_dir));
    }

    /// @brief Verifies that move construction and move assignment transfer root nodes and asset database pointer,
    /// while resetting the source model to an empty state.
    TEST(project_tree_model_test, move_operations_transfer_roots_and_db)
    {
        // Setup
        auto database = assets::asset_database{};
        auto original_model = project_tree_model{&database};

        const auto temp_dir = fs::temp_directory_path() / "tempest_test_tree_move";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir));
        ASSERT_TRUE(write_dummy_file(temp_dir / "test.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "MoveMount",
        });
        original_model.rebuild_from_mounts(mounts);
        ASSERT_EQ(original_model.roots().size(), 1u);

        // Act: Move construct
        auto moved_model = project_tree_model{tempest::move(original_model)};

        // Assert: Move construct transferred state and cleared source
        EXPECT_EQ(moved_model.asset_database(), &database);
        ASSERT_EQ(moved_model.roots().size(), 1u);
        EXPECT_EQ(moved_model.roots().front()->name, "MoveMount");
        EXPECT_EQ(original_model.roots().size(), 0u);
        EXPECT_EQ(original_model.asset_database(), nullptr);

        // Act: Move assign
        auto assigned_model = project_tree_model{};
        assigned_model = tempest::move(moved_model);

        // Assert: Move assign transferred state and cleared source
        EXPECT_EQ(assigned_model.asset_database(), &database);
        ASSERT_EQ(assigned_model.roots().size(), 1u);
        EXPECT_EQ(assigned_model.roots().front()->name, "MoveMount");
        EXPECT_EQ(moved_model.roots().size(), 0u);
        EXPECT_EQ(moved_model.asset_database(), nullptr);

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that const references to project_tree_model support find_node_by_path and find_node_by_guid lookups.
    TEST(project_tree_model_test, const_lookups)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_tree_const_lookups";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir));

        const auto asset_file = temp_dir / "sample.mesh";
        const auto meta_file = temp_dir / "sample.mesh.tmeta";
        ASSERT_TRUE(write_dummy_file(asset_file));

        const auto expected_guid = guid::generate_random_guid();
        auto meta = assets::sidecar_meta{};
        meta.source_guid = expected_guid;
        ASSERT_TRUE(meta.save_to_file(meta_file.generic_string()));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "ConstMount",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        // Act & Assert: Call through const reference
        const auto& const_model_ref = model;

        const auto* found_by_path = const_model_ref.find_node_by_path("sample.mesh");
        ASSERT_NE(found_by_path, nullptr);
        EXPECT_EQ(found_by_path->name, "sample.mesh");
        EXPECT_EQ(found_by_path->asset_id, expected_guid);

        const auto* found_by_mount_path = const_model_ref.find_node_by_path("ConstMount/sample.mesh");
        EXPECT_EQ(found_by_mount_path, found_by_path);

        const auto* found_by_guid = const_model_ref.find_node_by_guid(expected_guid);
        EXPECT_EQ(found_by_guid, found_by_path);

        const auto* non_existent = const_model_ref.find_node_by_path("missing.txt");
        EXPECT_EQ(non_existent, nullptr);

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies setting and getting the associated asset database.
    TEST(project_tree_model_test, asset_database_accessor)
    {
        // Setup
        auto model = project_tree_model{};
        EXPECT_EQ(model.asset_database(), nullptr);

        auto database_one = assets::asset_database{};
        auto database_two = assets::asset_database{};

        // Act: Assign first database
        model.set_asset_database(&database_one);

        // Assert
        EXPECT_EQ(model.asset_database(), &database_one);

        // Act: Assign second database
        model.set_asset_database(&database_two);

        // Assert
        EXPECT_EQ(model.asset_database(), &database_two);

        // Act: Reset to nullptr
        model.set_asset_database(nullptr);

        // Assert
        EXPECT_EQ(model.asset_database(), nullptr);
    }

    // =========================================================================
    // project_tree_model Virtualization, Expansion & Search Tests
    // =========================================================================

    /// @brief Verifies that visible_rows after rebuild_from_mounts starts with the mount root
    /// expanded at depth 0 and immediate child directories collapsed at depth 1.
    TEST(project_tree_model_test, initial_visible_rows_mount_expanded_subfolders_collapsed)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_initial_visible_rows";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "textures"));
        ASSERT_TRUE(fs::create_directories(temp_dir / "models"));

        ASSERT_TRUE(write_dummy_file(temp_dir / "textures" / "albedo.png"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "models" / "character.fbx"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "root_readme.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        // Act
        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        // Assert
        ASSERT_EQ(model.roots().size(), 1u);
        const auto* root = model.roots().front().get();
        ASSERT_NE(root, nullptr);
        EXPECT_TRUE(root->is_expanded);

        const auto rows = model.visible_rows();
        ASSERT_EQ(rows.size(), 4u);
        EXPECT_EQ(model.visible_row_count(), 4u);

        // Mount root row
        EXPECT_EQ(rows[0].node, root);
        EXPECT_EQ(rows[0].depth, 0u);
        EXPECT_EQ(rows[0].display_label, "Assets");
        EXPECT_EQ(rows[0].relative_path, "Assets");
        EXPECT_EQ(rows[0].kind, tree_node_kind::mount_root);
        EXPECT_TRUE(rows[0].is_expanded);
        EXPECT_TRUE(rows[0].has_children);

        // Models directory row (collapsed)
        EXPECT_EQ(rows[1].depth, 1u);
        EXPECT_EQ(rows[1].display_label, "models");
        EXPECT_EQ(rows[1].relative_path, "models");
        EXPECT_EQ(rows[1].kind, tree_node_kind::directory);
        EXPECT_FALSE(rows[1].is_expanded);
        EXPECT_TRUE(rows[1].has_children);

        // Textures directory row (collapsed)
        EXPECT_EQ(rows[2].depth, 1u);
        EXPECT_EQ(rows[2].display_label, "textures");
        EXPECT_EQ(rows[2].relative_path, "textures");
        EXPECT_EQ(rows[2].kind, tree_node_kind::directory);
        EXPECT_FALSE(rows[2].is_expanded);
        EXPECT_TRUE(rows[2].has_children);

        // Root readme loose file row
        EXPECT_EQ(rows[3].depth, 1u);
        EXPECT_EQ(rows[3].display_label, "root_readme.txt");
        EXPECT_EQ(rows[3].relative_path, "root_readme.txt");
        EXPECT_EQ(rows[3].kind, tree_node_kind::loose_file);
        EXPECT_FALSE(rows[3].is_expanded);
        EXPECT_FALSE(rows[3].has_children);

        // get_row bounds checking
        EXPECT_EQ(model.get_row(0), &rows[0]);
        EXPECT_EQ(model.get_row(3), &rows[3]);
        EXPECT_EQ(model.get_row(4), nullptr);

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies set_expanded, toggle_expanded, and set_expanded_by_path on directories.
    TEST(project_tree_model_test, expansion_methods_for_directories)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_dir_expansion";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "models"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "models" / "character.fbx"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "root.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Content",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        ASSERT_EQ(model.visible_row_count(), 3u);

        // Act: Expand by path
        model.set_expanded_by_path("models", true);

        // Assert: "character.fbx" is now visible at depth 2
        ASSERT_EQ(model.visible_row_count(), 4u);
        const auto rows_after_expand = model.visible_rows();
        EXPECT_EQ(rows_after_expand[1].display_label, "models");
        EXPECT_TRUE(rows_after_expand[1].is_expanded);
        EXPECT_EQ(rows_after_expand[2].display_label, "character.fbx");
        EXPECT_EQ(rows_after_expand[2].depth, 2u);

        // Act: Toggle expansion using pointer
        auto* models_node = model.find_node_by_path("models");
        ASSERT_NE(models_node, nullptr);
        model.toggle_expanded(models_node);

        // Assert: Collapsed back to 3 rows
        EXPECT_FALSE(models_node->is_expanded);
        ASSERT_EQ(model.visible_row_count(), 3u);

        // Act: Set expanded using pointer
        model.set_expanded(models_node, true);

        // Assert: Expanded again
        EXPECT_TRUE(models_node->is_expanded);
        ASSERT_EQ(model.visible_row_count(), 4u);

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that expanding an imported_file node reveals its sub-assets at depth + 1.
    TEST(project_tree_model_test, expanding_imported_file_reveals_sub_assets)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_imported_file_expansion";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir));

        const auto model_file = temp_dir / "hero.model";
        const auto meta_file = temp_dir / "hero.model.tmeta";
        ASSERT_TRUE(write_dummy_file(model_file));

        const auto hero_guid = guid::generate_random_guid();
        const auto mesh_guid = guid::generate_random_guid();
        const auto mat_guid = guid::generate_random_guid();

        auto meta = assets::sidecar_meta{};
        meta.source_guid = hero_guid;
        meta.importer = "model_importer";
        meta.sub_assets["mesh/0"] = mesh_guid;
        meta.sub_assets["material/0"] = mat_guid;
        ASSERT_TRUE(meta.save_to_file(meta_file.generic_string()));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        // Assert initial state: "hero.model" is collapsed, sub-assets hidden
        ASSERT_EQ(model.visible_row_count(), 2u);
        EXPECT_EQ(model.visible_rows()[1].display_label, "hero.model");
        EXPECT_FALSE(model.visible_rows()[1].is_expanded);
        EXPECT_TRUE(model.visible_rows()[1].has_children);

        // Act: Expand imported file
        model.set_expanded_by_path("hero.model", true);

        // Assert: 2 sub-assets now visible at depth 2
        ASSERT_EQ(model.visible_row_count(), 4u);
        const auto rows = model.visible_rows();

        EXPECT_EQ(rows[1].display_label, "hero.model");
        EXPECT_TRUE(rows[1].is_expanded);

        // Sub-assets sorted alphabetically: "material/0" then "mesh/0"
        EXPECT_EQ(rows[2].display_label, "material/0");
        EXPECT_EQ(rows[2].relative_path, "hero.model:material/0");
        EXPECT_EQ(rows[2].depth, 2u);
        EXPECT_EQ(rows[2].kind, tree_node_kind::sub_asset);
        EXPECT_EQ(rows[2].asset_id, mat_guid);
        EXPECT_FALSE(rows[2].is_expanded);
        EXPECT_FALSE(rows[2].has_children);

        EXPECT_EQ(rows[3].display_label, "mesh/0");
        EXPECT_EQ(rows[3].relative_path, "hero.model:mesh/0");
        EXPECT_EQ(rows[3].depth, 2u);
        EXPECT_EQ(rows[3].kind, tree_node_kind::sub_asset);
        EXPECT_EQ(rows[3].asset_id, mesh_guid);
        EXPECT_FALSE(rows[3].is_expanded);
        EXPECT_FALSE(rows[3].has_children);

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that collapse_all collapses all nodes to root level, and expand_all expands all nodes.
    TEST(project_tree_model_test, collapse_all_and_expand_all)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_expand_collapse_all";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "subfolder"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "subfolder" / "item.txt"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "root.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "RootMount",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        ASSERT_EQ(model.visible_row_count(), 3u);

        // Act: Collapse all
        model.collapse_all();

        // Assert: Only mount root remains visible, and it is collapsed
        ASSERT_EQ(model.visible_row_count(), 1u);
        EXPECT_EQ(model.visible_rows().front().display_label, "RootMount");
        EXPECT_FALSE(model.visible_rows().front().is_expanded);

        // Act: Expand all
        model.expand_all();

        // Assert: Root, subfolder, and both files are all visible
        ASSERT_EQ(model.visible_row_count(), 4u);
        const auto rows = model.visible_rows();
        EXPECT_EQ(rows[0].display_label, "RootMount");
        EXPECT_TRUE(rows[0].is_expanded);
        EXPECT_EQ(rows[1].display_label, "subfolder");
        EXPECT_TRUE(rows[1].is_expanded);
        EXPECT_EQ(rows[2].display_label, "item.txt");
        EXPECT_EQ(rows[2].depth, 2u);
        EXPECT_EQ(rows[3].display_label, "root.txt");
        EXPECT_EQ(rows[3].depth, 1u);

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that setting a search query switches to flattened search mode with case-insensitivity,
    /// returning matching files and sub-assets at depth 0 sorted alphabetically by relative path.
    TEST(project_tree_model_test, search_query_flattened_mode)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_search_query";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "models"));
        ASSERT_TRUE(fs::create_directories(temp_dir / "textures"));

        const auto model_file = temp_dir / "models" / "character.fbx";
        const auto meta_file = temp_dir / "models" / "character.fbx.tmeta";
        ASSERT_TRUE(write_dummy_file(model_file));
        ASSERT_TRUE(write_dummy_file(temp_dir / "textures" / "albedo.png"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "readme.txt"));

        const auto character_guid = guid::generate_random_guid();
        const auto mesh_guid = guid::generate_random_guid();
        const auto mat_guid = guid::generate_random_guid();

        auto meta = assets::sidecar_meta{};
        meta.source_guid = character_guid;
        meta.importer = "fbx_importer";
        meta.sub_assets["mesh_lod0"] = mesh_guid;
        meta.sub_assets["material_skin"] = mat_guid;
        ASSERT_TRUE(meta.save_to_file(meta_file.generic_string()));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        // Act: Search for "mesh"
        model.set_search_query("mesh");

        // Assert: Only "mesh_lod0" matches
        EXPECT_TRUE(model.is_search_active());
        EXPECT_EQ(model.search_query(), "mesh");
        ASSERT_EQ(model.visible_row_count(), 1u);

        const auto mesh_row = model.visible_rows().front();
        EXPECT_EQ(mesh_row.display_label, "mesh_lod0");
        EXPECT_EQ(mesh_row.relative_path, "models/character.fbx:mesh_lod0");
        EXPECT_EQ(mesh_row.depth, 0u);
        EXPECT_EQ(mesh_row.kind, tree_node_kind::sub_asset);
        EXPECT_EQ(mesh_row.asset_id, mesh_guid);
        EXPECT_FALSE(mesh_row.is_expanded);
        EXPECT_FALSE(mesh_row.has_children);

        // Act: Case-insensitive search "MESH"
        model.set_search_query("MESH");

        // Assert: Identical result
        ASSERT_EQ(model.visible_row_count(), 1u);
        EXPECT_EQ(model.visible_rows().front().display_label, "mesh_lod0");

        // Act: Search for "character" matches character.fbx and both its sub-assets (via relative_path)
        model.set_search_query("character");

        // Assert: 3 matching rows sorted alphabetically by relative_path
        ASSERT_EQ(model.visible_row_count(), 3u);
        const auto character_rows = model.visible_rows();

        EXPECT_EQ(character_rows[0].relative_path, "models/character.fbx");
        EXPECT_EQ(character_rows[0].depth, 0u);
        EXPECT_EQ(character_rows[0].kind, tree_node_kind::imported_file);

        EXPECT_EQ(character_rows[1].relative_path, "models/character.fbx:material_skin");
        EXPECT_EQ(character_rows[1].depth, 0u);
        EXPECT_EQ(character_rows[1].kind, tree_node_kind::sub_asset);

        EXPECT_EQ(character_rows[2].relative_path, "models/character.fbx:mesh_lod0");
        EXPECT_EQ(character_rows[2].depth, 0u);
        EXPECT_EQ(character_rows[2].kind, tree_node_kind::sub_asset);

        // Act: Search for non-existent text
        model.set_search_query("nonexistent_pattern");

        // Assert: 0 rows returned
        EXPECT_EQ(model.visible_row_count(), 0u);

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that clear_search_query restores the hierarchy mode and preserves previous expansion states.
    TEST(project_tree_model_test, clear_search_query_restores_hierarchy_and_expansion_state)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_restore_hierarchy";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "models"));
        ASSERT_TRUE(fs::create_directories(temp_dir / "textures"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "models" / "box.fbx"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "textures" / "tex.png"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        // Expand "models" directory; keep "textures" collapsed
        model.set_expanded_by_path("models", true);
        const auto pre_search_count = model.visible_row_count();
        ASSERT_EQ(pre_search_count, 4u); // Assets, models, box.fbx, textures

        // Act: Enter search mode
        model.set_search_query("tex");
        EXPECT_TRUE(model.is_search_active());
        // tex matches "textures/tex.png"
        EXPECT_EQ(model.visible_row_count(), 1u);

        // Act: Clear search query
        model.clear_search_query();

        // Assert: Back to hierarchy mode, expansion state preserved
        EXPECT_FALSE(model.is_search_active());
        EXPECT_TRUE(model.search_query().empty());
        ASSERT_EQ(model.visible_row_count(), pre_search_count);

        const auto rows = model.visible_rows();
        EXPECT_EQ(rows[1].display_label, "models");
        EXPECT_TRUE(rows[1].is_expanded);
        EXPECT_EQ(rows[2].display_label, "box.fbx");
        EXPECT_EQ(rows[3].display_label, "textures");
        EXPECT_FALSE(rows[3].is_expanded);

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that move construction and move assignment maintain valid visible_rows cache
    /// and correct row node references.
    TEST(project_tree_model_test, move_operations_preserve_visible_rows)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_move_visible_rows";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "scripts"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "scripts" / "game.lua"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Project",
        });

        auto original_model = project_tree_model{};
        original_model.rebuild_from_mounts(mounts);
        original_model.set_expanded_by_path("scripts", true);
        const auto expected_count = original_model.visible_row_count();
        ASSERT_EQ(expected_count, 3u); // Project, scripts, game.lua

        // Act: Move construct
        auto moved_model = project_tree_model{tempest::move(original_model)};

        // Assert: Moved model owns rows and valid node pointers; original is empty
        EXPECT_EQ(moved_model.visible_row_count(), expected_count);
        EXPECT_EQ(moved_model.visible_rows().front().node, moved_model.roots().front().get());
        EXPECT_EQ(original_model.visible_row_count(), 0u);
        EXPECT_TRUE(original_model.visible_rows().empty());

        // Act: Move assign
        auto assigned_model = project_tree_model{};
        assigned_model = tempest::move(moved_model);

        // Assert: Assigned model owns rows and valid node pointers; moved is empty
        EXPECT_EQ(assigned_model.visible_row_count(), expected_count);
        EXPECT_EQ(assigned_model.visible_rows().front().node, assigned_model.roots().front().get());
        EXPECT_EQ(moved_model.visible_row_count(), 0u);
        EXPECT_TRUE(moved_model.visible_rows().empty());

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies dirty flag tracking and manual invalidation via mark_dirty.
    TEST(project_tree_model_test, dirty_flag_tracking)
    {
        // Setup & Act
        auto model = project_tree_model{};
        EXPECT_TRUE(model.is_dirty());

        model.rebuild_visible_rows();
        EXPECT_FALSE(model.is_dirty());

        // Act: Manually mark dirty
        model.mark_dirty();

        // Assert
        EXPECT_TRUE(model.is_dirty());

        // Act: Rebuild clears dirty
        model.rebuild_visible_rows();
        EXPECT_FALSE(model.is_dirty());
    }

    // =========================================================================
    // project_tree_model Invalidation & Deferral Tests
    // =========================================================================

    /// @brief Verifies that invalidating a directory after creating a new file updates the tree
    /// and visible rows for that directory while keeping sibling branches untouched.
    TEST(project_tree_model_test, invalidate_path_file_addition)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_inval_add";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "textures"));
        ASSERT_TRUE(fs::create_directories(temp_dir / "models"));

        ASSERT_TRUE(write_dummy_file(temp_dir / "textures" / "albedo.png"));
        ASSERT_TRUE(write_dummy_file(temp_dir / "models" / "hero.fbx"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        model.set_expanded_by_path("textures", true);

        const auto initial_row_count = model.visible_row_count();
        ASSERT_EQ(initial_row_count, 4u); // Assets, models, textures, albedo.png

        // Act: Add new file to textures folder and invalidate that file path
        const auto new_file_path = temp_dir / "textures" / "normal.png";
        ASSERT_TRUE(write_dummy_file(new_file_path));
        model.invalidate_path(new_file_path);

        // Assert: New file appears in tree and visible rows
        EXPECT_EQ(model.visible_row_count(), initial_row_count + 1u);
        const auto* normal_node = model.find_node_by_path("textures/normal.png");
        ASSERT_NE(normal_node, nullptr);
        EXPECT_EQ(normal_node->name, "normal.png");
        EXPECT_EQ(normal_node->kind, tree_node_kind::loose_file);

        // Models branch remains untouched and collapsed
        const auto* models_node = model.find_node_by_path("models");
        ASSERT_NE(models_node, nullptr);
        EXPECT_FALSE(models_node->is_expanded);
        ASSERT_EQ(models_node->children.size(), 1u);
        EXPECT_EQ(models_node->children.front()->name, "hero.fbx");

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that invalidating a directory after removing a file on disk purges it
    /// from the tree and removes it from visible rows.
    TEST(project_tree_model_test, invalidate_path_file_removal)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_inval_remove";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "scripts"));

        const auto script_one = temp_dir / "scripts" / "player.lua";
        const auto script_two = temp_dir / "scripts" / "enemy.lua";
        ASSERT_TRUE(write_dummy_file(script_one));
        ASSERT_TRUE(write_dummy_file(script_two));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        model.set_expanded_by_path("scripts", true);
        ASSERT_EQ(model.visible_row_count(), 4u); // Assets, scripts, enemy.lua, player.lua

        // Act: Remove enemy.lua and invalidate path
        ASSERT_TRUE(fs::remove(script_two));
        model.invalidate_path(script_two);

        // Assert: enemy.lua is purged from tree and visible rows
        EXPECT_EQ(model.visible_row_count(), 3u); // Assets, scripts, player.lua
        EXPECT_EQ(model.find_node_by_path("scripts/enemy.lua"), nullptr);
        const auto* remaining_node = model.find_node_by_path("scripts/player.lua");
        ASSERT_NE(remaining_node, nullptr);
        EXPECT_EQ(remaining_node->name, "player.lua");

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that modifying a .tmeta sidecar on disk and calling invalidate_path
    /// reloads the sidecar and reflects updated sub-assets while preserving expansion state.
    TEST(project_tree_model_test, invalidate_path_sidecar_modification_updates_sub_assets)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_inval_meta";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir));

        const auto mesh_path = temp_dir / "mesh.fbx";
        const auto meta_path = temp_dir / "mesh.fbx.tmeta";
        ASSERT_TRUE(write_dummy_file(mesh_path));

        const auto source_guid = guid::generate_random_guid();
        const auto sub_guid_mesh = guid::generate_random_guid();

        auto meta = assets::sidecar_meta{};
        meta.source_guid = source_guid;
        meta.importer = "model_importer";
        meta.sub_assets["mesh/0"] = sub_guid_mesh;
        meta.sub_asset_types["mesh/0"] = assets::asset_type_id::from_hash(0x1010);
        ASSERT_TRUE(meta.save_to_file(meta_path.generic_string()));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        // Expand mesh.fbx to reveal sub-assets
        model.set_expanded_by_path("mesh.fbx", true);
        ASSERT_EQ(model.visible_row_count(), 3u); // Assets, mesh.fbx, mesh/0

        // Act: Update .tmeta with an additional material sub-asset and invalidate
        const auto sub_guid_mat = guid::generate_random_guid();
        meta.sub_assets["material/0"] = sub_guid_mat;
        meta.sub_asset_types["material/0"] = assets::asset_type_id::from_hash(0x2020);
        ASSERT_TRUE(meta.save_to_file(meta_path.generic_string()));

        model.invalidate_path(meta_path);

        // Assert: Both sub-assets are present, mesh.fbx remains expanded, visible rows updated
        const auto* mesh_node = model.find_node_by_path("mesh.fbx");
        ASSERT_NE(mesh_node, nullptr);
        EXPECT_TRUE(mesh_node->is_expanded);
        ASSERT_EQ(mesh_node->children.size(), 2u);
        EXPECT_EQ(mesh_node->children[0]->name, "material/0");
        EXPECT_EQ(mesh_node->children[1]->name, "mesh/0");
        EXPECT_EQ(model.visible_row_count(), 4u); // Assets, mesh.fbx, material/0, mesh/0

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that child directory expansion states are preserved across directory rescans.
    TEST(project_tree_model_test, preserve_expansion_states_across_directory_rescan)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_inval_preserve_exp";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "textures" / "characters"));

        const auto hero_path = temp_dir / "textures" / "characters" / "hero.png";
        ASSERT_TRUE(write_dummy_file(hero_path));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        // Expand "textures" and "textures/characters"
        model.set_expanded_by_path("textures", true);
        model.set_expanded_by_path("textures/characters", true);

        ASSERT_EQ(model.visible_row_count(), 4u); // Assets, textures, characters, hero.png

        // Act: Add another file to the nested directory and invalidate the parent textures directory
        const auto villain_path = temp_dir / "textures" / "characters" / "villain.png";
        ASSERT_TRUE(write_dummy_file(villain_path));
        model.invalidate_path(temp_dir / "textures");

        // Assert: Nested directory remains expanded and shows both children
        const auto* characters_node = model.find_node_by_path("textures/characters");
        ASSERT_NE(characters_node, nullptr);
        EXPECT_TRUE(characters_node->is_expanded);
        ASSERT_EQ(characters_node->children.size(), 2u);

        EXPECT_EQ(model.visible_row_count(), 5u); // Assets, textures, characters, hero.png, villain.png

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies deferred invalidation lifecycle: events during deferral do not mutate visible rows
    /// until deferral is disabled, which automatically flushes pending invalidations.
    TEST(project_tree_model_test, deferred_invalidation_lifecycle)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_inval_defer";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "sounds"));

        const auto sound_one = temp_dir / "sounds" / "ambient.wav";
        ASSERT_TRUE(write_dummy_file(sound_one));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        model.set_expanded_by_path("sounds", true);

        const auto baseline_rows = model.visible_row_count();
        ASSERT_EQ(baseline_rows, 3u); // Assets, sounds, ambient.wav

        // Act: Enable invalidation deferral
        EXPECT_FALSE(model.is_invalidation_deferred());
        EXPECT_FALSE(model.has_deferred_invalidation());
        model.set_invalidation_deferred(true);
        EXPECT_TRUE(model.is_invalidation_deferred());

        // Create new file and invalidate path while deferred
        const auto sound_two = temp_dir / "sounds" / "music.wav";
        ASSERT_TRUE(write_dummy_file(sound_two));
        model.invalidate_path(sound_two);

        // Assert: Deferred state is flagged, but model visible rows remain untouched
        EXPECT_TRUE(model.has_deferred_invalidation());
        EXPECT_EQ(model.visible_row_count(), baseline_rows);
        EXPECT_EQ(model.find_node_by_path("sounds/music.wav"), nullptr);

        // Act: Disable invalidation deferral (triggers automatic flush)
        model.set_invalidation_deferred(false);

        // Assert: Deferral flushed, new file appears in visible rows and tree
        EXPECT_FALSE(model.is_invalidation_deferred());
        EXPECT_FALSE(model.has_deferred_invalidation());
        EXPECT_EQ(model.visible_row_count(), baseline_rows + 1u);
        const auto* music_node = model.find_node_by_path("sounds/music.wav");
        ASSERT_NE(music_node, nullptr);
        EXPECT_EQ(music_node->name, "music.wav");

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that handling a rename watch event invalidates both the source and destination directories.
    TEST(project_tree_model_test, handle_file_watch_event_rename_refreshes_both_directories)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_inval_rename";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "source_dir"));
        ASSERT_TRUE(fs::create_directories(temp_dir / "dest_dir"));

        const auto old_file_path = temp_dir / "source_dir" / "asset.txt";
        const auto new_file_path = temp_dir / "dest_dir" / "renamed_asset.txt";
        ASSERT_TRUE(write_dummy_file(old_file_path));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        model.set_expanded_by_path("source_dir", true);
        model.set_expanded_by_path("dest_dir", true);

        ASSERT_EQ(model.visible_row_count(), 4u); // Assets, dest_dir, source_dir, asset.txt
        EXPECT_NE(model.find_node_by_path("source_dir/asset.txt"), nullptr);
        EXPECT_EQ(model.find_node_by_path("dest_dir/renamed_asset.txt"), nullptr);

        // Act: Move file on disk and dispatch rename watch event
        ASSERT_TRUE(fs::rename(old_file_path, new_file_path));

        const auto rename_event = file_watch_event{
            .type = file_watch_event_type::renamed,
            .path = new_file_path,
            .old_path = old_file_path,
        };
        model.handle_file_watch_event(rename_event);

        // Assert: Old directory purged of file, new directory populated with file
        EXPECT_EQ(model.find_node_by_path("source_dir/asset.txt"), nullptr);
        const auto* renamed_node = model.find_node_by_path("dest_dir/renamed_asset.txt");
        ASSERT_NE(renamed_node, nullptr);
        EXPECT_EQ(renamed_node->name, "renamed_asset.txt");
        EXPECT_EQ(model.visible_row_count(), 4u); // Assets, dest_dir, renamed_asset.txt, source_dir

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that batch event processing via handle_file_watch_events refreshes all affected paths.
    TEST(project_tree_model_test, handle_file_watch_events_batch)
    {
        // Setup
        const auto temp_dir = fs::temp_directory_path() / "tempest_test_inval_batch";
        static_cast<void>(fs::remove_all(temp_dir));
        ASSERT_TRUE(fs::create_directories(temp_dir / "batch_dir"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = temp_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        model.set_expanded_by_path("batch_dir", true);
        ASSERT_EQ(model.visible_row_count(), 2u); // Assets, batch_dir

        // Act: Create two files on disk and dispatch batch events
        const auto file_one = temp_dir / "batch_dir" / "first.bin";
        const auto file_two = temp_dir / "batch_dir" / "second.bin";
        ASSERT_TRUE(write_dummy_file(file_one));
        ASSERT_TRUE(write_dummy_file(file_two));

        auto events = vector<file_watch_event>{};
        events.push_back(file_watch_event{
            .type = file_watch_event_type::added,
            .path = file_one,
        });
        events.push_back(file_watch_event{
            .type = file_watch_event_type::added,
            .path = file_two,
        });

        model.handle_file_watch_events(events);

        // Assert: Both files now present in tree and visible rows
        EXPECT_EQ(model.visible_row_count(), 4u);
        EXPECT_NE(model.find_node_by_path("batch_dir/first.bin"), nullptr);
        EXPECT_NE(model.find_node_by_path("batch_dir/second.bin"), nullptr);

        // Teardown
        static_cast<void>(fs::remove_all(temp_dir));
    }

    /// @brief Verifies that move operations transfer and preserve invalidation deferral state.
    TEST(project_tree_model_test, move_operations_preserve_deferral_state)
    {
        // Setup
        auto source_model = project_tree_model{};
        source_model.set_invalidation_deferred(true);
        source_model.invalidate_path("deferred/sample_path.txt");

        EXPECT_TRUE(source_model.is_invalidation_deferred());
        EXPECT_TRUE(source_model.has_deferred_invalidation());

        // Act: Move construct
        auto moved_model = project_tree_model{tempest::move(source_model)};

        // Assert: Moved model retains deferral state; source is reset to defaults
        EXPECT_TRUE(moved_model.is_invalidation_deferred());
        EXPECT_TRUE(moved_model.has_deferred_invalidation());
        EXPECT_FALSE(source_model.is_invalidation_deferred());
        EXPECT_FALSE(source_model.has_deferred_invalidation());

        // Act: Move assign
        auto assigned_model = project_tree_model{};
        assigned_model = tempest::move(moved_model);

        // Assert: Assigned model retains deferral state; moved model is reset
        EXPECT_TRUE(assigned_model.is_invalidation_deferred());
        EXPECT_TRUE(assigned_model.has_deferred_invalidation());
        EXPECT_FALSE(moved_model.is_invalidation_deferred());
        EXPECT_FALSE(moved_model.has_deferred_invalidation());
    }
} // namespace tempest::editor::tests
