#include <tempest/windows/project_view_window.hpp>

#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/editor.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/entity_hierarchy.hpp>
#include <tempest/event.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/material.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/span.hpp>
#include <tempest/texture.hpp>
#include <tempest/ui.hpp>
#include <tempest/vertex.hpp>

#include <gtest/gtest.h>
#include <imgui.h>

namespace tempest::editor::tests
{
    namespace
    {
        namespace fs = tempest::filesystem;

        auto write_dummy_file(const fs::path& target_path, string_view content = "dummy") -> bool
        {
            const auto span_bytes = span<const byte>{
                reinterpret_cast<const byte*>(content.data()),
                content.size(),
            };
            return write_file_from_bytes(target_path, span_bytes).has_value();
        }

        struct scoped_temp_directory
        {
            fs::path path;

            explicit scoped_temp_directory(const fs::path& target_path)
                : path{target_path}
            {
                fs::create_directories(path);
            }

            ~scoped_temp_directory()
            {
                if (fs::exists(path))
                {
                    fs::remove_all(path);
                }
            }

            scoped_temp_directory(const scoped_temp_directory&) = delete;
            scoped_temp_directory(scoped_temp_directory&&) noexcept = delete;
            auto operator=(const scoped_temp_directory&) -> scoped_temp_directory& = delete;
            auto operator=(scoped_temp_directory&&) noexcept -> scoped_temp_directory& = delete;
        };
    } // namespace

    class project_view_window_test : public ::testing::Test
    {
      protected:
        event::event_registry events{};
        ecs::component_type_registry types{};
        ecs::archetype_registry registry{events, types};
        scene::scene_manager scene_mgr{registry};
        editor_selection selection{};
    };

    // ============================================================================
    // Window Properties & Lifecycle Tests
    // ============================================================================

    /// @brief Verifies that project_view_window provides the expected initial dock location.
    TEST_F(project_view_window_test, desired_initial_dock_is_bottom)
    {
        // Setup: Construct default project_view_window
        const auto window = project_view_window{scene_mgr, selection};

        // Act & Assert: Window requests bottom dock region
        EXPECT_EQ(window.desired_initial_dock(), editor_window::dock_location::bottom);
    }

    /// @brief Verifies that project_view_window exposes the designated window name "Project".
    TEST_F(project_view_window_test, window_name_is_project)
    {
        // Setup: Construct default project_view_window
        const auto window = project_view_window{scene_mgr, selection};

        // Act & Assert: Window name is exactly "Project"
        EXPECT_EQ(window.window_name(), "Project");
    }

    /// @brief Verifies that project_view_window supports all editor simulation modes (edit, paused, playing).
    TEST_F(project_view_window_test, simulation_modes_supported)
    {
        // Setup: Construct default project_view_window
        const auto window = project_view_window{scene_mgr, selection};

        // Act & Assert: All simulation modes return true
        EXPECT_TRUE(window.is_mode_supported(simulation_state::edit));
        EXPECT_TRUE(window.is_mode_supported(simulation_state::paused));
        EXPECT_TRUE(window.is_mode_supported(simulation_state::playing));
    }

    /// @brief Verifies initial open state and toggling behavior.
    TEST_F(project_view_window_test, open_state_and_toggling)
    {
        // Setup: Construct default project_view_window
        auto window = project_view_window{scene_mgr, selection};

        // Assert: Window is open by default
        EXPECT_TRUE(window.is_open());

        // Act: Toggle open off
        window.set_open(false);

        // Assert: Window is closed
        EXPECT_FALSE(window.is_open());

        // Act: Toggle open back on
        window.set_open(true);

        // Assert: Window is open
        EXPECT_TRUE(window.is_open());
    }

    // ============================================================================
    // Model Access & Replacement Tests
    // ============================================================================

    /// @brief Verifies mutable and const access to the backing project_tree_model, as well as replacing it.
    TEST_F(project_view_window_test, model_access_and_mutation)
    {
        // Setup: Construct project_view_window
        auto window = project_view_window{scene_mgr, selection};

        // Assert: Initially dirty by default until visible rows are rebuilt
        auto& model = window.get_model();
        EXPECT_TRUE(model.is_dirty());

        // Act & Assert: Rebuilding visible rows marks model clean
        model.rebuild_visible_rows();
        EXPECT_FALSE(model.is_dirty());

        // Act & Assert: Marking dirty sets the flag
        model.mark_dirty();
        EXPECT_TRUE(model.is_dirty());

        // Act & Assert: Const accessor reflects the model state
        const auto& const_window = window;
        const auto& const_model = const_window.get_model();
        EXPECT_TRUE(const_model.is_dirty());

        // Act: Replace model via set_model with a clean model
        auto new_model = project_tree_model{};
        new_model.rebuild_visible_rows();
        EXPECT_FALSE(new_model.is_dirty());
        window.set_model(tempest::move(new_model));

        // Assert: Replaced model has its clean state
        EXPECT_FALSE(window.get_model().is_dirty());
    }

    /// @brief Verifies that project_view_window can be constructed directly from a project_tree_model.
    TEST_F(project_view_window_test, construct_from_model)
    {
        // Setup: Create a model with dirty flag set
        auto source_model = project_tree_model{};
        source_model.mark_dirty();

        // Act: Construct window from model rvalue
        const auto window = project_view_window{tempest::move(source_model), scene_mgr, selection};

        // Assert: Window retains the provided model's state
        EXPECT_TRUE(window.get_model().is_dirty());
    }

    // ============================================================================
    // Search Bar & Query Management Tests
    // ============================================================================

    /// @brief Verifies that setting search query updates both the buffer and the backing model.
    TEST_F(project_view_window_test, search_buffer_input_updates_model_query)
    {
        // Setup: Construct default project_view_window
        auto window = project_view_window{scene_mgr, selection};

        // Assert: Initial search state is empty
        EXPECT_TRUE(window.search_query().empty());
        EXPECT_FALSE(window.get_model().is_search_active());

        // Act: Set search query via window API
        window.set_search_query("character");

        // Assert: Window search query and backing model are synchronized
        EXPECT_EQ(window.search_query(), "character");
        EXPECT_EQ(window.get_model().search_query(), "character");
        EXPECT_TRUE(window.get_model().is_search_active());
    }

