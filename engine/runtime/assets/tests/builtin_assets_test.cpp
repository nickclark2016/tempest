#include <gtest/gtest.h>

#include <tempest/asset_database.hpp>
#include <tempest/builtin_assets.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/material.hpp>
#include <tempest/material_asset.hpp>
#include <tempest/texture.hpp>
#include <tempest/vertex.hpp>

// ============================================================================
// Built-in Assets & Material Asset Test Suite
// ============================================================================

/// @brief Verify that registering built-in assets into an empty database and registries
/// populates deterministic meshes, textures, and materials with valid geometry and dimensions,
/// and that all built-in assets are marked non-transient.
TEST(builtin_assets_tests, builtin_mount_resolves_deterministic_primitives)
{
    // Setup: Create empty asset database and registries
    auto database = tempest::assets::asset_database{};
    auto mesh_registry = tempest::core::mesh_registry{};
    auto texture_registry = tempest::core::texture_registry{};
    auto material_registry = tempest::core::material_registry{};

    // Act: Register built-in assets
    tempest::assets::builtin::register_builtin_assets(database, &mesh_registry, &texture_registry, &material_registry);

    // Assert: Canonical meshes are registered and resolve valid geometry
    const auto plane_mesh_opt = mesh_registry.find(tempest::assets::builtin::plane_mesh_guid);
    ASSERT_TRUE(plane_mesh_opt.has_value());
    EXPECT_GT(plane_mesh_opt->vertices.size(), 0U);
    EXPECT_GT(plane_mesh_opt->indices.size(), 0U);
    EXPECT_TRUE(plane_mesh_opt->has_normals);
    EXPECT_TRUE(plane_mesh_opt->has_tangents);
    EXPECT_TRUE(plane_mesh_opt->validate());

    const auto box_mesh_opt = mesh_registry.find(tempest::assets::builtin::box_mesh_guid);
    ASSERT_TRUE(box_mesh_opt.has_value());
    EXPECT_GT(box_mesh_opt->vertices.size(), 0U);
    EXPECT_GT(box_mesh_opt->indices.size(), 0U);
    EXPECT_TRUE(box_mesh_opt->has_normals);
    EXPECT_TRUE(box_mesh_opt->has_tangents);
    EXPECT_TRUE(box_mesh_opt->validate());

    const auto sphere_mesh_opt = mesh_registry.find(tempest::assets::builtin::sphere_mesh_guid);
    ASSERT_TRUE(sphere_mesh_opt.has_value());
    EXPECT_GT(sphere_mesh_opt->vertices.size(), 0U);
    EXPECT_GT(sphere_mesh_opt->indices.size(), 0U);
    EXPECT_TRUE(sphere_mesh_opt->has_normals);
    EXPECT_TRUE(sphere_mesh_opt->has_tangents);
    EXPECT_TRUE(sphere_mesh_opt->validate());

    const auto capsule_mesh_opt = mesh_registry.find(tempest::assets::builtin::capsule_mesh_guid);
    ASSERT_TRUE(capsule_mesh_opt.has_value());
    EXPECT_GT(capsule_mesh_opt->vertices.size(), 0U);
    EXPECT_GT(capsule_mesh_opt->indices.size(), 0U);
    EXPECT_TRUE(capsule_mesh_opt->has_normals);
    EXPECT_TRUE(capsule_mesh_opt->has_tangents);
    EXPECT_TRUE(capsule_mesh_opt->validate());

    const auto cylinder_mesh_opt = mesh_registry.find(tempest::assets::builtin::cylinder_mesh_guid);
    ASSERT_TRUE(cylinder_mesh_opt.has_value());
    EXPECT_GT(cylinder_mesh_opt->vertices.size(), 0U);
    EXPECT_GT(cylinder_mesh_opt->indices.size(), 0U);
    EXPECT_TRUE(cylinder_mesh_opt->has_normals);
    EXPECT_TRUE(cylinder_mesh_opt->has_tangents);
    EXPECT_TRUE(cylinder_mesh_opt->validate());

    // Assert: Canonical textures are registered with valid mip data
    const auto missing_texture_opt = texture_registry.get_texture(tempest::assets::builtin::missing_texture_guid);
    ASSERT_TRUE(missing_texture_opt.has_value());
    EXPECT_EQ(missing_texture_opt->width, 2U);
    EXPECT_EQ(missing_texture_opt->height, 2U);
    ASSERT_FALSE(missing_texture_opt->mips.empty());
    EXPECT_EQ(missing_texture_opt->mips[0].data.size(), 16U);

    const auto white_texture_opt = texture_registry.get_texture(tempest::assets::builtin::white_texture_guid);
    ASSERT_TRUE(white_texture_opt.has_value());
    EXPECT_EQ(white_texture_opt->width, 1U);
    EXPECT_EQ(white_texture_opt->height, 1U);
    ASSERT_FALSE(white_texture_opt->mips.empty());
    EXPECT_EQ(white_texture_opt->mips[0].data.size(), 4U);

    const auto normal_flat_texture_opt =
        texture_registry.get_texture(tempest::assets::builtin::normal_flat_texture_guid);
    ASSERT_TRUE(normal_flat_texture_opt.has_value());
    EXPECT_EQ(normal_flat_texture_opt->width, 1U);
    EXPECT_EQ(normal_flat_texture_opt->height, 1U);
    ASSERT_FALSE(normal_flat_texture_opt->mips.empty());
    EXPECT_EQ(normal_flat_texture_opt->mips[0].data.size(), 4U);

    // Assert: Canonical materials are registered
    const auto default_material_opt = material_registry.find(tempest::assets::builtin::default_material_guid);
    ASSERT_TRUE(default_material_opt.has_value());
    EXPECT_EQ(default_material_opt->get_name(), "default");

    const auto missing_material_opt = material_registry.find(tempest::assets::builtin::missing_material_guid);
    ASSERT_TRUE(missing_material_opt.has_value());
    EXPECT_EQ(missing_material_opt->get_name(), "missing");

    // Assert: Built-in assets in database are non-transient
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::plane_mesh_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::box_mesh_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::sphere_mesh_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::capsule_mesh_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::cylinder_mesh_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::missing_texture_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::white_texture_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::normal_flat_texture_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::default_material_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::missing_material_guid));

    // Assert: Built-in paths resolve via database path lookup
    EXPECT_NE(database.find_asset("@builtin/meshes/plane"), nullptr);
    EXPECT_NE(database.find_asset("@builtin/meshes/box"), nullptr);
    EXPECT_NE(database.find_asset("@builtin/meshes/sphere"), nullptr);
    EXPECT_NE(database.find_asset("@builtin/meshes/capsule"), nullptr);
    EXPECT_NE(database.find_asset("@builtin/meshes/cylinder"), nullptr);
    EXPECT_NE(database.find_asset("@builtin/textures/missing"), nullptr);
    EXPECT_NE(database.find_asset("@builtin/textures/white"), nullptr);
    EXPECT_NE(database.find_asset("@builtin/textures/normal_flat"), nullptr);
    EXPECT_NE(database.find_asset("@builtin/materials/default"), nullptr);
    EXPECT_NE(database.find_asset("@builtin/materials/missing"), nullptr);
}

