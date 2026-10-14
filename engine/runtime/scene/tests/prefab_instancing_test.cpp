#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_instantiator.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/string.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

#if defined(_MSC_VER)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

namespace tempest_prefab_instancing_test
{
    struct test_data_component
    {
        int32_t val = 123;
        float weight = 4.5F;
    };

    static_assert(tempest::ecs::component<test_data_component>);

    template <typename T>
    auto to_bytes(const T& value) -> tempest::vector<tempest::byte>
    {
        auto bytes = tempest::vector<tempest::byte>(sizeof(T));
        tempest::memcpy(bytes.data(), &value, sizeof(T));
        return bytes;
    }

    struct test_registry_context
    {
        tempest::event::event_registry event_reg{};
        tempest::ecs::component_type_registry type_reg{};
        tempest::ecs::archetype_registry registry{event_reg, type_reg};

        test_registry_context()
        {
            const auto data_idx = type_reg.ensure<test_data_component>();
            const auto tf_idx = type_reg.ensure<tempest::ecs::transform_component>();
            type_reg.ensure<tempest::scene::prefab_instance_component>();
            type_reg.ensure<tempest::scene::prefab_member_component>();

            type_reg.add_alias("test_data_component", type_reg.type_info(data_idx).name);
            type_reg.add_alias("transform_component", type_reg.type_info(tf_idx).name);
        }
    };
} // namespace tempest_prefab_instancing_test

namespace tempest::ecs
{
    template <>
    struct reflect<tempest_prefab_instancing_test::test_data_component>
    {
        static void describe(type_builder<tempest_prefab_instancing_test::test_data_component>& builder)
        {
            builder.version(1)
                .field<&tempest_prefab_instancing_test::test_data_component::val>("val")
                .field<&tempest_prefab_instancing_test::test_data_component::weight>("weight");
        }
    };
} // namespace tempest::ecs

//==============================================================================
// Basic Prefab Instantiation Tests
//==============================================================================

/// @brief Verifies that basic template records instantiate into world entities with proper
/// member and instance components, hierarchy relationships, and local_to_entity mappings.
TEST(prefab_instancing_test, instantiate_basic_template)
{
    // Setup: Register component types and build a two-entity template
    auto ctx = tempest_prefab_instancing_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto& type_reg = ctx.type_reg;

    const auto* comp_info = type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_instancing_test::test_data_component>());
    ASSERT_NE(comp_info, nullptr);

    auto builder = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder.add_entity(tempest::scene::entity_local_id{1}, "RootNode");
    const auto child_idx = builder.add_entity(tempest::scene::entity_local_id{2}, "ChildNode", root_idx);

    const auto root_data = tempest_prefab_instancing_test::test_data_component{
        .val = 42,
        .weight = 1.5F,
    };
    const auto child_data = tempest_prefab_instancing_test::test_data_component{
        .val = 100,
        .weight = 2.5F,
    };
    builder.add_component(root_idx, comp_info, tempest_prefab_instancing_test::to_bytes(root_data));
    builder.add_component(child_idx, comp_info, tempest_prefab_instancing_test::to_bytes(child_data));

    const auto tmpl = builder.build();
    const auto test_guid = tempest::guid::generate_random_guid();
    const auto inst_local_id = tempest::scene::entity_local_id{99};

    // Act: Instantiate the template
    const auto result = tempest::scene::prefab_instantiator::instantiate(tmpl, registry, {}, nullptr, test_guid,
                                                                        inst_local_id);

    // Assert: Entity instantiation and component population
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->all_entities.size(), 2U);

    const auto root_entity = result->root_entity;
    ASSERT_TRUE(registry.is_valid(root_entity));
    EXPECT_EQ(registry.name(root_entity).value_or(""), "RootNode");

    // Root entity has prefab_instance_component and prefab_member_component
    const auto* inst_comp = registry.try_get<tempest::scene::prefab_instance_component>(root_entity);
    ASSERT_NE(inst_comp, nullptr);
    EXPECT_EQ(inst_comp->prefab_guid, test_guid);
    EXPECT_EQ(inst_comp->instance_local_id, inst_local_id);

    const auto* root_member = registry.try_get<tempest::scene::prefab_member_component>(root_entity);
    ASSERT_NE(root_member, nullptr);
    EXPECT_EQ(root_member->template_local_id.value, 1ULL);

    // Child entity hierarchy and member component
    const auto child_iter = result->local_to_entity.find(2ULL);
    ASSERT_NE(child_iter, result->local_to_entity.end());
    const auto child_entity = child_iter->second;
    ASSERT_TRUE(registry.is_valid(child_entity));
    EXPECT_EQ(registry.name(child_entity).value_or(""), "ChildNode");

    const auto* child_member = registry.try_get<tempest::scene::prefab_member_component>(child_entity);
    ASSERT_NE(child_member, nullptr);
    EXPECT_EQ(child_member->template_local_id.value, 2ULL);

    const auto* child_rel = registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(child_entity);
    ASSERT_NE(child_rel, nullptr);
    EXPECT_EQ(child_rel->parent, root_entity);

    // Component data verified
    const auto* root_comp_val = registry.try_get<tempest_prefab_instancing_test::test_data_component>(root_entity);
    ASSERT_NE(root_comp_val, nullptr);
    EXPECT_EQ(root_comp_val->val, 42);
    EXPECT_FLOAT_EQ(root_comp_val->weight, 1.5F);

    const auto* child_comp_val = registry.try_get<tempest_prefab_instancing_test::test_data_component>(child_entity);
    ASSERT_NE(child_comp_val, nullptr);
    EXPECT_EQ(child_comp_val->val, 100);
    EXPECT_FLOAT_EQ(child_comp_val->weight, 2.5F);
}

