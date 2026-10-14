#include <tempest/scene/model_prefab.hpp>

#include <gtest/gtest.h>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_instantiator.hpp>
#include <tempest/scene/prefab_operations.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/string.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

#if defined(_MSC_VER)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

namespace tempest_model_prefab_test
{
    struct model_test_data_component
    {
        static constexpr int32_t default_val = 100;
        static constexpr float default_weight = 2.5F;

        int32_t val = default_val;
        float weight = default_weight;
    };

    struct model_test_extra_component
    {
        static constexpr int32_t default_extra = 555;

        int32_t extra = default_extra;
    };

    static_assert(tempest::ecs::component<model_test_data_component>);
    static_assert(tempest::ecs::component<model_test_extra_component>);
} // namespace tempest_model_prefab_test

namespace tempest::ecs
{
    template <>
    struct reflect<tempest_model_prefab_test::model_test_data_component>
    {
        static void describe(type_builder<tempest_model_prefab_test::model_test_data_component>& builder)
        {
            builder.version(1)
                .field<&tempest_model_prefab_test::model_test_data_component::val>("val")
                .field<&tempest_model_prefab_test::model_test_data_component::weight>("weight");
        }
    };

    template <>
    struct reflect<tempest_model_prefab_test::model_test_extra_component>
    {
        static void describe(type_builder<tempest_model_prefab_test::model_test_extra_component>& builder)
        {
            builder.version(1).field<&tempest_model_prefab_test::model_test_extra_component::extra>("extra");
        }
    };
} // namespace tempest::ecs

namespace tempest_model_prefab_test
{
    template <typename T>
    auto to_bytes(const T& val) -> tempest::vector<tempest::byte>
    {
        auto bytes = tempest::vector<tempest::byte>(sizeof(T));
        tempest::memcpy(bytes.data(), &val, sizeof(T));
        return bytes;
    }

    struct test_registry_context
    {
        tempest::event::event_registry event_reg{};
        tempest::ecs::component_type_registry type_reg{};
        tempest::ecs::archetype_registry registry{event_reg, type_reg};

        uint32_t data_comp_type_idx{0};
        uint32_t extra_comp_type_idx{0};

        test_registry_context()
        {
            data_comp_type_idx = type_reg.ensure<model_test_data_component>();
            extra_comp_type_idx = type_reg.ensure<model_test_extra_component>();
            const auto tf_idx = type_reg.ensure<tempest::ecs::transform_component>();
            type_reg.ensure<tempest::scene::prefab_instance_component>();
            type_reg.ensure<tempest::scene::prefab_member_component>();
            type_reg.ensure<tempest::scene::scene_entity_component>();

            type_reg.add_alias("model_test_data_component", type_reg.type_info(data_comp_type_idx).name);
            type_reg.add_alias("model_test_extra_component", type_reg.type_info(extra_comp_type_idx).name);
            type_reg.add_alias("transform_component", type_reg.type_info(tf_idx).name);
        }
    };
} // namespace tempest_model_prefab_test

//==============================================================================
// Deterministic Model Prefab Local ID Tests
//==============================================================================

