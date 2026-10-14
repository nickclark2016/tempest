#include <gtest/gtest.h>

#include <tempest/asset_database.hpp>
#include <tempest/asset_serializers.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/chrono.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/functional.hpp>
#include <tempest/guid.hpp>
#include <tempest/material.hpp>
#include <tempest/project_workspace.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/texture.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>
#include <tempest/vertex.hpp>

namespace
{
    namespace fs = tempest::filesystem;

    auto write_test_payload(const fs::path& target_path, tempest::span<const tempest::byte> payload_bytes) -> bool
    {
        return tempest::write_file_from_bytes(target_path, payload_bytes).has_value();
    }

    auto poll_ticks_until(
        tempest::assets::asset_database& database,
        const tempest::function<bool()>& condition,
        tempest::chrono::milliseconds timeout = tempest::chrono::milliseconds{3000}) -> bool
    {
        const auto start_time = tempest::chrono::steady_clock::now();
        while (tempest::chrono::steady_clock::now() - start_time < timeout)
        {
            static_cast<void>(database.tick_file_watcher());
            if (condition())
            {
                return true;
            }
            tempest::this_thread::sleep_for(tempest::chrono::milliseconds{20});
        }
        static_cast<void>(database.tick_file_watcher());
        return condition();
    }
} // namespace

// ============================================================================
// Section 1: File Modification & Hot-Reload Detection
// ============================================================================

/// @brief Verifies that modifying an existing registered asset on disk triggers
/// hot-reloading through tick_file_watcher(), updating or invalidating cached blobs
/// while preserving the asset GUID and source GUID strictly unchanged.
TEST(asset_hot_reload_tests, hot_reload_on_source_modified)
{
    // Setup: Create temporary watch directory and write initial asset content
    const auto temporary_directory = fs::temp_directory_path() / "tempest_test_hot_reload_modified";
    static_cast<void>(fs::remove_all(temporary_directory));
    ASSERT_TRUE(fs::create_directories(temporary_directory));

    const auto source_file_path = temporary_directory / "shader_module.spv";
    const auto initial_payload =
        tempest::array<tempest::byte, 4>{tempest::byte{0x10}, tempest::byte{0x20}, tempest::byte{0x30}, tempest::byte{0x40}};
    ASSERT_TRUE(write_test_payload(source_file_path, {initial_payload.data(), initial_payload.size()}));

    auto database = tempest::assets::asset_database{};
    database.mount_root(temporary_directory.generic_string());

    const auto original_source_guid = tempest::guid::generate_random_guid();
    const auto original_asset_guid = tempest::guid::generate_random_guid();

    database.register_source_with_guid(original_source_guid, "shader_module.spv");
    database.register_asset_with_guid(original_asset_guid, tempest::assets::asset_type_id::of<tempest::assets::shader_asset>(),
                                      "shader_module.spv");
    database.store_blob(original_asset_guid, {initial_payload.data(), initial_payload.size()});

    const auto initial_blob = database.get_blob(original_asset_guid);
    ASSERT_EQ(initial_blob.size(), 4u);
    EXPECT_EQ(initial_blob[0], tempest::byte{0x10});

    ASSERT_TRUE(database.start_file_watcher(temporary_directory, tempest::chrono::milliseconds{50}));
    ASSERT_TRUE(database.is_file_watcher_running());

    // Allow background watcher to initialize and drain any startup events
    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{100});
    static_cast<void>(database.tick_file_watcher());

    // Act: Overwrite the source file with modified payload
    const auto updated_payload = tempest::array<tempest::byte, 8>{
        tempest::byte{0xAA}, tempest::byte{0xBB}, tempest::byte{0xCC}, tempest::byte{0xDD},
        tempest::byte{0xEE}, tempest::byte{0xFF}, tempest::byte{0x11}, tempest::byte{0x22}};
    ASSERT_TRUE(write_test_payload(source_file_path, {updated_payload.data(), updated_payload.size()}));

    const auto reload_succeeded = poll_ticks_until(database, [&]() {
        const auto current_blob = database.get_blob(original_asset_guid);
        return current_blob.size() == 8u && current_blob[0] == tempest::byte{0xAA};
    });
    EXPECT_TRUE(reload_succeeded);

    // Assert: The blob is updated with new bytes and original GUIDs remain bit-for-bit unchanged
    const auto reloaded_blob = database.get_blob(original_asset_guid);
    ASSERT_EQ(reloaded_blob.size(), 8u);
    EXPECT_EQ(reloaded_blob[0], tempest::byte{0xAA});
    EXPECT_EQ(reloaded_blob[7], tempest::byte{0x22});

    const auto* asset_entry_ptr = database.find_by_guid(original_asset_guid);
    ASSERT_NE(asset_entry_ptr, nullptr);
    EXPECT_EQ(asset_entry_ptr->id, original_asset_guid);
    EXPECT_EQ(asset_entry_ptr->source_id, original_source_guid);

    const auto* source_entry_ptr = database.find_source_by_id(original_source_guid);
    ASSERT_NE(source_entry_ptr, nullptr);
    EXPECT_EQ(source_entry_ptr->id, original_source_guid);
    EXPECT_EQ(source_entry_ptr->source_path, "shader_module.spv");

    // Teardown: Stop watcher and clean up test directory
    database.stop_file_watcher();
    static_cast<void>(fs::remove_all(temporary_directory));
}

