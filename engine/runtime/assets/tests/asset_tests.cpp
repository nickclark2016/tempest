#include <gtest/gtest.h>

#include <tempest/asset_database.hpp>
#include <tempest/asset_serializers.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/default_importers.hpp>
#include <tempest/entity_hierarchy.hpp>
#include <tempest/event.hpp>
#include <tempest/guid.hpp>
#include <tempest/material.hpp>
#include <tempest/meta.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/serial.hpp>
#include <tempest/texture.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vertex.hpp>

#include "../src/importers/gltf_importer.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

// ============================================================================
// Test types for asset_type_id tests
// ============================================================================

namespace
{
    struct type_a
    {
        int value;
    };

    struct type_b
    {
        float value;
    };

    struct type_c
    {
        double value;
    };
} // namespace

// ============================================================================
// 1. asset_type_id Tests
// ============================================================================

TEST(asset_type_id, of_produces_different_hashes_for_different_types)
{
    auto id_a = tempest::assets::asset_type_id::of<type_a>();
    auto id_b = tempest::assets::asset_type_id::of<type_b>();
    auto id_c = tempest::assets::asset_type_id::of<type_c>();

    EXPECT_NE(id_a.hash(), id_b.hash());
    EXPECT_NE(id_a.hash(), id_c.hash());
    EXPECT_NE(id_b.hash(), id_c.hash());
}

TEST(asset_type_id, of_produces_same_hash_for_same_type)
{
    auto id_a1 = tempest::assets::asset_type_id::of<type_a>();
    auto id_a2 = tempest::assets::asset_type_id::of<type_a>();

    EXPECT_EQ(id_a1.hash(), id_a2.hash());
    EXPECT_EQ(id_a1, id_a2);
}

TEST(asset_type_id, from_hash_roundtrips)
{
    auto id_a = tempest::assets::asset_type_id::of<type_a>();
    auto reconstructed = tempest::assets::asset_type_id::from_hash(id_a.hash());

    EXPECT_EQ(id_a, reconstructed);
    EXPECT_EQ(id_a.hash(), reconstructed.hash());
}

TEST(asset_type_id, equality_operators)
{
    auto id_a = tempest::assets::asset_type_id::of<type_a>();
    auto id_b = tempest::assets::asset_type_id::of<type_b>();
    auto id_a_copy = tempest::assets::asset_type_id::of<type_a>();

    EXPECT_TRUE(id_a == id_a_copy);
    EXPECT_FALSE(id_a == id_b);
    EXPECT_TRUE(id_a != id_b);
    EXPECT_FALSE(id_a != id_a_copy);
}

// ============================================================================
// 2. asset_type_registry Tests
// ============================================================================

TEST(asset_type_registry, register_new_type_succeeds)
{
    tempest::assets::asset_type_registry registry;

    auto result = registry.register_type<type_a>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    EXPECT_TRUE(result);
}

TEST(asset_type_registry, register_same_type_twice_is_idempotent)
{
    tempest::assets::asset_type_registry registry;

    auto result1 = registry.register_type<type_a>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    auto result2 = registry.register_type<type_a>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
}

TEST(asset_type_registry, find_returns_correct_entry)
{
    tempest::assets::asset_type_registry registry;

    registry.register_type<type_a>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    auto type_id = tempest::assets::asset_type_id::of<type_a>();
    const auto* entry = registry.find(type_id);

    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->id, type_id);
}

TEST(asset_type_registry, find_returns_null_for_unregistered)
{
    tempest::assets::asset_type_registry registry;

    auto type_id = tempest::assets::asset_type_id::of<type_a>();
    const auto* entry = registry.find(type_id);

    EXPECT_EQ(entry, nullptr);
}

TEST(asset_type_registry, find_by_name_returns_correct_entry)
{
    tempest::assets::asset_type_registry registry;

    registry.register_type<type_a>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    auto type_id = tempest::assets::asset_type_id::of<type_a>();
    auto type_name = tempest::core::type_name<type_a>::value();
    const auto* entry = registry.find_by_name(type_name);

    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->id, type_id);
}

TEST(asset_type_registry, name_of_returns_correct_name)
{
    tempest::assets::asset_type_registry registry;

    registry.register_type<type_a>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    auto type_id = tempest::assets::asset_type_id::of<type_a>();
    auto name = registry.name_of(type_id);

    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(name.value(), tempest::core::type_name<type_a>::value());
}

TEST(asset_type_registry, validate_returns_true_for_matching_pair)
{
    tempest::assets::asset_type_registry registry;

    registry.register_type<type_a>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    auto type_id = tempest::assets::asset_type_id::of<type_a>();
    auto type_name = tempest::core::type_name<type_a>::value();
    auto result = registry.validate(type_id, type_name);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
}

TEST(asset_type_registry, validate_returns_false_for_mismatched_name)
{
    tempest::assets::asset_type_registry registry;

    registry.register_type<type_a>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    auto type_id = tempest::assets::asset_type_id::of<type_a>();
    auto result = registry.validate(type_id, "wrong_name");

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value());
}

TEST(asset_type_registry, validate_returns_nullopt_for_unknown_hash)
{
    tempest::assets::asset_type_registry registry;

    auto type_id = tempest::assets::asset_type_id::from_hash(999999);
    auto result = registry.validate(type_id, "unknown");

    EXPECT_FALSE(result.has_value());
}

TEST(asset_type_registry, references_remain_valid_after_additional_registrations)
{
    tempest::assets::asset_type_registry registry;

    registry.register_type<type_a>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    auto type_id_a = tempest::assets::asset_type_id::of<type_a>();
    const auto* entry_a = registry.find(type_id_a);
    ASSERT_NE(entry_a, nullptr);

    // Store pointer for later comparison
    const auto* entry_a_ptr = entry_a;

    // Register more types to potentially cause vector reallocation
    registry.register_type<type_b>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    registry.register_type<type_c>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    // The pointer should still be valid due to unique_ptr stability
    EXPECT_EQ(entry_a_ptr, registry.find(type_id_a));
    EXPECT_EQ(entry_a_ptr->id, type_id_a);
}

// ============================================================================
// 3. Serializer Specialization Tests
// ============================================================================

TEST(serializer_texture, roundtrip)
{
    tempest::serialization::binary_archive archive;

    tempest::core::texture tex;
    tex.width = 64;
    tex.height = 32;
    tex.format = tempest::core::texture_format::rgba8_srgb;
    tex.compression = tempest::core::texture_compression::none;
    tex.sampler.mag_filter = tempest::core::magnify_texture_filter::linear;
    tex.sampler.min_filter = tempest::core::minify_texture_filter::nearest_mipmap_linear;
    tex.sampler.wrap_s = tempest::core::texture_wrap_mode::repeat;
    tex.sampler.wrap_t = tempest::core::texture_wrap_mode::clamp_to_edge;
    tex.name = "test_texture";

    tempest::core::texture_mip_data mip;
    mip.width = 64;
    mip.height = 32;
    mip.data.resize(64 * 32 * 4);
    for (tempest::size_t idx = 0; idx < mip.data.size(); ++idx)
    {
        mip.data[idx] = static_cast<tempest::byte>(idx & 0xFF);
    }
    tex.mips.push_back(tempest::move(mip));

    tempest::serialization::serializer<tempest::serialization::binary_archive, tempest::core::texture>::serialize(
        archive, tex);
    auto result =
        tempest::serialization::serializer<tempest::serialization::binary_archive, tempest::core::texture>::deserialize(
            archive);

    EXPECT_EQ(result.width, tex.width);
    EXPECT_EQ(result.height, tex.height);
    EXPECT_EQ(result.format, tex.format);
    EXPECT_EQ(result.compression, tex.compression);
    EXPECT_EQ(result.sampler.mag_filter, tex.sampler.mag_filter);
    EXPECT_EQ(result.sampler.min_filter, tex.sampler.min_filter);
    EXPECT_EQ(result.sampler.wrap_s, tex.sampler.wrap_s);
    EXPECT_EQ(result.sampler.wrap_t, tex.sampler.wrap_t);
    EXPECT_EQ(result.name, tex.name);
    ASSERT_EQ(result.mips.size(), tex.mips.size());
    EXPECT_EQ(result.mips[0].width, tex.mips[0].width);
    EXPECT_EQ(result.mips[0].height, tex.mips[0].height);
    EXPECT_EQ(result.mips[0].data.size(), tex.mips[0].data.size());
}

TEST(serializer_mesh, roundtrip)
{
    tempest::serialization::binary_archive archive;

    tempest::core::mesh mesh;
    mesh.name = "test_mesh";
    mesh.has_normals = true;
    mesh.has_tangents = false;
    mesh.has_colors = true;

    tempest::core::vertex vert1;
    vert1.position = {1.0f, 2.0f, 3.0f};
    vert1.uv = {0.5f, 0.5f};
    vert1.normal = {0.0f, 1.0f, 0.0f};
    vert1.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
    vert1.color = {1.0f, 0.0f, 0.0f, 1.0f};
    mesh.vertices.push_back(vert1);

    tempest::core::vertex vert2;
    vert2.position = {4.0f, 5.0f, 6.0f};
    vert2.uv = {0.0f, 1.0f};
    vert2.normal = {0.0f, 0.0f, 1.0f};
    vert2.tangent = {0.0f, 1.0f, 0.0f, 1.0f};
    vert2.color = {0.0f, 1.0f, 0.0f, 1.0f};
    mesh.vertices.push_back(vert2);

    mesh.indices.push_back(0);
    mesh.indices.push_back(1);
    mesh.indices.push_back(0);

    tempest::serialization::serializer<tempest::serialization::binary_archive, tempest::core::mesh>::serialize(archive,
                                                                                                               mesh);
    auto result =
        tempest::serialization::serializer<tempest::serialization::binary_archive, tempest::core::mesh>::deserialize(
            archive);

    EXPECT_EQ(result.name, mesh.name);
    EXPECT_EQ(result.has_normals, mesh.has_normals);
    EXPECT_EQ(result.has_tangents, mesh.has_tangents);
    EXPECT_EQ(result.has_colors, mesh.has_colors);
    ASSERT_EQ(result.vertices.size(), mesh.vertices.size());
    EXPECT_FLOAT_EQ(result.vertices[0].position.x, mesh.vertices[0].position.x);
    EXPECT_FLOAT_EQ(result.vertices[0].position.y, mesh.vertices[0].position.y);
    EXPECT_FLOAT_EQ(result.vertices[0].position.z, mesh.vertices[0].position.z);
    ASSERT_EQ(result.indices.size(), mesh.indices.size());
    EXPECT_EQ(result.indices[0], mesh.indices[0]);
    EXPECT_EQ(result.indices[1], mesh.indices[1]);
    EXPECT_EQ(result.indices[2], mesh.indices[2]);
}

