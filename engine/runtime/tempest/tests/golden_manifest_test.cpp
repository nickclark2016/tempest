#include <tempest/engine_types.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/format.hpp>
#include <tempest/type_manifest.hpp>

#include <tempest/archetype.hpp>
#include <tempest/material.hpp>
#include <tempest/physics/character_controller_component.hpp>
#include <tempest/physics/velocity_component.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/vertex.hpp>

#include <gtest/gtest.h>

namespace
{
    auto find_golden_file_path() -> tempest::filesystem::path
    {
        const auto candidates = tempest::array{
            tempest::filesystem::path{"engine/runtime/tempest/tests/type_manifest.golden.json"},
            tempest::filesystem::path{"../../engine/runtime/tempest/tests/type_manifest.golden.json"},
            tempest::filesystem::path{"../../../engine/runtime/tempest/tests/type_manifest.golden.json"},
        };

        for (const auto& candidate : candidates)
        {
            if (tempest::filesystem::exists(candidate))
            {
                return candidate;
            }
        }

        return candidates[0];
    }

    auto should_update_golden() -> bool
    {
        const auto& args = ::testing::internal::GetArgvs();
        for (const auto& arg : args)
        {
            if (arg == "--update-golden")
            {
                return true;
            }
        }
        return false;
    }

    auto normalize_line_endings(tempest::string_view input) -> tempest::string
    {
        auto normalized = tempest::string{};
        normalized.reserve(input.size());
        for (const auto character : input)
        {
            if (character != '\r')
            {
                normalized.push_back(character);
            }
        }
        return normalized;
    }
} // namespace

// ============================================================================
// Golden Manifest Validation Tests
// ============================================================================

/// @brief Validates that engine component registrations produce an identical type manifest
/// and schema hash across toolchains matching the golden reference file.
TEST(golden_manifest_test, engine_types_match_golden_file)
{
    // 1. Setup: instantiate a component type registry and register all engine components.
    auto registry = tempest::ecs::component_type_registry{};
    tempest::register_engine_types(registry);

    // 2. Act: export manifest and serialize to pretty JSON.
    const auto manifest = registry.export_manifest();
    ASSERT_NE(manifest.schema_hash, 0ULL);
    EXPECT_EQ(manifest.schema_hash, registry.schema_hash());

    const auto json_str = manifest.to_json(tempest::core::json_format::pretty);
    ASSERT_FALSE(json_str.empty());

    // 3. Resolve golden file path.
    const auto golden_path = find_golden_file_path();
    const auto update_requested = should_update_golden();
    const auto file_exists = tempest::filesystem::exists(golden_path);

    // 4. Update golden file if requested or if missing.
    if (update_requested || !file_exists)
    {
        const auto byte_data = tempest::span<const tempest::byte>{
            reinterpret_cast<const tempest::byte*>(json_str.data()), json_str.size()};
        const auto write_result = tempest::write_file_from_bytes(golden_path, byte_data);
        ASSERT_TRUE(write_result.has_value());
    }

    // 5. Read existing golden file contents from disk.
    const auto read_result = tempest::read_file_to_string(golden_path);
    ASSERT_TRUE(read_result.has_value());

    const auto& file_contents = *read_result;
    const auto normalized_golden = normalize_line_endings(tempest::string_view{file_contents.data(), file_contents.size()});
    const auto normalized_actual = normalize_line_endings(tempest::string_view{json_str.data(), json_str.size()});

    // 6. Assert: compare byte-for-byte.
    if (normalized_golden != normalized_actual)
    {
        FAIL() << "Golden manifest mismatch at: " << golden_path.c_str() << "\n"
               << "Expected (Golden):\n"
               << normalized_golden.c_str() << "\n"
               << "Actual (Generated):\n"
               << normalized_actual.c_str() << "\n";
    }

    EXPECT_EQ(normalized_golden, normalized_actual);
}

/// @brief Verifies that schema_hash and manifest generation are completely deterministic
/// regardless of the order in which component types are registered.
TEST(golden_manifest_test, schema_hash_deterministic_across_registration_order)
{
    // 1. Setup: standard forward registration.
    auto forward_registry = tempest::ecs::component_type_registry{};
    tempest::register_engine_types(forward_registry);

    // 2. Setup: manually registered in reversed order.
    auto reverse_registry = tempest::ecs::component_type_registry{};
    reverse_registry.register_type<tempest::physics::character_controller_component>();
    reverse_registry.register_type<tempest::physics::velocity_component>();
    reverse_registry.register_type<tempest::render_system::shadow_caster_component>();
    reverse_registry.register_type<tempest::render_system::point_light_component>();
    reverse_registry.register_type<tempest::render_system::directional_light_component>();
    reverse_registry.register_type<tempest::render_system::camera_component>();
    reverse_registry.register_type<tempest::core::material_component>();
    reverse_registry.register_type<tempest::core::mesh_component>();
    reverse_registry.register_type<tempest::ecs::transform_history_component>();
    reverse_registry.register_type<tempest::ecs::relationship_component<tempest::ecs::entity>>();
    reverse_registry.register_type<tempest::ecs::self_component>();
    reverse_registry.register_type<tempest::ecs::transform_component>();

    // 3. Act: export manifests.
    const auto forward_manifest = forward_registry.export_manifest();
    const auto reverse_manifest = reverse_registry.export_manifest();

    // 4. Assert: identical schema hashes and JSON manifests.
    EXPECT_EQ(forward_manifest.schema_hash, reverse_manifest.schema_hash);
    EXPECT_EQ(forward_registry.schema_hash(), reverse_registry.schema_hash());
    EXPECT_EQ(forward_manifest.to_json(), reverse_manifest.to_json());
}
