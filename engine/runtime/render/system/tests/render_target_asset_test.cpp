#include <tempest/render_target_asset.hpp>

#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/render_system/render_components.hpp>

#include <gtest/gtest.h>

namespace
{
    struct test_conditional_component
    {
        static constexpr int32_t default_extra_value = 42;

        bool enable_extra = false;
        int32_t extra_value = default_extra_value;

        static constexpr auto is_extra_visible(const test_conditional_component& component) noexcept -> bool
        {
            return component.enable_extra;
        }
    };
} // namespace

template <>
struct tempest::ecs::reflect<test_conditional_component>
{
    static auto describe(tempest::ecs::type_builder<test_conditional_component>& builder) -> void
    {
        builder.version(1)
            .field<&test_conditional_component::enable_extra>("enable_extra")
            .field<&test_conditional_component::extra_value>("extra_value")
            .visible_when<&test_conditional_component::is_extra_visible>();
    }
};

// ============================================================================
// Render Target Asset Serialization & Conversion Tests
// ============================================================================

/// @brief Verifies that render_target_asset serializes to JSON and deserializes back matching original values.
TEST(render_target_asset_tests, json_round_trip)
{
    // Setup: Create a populated render_target_asset with custom non-default properties
    auto original_asset = tempest::assets::render_target_asset{};
    original_asset.name = "offscreen_color_target";
    original_asset.width = 2560;
    original_asset.height = 1440;
    original_asset.format = tempest::core::texture_format::rgba16_unorm;
    original_asset.clear_color = {0.1F, 0.2F, 0.3F, 1.0F};
    original_asset.sampler.mag_filter = tempest::core::magnify_texture_filter::nearest;
    original_asset.sampler.min_filter = tempest::core::minify_texture_filter::nearest_mipmap_nearest;
    original_asset.sampler.wrap_s = tempest::core::texture_wrap_mode::clamp_to_edge;
    original_asset.sampler.wrap_t = tempest::core::texture_wrap_mode::clamp_to_edge;
    original_asset.texture_id = tempest::guid::generate_random_guid();

    // Act: Serialize to JSON string and parse back into a new asset
    const auto serialized_json = original_asset.to_string();
    const auto deserialized_asset = tempest::assets::render_target_asset::load_from_string(serialized_json);

    // Assert: Deserialization succeeds and all properties match the original
    ASSERT_TRUE(deserialized_asset.has_value());
    EXPECT_EQ(deserialized_asset->name, original_asset.name);
    EXPECT_EQ(deserialized_asset->width, original_asset.width);
    EXPECT_EQ(deserialized_asset->height, original_asset.height);
    EXPECT_EQ(deserialized_asset->format, original_asset.format);
    EXPECT_EQ(deserialized_asset->clear_color, original_asset.clear_color);
    EXPECT_EQ(deserialized_asset->sampler, original_asset.sampler);
    EXPECT_EQ(deserialized_asset->texture_id, original_asset.texture_id);
    EXPECT_EQ(*deserialized_asset, original_asset);
}

/// @brief Verifies that render_target_asset can be persisted to disk and reloaded.
TEST(render_target_asset_tests, file_round_trip)
{
    // Setup: Create asset and determine test file path
    auto original_asset = tempest::assets::render_target_asset{};
    original_asset.name = "disk_saved_target";
    original_asset.width = 1920;
    original_asset.height = 1080;
    original_asset.format = tempest::core::texture_format::rgba32_float;
    original_asset.clear_color = {0.0F, 0.5F, 1.0F, 1.0F};

    const auto temp_directory = tempest::filesystem::temp_directory_path();
    const auto temp_file_path = temp_directory / "tempest_render_target_test.json";
    const auto temp_file_string = temp_file_path.string();

    // Act: Save to file and reload from file
    const auto save_succeeded = original_asset.save_to_file(temp_file_string);
    const auto loaded_asset = tempest::assets::render_target_asset::load_from_file(temp_file_string);

    // Clean up temporary test file
    tempest::filesystem::remove(temp_file_path);

    // Assert: File save and load operations both succeed and contents match
    EXPECT_TRUE(save_succeeded);
    ASSERT_TRUE(loaded_asset.has_value());
    EXPECT_EQ(*loaded_asset, original_asset);
}