TEST(serializer_material, roundtrip)
{
    tempest::serialization::binary_archive archive;

    tempest::core::material mat;
    mat.set_name("test_material");
    mat.set_vec4(tempest::core::material::base_color_factor_name, tempest::math::vec4<float>{1.0f, 0.5f, 0.25f, 1.0f});
    mat.set_scalar(tempest::core::material::metallic_factor_name, 0.8f);
    mat.set_scalar(tempest::core::material::roughness_factor_name, 0.4f);
    mat.set_bool(tempest::core::material::double_sided_name, true);
    mat.set_vec3(tempest::core::material::emissive_factor_name, tempest::math::vec3<float>{0.1f, 0.2f, 0.3f});
    mat.set_string(tempest::core::material::alpha_mode_name, "OPAQUE");

    tempest::serialization::serializer<tempest::serialization::binary_archive, tempest::core::material>::serialize(
        archive, mat);
    auto result = tempest::serialization::serializer<tempest::serialization::binary_archive,
                                                     tempest::core::material>::deserialize(archive);

    EXPECT_EQ(result.get_name(), "test_material");

    auto bcf = result.get_vec4(tempest::core::material::base_color_factor_name);
    ASSERT_TRUE(bcf.has_value());
    EXPECT_FLOAT_EQ(bcf.value().x, 1.0f);
    EXPECT_FLOAT_EQ(bcf.value().y, 0.5f);
    EXPECT_FLOAT_EQ(bcf.value().z, 0.25f);
    EXPECT_FLOAT_EQ(bcf.value().w, 1.0f);

    auto metallic = result.get_scalar(tempest::core::material::metallic_factor_name);
    ASSERT_TRUE(metallic.has_value());
    EXPECT_FLOAT_EQ(metallic.value(), 0.8f);

    auto roughness = result.get_scalar(tempest::core::material::roughness_factor_name);
    ASSERT_TRUE(roughness.has_value());
    EXPECT_FLOAT_EQ(roughness.value(), 0.4f);

    auto double_sided = result.get_bool(tempest::core::material::double_sided_name);
    ASSERT_TRUE(double_sided.has_value());
    EXPECT_TRUE(double_sided.value());

    auto emissive = result.get_vec3(tempest::core::material::emissive_factor_name);
    ASSERT_TRUE(emissive.has_value());
    EXPECT_FLOAT_EQ(emissive.value().x, 0.1f);
    EXPECT_FLOAT_EQ(emissive.value().y, 0.2f);
    EXPECT_FLOAT_EQ(emissive.value().z, 0.3f);

    auto alpha_mode = result.get_string(tempest::core::material::alpha_mode_name);
    ASSERT_TRUE(alpha_mode.has_value());
    EXPECT_EQ(alpha_mode.value(), "OPAQUE");
}

TEST(serializer_entity_hierarchy, roundtrip)
{
    tempest::serialization::binary_archive archive;

    tempest::assets::entity_hierarchy hierarchy;
    hierarchy.root_index = 0;

    // Create root record
    tempest::assets::entity_hierarchy::entity_record root_record;
    root_record.child_indices.push_back(1);
    root_record.child_indices.push_back(2);

    // Add a component to root
    tempest::vector<tempest::byte> comp_data;
    comp_data.push_back(static_cast<tempest::byte>(42));
    comp_data.push_back(static_cast<tempest::byte>(43));
    root_record.components.push_back(
        tempest::pair<tempest::size_t, tempest::vector<tempest::byte>>{12345, tempest::move(comp_data)});

    hierarchy.records.push_back(tempest::move(root_record));

    // Create child records
    tempest::assets::entity_hierarchy::entity_record child1;
    hierarchy.records.push_back(tempest::move(child1));

    tempest::assets::entity_hierarchy::entity_record child2;
    hierarchy.records.push_back(tempest::move(child2));

    tempest::serialization::serializer<tempest::serialization::binary_archive,
                                       tempest::assets::entity_hierarchy>::serialize(archive, hierarchy);
    auto result = tempest::serialization::serializer<tempest::serialization::binary_archive,
                                                     tempest::assets::entity_hierarchy>::deserialize(archive);

    EXPECT_EQ(result.root_index, hierarchy.root_index);
    ASSERT_EQ(result.records.size(), hierarchy.records.size());
    EXPECT_EQ(result.records[0].child_indices.size(), static_cast<tempest::size_t>(2));
    EXPECT_EQ(result.records[0].child_indices[0], static_cast<tempest::size_t>(1));
    EXPECT_EQ(result.records[0].child_indices[1], static_cast<tempest::size_t>(2));
    ASSERT_EQ(result.records[0].components.size(), static_cast<tempest::size_t>(1));
    EXPECT_EQ(result.records[0].components[0].first, static_cast<tempest::size_t>(12345));
    EXPECT_EQ(result.records[0].components[0].second.size(), static_cast<tempest::size_t>(2));
}

// ============================================================================
// 4. asset_database Persistence Tests
// ============================================================================

namespace
{
    const char* test_db_path = "test_asset_database.tassetdb";

    void cleanup_test_db()
    {
        std::remove(test_db_path);
    }
} // namespace

TEST(asset_database, open_nonexistent_file_produces_empty_database)
{
    cleanup_test_db();

    tempest::assets::asset_type_registry type_reg;
    tempest::assets::asset_database database(&type_reg);

    // Opening a nonexistent file should not error
    database.open("nonexistent_file.tassetdb");

    // Database should be empty
    auto guid_result = database.find_by_guid(tempest::guid::generate_random_guid());
    EXPECT_EQ(guid_result, nullptr);

    auto path_result = database.find_by_path("some/path.gltf");
    EXPECT_EQ(path_result, nullptr);
}

TEST(asset_database, save_then_open_roundtrips)
{
    cleanup_test_db();

    // Register a type
    tempest::assets::asset_type_registry type_reg;
    type_reg.register_type<type_a>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    // Create and populate database
    tempest::assets::asset_database database(&type_reg);
    database.open(test_db_path);

    auto asset_id = database.register_asset(tempest::assets::asset_type_id::of<type_a>(), "test/source_file.gltf");

    // Store a blob
    tempest::vector<tempest::byte> blob;
    blob.push_back(static_cast<tempest::byte>(1));
    blob.push_back(static_cast<tempest::byte>(2));
    blob.push_back(static_cast<tempest::byte>(3));
    database.store_blob(asset_id, tempest::span<const tempest::byte>{blob.data(), blob.size()});

    // Save
    bool save_result = database.save();
    EXPECT_TRUE(save_result);

    // Open in a new database
    tempest::assets::asset_database database2(&type_reg);
    database2.open(test_db_path);

    // Verify the asset was loaded
    const auto* entry = database2.find_by_guid(asset_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->id, asset_id);
    EXPECT_EQ(entry->type, tempest::assets::asset_type_id::of<type_a>());

    // Verify the blob was loaded
    auto loaded_blob = database2.get_blob(asset_id);
    ASSERT_EQ(loaded_blob.size(), static_cast<tempest::size_t>(3));
    EXPECT_EQ(loaded_blob[0], static_cast<tempest::byte>(1));
    EXPECT_EQ(loaded_blob[1], static_cast<tempest::byte>(2));
    EXPECT_EQ(loaded_blob[2], static_cast<tempest::byte>(3));

    cleanup_test_db();
}

TEST(asset_database, find_by_path_returns_correct_result)
{
    cleanup_test_db();

    tempest::assets::asset_type_registry type_reg;
    type_reg.register_type<type_a>(
        [](tempest::span<const tempest::byte>, const tempest::guid&, tempest::assets::asset_database&) { return true; },
        [](const tempest::guid&, const tempest::assets::asset_database&, tempest::vector<tempest::byte>&) {
            return true;
        });

    tempest::assets::asset_database database(&type_reg);
    database.open(test_db_path);

    auto asset_id = database.register_asset(tempest::assets::asset_type_id::of<type_a>(), "test/my_model.gltf");

    const auto* entry = database.find_by_path("test/my_model.gltf");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->id, asset_id);

    // Non-existent path returns nullptr
    const auto* no_entry = database.find_by_path("test/does_not_exist.gltf");
    EXPECT_EQ(no_entry, nullptr);

    cleanup_test_db();
}

// ============================================================================
// 5. asset_database::load() Unified Path Tests
// ============================================================================

struct fake_single_asset
{
    int value;
};

struct fake_texture
{
    int width;
    int height;
};

struct fake_mesh
{
    int vertex_count;
};

struct fake_material
{
    float roughness;
};

namespace
{
    // A simple mock importer that registers a single asset with a blob.
    class mock_importer : public tempest::assets::asset_importer
    {
      public:
        int import_call_count{0};
        tempest::guid last_registered_asset_id{};

        // Override the path-based overload so it doesn't try to read a file from disk.
        [[nodiscard]] tempest::ecs::entity import(tempest::assets::asset_database& asset_db, tempest::string_view path,
                                                  tempest::ecs::archetype_registry& registry) override
        {
            return import(asset_db, tempest::span<const tempest::byte>{}, registry, tempest::some(path));
        }

        [[nodiscard]] tempest::ecs::entity import(tempest::assets::asset_database& asset_db,
                                                  tempest::span<const tempest::byte> data,
                                                  tempest::ecs::archetype_registry& registry,
                                                  tempest::optional<tempest::string_view> path) override
        {
            (void)data;
            ++import_call_count;

            // Register an asset and store a blob, just like a real importer would.
            auto source = path.has_value() ? path.value() : tempest::string_view("unknown");
            auto asset_id = asset_db.register_asset(tempest::assets::asset_type_id::of<fake_single_asset>(), source);
            last_registered_asset_id = asset_id;

            tempest::vector<tempest::byte> blob;
            blob.push_back(static_cast<tempest::byte>(0xAB));
            blob.push_back(static_cast<tempest::byte>(0xCD));
            asset_db.store_blob(asset_id, tempest::span<const tempest::byte>{blob.data(), blob.size()});

            auto ent = registry.create<>();
            if (path.has_value())
            {
                registry.name(ent, path.value());
            }
            return ent;
        }
    };

    // A mock importer that produces multiple assets from a single source file,
    // similar to how a GLTF importer produces textures, meshes, and materials.
    class multi_asset_importer : public tempest::assets::asset_importer
    {
      public:
        int import_call_count{0};
        tempest::vector<tempest::guid> produced_texture_ids;
        tempest::vector<tempest::guid> produced_mesh_ids;
        tempest::vector<tempest::guid> produced_material_ids;

        // Override the path-based overload so it doesn't try to read a file from disk.
        [[nodiscard]] tempest::ecs::entity import(tempest::assets::asset_database& asset_db, tempest::string_view path,
                                                  tempest::ecs::archetype_registry& registry) override
        {
            return import(asset_db, tempest::span<const tempest::byte>{}, registry, tempest::some(path));
        }

        [[nodiscard]] tempest::ecs::entity import(tempest::assets::asset_database& asset_db,
                                                  tempest::span<const tempest::byte> data,
                                                  tempest::ecs::archetype_registry& registry,
                                                  tempest::optional<tempest::string_view> path) override
        {
            (void)data;
            ++import_call_count;

            auto source = path.has_value() ? path.value() : tempest::string_view("unknown");

            // Register 2 textures
            for (int i = 0; i < 2; ++i)
            {
                auto tex_id = asset_db.register_asset(tempest::assets::asset_type_id::of<fake_texture>(), source);
                fake_texture tex{.width = 64 * (i + 1), .height = 32 * (i + 1)};
                auto tex_bytes =
                    tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(&tex), sizeof(tex)};
                asset_db.store_blob(tex_id, tex_bytes);
                produced_texture_ids.push_back(tex_id);
            }

            // Register 1 mesh
            {
                auto mesh_id = asset_db.register_asset(tempest::assets::asset_type_id::of<fake_mesh>(), source);
                fake_mesh mesh{.vertex_count = 1024};
                auto mesh_bytes =
                    tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(&mesh), sizeof(mesh)};
                asset_db.store_blob(mesh_id, mesh_bytes);
                produced_mesh_ids.push_back(mesh_id);
            }

            // Register 1 material
            {
                auto mat_id = asset_db.register_asset(tempest::assets::asset_type_id::of<fake_material>(), source);
                fake_material mat{.roughness = 0.42f};
                auto mat_bytes =
                    tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(&mat), sizeof(mat)};
                asset_db.store_blob(mat_id, mat_bytes);
                produced_material_ids.push_back(mat_id);
            }

            // Build a small entity hierarchy: root -> [mesh_node, material_node]
            auto root = registry.create<>();
            (void)registry.create<>(); // mesh_node
            (void)registry.create<>(); // material_node

            if (path.has_value())
            {
                registry.name(root, path.value());
            }

            return root;
        }
    };
} // namespace