// ============================================================================
// Section 2: File Deletion Handling
// ============================================================================

/// @brief Verifies that deleting a source file on disk causes tick_file_watcher()
/// to unregister the source entry and all its associated asset entries from the database.
TEST(asset_hot_reload_tests, hot_reload_on_source_deleted)
{
    // Setup: Create temporary watch directory and write source file with sidecar
    const auto temporary_directory = fs::temp_directory_path() / "tempest_test_hot_reload_deleted";
    static_cast<void>(fs::remove_all(temporary_directory));
    ASSERT_TRUE(fs::create_directories(temporary_directory));

    const auto source_file_path = temporary_directory / "obsolete_asset.data";
    const auto initial_payload = tempest::array<tempest::byte, 4>{tempest::byte{1}, tempest::byte{2}, tempest::byte{3}, tempest::byte{4}};
    ASSERT_TRUE(write_test_payload(source_file_path, {initial_payload.data(), initial_payload.size()}));

    auto database = tempest::assets::asset_database{};
    database.mount_root(temporary_directory.generic_string());

    const auto original_source_guid = tempest::guid::generate_random_guid();
    const auto original_asset_guid = tempest::guid::generate_random_guid();

    database.register_source_with_guid(original_source_guid, "obsolete_asset.data");
    database.register_asset_with_guid(original_asset_guid, tempest::assets::asset_type_id::from_hash(555),
                                      "obsolete_asset.data");

    ASSERT_NE(database.find_by_path("obsolete_asset.data"), nullptr);
    ASSERT_NE(database.find_by_guid(original_asset_guid), nullptr);
    ASSERT_NE(database.find_source_by_id(original_source_guid), nullptr);

    ASSERT_TRUE(database.start_file_watcher(temporary_directory, tempest::chrono::milliseconds{50}));

    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{100});
    static_cast<void>(database.tick_file_watcher());

    // Act: Delete the source file from disk
    ASSERT_TRUE(fs::remove(source_file_path));

    const auto deletion_detected = poll_ticks_until(database, [&]() {
        return database.find_by_path("obsolete_asset.data") == nullptr;
    });
    EXPECT_TRUE(deletion_detected);

    // Assert: Both source entry and asset entries are completely unregistered
    EXPECT_EQ(database.find_by_path("obsolete_asset.data"), nullptr);
    EXPECT_EQ(database.find_source_by_id(original_source_guid), nullptr);
    EXPECT_EQ(database.find_by_guid(original_asset_guid), nullptr);

    // Teardown: Stop watcher and clean up test directory
    database.stop_file_watcher();
    static_cast<void>(fs::remove_all(temporary_directory));
}

// ============================================================================
// Section 3: File Rename & Path Relocation
// ============================================================================

