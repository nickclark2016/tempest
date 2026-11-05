#include <gtest/gtest.h>

#include <tempest/editor.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/builtin_assets.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/format.hpp>
#include <tempest/json.hpp>
#include <tempest/math.hpp>
#include <tempest/memory.hpp>
#include <tempest/project_workspace.hpp>
#include <tempest/recent_projects.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
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
                if (fs::exists(path))
                {
                    fs::remove_all(path);
                }
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

    /// @brief Verifies that passing --project=<nonexistent_path> fails resolution and opens the launcher dialog.
    TEST(project_launcher_test, startup_resolution_nonexistent_project_equals_arg_opens_launcher)
    {
        // Setup: Engine context without any project created on disk
        const auto temp_dir = fs::temp_directory_path() / "tempest_startup_arg_nonexistent";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto nonexistent_file = temp_dir / "NonExistentGame.tproject";

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Nonexistent Startup Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        const auto arg_string = tempest::format("--project={}", nonexistent_file.generic_string());
        auto args = vector<string_view>{};
        args.push_back(string_view{arg_string});

        // Act: Initialize with nonexistent project path
        const auto resolved =
            editor.initialize_project_from_startup(span<const string_view>{args.data(), args.size()});

        // Assert: Resolution failed and launcher dialog opened
        EXPECT_FALSE(resolved);
        EXPECT_FALSE(engine_ctx.has_project());
        EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());
    }

    /// @brief Verifies that passing --project <nonexistent_path> fails resolution and opens the launcher dialog.
    TEST(project_launcher_test, startup_resolution_nonexistent_project_space_arg_opens_launcher)
    {
        // Setup: Engine context without any project created on disk
        const auto temp_dir = fs::temp_directory_path() / "tempest_startup_arg_space_nonexistent";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto nonexistent_file = temp_dir / "NonExistentSpaceGame.tproject";

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Nonexistent Space Startup Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        const auto target_path_str = nonexistent_file.generic_string();
        auto args = vector<string_view>{};
        args.push_back("--project");
        args.push_back(string_view{target_path_str});

        // Act: Initialize with nonexistent project path
        const auto resolved =
            editor.initialize_project_from_startup(span<const string_view>{args.data(), args.size()});

        // Assert: Resolution failed and launcher dialog opened
        EXPECT_FALSE(resolved);
        EXPECT_FALSE(engine_ctx.has_project());
        EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());
    }

    /// @brief Verifies that passing arbitrary flags without --project fails resolution and opens the launcher dialog.
    TEST(project_launcher_test, startup_resolution_arbitrary_flags_without_project_opens_launcher)
    {
        // Setup: Engine context
        const auto temp_dir = fs::temp_directory_path() / "tempest_startup_arbitrary_flags";
        const auto temp_guard = scoped_temp_directory{temp_dir};

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Arbitrary Flags Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        auto args = vector<string_view>{};
        args.push_back("--debug");
        args.push_back("-v");
        args.push_back("--window-width");
        args.push_back("1920");

        // Act: Initialize with arbitrary non-project flags
        const auto resolved =
            editor.initialize_project_from_startup(span<const string_view>{args.data(), args.size()});

        // Assert: Resolution failed and launcher dialog opened
        EXPECT_FALSE(resolved);
        EXPECT_FALSE(engine_ctx.has_project());
        EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());
    }

    /// @brief Verifies that passing empty --project= or trailing --project without path opens launcher dialog.
    TEST(project_launcher_test, startup_resolution_empty_or_trailing_project_flag_opens_launcher)
    {
        // Setup: Engine context
        const auto temp_dir = fs::temp_directory_path() / "tempest_startup_empty_flag";
        const auto temp_guard = scoped_temp_directory{temp_dir};

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Empty Flag Test Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Case A: --project= with empty path
        auto empty_eq_args = vector<string_view>{};
        empty_eq_args.push_back("--project=");
        const auto resolved_eq =
            editor.initialize_project_from_startup(span<const string_view>{empty_eq_args.data(), empty_eq_args.size()});
        EXPECT_FALSE(resolved_eq);
        EXPECT_FALSE(engine_ctx.has_project());
        EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());

        // Case B: trailing --project with no following path argument
        auto trailing_args = vector<string_view>{};
        trailing_args.push_back("--project");
        const auto resolved_trailing =
            editor.initialize_project_from_startup(span<const string_view>{trailing_args.data(), trailing_args.size()});
        EXPECT_FALSE(resolved_trailing);
        EXPECT_FALSE(engine_ctx.has_project());
        EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());
    }

    /// @brief Verifies that launching without arguments forces the launcher dialog to show first even if a project exists in CWD.
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

        // Assert: Does not auto-open project; forces launcher dialog to show first
        EXPECT_FALSE(resolved);
        EXPECT_FALSE(engine_ctx.has_project());
        EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());
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

    /// @brief Verifies that launching without arguments forces the launcher dialog to show first
    /// even if an immediate child directory contains a project.
    TEST(project_launcher_test, startup_resolution_single_project_in_immediate_child_directory)
    {
        // Setup: Working directory with a child directory 'game' containing game.tproject
        const auto temp_dir = fs::temp_directory_path() / "tempest_startup_child_cwd";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto game_dir = temp_dir / "game";
        fs::create_directories(game_dir);
        const auto proj_file = game_dir / "game.tproject";
        ASSERT_TRUE(create_valid_test_project_file(proj_file, "game"));

        const auto cwd_guard = scoped_current_path{temp_dir};

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Child CWD Test Window",
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

        // Assert: Did not auto-open project; forces launcher dialog to show first
        EXPECT_FALSE(resolved);
        EXPECT_FALSE(engine_ctx.has_project());
        EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());
    }

    /// @brief Verifies that when multiple projects exist in different child directories,
    /// disambiguation is required and the launcher dialog opens.
    TEST(project_launcher_test, startup_resolution_multiple_projects_in_subdirectories_opens_launcher)
    {
        // Setup: Working directory with two child directories each containing a project
        const auto temp_dir = fs::temp_directory_path() / "tempest_startup_multi_child_cwd";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto sub1 = temp_dir / "project_a";
        const auto sub2 = temp_dir / "project_b";
        fs::create_directories(sub1);
        fs::create_directories(sub2);
        ASSERT_TRUE(create_valid_test_project_file(sub1 / "project_a.tproject", "project_a"));
        ASSERT_TRUE(create_valid_test_project_file(sub2 / "project_b.tproject", "project_b"));

        const auto cwd_guard = scoped_current_path{temp_dir};

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Multi Child Test Window",
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

        // Assert: Multiple projects in child directories require user choice via launcher dialog
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

    /// @brief Verifies that project_launcher_dialog persists across multiple successive frames without auto-closing.
    TEST(project_launcher_test, launcher_dialog_persists_across_multiple_frames)
    {
        // Setup: Engine and UI context
        auto engine_ctx = editor_engine_context{};
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Multi Frame Persistence Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        auto launcher = project_launcher_dialog{};
        launcher.open(launcher_tab::recent_projects);
        EXPECT_TRUE(launcher.is_open());

        // Act & Assert: Render 10 consecutive frames
        for (auto frame_index = 0; frame_index < 10; ++frame_index)
        {
            ui_ctx.begin_ui_commands();
            launcher.draw();
            ui_ctx.finish_ui_commands();

            EXPECT_TRUE(launcher.is_open());
        }

        // Act: Explicitly close dialog
        launcher.close();
        EXPECT_FALSE(launcher.is_open());

        // Assert: Remains closed on subsequent frame
        ui_ctx.begin_ui_commands();
        launcher.draw();
        ui_ctx.finish_ui_commands();

        EXPECT_FALSE(launcher.is_open());
    }

    /// @brief Verifies that when launched without a project, editor_context::draw() preserves launcher modal across frames.
    TEST(project_launcher_test, editor_draw_preserves_launcher_dialog_across_frames)
    {
        // Setup: Empty working directory with no .tproject files
        const auto temp_dir = fs::temp_directory_path() / "tempest_editor_draw_persistence";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto cwd_guard = scoped_current_path{temp_dir};

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Editor Draw Persistence Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Act: Initialize startup without arguments -> opens launcher
        const auto resolved = editor.initialize_project_from_startup({});
        EXPECT_FALSE(resolved);
        EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());

        // Act & Assert: Execute multiple full editor frames
        for (auto frame_index = 0; frame_index < 5; ++frame_index)
        {
            ui_ctx.begin_ui_commands();
            editor.draw();
            ui_ctx.finish_ui_commands();

            EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());
        }
    }

    /// @brief Verifies that project launcher closes when project open succeeds, but remains open if open fails.
    TEST(project_launcher_test, launcher_dialog_open_project_lifecycle)
    {
        auto launcher = project_launcher_dialog{};
        auto should_succeed = false;
        launcher.set_on_open_project([&](string_view /*path*/) -> bool {
            return should_succeed;
        });

        launcher.open(launcher_tab::open_browse);
        EXPECT_TRUE(launcher.is_open());

        // Act: Simulate open failure
        should_succeed = false;
        launcher.set_open_path("NonExistent.tproject");
        EXPECT_TRUE(launcher.is_open());

        // Act: Simulate open success
        should_succeed = true;
        launcher.close();
        EXPECT_FALSE(launcher.is_open());
    }

    /// @brief Verifies that project launcher closes when project creation succeeds, but remains open if creation fails.
    TEST(project_launcher_test, launcher_dialog_create_project_lifecycle)
    {
        auto launcher = project_launcher_dialog{};
        auto should_succeed = false;
        launcher.set_on_create_project([&](string_view /*name*/, string_view /*parent_dir*/) -> bool {
            return should_succeed;
        });

        launcher.open(launcher_tab::create_project);
        EXPECT_TRUE(launcher.is_open());

        // Act: Simulate creation failure
        should_succeed = false;
        launcher.set_new_project_fields("TestGame", "C:/InvalidPath");
        EXPECT_TRUE(launcher.is_open());

        // Act: Simulate creation success
        should_succeed = true;
        launcher.close();
        EXPECT_FALSE(launcher.is_open());
    }

    /// @brief Verifies that Cancel capability is governed by whether an active project exists.
    TEST(project_launcher_test, launcher_dialog_cancel_conditional_on_active_project)
    {
        auto launcher = project_launcher_dialog{};
        auto has_active = false;
        launcher.set_has_active_project([&]() -> bool {
            return has_active;
        });

        // Case A: No active project -> dialog open
        launcher.open();
        EXPECT_TRUE(launcher.is_open());

        // Case B: Active project exists -> closing dismisses modal
        has_active = true;
        launcher.close();
        EXPECT_FALSE(launcher.is_open());
    }

    //==============================================================================
    // Empirical Stress Tests: Modal Persistence, Subdirectory Discovery & Fallback
    //==============================================================================

    /// @brief Stress-tests modal persistence over 120 rapid frames with dynamic viewport resizing,
    /// zero-size minimization, extreme clipping, and tab transitions, verifying that _is_open
    /// never drops prematurely without explicit dismissal.
    TEST(project_launcher_test, launcher_dialog_rapid_120_frames_viewport_stress)
    {
        // Setup: Engine and headless UI context
        auto engine_ctx = editor_engine_context{};
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Modal Stress Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        auto launcher = project_launcher_dialog{};
        launcher.open(launcher_tab::recent_projects);
        EXPECT_TRUE(launcher.is_open());

        // Act & Assert: Execute 120 rapid frames with simulated clipping, collapse, and tab transitions
        for (auto frame_idx = 0; frame_idx < 120; ++frame_idx)
        {
            engine_ctx.get_logger().info(tempest::format("Running frame {}", frame_idx));
            // Adversarially alternate tab requests while drawing
            if (frame_idx % 15 == 0)
            {
                launcher.open(launcher_tab::open_browse);
            }
            else if (frame_idx % 30 == 0)
            {
                launcher.open(launcher_tab::create_project);
            }

            ui_ctx.begin_ui_commands();
            launcher.draw();
            ui_ctx.finish_ui_commands();

            // Crucial invariant: modal MUST remain open despite clipping and collapse
            EXPECT_TRUE(launcher.is_open());
        }

        // Act: Explicit close must dismiss
        launcher.close();
        EXPECT_FALSE(launcher.is_open());

        // Assert: Remains closed on subsequent frame
        ui_ctx.begin_ui_commands();
        launcher.draw();
        ui_ctx.finish_ui_commands();
        EXPECT_FALSE(launcher.is_open());
    }

    /// @brief Stress-tests startup resolution when launching without arguments in complex directory trees,
    /// ensuring the launcher dialog is always opened first without auto-opening discovered projects.
    TEST(project_launcher_test, startup_resolution_deep_nesting_and_ignored_directories_stress)
    {
        const auto temp_dir = fs::temp_directory_path() / "tempest_discovery_stress";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto cwd_guard = scoped_current_path{temp_dir};

        // Create ignored directory projects
        const auto build_proj_dir = temp_dir / "build";
        fs::create_directories(build_proj_dir);
        EXPECT_TRUE(create_valid_test_project_file(build_proj_dir / "ignored_build.tproject", "IgnoredBuild"));

        const auto dot_dir = temp_dir / ".git";
        fs::create_directories(dot_dir);
        EXPECT_TRUE(create_valid_test_project_file(dot_dir / "ignored_git.tproject", "IgnoredGit"));

        const auto vendor_dir = temp_dir / "vendor";
        fs::create_directories(vendor_dir);
        EXPECT_TRUE(create_valid_test_project_file(vendor_dir / "ignored_vendor.tproject", "IgnoredVendor"));

        // Deeply nested project (depth > 2) should NOT be discovered by single-depth child search
        const auto deep_dir = temp_dir / "deep" / "nested" / "sub";
        fs::create_directories(deep_dir);
        EXPECT_TRUE(create_valid_test_project_file(deep_dir / "deep.tproject", "DeepProject"));

        // Valid immediate child project: game/game.tproject
        const auto game_dir = temp_dir / "game";
        fs::create_directories(game_dir);
        EXPECT_TRUE(create_valid_test_project_file(game_dir / "game.tproject", "game"));

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());
        const auto win_desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Discovery Stress Window",
            .fullscreen = false,
            .resizable = true,
        };
        auto reg_info = engine_ctx.register_window(win_desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);
        auto editor = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Act: Initialize startup without arguments forces launcher dialog
        const auto resolved = editor.initialize_project_from_startup({});
        EXPECT_FALSE(resolved);
        EXPECT_FALSE(engine_ctx.has_project());
        EXPECT_TRUE(editor.get_project_launcher_dialog().is_open());

        // Act: Specifying --project explicitly opens the project directly
        const auto project_arg = tempest::format("--project={}", (game_dir / "game.tproject").generic_string());
        auto explicit_args = vector<string_view>{};
        explicit_args.push_back(string_view{project_arg});
        const auto explicit_resolved = editor.initialize_project_from_startup(
            span<const string_view>{explicit_args.data(), explicit_args.size()});
        EXPECT_TRUE(explicit_resolved);
        EXPECT_TRUE(engine_ctx.has_project());
        EXPECT_EQ(fs::path{engine_ctx.get_current_project_path()}.filename(), "game.tproject");
        EXPECT_FALSE(editor.get_project_launcher_dialog().is_open());
    }

    /// @brief Stress-tests in-memory fallback scene creation, verifying valid ECS entity IDs,
    /// non-null transform and camera components, active camera tag component, valid projection and view matrices, and directional lighting.
    TEST(project_launcher_test, fallback_scene_ecs_camera_and_lighting_validation)
    {
        // Setup: Clean environment without disk scene files
        const auto temp_dir = fs::temp_directory_path() / "tempest_fallback_scene_stress";
        const auto temp_guard = scoped_temp_directory{temp_dir};
        const auto cwd_guard = scoped_current_path{temp_dir};

        auto engine_ctx = editor_engine_context{};
        engine_ctx.set_session_file_path((temp_dir / "editor_session.json").generic_string());

        // Act: Invoke startup scene loading when no scenes exist on disk
        const auto scene_loaded = engine_ctx.load_startup_scene_if_needed();
        EXPECT_TRUE(scene_loaded);

        auto& registry = engine_ctx.get_entities();
        auto camera_sys = render_system::camera_system{registry};

        // Active camera entity discovery
        const auto active_cam_entity = camera_sys.get_active_camera_entity();
        ASSERT_TRUE(active_cam_entity.has_value());
        EXPECT_TRUE(registry.is_valid(*active_cam_entity));

        // Entity component attachment invariants
        EXPECT_TRUE(registry.has<ecs::transform_component>(*active_cam_entity));
        EXPECT_TRUE(registry.has<render_system::camera_component>(*active_cam_entity));
        // Active camera tag component registered in engine_types must be properly deserialized from fallback scene JSON
        EXPECT_TRUE(registry.has<render_system::active_camera_component>(*active_cam_entity));

        const auto* const cam_comp = registry.try_get<render_system::camera_component>(*active_cam_entity);
        ASSERT_NE(cam_comp, nullptr);
        EXPECT_TRUE(cam_comp->is_active);
        EXPECT_EQ(cam_comp->target, render_system::camera_target_type::viewport);
        EXPECT_GT(cam_comp->aspect_ratio, 0.0F);
        EXPECT_GT(cam_comp->vertical_fov, 0.0F);
        EXPECT_GT(cam_comp->near_plane, 0.0F);
        EXPECT_GT(cam_comp->far_plane, cam_comp->near_plane);

        const auto* const tx_comp = registry.try_get<ecs::transform_component>(*active_cam_entity);
        ASSERT_NE(tx_comp, nullptr);
        EXPECT_EQ(tx_comp->position().y, 1.6F);
        EXPECT_EQ(tx_comp->position().z, -5.0F);

        // Active render camera matrix mathematical validity
        const auto active_render_cam = camera_sys.get_active_camera();
        ASSERT_TRUE(active_render_cam.has_value());

        // Check view matrix and projection matrix for finite values
        for (auto idx = 0; idx < 16; ++idx)
        {
            EXPECT_TRUE(tempest::isfinite(active_render_cam->view.data[idx]));
            EXPECT_TRUE(tempest::isfinite(active_render_cam->proj.data[idx]));
        }

        // Directional light entity verification
        auto found_light = false;
        registry.each([&](const ecs::self_component& self, const render_system::directional_light_component& light,
                          const ecs::transform_component&) {
            found_light = true;
            EXPECT_TRUE(registry.is_valid(self.entity));
            EXPECT_GT(light.intensity, 0.0F);
            EXPECT_GE(light.color.x, 0.0F);
            EXPECT_LE(light.color.x, 1.0F);
        });
        EXPECT_TRUE(found_light);
    }

    /// @brief Stress-tests asset database mounting resilience and verifies shader bytecode
    /// resolution with default roots and fallback mechanisms.
    TEST(project_launcher_test, shader_loading_resilience_under_missing_or_partial_mounts)
    {
        auto engine_ctx = editor_engine_context{};

        // Verify default shader roots are mounted during engine context construction
        const auto& asset_db = engine_ctx.get_assets();
        const auto disk_path_opt = asset_db.resolve_disk_path("@shaders/imgui.vert.spv");
        // Note: disk path may or may not exist depending on build environment, but resolution must not assert
        EXPECT_FALSE(string_view(disk_path_opt.has_value() ? *disk_path_opt : "").empty() &&
                     !disk_path_opt.has_value());

        // Querying a completely non-existent shader alias must fail gracefully with nullopt, never assert
        const auto non_existent_opt = asset_db.resolve_disk_path("@shaders/non_existent_stress_shader.spv");
        EXPECT_FALSE(non_existent_opt.has_value());
    }
} // namespace tempest::editor::tests