/// @brief Verify that built-in GUIDs are compile-time constants that resolve deterministically
/// without loading any project workspace or metadata files.
TEST(builtin_assets_tests, builtin_guids_resolve_without_project)
{
    // Setup & Act: Collect all built-in GUIDs
    const auto plane_guid = tempest::assets::builtin::plane_mesh_guid;
    const auto box_guid = tempest::assets::builtin::box_mesh_guid;
    const auto sphere_guid = tempest::assets::builtin::sphere_mesh_guid;
    const auto capsule_guid = tempest::assets::builtin::capsule_mesh_guid;
    const auto cylinder_guid = tempest::assets::builtin::cylinder_mesh_guid;
    const auto missing_tex_guid = tempest::assets::builtin::missing_texture_guid;
    const auto white_tex_guid = tempest::assets::builtin::white_texture_guid;
    const auto normal_flat_guid = tempest::assets::builtin::normal_flat_texture_guid;
    const auto default_mat_guid = tempest::assets::builtin::default_material_guid;
    const auto missing_mat_guid = tempest::assets::builtin::missing_material_guid;

    // Assert: Verify each GUID is non-zero and pairwise distinct
    const auto empty_guid = tempest::guid{};
    EXPECT_NE(plane_guid, empty_guid);
    EXPECT_NE(box_guid, empty_guid);
    EXPECT_NE(sphere_guid, empty_guid);
    EXPECT_NE(capsule_guid, empty_guid);
    EXPECT_NE(cylinder_guid, empty_guid);
    EXPECT_NE(missing_tex_guid, empty_guid);
    EXPECT_NE(white_tex_guid, empty_guid);
    EXPECT_NE(normal_flat_guid, empty_guid);
    EXPECT_NE(default_mat_guid, empty_guid);
    EXPECT_NE(missing_mat_guid, empty_guid);

    EXPECT_NE(plane_guid, box_guid);
    EXPECT_NE(sphere_guid, capsule_guid);
    EXPECT_NE(missing_tex_guid, white_tex_guid);
    EXPECT_NE(default_mat_guid, missing_mat_guid);
}