/// @brief Verifies that default values are correctly populated on deserialization when fields are omitted.
TEST(render_target_asset_tests, default_values_round_trip)
{
    // Setup: Default construct an asset
    const auto default_asset = tempest::assets::render_target_asset{};

    // Act: Serialize and deserialize the default asset
    const auto serialized_json = default_asset.to_string();
    const auto deserialized_asset = tempest::assets::render_target_asset::load_from_string(serialized_json);

    // Assert: Deserialization preserves default values
    ASSERT_TRUE(deserialized_asset.has_value());
    EXPECT_EQ(deserialized_asset->width, tempest::assets::render_target_asset::default_width);
    EXPECT_EQ(deserialized_asset->height, tempest::assets::render_target_asset::default_height);
    EXPECT_EQ(deserialized_asset->format, tempest::assets::render_target_asset::default_format);
    EXPECT_EQ(deserialized_asset->clear_color, tempest::assets::render_target_asset::default_clear_color);
    EXPECT_EQ(deserialized_asset->texture_id, tempest::assets::render_target_asset::default_texture_id);
    EXPECT_EQ(*deserialized_asset, default_asset);
}

/// @brief Verifies bidirectional conversion between render_target_asset and core::texture.
TEST(render_target_asset_tests, to_and_from_core_texture)
{
    // Setup: Construct render_target_asset with explicit configuration
    auto source_asset = tempest::assets::render_target_asset{};
    source_asset.name = "ui_viewport_texture";
    source_asset.width = 800;
    source_asset.height = 600;
    source_asset.format = tempest::core::texture_format::rgba8_srgb;
    source_asset.sampler.mag_filter = tempest::core::magnify_texture_filter::linear;
    source_asset.sampler.min_filter = tempest::core::minify_texture_filter::linear;
    source_asset.sampler.wrap_s = tempest::core::texture_wrap_mode::repeat;
    source_asset.sampler.wrap_t = tempest::core::texture_wrap_mode::repeat;

    // Act: Convert to core::texture and then convert back to render_target_asset
    const auto core_texture = source_asset.to_core_texture();
    const auto roundtrip_asset = tempest::assets::render_target_asset::from_core_texture(core_texture);

    // Assert: Core texture fields reflect source asset
    EXPECT_EQ(core_texture.name, source_asset.name);
    EXPECT_EQ(core_texture.width, source_asset.width);
    EXPECT_EQ(core_texture.height, source_asset.height);
    EXPECT_EQ(core_texture.format, source_asset.format);
    EXPECT_EQ(core_texture.sampler, source_asset.sampler);
    EXPECT_EQ(core_texture.compression, tempest::core::texture_compression::none);

    // Assert: Round-tripped asset fields match core texture
    EXPECT_EQ(roundtrip_asset.name, core_texture.name);
    EXPECT_EQ(roundtrip_asset.width, core_texture.width);
    EXPECT_EQ(roundtrip_asset.height, core_texture.height);
    EXPECT_EQ(roundtrip_asset.format, core_texture.format);
    EXPECT_EQ(roundtrip_asset.sampler, core_texture.sampler);
}

/// @brief Verifies get_texture_id fallback logic when explicit texture_id is unset vs set.
TEST(render_target_asset_tests, get_texture_id)
{
    // Setup: Construct asset with default empty texture_id and generate a fallback asset guid
    auto asset = tempest::assets::render_target_asset{};
    const auto fallback_guid = tempest::guid::generate_random_guid();
    const auto explicit_guid = tempest::guid::generate_random_guid();

    // Act & Assert: When texture_id is empty, fallback asset guid is returned
    EXPECT_EQ(asset.get_texture_id(fallback_guid), fallback_guid);

    // Act: Set explicit texture_id on the asset
    asset.texture_id = explicit_guid;

    // Assert: Explicit texture_id takes priority over the fallback guid
    EXPECT_EQ(asset.get_texture_id(fallback_guid), explicit_guid);
}

// ============================================================================
// Reflection and Visibility Predicate Tests
// ============================================================================