    /// @brief Verifies that clearing the search query resets both buffer and backing model filter.
    TEST_F(project_view_window_test, clear_search_resets_buffer_and_model_query)
    {
        // Setup: Construct window with active search filter
        auto window = project_view_window{scene_mgr, selection};
        window.set_search_query("texture");
        EXPECT_EQ(window.search_query(), "texture");
        EXPECT_TRUE(window.get_model().is_search_active());

        // Act: Clear search filter
        window.clear_search_query();

        // Assert: Both buffer and model search state are restored to empty
        EXPECT_TRUE(window.search_query().empty());
        EXPECT_TRUE(window.get_model().search_query().empty());
        EXPECT_FALSE(window.get_model().is_search_active());
    }

    // ============================================================================
    // Badge Resolution Tests
    // ============================================================================

    /// @brief Verifies badge resolution across all tree node kinds, extensions, and asset types.
    TEST_F(project_view_window_test, badge_resolution_across_kinds_and_extensions)
    {
        // Setup: Directory-like rows
        const auto root_row = project_tree_row{
            .display_label = "Assets",
            .kind = tree_node_kind::mount_root,
        };
        const auto dir_row = project_tree_row{
            .display_label = "models",
            .kind = tree_node_kind::directory,
        };

        // Assert: Folders
        EXPECT_EQ(project_view_window::get_node_badge(root_row), "[Folder]");
        EXPECT_EQ(project_view_window::get_node_badge(dir_row), "[Folder]");

        // Setup: Loose file row
        const auto loose_row = project_tree_row{
            .display_label = "untracked.dat",
            .kind = tree_node_kind::loose_file,
        };
        EXPECT_EQ(project_view_window::get_node_badge(loose_row), "[Loose]");

        // Setup & Assert: Imported files procedural extension badge
        const auto gltf_row = project_tree_row{
            .display_label = "environment.gltf",
            .kind = tree_node_kind::imported_file,
        };
        const auto exr_row = project_tree_row{
            .display_label = "sky.exr",
            .kind = tree_node_kind::imported_file,
        };
        const auto png_row = project_tree_row{
            .display_label = "albedo.png",
            .kind = tree_node_kind::imported_file,
        };
        const auto lua_row = project_tree_row{
            .display_label = "script.lua",
            .kind = tree_node_kind::imported_file,
        };
        const auto no_ext_row = project_tree_row{
            .display_label = "raw_asset_data",
            .kind = tree_node_kind::imported_file,
        };
        EXPECT_EQ(project_view_window::get_node_badge(gltf_row), "[GLTF]");
        EXPECT_EQ(project_view_window::get_node_badge(exr_row), "[EXR]");
        EXPECT_EQ(project_view_window::get_node_badge(png_row), "[PNG]");
        EXPECT_EQ(project_view_window::get_node_badge(lua_row), "[LUA]");
        EXPECT_EQ(project_view_window::get_node_badge(no_ext_row), "[Asset]");

        // Setup & Assert: Imported files by explicit asset type ID
        const auto typed_mat_row = project_tree_row{
            .display_label = "opaque_blob",
            .kind = tree_node_kind::imported_file,
            .type = assets::asset_type_id::of<core::material>(),
        };
        const auto typed_mesh_row = project_tree_row{
            .display_label = "opaque_blob",
            .kind = tree_node_kind::imported_file,
            .type = assets::asset_type_id::of<core::mesh>(),
        };
        const auto typed_tex_row = project_tree_row{
            .display_label = "opaque_blob",
            .kind = tree_node_kind::imported_file,
            .type = assets::asset_type_id::of<core::texture>(),
        };
        EXPECT_EQ(project_view_window::get_node_badge(typed_mat_row), "[Material]");
        EXPECT_EQ(project_view_window::get_node_badge(typed_mesh_row), "[Mesh]");
        EXPECT_EQ(project_view_window::get_node_badge(typed_tex_row), "[Texture]");

        // Setup & Assert: Sub-assets by key prefix
        const auto sub_mesh_row = project_tree_row{
            .display_label = "mesh/0",
            .kind = tree_node_kind::sub_asset,
        };
        const auto sub_mat_row = project_tree_row{
            .display_label = "material/default",
            .kind = tree_node_kind::sub_asset,
        };
        const auto sub_prefab_row = project_tree_row{
            .display_label = "prefab/root",
            .kind = tree_node_kind::sub_asset,
        };
        const auto sub_tex_row = project_tree_row{
            .display_label = "texture/albedo",
            .kind = tree_node_kind::sub_asset,
        };
        const auto sub_generic_row = project_tree_row{
            .display_label = "chunk/custom_data",
            .kind = tree_node_kind::sub_asset,
        };
        EXPECT_EQ(project_view_window::get_node_badge(sub_mesh_row), "[Mesh]");
        EXPECT_EQ(project_view_window::get_node_badge(sub_mat_row), "[Material]");
        EXPECT_EQ(project_view_window::get_node_badge(sub_prefab_row), "[Prefab]");
        EXPECT_EQ(project_view_window::get_node_badge(sub_tex_row), "[Texture]");
        EXPECT_EQ(project_view_window::get_node_badge(sub_generic_row), "[SubAsset]");

        // Setup & Assert: Sub-assets by type ID
        const auto sub_typed_mesh_row = project_tree_row{
            .display_label = "custom_slot",
            .kind = tree_node_kind::sub_asset,
            .type = assets::asset_type_id::of<core::mesh>(),
        };
        const auto sub_typed_tex_row = project_tree_row{
            .display_label = "custom_slot",
            .kind = tree_node_kind::sub_asset,
            .type = assets::asset_type_id::of<core::texture>(),
        };
        EXPECT_EQ(project_view_window::get_node_badge(sub_typed_mesh_row), "[Mesh]");
        EXPECT_EQ(project_view_window::get_node_badge(sub_typed_tex_row), "[Texture]");

        // Setup & Assert: Procedural resolution via asset_type_registry
        assets::asset_type_registry custom_type_reg;
        custom_type_reg.register_type<core::mesh>(nullptr, nullptr);
        assets::asset_database custom_asset_db{&custom_type_reg};
        const auto registry_resolved_row = project_tree_row{
            .display_label = "dynamic_entry",
            .kind = tree_node_kind::imported_file,
            .type = assets::asset_type_id::of<core::mesh>(),
        };
        EXPECT_EQ(project_view_window::get_node_badge(registry_resolved_row, &custom_asset_db), "[Mesh]");
    }

    // ============================================================================
    // Breadcrumb Navigation Tests
    // ============================================================================

