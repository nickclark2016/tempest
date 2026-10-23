#include <tempest/windows/project_view_window.hpp>

#include <tempest/editor.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/ui.hpp>

#include <gtest/gtest.h>
#include <imgui.h>

namespace tempest::editor::tests
{
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