TEST(asset_database_load, falls_back_to_importer_when_not_cached)
{
    cleanup_test_db();

    tempest::assets::asset_type_registry type_reg;
    type_reg.register_type<fake_single_asset>(nullptr, nullptr);

    tempest::assets::asset_database database(&type_reg);

    auto* mock_ptr = new mock_importer();
    database.register_importer(tempest::unique_ptr<tempest::assets::asset_importer>(mock_ptr), ".mock");

    auto events = tempest::event::event_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events);

    auto result = database.load("test_file.mock", reg);

    EXPECT_TRUE(result != tempest::ecs::tombstone);
    EXPECT_EQ(mock_ptr->import_call_count, 1);

    // The importer should have registered an asset that is now tracked.
    const auto* entry = database.find_by_path("test_file.mock");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->type, tempest::assets::asset_type_id::of<fake_single_asset>());
}

TEST(asset_database_load, returns_tombstone_for_unknown_extension)
{
    tempest::assets::asset_type_registry type_reg;
    tempest::assets::asset_database database(&type_reg);

    auto events = tempest::event::event_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events);

    auto result = database.load("test_file.unknown", reg);

    EXPECT_TRUE(result == tempest::ecs::tombstone);
}

TEST(asset_database_load, cached_asset_resolves_from_blobs)
{
    cleanup_test_db();

    tempest::assets::asset_type_registry type_reg;
    type_reg.register_type<fake_single_asset>(nullptr, nullptr);

    tempest::guid saved_asset_id{};

    // First pass: import and save
    {
        tempest::assets::asset_database database(&type_reg);

        auto* mock_ptr = new mock_importer();
        database.register_importer(tempest::unique_ptr<tempest::assets::asset_importer>(mock_ptr), ".mock");

        database.open(test_db_path);

        auto events = tempest::event::event_registry();
        auto reg = tempest::ecs::basic_archetype_registry(events);

        // First load triggers import — the mock importer registers the asset itself.
        auto result1 = database.load("test_file.mock", reg);
        EXPECT_TRUE(result1 != tempest::ecs::tombstone);
        EXPECT_EQ(mock_ptr->import_call_count, 1);

        // Verify the importer tracked the asset in the database.
        const auto* entry = database.find_by_path("test_file.mock");
        ASSERT_NE(entry, nullptr);
        saved_asset_id = entry->id;

        (void)database.save();
    }

    // Second pass: load from saved database
    {
        tempest::assets::asset_database database(&type_reg);

        auto* mock_ptr = new mock_importer();
        database.register_importer(tempest::unique_ptr<tempest::assets::asset_importer>(mock_ptr), ".mock");

        database.open(test_db_path);

        auto events = tempest::event::event_registry();
        auto reg = tempest::ecs::basic_archetype_registry(events);

        // Second load should resolve from blobs (source found in database).
        auto result2 = database.load("test_file.mock", reg);
        EXPECT_TRUE(result2 != tempest::ecs::tombstone);

        // The importer should NOT have been called.
        EXPECT_EQ(mock_ptr->import_call_count, 0);

        // The asset should still be discoverable by GUID.
        const auto* entry = database.find_by_guid(saved_asset_id);
        ASSERT_NE(entry, nullptr);
        EXPECT_EQ(entry->type, tempest::assets::asset_type_id::of<fake_single_asset>());

        // The blob should have been preserved across the roundtrip.
        auto blob = database.get_blob(saved_asset_id);
        ASSERT_EQ(blob.size(), static_cast<tempest::size_t>(2));
        EXPECT_EQ(blob[0], static_cast<tempest::byte>(0xAB));
        EXPECT_EQ(blob[1], static_cast<tempest::byte>(0xCD));
    }

    cleanup_test_db();
}

// ============================================================================
// 6. Multi-asset import tests (GLTF-like)
// ============================================================================

TEST(asset_database_load, multi_asset_import_registers_all_assets)
{
    cleanup_test_db();

    tempest::assets::asset_type_registry type_reg;
    type_reg.register_type<fake_texture>(nullptr, nullptr);
    type_reg.register_type<fake_mesh>(nullptr, nullptr);
    type_reg.register_type<fake_material>(nullptr, nullptr);

    tempest::assets::asset_database database(&type_reg);

    auto* importer_ptr = new multi_asset_importer();
    database.register_importer(tempest::unique_ptr<tempest::assets::asset_importer>(importer_ptr), ".multi");

    auto events = tempest::event::event_registry();
    auto reg = tempest::ecs::basic_archetype_registry(events);

    auto result = database.load("scene.multi", reg);
    EXPECT_TRUE(result != tempest::ecs::tombstone);
    EXPECT_EQ(importer_ptr->import_call_count, 1);

    // The importer should have registered 2 textures + 1 mesh + 1 material = 4 assets.
    ASSERT_EQ(importer_ptr->produced_texture_ids.size(), static_cast<tempest::size_t>(2));
    ASSERT_EQ(importer_ptr->produced_mesh_ids.size(), static_cast<tempest::size_t>(1));
    ASSERT_EQ(importer_ptr->produced_material_ids.size(), static_cast<tempest::size_t>(1));

    // All assets should be findable by GUID.
    for (const auto& tex_id : importer_ptr->produced_texture_ids)
    {
        const auto* entry = database.find_by_guid(tex_id);
        ASSERT_NE(entry, nullptr);
        EXPECT_EQ(entry->type, tempest::assets::asset_type_id::of<fake_texture>());
    }

    const auto* mesh_entry = database.find_by_guid(importer_ptr->produced_mesh_ids[0]);
    ASSERT_NE(mesh_entry, nullptr);
    EXPECT_EQ(mesh_entry->type, tempest::assets::asset_type_id::of<fake_mesh>());

    const auto* mat_entry = database.find_by_guid(importer_ptr->produced_material_ids[0]);
    ASSERT_NE(mat_entry, nullptr);
    EXPECT_EQ(mat_entry->type, tempest::assets::asset_type_id::of<fake_material>());

    // Verify a mesh blob roundtrips correctly.
    auto mesh_blob = database.get_blob(importer_ptr->produced_mesh_ids[0]);
    ASSERT_EQ(mesh_blob.size(), sizeof(fake_mesh));
    fake_mesh loaded_mesh;
    tempest::memcpy(&loaded_mesh, mesh_blob.data(), sizeof(fake_mesh));
    EXPECT_EQ(loaded_mesh.vertex_count, 1024);
}

TEST(asset_database_load, multi_asset_import_roundtrips_through_save_and_open)
{
    cleanup_test_db();

    tempest::assets::asset_type_registry type_reg;
    type_reg.register_type<fake_texture>(nullptr, nullptr);
    type_reg.register_type<fake_mesh>(nullptr, nullptr);
    type_reg.register_type<fake_material>(nullptr, nullptr);

    tempest::vector<tempest::guid> texture_ids;
    tempest::guid mesh_id{};
    tempest::guid material_id{};

    // First pass: import and save.
    {
        tempest::assets::asset_database database(&type_reg);

        auto* importer_ptr = new multi_asset_importer();
        database.register_importer(tempest::unique_ptr<tempest::assets::asset_importer>(importer_ptr), ".multi");

        database.open(test_db_path);

        auto events = tempest::event::event_registry();
        auto reg = tempest::ecs::basic_archetype_registry(events);
        auto result = database.load("scene.multi", reg);
        EXPECT_TRUE(result != tempest::ecs::tombstone);
        EXPECT_EQ(importer_ptr->import_call_count, 1);

        // Stash the IDs so we can verify them after the roundtrip.
        texture_ids = importer_ptr->produced_texture_ids;
        mesh_id = importer_ptr->produced_mesh_ids[0];
        material_id = importer_ptr->produced_material_ids[0];

        bool saved = database.save();
        EXPECT_TRUE(saved);
    }

    // Second pass: reopen and verify all assets are cached.
    {
        tempest::assets::asset_database database(&type_reg);

        auto* importer_ptr = new multi_asset_importer();
        database.register_importer(tempest::unique_ptr<tempest::assets::asset_importer>(importer_ptr), ".multi");

        database.open(test_db_path);

        auto events = tempest::event::event_registry();
        auto reg = tempest::ecs::basic_archetype_registry(events);
        auto result = database.load("scene.multi", reg);
        EXPECT_TRUE(result != tempest::ecs::tombstone);

        // The importer should NOT have been called.
        EXPECT_EQ(importer_ptr->import_call_count, 0);

        // All 4 assets (2 tex + 1 mesh + 1 mat) should still exist.
        for (const auto& tex_id : texture_ids)
        {
            const auto* entry = database.find_by_guid(tex_id);
            ASSERT_NE(entry, nullptr);
            EXPECT_EQ(entry->type, tempest::assets::asset_type_id::of<fake_texture>());

            auto blob = database.get_blob(tex_id);
            EXPECT_EQ(blob.size(), sizeof(fake_texture));
        }

        // Verify mesh blob data survived the roundtrip.
        auto mesh_blob = database.get_blob(mesh_id);
        ASSERT_EQ(mesh_blob.size(), sizeof(fake_mesh));
        fake_mesh loaded_mesh;
        tempest::memcpy(&loaded_mesh, mesh_blob.data(), sizeof(fake_mesh));
        EXPECT_EQ(loaded_mesh.vertex_count, 1024);

        // Verify material blob data survived the roundtrip.
        auto mat_blob = database.get_blob(material_id);
        ASSERT_EQ(mat_blob.size(), sizeof(fake_material));
        fake_material loaded_mat;
        tempest::memcpy(&loaded_mat, mat_blob.data(), sizeof(fake_material));
        EXPECT_FLOAT_EQ(loaded_mat.roughness, 0.42f);

        // Verify texture blob data survived the roundtrip.
        auto tex0_blob = database.get_blob(texture_ids[0]);
        ASSERT_EQ(tex0_blob.size(), sizeof(fake_texture));
        fake_texture loaded_tex0;
        tempest::memcpy(&loaded_tex0, tex0_blob.data(), sizeof(fake_texture));
        EXPECT_EQ(loaded_tex0.width, 64);
        EXPECT_EQ(loaded_tex0.height, 32);

        auto tex1_blob = database.get_blob(texture_ids[1]);
        ASSERT_EQ(tex1_blob.size(), sizeof(fake_texture));
        fake_texture loaded_tex1;
        tempest::memcpy(&loaded_tex1, tex1_blob.data(), sizeof(fake_texture));
        EXPECT_EQ(loaded_tex1.width, 128);
        EXPECT_EQ(loaded_tex1.height, 64);
    }

    cleanup_test_db();
}

// ============================================================================
// 9. asset_database Mount, Scan, Index, and Source Resolution Tests
// ============================================================================

/// @brief Verifies that mount roots are maintained in descending priority order and can be unmounted or cleared.
TEST(asset_database_mount_and_scan, mount_roots_priority_ordering)
{
    // 1. Setup
    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount roots with mixed priorities
    database.mount_root("assets/engine", 0);
    database.mount_root("projects/game/assets", 10);
    database.mount_root("fallback/assets", -5);
    database.mount_root("plugins/custom/assets", 5);

    // 3. Assert: Order should be descending by priority: 10, 5, 0, -5
    auto roots = database.get_mount_roots();
    ASSERT_EQ(roots.size(), 4U);
    EXPECT_EQ(roots[0].priority, 10);
    EXPECT_EQ(roots[0].path, "projects/game/assets");
    EXPECT_EQ(roots[1].priority, 5);
    EXPECT_EQ(roots[1].path, "plugins/custom/assets");
    EXPECT_EQ(roots[2].priority, 0);
    EXPECT_EQ(roots[2].path, "assets/engine");
    EXPECT_EQ(roots[3].priority, -5);
    EXPECT_EQ(roots[3].path, "fallback/assets");

    // 4. Act & Assert: Unmount and clear
    database.unmount_root("assets/engine");
    roots = database.get_mount_roots();
    EXPECT_EQ(roots.size(), 3U);

    database.clear_mount_roots();
    roots = database.get_mount_roots();
    EXPECT_EQ(roots.size(), 0U);
}