    /// @brief Verifies that split_breadcrumb_path correctly tokenizes hierarchical path strings.
    TEST_F(project_view_window_test, split_breadcrumb_path_parsing)
    {
        // Act & Assert: Empty path yields empty breadcrumb list
        const auto empty_crumbs = project_view_window::split_breadcrumb_path("");
        EXPECT_TRUE(empty_crumbs.empty());

        // Act & Assert: Single segment path
        const auto single_crumbs = project_view_window::split_breadcrumb_path("Assets");
        ASSERT_EQ(single_crumbs.size(), 1u);
        EXPECT_EQ(single_crumbs[0].name, "Assets");
        EXPECT_EQ(single_crumbs[0].path, "Assets");

        // Act & Assert: Multi-segment forward slash path
        const auto multi_crumbs = project_view_window::split_breadcrumb_path("Assets/models/characters");
        ASSERT_EQ(multi_crumbs.size(), 3u);
        EXPECT_EQ(multi_crumbs[0].name, "Assets");
        EXPECT_EQ(multi_crumbs[0].path, "Assets");
        EXPECT_EQ(multi_crumbs[1].name, "models");
        EXPECT_EQ(multi_crumbs[1].path, "Assets/models");
        EXPECT_EQ(multi_crumbs[2].name, "characters");
        EXPECT_EQ(multi_crumbs[2].path, "Assets/models/characters");

        // Act & Assert: Windows backslash separators
        const auto backslash_crumbs = project_view_window::split_breadcrumb_path("Assets\\models\\characters");
        ASSERT_EQ(backslash_crumbs.size(), 3u);
        EXPECT_EQ(backslash_crumbs[0].name, "Assets");
        EXPECT_EQ(backslash_crumbs[0].path, "Assets");
        EXPECT_EQ(backslash_crumbs[1].name, "models");
        EXPECT_EQ(backslash_crumbs[1].path, "Assets/models");
        EXPECT_EQ(backslash_crumbs[2].name, "characters");
        EXPECT_EQ(backslash_crumbs[2].path, "Assets/models/characters");

        // Act & Assert: Leading, trailing, and duplicate redundant separators
        const auto redundant_crumbs = project_view_window::split_breadcrumb_path("/Assets///models/");
        ASSERT_EQ(redundant_crumbs.size(), 2u);
        EXPECT_EQ(redundant_crumbs[0].name, "Assets");
        EXPECT_EQ(redundant_crumbs[0].path, "Assets");
        EXPECT_EQ(redundant_crumbs[1].name, "models");
        EXPECT_EQ(redundant_crumbs[1].path, "Assets/models");
    }