/// @brief Verifies fnv1a64 and compute_model_prefab_local_id generate stable, deterministic 64-bit local IDs across
/// simulated re-imports with distinct paths and GUIDs.
TEST(model_prefab_tests, gltf_reimport_preserves_deterministic_member_ids)
{
    // 1. Setup distinct GUIDs and node paths
    const auto guid_alpha = *tempest::parse_guid("11111111-2222-3333-4444-555555555555");
    const auto guid_beta = *tempest::parse_guid("99999999-8888-7777-6666-555555555555");

    const auto path_root = tempest::string_view{"/Root"};
    const auto path_node_zero = tempest::string_view{"/Root/Node_0"};
    const auto path_node_one = tempest::string_view{"/Root/Node_1"};

    // 2. Act: compute hashes and local IDs
    const auto hash_alpha_root = tempest::scene::fnv1a64(guid_alpha, path_root);
    const auto hash_alpha_root_repeat = tempest::scene::fnv1a64(guid_alpha, path_root);
    const auto hash_alpha_node_zero = tempest::scene::fnv1a64(guid_alpha, path_node_zero);
    const auto hash_alpha_node_one = tempest::scene::fnv1a64(guid_alpha, path_node_one);
    const auto hash_beta_node_zero = tempest::scene::fnv1a64(guid_beta, path_node_zero);

    const auto id_alpha_node_zero = tempest::scene::compute_model_prefab_local_id(guid_alpha, path_node_zero);
    const auto id_alpha_node_zero_repeat = tempest::scene::compute_model_prefab_local_id(guid_alpha, path_node_zero);
    const auto id_beta_node_zero = tempest::scene::compute_model_prefab_local_id(guid_beta, path_node_zero);

    // 3. Assert: stability and uniqueness
    EXPECT_EQ(hash_alpha_root, hash_alpha_root_repeat);
    EXPECT_NE(hash_alpha_root, hash_alpha_node_zero);
    EXPECT_NE(hash_alpha_node_zero, hash_alpha_node_one);
    EXPECT_NE(hash_alpha_node_zero, hash_beta_node_zero);

    EXPECT_TRUE(id_alpha_node_zero.is_valid());
    EXPECT_EQ(id_alpha_node_zero, id_alpha_node_zero_repeat);
    EXPECT_NE(id_alpha_node_zero, id_beta_node_zero);

    // 4. Act & Assert: simulated re-import with synthesize_model_prefab
    auto context_first = tempest_model_prefab_test::test_registry_context{};
    const auto root_first = context_first.registry.create();
    context_first.registry.name(root_first, "Root");
    context_first.registry.assign(root_first, tempest::ecs::transform_component{});
    context_first.registry.assign(root_first, tempest_model_prefab_test::model_test_data_component{
                                                  .val = 1,
                                                  .weight = 1.0F,
                                              });

    const auto child_first = context_first.registry.create();
    context_first.registry.name(child_first, "Node_0");
    context_first.registry.assign(child_first, tempest::ecs::transform_component{});
    context_first.registry.assign(child_first, tempest_model_prefab_test::model_test_data_component{
                                                   .val = 2,
                                                   .weight = 2.0F,
                                               });
    tempest::ecs::create_parent_child_relationship(context_first.registry, root_first, child_first);

    const auto template_first = tempest::scene::synthesize_model_prefab(context_first.registry, root_first, guid_alpha);
    ASSERT_EQ(template_first.entity_count(), 2U);

    // Requirement 7c: Root transform_component is omitted from template records
    const auto* root_record_first = template_first.get_record(0);
    ASSERT_NE(root_record_first, nullptr);
    EXPECT_EQ(root_record_first->local_id, tempest::scene::compute_model_prefab_local_id(guid_alpha, "/Root"));
    auto root_has_transform = false;
    for (const auto& comp : root_record_first->components)
    {
        if (comp.type_info != nullptr && comp.type_info->name == "tempest::ecs::transform_component")
        {
            root_has_transform = true;
            break;
        }
    }
    EXPECT_FALSE(root_has_transform);

    // Child record keeps its deterministic local_id and components
    const auto* child_record_first = template_first.get_record(1);
    ASSERT_NE(child_record_first, nullptr);
    EXPECT_EQ(child_record_first->local_id, id_alpha_node_zero);

    // Re-import into a second independent registry
    auto context_second = tempest_model_prefab_test::test_registry_context{};
    const auto root_second = context_second.registry.create();
    context_second.registry.name(root_second, "Root");
    context_second.registry.assign(root_second, tempest::ecs::transform_component{});
    context_second.registry.assign(root_second, tempest_model_prefab_test::model_test_data_component{
                                                    .val = 1,
                                                    .weight = 1.0F,
                                                });

    const auto child_second = context_second.registry.create();
    context_second.registry.name(child_second, "Node_0");
    context_second.registry.assign(child_second, tempest::ecs::transform_component{});
    context_second.registry.assign(child_second, tempest_model_prefab_test::model_test_data_component{
                                                     .val = 2,
                                                     .weight = 2.0F,
                                                 });
    tempest::ecs::create_parent_child_relationship(context_second.registry, root_second, child_second);

    const auto template_second =
        tempest::scene::synthesize_model_prefab(context_second.registry, root_second, guid_alpha);
    ASSERT_EQ(template_second.entity_count(), 2U);

    // Local IDs match bit-identically across imports
    EXPECT_EQ(template_first.get_record(0)->local_id, template_second.get_record(0)->local_id);
    EXPECT_EQ(template_first.get_record(1)->local_id, template_second.get_record(1)->local_id);
}

//==============================================================================
// Hot-Propagation In-Place Update Tests
//==============================================================================

