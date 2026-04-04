#include <gtest/gtest.h>

#include <tempest/asset_database.hpp>
#include <tempest/asset_serializers.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/entity_hierarchy.hpp>
#include <tempest/guid.hpp>
#include <tempest/material.hpp>
#include <tempest/meta.hpp>
#include <tempest/serial.hpp>
#include <tempest/texture.hpp>
#include <tempest/vertex.hpp>

#include <cstdio>

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
        [[nodiscard]] tempest::ecs::archetype_entity import(tempest::assets::asset_database& asset_db,
                                                            tempest::string_view path,
                                                            tempest::ecs::archetype_registry& registry) override
        {
            return import(asset_db, tempest::span<const tempest::byte>{}, registry, tempest::some(path));
        }

        [[nodiscard]] tempest::ecs::archetype_entity import(tempest::assets::asset_database& asset_db,
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
        [[nodiscard]] tempest::ecs::archetype_entity import(tempest::assets::asset_database& asset_db,
                                                            tempest::string_view path,
                                                            tempest::ecs::archetype_registry& registry) override
        {
            return import(asset_db, tempest::span<const tempest::byte>{}, registry, tempest::some(path));
        }

        [[nodiscard]] tempest::ecs::archetype_entity import(tempest::assets::asset_database& asset_db,
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