    /// @brief Verifies that navigate_to_directory updates current_directory and expands ancestor chain.
    TEST_F(project_view_window_test, navigate_to_directory_expands_ancestors)
    {
        // Setup: Create physical directory structure
        const auto base_dir = fs::temp_directory_path() / "tempest_test_breadcrumbs_nav";
        const auto temp_guard = scoped_temp_directory{base_dir};

        const auto models_dir = base_dir / "models";
        const auto characters_dir = models_dir / "characters";
        fs::create_directories(characters_dir);
        ASSERT_TRUE(write_dummy_file(characters_dir / "hero.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        // Assert: Root mount is initially expanded by default, but subdirectories are collapsed
        const auto* initial_models = window.get_model().find_node_by_path("Assets/models");
        ASSERT_NE(initial_models, nullptr);
        EXPECT_FALSE(initial_models->is_expanded);

        const auto* initial_characters = window.get_model().find_node_by_path("Assets/models/characters");
        ASSERT_NE(initial_characters, nullptr);
        EXPECT_FALSE(initial_characters->is_expanded);

        // Act: Navigate to leaf directory "Assets/models/characters"
        window.navigate_to_directory("Assets/models/characters");

        // Assert: current_directory updated and all ancestors expanded
        EXPECT_EQ(window.current_directory(), "Assets/models/characters");

        const auto* expanded_root = window.get_model().find_node_by_path("Assets");
        ASSERT_NE(expanded_root, nullptr);
        EXPECT_TRUE(expanded_root->is_expanded);

        const auto* expanded_models = window.get_model().find_node_by_path("Assets/models");
        ASSERT_NE(expanded_models, nullptr);
        EXPECT_TRUE(expanded_models->is_expanded);

        const auto* expanded_characters = window.get_model().find_node_by_path("Assets/models/characters");
        ASSERT_NE(expanded_characters, nullptr);
        EXPECT_TRUE(expanded_characters->is_expanded);

        // Act: Navigate to ancestor directory "Assets/models"
        window.navigate_to_directory("Assets/models");

        // Assert: current_directory updated to ancestor
        EXPECT_EQ(window.current_directory(), "Assets/models");
    }

    // ============================================================================
    // Headless ImGui Draw Tests
    // ============================================================================

    /// @brief Verifies that draw() executes without throwing when the window is open and when closed.
    TEST_F(project_view_window_test, draw_execution_headless)
    {
        // Setup: Initialize engine context and UI context for ImGui command recording
        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Project View Headless Draw Test",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        auto window = project_view_window{scene_mgr, selection};

        // Act: Render frame with open = true
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();
        EXPECT_TRUE(window.is_open());

        // Act: Render frame with is_open = false (early returns without ImGui window)
        window.set_open(false);
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();
        EXPECT_FALSE(window.is_open());
    }

    /// @brief Verifies virtualized row rendering and breadcrumbs with populated directory hierarchy.
    TEST_F(project_view_window_test, draw_headless_with_populated_tree)
    {
        // Setup: Create physical directory structure with files and sidecars
        const auto base_dir = fs::temp_directory_path() / "tempest_test_draw_populated";
        const auto temp_guard = scoped_temp_directory{base_dir};

        const auto textures_dir = base_dir / "textures";
        const auto models_dir = base_dir / "models";
        fs::create_directories(textures_dir);
        fs::create_directories(models_dir);

        ASSERT_TRUE(write_dummy_file(textures_dir / "albedo.png"));
        ASSERT_TRUE(write_dummy_file(textures_dir / "normal.jpg"));
        ASSERT_TRUE(write_dummy_file(models_dir / "hero.model"));
        ASSERT_TRUE(write_dummy_file(base_dir / "readme.txt"));

        // Setup sidecar metadata for sub-assets
        auto meta = assets::sidecar_meta{};
        meta.source_guid = guid::generate_random_guid();
        meta.importer = "model_importer";
        meta.sub_assets["mesh/0"] = guid::generate_random_guid();
        meta.sub_assets["material/0"] = guid::generate_random_guid();
        meta.sub_asset_types["mesh/0"] = assets::asset_type_id::of<core::mesh>();
        meta.sub_asset_types["material/0"] = assets::asset_type_id::of<core::material>();
        ASSERT_TRUE(meta.save_to_file((models_dir / "hero.model.tmeta").generic_string()));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Project View Populated Draw Test",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        // Act: Draw initial state through ImGui clipper
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();

        // Act: Expand all nodes so virtualized rows span full hierarchy
        window.get_model().expand_all();
        EXPECT_GT(window.get_model().visible_row_count(), 3u);

        // Act: Render with expanded rows
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();
    }

    /// @brief Verifies that search filtering renders matching rows through virtualization in draw().
    TEST_F(project_view_window_test, draw_headless_with_search_filtering)
    {
        // Setup: Create physical directory structure
        const auto base_dir = fs::temp_directory_path() / "tempest_test_draw_search";
        const auto temp_guard = scoped_temp_directory{base_dir};

        const auto dir = base_dir / "content";
        fs::create_directories(dir);
        ASSERT_TRUE(write_dummy_file(dir / "warrior.model"));
        ASSERT_TRUE(write_dummy_file(dir / "archer.model"));
        ASSERT_TRUE(write_dummy_file(dir / "landscape.png"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Project View Search Draw Test",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        // Act: Filter by search query "warrior"
        window.set_search_query("warrior");
        EXPECT_TRUE(window.get_model().is_search_active());

        // Assert: Draw renders filtered search results without error
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();

        // Act: Filter with no matching assets
        window.set_search_query("nonexistent_item_xyz");
        EXPECT_EQ(window.get_model().visible_row_count(), 0u);

        // Assert: Draw renders 0 matches without error
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();

        // Act: Clear search query
        window.clear_search_query();
        EXPECT_FALSE(window.get_model().is_search_active());

        // Assert: Draw returns to hierarchical rendering
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();
    }

    // ============================================================================
    // Selection Management Tests
    // ============================================================================

    /// @brief Verifies that project_view_window manages item selection state and exposes expected queries.
    TEST_F(project_view_window_test, selection_query_and_mutation_apis)
    {
        // Setup: Construct model with mount points and files
        const auto base_dir = fs::temp_directory_path() / "tempest_test_proj_view_sel";
        const auto temp_guard = scoped_temp_directory{base_dir};
        const auto sub_dir = base_dir / "textures";
        fs::create_directories(sub_dir);
        ASSERT_TRUE(write_dummy_file(sub_dir / "albedo.png"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "GameAssets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        // Assert: Default selection state is empty
        EXPECT_FALSE(window.has_selection());
        EXPECT_TRUE(window.selected_path().empty());
        EXPECT_EQ(window.selected_asset_id(), guid{});
        EXPECT_EQ(window.selected_kind(), tree_node_kind::directory);

        // Act: Set selection to directory
        window.set_selected_path("GameAssets/textures");

        // Assert: Selected path and kind are updated
        EXPECT_TRUE(window.has_selection());
        EXPECT_EQ(window.selected_path(), "GameAssets/textures");
        EXPECT_EQ(window.selected_kind(), tree_node_kind::directory);

        // Act: Set selection to file
        window.set_selected_path("GameAssets/textures/albedo.png");

        // Assert: Selected path is updated
        EXPECT_TRUE(window.has_selection());
        EXPECT_EQ(window.selected_path(), "GameAssets/textures/albedo.png");

        // Act: Clear selection
        window.clear_selection();

        // Assert: Selection is cleared
        EXPECT_FALSE(window.has_selection());
        EXPECT_TRUE(window.selected_path().empty());
        EXPECT_EQ(window.selected_asset_id(), guid{});
    }

    /// @brief Verifies that selected rows render with ImGui selection flags without graphical issues.
    TEST_F(project_view_window_test, selection_highlighting_during_draw)
    {
        // Setup: Initialize engine context and UI context
        const auto base_dir = fs::temp_directory_path() / "tempest_test_proj_view_draw_sel";
        const auto temp_guard = scoped_temp_directory{base_dir};
        ASSERT_TRUE(write_dummy_file(base_dir / "icon.png"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Content",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);

        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Project View Selection Draw Test",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        // Act: Select file and render frame
        window.set_selected_path("Content/icon.png");
        EXPECT_TRUE(window.has_selection());

        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();

        // Assert: Selection state remains intact after draw
        EXPECT_TRUE(window.has_selection());
        EXPECT_EQ(window.selected_path(), "Content/icon.png");
    }

    // ============================================================================
    // Activation & Double-Click Tests
    // ============================================================================

    /// @brief Verifies that activating a directory toggles its expansion state and navigates current_directory.
    TEST_F(project_view_window_test, double_click_directory_toggles_expansion_and_sets_current_directory)
    {
        // Setup: Construct model with folder
        const auto base_dir = fs::temp_directory_path() / "tempest_test_proj_view_expand";
        const auto temp_guard = scoped_temp_directory{base_dir};
        const auto sub_dir = base_dir / "scripts";
        fs::create_directories(sub_dir);
        ASSERT_TRUE(write_dummy_file(sub_dir / "test.lua"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        // Assert: Initial directory node is collapsed
        const auto* scripts_node = window.get_model().find_node_by_path("Assets/scripts");
        ASSERT_NE(scripts_node, nullptr);
        EXPECT_FALSE(scripts_node->is_expanded);

        // Act: Activate directory row to expand
        const auto row_to_expand = project_tree_row{
            .node = scripts_node,
            .display_label = "scripts",
            .relative_path = "Assets/scripts",
            .kind = tree_node_kind::directory,
            .is_expanded = false,
            .has_children = true,
        };
        window.activate_item(row_to_expand);

        // Assert: Node is expanded and current_directory updated
        EXPECT_TRUE(window.get_model().find_node_by_path("Assets/scripts")->is_expanded);
        EXPECT_EQ(window.current_directory(), "Assets/scripts");
        EXPECT_EQ(window.selected_path(), "Assets/scripts");

        // Act: Activate directory row again to collapse
        const auto row_to_collapse = project_tree_row{
            .node = scripts_node,
            .display_label = "scripts",
            .relative_path = "Assets/scripts",
            .kind = tree_node_kind::directory,
            .is_expanded = true,
            .has_children = true,
        };
        window.activate_item(row_to_collapse);

        // Assert: Node is now collapsed
        EXPECT_FALSE(window.get_model().find_node_by_path("Assets/scripts")->is_expanded);
    }

    /// @brief Verifies that activating an external file invokes the registered open_external_callback.
    TEST_F(project_view_window_test, double_click_external_file_invokes_callback)
    {
        // Setup: Construct window and register callback
        auto window = project_view_window{scene_mgr, selection};
        auto opened_path = string{};
        window.set_on_open_external_file([&opened_path](string_view path) {
            opened_path = string{path};
        });

        // Act: Activate a loose script file
        const auto row = project_tree_row{
            .display_label = "player_controller.lua",
            .relative_path = "scripts/player_controller.lua",
            .kind = tree_node_kind::loose_file,
        };
        window.activate_item(row);

        // Assert: Callback received the file path
        EXPECT_EQ(opened_path, "scripts/player_controller.lua");
        EXPECT_EQ(window.selected_path(), "scripts/player_controller.lua");
    }

    /// @brief Verifies that activating prefab or material assets invokes the inspect_asset_callback.
    TEST_F(project_view_window_test, double_click_prefab_and_material_invokes_inspect_callback)
    {
        // Setup: Construct window and register inspect callback
        auto window = project_view_window{scene_mgr, selection};
        auto inspected_guid = guid{};
        auto inspected_type = assets::asset_type_id{};
        auto inspected_path = string{};
        auto callback_invoked = false;

        window.set_on_inspect_asset([&](const guid& id, assets::asset_type_id type, string_view path) {
            inspected_guid = id;
            inspected_type = type;
            inspected_path = string{path};
            callback_invoked = true;
        });

        const auto test_guid = guid{array<byte, 16>{byte{1}, byte{2}, byte{3}, byte{4}}};
        const auto prefab_type = assets::asset_type_id::of<assets::entity_hierarchy>();
        const auto prefab_row = project_tree_row{
            .display_label = "enemy.tprefab",
            .relative_path = "prefabs/enemy.tprefab",
            .kind = tree_node_kind::imported_file,
            .asset_id = test_guid,
            .type = prefab_type,
        };

        // Act: Activate prefab row
        window.activate_item(prefab_row);

        // Assert: Inspect callback received prefab info
        EXPECT_TRUE(callback_invoked);
        EXPECT_EQ(inspected_guid, test_guid);
        EXPECT_EQ(inspected_type, prefab_type);
        EXPECT_EQ(inspected_path, "prefabs/enemy.tprefab");

        // Act: Activate material row
        callback_invoked = false;
        const auto material_type = assets::asset_type_id::of<core::material>();
        const auto material_row = project_tree_row{
            .display_label = "rust.tmaterial",
            .relative_path = "materials/rust.tmaterial",
            .kind = tree_node_kind::imported_file,
            .asset_id = test_guid,
            .type = material_type,
        };
        window.activate_item(material_row);

        // Assert: Inspect callback received material info
        EXPECT_TRUE(callback_invoked);
        EXPECT_EQ(inspected_type, material_type);
        EXPECT_EQ(inspected_path, "materials/rust.tmaterial");
    }

    // ============================================================================
    // Dirty Scene Protection & Scene Activation Tests
    // ============================================================================

    /// @brief Verifies that opening a scene when loaded scenes are clean immediately loads via scene_manager.
    TEST_F(project_view_window_test, double_click_clean_scene_loads_immediately)
    {
        // Setup: Create scene file on disk and initialize scene_manager
        const auto base_dir = fs::temp_directory_path() / "tempest_test_proj_view_clean_scene";
        const auto temp_guard = scoped_temp_directory{base_dir};
        const auto scene_path = base_dir / "level.tscene";
        ASSERT_TRUE(write_dummy_file(scene_path, R"({"schema_version":1,"generator":"test","entities":[]})"));

        auto window = project_view_window{scene_mgr, selection};

        // Act: Request opening scene
        window.open_scene_requested(scene_path.generic_string());

        // Assert: Loaded directly without opening dirty dialog
        EXPECT_FALSE(window.is_dirty_dialog_open());
        EXPECT_EQ(scene_mgr.get_scene_count(), 1u);
    }

    /// @brief Verifies that attempting to open a new scene while a loaded scene is dirty prompts save confirmation.
    TEST_F(project_view_window_test, double_click_dirty_scene_opens_dirty_dialog)
    {
        // Setup: Initialize scene_manager with a loaded dirty scene
        const auto load_result = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "active_level.tscene");
        ASSERT_TRUE(load_result.has_value());
        EXPECT_TRUE(scene_mgr.set_dirty(*load_result, true));
        EXPECT_TRUE(scene_mgr.is_dirty(*load_result));

        auto window = project_view_window{scene_mgr, selection};

        // Act: Request opening another scene
        window.open_scene_requested("next_level.tscene");

        // Assert: Dirty dialog is opened with pending scene and dirty scene details
        EXPECT_TRUE(window.is_dirty_dialog_open());
        EXPECT_EQ(window.pending_scene_path(), "next_level.tscene");
        EXPECT_EQ(window.dirty_scene_name(), "active_level.tscene");
    }

    /// @brief Verifies that confirming save-and-open saves the dirty scene and loads the target scene.
    TEST_F(project_view_window_test, confirm_save_and_open_saves_dirty_and_loads_new_scene)
    {
        // Setup: Create two scene files on disk
        const auto base_dir = fs::temp_directory_path() / "tempest_test_proj_view_save_open";
        const auto temp_guard = scoped_temp_directory{base_dir};
        const auto dirty_scene_path = base_dir / "dirty_scene.tscene";
        const auto next_scene_path = base_dir / "next_scene.tscene";
        ASSERT_TRUE(write_dummy_file(dirty_scene_path, R"({"schema_version":1,"generator":"test","entities":[]})"));
        ASSERT_TRUE(write_dummy_file(next_scene_path, R"({"schema_version":1,"generator":"test","entities":[]})"));

        const auto handle = scene_mgr.load_sync(dirty_scene_path);
        ASSERT_TRUE(handle.has_value());
        scene_mgr.set_dirty(*handle, true);
        EXPECT_TRUE(scene_mgr.is_dirty(*handle));

        auto window = project_view_window{scene_mgr, selection};

        // Act: Open scene requested and confirm save
        window.open_scene_requested(next_scene_path.generic_string());
        ASSERT_TRUE(window.is_dirty_dialog_open());

        window.confirm_save_and_open();

        // Assert: Dialog closed, scene is clean, and active scene is next_scene
        EXPECT_FALSE(window.is_dirty_dialog_open());
        const auto active_handle = scene_mgr.get_active_scene();
        EXPECT_TRUE(scene_mgr.is_valid(active_handle));
        EXPECT_FALSE(scene_mgr.is_dirty(active_handle));
        const auto active_path = scene_mgr.get_scene_path(active_handle);
        ASSERT_TRUE(active_path.has_value());
        EXPECT_EQ(*active_path, next_scene_path.generic_string());
    }

    /// @brief Verifies that confirming discard-and-open loads the target scene without saving dirty scene.
    TEST_F(project_view_window_test, confirm_discard_and_open_loads_without_saving)
    {
        // Setup: Create target scene file on disk
        const auto base_dir = fs::temp_directory_path() / "tempest_test_proj_view_discard_open";
        const auto temp_guard = scoped_temp_directory{base_dir};
        const auto next_scene_path = base_dir / "next_scene.tscene";
        ASSERT_TRUE(write_dummy_file(next_scene_path, R"({"schema_version":1,"generator":"test","entities":[]})"));

        const auto handle = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "unsaved_scene.tscene");
        ASSERT_TRUE(handle.has_value());
        scene_mgr.set_dirty(*handle, true);

        auto window = project_view_window{scene_mgr, selection};

        // Act: Open requested and confirm discard
        window.open_scene_requested(next_scene_path.generic_string());
        ASSERT_TRUE(window.is_dirty_dialog_open());

        window.confirm_discard_and_open();

        // Assert: Dialog closed and target scene loaded
        EXPECT_FALSE(window.is_dirty_dialog_open());
        const auto active_handle = scene_mgr.get_active_scene();
        EXPECT_TRUE(scene_mgr.is_valid(active_handle));
        const auto active_path = scene_mgr.get_scene_path(active_handle);
        ASSERT_TRUE(active_path.has_value());
        EXPECT_EQ(*active_path, next_scene_path.generic_string());
    }

    /// @brief Verifies that cancelling scene opening dismisses the dialog and preserves the dirty scene unmodified.
    TEST_F(project_view_window_test, cancel_open_scene_dismisses_dialog_and_retains_dirty_state)
    {
        // Setup: Initialize scene_manager with a dirty scene
        const auto handle = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "my_dirty_scene.tscene");
        ASSERT_TRUE(handle.has_value());
        scene_mgr.set_dirty(*handle, true);

        auto window = project_view_window{scene_mgr, selection};

        // Act: Open requested and cancel
        window.open_scene_requested("other.tscene");
        ASSERT_TRUE(window.is_dirty_dialog_open());

        window.cancel_open_scene();

        // Assert: Dialog closed, scene remains dirty and loaded
        EXPECT_FALSE(window.is_dirty_dialog_open());
        EXPECT_TRUE(scene_mgr.is_dirty(*handle));
        EXPECT_EQ(scene_mgr.get_scene_count(), 1u);
    }

    /// @brief Verifies that the "Save Changes?" confirmation popup modal renders via ImGui without crashes.
    TEST_F(project_view_window_test, headless_imgui_dirty_dialog_rendering)
    {
        // Setup: Initialize engine context and UI context
        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Save Changes Dialog Test Window",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        const auto handle = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "dialog_test_scene.tscene");
        ASSERT_TRUE(handle.has_value());
        scene_mgr.set_dirty(*handle, true);

        auto window = project_view_window{scene_mgr, selection};

        // Act: Trigger dirty dialog
        window.open_scene_requested("next.tscene");
        ASSERT_TRUE(window.is_dirty_dialog_open());

        // Assert: Draw renders modal dialog cleanly
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();

        // Act: Redraw second frame with modal active
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();

        // Assert: State remains valid
        EXPECT_TRUE(window.is_dirty_dialog_open());
    }

    // ============================================================================
    // Editor Context Integration Tests
    // ============================================================================

    /// @brief Verifies that editor_context initializes project_view_window and exposes valid accessors.
    TEST_F(project_view_window_test, editor_context_integration)
    {
        // Setup: Initialize engine context, test window, and UI context
        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Project View Integration Test Window",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        // Act: Construct editor_context
        auto ed_ctx = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Assert: project_view_window is non-null and valid
        auto* project_view = ed_ctx.get_project_view_window();
        ASSERT_NE(project_view, nullptr);
        EXPECT_EQ(project_view->window_name(), "Project");
        EXPECT_TRUE(project_view->is_open());

        // Assert: const accessor returns the identical instance
        const auto& const_ed_ctx = ed_ctx;
        const auto* const_project_view = const_ed_ctx.get_project_view_window();
        ASSERT_NE(const_project_view, nullptr);
        EXPECT_EQ(const_project_view, project_view);

        // Assert: Subsystem dependency injection is populated correctly
        EXPECT_EQ(&project_view->scene_manager(), &engine_ctx.get_scene_manager());
        EXPECT_EQ(&project_view->selection(), &ed_ctx.get_selection());

        // Act: Render frame via editor_context draw
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(ed_ctx.draw());
        ui_ctx.finish_ui_commands();

        // Act: Toggle window visibility off and redraw
        project_view->set_open(false);
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(ed_ctx.draw());
        ui_ctx.finish_ui_commands();
        EXPECT_FALSE(project_view->is_open());
    }

    // ============================================================================
    // Inline Renaming & Validation Tests
    // ============================================================================

    /// @brief Verifies that start_rename initiates rename state, copies item name, and defers model invalidation.
    TEST_F(project_view_window_test, start_rename_file_initiates_rename_and_defers_invalidation)
    {
        // Setup: Create directory hierarchy with a test file
        const auto base_dir = fs::temp_directory_path() / "tempest_test_start_rename";
        const auto temp_guard = scoped_temp_directory{base_dir};
        ASSERT_TRUE(write_dummy_file(base_dir / "notes.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Notes",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        // Act: Initiate inline rename for the file
        const auto start_result = window.start_rename("notes.txt");

        // Assert: Rename state is active, name buffered, and invalidation deferred
        EXPECT_TRUE(start_result);
        EXPECT_TRUE(window.is_renaming());
        EXPECT_EQ(window.renaming_path(), "notes.txt");
        EXPECT_EQ(window.rename_buffer(), "notes.txt");
        EXPECT_TRUE(window.rename_error().empty());
        EXPECT_TRUE(window.get_model().is_invalidation_deferred());
    }

    /// @brief Verifies that start_rename rejects mount roots, sub-assets, and non-existent paths.
    TEST_F(project_view_window_test, start_rename_mount_root_and_sub_asset_rejected)
    {
        // Setup: Create directory with model file and sub-asset sidecar metadata
        const auto base_dir = fs::temp_directory_path() / "tempest_test_rename_rejections";
        const auto temp_guard = scoped_temp_directory{base_dir};
        const auto mesh_path = base_dir / "cube.mesh";
        ASSERT_TRUE(write_dummy_file(mesh_path));

        auto meta = assets::sidecar_meta{};
        meta.source_guid = guid::generate_random_guid();
        meta.importer = "mesh_importer";
        meta.sub_assets["mesh/cube_sub"] = guid::generate_random_guid();
        meta.sub_asset_types["mesh/cube_sub"] = assets::asset_type_id::of<core::mesh>();
        ASSERT_TRUE(meta.save_to_file((base_dir / "cube.mesh.tmeta").generic_string()));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Assets",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        model.expand_all();

        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        // Act & Assert: Mount root cannot be renamed
        EXPECT_FALSE(window.start_rename("Assets"));
        EXPECT_FALSE(window.is_renaming());

        // Act & Assert: Sub-asset cannot be renamed
        EXPECT_FALSE(window.start_rename("mesh/cube_sub"));
        EXPECT_FALSE(window.is_renaming());

        // Act & Assert: Non-existent path cannot be renamed
        EXPECT_FALSE(window.start_rename("does_not_exist.txt"));
        EXPECT_FALSE(window.is_renaming());
    }

    /// @brief Verifies rename name validation for emptiness, illegal characters, and sibling collisions.
    TEST_F(project_view_window_test, validate_rename_name_character_and_collision_rules)
    {
        // Setup: Create physical directory structure with multiple siblings
        const auto base_dir = fs::temp_directory_path() / "tempest_test_validate_rules";
        const auto temp_guard = scoped_temp_directory{base_dir};
        ASSERT_TRUE(write_dummy_file(base_dir / "target.txt"));
        ASSERT_TRUE(write_dummy_file(base_dir / "sibling.txt"));
        fs::create_directory(base_dir / "subfolder");

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Project",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        // Setup: Start renaming target.txt
        ASSERT_TRUE(window.start_rename("target.txt"));

        // Act & Assert: Empty and whitespace-only name validation errors
        const auto empty_res = window.validate_rename_name("");
        ASSERT_TRUE(empty_res.has_value());
        EXPECT_EQ(*empty_res, "Name cannot be empty.");

        const auto whitespace_res = window.validate_rename_name("   ");
        ASSERT_TRUE(whitespace_res.has_value());
        EXPECT_EQ(*whitespace_res, "Name cannot be empty.");

        // Act & Assert: Illegal character validation errors
        const auto illegal_chars = string_view{"/\\:*?\"<>|"};
        for (const auto ch : illegal_chars)
        {
            auto bad_name = string{"bad"};
            bad_name.push_back(ch);
            bad_name.append("name.txt");
            const auto illegal_res = window.validate_rename_name(bad_name);
            ASSERT_TRUE(illegal_res.has_value());
            EXPECT_EQ(*illegal_res, "Name contains illegal characters.");
        }

        // Act & Assert: Sibling file collision
        const auto file_coll_res = window.validate_rename_name("sibling.txt");
        ASSERT_TRUE(file_coll_res.has_value());
        EXPECT_EQ(*file_coll_res, "An item with this name already exists.");

        // Act & Assert: Sibling folder collision
        const auto folder_coll_res = window.validate_rename_name("subfolder");
        ASSERT_TRUE(folder_coll_res.has_value());
        EXPECT_EQ(*folder_coll_res, "An item with this name already exists.");

        // Act & Assert: Same name (unchanged) is valid
        const auto same_res = window.validate_rename_name("target.txt");
        EXPECT_FALSE(same_res.has_value());

        // Act & Assert: New non-colliding name is valid
        const auto valid_res = window.validate_rename_name("unique_new_target.txt");
        EXPECT_FALSE(valid_res.has_value());
    }

    /// @brief Verifies that cancel_rename resets rename state and restores model invalidation.
    TEST_F(project_view_window_test, cancel_rename_resets_state_and_restores_deferral)
    {
        // Setup: Create directory with file and start rename
        const auto base_dir = fs::temp_directory_path() / "tempest_test_cancel_rename";
        const auto temp_guard = scoped_temp_directory{base_dir};
        ASSERT_TRUE(write_dummy_file(base_dir / "document.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Docs",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        ASSERT_TRUE(window.start_rename("document.txt"));
        window.set_rename_buffer("changed_draft.txt");
        ASSERT_TRUE(window.is_renaming());
        ASSERT_TRUE(window.get_model().is_invalidation_deferred());

        // Act: Cancel renaming
        window.cancel_rename();

        // Assert: State is reset and invalidation deferral restored
        EXPECT_FALSE(window.is_renaming());
        EXPECT_TRUE(window.renaming_path().empty());
        EXPECT_TRUE(window.rename_buffer().empty());
        EXPECT_TRUE(window.rename_error().empty());
        EXPECT_FALSE(window.get_model().is_invalidation_deferred());
    }

    /// @brief Verifies that commit_rename renames disk file, .tmeta sidecar, and updates model and selection.
    TEST_F(project_view_window_test, commit_rename_renames_disk_files_and_sidecar_and_updates_model_and_selection)
    {
        // Setup: Create physical directory structure with file and sidecar
        const auto base_dir = fs::temp_directory_path() / "tempest_test_commit_rename";
        const auto temp_guard = scoped_temp_directory{base_dir};
        const auto old_file_path = base_dir / "texture.png";
        ASSERT_TRUE(write_dummy_file(old_file_path));

        auto meta = assets::sidecar_meta{};
        meta.source_guid = guid::generate_random_guid();
        meta.importer = "texture_importer";
        ASSERT_TRUE(meta.save_to_file((base_dir / "texture.png.tmeta").generic_string()));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Textures",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        // Setup: Select file and start rename
        window.set_selected_path("texture.png");
        EXPECT_EQ(window.selected_path(), "texture.png");

        ASSERT_TRUE(window.start_rename("texture.png"));
        window.set_rename_buffer("albedo_diffuse.png");

        // Act: Commit rename
        const auto commit_result = window.commit_rename();

        // Assert: Rename committed cleanly
        EXPECT_TRUE(commit_result);
        EXPECT_FALSE(window.is_renaming());
        EXPECT_FALSE(window.get_model().is_invalidation_deferred());

        // Assert: Disk files and sidecars renamed
        EXPECT_FALSE(fs::exists(base_dir / "texture.png"));
        EXPECT_FALSE(fs::exists(base_dir / "texture.png.tmeta"));
        EXPECT_TRUE(fs::exists(base_dir / "albedo_diffuse.png"));
        EXPECT_TRUE(fs::exists(base_dir / "albedo_diffuse.png.tmeta"));

        // Assert: Backing model reflects new path and removes old path
        EXPECT_EQ(window.get_model().find_node_by_path("texture.png"), nullptr);
        EXPECT_NE(window.get_model().find_node_by_path("albedo_diffuse.png"), nullptr);

        // Assert: Active selection followed the renamed path
        EXPECT_EQ(window.selected_path(), "albedo_diffuse.png");
    }

    /// @brief Verifies that commit_rename with unchanged name succeeds and preserves disk files.
    TEST_F(project_view_window_test, commit_rename_with_same_name_cancels_and_succeeds)
    {
        // Setup: Create file and model
        const auto base_dir = fs::temp_directory_path() / "tempest_test_commit_same_name";
        const auto temp_guard = scoped_temp_directory{base_dir};
        ASSERT_TRUE(write_dummy_file(base_dir / "unchanged.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Root",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        ASSERT_TRUE(window.start_rename("unchanged.txt"));
        EXPECT_EQ(window.rename_buffer(), "unchanged.txt");

        // Act: Commit rename without changing buffer
        const auto commit_res = window.commit_rename();

        // Assert: Returns true and resets rename mode
        EXPECT_TRUE(commit_res);
        EXPECT_FALSE(window.is_renaming());
        EXPECT_TRUE(fs::exists(base_dir / "unchanged.txt"));
    }

    /// @brief Verifies that commit_rename fails and sets error when validation fails on collision.
    TEST_F(project_view_window_test, commit_rename_with_validation_error_fails_and_records_error)
    {
        // Setup: Create two sibling files
        const auto base_dir = fs::temp_directory_path() / "tempest_test_commit_error";
        const auto temp_guard = scoped_temp_directory{base_dir};
        ASSERT_TRUE(write_dummy_file(base_dir / "alpha.txt"));
        ASSERT_TRUE(write_dummy_file(base_dir / "beta.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Letters",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        ASSERT_TRUE(window.start_rename("alpha.txt"));
        window.set_rename_buffer("beta.txt"); // Conflicting name

        // Act: Commit rename with conflict
        const auto commit_res = window.commit_rename();

        // Assert: Fails, remains in rename mode, records error
        EXPECT_FALSE(commit_res);
        EXPECT_TRUE(window.is_renaming());
        EXPECT_EQ(window.rename_error(), "An item with this name already exists.");
        EXPECT_TRUE(fs::exists(base_dir / "alpha.txt"));
        EXPECT_TRUE(fs::exists(base_dir / "beta.txt"));
    }

    /// @brief Verifies headless ImGui draw execution during active rename with Escape cancellation.
    TEST_F(project_view_window_test, headless_imgui_inline_rename_escape_cancels)
    {
        // Setup: Engine context, UI context, and window with test file
        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Rename Escape Test Window",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

        const auto base_dir = fs::temp_directory_path() / "tempest_test_ui_rename_escape";
        const auto temp_guard = scoped_temp_directory{base_dir};
        ASSERT_TRUE(write_dummy_file(base_dir / "edit_file.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Workspace",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        window.set_selected_path("edit_file.txt");
        ASSERT_TRUE(window.start_rename("edit_file.txt"));
        ASSERT_TRUE(window.is_renaming());

        // Act: Render initial frame showing inline rename input widget
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();
        EXPECT_TRUE(window.is_renaming());

        // Act: Simulate Escape key press in next frame
        auto& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiKey_Escape, true);
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();
        io.AddKeyEvent(ImGuiKey_Escape, false);

        // Assert: Rename is cancelled and file unchanged
        EXPECT_FALSE(window.is_renaming());
        EXPECT_TRUE(fs::exists(base_dir / "edit_file.txt"));
    }

    /// @brief Verifies headless ImGui draw execution with F2 trigger and Enter commit.
    TEST_F(project_view_window_test, headless_imgui_inline_rename_f2_shortcut_and_enter_commits)
    {
        // Setup: Engine context, UI context, and window with test file
        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Rename F2 and Enter Test Window",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

        const auto base_dir = fs::temp_directory_path() / "tempest_test_ui_rename_f2_enter";
        const auto temp_guard = scoped_temp_directory{base_dir};
        ASSERT_TRUE(write_dummy_file(base_dir / "starting_name.txt"));

        auto mounts = vector<assets::mount_point>{};
        mounts.push_back(assets::mount_point{
            .path = base_dir.generic_string(),
            .alias = "Workspace",
        });

        auto model = project_tree_model{};
        model.rebuild_from_mounts(mounts);
        auto window = project_view_window{tempest::move(model), scene_mgr, selection};

        window.set_selected_path("starting_name.txt");

        // Act: Simulate F2 shortcut to initiate rename
        auto& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiKey_F2, true);
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();
        io.AddKeyEvent(ImGuiKey_F2, false);

        // Assert: Inline rename started via F2
        EXPECT_TRUE(window.is_renaming());
        EXPECT_EQ(window.rename_buffer(), "starting_name.txt");

        // Act: Update rename buffer with new name and settle focus
        window.set_rename_buffer("completed_rename.txt");
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();

        // Act: Press Enter to commit rename
        io.AddKeyEvent(ImGuiKey_Enter, true);
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();
        io.AddKeyEvent(ImGuiKey_Enter, false);

        // Assert: Rename committed and disk reflects update
        EXPECT_FALSE(window.is_renaming());
        EXPECT_TRUE(fs::exists(base_dir / "completed_rename.txt"));
        EXPECT_FALSE(fs::exists(base_dir / "starting_name.txt"));
        EXPECT_EQ(window.selected_path(), "completed_rename.txt");
    }
} // namespace tempest::editor::tests