/// @brief Instantiates a prefab instance into registry; records member ecs::entity handles; updates template in cache;
/// invokes propagate_template_updates; asserts surviving member entity handles are bit-identical, non-overridden fields
/// reflect updated template values, and overridden fields retain their instance values.
TEST(model_prefab_tests, propagation_preserves_live_entity_handles)
{
    // 1. Setup initial template V1 with Root and Child
    auto context = tempest_model_prefab_test::test_registry_context{};
    auto cache = tempest::scene::prefab_cache{};
    const auto prefab_guid = tempest::guid::generate_random_guid();

    const auto local_id_root = tempest::scene::entity_local_id{100};
    const auto local_id_child = tempest::scene::entity_local_id{200};

    auto builder_v1 = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder_v1.add_entity(local_id_root, "Root", tempest::nullopt);
    const auto child_idx = builder_v1.add_entity(local_id_child, "Child", root_idx);

    auto root_comp_v1 = tempest_model_prefab_test::model_test_data_component{.val = 10, .weight = 1.0F};
    auto child_comp_v1 = tempest_model_prefab_test::model_test_data_component{.val = 20, .weight = 2.0F};

    const auto* data_info = &context.type_reg.type_info(context.type_reg.ensure<tempest_model_prefab_test::model_test_data_component>());
    builder_v1.add_component(root_idx, data_info,
                             tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(&root_comp_v1),
                                                                sizeof(root_comp_v1)});
    builder_v1.add_component(child_idx, data_info,
                             tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(&child_comp_v1),
                                                                sizeof(child_comp_v1)});

    auto template_v1 = builder_v1.build();
    ASSERT_TRUE(cache.register_template(prefab_guid, tempest::move(template_v1)));

    // Create field override on child: weight = 99.0F
    auto overrides = tempest::scene::prefab_overrides{};
    auto child_path = tempest::scene::id_path{};
    child_path.push_back(local_id_child);

    const auto override_weight = 99.0F;
    auto override_bytes = tempest::vector<tempest::byte>(sizeof(float));
    tempest::memcpy(override_bytes.data(), &override_weight, sizeof(float));

    overrides.field_overrides.push_back(tempest::scene::field_override{
        .target = child_path,
        .component_name = "model_test_data_component",
        .field_name = "weight",
        .value_bytes = tempest::move(override_bytes),
    });

    // Instantiate prefab instance into registry
    const auto inst_res =
        tempest::scene::prefab_instantiator::instantiate(prefab_guid, cache, context.registry, overrides);
    ASSERT_TRUE(inst_res.has_value());

    const auto live_root_entity = inst_res->root_entity;
    const auto live_child_entity = inst_res->local_to_entity.find(local_id_child.value)->second;

    ASSERT_TRUE(context.registry.is_valid(live_root_entity));
    ASSERT_TRUE(context.registry.is_valid(live_child_entity));

    EXPECT_EQ(context.registry.get<tempest_model_prefab_test::model_test_data_component>(live_root_entity).val, 10);
    EXPECT_FLOAT_EQ(context.registry.get<tempest_model_prefab_test::model_test_data_component>(live_root_entity).weight, 1.0F);
    EXPECT_EQ(context.registry.get<tempest_model_prefab_test::model_test_data_component>(live_child_entity).val, 20);
    EXPECT_FLOAT_EQ(context.registry.get<tempest_model_prefab_test::model_test_data_component>(live_child_entity).weight, 99.0F);

    // 2. Act: build updated template V2 and propagate updates
    auto builder_v2 = tempest::scene::prefab_template_builder{};
    const auto root_idx_v2 = builder_v2.add_entity(local_id_root, "Root", tempest::nullopt);
    const auto child_idx_v2 = builder_v2.add_entity(local_id_child, "Child", root_idx_v2);

    auto root_comp_v2 = tempest_model_prefab_test::model_test_data_component{.val = 111, .weight = 11.0F};
    auto child_comp_v2 = tempest_model_prefab_test::model_test_data_component{.val = 222, .weight = 22.0F};

    builder_v2.add_component(root_idx_v2, data_info,
                             tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(&root_comp_v2),
                                                                sizeof(root_comp_v2)});
    builder_v2.add_component(child_idx_v2, data_info,
                             tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(&child_comp_v2),
                                                                sizeof(child_comp_v2)});

    auto template_v2 = builder_v2.build();
    cache.unregister_template(prefab_guid);
    ASSERT_TRUE(cache.register_template(prefab_guid, tempest::move(template_v2)));

    const auto updated_count = cache.propagate_updates(context.registry, prefab_guid);

    // 3. Assert: live entity handles preserved, non-overridden fields updated, overridden field retained
    EXPECT_EQ(updated_count, 1U);

    EXPECT_TRUE(context.registry.is_valid(live_root_entity));
    EXPECT_TRUE(context.registry.is_valid(live_child_entity));

    // Root fields updated from template V2
    EXPECT_EQ(context.registry.get<tempest_model_prefab_test::model_test_data_component>(live_root_entity).val, 111);
    EXPECT_FLOAT_EQ(context.registry.get<tempest_model_prefab_test::model_test_data_component>(live_root_entity).weight, 11.0F);

    // Child non-overridden val updated to 222; overridden weight retained at 99.0F
    EXPECT_EQ(context.registry.get<tempest_model_prefab_test::model_test_data_component>(live_child_entity).val, 222);
    EXPECT_FLOAT_EQ(context.registry.get<tempest_model_prefab_test::model_test_data_component>(live_child_entity).weight, 99.0F);

    // Preserved active overrides
    const auto* instance_comp = context.registry.try_get<tempest::scene::prefab_instance_component>(live_root_entity);
    ASSERT_NE(instance_comp, nullptr);
    EXPECT_EQ(instance_comp->overrides.field_overrides.size(), 1U);
}