//==============================================================================
// Prefab Overrides Tests
//==============================================================================

/// @brief Verifies that field overrides targeting both root and child entities modify
/// component values and execute post_load lifecycle callbacks.
TEST(prefab_instancing_test, instantiate_applies_field_overrides)
{
    // Setup: Register reflected component types and build template
    auto ctx = tempest_prefab_instancing_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto& type_reg = ctx.type_reg;

    const auto* test_comp_info = type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_instancing_test::test_data_component>());
    const auto* transform_info = type_reg.type_info(
        tempest::core::normalized_type_name<tempest::ecs::transform_component>());
    ASSERT_NE(test_comp_info, nullptr);
    ASSERT_NE(transform_info, nullptr);

    auto builder = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder.add_entity(tempest::scene::entity_local_id{1}, "Root");
    const auto child_idx = builder.add_entity(tempest::scene::entity_local_id{2}, "Child", root_idx);

    const auto initial_data = tempest_prefab_instancing_test::test_data_component{
        .val = 10,
        .weight = 1.0F,
    };
    builder.add_component(root_idx, test_comp_info, tempest_prefab_instancing_test::to_bytes(initial_data));
    builder.add_component(child_idx, test_comp_info, tempest_prefab_instancing_test::to_bytes(initial_data));

    const auto initial_transform = tempest::ecs::transform_component{};
    builder.add_component(root_idx, transform_info, tempest_prefab_instancing_test::to_bytes(initial_transform));

    const auto tmpl = builder.build();

    // Configure overrides: root val -> 999, child weight -> 7.5, root position -> (10, 20, 30)
    auto overrides = tempest::scene::prefab_overrides{};
    overrides.field_overrides.push_back(tempest::scene::field_override{
        .target = {},
        .component_name = "test_data_component",
        .field_name = "val",
        .value_bytes = tempest_prefab_instancing_test::to_bytes(int32_t{999}),
    });
    auto child_target = tempest::scene::id_path{};
    child_target.push_back(tempest::scene::entity_local_id{2});
    overrides.field_overrides.push_back(tempest::scene::field_override{
        .target = child_target,
        .component_name = "test_data_component",
        .field_name = "weight",
        .value_bytes = tempest_prefab_instancing_test::to_bytes(float{7.5F}),
    });
    overrides.field_overrides.push_back(tempest::scene::field_override{
        .target = {},
        .component_name = "transform_component",
        .field_name = "position",
        .value_bytes = tempest_prefab_instancing_test::to_bytes(tempest::math::vec3<float>{10.0F, 20.0F, 30.0F}),
    });

    // Act: Instantiate with overrides
    const auto result = tempest::scene::prefab_instantiator::instantiate(tmpl, registry, overrides);

    // Assert: Overridden field values and post_load transform update
    ASSERT_TRUE(result.has_value());
    const auto root_ent = result->root_entity;
    const auto child_iter = result->local_to_entity.find(2ULL);
    ASSERT_NE(child_iter, result->local_to_entity.end());
    const auto child_ent = child_iter->second;

    const auto* root_data_val = registry.try_get<tempest_prefab_instancing_test::test_data_component>(root_ent);
    ASSERT_NE(root_data_val, nullptr);
    EXPECT_EQ(root_data_val->val, 999);
    EXPECT_FLOAT_EQ(root_data_val->weight, 1.0F);

    const auto* child_data_val = registry.try_get<tempest_prefab_instancing_test::test_data_component>(child_ent);
    ASSERT_NE(child_data_val, nullptr);
    EXPECT_EQ(child_data_val->val, 10);
    EXPECT_FLOAT_EQ(child_data_val->weight, 7.5F);

    const auto* root_transform = registry.try_get<tempest::ecs::transform_component>(root_ent);
    ASSERT_NE(root_transform, nullptr);
    EXPECT_FLOAT_EQ(root_transform->position().x, 10.0F);
    EXPECT_FLOAT_EQ(root_transform->position().y, 20.0F);
    EXPECT_FLOAT_EQ(root_transform->position().z, 30.0F);

    // Verify post_load recomputed matrix translation
    const auto matrix = root_transform->matrix();
    EXPECT_FLOAT_EQ(matrix[3][0], 10.0F);
    EXPECT_FLOAT_EQ(matrix[3][1], 20.0F);
    EXPECT_FLOAT_EQ(matrix[3][2], 30.0F);
}

