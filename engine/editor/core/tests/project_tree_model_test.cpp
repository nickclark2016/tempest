#include <gtest/gtest.h>

#include <tempest/asset_database.hpp>
#include <tempest/asset_type_id.hpp>
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
} // namespace tempest::editor::tests
