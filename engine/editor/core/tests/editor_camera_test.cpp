#include <tempest/editor_camera.hpp>

#include <tempest/math_utils.hpp>
#include <tempest/transformations.hpp>

#include <gtest/gtest.h>

namespace tempest::editor::tests
{
    // =========================================================================
    // Editor Camera Initialization & Matrix Math Tests
    // =========================================================================

    /// @brief Verifies default constructor field initialization and ensures that
    /// the generated perspective projection and view matrices satisfy mathematical
    /// invertibility invariants (P * P^-1 = I and V * V^-1 = I).
    TEST(editor_camera_test, default_state_and_matrices)
    {
        // 1. Setup & Default Construction
        const auto cam = editor_camera{};

        // 2. Validate Default Fields
        EXPECT_EQ(cam.get_position(), math::vec3<float>(0.0F, 2.0F, -5.0F));
        EXPECT_FLOAT_EQ(cam.get_yaw(), 0.0F);
        EXPECT_FLOAT_EQ(cam.get_pitch(), 0.0F);
        EXPECT_NEAR(cam.get_aspect_ratio(), 16.0F / 9.0F, 0.001F);
        EXPECT_NEAR(cam.get_vertical_fov(), math::as_radians(60.0F), 0.001F);
        EXPECT_FLOAT_EQ(cam.get_near_plane(), 0.1F);
        EXPECT_FLOAT_EQ(cam.get_move_speed(), 5.0F);

        // 3. Validate Render Camera Output
        const auto render_cam = cam.get_render_camera();

        // Eye position should match camera position with w = 1.0f
        EXPECT_FLOAT_EQ(render_cam.eye_position.x, 0.0F);
        EXPECT_FLOAT_EQ(render_cam.eye_position.y, 2.0F);
        EXPECT_FLOAT_EQ(render_cam.eye_position.z, -5.0F);
        EXPECT_FLOAT_EQ(render_cam.eye_position.w, 1.0F);

        // 4. Assert Projection Invertibility
        const auto proj_ident = render_cam.proj * render_cam.inv_proj;
        for (std::size_t i = 0; i < 4; ++i)
        {
            for (std::size_t j = 0; j < 4; ++j)
            {
                const auto expected = (i == j) ? 1.0F : 0.0F;
                EXPECT_NEAR(proj_ident[i][j], expected, 1e-4F);
            }
        }

        // 5. Assert View Matrix Invertibility
        const auto view_ident = render_cam.view * render_cam.inv_view;
        for (std::size_t i = 0; i < 4; ++i)
        {
            for (std::size_t j = 0; j < 4; ++j)
            {
                const auto expected = (i == j) ? 1.0F : 0.0F;
                EXPECT_NEAR(view_ident[i][j], expected, 1e-4F);
            }
        }
    }

    // =========================================================================
    // Free Fly Movement & Orientation Tests
    // =========================================================================

    /// @brief Verifies relative translation along local forward, right, and world-up axes
    /// in the camera's default identity orientation.
    TEST(editor_camera_test, relative_wasd_movement)
    {
        // 1. Setup Camera at Origin
        auto cam = editor_camera{};
        cam.set_position({0.0F, 0.0F, 0.0F});

        // 2. Forward Movement (+Z in default orientation)
        cam.move(5.0F, 0.0F, 0.0F);
        EXPECT_NEAR(cam.get_position().x, 0.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().y, 0.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().z, 5.0F, 1e-4F);

        // 3. Right Movement (+X)
        cam.move(0.0F, 3.0F, 0.0F);
        EXPECT_NEAR(cam.get_position().x, 3.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().y, 0.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().z, 5.0F, 1e-4F);

        // 4. Elevation / World Up Movement (+Y)
        cam.move(0.0F, 0.0F, 2.0F);
        EXPECT_NEAR(cam.get_position().x, 3.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().y, 2.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().z, 5.0F, 1e-4F);
    }

