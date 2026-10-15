#include <tempest/property_grid.hpp>

#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/velocity_component.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/transform_component.hpp>

#include <gtest/gtest.h>

namespace tempest::editor::tests
{
    // =========================================================================
    // Property Grid Field Name Prettification Tests
    // =========================================================================

    /// @brief Verifies that basic snake_case field identifiers are converted into Title Case display names.
    TEST(PropertyGridTests, PrettifyBasicSnakeCaseFieldNames)
    {
        // Setup & Act
        const auto aspect_ratio_display = prettify_field_name("aspect_ratio", ecs::field_kind::float32);
        const auto near_plane_display = prettify_field_name("near_plane", ecs::field_kind::float32);
        const auto far_plane_display = prettify_field_name("far_plane", ecs::field_kind::float32);
        const auto walk_speed_display = prettify_field_name("walk_speed", ecs::field_kind::float32);

        // Assert
        EXPECT_EQ(aspect_ratio_display, "Aspect Ratio");
        EXPECT_EQ(near_plane_display, "Near Plane");
        EXPECT_EQ(far_plane_display, "Far Plane");
        EXPECT_EQ(walk_speed_display, "Walk Speed");
    }

    /// @brief Verifies that asset_ref field identifiers strip the trailing '_id' suffix.
    TEST(PropertyGridTests, PrettifyAssetRefStripsIdSuffix)
    {
        // Setup & Act
        const auto mesh_display = prettify_field_name("mesh_id", ecs::field_kind::asset_ref);
        const auto material_display = prettify_field_name("material_id", ecs::field_kind::asset_ref);
        const auto render_texture_display = prettify_field_name("render_texture_id", ecs::field_kind::asset_ref);

        // Assert
        EXPECT_EQ(mesh_display, "Mesh");
        EXPECT_EQ(material_display, "Material");
        EXPECT_EQ(render_texture_display, "Render Texture");
    }

    /// @brief Verifies that asset_ref names that are simply "id" or non-asset fields ending in "_id" retain their name.
    TEST(PropertyGridTests, PrettifyAssetRefPreservesShortIdOrNonAssetRefs)
    {
        // Setup & Act
        const auto raw_id_display = prettify_field_name("id", ecs::field_kind::asset_ref);
        const auto non_asset_id_display = prettify_field_name("mesh_id", ecs::field_kind::guid);

        // Assert
        EXPECT_EQ(raw_id_display, "Id");
        EXPECT_EQ(non_asset_id_display, "Mesh Id");
    }

    /// @brief Verifies handling of edge cases such as empty strings, single letters, and underscores.
    TEST(PropertyGridTests, PrettifyEdgeCases)
    {
        // Setup & Act
        const auto empty_display = prettify_field_name("", ecs::field_kind::boolean);
        const auto single_char_display = prettify_field_name("x", ecs::field_kind::float32);
        const auto leading_underscore_display = prettify_field_name("_hidden_value", ecs::field_kind::int32);
        const auto trailing_underscore_display = prettify_field_name("padded_value_", ecs::field_kind::int32);
        const auto double_underscore_display = prettify_field_name("foo__bar", ecs::field_kind::int32);

        // Assert
        EXPECT_EQ(empty_display, "");
        EXPECT_EQ(single_char_display, "X");
        EXPECT_EQ(leading_underscore_display, "Hidden Value");
        EXPECT_EQ(trailing_underscore_display, "Padded Value");
        EXPECT_EQ(double_underscore_display, "Foo Bar");
    }

    // =========================================================================
    // Component Explicit Display Name UI Hints Tests
    // =========================================================================

    /// @brief Verifies that camera_component reflection provides user-friendly display name hints.
    TEST(PropertyGridTests, CameraComponentDisplayNameHints)
    {
        // Setup
        auto builder = ecs::type_builder<render_system::camera_component>{"tempest::render_system::camera_component"};
        ecs::reflect<render_system::camera_component>::describe(builder);
        const auto descriptor = builder.build();

        // Act & Assert
        const auto* target_field = descriptor.find_field("target");
        ASSERT_NE(target_field, nullptr);
        EXPECT_EQ(target_field->hints.display_name, "Target");

        const auto* render_target_field = descriptor.find_field("render_texture_id");
        ASSERT_NE(render_target_field, nullptr);
        EXPECT_EQ(render_target_field->hints.display_name, "Render Target");

        const auto* fov_field = descriptor.find_field("vertical_fov");
        ASSERT_NE(fov_field, nullptr);
        EXPECT_EQ(fov_field->hints.display_name, "Vertical FOV");

        const auto* active_field = descriptor.find_field("is_active");
        ASSERT_NE(active_field, nullptr);
        EXPECT_EQ(active_field->hints.display_name, "Active");
    }

    /// @brief Verifies that transform_component reflection provides user-friendly display name hints.
    TEST(PropertyGridTests, TransformComponentDisplayNameHints)
    {
        // Setup
        auto builder = ecs::type_builder<ecs::transform_component>{"tempest::ecs::transform_component"};
        ecs::reflect<ecs::transform_component>::describe(builder);
        const auto descriptor = builder.build();

        // Act & Assert
        const auto* pos_field = descriptor.find_field("position");
        ASSERT_NE(pos_field, nullptr);
        EXPECT_EQ(pos_field->hints.display_name, "Position");

        const auto* rot_field = descriptor.find_field("rotation");
        ASSERT_NE(rot_field, nullptr);
        EXPECT_EQ(rot_field->hints.display_name, "Rotation");

        const auto* scale_field = descriptor.find_field("scale");
        ASSERT_NE(scale_field, nullptr);
        EXPECT_EQ(scale_field->hints.display_name, "Scale");
    }

    /// @brief Verifies that character_controller_component reflection provides user-friendly display name hints.
    TEST(PropertyGridTests, CharacterControllerComponentDisplayNameHints)
    {
        // Setup
        auto builder = ecs::type_builder<physics::character_controller_component>{
            "tempest::physics::character_controller_component"};
        ecs::reflect<physics::character_controller_component>::describe(builder);
        const auto descriptor = builder.build();

        // Act & Assert
        const auto* height_field = descriptor.find_field("character_height");
        ASSERT_NE(height_field, nullptr);
        EXPECT_EQ(height_field->hints.display_name, "Character Height");

        const auto* speed_field = descriptor.find_field("walk_speed");
        ASSERT_NE(speed_field, nullptr);
        EXPECT_EQ(speed_field->hints.display_name, "Walk Speed");
    }

    /// @brief Verifies that velocity_component reflection provides user-friendly display name hints.
    TEST(PropertyGridTests, VelocityComponentDisplayNameHints)
    {
        // Setup
        auto builder = ecs::type_builder<physics::velocity_component>{"tempest::physics::velocity_component"};
        ecs::reflect<physics::velocity_component>::describe(builder);
        const auto descriptor = builder.build();

        // Act & Assert
        const auto* linear_field = descriptor.find_field("linear");
        ASSERT_NE(linear_field, nullptr);
        EXPECT_EQ(linear_field->hints.display_name, "Linear Velocity");

        const auto* angular_field = descriptor.find_field("angular");
        ASSERT_NE(angular_field, nullptr);
        EXPECT_EQ(angular_field->hints.display_name, "Angular Velocity");
    }
} // namespace tempest::editor::tests