/// @brief Verifies that added_component entries attach new components to target entities.
TEST(prefab_instancing_test, instantiate_applies_added_components)
{
    // Setup: Template with empty root entity
    auto ctx = tempest_prefab_instancing_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto& type_reg = ctx.type_reg;

    const auto* comp_info = type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_instancing_test::test_data_component>());
    ASSERT_NE(comp_info, nullptr);

    auto builder = tempest::scene::prefab_template_builder{};
    builder.add_entity(tempest::scene::entity_local_id{1}, "EmptyRoot");
    const auto tmpl = builder.build();

    // Configure added component
    auto overrides = tempest::scene::prefab_overrides{};
    const auto new_comp_data = tempest_prefab_instancing_test::test_data_component{
        .val = 777,
        .weight = 8.8F,
    };
    overrides.added_components.push_back(tempest::scene::added_component{
        .target = {},
        .component =
            tempest::scene::staged_component{
                .type_info = comp_info,
                .data = tempest_prefab_instancing_test::to_bytes(new_comp_data),
            },
    });

    // Act: Instantiate with added component
    const auto result = tempest::scene::prefab_instantiator::instantiate(tmpl, registry, overrides);

    // Assert: Component is assigned and populated
    ASSERT_TRUE(result.has_value());
    const auto root_ent = result->root_entity;
    EXPECT_TRUE(registry.has<tempest_prefab_instancing_test::test_data_component>(root_ent));

    const auto* comp_ptr = registry.try_get<tempest_prefab_instancing_test::test_data_component>(root_ent);
    ASSERT_NE(comp_ptr, nullptr);
    EXPECT_EQ(comp_ptr->val, 777);
    EXPECT_FLOAT_EQ(comp_ptr->weight, 8.8F);
}