/// @brief Verify material_asset JSON serialization, file persistence, and bidirectional
/// conversion to and from core::material.
TEST(builtin_assets_tests, material_asset_round_trip)
{
    // Setup: Construct and populate an authorable material_asset
    auto source_material = tempest::assets::material_asset{};
    source_material.name = "PBR_Brushed_Copper";
    source_material.shading_model = "openpbr";
    source_material.base_color_factor = tempest::math::vec4<float>{0.95f, 0.64f, 0.54f, 1.0f};
    source_material.roughness_factor = 0.25f;
    source_material.metallic_factor = 0.9f;
    source_material.transmission_factor = 0.1f;
    source_material.emissive_factor = tempest::math::vec3<float>{0.1f, 0.05f, 0.0f};
    source_material.normal_scale = 1.2f;
    source_material.occlusion_strength = 0.8f;
    source_material.alpha_cutoff = 0.33f;
    source_material.alpha_mode = "blend";
    source_material.double_sided = true;

    source_material.base_color_map = tempest::guid::generate_random_guid();
    source_material.metallic_roughness_map = tempest::guid::generate_random_guid();
    source_material.normal_map = tempest::guid::generate_random_guid();
    source_material.occlusion_map = tempest::guid::generate_random_guid();
    source_material.emissive_map = tempest::guid::generate_random_guid();
    source_material.transmission_map = tempest::guid::generate_random_guid();

    source_material.custom_scalars.insert({"clearcoat_strength", 0.5f});
    source_material.custom_vec4s.insert({"subsurface_tint", tempest::math::vec4<float>{0.8f, 0.2f, 0.1f, 1.0f}});
    source_material.custom_textures.insert({"detail_normal_map", tempest::guid::generate_random_guid()});

    // Act & Assert Part A: JSON string round-trip
    const auto json_string = source_material.to_string();
    EXPECT_FALSE(json_string.empty());

    const auto loaded_from_string_opt = tempest::assets::material_asset::load_from_string(json_string);
    ASSERT_TRUE(loaded_from_string_opt.has_value());
    EXPECT_EQ(*loaded_from_string_opt, source_material);

    // Act & Assert Part B: File save and load round-trip
    const auto test_material_file = "test_material_roundtrip.tmaterial";
    ASSERT_TRUE(source_material.save_to_file(test_material_file));

    const auto loaded_from_file_opt = tempest::assets::material_asset::load_from_file(test_material_file);
    tempest::filesystem::remove(tempest::filesystem::path(test_material_file));
    ASSERT_TRUE(loaded_from_file_opt.has_value());
    EXPECT_EQ(*loaded_from_file_opt, source_material);

    // Act & Assert Part C: Conversion to and from core::material
    const auto core_mat = source_material.to_core_material();
    const auto converted_back = tempest::assets::material_asset::from_core_material(core_mat);

    EXPECT_EQ(converted_back.name, source_material.name);
    EXPECT_EQ(converted_back.shading_model, source_material.shading_model);
    EXPECT_FLOAT_EQ(converted_back.base_color_factor.x, source_material.base_color_factor.x);
    EXPECT_FLOAT_EQ(converted_back.base_color_factor.y, source_material.base_color_factor.y);
    EXPECT_FLOAT_EQ(converted_back.base_color_factor.z, source_material.base_color_factor.z);
    EXPECT_FLOAT_EQ(converted_back.base_color_factor.w, source_material.base_color_factor.w);
    EXPECT_FLOAT_EQ(converted_back.roughness_factor, source_material.roughness_factor);
    EXPECT_FLOAT_EQ(converted_back.metallic_factor, source_material.metallic_factor);
    EXPECT_FLOAT_EQ(converted_back.transmission_factor, source_material.transmission_factor);
    EXPECT_FLOAT_EQ(converted_back.emissive_factor.x, source_material.emissive_factor.x);
    EXPECT_FLOAT_EQ(converted_back.emissive_factor.y, source_material.emissive_factor.y);
    EXPECT_FLOAT_EQ(converted_back.emissive_factor.z, source_material.emissive_factor.z);
    EXPECT_FLOAT_EQ(converted_back.normal_scale, source_material.normal_scale);
    EXPECT_FLOAT_EQ(converted_back.occlusion_strength, source_material.occlusion_strength);
    EXPECT_FLOAT_EQ(converted_back.alpha_cutoff, source_material.alpha_cutoff);
    EXPECT_EQ(converted_back.alpha_mode, source_material.alpha_mode);
    EXPECT_EQ(converted_back.double_sided, source_material.double_sided);

    EXPECT_EQ(converted_back.base_color_map, source_material.base_color_map);
    EXPECT_EQ(converted_back.metallic_roughness_map, source_material.metallic_roughness_map);
    EXPECT_EQ(converted_back.normal_map, source_material.normal_map);
    EXPECT_EQ(converted_back.occlusion_map, source_material.occlusion_map);
    EXPECT_EQ(converted_back.emissive_map, source_material.emissive_map);
    EXPECT_EQ(converted_back.transmission_map, source_material.transmission_map);
}