/// @brief Verifies that scan_and_index discovers shaders in mounted directories, indexes normalized relative paths, and
/// populates basename fallbacks.
TEST(asset_database_mount_and_scan, scan_and_index_discovers_shaders_and_basenames)
{
    // 1. Setup: Create temporary mock mount directory with nested shaders
    const auto temp_mount = std::filesystem::path("tempest_test_mount_scan");
    std::filesystem::create_directories(temp_mount / "shaders" / "engine");
    std::filesystem::create_directories(temp_mount / "shaders" / "post");

    {
        auto f1 = std::ofstream(temp_mount / "shaders" / "engine" / "pbr.vert.spv", std::ios::binary);
        f1 << "mock_pbr_vert_bytecode_12345";
        auto f2 = std::ofstream(temp_mount / "shaders" / "engine" / "pbr.frag.spv", std::ios::binary);
        f2 << "mock_pbr_frag_bytecode_67890";
        auto f3 = std::ofstream(temp_mount / "shaders" / "post" / "bloom.comp.spv", std::ios::binary);
        f3 << "mock_bloom_comp_bytecode_abcde";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount directory and scan
    database.mount_root(temp_mount.generic_string().c_str(), 0);
    database.scan_and_index();

    // 3. Assert: Exact relative path lookup
    const auto* pbr_vert_exact = database.find_asset("shaders/engine/pbr.vert.spv");
    ASSERT_NE(pbr_vert_exact, nullptr);
    EXPECT_EQ(pbr_vert_exact->type, tempest::assets::asset_type_id::of<tempest::assets::shader_asset>());

    // 4. Assert: Basename fallback lookup
    const auto* pbr_vert_base = database.find_asset("pbr.vert.spv");
    ASSERT_NE(pbr_vert_base, nullptr);
    EXPECT_EQ(pbr_vert_base->id, pbr_vert_exact->id);

    const auto* bloom_comp = database.find_asset("bloom.comp.spv");
    ASSERT_NE(bloom_comp, nullptr);

    // 5. Assert: On-demand bytecode reading from disk
    auto bytes = database.get_blob(pbr_vert_exact->id);
    EXPECT_FALSE(bytes.empty());
    EXPECT_EQ(bytes.size(), sizeof("mock_pbr_vert_bytecode_12345") - 1);

    // 6. Teardown
    std::filesystem::remove_all(temp_mount);
}

/// @brief Verifies that higher-priority mount roots override assets with the same basename from lower-priority roots.
TEST(asset_database_mount_and_scan, priority_override_precedence)
{
    // 1. Setup: Create engine and project mock folders with duplicate shader names
    const auto engine_mount = std::filesystem::path("tempest_test_engine_root");
    const auto project_mount = std::filesystem::path("tempest_test_project_root");
    std::filesystem::create_directories(engine_mount / "shaders");
    std::filesystem::create_directories(project_mount / "shaders");

    {
        auto f_engine = std::ofstream(engine_mount / "shaders" / "lighting.frag.spv", std::ios::binary);
        f_engine << "ENGINE_DEFAULT_LIGHTING";
        auto f_project = std::ofstream(project_mount / "shaders" / "lighting.frag.spv", std::ios::binary);
        f_project << "PROJECT_CUSTOM_LIGHTING_OVERRIDE";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount engine root at 0 and project root at 10
    database.mount_root(engine_mount.generic_string().c_str(), 0);
    database.mount_root(project_mount.generic_string().c_str(), 10);
    database.scan_and_index();

    // 3. Assert: Basename lookup resolves to project override
    const auto* asset = database.find_asset("lighting.frag.spv");
    ASSERT_NE(asset, nullptr);

    auto blob = database.get_blob(asset->id);
    ASSERT_FALSE(blob.empty());
    auto content = std::string(reinterpret_cast<const char*>(blob.data()), blob.size());
    EXPECT_EQ(content, "PROJECT_CUSTOM_LIGHTING_OVERRIDE");

    // 4. Teardown
    std::filesystem::remove_all(engine_mount);
    std::filesystem::remove_all(project_mount);
}

/// @brief Verifies that resolve_source_path maps compiled .spv files back to their original .slang source files.
TEST(asset_database_mount_and_scan, resolve_source_path_maps_spv_to_slang)
{
    // 1. Setup: Create temporary mock folder with .spv and corresponding .slang
    const auto temp_mount = std::filesystem::path("tempest_test_source_map");
    std::filesystem::create_directories(temp_mount / "shaders");

    {
        auto f1 = std::ofstream(temp_mount / "shaders" / "tonemap.vert.spv", std::ios::binary);
        f1 << "tonemap_vs";
        auto f2 = std::ofstream(temp_mount / "shaders" / "tonemap.slang");
        f2 << "// tonemap slang source";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};
    database.mount_root(temp_mount.generic_string().c_str(), 0);
    database.scan_and_index();

    // 2. Act: Resolve source path
    auto src_opt = database.resolve_source_path("shaders/tonemap.vert.spv");

    // 3. Assert: Correctly resolves to tonemap.slang
    ASSERT_TRUE(src_opt.has_value());
    EXPECT_EQ(*src_opt, "tonemap.slang");

    // 4. Teardown
    std::filesystem::remove_all(temp_mount);
}

/// @brief Verifies that notify_file_changed detects source modifications and invalidates cached blobs for
/// hot-reloading.
TEST(asset_database_mount_and_scan, notify_file_changed_invalidates_cache)
{
    // 1. Setup: Create mock shader and index it
    const auto temp_mount = std::filesystem::path("tempest_test_hotreload");
    std::filesystem::create_directories(temp_mount / "shaders");
    const auto shader_path = temp_mount / "shaders" / "reloaded.frag.spv";

    {
        auto f = std::ofstream(shader_path, std::ios::binary);
        f << "INITIAL_BYTECODE";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};
    database.mount_root(temp_mount.generic_string().c_str(), 0);
    database.scan_and_index();

    const auto* asset = database.find_asset("reloaded.frag.spv");
    ASSERT_NE(asset, nullptr);
    const auto asset_id = asset->id;

    // Load initial bytes into cache
    auto initial_blob = database.get_blob(asset_id);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(initial_blob.data()), initial_blob.size()), "INITIAL_BYTECODE");

    // 2. Act: Modify file on disk and trigger notify_file_changed
    {
        auto f = std::ofstream(shader_path, std::ios::binary);
        f << "UPDATED_HOT_RELOADED_BYTECODE";
    }

    bool notified = database.notify_file_changed("shaders/reloaded.frag.spv");
    EXPECT_TRUE(notified);

    // 3. Assert: Subsequent get_blob reloads updated bytes
    auto reloaded_blob = database.get_blob(asset_id);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(reloaded_blob.data()), reloaded_blob.size()),
              "UPDATED_HOT_RELOADED_BYTECODE");

    // 4. Teardown
    std::filesystem::remove_all(temp_mount);
}

/// @brief Verifies edge cases in path normalization: backslashes, leading/trailing slashes, and ./ prefixes.
TEST(asset_database_mount_and_scan, path_normalization_edge_cases)
{
    // 1. Setup: Create test folder with trailing slash in mount path
    const auto temp_mount = std::filesystem::path("tempest_test_norm_edge");
    std::filesystem::create_directories(temp_mount / "shaders" / "edge");
    {
        auto f = std::ofstream(temp_mount / "shaders" / "edge" / "test.vert.spv", std::ios::binary);
        f << "NORM_EDGE_TEST";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount with trailing slash and redundant dot-slashes
    auto mount_with_slashes = temp_mount.generic_string() + "/";
    database.mount_root(mount_with_slashes.c_str(), 0);
    database.scan_and_index();

    // 3. Assert: Queries with backslashes, leading slash, and ./ prefix all resolve identically
    const auto* standard = database.find_asset("shaders/edge/test.vert.spv");
    ASSERT_NE(standard, nullptr);

    const auto* backslash = database.find_asset("shaders\\edge\\test.vert.spv");
    ASSERT_NE(backslash, nullptr);
    EXPECT_EQ(backslash->id, standard->id);

    const auto* leading_slash = database.find_asset("/shaders/edge/test.vert.spv");
    ASSERT_NE(leading_slash, nullptr);
    EXPECT_EQ(leading_slash->id, standard->id);

    const auto* dot_slash = database.find_asset("./shaders/edge/test.vert.spv");
    ASSERT_NE(dot_slash, nullptr);
    EXPECT_EQ(dot_slash->id, standard->id);

    // 4. Teardown
    std::filesystem::remove_all(temp_mount);
}

/// @brief Verifies that duplicate basenames in different subdirectories of the same mount root are uniquely addressable
/// via exact relative paths.
TEST(asset_database_mount_and_scan, duplicate_basename_different_subdirectories)
{
    // 1. Setup: Create pass_a and pass_b subdirectories with identical shader file names
    const auto temp_mount = std::filesystem::path("tempest_test_dup_basename");
    std::filesystem::create_directories(temp_mount / "shaders" / "pass_a");
    std::filesystem::create_directories(temp_mount / "shaders" / "pass_b");

    {
        auto f_a = std::ofstream(temp_mount / "shaders" / "pass_a" / "main.frag.spv", std::ios::binary);
        f_a << "PASS_A_BYTECODE";
        auto f_b = std::ofstream(temp_mount / "shaders" / "pass_b" / "main.frag.spv", std::ios::binary);
        f_b << "PASS_B_BYTECODE";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};
    database.mount_root(temp_mount.generic_string().c_str(), 0);
    database.scan_and_index();

    // 2. Act: Query exact relative paths for both
    const auto* asset_a = database.find_asset("shaders/pass_a/main.frag.spv");
    const auto* asset_b = database.find_asset("shaders/pass_b/main.frag.spv");

    // 3. Assert: Both assets exist and are distinct
    ASSERT_NE(asset_a, nullptr);
    ASSERT_NE(asset_b, nullptr);
    EXPECT_NE(asset_a->id, asset_b->id);

    auto blob_a = database.get_blob(asset_a->id);
    auto blob_b = database.get_blob(asset_b->id);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(blob_a.data()), blob_a.size()), "PASS_A_BYTECODE");
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(blob_b.data()), blob_b.size()), "PASS_B_BYTECODE");

    // 4. Teardown
    std::filesystem::remove_all(temp_mount);
}

/// @brief Verifies that on-demand asset lookup locates and registers a file on disk even without an explicit
/// scan_and_index call.
TEST(asset_database_mount_and_scan, on_demand_lookup_without_explicit_scan)
{
    // 1. Setup: Create test folder with a shader file
    const auto temp_mount = std::filesystem::path("tempest_test_ondemand");
    std::filesystem::create_directories(temp_mount / "shaders" / "ondemand");
    {
        auto f = std::ofstream(temp_mount / "shaders" / "ondemand" / "lazy.comp.spv", std::ios::binary);
        f << "LAZY_DISCOVERED_BYTECODE";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount root, but do NOT call scan_and_index()
    database.mount_root(temp_mount.generic_string().c_str(), 0);

    // 3. Assert: find_asset dynamically resolves the file on disk
    const auto* asset = database.find_asset("shaders/ondemand/lazy.comp.spv");
    ASSERT_NE(asset, nullptr);

    auto blob = database.get_blob(asset->id);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(blob.data()), blob.size()), "LAZY_DISCOVERED_BYTECODE");

    // 4. Teardown
    std::filesystem::remove_all(temp_mount);
}

/// @brief Verifies that remounting an existing root with a new priority updates the priority and re-sorts the mount
/// list.
TEST(asset_database_mount_and_scan, mount_root_priority_update)
{
    // 1. Setup
    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    database.mount_root("root_a", 0);
    database.mount_root("root_b", 5);

    // Initial order: root_b (5), root_a (0)
    auto roots = database.get_mount_roots();
    ASSERT_EQ(roots.size(), 2U);
    EXPECT_EQ(roots[0].path, "root_b");
    EXPECT_EQ(roots[1].path, "root_a");

    // 2. Act: Update root_a to priority 10
    database.mount_root("root_a", 10);

    // 3. Assert: Order is now root_a (10), root_b (5)
    roots = database.get_mount_roots();
    ASSERT_EQ(roots.size(), 2U);
    EXPECT_EQ(roots[0].path, "root_a");
    EXPECT_EQ(roots[0].priority, 10);
    EXPECT_EQ(roots[1].path, "root_b");
    EXPECT_EQ(roots[1].priority, 5);
}