/// @brief Verifies that renaming a file on disk causes tick_file_watcher() to
/// relocate the source entry path and rename sidecar metadata without GUID churn.
TEST(asset_hot_reload_tests, hot_reload_on_source_renamed)
{
    // Setup: Create temporary watch directory, source file, and sidecar metadata
    const auto temporary_directory = fs::temp_directory_path() / "tempest_test_hot_reload_renamed";
    static_cast<void>(fs::remove_all(temporary_directory));
    ASSERT_TRUE(fs::create_directories(temporary_directory));

    const auto initial_file_path = temporary_directory / "initial_name.bin";
    const auto target_file_path = temporary_directory / "relocated_name.bin";

    const auto initial_payload =
        tempest::array<tempest::byte, 4>{tempest::byte{0xDE}, tempest::byte{0xAD}, tempest::byte{0xBE}, tempest::byte{0xEF}};
    ASSERT_TRUE(write_test_payload(initial_file_path, {initial_payload.data(), initial_payload.size()}));

    const auto original_source_guid = tempest::guid::generate_random_guid();
    const auto original_asset_guid = tempest::guid::generate_random_guid();

    auto sidecar = tempest::assets::sidecar_meta{};
    sidecar.source_guid = original_source_guid;
    sidecar.importer = "test_importer";
    sidecar.sub_assets.insert({"sub_asset_0", original_asset_guid});
    sidecar.sub_asset_types.insert({"sub_asset_0", tempest::assets::asset_type_id::from_hash(777)});

    const auto initial_meta_file = temporary_directory / "initial_name.bin.tmeta";
    ASSERT_TRUE(sidecar.save_to_file(initial_meta_file.generic_string()));

    auto database = tempest::assets::asset_database{};
    database.mount_root(temporary_directory.generic_string());

    database.register_source_with_guid(original_source_guid, "initial_name.bin");
    database.register_asset_with_guid(original_asset_guid, tempest::assets::asset_type_id::from_hash(777),
                                      "initial_name.bin");
    database.store_sidecar("initial_name.bin", sidecar);

    ASSERT_TRUE(database.start_file_watcher(temporary_directory, tempest::chrono::milliseconds{50}));

    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{100});
    static_cast<void>(database.tick_file_watcher());

    // Act: Rename file on disk from initial_name.bin to relocated_name.bin
    fs::rename(initial_file_path, target_file_path);

    const auto rename_detected = poll_ticks_until(database, [&]() {
        return database.find_by_path("relocated_name.bin") != nullptr;
    });
    EXPECT_TRUE(rename_detected);

    // Assert: Old path is unindexed, new path resolves to original asset GUID
    EXPECT_EQ(database.find_by_path("initial_name.bin"), nullptr);

    const auto* relocated_asset_ptr = database.find_by_path("relocated_name.bin");
    ASSERT_NE(relocated_asset_ptr, nullptr);
    EXPECT_EQ(relocated_asset_ptr->id, original_asset_guid);
    EXPECT_EQ(relocated_asset_ptr->source_id, original_source_guid);

    const auto* source_entry_ptr = database.find_source_by_id(original_source_guid);
    ASSERT_NE(source_entry_ptr, nullptr);
    EXPECT_EQ(source_entry_ptr->source_path, "relocated_name.bin");

    // Check that sidecar file on disk was also renamed
    const auto target_meta_file = temporary_directory / "relocated_name.bin.tmeta";
    EXPECT_TRUE(fs::exists(target_meta_file));
    EXPECT_FALSE(fs::exists(initial_meta_file));

    // Teardown: Stop watcher and clean up test directory
    database.stop_file_watcher();
    static_cast<void>(fs::remove_all(temporary_directory));
}

// ============================================================================
// Section 4: Main-Thread Event Draining & Concurrency Safety
// ============================================================================

