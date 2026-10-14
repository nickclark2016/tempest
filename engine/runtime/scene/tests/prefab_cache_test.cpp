#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/atomic.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/string.hpp>
#include <tempest/thread.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

#if defined(_MSC_VER)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

namespace tempest_prefab_test
{
    struct test_prefab_component
    {
        int32_t val = 123;
        float weight = 4.5F;
    };

    static_assert(tempest::ecs::component<test_prefab_component>);
} // namespace tempest_prefab_test

namespace tempest::ecs
{
    template <>
    struct reflect<tempest_prefab_test::test_prefab_component>
    {
        static void describe(type_builder<tempest_prefab_test::test_prefab_component>& builder)
        {
            builder.version(1)
                .field<&tempest_prefab_test::test_prefab_component::val>("val")
                .field<&tempest_prefab_test::test_prefab_component::weight>("weight");
        }
    };
} // namespace tempest::ecs

//==============================================================================
// Prefab Template Construction & Hierarchy Tests
//==============================================================================

/// @brief Verifies that default-constructed prefab templates are empty and that records can be populated.
TEST(prefab_template_test, create_empty_and_valid_template)
{
    // Setup: Construct an empty prefab_template
    const auto empty_tmpl = tempest::scene::prefab_template{};

    // Act & Assert: Empty template invariants
    EXPECT_EQ(empty_tmpl.entity_count(), 0U);
    EXPECT_TRUE(empty_tmpl.is_empty());
    EXPECT_TRUE(empty_tmpl.get_records().empty());
    EXPECT_TRUE(empty_tmpl.get_root_indices().empty());
    EXPECT_EQ(empty_tmpl.get_root_record(), nullptr);
    EXPECT_EQ(empty_tmpl.get_record(0), nullptr);

    // Act: Construct a valid non-empty template with records
    auto records = tempest::vector<tempest::scene::prefab_entity_record>{};
    auto rec = tempest::scene::prefab_entity_record{
        .local_id = tempest::scene::entity_local_id{1},
        .name = "RootNode",
        .parent_index = tempest::nullopt,
    };
    records.push_back(tempest::move(rec));

    const auto populated_tmpl = tempest::scene::prefab_template{tempest::move(records)};

    // Assert: Populated template invariants
    EXPECT_EQ(populated_tmpl.entity_count(), 1U);
    EXPECT_FALSE(populated_tmpl.is_empty());
    EXPECT_EQ(populated_tmpl.get_records().size(), 1U);
    EXPECT_EQ(populated_tmpl.get_root_indices().size(), 1U);
    EXPECT_EQ(populated_tmpl.get_root_indices()[0], 0U);
    ASSERT_NE(populated_tmpl.get_root_record(), nullptr);
    EXPECT_EQ(populated_tmpl.get_root_record()->name, "RootNode");
    EXPECT_EQ(populated_tmpl.get_root_record()->local_id.value, 1ULL);
}

/// @brief Verifies parent-child hierarchy indexing and multi-root identification within a prefab template.
TEST(prefab_template_test, template_hierarchy_and_root_lookup)
{
    // Setup: Build a multi-entity tree hierarchy (Root, Child1, Child2, Grandchild)
    auto builder = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder.add_entity(tempest::scene::entity_local_id{10}, "Root");
    const auto child1_idx = builder.add_entity(tempest::scene::entity_local_id{20}, "Child1", root_idx);
    const auto child2_idx = builder.add_entity(tempest::scene::entity_local_id{30}, "Child2", root_idx);
    const auto grandchild_idx = builder.add_entity(tempest::scene::entity_local_id{40}, "Grandchild", child1_idx);

    // Act: Build the template
    const auto tmpl = builder.build();

    // Assert: Verify hierarchy and root records
    EXPECT_EQ(tmpl.entity_count(), 4U);
    EXPECT_EQ(tmpl.get_root_indices().size(), 1U);
    EXPECT_EQ(tmpl.get_root_indices()[0], root_idx);

    const auto* root_rec = tmpl.get_root_record();
    ASSERT_NE(root_rec, nullptr);
    EXPECT_EQ(root_rec->name, "Root");
    EXPECT_FALSE(root_rec->parent_index.has_value());

    const auto* c1_rec = tmpl.get_record(child1_idx);
    ASSERT_NE(c1_rec, nullptr);
    ASSERT_TRUE(c1_rec->parent_index.has_value());
    EXPECT_EQ(*c1_rec->parent_index, root_idx);

    const auto* c2_rec = tmpl.get_record(child2_idx);
    ASSERT_NE(c2_rec, nullptr);
    ASSERT_TRUE(c2_rec->parent_index.has_value());
    EXPECT_EQ(*c2_rec->parent_index, root_idx);

    const auto* gc_rec = tmpl.get_record(grandchild_idx);
    ASSERT_NE(gc_rec, nullptr);
    ASSERT_TRUE(gc_rec->parent_index.has_value());
    EXPECT_EQ(*gc_rec->parent_index, child1_idx);
}