/// @brief Verifies that non-existent mount roots and invalid/empty path queries fail gracefully without crashing.
TEST(asset_database_mount_and_scan, empty_and_invalid_path_handling)
{
    // 1. Setup: Mount non-existent folder
    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    database.mount_root("non_existent_folder_xyz123", 0);

    // 2. Act: scan_and_index on non-existent folder does not throw or crash
    database.scan_and_index();

    // 3. Assert: Empty and non-existent queries return nullptr / nullopt
    EXPECT_EQ(database.find_asset(""), nullptr);
    EXPECT_EQ(database.find_asset("non_existent_file.spv"), nullptr);
    EXPECT_FALSE(database.resolve_source_path("").has_value());
    EXPECT_FALSE(database.resolve_disk_path("non_existent_file.spv").has_value());
    EXPECT_FALSE(database.notify_file_changed("non_existent_file.spv"));
}

/// @brief Verifies that small assets pack into shared 64KB chunks while oversized assets allocate dedicated chunks
/// without pointer invalidation.
TEST(asset_database_mount_and_scan, large_and_small_assets_chunk_arena)
{
    // 1. Setup
    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // Register 10 small assets (each 8 KB)
    auto small_ids = tempest::vector<tempest::guid>{};
    for (int i = 0; i < 10; ++i)
    {
        auto name = std::string("small_asset_") + std::to_string(i) + ".bin";
        auto id = database.register_asset(tempest::assets::asset_type_id::from_hash(100 + i), name.c_str());
        small_ids.push_back(id);

        auto payload = std::vector<tempest::byte>(8 * 1024, static_cast<tempest::byte>(i + 1));
        database.store_blob(id, tempest::span<const tempest::byte>{payload.data(), payload.size()});
    }

    // Capture pointers of small assets
    auto first_small_blob = database.get_blob(small_ids[0]);
    ASSERT_EQ(first_small_blob.size(), 8 * 1024U);
    const auto* initial_first_ptr = first_small_blob.data();

    // 2. Act: Register an oversized asset (128 KB > 64 KB default chunk size)
    auto large_id = database.register_asset(tempest::assets::asset_type_id::from_hash(999), "large_asset.bin");
    auto large_payload = std::vector<tempest::byte>(128 * 1024, static_cast<tempest::byte>(0xAA));
    database.store_blob(large_id, tempest::span<const tempest::byte>{large_payload.data(), large_payload.size()});

    // 3. Assert: Memory stability - previous small blob span pointer remains unchanged!
    auto post_alloc_small_blob = database.get_blob(small_ids[0]);
    EXPECT_EQ(post_alloc_small_blob.data(), initial_first_ptr);
    EXPECT_EQ(post_alloc_small_blob[0], static_cast<tempest::byte>(1));

    // Assert: Large blob is correct
    auto large_blob = database.get_blob(large_id);
    EXPECT_EQ(large_blob.size(), 128 * 1024U);
    EXPECT_EQ(large_blob[0], static_cast<tempest::byte>(0xAA));
    EXPECT_EQ(large_blob[128 * 1024 - 1], static_cast<tempest::byte>(0xAA));
}

/// @brief Verifies that runtime chunks are completely coalesced into a single contiguous block when saved to .tassetdb
/// and reopened.
TEST(asset_database_mount_and_scan, chunks_coalesce_on_save_and_reopen)
{
    const auto test_db_path = std::filesystem::path("tempest_test_coalesce.tassetdb");
    if (std::filesystem::exists(test_db_path))
    {
        std::filesystem::remove(test_db_path);
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto id_small = tempest::guid{};
    auto id_large = tempest::guid{};

    // 1. Setup & Act: Create database with multiple mixed-size chunks and save to disk
    {
        auto database = tempest::assets::asset_database{&type_reg};
        database.open(test_db_path.generic_string().c_str());

        id_small = database.register_asset(tempest::assets::asset_type_id::from_hash(1), "chunked_small.spv");
        auto small_bytes = std::vector<tempest::byte>(4096, static_cast<tempest::byte>(0x11));
        database.store_blob(id_small, tempest::span<const tempest::byte>{small_bytes.data(), small_bytes.size()});

        id_large = database.register_asset(tempest::assets::asset_type_id::from_hash(2), "chunked_large.bin");
        auto large_bytes = std::vector<tempest::byte>(96 * 1024, static_cast<tempest::byte>(0x22));
        database.store_blob(id_large, tempest::span<const tempest::byte>{large_bytes.data(), large_bytes.size()});

        bool saved = database.save();
        EXPECT_TRUE(saved);
    }

    // 2. Act: Reopen from .tassetdb
    {
        auto database = tempest::assets::asset_database{&type_reg};
        database.open(test_db_path.generic_string().c_str());

        // 3. Assert: All assets are present, contiguous, and intact
        const auto* entry_small = database.find_by_guid(id_small);
        const auto* entry_large = database.find_by_guid(id_large);
        ASSERT_NE(entry_small, nullptr);
        ASSERT_NE(entry_large, nullptr);

        // Coalesced contiguous offsets check
        EXPECT_EQ(entry_small->blob_offset, 0U);
        EXPECT_EQ(entry_small->blob_size, 4096U);
        EXPECT_EQ(entry_large->blob_offset, 4096U);
        EXPECT_EQ(entry_large->blob_size, 96 * 1024U);

        auto blob_small = database.get_blob(id_small);
        auto blob_large = database.get_blob(id_large);
        EXPECT_EQ(blob_small.size(), 4096U);
        EXPECT_EQ(blob_small[0], static_cast<tempest::byte>(0x11));
        EXPECT_EQ(blob_large.size(), 96 * 1024U);
        EXPECT_EQ(blob_large[0], static_cast<tempest::byte>(0x22));
    }

    // 4. Teardown
    if (std::filesystem::exists(test_db_path))
    {
        std::filesystem::remove(test_db_path);
    }
}

/// @brief Verifies mutation/hot-reload of an asset loaded from .tassetdb and subsequent re-coalescing on save.
TEST(asset_database_mount_and_scan, mutation_and_hot_reload_coalescing)
{
    const auto test_db_path = std::filesystem::path("tempest_test_mutate_coalesce.tassetdb");
    if (std::filesystem::exists(test_db_path))
    {
        std::filesystem::remove(test_db_path);
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto shader_id = tempest::guid{};
    auto mesh_id = tempest::guid{};

    // 1. Setup: Create and save initial database
    {
        auto database = tempest::assets::asset_database{&type_reg};
        database.open(test_db_path.generic_string().c_str());

        shader_id = database.register_asset(tempest::assets::asset_type_id::from_hash(10), "pbr.frag.spv");
        auto initial_shader = std::string("INITIAL_SHADER_V1");
        database.store_blob(
            shader_id, tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(initial_shader.data()),
                                                          initial_shader.size()});

        mesh_id = database.register_asset(tempest::assets::asset_type_id::from_hash(20), "cube.mesh");
        auto mesh_data = std::string("CUBE_MESH_DATA_UNTOUCHED");
        database.store_blob(mesh_id, tempest::span<const tempest::byte>{
                                         reinterpret_cast<const tempest::byte*>(mesh_data.data()), mesh_data.size()});

        EXPECT_TRUE(database.save());
    }

    // 2. Act: Reopen and mutate shader_id (hot-reload simulation)
    {
        auto database = tempest::assets::asset_database{&type_reg};
        database.open(test_db_path.generic_string().c_str());

        // Mutate shader blob
        auto updated_shader = std::string("RELOADED_SHADER_V2_LONGER_PAYLOAD_HERE");
        database.store_blob(
            shader_id, tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(updated_shader.data()),
                                                          updated_shader.size()});

        // Verify in-memory updated content and untouched mesh content
        auto cur_shader_blob = database.get_blob(shader_id);
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(cur_shader_blob.data()), cur_shader_blob.size()),
                  "RELOADED_SHADER_V2_LONGER_PAYLOAD_HERE");

        auto cur_mesh_blob = database.get_blob(mesh_id);
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(cur_mesh_blob.data()), cur_mesh_blob.size()),
                  "CUBE_MESH_DATA_UNTOUCHED");

        // Save mutated database
        EXPECT_TRUE(database.save());
    }

    // 3. Assert: Reopen and verify new mutated data is cleanly coalesced
    {
        auto database = tempest::assets::asset_database{&type_reg};
        database.open(test_db_path.generic_string().c_str());

        auto final_shader_blob = database.get_blob(shader_id);
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(final_shader_blob.data()), final_shader_blob.size()),
                  "RELOADED_SHADER_V2_LONGER_PAYLOAD_HERE");

        auto final_mesh_blob = database.get_blob(mesh_id);
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(final_mesh_blob.data()), final_mesh_blob.size()),
                  "CUBE_MESH_DATA_UNTOUCHED");
    }

    // 4. Teardown
    if (std::filesystem::exists(test_db_path))
    {
        std::filesystem::remove(test_db_path);
    }
}

// ============================================================================
// 10. glTF Hierarchy & Transform Invariants
// ============================================================================

/// @brief Tests that glTF importing does not leave orphan template mesh primitives in the registry.
TEST(gltf_importer_tests, no_orphan_template_primitives_created)
{
    // 1. Setup: Create binary buffer with 3 vertices and 3 indices
    const auto bin_file_path = std::filesystem::path("tempest_test_instanced_mesh.bin");
    {
        auto f = std::ofstream(bin_file_path, std::ios::binary);
        float verts[9] = {
            0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
        };
        uint16_t indices[3] = {0, 1, 2};
        f.write(reinterpret_cast<const char*>(verts), sizeof(verts));
        f.write(reinterpret_cast<const char*>(indices), sizeof(indices));
    }

    const auto test_file_path = std::filesystem::path("tempest_test_instanced_mesh.gltf");
    {
        auto f = std::ofstream(test_file_path, std::ios::binary);
        f << R"({
        "asset": { "version": "2.0" },
        "buffers": [
            { "byteLength": 42, "uri": "tempest_test_instanced_mesh.bin" }
        ],
        "bufferViews": [
            { "buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962 },
            { "buffer": 0, "byteOffset": 36, "byteLength": 6, "target": 34963 }
        ],
        "accessors": [
            { "bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 3, "type": "VEC3", "max": [1.0, 1.0, 0.0], "min": [0.0, 0.0, 0.0] },
            { "bufferView": 1, "byteOffset": 0, "componentType": 5123, "count": 3, "type": "SCALAR", "max": [2], "min": [0] }
        ],
        "meshes": [
            {
                "primitives": [
                    { "attributes": { "POSITION": 0 }, "indices": 1 }
                ]
            }
        ],
        "nodes": [
            { "mesh": 0, "translation": [10.0, 0.0, 0.0] },
            { "mesh": 0, "translation": [-10.0, 0.0, 0.0] }
        ],
        "scenes": [
            { "nodes": [0, 1] }
        ],
        "scene": 0
    })";
    }

    auto mesh_reg = tempest::core::mesh_registry{};
    auto tex_reg = tempest::core::texture_registry{};
    auto mat_reg = tempest::core::material_registry{};
    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};
    tempest::assets::register_default_importers(database, &mesh_reg, &tex_reg, &mat_reg);

    auto events = tempest::event::event_registry{};
    auto registry = tempest::ecs::basic_archetype_registry{events};

    // 2. Act: Import the glTF content via asset_database
    auto root = database.load(test_file_path.generic_string().c_str(), registry);

    EXPECT_TRUE(root != tempest::ecs::tombstone);

    // 3. Assert: Exactly 2 mesh components exist in the registry (one for each node instance), NO orphan template
    // primitives
    auto mesh_count = 0U;
    registry.each([&](const tempest::core::mesh_component&) { ++mesh_count; });

    EXPECT_EQ(mesh_count, 2U);

    // Root should have 2 children (the 2 nodes)
    const auto* root_rel = registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(root);
    ASSERT_NE(root_rel, nullptr);
    EXPECT_TRUE(root_rel->first_child != tempest::ecs::tombstone);

    auto node_count = 0U;
    auto child = root_rel->first_child;
    while (child != tempest::ecs::tombstone)
    {
        ++node_count;
        const auto* child_rel = registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(child);
        ASSERT_NE(child_rel, nullptr);
        EXPECT_EQ(child_rel->parent, root);

        // Each node should have 1 child mesh primitive
        EXPECT_TRUE(child_rel->first_child != tempest::ecs::tombstone);
        const auto* mesh_rel =
            registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(child_rel->first_child);
        ASSERT_NE(mesh_rel, nullptr);
        EXPECT_EQ(mesh_rel->parent, child);
        EXPECT_TRUE(registry.has<tempest::core::mesh_component>(child_rel->first_child));

        child = child_rel->next_sibling;
    }
    EXPECT_EQ(node_count, 2U);

    // 4. Teardown
    if (std::filesystem::exists(test_file_path))
    {
        std::filesystem::remove(test_file_path);
    }
    if (std::filesystem::exists(bin_file_path))
    {
        std::filesystem::remove(bin_file_path);
    }
}