    /// @brief Verifies that translation moves along the camera's local rotated heading
    /// (e.g. after a 90-degree yaw rotation) rather than world coordinates.
    TEST(editor_camera_test, rotated_wasd_movement_and_heading)
    {
        // 1. Setup Camera at Origin & Rotate 90 degrees Yaw
        auto cam = editor_camera{};
        cam.set_position({0.0F, 0.0F, 0.0F});
        cam.set_rotation(math::as_radians(90.0F), 0.0F);

        // 2. Validate Rotated Heading Vectors
        const auto fwd = cam.get_forward_vector();
        EXPECT_NEAR(fwd.x, 1.0F, 1e-4F);
        EXPECT_NEAR(fwd.y, 0.0F, 1e-4F);
        EXPECT_NEAR(fwd.z, 0.0F, 1e-4F);

        const auto rgt = cam.get_right_vector();
        EXPECT_NEAR(rgt.x, 0.0F, 1e-4F);
        EXPECT_NEAR(rgt.y, 0.0F, 1e-4F);
        EXPECT_NEAR(rgt.z, -1.0F, 1e-4F);

        // 3. Move Forward (Should Translate Along Rotated +X)
        cam.move(4.0F, 0.0F, 0.0F);
        EXPECT_NEAR(cam.get_position().x, 4.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().y, 0.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().z, 0.0F, 1e-4F);

        // 4. Move Right (Should Translate Along Rotated -Z)
        cam.move(0.0F, 2.0F, 0.0F);
        EXPECT_NEAR(cam.get_position().x, 4.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().y, 0.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().z, -2.0F, 1e-4F);
    }

    /// @brief Verifies mouse look yaw/pitch rotations and asserts that pitch angles
    /// exceeding physical limits strictly clamp to [-89 deg, 89 deg] to prevent gimbal flip.
    TEST(editor_camera_test, mouse_look_and_pitch_clamping)
    {
        auto cam = editor_camera{};

        // 1. Rotate Yaw by 90 degrees
        cam.rotate(math::as_radians(90.0F), 0.0F);
        EXPECT_NEAR(cam.get_yaw(), math::as_radians(90.0F), 1e-4F);

        // 2. Rotate Pitch Upwards Beyond 90 deg (Must clamp to +89 deg)
        cam.rotate(0.0F, math::as_radians(120.0F));
        EXPECT_NEAR(cam.get_pitch(), math::as_radians(89.0F), 1e-4F);

        // 3. Rotate Pitch Downwards Beyond -90 deg (Must clamp to -89 deg)
        cam.rotate(0.0F, math::as_radians(-200.0F));
        EXPECT_NEAR(cam.get_pitch(), math::as_radians(-89.0F), 1e-4F);
    }

    // =========================================================================
    // Focus Framing & Target Tracking Tests
    // =========================================================================

    /// @brief Verifies that focus_on accurately positions the camera at the requested
    /// distance along the default view heading.
    TEST(editor_camera_test, focus_on_target)
    {
        // 1. Setup Camera Looking Along +Z
        auto cam = editor_camera{};
        cam.set_rotation(0.0F, 0.0F);

        // 2. Focus on Target Point
        const auto target = math::vec3<float>{10.0F, 5.0F, 20.0F};
        cam.focus_on(target, 5.0F);

        // 3. Assert Position (target - forward * distance)
        EXPECT_NEAR(cam.get_position().x, 10.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().y, 5.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().z, 15.0F, 1e-4F);
    }

    /// @brief Verifies that focus_on accurately positions the camera along rotated headings.
    TEST(editor_camera_test, focus_on_rotated_heading)
    {
        // 1. Setup Camera Looking Along +X (90 deg Yaw)
        auto cam = editor_camera{};
        cam.set_rotation(math::as_radians(90.0F), 0.0F);

        // 2. Focus on Target Point
        const auto target = math::vec3<float>{10.0F, 0.0F, 0.0F};
        cam.focus_on(target, 4.0F);

        // 3. Assert Position (target - forward * distance)
        EXPECT_NEAR(cam.get_position().x, 6.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().y, 0.0F, 1e-4F);
        EXPECT_NEAR(cam.get_position().z, 0.0F, 1e-4F);
    }

    // =========================================================================
    // Boundary & Input Sanitization Tests
    // =========================================================================