/// @brief Verifies that entities can be looked up by their entity_local_id via fast indexed hash map.
TEST(prefab_template_test, find_record_by_local_id)
{
    // Setup: Build a template with disparate local IDs
    auto builder = tempest::scene::prefab_template_builder{};
    const auto id_a = tempest::scene::entity_local_id{0x1000'0001ULL};
    const auto id_b = tempest::scene::entity_local_id{0xABCD'0002ULL};
    const auto non_existent_id = tempest::scene::entity_local_id{0xDEAD'BEEFULL};

    builder.add_entity(id_a, "EntityA");
    builder.add_entity(id_b, "EntityB");

    // Act: Build template and query
    const auto tmpl = builder.build();

    // Assert: Existing records are found with correct metadata
    const auto idx_a_opt = tmpl.find_index(id_a);
    ASSERT_TRUE(idx_a_opt.has_value());
    EXPECT_EQ(*idx_a_opt, 0U);

    const auto* rec_a = tmpl.find_record(id_a);
    ASSERT_NE(rec_a, nullptr);
    EXPECT_EQ(rec_a->name, "EntityA");
    EXPECT_EQ(rec_a->local_id, id_a);

    const auto idx_b_opt = tmpl.find_index(id_b);
    ASSERT_TRUE(idx_b_opt.has_value());
    EXPECT_EQ(*idx_b_opt, 1U);

    const auto* rec_b = tmpl.find_record(id_b);
    ASSERT_NE(rec_b, nullptr);
    EXPECT_EQ(rec_b->name, "EntityB");
    EXPECT_EQ(rec_b->local_id, id_b);

    // Assert: Non-existent IDs return nullopt / nullptr
    EXPECT_FALSE(tmpl.find_index(non_existent_id).has_value());
    EXPECT_EQ(tmpl.find_record(non_existent_id), nullptr);
}

/// @brief Verifies conversion of staged entities into a prefab template.
TEST(prefab_template_test, from_staged_entities_conversion)
{
    // Setup: Prepare a vector of staged entities
    auto staged = tempest::vector<tempest::scene::staged_entity>{};
    auto root_staged = tempest::scene::staged_entity{
        .local_id = tempest::scene::entity_local_id{1},
        .name = "StagedRoot",
        .parent_index = tempest::nullopt,
        .components = {},
    };
    auto child_staged = tempest::scene::staged_entity{
        .local_id = tempest::scene::entity_local_id{2},
        .name = "StagedChild",
        .parent_index = 0U,
        .components = {},
    };
    staged.push_back(tempest::move(root_staged));
    staged.push_back(tempest::move(child_staged));

    // Act: Convert to prefab template
    const auto tmpl = tempest::scene::prefab_template::from_staged_entities(
        tempest::span<const tempest::scene::staged_entity>{staged.data(), staged.size()});

    // Assert: Invariants and data match staged source
    EXPECT_EQ(tmpl.entity_count(), 2U);
    EXPECT_EQ(tmpl.get_root_indices().size(), 1U);
    EXPECT_EQ(tmpl.get_root_indices()[0], 0U);

    const auto* root_rec = tmpl.get_record(0);
    ASSERT_NE(root_rec, nullptr);
    EXPECT_EQ(root_rec->name, "StagedRoot");

    const auto* child_rec = tmpl.get_record(1);
    ASSERT_NE(child_rec, nullptr);
    EXPECT_EQ(child_rec->name, "StagedChild");
    ASSERT_TRUE(child_rec->parent_index.has_value());
    EXPECT_EQ(*child_rec->parent_index, 0U);
}

/// @brief Verifies extracting a prefab template directly from an existing live ECS entity hierarchy.
TEST(prefab_template_test, from_entity_conversion)
{
    // Setup: ECS registry with reflected test component
    auto event_reg = tempest::event::event_registry{};
    auto type_reg = tempest::ecs::component_type_registry{};
    type_reg.ensure<tempest_prefab_test::test_prefab_component>();

    auto registry = tempest::ecs::basic_archetype_registry{event_reg, type_reg};

    const auto root_ent = registry.create<tempest_prefab_test::test_prefab_component>();
    registry.name(root_ent, "ECSRoot");
    registry.replace(root_ent, tempest_prefab_test::test_prefab_component{.val = 777, .weight = 9.25F});

    const auto child_ent = registry.create();
    registry.name(child_ent, "ECSChild");
    tempest::ecs::create_parent_child_relationship(registry, root_ent, child_ent);

    // Act: Extract template from ECS root entity
    const auto tmpl = tempest::scene::prefab_template::from_entity(registry, root_ent);

    // Assert: Invariants and components are captured
    EXPECT_EQ(tmpl.entity_count(), 2U);
    EXPECT_EQ(tmpl.get_root_indices().size(), 1U);
    EXPECT_EQ(tmpl.get_root_indices()[0], 0U);

    const auto* root_rec = tmpl.get_record(0);
    ASSERT_NE(root_rec, nullptr);
    EXPECT_EQ(root_rec->name, "ECSRoot");
    EXPECT_FALSE(root_rec->parent_index.has_value());
    EXPECT_EQ(root_rec->components.size(), 1U);

    const auto* child_rec = tmpl.get_record(1);
    ASSERT_NE(child_rec, nullptr);
    EXPECT_EQ(child_rec->name, "ECSChild");
    ASSERT_TRUE(child_rec->parent_index.has_value());
    EXPECT_EQ(*child_rec->parent_index, 0U);
}

//==============================================================================
// Prefab Cache Lifecycle & Thread Safety Tests
//==============================================================================

/// @brief Verifies registration, inspection, and unregistration of templates within prefab_cache.
TEST(prefab_cache_test, cache_register_find_unregister)
{
    // Setup: Cache and dummy template
    auto cache = tempest::scene::prefab_cache{};
    const auto test_guid = tempest::guid::generate_random_guid();

    auto builder = tempest::scene::prefab_template_builder{};
    builder.add_entity(tempest::scene::entity_local_id{1}, "CachedRoot");
    const auto tmpl = builder.build();

    EXPECT_EQ(cache.template_count(), 0U);
    EXPECT_FALSE(cache.has_template(test_guid));

    // Act: Register template
    const auto reg_ok = cache.register_template(test_guid, tmpl);
    EXPECT_TRUE(reg_ok);

    // Duplicate registration should fail
    const auto dup_ok = cache.register_template(test_guid, tmpl);
    EXPECT_FALSE(dup_ok);

    // Assert: Template exists and can be looked up / inspected
    EXPECT_EQ(cache.template_count(), 1U);
    EXPECT_TRUE(cache.has_template(test_guid));

    const auto found_tmpl_opt = cache.find_template(test_guid);
    ASSERT_TRUE(found_tmpl_opt.has_value());
    EXPECT_EQ(found_tmpl_opt->entity_count(), 1U);
    EXPECT_EQ(found_tmpl_opt->get_root_record()->name, "CachedRoot");

    // Zero-copy inspection via with_template
    auto visited = false;
    const auto with_ok = cache.with_template(test_guid, [&](const tempest::scene::prefab_template& inspected) {
        visited = true;
        EXPECT_EQ(inspected.entity_count(), 1U);
        EXPECT_EQ(inspected.get_root_record()->name, "CachedRoot");
    });
    EXPECT_TRUE(with_ok);
    EXPECT_TRUE(visited);

    // Act: Unregister template
    const auto unreg_ok = cache.unregister_template(test_guid);
    EXPECT_TRUE(unreg_ok);

    // Unregister non-existent returns false
    const auto unreg_again = cache.unregister_template(test_guid);
    EXPECT_FALSE(unreg_again);

    EXPECT_EQ(cache.template_count(), 0U);
    EXPECT_FALSE(cache.has_template(test_guid));

    // Test clear()
    cache.register_template(tempest::guid::generate_random_guid(), tmpl);
    cache.register_template(tempest::guid::generate_random_guid(), tmpl);
    EXPECT_EQ(cache.template_count(), 2U);
    cache.clear();
    EXPECT_EQ(cache.template_count(), 0U);
}

/// @brief Verifies that prefab templates and prefab cache remain completely off-world without mutating the ECS registry.
TEST(prefab_cache_test, off_world_isolation_no_registry_mutation)
{
    // Setup: Event registry, component type registry, and ECS archetype registry
    auto event_reg = tempest::event::event_registry{};
    auto type_reg = tempest::ecs::component_type_registry{};
    auto registry = tempest::ecs::basic_archetype_registry{event_reg, type_reg};

    // Pre-populate an entity in the world registry to establish baseline
    const auto existing_ent = registry.create();
    registry.name(existing_ent, "PreExistingEntity");
    const auto initial_entity_count = registry.size();
    EXPECT_EQ(initial_entity_count, 1U);

    auto cache = tempest::scene::prefab_cache{};

    // Act: Construct complex templates with records and nested prefab descriptors, and store them in the cache
    auto builder = tempest::scene::prefab_template_builder{};
    const auto root_id = tempest::scene::entity_local_id{100};
    const auto child_id = tempest::scene::entity_local_id{200};
    const auto nested_guid = tempest::guid::generate_random_guid();

    const auto root_idx = builder.add_entity(root_id, "TemplateRoot");
    const auto child_idx = builder.add_entity(child_id, "TemplateChild", root_idx);
    builder.set_nested_prefab(child_idx, nested_guid, tempest::scene::entity_local_id{1});

    const auto tmpl = builder.build();
    const auto tmpl_guid = tempest::guid::generate_random_guid();

    const auto reg_ok = cache.register_template(tmpl_guid, tmpl);
    EXPECT_TRUE(reg_ok);

    // Look up and inspect records repeatedly
    const auto found = cache.find_template(tmpl_guid);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->entity_count(), 2U);

    cache.with_template(tmpl_guid, [](const tempest::scene::prefab_template& t) {
        EXPECT_EQ(t.entity_count(), 2U);
    });

    // Assert: Invariant verification - the world registry was never touched
    EXPECT_EQ(registry.size(), initial_entity_count);
    EXPECT_TRUE(registry.is_valid(existing_ent));
    const auto name_opt = registry.name(existing_ent);
    ASSERT_TRUE(name_opt.has_value());
    EXPECT_EQ(*name_opt, "PreExistingEntity");
}

/// @brief Verifies that prefab_cache supports concurrent multi-threaded reads under shared_lock without contention or race.
TEST(prefab_cache_test, multithreaded_concurrent_reads)
{
    // Setup: Populate prefab_cache with multiple templates
    auto cache = tempest::scene::prefab_cache{};
    static constexpr size_t template_count = 8;
    auto guids = tempest::vector<tempest::guid>{};

    for (auto index = size_t{0}; index < template_count; ++index)
    {
        const auto generated_guid = tempest::guid::generate_random_guid();
        guids.push_back(generated_guid);

        auto builder = tempest::scene::prefab_template_builder{};
        builder.add_entity(tempest::scene::entity_local_id{index + 1}, "WorkerTemplate");
        cache.register_template(generated_guid, builder.build());
    }

    EXPECT_EQ(cache.template_count(), template_count);

    // Act: Spawn multiple reader worker threads reading concurrently
    static constexpr size_t thread_count = 4;
    static constexpr size_t iterations_per_thread = 200;
    auto threads = tempest::vector<tempest::thread>{};
    auto success_counter = tempest::atomic<uint32_t>{0};

    for (auto thread_idx = size_t{0}; thread_idx < thread_count; ++thread_idx)
    {
        threads.push_back(tempest::thread{[&cache, &guids, &success_counter]() -> void {
            for (auto iter = size_t{0}; iter < iterations_per_thread; ++iter)
            {
                for (const auto& item_guid : guids)
                {
                    if (cache.has_template(item_guid))
                    {
                        const auto found = cache.find_template(item_guid);
                        if (found.has_value() && found->entity_count() == 1U)
                        {
                            cache.with_template(item_guid, [&](const tempest::scene::prefab_template& tmpl) {
                                if (tmpl.entity_count() == 1U)
                                {
                                    success_counter.fetch_add(1, tempest::memory_order::relaxed);
                                }
                            });
                        }
                    }
                }
            }
        }});
    }

    // Assert: Join all threads and verify all concurrent reads completed successfully
    for (auto& worker : threads)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    constexpr uint32_t expected_successes =
        static_cast<uint32_t>(thread_count * iterations_per_thread * template_count);
    EXPECT_EQ(success_counter.load(tempest::memory_order::relaxed), expected_successes);
}