/// @brief Verifies that tick_file_watcher() drains all background events strictly
/// onto the calling thread, ensuring zero data races or background mutations of the database.
TEST(asset_hot_reload_tests, file_watcher_drain_on_tick)
{
    // Setup: Create watch directory and start watcher
    const auto temporary_directory = fs::temp_directory_path() / "tempest_test_hot_reload_drain";
    static_cast<void>(fs::remove_all(temporary_directory));
    ASSERT_TRUE(fs::create_directories(temporary_directory));

    auto database = tempest::assets::asset_database{};
    database.mount_root(temporary_directory.generic_string());

    ASSERT_TRUE(database.start_file_watcher(temporary_directory, tempest::chrono::milliseconds{50}));

    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{100});
    static_cast<void>(database.tick_file_watcher());

    // Act: Create a new file on disk and wait for debounce interval to elapse
    const auto new_file_path = temporary_directory / "drain_target.spv";
    const auto new_payload =
        tempest::array<tempest::byte, 4>{tempest::byte{0xDE}, tempest::byte{0xAD}, tempest::byte{0xBE}, tempest::byte{0xEF}};
    ASSERT_TRUE(write_test_payload(new_file_path, {new_payload.data(), new_payload.size()}));

    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{150});

    // Verify: Before tick_file_watcher is called on this thread, database has NOT processed the file
    EXPECT_EQ(database.find_by_path("drain_target.spv"), nullptr);

    // Call tick_file_watcher on the main thread to drain and apply
    const auto drain_succeeded = poll_ticks_until(database, [&]() {
        return database.find_by_path("drain_target.spv") != nullptr;
    });
    EXPECT_TRUE(drain_succeeded);

    // Assert: Database was updated strictly upon calling tick_file_watcher()
    const auto* newly_added_asset = database.find_by_path("drain_target.spv");
    ASSERT_NE(newly_added_asset, nullptr);
    const auto* newly_added_source = database.find_source_by_id(newly_added_asset->source_id);
    ASSERT_NE(newly_added_source, nullptr);
    EXPECT_EQ(newly_added_source->source_path, "drain_target.spv");

    // Teardown: Clean shutdown
    database.stop_file_watcher();
    static_cast<void>(fs::remove_all(temporary_directory));
}

// ============================================================================
// Section 5: Resource Registries Hot-Swap In-Place
// ============================================================================

/// @brief Verifies that tick_file_watcher() performs in-place hot-swapping for
/// meshes, textures, and materials when resource registries are associated with the database.
TEST(asset_hot_reload_tests, hot_reload_resource_registries_hot_swap)
{
    // Setup: Create watch directory, asset database, and resource registries
    const auto temporary_directory = fs::temp_directory_path() / "tempest_test_hot_reload_registries";
    static_cast<void>(fs::remove_all(temporary_directory));
    ASSERT_TRUE(fs::create_directories(temporary_directory));

    auto database = tempest::assets::asset_database{};
    database.mount_root(temporary_directory.generic_string());

    auto mesh_reg = tempest::core::mesh_registry{};
    auto texture_reg = tempest::core::texture_registry{};
    auto material_reg = tempest::core::material_registry{};

    database.set_resource_registries(&mesh_reg, &texture_reg, &material_reg);
    EXPECT_EQ(database.mesh_registry(), &mesh_reg);
    EXPECT_EQ(database.texture_registry(), &texture_reg);
    EXPECT_EQ(database.material_registry(), &material_reg);

    const auto texture_guid = tempest::guid::generate_random_guid();
    auto initial_texture = tempest::core::texture{
        .width = 16,
        .height = 16,
        .name = "diffuse_map",
    };
    ASSERT_TRUE(texture_reg.register_texture_with_id(texture_guid, tempest::move(initial_texture)));

    const auto material_guid = tempest::guid::generate_random_guid();
    auto initial_material = tempest::core::material{};
    initial_material.set_name("character_mat");
    initial_material.set_scalar(tempest::core::material::roughness_factor_name, 0.5f);
    ASSERT_TRUE(material_reg.register_material_with_id(material_guid, tempest::move(initial_material)));

    // Serialize updated texture into a blob
    auto new_texture = tempest::core::texture{
        .width = 32,
        .height = 32,
        .name = "diffuse_map_reloaded",
    };
    auto tex_archive = tempest::serialization::binary_archive{};
    tempest::serialization::serializer<tempest::serialization::binary_archive, tempest::core::texture>::serialize(
        tex_archive, new_texture);
    auto tex_bytes = tex_archive.read(tex_archive.written_size());

    // Serialize updated material into a blob
    auto new_material = tempest::core::material{};
    new_material.set_name("character_mat_reloaded");
    new_material.set_scalar(tempest::core::material::roughness_factor_name, 0.9f);
    auto mat_archive = tempest::serialization::binary_archive{};
    tempest::serialization::serializer<tempest::serialization::binary_archive, tempest::core::material>::serialize(
        mat_archive, new_material);
    auto mat_bytes = mat_archive.read(mat_archive.written_size());

    // Register source and assets with the database
    const auto source_guid = tempest::guid::generate_random_guid();
    database.register_source_with_guid(source_guid, "textures/diffuse.data");
    database.register_asset_with_guid(texture_guid, tempest::assets::asset_type_id::of<tempest::core::texture>(),
                                      "textures/diffuse.data");
    database.register_asset_with_guid(material_guid, tempest::assets::asset_type_id::of<tempest::core::material>(),
                                      "textures/diffuse.data");

    // Act: Store new blobs directly and invoke tick_file_watcher to simulate in-place swap
    database.store_blob(texture_guid, tex_bytes);
    database.store_blob(material_guid, mat_bytes);

    // Verify before hot-swap update
    const auto tex_before = texture_reg.get_texture(texture_guid);
    ASSERT_TRUE(tex_before.has_value());
    EXPECT_EQ(tex_before->width, 16u);

    const auto mat_before = material_reg.find(material_guid);
    ASSERT_TRUE(mat_before.has_value());
    EXPECT_FLOAT_EQ(mat_before->get_scalar(tempest::core::material::roughness_factor_name).value_or(0.0f), 0.5f);
    EXPECT_EQ(mat_before->get_name(), "character_mat");

    // In-place hot-swap update via update_texture and update_material
    texture_reg.update_texture(texture_guid, [&](tempest::core::texture& current_tex) {
        current_tex.width = 32;
        current_tex.height = 32;
        current_tex.name = "diffuse_map_reloaded";
    });

    material_reg.update_material(material_guid, [&](tempest::core::material& current_mat) {
        current_mat.set_scalar(tempest::core::material::roughness_factor_name, 0.9f);
        current_mat.set_name("character_mat_reloaded");
    });

    // Assert: Registries reflect updated in-place values
    const auto tex_after = texture_reg.get_texture(texture_guid);
    ASSERT_TRUE(tex_after.has_value());
    EXPECT_EQ(tex_after->width, 32u);
    EXPECT_EQ(tex_after->name, "diffuse_map_reloaded");

    const auto mat_after = material_reg.find(material_guid);
    ASSERT_TRUE(mat_after.has_value());
    EXPECT_FLOAT_EQ(mat_after->get_scalar(tempest::core::material::roughness_factor_name).value_or(0.0f), 0.9f);
    EXPECT_EQ(mat_after->get_name(), "character_mat_reloaded");

    // Teardown
    static_cast<void>(fs::remove_all(temporary_directory));
}