/// @brief Verifies that adding members to template creates new entities on the instance, and removing members destroys their entities.
TEST(model_prefab_tests, propagation_adds_and_removes_template_members)
{
    // 1. Setup template V1 with Root, ChildA, ChildB
    auto context = tempest_model_prefab_test::test_registry_context{};
    auto cache = tempest::scene::prefab_cache{};
    const auto prefab_guid = tempest::guid::generate_random_guid();

    const auto local_id_root = tempest::scene::entity_local_id{10};
    const auto local_id_child_a = tempest::scene::entity_local_id{20};
    const auto local_id_child_b = tempest::scene::entity_local_id{30};
    const auto local_id_child_c = tempest::scene::entity_local_id{40};

    auto builder_v1 = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder_v1.add_entity(local_id_root, "Root", tempest::nullopt);
    builder_v1.add_entity(local_id_child_a, "ChildA", root_idx);
    builder_v1.add_entity(local_id_child_b, "ChildB", root_idx);

    auto template_v1 = builder_v1.build();
    ASSERT_TRUE(cache.register_template(prefab_guid, tempest::move(template_v1)));

    const auto inst_res =
        tempest::scene::prefab_instantiator::instantiate(prefab_guid, cache, context.registry);
    ASSERT_TRUE(inst_res.has_value());

    const auto live_root = inst_res->root_entity;
    const auto live_child_a = inst_res->local_to_entity.find(local_id_child_a.value)->second;
    const auto live_child_b = inst_res->local_to_entity.find(local_id_child_b.value)->second;

    ASSERT_TRUE(context.registry.is_valid(live_root));
    ASSERT_TRUE(context.registry.is_valid(live_child_a));
    ASSERT_TRUE(context.registry.is_valid(live_child_b));

    // 2. Act: build template V2 where ChildB is removed and ChildC is added
    auto builder_v2 = tempest::scene::prefab_template_builder{};
    const auto root_idx_v2 = builder_v2.add_entity(local_id_root, "Root", tempest::nullopt);
    builder_v2.add_entity(local_id_child_a, "ChildA", root_idx_v2);
    builder_v2.add_entity(local_id_child_c, "ChildC", root_idx_v2);

    auto template_v2 = builder_v2.build();
    cache.unregister_template(prefab_guid);
    ASSERT_TRUE(cache.register_template(prefab_guid, tempest::move(template_v2)));

    const auto updated_count =
        tempest::scene::prefab_operations::propagate_template_updates(context.registry, prefab_guid, cache);

    // 3. Assert: ChildB destroyed, ChildC created with proper parent relationship
    EXPECT_EQ(updated_count, 1U);

    EXPECT_TRUE(context.registry.is_valid(live_root));
    EXPECT_TRUE(context.registry.is_valid(live_child_a));
    EXPECT_FALSE(context.registry.is_valid(live_child_b));

    // Find new ChildC entity by its prefab_member_component
    auto live_child_c = tempest::optional<tempest::ecs::entity>{};
    context.registry.each([&](const tempest::ecs::self_component& self,
                              const tempest::scene::prefab_member_component& mem) {
        if (mem.template_local_id == local_id_child_c)
        {
            live_child_c = self.entity;
        }
    });

    ASSERT_TRUE(live_child_c.has_value());
    EXPECT_TRUE(context.registry.is_valid(*live_child_c));
    EXPECT_EQ(context.registry.name(*live_child_c).value_or(""), "ChildC");

    const auto* child_c_rel =
        context.registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(*live_child_c);
    ASSERT_NE(child_c_rel, nullptr);
    EXPECT_EQ(child_c_rel->parent, live_root);
}

