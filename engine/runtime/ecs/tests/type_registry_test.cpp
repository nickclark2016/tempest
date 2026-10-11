#include <tempest/component_type_registry.hpp>
#include <tempest/archetype.hpp>
#include <tempest/event_registry.hpp>

#include <gtest/gtest.h>

namespace
{
    struct component_alpha
    {
        int32_t value{0};
    };

    struct component_beta
    {
        float x{0.0F};
        float y{0.0F};
    };

    struct component_gamma
    {
        uint64_t identifier{0};
    };

    template <size_t Index>
    struct dummy_component
    {
        uint8_t payload{static_cast<uint8_t>(Index)};
    };
} // namespace

// ============================================================================
// Component Type Registry - Isolation and State Tests
// ============================================================================

/// @brief Verifies that two distinct component_type_registry instances maintain
/// completely isolated type indices with zero cross-contamination.
TEST(component_type_registry_tests, separate_registries_isolate_indices)
{
    // 1. Setup two independent type registries.
    auto registry_a = tempest::ecs::component_type_registry{};
    auto registry_b = tempest::ecs::component_type_registry{};

    // 2. Act: register Alpha then Beta on registry A.
    const auto alpha_idx_a = registry_a.register_type<component_alpha>();
    const auto beta_idx_a = registry_a.register_type<component_beta>();

    // 3. Act: register Beta then Alpha on registry B in inverted order.
    const auto beta_idx_b = registry_b.register_type<component_beta>();
    const auto alpha_idx_b = registry_b.register_type<component_alpha>();

    // 4. Assert: verify distinct indices between the registries.
    ASSERT_EQ(alpha_idx_a, 0U);
    ASSERT_EQ(beta_idx_a, 1U);
    ASSERT_EQ(beta_idx_b, 0U);
    ASSERT_EQ(alpha_idx_b, 1U);

    // 5. Assert: query type_index accessors to ensure zero cross-contamination.
    const auto lookup_beta_a = registry_a.type_index<component_beta>();
    const auto lookup_beta_b = registry_b.type_index<component_beta>();

    ASSERT_TRUE(lookup_beta_a.has_value());
    ASSERT_TRUE(lookup_beta_b.has_value());
    ASSERT_EQ(*lookup_beta_a, 1U);
    ASSERT_EQ(*lookup_beta_b, 0U);
}

/// @brief Verifies that registering the same component type multiple times is idempotent.
TEST(component_type_registry_tests, ensure_is_idempotent)
{
    // 1. Setup type registry.
    auto registry = tempest::ecs::component_type_registry{};

    // 2. Act: register the same type multiple times.
    const auto first_idx = registry.ensure<component_alpha>();
    const auto second_idx = registry.ensure<component_alpha>();
    const auto third_idx = registry.register_type<component_alpha>();

    // 3. Assert: verify returned index is stable and registry count remains 1.
    ASSERT_EQ(first_idx, 0U);
    ASSERT_EQ(second_idx, 0U);
    ASSERT_EQ(third_idx, 0U);
    ASSERT_EQ(registry.size(), 1U);
    ASSERT_FALSE(registry.empty());
}

/// @brief Verifies that component_type_info retrieves correct size, alignment, and metadata.
TEST(component_type_registry_tests, type_info_retrieval)
{
    // 1. Setup type registry.
    auto registry = tempest::ecs::component_type_registry{};

    // 2. Act: register a component type.
    const auto idx = registry.register_type<component_beta>();

    // 3. Assert: inspect type_info by index and by name.
    const auto& info_by_idx = registry.type_info(idx);
    ASSERT_EQ(info_by_idx.index, idx);
    ASSERT_EQ(info_by_idx.size, sizeof(component_beta));
    ASSERT_EQ(info_by_idx.alignment, alignof(component_beta));
    ASSERT_TRUE(info_by_idx.should_duplicate);

    const auto* info_by_name = registry.type_info(info_by_idx.name);
    ASSERT_NE(info_by_name, nullptr);
    ASSERT_EQ(info_by_name->index, idx);
}

/// @brief Verifies alias resolution for legacy and migrated component names.
TEST(component_type_registry_tests, alias_resolution)
{
    // 1. Setup type registry.
    auto registry = tempest::ecs::component_type_registry{};

    // 2. Act: register alias before type registration.
    static constexpr auto legacy_name = tempest::string_view{"OldTransform"};
    static constexpr auto canonical_name = tempest::string_view{"Transform"};

    registry.add_alias(legacy_name, canonical_name);

    // 3. Assert: alias resolution directly.
    ASSERT_EQ(registry.resolve_alias(legacy_name), canonical_name);
    ASSERT_EQ(registry.resolve_alias(canonical_name), canonical_name);

    // 4. Act: register type with canonical name.
    const auto canonical_idx = registry.register_type<component_alpha>();
    // Add alias pointing to the registered canonical name.
    registry.add_alias(legacy_name, registry.type_info(canonical_idx).name);

    // 5. Assert: type_index and type_info with legacy alias return canonical entry.
    const auto legacy_idx = registry.type_index(legacy_name);
    ASSERT_TRUE(legacy_idx.has_value());
    ASSERT_EQ(*legacy_idx, canonical_idx);

    const auto* info_from_alias = registry.type_info(legacy_name);
    ASSERT_NE(info_from_alias, nullptr);
    ASSERT_EQ(info_from_alias->index, canonical_idx);
}