/// @brief Verifies that removed_component entries prevent components from being assigned during instantiation.
TEST(prefab_instancing_test, instantiate_applies_removed_components)
{
    // Setup: Template with root containing test_data_component and transform_component
    auto ctx = tempest_prefab_instancing_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto& type_reg = ctx.type_reg;

    const auto* test_comp_info = type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_instancing_test::test_data_component>());
    const auto* transform_info = type_reg.type_info(
        tempest::core::normalized_type_name<tempest::ecs::transform_component>());

    auto builder = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder.add_entity(tempest::scene::entity_local_id{1}, "Root");

    const auto test_comp = tempest_prefab_instancing_test::test_data_component{.val = 50, .weight = 2.0F};
    const auto trans_comp = tempest::ecs::transform_component{};
    builder.add_component(root_idx, test_comp_info, tempest_prefab_instancing_test::to_bytes(test_comp));
    builder.add_component(root_idx, transform_info, tempest_prefab_instancing_test::to_bytes(trans_comp));

    const auto tmpl = builder.build();

    // Configure removed component: remove test_data_component
    auto overrides = tempest::scene::prefab_overrides{};
    overrides.removed_components.push_back(tempest::scene::removed_component{
        .target = {},
        .component_name = "test_data_component",
    });

    // Act: Instantiate template
    const auto result = tempest::scene::prefab_instantiator::instantiate(tmpl, registry, overrides);

    // Assert: test_data_component was omitted, transform_component remains
    ASSERT_TRUE(result.has_value());
    const auto root_ent = result->root_entity;
    EXPECT_FALSE(registry.has<tempest_prefab_instancing_test::test_data_component>(root_ent));
    EXPECT_TRUE(registry.has<tempest::ecs::transform_component>(root_ent));
}

//==============================================================================
// Nested Prefab & id_path Addressing Tests
//==============================================================================

/// @brief Verifies recursive instantiation of nested prefabs and override application via hierarchical id_path.
TEST(prefab_instancing_test, instantiate_nested_prefab)
{
    // Setup: Register types and create inner and outer prefab templates
    auto ctx = tempest_prefab_instancing_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto& type_reg = ctx.type_reg;

    const auto* comp_info = type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_instancing_test::test_data_component>());
    ASSERT_NE(comp_info, nullptr);

    auto cache = tempest::scene::prefab_cache{};
    const auto inner_guid = tempest::guid::generate_random_guid();
    const auto outer_guid = tempest::guid::generate_random_guid();

    // Inner template: root (ID 10) with child (ID 20) having test_data_component
    auto inner_builder = tempest::scene::prefab_template_builder{};
    const auto inner_root_idx = inner_builder.add_entity(tempest::scene::entity_local_id{10}, "InnerRoot");
    const auto inner_child_idx =
        inner_builder.add_entity(tempest::scene::entity_local_id{20}, "InnerChild", inner_root_idx);

    const auto inner_data = tempest_prefab_instancing_test::test_data_component{.val = 100, .weight = 1.0F};
    inner_builder.add_component(inner_child_idx, comp_info, tempest_prefab_instancing_test::to_bytes(inner_data));
    cache.register_template(inner_guid, inner_builder.build());

    // Outer template: root (ID 1) with child (ID 2) hosting nested instance of inner prefab
    auto outer_builder = tempest::scene::prefab_template_builder{};
    const auto outer_root_idx = outer_builder.add_entity(tempest::scene::entity_local_id{1}, "OuterRoot");
    const auto outer_host_idx =
        outer_builder.add_entity(tempest::scene::entity_local_id{2}, "NestedHost", outer_root_idx);
    outer_builder.set_nested_prefab(outer_host_idx, inner_guid, tempest::scene::entity_local_id{200});

    const auto outer_tmpl = outer_builder.build();
    cache.register_template(outer_guid, outer_tmpl);

    // Overrides on outer instance targeting nested child via id_path [2, 20]
    auto overrides = tempest::scene::prefab_overrides{};
    auto nested_target = tempest::scene::id_path{};
    nested_target.push_back(tempest::scene::entity_local_id{2});
    nested_target.push_back(tempest::scene::entity_local_id{20});

    overrides.field_overrides.push_back(tempest::scene::field_override{
        .target = nested_target,
        .component_name = "test_data_component",
        .field_name = "val",
        .value_bytes = tempest_prefab_instancing_test::to_bytes(int32_t{555}),
    });
    overrides.field_overrides.push_back(tempest::scene::field_override{
        .target = nested_target,
        .component_name = "test_data_component",
        .field_name = "weight",
        .value_bytes = tempest_prefab_instancing_test::to_bytes(float{9.25F}),
    });

    // Act: Instantiate outer template
    const auto result = tempest::scene::prefab_instantiator::instantiate(outer_guid, cache, registry, overrides);

    // Assert: Hierarchy and override application
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->all_entities.size(), 4U);

    const auto outer_root = result->root_entity;
    const auto nested_host_iter = result->local_to_entity.find(2ULL);
    ASSERT_NE(nested_host_iter, result->local_to_entity.end());
    const auto nested_host = nested_host_iter->second;

    const auto inner_root_iter = result->local_to_entity.find(10ULL);
    ASSERT_NE(inner_root_iter, result->local_to_entity.end());
    const auto inner_root = inner_root_iter->second;

    const auto inner_child_iter = result->local_to_entity.find(20ULL);
    ASSERT_NE(inner_child_iter, result->local_to_entity.end());
    const auto inner_child = inner_child_iter->second;

    // Verify parent-child hierarchy across nested boundary
    const auto* host_rel = registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(nested_host);
    ASSERT_NE(host_rel, nullptr);
    EXPECT_EQ(host_rel->parent, outer_root);

    const auto* inner_root_rel =
        registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(inner_root);
    ASSERT_NE(inner_root_rel, nullptr);
    EXPECT_EQ(inner_root_rel->parent, nested_host);

    const auto* inner_child_rel =
        registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(inner_child);
    ASSERT_NE(inner_child_rel, nullptr);
    EXPECT_EQ(inner_child_rel->parent, inner_root);

    // Verify nested child component was overridden
    const auto* overridden_comp =
        registry.try_get<tempest_prefab_instancing_test::test_data_component>(inner_child);
    ASSERT_NE(overridden_comp, nullptr);
    EXPECT_EQ(overridden_comp->val, 555);
    EXPECT_FLOAT_EQ(overridden_comp->weight, 9.25F);

    // Verify inner root has its own prefab_instance_component
    const auto* inner_inst_comp = registry.try_get<tempest::scene::prefab_instance_component>(inner_root);
    ASSERT_NE(inner_inst_comp, nullptr);
    EXPECT_EQ(inner_inst_comp->prefab_guid, inner_guid);
}