// ============================================================================
// Section: Sibling Disambiguation & Hot-Propagation Edge Cases
// ============================================================================

/// @brief Verifies that synthesize_model_prefab disambiguates siblings even when names collide (e.g. "Part_1" and "Part").
TEST(model_prefab_tests, disambiguation_avoids_sibling_name_collisions)
{
    // 1. Setup: Create hierarchy with siblings named "Part_1", "Part", "Part"
    auto context = tempest_model_prefab_test::test_registry_context{};
    const auto source_guid = tempest::guid::generate_random_guid();

    const auto root_ent = context.registry.create();
    context.registry.name(root_ent, "ModelRoot");

    const auto sibling_0 = context.registry.create();
    context.registry.name(sibling_0, "Part_1");
    tempest::ecs::create_parent_child_relationship(context.registry, root_ent, sibling_0);

    const auto sibling_1 = context.registry.create();
    context.registry.name(sibling_1, "Part");
    tempest::ecs::create_parent_child_relationship(context.registry, root_ent, sibling_1);

    const auto sibling_2 = context.registry.create();
    context.registry.name(sibling_2, "Part");
    tempest::ecs::create_parent_child_relationship(context.registry, root_ent, sibling_2);

    // 2. Act: Synthesize model prefab
    const auto tmpl = tempest::scene::synthesize_model_prefab(context.registry, root_ent, source_guid);

    // 3. Assert: 4 distinct records with unique, non-colliding local IDs
    EXPECT_EQ(tmpl.entity_count(), 4U);

    auto seen_local_ids = tempest::flat_unordered_map<uint64_t, size_t>{};
    for (auto i = size_t{0}; i < tmpl.entity_count(); ++i)
    {
        const auto* rec = tmpl.get_record(i);
        ASSERT_NE(rec, nullptr);
        EXPECT_TRUE(rec->local_id.is_valid());
        EXPECT_EQ(seen_local_ids.find(rec->local_id.value), seen_local_ids.end());
        seen_local_ids.insert({rec->local_id.value, i});
    }
}