/// @brief Verifies that camera_component reflects render_texture_id as an asset_ref with conditional visibility.
TEST(render_target_asset_tests, camera_component_reflection_visible_when)
{
    // Setup: Build the reflection type descriptor for camera_component
    auto builder = tempest::ecs::type_builder<tempest::render_system::camera_component>{
        "tempest::render_system::camera_component"};
    tempest::ecs::reflect<tempest::render_system::camera_component>::describe(builder);
    const auto descriptor = builder.build();

    // Act: Look up the reflected render_texture_id field descriptor
    const auto* field_desc = descriptor.find_field("render_texture_id");

    // Assert: Field descriptor exists and has expected metadata
    ASSERT_NE(field_desc, nullptr);
    EXPECT_EQ(field_desc->kind, tempest::ecs::field_kind::asset_ref);
    EXPECT_EQ(field_desc->asset_type_hash,
              tempest::normalized_type_hash_v<tempest::assets::render_target_asset>);
    ASSERT_NE(field_desc->is_visible, nullptr);

    // Setup: Construct camera_component with viewport target
    auto camera_instance = tempest::render_system::camera_component{};
    camera_instance.target = tempest::render_system::camera_target_type::viewport;

    // Act & Assert: Field is hidden when target is viewport
    EXPECT_FALSE(field_desc->is_visible(&camera_instance));

    // Act: Switch camera target to render_texture
    camera_instance.target = tempest::render_system::camera_target_type::render_texture;

    // Assert: Field is visible when target is render_texture
    EXPECT_TRUE(field_desc->is_visible(&camera_instance));
}

/// @brief Verifies that mesh_component and material_component reflect their IDs as asset_ref.
TEST(render_target_asset_tests, mesh_and_material_component_reflection_asset_ref)
{
    // Setup: Build reflection type descriptor for mesh_component
    auto mesh_builder = tempest::ecs::type_builder<tempest::core::mesh_component>{
        "tempest::core::mesh_component"};
    tempest::ecs::reflect<tempest::core::mesh_component>::describe(mesh_builder);
    const auto mesh_descriptor = mesh_builder.build();

    // Act & Assert: mesh_id is an asset_ref pointing to core::mesh
    const auto* mesh_field = mesh_descriptor.find_field("mesh_id");
    ASSERT_NE(mesh_field, nullptr);
    EXPECT_EQ(mesh_field->kind, tempest::ecs::field_kind::asset_ref);
    EXPECT_EQ(mesh_field->asset_type_hash, tempest::normalized_type_hash_v<tempest::core::mesh>);

    // Setup: Build reflection type descriptor for material_component
    auto material_builder = tempest::ecs::type_builder<tempest::core::material_component>{
        "tempest::core::material_component"};
    tempest::ecs::reflect<tempest::core::material_component>::describe(material_builder);
    const auto material_descriptor = material_builder.build();

    // Act & Assert: material_id is an asset_ref pointing to core::material
    const auto* material_field = material_descriptor.find_field("material_id");
    ASSERT_NE(material_field, nullptr);
    EXPECT_EQ(material_field->kind, tempest::ecs::field_kind::asset_ref);
    EXPECT_EQ(material_field->asset_type_hash, tempest::normalized_type_hash_v<tempest::core::material>);
}

/// @brief Verifies that type_builder::visible_when template predicate evaluates correctly.
TEST(render_target_asset_tests, visible_when_template_predicate)
{
    // Setup: Build reflection type descriptor for test_conditional_component
    auto builder = tempest::ecs::type_builder<test_conditional_component>{"test_conditional_component"};
    tempest::ecs::reflect<test_conditional_component>::describe(builder);
    const auto descriptor = builder.build();

    // Act: Look up extra_value field descriptor
    const auto* field_desc = descriptor.find_field("extra_value");

    // Assert: Field descriptor exists and has non-null is_visible predicate
    ASSERT_NE(field_desc, nullptr);
    ASSERT_NE(field_desc->is_visible, nullptr);

    // Setup: Component with enable_extra = false
    auto component = test_conditional_component{};
    component.enable_extra = false;

    // Act & Assert: Predicate returns false
    EXPECT_FALSE(field_desc->is_visible(&component));

    // Act: Toggle enable_extra = true
    component.enable_extra = true;

    // Assert: Predicate returns true
    EXPECT_TRUE(field_desc->is_visible(&component));
}