//==============================================================================
// Cycle Detection & Error Handling Tests
//==============================================================================

/// @brief Verifies that self-referential or cyclic nested prefabs are rejected with cyclic_nesting error.
TEST(prefab_instancing_test, cyclic_nesting_is_rejected)
{
    // Setup: Direct self-referencing template A -> A
    auto ctx = tempest_prefab_instancing_test::test_registry_context{};
    auto& registry = ctx.registry;

    auto cache = tempest::scene::prefab_cache{};
    const auto self_guid = tempest::guid::generate_random_guid();

    auto self_builder = tempest::scene::prefab_template_builder{};
    const auto self_root_idx = self_builder.add_entity(tempest::scene::entity_local_id{1}, "SelfRoot");
    self_builder.set_nested_prefab(self_root_idx, self_guid, tempest::scene::entity_local_id{10});
    cache.register_template(self_guid, self_builder.build());

    // Act: Attempt instantiating self-referential prefab
    const auto self_result = tempest::scene::prefab_instantiator::instantiate(self_guid, cache, registry);

    // Assert: Direct cycle is detected and rejected
    ASSERT_FALSE(self_result.has_value());
    EXPECT_EQ(self_result.error(), tempest::scene::prefab_error::cyclic_nesting);

    // Setup: Mutual cycle A -> B -> A
    const auto guid_a = tempest::guid::generate_random_guid();
    const auto guid_b = tempest::guid::generate_random_guid();

    auto builder_a = tempest::scene::prefab_template_builder{};
    const auto root_a_idx = builder_a.add_entity(tempest::scene::entity_local_id{1}, "RootA");
    builder_a.set_nested_prefab(root_a_idx, guid_b, tempest::scene::entity_local_id{10});
    cache.register_template(guid_a, builder_a.build());

    auto builder_b = tempest::scene::prefab_template_builder{};
    const auto root_b_idx = builder_b.add_entity(tempest::scene::entity_local_id{2}, "RootB");
    builder_b.set_nested_prefab(root_b_idx, guid_a, tempest::scene::entity_local_id{20});
    cache.register_template(guid_b, builder_b.build());

    // Act: Attempt instantiating mutual cycle
    const auto mutual_result = tempest::scene::prefab_instantiator::instantiate(guid_a, cache, registry);

    // Assert: Mutual cycle is detected and rejected
    ASSERT_FALSE(mutual_result.has_value());
    EXPECT_EQ(mutual_result.error(), tempest::scene::prefab_error::cyclic_nesting);
}