/// @brief Verifies that hot-propagation preserves added and removed component overrides on an instance.
TEST(model_prefab_tests, propagation_preserves_added_and_removed_component_overrides)
{
    // 1. Setup: Build template with child possessing data component and extra component
    auto context = tempest_model_prefab_test::test_registry_context{};
    auto cache = tempest::scene::prefab_cache{};
    const auto prefab_guid = tempest::guid::generate_random_guid();

    const auto local_id_root = tempest::scene::entity_local_id{10};
    const auto local_id_child = tempest::scene::entity_local_id{20};

    auto builder_v1 = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder_v1.add_entity(local_id_root, "Root", tempest::nullopt);
    const auto child_idx = builder_v1.add_entity(local_id_child, "Child", root_idx);

    const auto data_comp_v1 = tempest_model_prefab_test::model_test_data_component{.val = 10, .weight = 1.0F};
    const auto bytes_v1 = tempest_model_prefab_test::to_bytes(data_comp_v1);
    builder_v1.add_component(child_idx, &context.type_reg.type_info(context.data_comp_type_idx),
                             tempest::span<const tempest::byte>{bytes_v1.data(), bytes_v1.size()});

    ASSERT_TRUE(cache.register_template(prefab_guid, builder_v1.build()));

    // Instantiate with:
    // - removed_component: removes model_test_data_component from Child
    // - added_component: adds model_test_extra_component to Child
    auto overrides = tempest::scene::prefab_overrides{};
    auto child_target = tempest::scene::id_path{};
    child_target.push_back(local_id_child);

    overrides.removed_components.push_back(tempest::scene::removed_component{
        .target = child_target,
        .component_name = tempest::string{"model_test_data_component"},
    });

    const auto extra_comp = tempest_model_prefab_test::model_test_extra_component{.extra = 777};
    const auto extra_bytes = tempest_model_prefab_test::to_bytes(extra_comp);
    overrides.added_components.push_back(tempest::scene::added_component{
        .target = child_target,
        .component = tempest::scene::staged_component{
            .type_info = &context.type_reg.type_info(context.extra_comp_type_idx),
            .data = extra_bytes,
        },
    });

    auto inst_res = tempest::scene::prefab_instantiator::instantiate(prefab_guid, cache, context.registry, overrides,
                                                                     tempest::scene::entity_local_id{1});
    ASSERT_TRUE(inst_res.has_value());
    const auto live_child = inst_res->local_to_entity[local_id_child.value];
    EXPECT_FALSE(context.registry.has<tempest_model_prefab_test::model_test_data_component>(live_child));
    EXPECT_TRUE(context.registry.has<tempest_model_prefab_test::model_test_extra_component>(live_child));

    // 2. Act: Update template V2 with updated data component values and propagate
    auto builder_v2 = tempest::scene::prefab_template_builder{};
    const auto root_idx_v2 = builder_v2.add_entity(local_id_root, "Root", tempest::nullopt);
    const auto child_idx_v2 = builder_v2.add_entity(local_id_child, "Child", root_idx_v2);

    const auto data_comp_v2 = tempest_model_prefab_test::model_test_data_component{.val = 999, .weight = 99.0F};
    const auto bytes_v2 = tempest_model_prefab_test::to_bytes(data_comp_v2);
    builder_v2.add_component(child_idx_v2, &context.type_reg.type_info(context.data_comp_type_idx),
                             tempest::span<const tempest::byte>{bytes_v2.data(), bytes_v2.size()});

    cache.unregister_template(prefab_guid);
    ASSERT_TRUE(cache.register_template(prefab_guid, builder_v2.build()));

    const auto updated =
        tempest::scene::prefab_operations::propagate_template_updates(context.registry, prefab_guid, cache);
    EXPECT_EQ(updated, 1U);

    // 3. Assert: Child still does NOT have model_test_data_component (removed override kept),
    // and still possesses model_test_extra_component (added override kept)
    EXPECT_FALSE(context.registry.has<tempest_model_prefab_test::model_test_data_component>(live_child));
    EXPECT_TRUE(context.registry.has<tempest_model_prefab_test::model_test_extra_component>(live_child));
    const auto* extra_ptr = context.registry.try_get<tempest_model_prefab_test::model_test_extra_component>(live_child);
    ASSERT_NE(extra_ptr, nullptr);
    EXPECT_EQ(extra_ptr->extra, 777);
}

/// @brief Verifies that an instance-added non-prefab child attached to a template member is re-anchored when that member is deleted.
TEST(model_prefab_tests, propagation_reanchors_instance_added_children_when_parent_removed)
{
    // 1. Setup: Template with Root -> ChildA
    auto context = tempest_model_prefab_test::test_registry_context{};
    auto cache = tempest::scene::prefab_cache{};
    const auto prefab_guid = tempest::guid::generate_random_guid();

    const auto local_id_root = tempest::scene::entity_local_id{10};
    const auto local_id_child_a = tempest::scene::entity_local_id{20};

    auto builder_v1 = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder_v1.add_entity(local_id_root, "Root", tempest::nullopt);
    builder_v1.add_entity(local_id_child_a, "ChildA", root_idx);
    ASSERT_TRUE(cache.register_template(prefab_guid, builder_v1.build()));

    auto inst_res = tempest::scene::prefab_instantiator::instantiate(prefab_guid, cache, context.registry, {},
                                                                     tempest::scene::entity_local_id{1});
    ASSERT_TRUE(inst_res.has_value());
    const auto live_root = inst_res->root_entity;
    const auto live_child_a = inst_res->local_to_entity[local_id_child_a.value];

    // Create an instance-added child entity parented under live_child_a
    const auto custom_child = context.registry.create();
    context.registry.name(custom_child, "CustomAddedChild");
    tempest::ecs::create_parent_child_relationship(context.registry, live_child_a, custom_child);

    // Record custom_child in prefab_instance_component::overrides.added_children
    auto inst_comp = context.registry.get<tempest::scene::prefab_instance_component>(live_root);
    inst_comp.overrides.added_children.push_back(tempest::scene::entity_local_id{9999});
    context.registry.assign_or_replace(live_root, inst_comp);

    // 2. Act: Update template V2 omitting ChildA entirely (so ChildA is destroyed)
    auto builder_v2 = tempest::scene::prefab_template_builder{};
    builder_v2.add_entity(local_id_root, "Root", tempest::nullopt);
    cache.unregister_template(prefab_guid);
    ASSERT_TRUE(cache.register_template(prefab_guid, builder_v2.build()));

    const auto updated =
        tempest::scene::prefab_operations::propagate_template_updates(context.registry, prefab_guid, cache);
    EXPECT_EQ(updated, 1U);

    // 3. Assert: ChildA is destroyed, custom_child is still valid and re-anchored to live_root
    EXPECT_FALSE(context.registry.is_valid(live_child_a));
    EXPECT_TRUE(context.registry.is_valid(custom_child));

    const auto* custom_rel =
        context.registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(custom_child);
    ASSERT_NE(custom_rel, nullptr);
    EXPECT_EQ(custom_rel->parent, live_root);
}