// ============================================================================
// Capacity and Bounds Tests
// ============================================================================

namespace
{
    template <size_t... Is>
    void register_dummy_types_helper(tempest::ecs::component_type_registry& registry,
                                     tempest::index_sequence<Is...> /*unused*/)
    {
        (registry.register_type<dummy_component<Is>>(), ...);
    }

    template <size_t Count>
    void register_dummy_types(tempest::ecs::component_type_registry& registry)
    {
        register_dummy_types_helper(registry, tempest::make_index_sequence<Count>{});
    }
} // namespace

/// @brief Verifies that registering up to max_component_types succeeds and that
/// exceeding capacity triggers an assertion failure.
TEST(component_type_registry_tests, max_capacity_hard_error)
{
    // 1. Setup registry with max capacity constant.
    auto registry = tempest::ecs::component_type_registry{};
    static constexpr auto max_types = tempest::ecs::component_type_registry::max_component_types;

    // 2. Act: register exactly 256 distinct component types.
    register_dummy_types<max_types>(registry);

    // 3. Assert: verify all 256 types registered.
    ASSERT_EQ(registry.size(), max_types);

    const auto first_dummy = registry.type_index<dummy_component<0>>();
    const auto last_dummy = registry.type_index<dummy_component<max_types - 1>>();
    ASSERT_TRUE(first_dummy.has_value());
    ASSERT_TRUE(last_dummy.has_value());
    ASSERT_EQ(*first_dummy, 0U);
    ASSERT_EQ(*last_dummy, static_cast<uint32_t>(max_types - 1));

    // 4. Assert: registering the 257th type fails / asserts in debug builds.
#ifndef NDEBUG
    ASSERT_DEATH_IF_SUPPORTED({ registry.register_type<dummy_component<max_types>>(); }, ".*");
#endif
}

// ============================================================================
// Hash Mask and Archetype Integration Tests
// ============================================================================

/// @brief Verifies that hash_mask produces correct bit patterns for component type packs.
TEST(component_type_registry_tests, hash_mask_generation)
{
    // 1. Setup type registry.
    auto registry = tempest::ecs::component_type_registry{};

    // 2. Act: compute hash mask for Alpha, Beta, Gamma.
    const auto mask = registry.hash_mask<component_alpha, component_beta, component_gamma>();

    // 3. Assert: verify bits corresponding to Alpha, Beta, Gamma are set.
    const auto alpha_idx = *registry.type_index<component_alpha>();
    const auto beta_idx = *registry.type_index<component_beta>();
    const auto gamma_idx = *registry.type_index<component_gamma>();

    const auto alpha_bit = (mask.hash[alpha_idx / 8] & static_cast<tempest::byte>(1 << (alpha_idx % 8))) != tempest::byte{0};
    const auto beta_bit = (mask.hash[beta_idx / 8] & static_cast<tempest::byte>(1 << (beta_idx % 8))) != tempest::byte{0};
    const auto gamma_bit = (mask.hash[gamma_idx / 8] & static_cast<tempest::byte>(1 << (gamma_idx % 8))) != tempest::byte{0};

    ASSERT_TRUE(alpha_bit);
    ASSERT_TRUE(beta_bit);
    ASSERT_TRUE(gamma_bit);
}

/// @brief Verifies that two basic_archetype_registry instances sharing one
/// component_type_registry share identical component indices and interoperable types.
TEST(component_type_registry_tests, shared_type_registry_with_archetypes)
{
    // 1. Setup shared component_type_registry and event registry.
    auto shared_types = tempest::ecs::component_type_registry{};
    auto events = tempest::event::event_registry{};

    // 2. Setup two basic_archetype_registry instances sharing the same type registry.
    auto registry_a = tempest::ecs::basic_archetype_registry{events, shared_types};
    auto registry_b = tempest::ecs::basic_archetype_registry{events, shared_types};

    // 3. Act: create entities with components in both registries.
    const auto entity_a = registry_a.create_initialized(component_alpha{.value = 42}, component_beta{.x = 1.0F, .y = 2.0F});
    const auto entity_b = registry_b.create_initialized(component_beta{.x = 3.0F, .y = 4.0F}, component_gamma{.identifier = 999});

    // 4. Assert: both registries have access to the same type indices.
    ASSERT_EQ(&registry_a.type_registry(), &shared_types);
    ASSERT_EQ(&registry_b.type_registry(), &shared_types);

    const auto alpha_idx_a = registry_a.type_registry().type_index<component_alpha>();
    const auto alpha_idx_b = registry_b.type_registry().type_index<component_alpha>();
    ASSERT_TRUE(alpha_idx_a.has_value());
    ASSERT_TRUE(alpha_idx_b.has_value());
    ASSERT_EQ(*alpha_idx_a, *alpha_idx_b);

    const auto beta_idx_a = registry_a.type_registry().type_index<component_beta>();
    const auto beta_idx_b = registry_b.type_registry().type_index<component_beta>();
    ASSERT_TRUE(beta_idx_a.has_value());
    ASSERT_TRUE(beta_idx_b.has_value());
    ASSERT_EQ(*beta_idx_a, *beta_idx_b);

    // 5. Assert: components are retrieved correctly from their respective entities.
    ASSERT_TRUE((registry_a.has<component_alpha, component_beta>(entity_a)));
    ASSERT_EQ(registry_a.get<component_alpha>(entity_a).value, 42);

    ASSERT_TRUE((registry_b.has<component_beta, component_gamma>(entity_b)));
    ASSERT_EQ(registry_b.get<component_gamma>(entity_b).identifier, 999ULL);
}