/// @brief Verify transient asset detection tags runtime assets without sidecars as transient
/// while tagging built-ins and backed assets as non-transient.
TEST(builtin_assets_tests, transient_asset_detection)
{
    // Setup: Initialize asset database and mount built-ins
    auto database = tempest::assets::asset_database{};
    tempest::assets::builtin::register_builtin_assets(database);

    // Act: Register dynamic runtime assets without .tmeta sidecars
    const auto runtime_asset_type = tempest::assets::asset_type_id::from_hash(1001);
    const auto runtime_guid = database.register_asset(runtime_asset_type, "runtime/dynamic_mesh.obj");

    const auto explicit_guid = tempest::guid::generate_random_guid();
    const auto registered_explicit =
        database.register_asset_with_guid(explicit_guid, runtime_asset_type, "runtime/procedural_texture.png");
    ASSERT_TRUE(registered_explicit);

    // Assert: Runtime assets are detected as transient
    EXPECT_TRUE(database.is_transient_asset(runtime_guid));
    EXPECT_TRUE(database.is_transient_asset(explicit_guid));

    // Assert: Built-in assets are detected as non-transient
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::plane_mesh_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::box_mesh_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::sphere_mesh_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::capsule_mesh_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::cylinder_mesh_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::missing_texture_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::white_texture_guid));
    EXPECT_FALSE(database.is_transient_asset(tempest::assets::builtin::normal_flat_texture_guid));

    // Assert: Non-existent asset GUID returns false
    const auto non_existent_guid = tempest::guid::generate_random_guid();
    EXPECT_FALSE(database.is_transient_asset(non_existent_guid));
}
