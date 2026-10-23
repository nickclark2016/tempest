#include <tempest/windows/project_view_window.hpp>

#include <tempest/asset_database.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/editor.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/material.hpp>
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

    // ============================================================================
    // Window Properties & Lifecycle Tests
    // ============================================================================

    /// @brief Verifies that project_view_window provides the expected initial dock location.
    TEST(project_view_window_test, desired_initial_dock_is_bottom)
    {
        // Setup: Construct default project_view_window
        const auto window = project_view_window{};

        // Act & Assert: Window requests bottom dock region
        EXPECT_EQ(window.desired_initial_dock(), editor_window::dock_location::bottom);
    }

    /// @brief Verifies that project_view_window exposes the designated window name "Project".
    TEST(project_view_window_test, window_name_is_project)
    {
        // Setup: Construct default project_view_window
        const auto window = project_view_window{};

        // Act & Assert: Window name is exactly "Project"
        EXPECT_EQ(window.window_name(), "Project");
    }

    /// @brief Verifies that project_view_window supports all editor simulation modes (edit, paused, playing).
    TEST(project_view_window_test, simulation_modes_supported)
    {
        // Setup: Construct default project_view_window
        const auto window = project_view_window{};

        // Act & Assert: All simulation modes return true
        EXPECT_TRUE(window.is_mode_supported(simulation_state::edit));
        EXPECT_TRUE(window.is_mode_supported(simulation_state::paused));
        EXPECT_TRUE(window.is_mode_supported(simulation_state::playing));
    }

    /// @brief Verifies initial open state and toggling behavior.
    TEST(project_view_window_test, open_state_and_toggling)
    {
        // Setup: Construct default project_view_window
        auto window = project_view_window{};

        // Assert: Window is open by default
        EXPECT_TRUE(window.open);

        // Act: Toggle open off
        window.open = false;

        // Assert: Window is closed
        EXPECT_FALSE(window.open);

        // Act: Toggle open back on
        window.open = true;

        // Assert: Window is open
        EXPECT_TRUE(window.open);
    }

    // ============================================================================
    // Model Access & Replacement Tests
    // ============================================================================

    /// @brief Verifies mutable and const access to the backing project_tree_model, as well as replacing it.
    TEST(project_view_window_test, model_access_and_mutation)
    {
        // Setup: Construct project_view_window
        auto window = project_view_window{};

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
    TEST(project_view_window_test, construct_from_model)
    {
        // Setup: Create a model with dirty flag set
        auto source_model = project_tree_model{};
        source_model.mark_dirty();

        // Act: Construct window from model rvalue
        const auto window = project_view_window{tempest::move(source_model)};

        // Assert: Window retains the provided model's state
        EXPECT_TRUE(window.get_model().is_dirty());
    }

    // ============================================================================
    // Search Bar & Query Management Tests
    // ============================================================================

    /// @brief Verifies that setting search query updates both the buffer and the backing model.
    TEST(project_view_window_test, search_buffer_input_updates_model_query)
    {
        // Setup: Construct default project_view_window
        auto window = project_view_window{};

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
    TEST(project_view_window_test, clear_search_resets_buffer_and_model_query)
    {
        // Setup: Construct window with active search filter
        auto window = project_view_window{};
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
    TEST(project_view_window_test, badge_resolution_across_kinds_and_extensions)
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
    TEST(project_view_window_test, split_breadcrumb_path_parsing)
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
    TEST(project_view_window_test, navigate_to_directory_expands_ancestors)
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
        auto window = project_view_window{tempest::move(model)};

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
        EXPECT_EQ(window.current_directory, "Assets/models/characters");

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
        EXPECT_EQ(window.current_directory, "Assets/models");
    }

    // ============================================================================
    // Headless ImGui Draw Tests
    // ============================================================================

    /// @brief Verifies that draw() executes without throwing when the window is open and when closed.
    TEST(project_view_window_test, draw_execution_headless)
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

        auto window = project_view_window{};

        // Act: Render frame with open = true
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();
        EXPECT_TRUE(window.open);

        // Act: Render frame with open = false (early returns without ImGui window)
        window.open = false;
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(window.draw());
        ui_ctx.finish_ui_commands();
        EXPECT_FALSE(window.open);
    }

    /// @brief Verifies virtualized row rendering and breadcrumbs with populated directory hierarchy.
    TEST(project_view_window_test, draw_headless_with_populated_tree)
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

        auto window = project_view_window{tempest::move(model)};

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
    TEST(project_view_window_test, draw_headless_with_search_filtering)
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

        auto window = project_view_window{tempest::move(model)};

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
    // Editor Context Integration Tests
    // ============================================================================

    /// @brief Verifies that editor_context initializes project_view_window and exposes valid accessors.
    TEST(project_view_window_test, editor_context_integration)
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
        EXPECT_TRUE(project_view->open);

        // Assert: const accessor returns the identical instance
        const auto& const_ed_ctx = ed_ctx;
        const auto* const_project_view = const_ed_ctx.get_project_view_window();
        ASSERT_NE(const_project_view, nullptr);
        EXPECT_EQ(const_project_view, project_view);

        // Act: Render frame via editor_context draw
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(ed_ctx.draw());
        ui_ctx.finish_ui_commands();

        // Act: Toggle window visibility off and redraw
        project_view->open = false;
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(ed_ctx.draw());
        ui_ctx.finish_ui_commands();
        EXPECT_FALSE(project_view->open);
    }
} // namespace tempest::editor::tests