// ============================================================================
// Type Manifest & Schema Hash Tests
// ============================================================================

namespace
{
    struct reflected_manifest_component
    {
        float second_field{0.0F};
        int32_t first_field{0};
    };

    struct non_serializable_manifest_component
    {
        uint32_t tag{0};
    };
} // namespace

namespace tempest::ecs
{
    template <>
    struct is_serializable<non_serializable_manifest_component> : false_type
    {
    };

    template <>
    struct reflect<reflected_manifest_component>
    {
        static void describe(type_builder<reflected_manifest_component>& builder)
        {
            builder.version(3)
                .field<&reflected_manifest_component::first_field>("first_field")
                .field<&reflected_manifest_component::second_field>("second_field");
        }
    };
} // namespace tempest::ecs

/// @brief Verifies that export_manifest sorts types alphabetically by name and sorts
/// fields by offset then name deterministically.
TEST(component_type_registry_tests, export_manifest_deterministic_sorting)
{
    // 1. Setup: create two registries registering types in opposite orders.
    auto registry_a = tempest::ecs::component_type_registry{};
    auto registry_b = tempest::ecs::component_type_registry{};

    registry_a.register_type<component_gamma>();
    registry_a.register_type<reflected_manifest_component>();
    registry_a.register_type<component_alpha>();

    registry_b.register_type<component_alpha>();
    registry_b.register_type<component_gamma>();
    registry_b.register_type<reflected_manifest_component>();

    // 2. Act: export manifests.
    const auto manifest_a = registry_a.export_manifest();
    const auto manifest_b = registry_b.export_manifest();

    // 3. Assert: manifests match identically in size, schema hash, and entry ordering.
    ASSERT_EQ(manifest_a.types.size(), 3U);
    ASSERT_EQ(manifest_b.types.size(), 3U);
    EXPECT_EQ(manifest_a.schema_hash, manifest_b.schema_hash);
    EXPECT_EQ(manifest_a.to_json(), manifest_b.to_json());

    // 4. Assert: reflected component fields are sorted by offset (second_field at 0, first_field at 4).
    const auto* reflected_entry = static_cast<const tempest::ecs::type_manifest_entry*>(nullptr);
    for (const auto& entry : manifest_a.types)
    {
        if (entry.version == 3)
        {
            reflected_entry = &entry;
            break;
        }
    }
    ASSERT_NE(reflected_entry, nullptr);
    ASSERT_EQ(reflected_entry->fields.size(), 2U);
    EXPECT_EQ(reflected_entry->fields[0].name, "second_field");
    EXPECT_EQ(reflected_entry->fields[0].offset, 0U);
    EXPECT_EQ(reflected_entry->fields[1].name, "first_field");
    EXPECT_EQ(reflected_entry->fields[1].offset, 4U);
}

/// @brief Verifies that registering non-serializable components does not alter schema_hash
/// while still including them in the full exported manifest.
TEST(component_type_registry_tests, schema_hash_ignores_non_serializable_types)
{
    // 1. Setup: registry with only serializable components.
    auto base_registry = tempest::ecs::component_type_registry{};
    base_registry.register_type<component_alpha>();
    base_registry.register_type<component_beta>();

    const auto base_hash = base_registry.schema_hash();
    ASSERT_NE(base_hash, 0ULL);

    // 2. Act: add non-serializable component.
    base_registry.register_type<non_serializable_manifest_component>();

    // 3. Assert: schema_hash remains identical.
    EXPECT_EQ(base_registry.schema_hash(), base_hash);

    // 4. Assert: exported manifest contains the non-serializable entry marked correctly.
    const auto manifest = base_registry.export_manifest();
    EXPECT_EQ(manifest.schema_hash, base_hash);
    ASSERT_EQ(manifest.types.size(), 3U);

    auto found_non_serializable = false;
    for (const auto& entry : manifest.types)
    {
        if (!entry.is_serializable)
        {
            found_non_serializable = true;
            EXPECT_TRUE(entry.fields.empty());
        }
    }
    EXPECT_TRUE(found_non_serializable);
}