/// @brief Tests that meshes imported from glTF and saved to asset database retain byte-exact vertex and index data on
/// reload.
TEST(gltf_importer_tests, gltf_database_save_and_reload_mesh_integrity)
{
    const auto db_path = std::filesystem::path("tempest_test_integrity.tassetdb");
    const auto bin_file_path = std::filesystem::path("tempest_test_mesh_integrity.bin");
    const auto gltf_file_path = std::filesystem::path("tempest_test_mesh_integrity.gltf");

    if (std::filesystem::exists(db_path))
    {
        std::filesystem::remove(db_path);
    }

    // 1. Setup: Create glTF with 3 vertices and 3 indices with distinct position, normal, uv, tangent
    {
        auto f = std::ofstream(bin_file_path, std::ios::binary);
        float pos[9] = {1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F};
        float norm[9] = {0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F};
        float uvs[6] = {0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F};
        uint16_t idx[3] = {0, 1, 2};
        f.write(reinterpret_cast<const char*>(pos), sizeof(pos));
        f.write(reinterpret_cast<const char*>(norm), sizeof(norm));
        f.write(reinterpret_cast<const char*>(uvs), sizeof(uvs));
        f.write(reinterpret_cast<const char*>(idx), sizeof(idx));
    }

    {
        auto f = std::ofstream(gltf_file_path, std::ios::binary);
        f << R"({
        "asset": { "version": "2.0" },
        "buffers": [
            { "byteLength": 102, "uri": "tempest_test_mesh_integrity.bin" }
        ],
        "bufferViews": [
            { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
            { "buffer": 0, "byteOffset": 36, "byteLength": 36 },
            { "buffer": 0, "byteOffset": 72, "byteLength": 24 },
            { "buffer": 0, "byteOffset": 96, "byteLength": 6 }
        ],
        "accessors": [
            { "bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 3, "type": "VEC3" },
            { "bufferView": 1, "byteOffset": 0, "componentType": 5126, "count": 3, "type": "VEC3" },
            { "bufferView": 2, "byteOffset": 0, "componentType": 5126, "count": 3, "type": "VEC2" },
            { "bufferView": 3, "byteOffset": 0, "componentType": 5123, "count": 3, "type": "SCALAR" }
        ],
        "meshes": [
            {
                "primitives": [
                    { "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 }, "indices": 3 }
                ]
            }
        ],
        "nodes": [
            { "mesh": 0 }
        ],
        "scenes": [
            { "nodes": [0] }
        ],
        "scene": 0
    })";
    }

    // 2. Act 1: Import fresh and save to database
    auto orig_mesh_guid = tempest::guid{};
    auto orig_mesh = tempest::core::mesh{};
    {
        auto mesh_reg = tempest::core::mesh_registry{};
        auto tex_reg = tempest::core::texture_registry{};
        auto mat_reg = tempest::core::material_registry{};
        auto type_reg = tempest::assets::asset_type_registry{};
        auto database = tempest::assets::asset_database{&type_reg};
        tempest::assets::register_default_importers(database, &mesh_reg, &tex_reg, &mat_reg);
        database.open(db_path.generic_string().c_str());

        auto events = tempest::event::event_registry{};
        auto registry = tempest::ecs::basic_archetype_registry{events};
        auto root = database.load(gltf_file_path.generic_string().c_str(), registry);
        EXPECT_TRUE(root != tempest::ecs::tombstone);

        // Find the mesh in registry
        for (auto it = mesh_reg.begin(); it != mesh_reg.end(); ++it)
        {
            orig_mesh_guid = it->first;
            orig_mesh = it->second;
            break;
        }

        EXPECT_TRUE(orig_mesh_guid != tempest::guid{});
        EXPECT_EQ(orig_mesh.vertices.size(), 3U);
        EXPECT_EQ(orig_mesh.indices.size(), 3U);

        auto saved = database.save();
        EXPECT_TRUE(saved);
    }

    // 3. Act 2: Re-open the database from scratch and load from blobs
    {
        auto mesh_reg = tempest::core::mesh_registry{};
        auto tex_reg = tempest::core::texture_registry{};
        auto mat_reg = tempest::core::material_registry{};
        auto type_reg = tempest::assets::asset_type_registry{};
        auto database = tempest::assets::asset_database{&type_reg};
        tempest::assets::register_default_importers(database, &mesh_reg, &tex_reg, &mat_reg);
        database.open(db_path.generic_string().c_str());

        auto events = tempest::event::event_registry{};
        auto registry = tempest::ecs::basic_archetype_registry{events};
        auto root = database.load(gltf_file_path.generic_string().c_str(), registry);
        EXPECT_TRUE(root != tempest::ecs::tombstone);

        auto reloaded_mesh_opt = mesh_reg.find(orig_mesh_guid);
        ASSERT_TRUE(reloaded_mesh_opt.has_value());
        const auto& reloaded_mesh = reloaded_mesh_opt.value();

        EXPECT_EQ(reloaded_mesh.vertices.size(), orig_mesh.vertices.size());
        EXPECT_EQ(reloaded_mesh.indices.size(), orig_mesh.indices.size());
        EXPECT_EQ(reloaded_mesh.has_normals, orig_mesh.has_normals);
        EXPECT_EQ(reloaded_mesh.has_tangents, orig_mesh.has_tangents);

        for (size_t i = 0; i < orig_mesh.vertices.size(); ++i)
        {
            EXPECT_FLOAT_EQ(reloaded_mesh.vertices[i].position.x, orig_mesh.vertices[i].position.x);
            EXPECT_FLOAT_EQ(reloaded_mesh.vertices[i].position.y, orig_mesh.vertices[i].position.y);
            EXPECT_FLOAT_EQ(reloaded_mesh.vertices[i].position.z, orig_mesh.vertices[i].position.z);

            EXPECT_FLOAT_EQ(reloaded_mesh.vertices[i].normal.x, orig_mesh.vertices[i].normal.x);
            EXPECT_FLOAT_EQ(reloaded_mesh.vertices[i].normal.y, orig_mesh.vertices[i].normal.y);
            EXPECT_FLOAT_EQ(reloaded_mesh.vertices[i].normal.z, orig_mesh.vertices[i].normal.z);

            EXPECT_FLOAT_EQ(reloaded_mesh.vertices[i].uv.x, orig_mesh.vertices[i].uv.x);
            EXPECT_FLOAT_EQ(reloaded_mesh.vertices[i].uv.y, orig_mesh.vertices[i].uv.y);
        }

        for (size_t i = 0; i < orig_mesh.indices.size(); ++i)
        {
            EXPECT_EQ(reloaded_mesh.indices[i], orig_mesh.indices[i]);
        }
    }

    // 4. Teardown
    if (std::filesystem::exists(db_path))
    {
        std::filesystem::remove(db_path);
    }
    if (std::filesystem::exists(gltf_file_path))
    {
        std::filesystem::remove(gltf_file_path);
    }
    if (std::filesystem::exists(bin_file_path))
    {
        std::filesystem::remove(bin_file_path);
    }
}

/// @brief Tests that Sponza glTF saves and reloads from asset database with complete hierarchy and mesh integrity.
TEST(gltf_importer_tests, sponza_database_save_and_reload_integrity)
{
    const auto db_path = std::filesystem::path("sponza_test.tassetdb");
    const auto sponza_path = "assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf";

    if (std::filesystem::exists(db_path))
    {
        std::filesystem::remove(db_path);
    }

    auto orig_meshes = tempest::flat_unordered_map<tempest::guid, tempest::core::mesh>{};
    auto orig_mesh_count = 0U;

    // 1. Act 1: Import Sponza and save database
    {
        auto mesh_reg = tempest::core::mesh_registry{};
        auto tex_reg = tempest::core::texture_registry{};
        auto mat_reg = tempest::core::material_registry{};
        auto type_reg = tempest::assets::asset_type_registry{};
        auto database = tempest::assets::asset_database{&type_reg};
        tempest::assets::register_default_importers(database, &mesh_reg, &tex_reg, &mat_reg);
        database.open(db_path.generic_string().c_str());

        auto events = tempest::event::event_registry{};
        auto registry = tempest::ecs::basic_archetype_registry{events};
        auto root = database.load(sponza_path, registry);
        ASSERT_TRUE(root != tempest::ecs::tombstone);

        for (auto it = mesh_reg.begin(); it != mesh_reg.end(); ++it)
        {
            orig_meshes[it->first] = it->second;
            ++orig_mesh_count;
        }

        EXPECT_GT(orig_mesh_count, 0U);

        auto saved = database.save();
        EXPECT_TRUE(saved);
    }

    // 2. Act 2: Re-open and reload from tassetdb
    {
        auto mesh_reg = tempest::core::mesh_registry{};
        auto tex_reg = tempest::core::texture_registry{};
        auto mat_reg = tempest::core::material_registry{};
        auto type_reg = tempest::assets::asset_type_registry{};
        auto database = tempest::assets::asset_database{&type_reg};
        tempest::assets::register_default_importers(database, &mesh_reg, &tex_reg, &mat_reg);
        database.open(db_path.generic_string().c_str());

        auto events = tempest::event::event_registry{};
        auto registry = tempest::ecs::basic_archetype_registry{events};
        auto root = database.load(sponza_path, registry);
        ASSERT_TRUE(root != tempest::ecs::tombstone);

        auto reloaded_mesh_count = 0U;
        for (auto it = mesh_reg.begin(); it != mesh_reg.end(); ++it)
        {
            ++reloaded_mesh_count;
            auto orig_it = orig_meshes.find(it->first);
            ASSERT_TRUE(orig_it != orig_meshes.end());
            const auto& orig_m = orig_it->second;
            const auto& reloaded_m = it->second;

            EXPECT_EQ(reloaded_m.vertices.size(), orig_m.vertices.size());
            EXPECT_EQ(reloaded_m.indices.size(), orig_m.indices.size());
            EXPECT_EQ(reloaded_m.has_normals, orig_m.has_normals);
            EXPECT_EQ(reloaded_m.has_tangents, orig_m.has_tangents);

            for (size_t i = 0; i < orig_m.vertices.size(); ++i)
            {
                EXPECT_FLOAT_EQ(reloaded_m.vertices[i].position.x, orig_m.vertices[i].position.x);
                EXPECT_FLOAT_EQ(reloaded_m.vertices[i].position.y, orig_m.vertices[i].position.y);
                EXPECT_FLOAT_EQ(reloaded_m.vertices[i].position.z, orig_m.vertices[i].position.z);
            }
        }

        EXPECT_EQ(reloaded_mesh_count, orig_mesh_count);
    }

    // 3. Teardown
    if (std::filesystem::exists(db_path))
    {
        std::filesystem::remove(db_path);
    }
}

// ============================================================================
// 11. Grouped Mount Aliases & Strict Isolation Tests
// ============================================================================

/// @brief Tests that multiple mount roots sharing the same alias resolve candidates in priority order.
TEST(asset_database_mount_aliases, grouped_mount_alias_priority_resolution)
{
    // 1. Setup: Create directory hierarchies for high-priority and low-priority roots
    auto root_high = std::filesystem::path("temp_test_alias_high");
    auto root_low = std::filesystem::path("temp_test_alias_low");
    std::filesystem::create_directories(root_high);
    std::filesystem::create_directories(root_low);

    {
        std::ofstream fh((root_high / "common.slang").string().c_str());
        fh << "// High priority common";
    }
    {
        std::ofstream fl((root_low / "common.slang").string().c_str());
        fl << "// Low priority common";
    }
    {
        std::ofstream fb((root_low / "unique_low.slang").string().c_str());
        fb << "// Unique low";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount both roots under alias "shaders" with distinct priorities and scan
    database.mount_root(root_high.generic_string().c_str(), 10, "shaders");
    database.mount_root(root_low.generic_string().c_str(), 5, "shaders");
    database.scan_and_index();

    // 3. Assert: Resolution checks
    auto common_disk = database.resolve_disk_path("@shaders/common.slang");
    ASSERT_TRUE(common_disk.has_value());
    EXPECT_NE(std::string_view(common_disk->c_str()).find("temp_test_alias_high"), std::string_view::npos);

    auto unique_disk = database.resolve_disk_path("@shaders/unique_low.slang");
    ASSERT_TRUE(unique_disk.has_value());
    EXPECT_NE(std::string_view(unique_disk->c_str()).find("temp_test_alias_low"), std::string_view::npos);

    const auto* common_asset = database.find_asset("@shaders/common.slang");
    ASSERT_NE(common_asset, nullptr);

    const auto* unique_asset = database.find_asset("@shaders/unique_low.slang");
    ASSERT_NE(unique_asset, nullptr);

    auto common_src = database.resolve_source_path("@shaders/common.slang");
    ASSERT_TRUE(common_src.has_value());
    EXPECT_EQ(common_src.value(), "@shaders/common.slang");

    // 4. Teardown
    std::filesystem::remove_all(root_high);
    std::filesystem::remove_all(root_low);
}

/// @brief Tests that @alias/ queries enforce strict isolation and do not fall back to other roots.
TEST(asset_database_mount_aliases, strict_alias_isolation)
{
    // 1. Setup: Create two separate roots (one unaliased, one under "textures")
    auto root_unaliased = std::filesystem::path("temp_test_iso_unaliased");
    auto root_textures = std::filesystem::path("temp_test_iso_textures");
    std::filesystem::create_directories(root_unaliased);
    std::filesystem::create_directories(root_textures);

    {
        std::ofstream f1((root_unaliased / "secret.txt").string().c_str());
        f1 << "unaliased secret";
    }
    {
        std::ofstream f2((root_textures / "diffuse.png").string().c_str());
        f2 << "texture data";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount roots and scan
    database.mount_root(root_unaliased.generic_string().c_str(), 10, "");
    database.mount_root(root_textures.generic_string().c_str(), 10, "textures");
    database.scan_and_index();

    // 3. Assert: Querying @shaders for files that only exist in other roots must return nullopt/nullptr
    EXPECT_FALSE(database.resolve_disk_path("@shaders/secret.txt").has_value());
    EXPECT_FALSE(database.resolve_disk_path("@shaders/diffuse.png").has_value());
    EXPECT_EQ(database.find_asset("@shaders/secret.txt"), nullptr);
    EXPECT_EQ(database.find_asset("@shaders/diffuse.png"), nullptr);
    EXPECT_FALSE(database.resolve_source_path("@shaders/secret.txt").has_value());

    // Querying existing textures under @textures succeeds
    EXPECT_TRUE(database.resolve_disk_path("@textures/diffuse.png").has_value());

    // 4. Teardown
    std::filesystem::remove_all(root_unaliased);
    std::filesystem::remove_all(root_textures);
}

/// @brief Tests case-sensitive matching for mount aliases.
TEST(asset_database_mount_aliases, case_sensitive_alias_matching)
{
    // 1. Setup: Create mount directory with shader
    auto root_case = std::filesystem::path("temp_test_case_alias");
    std::filesystem::create_directories(root_case);

    {
        std::ofstream f((root_case / "pass.slang").string().c_str());
        f << "// Case shader";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount with alias "Shaders" (capital S)
    database.mount_root(root_case.generic_string().c_str(), 10, "Shaders");
    database.scan_and_index();

    // 3. Assert: Exact case matches; mismatched case fails under strict isolation
    EXPECT_TRUE(database.resolve_disk_path("@Shaders/pass.slang").has_value());
    EXPECT_FALSE(database.resolve_disk_path("@shaders/pass.slang").has_value());
    EXPECT_NE(database.find_asset("@Shaders/pass.slang"), nullptr);
    EXPECT_EQ(database.find_asset("@shaders/pass.slang"), nullptr);

    // 4. Teardown
    std::filesystem::remove_all(root_case);
}

/// @brief Tests path normalization with aliases across forward-slash and backslash representations.
TEST(asset_database_mount_aliases, path_normalization_with_aliases)
{
    // 1. Setup: Create nested subdirectories
    auto root_norm = std::filesystem::path("temp_test_alias_norm");
    std::filesystem::create_directories(root_norm / "sub" / "nested");

    {
        std::ofstream f((root_norm / "sub" / "nested" / "effect.slang").string().c_str());
        f << "// Nested effect";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount and scan
    database.mount_root(root_norm.generic_string().c_str(), 10, "core");
    database.scan_and_index();

    // 3. Assert: Querying with backslashes and redundant prefixes resolves cleanly
    auto disk_forward = database.resolve_disk_path("@core/sub/nested/effect.slang");
    auto disk_backward = database.resolve_disk_path("@core\\sub\\nested\\effect.slang");
    ASSERT_TRUE(disk_forward.has_value());
    ASSERT_TRUE(disk_backward.has_value());
    EXPECT_EQ(disk_forward.value(), disk_backward.value());

    EXPECT_NE(database.find_asset("@core\\sub\\nested\\effect.slang"), nullptr);

    // 4. Teardown
    std::filesystem::remove_all(root_norm);
}

/// @brief Tests that non-aliased fallback lookups resolve assets indexed inside aliased roots by basename.
TEST(asset_database_mount_aliases, non_aliased_fallback_to_basenames)
{
    // 1. Setup: Create an aliased file
    auto root_base = std::filesystem::path("temp_test_alias_base");
    std::filesystem::create_directories(root_base / "pipelines");

    {
        std::ofstream f((root_base / "pipelines" / "pbr_deferred.slang").string().c_str());
        f << "// PBR deferred";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount under alias and scan
    database.mount_root(root_base.generic_string().c_str(), 10, "render");
    database.scan_and_index();

    // 3. Assert: Querying by bare filename finds the asset via basename index
    const auto* asset_by_base = database.find_asset("pbr_deferred.slang");
    ASSERT_NE(asset_by_base, nullptr);

    const auto* asset_by_alias = database.find_asset("@render/pipelines/pbr_deferred.slang");
    ASSERT_NE(asset_by_alias, nullptr);
    EXPECT_EQ(asset_by_base->id, asset_by_alias->id);

    // 4. Teardown
    std::filesystem::remove_all(root_base);
}

// ============================================================================
// 12. Directory Ignores, Whitelists & Predicates Tests
// ============================================================================

/// @brief Tests that default ignored directories (.git, build, bin, .agents, etc.) are skipped during scan.
TEST(asset_database_ignores_and_predicates, default_ignored_directories_skipped)
{
    // 1. Setup: Create hierarchy containing default ignored directories
    auto root_ignores = std::filesystem::path("temp_test_def_ignores");
    std::filesystem::create_directories(root_ignores / ".git");
    std::filesystem::create_directories(root_ignores / "build");
    std::filesystem::create_directories(root_ignores / "bin");
    std::filesystem::create_directories(root_ignores / ".agents");
    std::filesystem::create_directories(root_ignores / "src");

    {
        std::ofstream f1((root_ignores / ".git" / "ignored.slang").string().c_str());
        f1 << "git";
        std::ofstream f2((root_ignores / "build" / "ignored.slang").string().c_str());
        f2 << "build";
        std::ofstream f3((root_ignores / "bin" / "ignored.slang").string().c_str());
        f3 << "bin";
        std::ofstream f4((root_ignores / ".agents" / "ignored.slang").string().c_str());
        f4 << "agents";
        std::ofstream f5((root_ignores / "src" / "valid.slang").string().c_str());
        f5 << "valid";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount and scan
    database.mount_root(root_ignores.generic_string().c_str(), 10);
    database.scan_and_index();

    // 3. Assert: valid file is found, ignored directories are skipped
    EXPECT_NE(database.find_asset("src/valid.slang"), nullptr);
    EXPECT_EQ(database.find_asset(".git/ignored.slang"), nullptr);
    EXPECT_EQ(database.find_asset("build/ignored.slang"), nullptr);
    EXPECT_EQ(database.find_asset("bin/ignored.slang"), nullptr);
    EXPECT_EQ(database.find_asset(".agents/ignored.slang"), nullptr);

    // 4. Teardown
    std::filesystem::remove_all(root_ignores);
}

/// @brief Tests that default ignored extensions (.tmp, .bak, .pdb, .obj, etc.) are skipped during scan.
TEST(asset_database_ignores_and_predicates, default_ignored_extensions_skipped)
{
    // 1. Setup: Create files with various extensions
    auto root_ext = std::filesystem::path("temp_test_ext_ignores");
    std::filesystem::create_directories(root_ext);

    {
        std::ofstream f1((root_ext / "shader.tmp").string().c_str());
        f1 << "tmp";
        std::ofstream f2((root_ext / "shader.bak").string().c_str());
        f2 << "bak";
        std::ofstream f3((root_ext / "shader.pdb").string().c_str());
        f3 << "pdb";
        std::ofstream f4((root_ext / "shader.obj").string().c_str());
        f4 << "obj";
        std::ofstream f5((root_ext / "shader.slang").string().c_str());
        f5 << "slang";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Mount and scan
    database.mount_root(root_ext.generic_string().c_str(), 10);
    database.scan_and_index();

    // 3. Assert: shader.slang is discovered, ignored extensions are omitted
    EXPECT_NE(database.find_asset("shader.slang"), nullptr);
    EXPECT_EQ(database.find_asset("shader.tmp"), nullptr);
    EXPECT_EQ(database.find_asset("shader.bak"), nullptr);
    EXPECT_EQ(database.find_asset("shader.pdb"), nullptr);
    EXPECT_EQ(database.find_asset("shader.obj"), nullptr);

    // 4. Teardown
    std::filesystem::remove_all(root_ext);
}

/// @brief Tests adding custom ignored directory and extension entries to asset_database.
TEST(asset_database_ignores_and_predicates, custom_ignored_directory_and_extension)
{
    // 1. Setup: Create custom directory and custom extension files
    auto root_custom = std::filesystem::path("temp_test_custom_ignores");
    std::filesystem::create_directories(root_custom / "my_secret_dir");
    std::filesystem::create_directories(root_custom / "normal_dir");

    {
        std::ofstream f1((root_custom / "my_secret_dir" / "file.slang").string().c_str());
        f1 << "secret";
        std::ofstream f2((root_custom / "normal_dir" / "file.custom").string().c_str());
        f2 << "custom";
        std::ofstream f3((root_custom / "normal_dir" / "file.slang").string().c_str());
        f3 << "normal";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Configure custom ignores with and without leading dots
    database.add_ignored_directory("my_secret_dir");
    database.add_ignored_extension("custom"); // Test normalization without leading dot
    database.mount_root(root_custom.generic_string().c_str(), 10);
    database.scan_and_index();

    // 3. Assert: Inspect ignore collections and scan results
    auto ignored_dirs = database.get_ignored_directories();
    auto ignored_exts = database.get_ignored_extensions();
    EXPECT_GE(ignored_dirs.size(), 11U);
    EXPECT_GE(ignored_exts.size(), 8U);

    EXPECT_NE(database.find_asset("normal_dir/file.slang"), nullptr);
    EXPECT_EQ(database.find_asset("my_secret_dir/file.slang"), nullptr);
    EXPECT_EQ(database.find_asset("normal_dir/file.custom"), nullptr);

    // 4. Teardown
    std::filesystem::remove_all(root_custom);
}

/// @brief Tests that clear_ignores() removes all default and custom ignore rules.
TEST(asset_database_ignores_and_predicates, clear_ignores_enables_all)
{
    // 1. Setup: Create file in build directory with .tmp extension
    auto root_clear = std::filesystem::path("temp_test_clear_ignores");
    std::filesystem::create_directories(root_clear / "build");

    {
        std::ofstream f((root_clear / "build" / "output.tmp").string().c_str());
        f << "data";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};

    // 2. Act: Clear all ignores and scan
    database.clear_ignores();
    EXPECT_EQ(database.get_ignored_directories().size(), 0U);
    EXPECT_EQ(database.get_ignored_extensions().size(), 0U);

    database.mount_root(root_clear.generic_string().c_str(), 10);
    database.scan_and_index();

    // 3. Assert: File in build/ with .tmp extension is now indexed
    EXPECT_TRUE(database.resolve_source_path("build/output.tmp").has_value());

    // 4. Teardown
    std::filesystem::remove_all(root_clear);
}

/// @brief Tests scan_options with extension whitelist, additional directory ignores, and custom predicate.
TEST(asset_database_ignores_and_predicates, scan_options_whitelist_and_predicate)
{
    // 1. Setup: Create hierarchy with various folders and extensions
    auto root_opts = std::filesystem::path("temp_test_scan_opts");
    std::filesystem::create_directories(root_opts / "included");
    std::filesystem::create_directories(root_opts / "pruned_by_predicate");
    std::filesystem::create_directories(root_opts / "ad_hoc_ignored");

    {
        std::ofstream f1((root_opts / "included" / "test.slang").string().c_str());
        f1 << "slang";
        std::ofstream f2((root_opts / "included" / "test.txt").string().c_str());
        f2 << "text";
        std::ofstream f3((root_opts / "pruned_by_predicate" / "test.slang").string().c_str());
        f3 << "pruned";
        std::ofstream f4((root_opts / "ad_hoc_ignored" / "test.slang").string().c_str());
        f4 << "adhoc";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};
    database.mount_root(root_opts.generic_string().c_str(), 10, "assets");

    // 2. Act: Scan with scan_options
    auto opts = tempest::assets::scan_options{};
    opts.extension_whitelist.push_back(".slang");
    opts.additional_ignored_directories.push_back("ad_hoc_ignored");
    opts.predicate = [](tempest::string_view rel_path, [[maybe_unused]] bool is_dir) {
        return !tempest::starts_with(rel_path, "pruned_by_predicate");
    };
    database.scan_and_index(opts);

    // 3. Assert: Whitelist, additional ignore, and predicate pruning rules are enforced
    EXPECT_NE(database.find_asset("@assets/included/test.slang"), nullptr);
    EXPECT_EQ(database.find_asset("@assets/included/test.txt"), nullptr);
    EXPECT_EQ(database.find_asset("@assets/pruned_by_predicate/test.slang"), nullptr);
    EXPECT_EQ(database.find_asset("@assets/ad_hoc_ignored/test.slang"), nullptr);

    // 4. Teardown
    std::filesystem::remove_all(root_opts);
}

/// @brief Tests that loading an asset through an alias loads via importer initially, and subsequent loads hit the
/// cached blob path.
TEST(asset_database_mount_aliases, aliased_load_subsequent_blob_cache_hit)
{
    // 1. Setup: Create mock asset file and register a dummy importer
    auto root_dir = std::filesystem::path("temp_test_alias_load_cache");
    std::filesystem::create_directories(root_dir);

    {
        std::ofstream f((root_dir / "model.mock").string().c_str());
        f << "MOCK_MESH_PAYLOAD";
    }

    struct mock_importer : tempest::assets::asset_importer
    {
        size_t import_count{0};
        auto import(tempest::assets::asset_database& db, [[maybe_unused]] tempest::span<const tempest::byte> data,
                    tempest::ecs::archetype_registry& registry, tempest::optional<tempest::string_view> asset_path)
            -> tempest::ecs::entity override
        {
            ++import_count;
            auto path = asset_path.value_or("mock_asset");
            auto asset_id = db.register_asset(tempest::assets::asset_type_id::from_hash(123), path);
            auto blob_str = std::string("PROCESSED_BLOB");
            db.store_blob(asset_id, tempest::span<const tempest::byte>{
                                        reinterpret_cast<const tempest::byte*>(blob_str.data()), blob_str.size()});
            return registry.create<>();
        }
    };

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};
    auto imp = tempest::make_unique<mock_importer>();
    auto* imp_ptr = imp.get();
    database.register_importer(tempest::move(imp), ".mock");

    database.mount_root(root_dir.generic_string().c_str(), 10, "models");

    // 2. Act 1: Initial load via alias triggers importer
    auto events = tempest::event::event_registry{};
    auto registry1 = tempest::ecs::basic_archetype_registry{events};
    auto e1 = database.load("@models/model.mock", registry1);
    EXPECT_TRUE(e1 != tempest::ecs::tombstone);
    EXPECT_EQ(imp_ptr->import_count, 1U);

    // 3. Act 2: Subsequent load via alias hits blob cache without re-importing
    auto registry2 = tempest::ecs::basic_archetype_registry{events};
    auto e2 = database.load("@models/model.mock", registry2);
    EXPECT_TRUE(e2 != tempest::ecs::tombstone);
    EXPECT_EQ(imp_ptr->import_count, 1U);

    // 4. Teardown
    std::filesystem::remove_all(root_dir);
}

/// @brief Tests unmounting an aliased root and verifying resolution fallbacks update accordingly.
TEST(asset_database_mount_aliases, unmount_aliased_root_updates_resolution)
{
    // 1. Setup: Create two mount roots with same alias but different priorities
    auto root_high = std::filesystem::path("temp_test_unmount_high");
    auto root_low = std::filesystem::path("temp_test_unmount_low");
    std::filesystem::create_directories(root_high);
    std::filesystem::create_directories(root_low);

    {
        std::ofstream fh((root_high / "common.slang").string().c_str());
        fh << "// High";
        std::ofstream fl((root_low / "common.slang").string().c_str());
        fl << "// Low";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};
    database.mount_root(root_high.generic_string().c_str(), 10, "shaders");
    database.mount_root(root_low.generic_string().c_str(), 5, "shaders");

    // 2. Act & Assert 1: High priority root resolves first
    auto disk1 = database.resolve_disk_path("@shaders/common.slang");
    ASSERT_TRUE(disk1.has_value());
    EXPECT_NE(std::string_view(disk1->c_str()).find("temp_test_unmount_high"), std::string_view::npos);

    // 3. Act 2: Unmount high priority root
    database.unmount_root(root_high.generic_string().c_str());

    // 4. Assert 2: Resolution falls back to low priority root
    auto disk2 = database.resolve_disk_path("@shaders/common.slang");
    ASSERT_TRUE(disk2.has_value());
    EXPECT_NE(std::string_view(disk2->c_str()).find("temp_test_unmount_low"), std::string_view::npos);

    // 5. Teardown
    std::filesystem::remove_all(root_high);
    std::filesystem::remove_all(root_low);
}

/// @brief Tests that notify_file_changed invalidates only the specific aliased source matching the disk path.
TEST(asset_database_mount_aliases, notify_file_changed_isolates_exact_aliased_mount)
{
    // 1. Setup: Create two separate roots containing a file with identical relative names
    auto root_a = std::filesystem::path("temp_test_notify_a");
    auto root_b = std::filesystem::path("temp_test_notify_b");
    std::filesystem::create_directories(root_a);
    std::filesystem::create_directories(root_b);

    {
        std::ofstream fa((root_a / "effect.slang").string().c_str());
        fa << "ALPHA";
        std::ofstream fb((root_b / "effect.slang").string().c_str());
        fb << "BETA";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};
    database.mount_root(root_a.generic_string().c_str(), 10, "alpha");
    database.mount_root(root_b.generic_string().c_str(), 10, "beta");
    database.scan_and_index();

    const auto* asset_a = database.find_asset("@alpha/effect.slang");
    const auto* asset_b = database.find_asset("@beta/effect.slang");
    ASSERT_NE(asset_a, nullptr);
    ASSERT_NE(asset_b, nullptr);

    // Store blobs for both assets
    auto data_a = std::string("BLOB_A");
    auto data_b = std::string("BLOB_B");
    database.store_blob(asset_a->id, tempest::span<const tempest::byte>{
                                         reinterpret_cast<const tempest::byte*>(data_a.data()), data_a.size()});
    database.store_blob(asset_b->id, tempest::span<const tempest::byte>{
                                         reinterpret_cast<const tempest::byte*>(data_b.data()), data_b.size()});

    EXPECT_FALSE(database.get_blob(asset_a->id).empty());
    EXPECT_FALSE(database.get_blob(asset_b->id).empty());

    // 2. Act: Notify that root_a's effect.slang changed
    auto changed = database.notify_file_changed((root_a / "effect.slang").generic_string().c_str());
    EXPECT_TRUE(changed);

    // 3. Assert: asset_a blob was invalidated and reloads "ALPHA" from disk, while asset_b blob remains cached as
    // "BLOB_B"
    auto blob_a = database.get_blob(asset_a->id);
    auto str_a = std::string(reinterpret_cast<const char*>(blob_a.data()), blob_a.size());
    EXPECT_EQ(str_a, "ALPHA");

    auto blob_b = database.get_blob(asset_b->id);
    auto str_b = std::string(reinterpret_cast<const char*>(blob_b.data()), blob_b.size());
    EXPECT_EQ(str_b, "BLOB_B");

    // 4. Teardown
    std::filesystem::remove_all(root_a);
    std::filesystem::remove_all(root_b);
}

/// @brief Tests parsing and handling of corner-case alias string formats (@, @/, @@alias).
TEST(asset_database_mount_aliases, corner_case_alias_strings_handling)
{
    // 1. Setup: Create test folder with files
    auto root_dir = std::filesystem::path("temp_test_corner_alias");
    std::filesystem::create_directories(root_dir);

    {
        std::ofstream f1((root_dir / "test.slang").string().c_str());
        f1 << "test";
    }

    auto type_reg = tempest::assets::asset_type_registry{};
    auto database = tempest::assets::asset_database{&type_reg};
    database.mount_root(root_dir.generic_string().c_str(), 10, "core");
    database.scan_and_index();

    // 2. Act & Assert: Corner-case queries
    // "@" and "@/" have empty alias and must not resolve against unaliased or aliased roots
    EXPECT_FALSE(database.resolve_disk_path("@").has_value());
    EXPECT_FALSE(database.resolve_disk_path("@/").has_value());
    EXPECT_FALSE(database.resolve_disk_path("@/test.slang").has_value());
    EXPECT_EQ(database.find_asset("@/test.slang"), nullptr);

    // "@@core/test.slang" parses alias "@core" which does not match alias "core"
    EXPECT_FALSE(database.resolve_disk_path("@@core/test.slang").has_value());
    EXPECT_EQ(database.find_asset("@@core/test.slang"), nullptr);

    // Redundant slashes in subpath normalize cleanly
    EXPECT_TRUE(database.resolve_disk_path("@core//test.slang").has_value());
    EXPECT_NE(database.find_asset("@core//test.slang"), nullptr);

    // 3. Teardown
    std::filesystem::remove_all(root_dir);
}