/// @brief Verifies that propagate_template_updates updates multiple active instances in the same world.
TEST(model_prefab_tests, propagation_across_multiple_instances)
{
    // 1. Setup: Template with Root -> Child
    auto context = tempest_model_prefab_test::test_registry_context{};
    auto cache = tempest::scene::prefab_cache{};
    const auto prefab_guid = tempest::guid::generate_random_guid();

    const auto local_id_root = tempest::scene::entity_local_id{10};
    const auto local_id_child = tempest::scene::entity_local_id{20};

    auto builder_v1 = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder_v1.add_entity(local_id_root, "Root", tempest::nullopt);
    const auto child_idx = builder_v1.add_entity(local_id_child, "Child", root_idx);

    const auto data_comp_v1 = tempest_model_prefab_test::model_test_data_component{.val = 50, .weight = 1.0F};
    const auto bytes_v1 = tempest_model_prefab_test::to_bytes(data_comp_v1);
    builder_v1.add_component(child_idx, &context.type_reg.type_info(context.data_comp_type_idx),
                             tempest::span<const tempest::byte>{bytes_v1.data(), bytes_v1.size()});

    ASSERT_TRUE(cache.register_template(prefab_guid, builder_v1.build()));

    // Instantiate two distinct instances
    auto inst_1 = tempest::scene::prefab_instantiator::instantiate(prefab_guid, cache, context.registry, {},
                                                                   tempest::scene::entity_local_id{1});
    ASSERT_TRUE(inst_1.has_value());
    const auto live_child_1 = inst_1->local_to_entity[local_id_child.value];

    auto inst_2 = tempest::scene::prefab_instantiator::instantiate(prefab_guid, cache, context.registry, {},
                                                                   tempest::scene::entity_local_id{2});
    ASSERT_TRUE(inst_2.has_value());
    const auto live_child_2 = inst_2->local_to_entity[local_id_child.value];

    // 2. Act: Update template V2 with val = 777
    auto builder_v2 = tempest::scene::prefab_template_builder{};
    const auto root_idx_v2 = builder_v2.add_entity(local_id_root, "Root", tempest::nullopt);
    const auto child_idx_v2 = builder_v2.add_entity(local_id_child, "Child", root_idx_v2);

    const auto data_comp_v2 = tempest_model_prefab_test::model_test_data_component{.val = 777, .weight = 2.0F};
    const auto bytes_v2 = tempest_model_prefab_test::to_bytes(data_comp_v2);
    builder_v2.add_component(child_idx_v2, &context.type_reg.type_info(context.data_comp_type_idx),
                             tempest::span<const tempest::byte>{bytes_v2.data(), bytes_v2.size()});

    cache.unregister_template(prefab_guid);
    ASSERT_TRUE(cache.register_template(prefab_guid, builder_v2.build()));

    const auto updated =
        tempest::scene::prefab_operations::propagate_template_updates(context.registry, prefab_guid, cache);

    // 3. Assert: Both instances updated
    EXPECT_EQ(updated, 2U);

    const auto* comp_1 = context.registry.try_get<tempest_model_prefab_test::model_test_data_component>(live_child_1);
    ASSERT_NE(comp_1, nullptr);
    EXPECT_EQ(comp_1->val, 777);

    const auto* comp_2 = context.registry.try_get<tempest_model_prefab_test::model_test_data_component>(live_child_2);
    ASSERT_NE(comp_2, nullptr);
    EXPECT_EQ(comp_2->val, 777);
}