/// @brief Verifies error conditions: empty template, missing template in cache, and invalid override paths.
TEST(prefab_instancing_test, error_handling_and_validation)
{
    auto ctx = tempest_prefab_instancing_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto& type_reg = ctx.type_reg;

    auto cache = tempest::scene::prefab_cache{};

    // Empty template
    const auto empty_tmpl = tempest::scene::prefab_template{};
    const auto empty_res = tempest::scene::prefab_instantiator::instantiate(empty_tmpl, registry);
    ASSERT_FALSE(empty_res.has_value());
    EXPECT_EQ(empty_res.error(), tempest::scene::prefab_error::invalid_template);

    // Missing template in cache
    const auto missing_guid = tempest::guid::generate_random_guid();
    const auto missing_res = tempest::scene::prefab_instantiator::instantiate(missing_guid, cache, registry);
    ASSERT_FALSE(missing_res.has_value());
    EXPECT_EQ(missing_res.error(), tempest::scene::prefab_error::template_not_found);

    // Build a valid simple template
    auto builder = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder.add_entity(tempest::scene::entity_local_id{1}, "Root");
    const auto* comp_info = type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_instancing_test::test_data_component>());
    const auto data = tempest_prefab_instancing_test::test_data_component{};
    builder.add_component(root_idx, comp_info, tempest_prefab_instancing_test::to_bytes(data));
    const auto valid_tmpl = builder.build();

    // Override targeting non-existent field
    auto invalid_field_overrides = tempest::scene::prefab_overrides{};
    invalid_field_overrides.field_overrides.push_back(tempest::scene::field_override{
        .target = {},
        .component_name = "test_data_component",
        .field_name = "non_existent_field",
        .value_bytes = tempest_prefab_instancing_test::to_bytes(int32_t{1}),
    });
    const auto bad_field_res =
        tempest::scene::prefab_instantiator::instantiate(valid_tmpl, registry, invalid_field_overrides);
    ASSERT_FALSE(bad_field_res.has_value());
    EXPECT_EQ(bad_field_res.error(), tempest::scene::prefab_error::field_not_found);

    // Override targeting non-existent entity
    auto bad_target_overrides = tempest::scene::prefab_overrides{};
    auto bad_path = tempest::scene::id_path{};
    bad_path.push_back(tempest::scene::entity_local_id{99999});
    bad_target_overrides.field_overrides.push_back(tempest::scene::field_override{
        .target = bad_path,
        .component_name = "test_data_component",
        .field_name = "val",
        .value_bytes = tempest_prefab_instancing_test::to_bytes(int32_t{1}),
    });
    const auto bad_target_res =
        tempest::scene::prefab_instantiator::instantiate(valid_tmpl, registry, bad_target_overrides);
    ASSERT_FALSE(bad_target_res.has_value());
    EXPECT_EQ(bad_target_res.error(), tempest::scene::prefab_error::component_not_found);
}

/// @brief Verifies that id_path formatting via to_string produces correct slash-separated hex strings.
TEST(prefab_instancing_test, id_path_to_string_formatting)
{
    // Setup: Empty id_path
    const auto empty_path = tempest::scene::id_path{};
    EXPECT_EQ(tempest::scene::to_string(empty_path), "");

    // Act & Assert: Single element id_path
    auto single_path = tempest::scene::id_path{};
    single_path.push_back(tempest::scene::entity_local_id{1});
    EXPECT_EQ(tempest::scene::to_string(single_path), "0x0000000000000001");

    // Act & Assert: Multi-element id_path
    auto multi_path = tempest::scene::id_path{};
    multi_path.push_back(tempest::scene::entity_local_id{1});
    multi_path.push_back(tempest::scene::entity_local_id{0xbeef});
    EXPECT_EQ(tempest::scene::to_string(multi_path), "0x0000000000000001/0x000000000000beef");
}