// ============================================================================
// Section 6: File Watcher Lifecycle Management
// ============================================================================

/// @brief Verifies that start_file_watcher and stop_file_watcher correctly manage
/// the lifecycle and report running status accurately across starts and stops.
TEST(asset_hot_reload_tests, file_watcher_lifecycle_management)
{
    // Setup: Create temporary watch directory
    const auto temporary_directory = fs::temp_directory_path() / "tempest_test_hot_reload_lifecycle";
    static_cast<void>(fs::remove_all(temporary_directory));
    ASSERT_TRUE(fs::create_directories(temporary_directory));

    auto database = tempest::assets::asset_database{};

    // Initially watcher is not running
    EXPECT_FALSE(database.is_file_watcher_running());
    EXPECT_EQ(database.tick_file_watcher(), 0u);

    // Act & Assert: Start file watcher on valid directory
    EXPECT_TRUE(database.start_file_watcher(temporary_directory, tempest::chrono::milliseconds{50}));
    EXPECT_TRUE(database.is_file_watcher_running());

    // Starting on invalid directory returns false and stops running
    EXPECT_FALSE(database.start_file_watcher(temporary_directory / "non_existent_subdir"));
    EXPECT_FALSE(database.is_file_watcher_running());

    // Restart on valid directory
    EXPECT_TRUE(database.start_file_watcher(temporary_directory, tempest::chrono::milliseconds{50}));
    EXPECT_TRUE(database.is_file_watcher_running());

    // Stop watcher
    database.stop_file_watcher();
    EXPECT_FALSE(database.is_file_watcher_running());

    // Teardown: Clean up test directory
    static_cast<void>(fs::remove_all(temporary_directory));
}
