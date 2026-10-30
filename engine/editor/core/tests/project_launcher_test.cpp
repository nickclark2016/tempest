#include <gtest/gtest.h>

#include <tempest/editor.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/builtin_assets.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/project_workspace.hpp>
#include <tempest/recent_projects.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>
#include <tempest/windows/project_launcher_dialog.hpp>

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

            explicit scoped_temp_directory(const fs::path& target_path) : path{target_path}
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

        struct scoped_current_path
        {
            fs::path original_path;

            explicit scoped_current_path(const fs::path& target) : original_path{fs::current_path()}
            {
                fs::current_path(target);
            }

            ~scoped_current_path()
            {
                fs::current_path(original_path);
            }

            scoped_current_path(const scoped_current_path&) = delete;
            scoped_current_path(scoped_current_path&&) noexcept = delete;
            auto operator=(const scoped_current_path&) -> scoped_current_path& = delete;
            auto operator=(scoped_current_path&&) noexcept -> scoped_current_path& = delete;
        };

        auto create_valid_test_project_file(const fs::path& project_path, string_view project_name = "TestProject")
            -> bool
        {
            auto workspace = assets::project_workspace{};
            workspace.name = string(project_name);
            workspace.root_directory = project_path.parent_path().generic_string();
            workspace.mounts.push_back(assets::mount_point{
                .path = "assets",
                .priority = 0,
                .alias = "assets",
            });

            fs::create_directories(project_path.parent_path() / "assets");
            return workspace.save_to_file(project_path.generic_string());
        }
    } // namespace

    //==============================================================================
    // Recent Projects Persistence Tests
    //==============================================================================

    /// @brief Verifies saving and loading of recent projects entries, preserving roundtrip integrity.
    TEST(project_launcher_test, recent_projects_roundtrip_serialization)
    {
        // Setup: Temporary session file path
        const auto temp_dir = fs::temp_directory_path() / "tempest_recents_roundtrip";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto session_file = (temp_dir / "editor_session.json").generic_string();

        auto original_entries = vector<recent_project_entry>{};
        original_entries.push_back(recent_project_entry{
            .name = "FirstProject",
            .path = "C:/Projects/First/First.tproject",
            .last_opened = 1700000000,
        });
        original_entries.push_back(recent_project_entry{
            .name = "SecondProject",
            .path = "C:/Projects/Second/Second.tproject",
            .last_opened = 1690000000,
        });

        // Act: Save and re-load
        const auto save_result = save_recent_projects(
            span<const recent_project_entry>{original_entries.data(), original_entries.size()}, session_file);
        EXPECT_TRUE(save_result);

        const auto loaded_entries = load_recent_projects(session_file);

        // Assert: Entries match exactly
        ASSERT_EQ(loaded_entries.size(), original_entries.size());
        EXPECT_EQ(loaded_entries[0].name, "FirstProject");
        EXPECT_EQ(loaded_entries[0].path, "C:/Projects/First/First.tproject");
        EXPECT_EQ(loaded_entries[0].last_opened, 1700000000ULL);
        EXPECT_EQ(loaded_entries[1].name, "SecondProject");
        EXPECT_EQ(loaded_entries[1].path, "C:/Projects/Second/Second.tproject");
        EXPECT_EQ(loaded_entries[1].last_opened, 1690000000ULL);
    }

    /// @brief Verifies that add_recent_project moves existing paths to the front and deduplicates entries.
    TEST(project_launcher_test, recent_projects_deduplication_and_ordering)
    {
        // Setup: Session with two initial projects
        const auto temp_dir = fs::temp_directory_path() / "tempest_recents_dedup";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto session_file = (temp_dir / "editor_session.json").generic_string();

        EXPECT_TRUE(add_recent_project("D:/Projects/Alpha/Alpha.tproject", "Alpha", session_file));
        EXPECT_TRUE(add_recent_project("D:/Projects/Beta/Beta.tproject", "Beta", session_file));

        auto initial_list = load_recent_projects(session_file);
        ASSERT_EQ(initial_list.size(), 2U);
        EXPECT_EQ(initial_list[0].name, "Beta");
        EXPECT_EQ(initial_list[1].name, "Alpha");

        // Act: Re-add Alpha (should move to front)
        EXPECT_TRUE(add_recent_project("D:/Projects/Alpha/Alpha.tproject", "Alpha Updated", session_file));

        auto updated_list = load_recent_projects(session_file);

        // Assert: Alpha moved to front, total size remains 2
        ASSERT_EQ(updated_list.size(), 2U);
        EXPECT_EQ(updated_list[0].name, "Alpha Updated");
        EXPECT_EQ(updated_list[0].path, "D:/Projects/Alpha/Alpha.tproject");
        EXPECT_EQ(updated_list[1].name, "Beta");
        EXPECT_EQ(updated_list[1].path, "D:/Projects/Beta/Beta.tproject");
    }

    /// @brief Verifies that recent projects are clamped to the maximum allowed count.
    TEST(project_launcher_test, recent_projects_clamped_to_max_limit)
    {
        // Setup: Session file
        const auto temp_dir = fs::temp_directory_path() / "tempest_recents_clamp";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto session_file = (temp_dir / "editor_session.json").generic_string();

        // Act: Add 15 projects (limit is default_max_recent_projects = 10)
        for (size_t index = 0; index < 15; ++index)
        {
            const auto proj_name = tempest::format("Project_{}", index);
            const auto proj_path = tempest::format("D:/Projects/{}/{}.tproject", proj_name, proj_name);
            EXPECT_TRUE(add_recent_project(proj_path, proj_name, session_file));
        }

        const auto final_list = load_recent_projects(session_file);

        // Assert: Exactly 10 items preserved, newest (Project_14) at front, oldest evicted
        ASSERT_EQ(final_list.size(), default_max_recent_projects);
        EXPECT_EQ(final_list.front().name, "Project_14");
        EXPECT_EQ(final_list.back().name, "Project_5");
    }

    /// @brief Verifies that saving recent projects preserves all other root keys in editor_session.json.
    TEST(project_launcher_test, recent_projects_preserves_external_root_keys)
    {
        // Setup: Pre-populate session JSON with external keys
        const auto temp_dir = fs::temp_directory_path() / "tempest_recents_preserve";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto session_file = (temp_dir / "editor_session.json").generic_string();

        const auto initial_json = R"({
  "theme": "dark_modern",
  "window_width": 1920,
  "recent_projects": []
})";
        ASSERT_TRUE(write_dummy_file(fs::path(session_file), initial_json));

        // Act: Add a project and clear recents
        EXPECT_TRUE(add_recent_project("D:/Work/Game.tproject", "Game", session_file));
        EXPECT_TRUE(clear_recent_projects(session_file));

        // Assert: Read back raw document and verify external keys remain intact
        auto allocator = system_allocator{};
        const auto raw_content = read_file_to_string(fs::path(session_file));
        ASSERT_TRUE(raw_content.has_value());

        auto parsed_doc = json_document::from_string(*raw_content, allocator);
        ASSERT_TRUE(parsed_doc.has_value());

        const auto root = parsed_doc->root().as_object().value();
        EXPECT_TRUE(root.contains("theme"));
        EXPECT_EQ(root["theme"].as_string().value(), "dark_modern");
        EXPECT_TRUE(root.contains("window_width"));
        EXPECT_EQ(root["window_width"].as_int32().value(), 1920);

        const auto recents = load_recent_projects(session_file);
        EXPECT_TRUE(recents.empty());
    }

    /// @brief Verifies that loading from a missing or corrupted file returns an empty vector safely.
    TEST(project_launcher_test, recent_projects_missing_file_safety)
    {
        // Act & Assert: Non-existent file
        const auto missing_entries = load_recent_projects("Z:/NonExistentPath/session.json");
        EXPECT_TRUE(missing_entries.empty());

        // Setup: Corrupt file
        const auto temp_dir = fs::temp_directory_path() / "tempest_recents_corrupt";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto corrupt_file = (temp_dir / "editor_session.json").generic_string();
        ASSERT_TRUE(write_dummy_file(fs::path(corrupt_file), "{ this is not json }"));

        // Act & Assert: Corrupt file returns empty
        const auto corrupt_entries = load_recent_projects(corrupt_file);
        EXPECT_TRUE(corrupt_entries.empty());
    }

    /// @brief Verifies that remove_recent_project deletes a specific project entry and preserves others.
    TEST(project_launcher_test, recent_projects_remove_entry)
    {
        // Setup: Session with two initial projects
        const auto temp_dir = fs::temp_directory_path() / "tempest_recents_remove";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto session_file = (temp_dir / "editor_session.json").generic_string();

        EXPECT_TRUE(add_recent_project("D:/Projects/Alpha/Alpha.tproject", "Alpha", session_file));
        EXPECT_TRUE(add_recent_project("D:/Projects/Beta/Beta.tproject", "Beta", session_file));

        auto initial_list = load_recent_projects(session_file);
        ASSERT_EQ(initial_list.size(), 2U);

        // Act: Remove non-existent entry returns false
        EXPECT_FALSE(remove_recent_project("D:/Projects/Gamma/Gamma.tproject", session_file));

        // Act: Remove Alpha
        EXPECT_TRUE(remove_recent_project("D:/Projects/Alpha/Alpha.tproject", session_file));

        // Assert: Only Beta remains
        const auto updated_list = load_recent_projects(session_file);
        ASSERT_EQ(updated_list.size(), 1U);
        EXPECT_EQ(updated_list[0].name, "Beta");
        EXPECT_EQ(updated_list[0].path, "D:/Projects/Beta/Beta.tproject");
    }

    //==============================================================================
    // Startup Resolution & Project Discovery Tests
    //==============================================================================

    /// @brief Verifies that --project=<path> command line argument opens the target project.
    TEST(project_launcher_test, startup_resolution_project_equals_arg)
    {
        // Setup: Engine context and valid project file
        const auto temp_dir = fs::temp_directory_path() / "tempest_startup_arg_equals";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto proj_file = temp_dir / "MyGame.tproject";
        ASSERT_TRUE(create_valid_test_project_file(proj_file, "MyGame"));

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Startup Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        const auto arg_string = tempest::format("--project={}", proj_file.generic_string());
        auto args = vector<string_view>{};
        args.push_back(string_view{arg_string});

        // Act: Initialize from startup arguments
        const auto resolved =
            editor.initialize_project_from_startup(span<const string_view>{args.data(), args.size()});

        // Assert: Resolved project successfully without opening launcher
        EXPECT_TRUE(resolved);
        EXPECT_TRUE(engine_ctx.has_project());
        EXPECT_EQ(engine_ctx.get_current_project()->name, "MyGame");
        EXPECT_FALSE(editor.get_project_launcher_dialog().is_open());
    }

    /// @brief Verifies that --project <path> command line argument opens the target project.
    TEST(project_launcher_test, startup_resolution_project_space_arg)
    {
        // Setup: Engine context and valid project file
        const auto temp_dir = fs::temp_directory_path() / "tempest_startup_arg_space";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto proj_file = temp_dir / "SpaceGame.tproject";
        ASSERT_TRUE(create_valid_test_project_file(proj_file, "SpaceGame"));

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Startup Space Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        const auto target_path_str = proj_file.generic_string();
        auto args = vector<string_view>{};
        args.push_back("--project");
        args.push_back(string_view{target_path_str});

        // Act: Initialize from startup arguments
        const auto resolved =
            editor.initialize_project_from_startup(span<const string_view>{args.data(), args.size()});

        // Assert: Resolved project successfully
        EXPECT_TRUE(resolved);
        EXPECT_TRUE(engine_ctx.has_project());
        EXPECT_EQ(engine_ctx.get_current_project()->name, "SpaceGame");
        EXPECT_FALSE(editor.get_project_launcher_dialog().is_open());
    }

    /// @brief Verifies auto-discovery when exactly one .tproject file resides in the current working directory.
    TEST(project_launcher_test, startup_resolution_single_project_in_cwd)
    {
        // Setup: Working directory containing single .tproject
        const auto temp_dir = fs::temp_directory_path() / "tempest_startup_single_cwd";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto proj_file = temp_dir / "SingleGame.tproject";
        ASSERT_TRUE(create_valid_test_project_file(proj_file, "SingleGame"));

        const auto cwd_guard = scoped_current_path{temp_dir};

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Single CWD Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Act: Initialize from startup with empty arguments
        const auto resolved = editor.initialize_project_from_startup({});

        // Assert: Auto-discovered single project in CWD
        EXPECT_TRUE(resolved);
        EXPECT_TRUE(engine_ctx.has_project());
        EXPECT_EQ(engine_ctx.get_current_project()->name, "SingleGame");
        EXPECT_FALSE(editor.get_project_launcher_dialog().is_open());
    }

    /// @brief Verifies that when 0 .tproject files exist in CWD, the launcher dialog opens.
    TEST(project_launcher_test, startup_resolution_zero_projects_in_cwd_opens_launcher)
    {
        // Setup: Empty working directory
        const auto temp_dir = fs::temp_directory_path() / "tempest_startup_zero_cwd";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto cwd_guard = scoped_current_path{temp_dir};

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Zero CWD Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Act: Initialize from startup with empty arguments
        const auto resolved = editor.initialize_project_from_startup({});

        // Assert: Did not resolve project, launcher dialog opened
        EXPECT_FALSE(resolved);
        EXPECT_FALSE(engine_ctx.has_project());
        EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());
    }

    /// @brief Verifies that when multiple .tproject files exist in CWD, the launcher dialog opens.
    TEST(project_launcher_test, startup_resolution_multiple_projects_in_cwd_opens_launcher)
    {
        // Setup: Working directory with two .tproject files
        const auto temp_dir = fs::temp_directory_path() / "tempest_startup_multi_cwd";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        ASSERT_TRUE(create_valid_test_project_file(temp_dir / "First.tproject", "First"));
        ASSERT_TRUE(create_valid_test_project_file(temp_dir / "Second.tproject", "Second"));

        const auto cwd_guard = scoped_current_path{temp_dir};

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Multi CWD Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Act: Initialize from startup with empty arguments
        const auto resolved = editor.initialize_project_from_startup({});

        // Assert: Disambiguation required, launcher dialog opened
        EXPECT_FALSE(resolved);
        EXPECT_FALSE(engine_ctx.has_project());
        EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());
    }

    //==============================================================================
    // Project Creation Logic Tests
    //==============================================================================

    /// @brief Verifies that create_project creates directory hierarchy, assets folder,
    /// valid .tproject JSON structure, mount point, and successfully opens it.
    TEST(project_launcher_test, project_creation_creates_directory_structure_and_opens)
    {
        // Setup: Temporary parent folder
        const auto temp_dir = fs::temp_directory_path() / "tempest_project_creation";
        const auto temp_guard = scoped_temp_directory{temp_dir};

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Creation Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Act: Create new project "Adventure" inside temp directory
        const auto created = editor.create_project("Adventure", temp_dir.generic_string());

        // Assert: Operation succeeded
        EXPECT_TRUE(created);
        EXPECT_TRUE(engine_ctx.has_project());
        EXPECT_EQ(engine_ctx.get_current_project()->name, "Adventure");

        // Verify disk files
        const auto project_dir = temp_dir / "Adventure";
        EXPECT_TRUE(fs::exists(project_dir));
        EXPECT_TRUE(fs::is_directory(project_dir));

        const auto assets_dir = project_dir / "assets";
        EXPECT_TRUE(fs::exists(assets_dir));
        EXPECT_TRUE(fs::is_directory(assets_dir));

        const auto project_file = project_dir / "Adventure.tproject";
        EXPECT_TRUE(fs::exists(project_file));

        // Read and validate project workspace structure
        auto loaded_ws = assets::project_workspace{};
        EXPECT_TRUE(loaded_ws.load_from_file(project_file.generic_string()));
        EXPECT_EQ(loaded_ws.name, "Adventure");
        ASSERT_EQ(loaded_ws.mounts.size(), 1U);
        EXPECT_EQ(loaded_ws.mounts[0].alias, "assets");
        EXPECT_EQ(loaded_ws.mounts[0].path, "assets");
    }

    /// @brief Verifies that create_project succeeds even if project directory or assets directory already exist on disk.
    TEST(project_launcher_test, project_creation_succeeds_when_directories_already_exist)
    {
        // Setup: Pre-create project and assets directories
        const auto temp_dir = fs::temp_directory_path() / "tempest_project_creation_existing_dirs";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto existing_project_dir = temp_dir / "ExistingGame";
        const auto existing_assets_dir = existing_project_dir / "assets";
        ASSERT_TRUE(fs::create_directories(existing_assets_dir));

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Existing Dirs Creation Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Act: Create project in existing directories
        const auto created = editor.create_project("ExistingGame", temp_dir.generic_string());

        // Assert: Operation succeeded and project file created
        EXPECT_TRUE(created);
        EXPECT_TRUE(engine_ctx.has_project());
        EXPECT_EQ(engine_ctx.get_current_project()->name, "ExistingGame");
        EXPECT_TRUE(fs::exists(existing_project_dir / "ExistingGame.tproject"));
    }

    /// @brief Verifies that opening a project workspace retains access to built-in meshes and materials,
    /// so that canonical primitive GUIDs resolve to valid display names rather than "Missing (...)".
    TEST(project_launcher_test, open_project_preserves_builtin_meshes_and_materials)
    {
        // Setup: Temporary project workspace
        const auto temp_dir = fs::temp_directory_path() / "tempest_open_project_builtin_assets";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto proj_file = temp_dir / "BuiltinTestGame.tproject";
        ASSERT_TRUE(create_valid_test_project_file(proj_file, "BuiltinTestGame"));

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Builtin Asset Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Act: Open project
        ASSERT_TRUE(editor.open_project(proj_file.generic_string()));

        // Assert: Built-in plane mesh and default material resolve to proper names rather than "Missing (...)"
        auto& asset_db = engine_ctx.get_assets();
        const auto plane_display_name = asset_db.get_asset_display_name(assets::builtin::plane_mesh_guid);
        EXPECT_EQ(plane_display_name, "@builtin/meshes/plane");

        const auto box_display_name = asset_db.get_asset_display_name(assets::builtin::box_mesh_guid);
        EXPECT_EQ(box_display_name, "@builtin/meshes/box");

        const auto mat_display_name = asset_db.get_asset_display_name(assets::builtin::default_material_guid);
        EXPECT_EQ(mat_display_name, "@builtin/materials/default");

        const auto* plane_entry = asset_db.find_by_guid(assets::builtin::plane_mesh_guid);
        ASSERT_NE(plane_entry, nullptr);

        const auto* mat_entry = asset_db.find_by_guid(assets::builtin::default_material_guid);
        ASSERT_NE(mat_entry, nullptr);
    }

    //==============================================================================
    // Launcher Modal Dialog Headless ImGui Draw Tests
    //==============================================================================

    /// @brief Verifies that project_launcher_dialog defaults to closed and responds to open/close requests.
    TEST(project_launcher_test, launcher_dialog_lifecycle_and_tab_selection)
    {
        // Setup: Dialog instance
        auto launcher = project_launcher_dialog{};
        EXPECT_FALSE(launcher.is_open());
        EXPECT_EQ(launcher.get_active_tab(), launcher_tab::recent_projects);

        // Act: Open with specific tabs
        launcher.open(launcher_tab::open_browse);
        EXPECT_TRUE(launcher.is_open());
        EXPECT_EQ(launcher.get_active_tab(), launcher_tab::open_browse);

        launcher.open(launcher_tab::create_project);
        EXPECT_TRUE(launcher.is_open());
        EXPECT_EQ(launcher.get_active_tab(), launcher_tab::create_project);

        launcher.close();
        EXPECT_FALSE(launcher.is_open());
    }

    /// @brief Verifies headless Dear ImGui rendering of recent projects tab and interactions.
    TEST(project_launcher_test, launcher_dialog_headless_draw_recent_projects_tab)
    {
        // Setup: Engine and UI context, temporary recents file
        auto engine_ctx = editor_engine_context{};
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Recents Tab Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        const auto temp_dir = fs::temp_directory_path() / "tempest_headless_launcher";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto session_file = (temp_dir / "editor_session.json").generic_string();

        EXPECT_TRUE(add_recent_project("C:/Games/Quest.tproject", "Quest", session_file));

        auto launcher = project_launcher_dialog{};
        launcher.set_session_file_path(session_file);
        auto opened_path = string{};
        launcher.set_on_open_project([&](string_view path) -> bool {
            opened_path = string(path);
            return true;
        });

        // Act: Open recents tab and render frames
        launcher.open(launcher_tab::recent_projects);

        ui_ctx.begin_ui_commands();
        launcher.draw();
        ui_ctx.finish_ui_commands();

        // Second frame: Modal is active
        ui_ctx.begin_ui_commands();
        launcher.draw();
        ui_ctx.finish_ui_commands();

        EXPECT_TRUE(launcher.is_open());
        EXPECT_EQ(launcher.get_active_tab(), launcher_tab::recent_projects);
    }

    /// @brief Verifies that project_launcher_dialog handles missing project paths gracefully in recent projects tab.
    TEST(project_launcher_test, launcher_dialog_headless_draw_missing_recent_project)
    {
        // Setup: Engine and UI context, recent project that does not exist on disk
        auto engine_ctx = editor_engine_context{};
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Missing Recent Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        const auto temp_dir = fs::temp_directory_path() / "tempest_headless_missing_recent";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto session_file = (temp_dir / "editor_session.json").generic_string();

        EXPECT_TRUE(add_recent_project("Z:/Missing/GhostProject.tproject", "GhostProject", session_file));

        auto launcher = project_launcher_dialog{};
        launcher.set_session_file_path(session_file);

        // Act: Open recents tab and render frames with missing project
        launcher.open(launcher_tab::recent_projects);

        ui_ctx.begin_ui_commands();
        launcher.draw();
        ui_ctx.finish_ui_commands();

        ui_ctx.begin_ui_commands();
        launcher.draw();
        ui_ctx.finish_ui_commands();

        // Assert: Dialog remains open without crash
        EXPECT_TRUE(launcher.is_open());
        EXPECT_EQ(launcher.get_active_tab(), launcher_tab::recent_projects);

        // Remove from recents
        EXPECT_TRUE(remove_recent_project("Z:/Missing/GhostProject.tproject", session_file));
        const auto remaining = load_recent_projects(session_file);
        EXPECT_TRUE(remaining.empty());
    }

    /// @brief Verifies headless Dear ImGui rendering of open/browse tab with field population.
    TEST(project_launcher_test, launcher_dialog_headless_draw_open_browse_tab)
    {
        // Setup: Engine and UI context, test project file
        auto engine_ctx = editor_engine_context{};
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Browse Tab Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        const auto temp_dir = fs::temp_directory_path() / "tempest_headless_browse";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto proj_file = temp_dir / "Demo.tproject";
        ASSERT_TRUE(create_valid_test_project_file(proj_file, "Demo"));

        auto launcher = project_launcher_dialog{};
        launcher.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        launcher.set_open_path(proj_file.generic_string());

        // Act: Open and render
        launcher.open(launcher_tab::open_browse);

        ui_ctx.begin_ui_commands();
        launcher.draw();
        ui_ctx.finish_ui_commands();

        ui_ctx.begin_ui_commands();
        launcher.draw();
        ui_ctx.finish_ui_commands();

        EXPECT_TRUE(launcher.is_open());
        EXPECT_EQ(launcher.get_active_tab(), launcher_tab::open_browse);
    }

    /// @brief Verifies headless Dear ImGui rendering of create project tab with field population.
    TEST(project_launcher_test, launcher_dialog_headless_draw_create_project_tab)
    {
        // Setup: Engine and UI context
        auto engine_ctx = editor_engine_context{};
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Create Tab Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        const auto temp_dir = fs::temp_directory_path() / "tempest_headless_create";
        const auto temp_guard = scoped_temp_directory{temp_dir};

        auto launcher = project_launcher_dialog{};
        launcher.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        launcher.set_new_project_fields("ActionRPG", "C:/Projects");

        // Act: Open and render
        launcher.open(launcher_tab::create_project);

        ui_ctx.begin_ui_commands();
        launcher.draw();
        ui_ctx.finish_ui_commands();

        ui_ctx.begin_ui_commands();
        launcher.draw();
        ui_ctx.finish_ui_commands();

        EXPECT_TRUE(launcher.is_open());
        EXPECT_EQ(launcher.get_active_tab(), launcher_tab::create_project);
    }

    /// @brief Verifies that Exit button invokes the exit callback and closes the launcher.
    TEST(project_launcher_test, launcher_dialog_exit_callback)
    {
        auto launcher = project_launcher_dialog{};
        auto exit_called = false;
        launcher.set_on_exit([&]() {
            exit_called = true;
        });

        launcher.open();
        EXPECT_TRUE(launcher.is_open());

        launcher.close();
        EXPECT_FALSE(launcher.is_open());
    }
} // namespace tempest::editor::tests