    /// @brief Verifies aspect ratio and movement speed adjustment bounds and safeguards.
    TEST(editor_camera_test, aspect_ratio_and_speed_adjustments)
    {
        auto cam = editor_camera{};

        // 1. Valid Aspect Ratio Update
        cam.set_aspect_ratio(1920.0F / 1080.0F);
        EXPECT_NEAR(cam.get_aspect_ratio(), 1920.0F / 1080.0F, 1e-4F);

        // 2. Invalid Zero/Negative Aspect Ratio (Must be ignored)
        cam.set_aspect_ratio(0.0F);
        EXPECT_NEAR(cam.get_aspect_ratio(), 1920.0F / 1080.0F, 1e-4F);

        // 3. Incremental Speed Adjustment
        cam.adjust_move_speed(2.5F);
        EXPECT_FLOAT_EQ(cam.get_move_speed(), 7.5F);

        // 4. Large Negative Decrement (Must clamp to minimum 0.1)
        cam.adjust_move_speed(-10.0F);
        EXPECT_FLOAT_EQ(cam.get_move_speed(), 0.1F);
    }

    /// @brief Verifies input clamping on camera parameter setters to prevent numerical
    /// instability or invalid matrix configurations.
    TEST(editor_camera_test, setter_boundary_clamping)
    {
        auto cam = editor_camera{};

        // 1. Negative Vertical FOV (Clamped to 1 deg)
        cam.set_vertical_fov(-1.0F);
        EXPECT_NEAR(cam.get_vertical_fov(), math::as_radians(1.0F), 1e-4F);

        // 2. Excessive Vertical FOV (Clamped to 179 deg)
        cam.set_vertical_fov(math::as_radians(200.0F));
        EXPECT_NEAR(cam.get_vertical_fov(), math::as_radians(179.0F), 1e-4F);

        // 3. Negative Near Plane (Clamped to 0.001)
        cam.set_near_plane(-5.0F);
        EXPECT_FLOAT_EQ(cam.get_near_plane(), 0.001F);

        // 4. Zero Near Plane (Clamped to 0.001)
        cam.set_near_plane(0.0F);
        EXPECT_FLOAT_EQ(cam.get_near_plane(), 0.001F);

        // 5. Negative Movement Speed (Clamped to 0.01)
        cam.set_move_speed(-10.0F);
        EXPECT_FLOAT_EQ(cam.get_move_speed(), 0.01F);
    }

    /// @brief Verifies that NaN and Infinite floating-point values are safely rejected by
    /// editor_camera setters and transform methods to prevent matrix corruption.
    TEST(editor_camera_test, non_finite_input_safeguards)
    {
        auto cam = editor_camera{};
        const auto initial_pos = cam.get_position();
        const auto initial_yaw = cam.get_yaw();
        const auto initial_pitch = cam.get_pitch();

        constexpr auto nan_val = std::numeric_limits<float>::quiet_NaN();
        constexpr auto inf_val = std::numeric_limits<float>::infinity();

        // 1. Position NaN / Inf Rejected
        cam.set_position({nan_val, 0.0F, 0.0F});
        EXPECT_EQ(cam.get_position(), initial_pos);

        cam.set_position({0.0F, inf_val, 0.0F});
        EXPECT_EQ(cam.get_position(), initial_pos);

        // 2. Move NaN / Inf Rejected
        cam.move(nan_val, 1.0F, 0.0F);
        EXPECT_EQ(cam.get_position(), initial_pos);

        cam.move(1.0F, inf_val, 0.0F);
        EXPECT_EQ(cam.get_position(), initial_pos);

        // 3. Rotation NaN / Inf Rejected
        cam.rotate(nan_val, 0.0F);
        EXPECT_FLOAT_EQ(cam.get_yaw(), initial_yaw);

        cam.rotate(0.0F, inf_val);
        EXPECT_FLOAT_EQ(cam.get_pitch(), initial_pitch);

        cam.set_rotation(nan_val, 0.0F);
        EXPECT_FLOAT_EQ(cam.get_yaw(), initial_yaw);

        // 4. Focus On NaN Target Rejected
        cam.focus_on({nan_val, 0.0F, 0.0F}, 5.0F);
        EXPECT_EQ(cam.get_position(), initial_pos);

        cam.focus_on({10.0F, 0.0F, 0.0F}, inf_val);
        EXPECT_EQ(cam.get_position(), initial_pos);
    }
} // namespace tempest::editor::tests
