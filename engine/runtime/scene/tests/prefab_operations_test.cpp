#include <tempest/scene/prefab_operations.hpp>

#include <gtest/gtest.h>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/math.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_codec.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_instantiator.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/string.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

#if defined(_MSC_VER)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

namespace tempest_prefab_ops_test
{
    struct test_op_data_component
    {
        int32_t val = 123;
        float weight = 4.5F;
    };

    struct test_op_extra_component
    {
        int32_t extra_value = 999;
    };

    static_assert(tempest::ecs::component<test_op_data_component>);
    static_assert(tempest::ecs::component<test_op_extra_component>);

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
            const auto data_idx = type_reg.ensure<test_op_data_component>();
            const auto extra_idx = type_reg.ensure<test_op_extra_component>();
            const auto tf_idx = type_reg.ensure<tempest::ecs::transform_component>();
            type_reg.ensure<tempest::scene::prefab_instance_component>();
            type_reg.ensure<tempest::scene::prefab_member_component>();
            type_reg.ensure<tempest::scene::scene_entity_component>();

            type_reg.add_alias("test_op_data_component", type_reg.type_info(data_idx).name);
            type_reg.add_alias("test_op_extra_component", type_reg.type_info(extra_idx).name);
            type_reg.add_alias("transform_component", type_reg.type_info(tf_idx).name);
        }
    };
} // namespace tempest_prefab_ops_test

namespace tempest::ecs
{
    template <>
    struct reflect<tempest_prefab_ops_test::test_op_data_component>
    {
        static void describe(type_builder<tempest_prefab_ops_test::test_op_data_component>& builder)
        {
            builder.version(1)
                .field<&tempest_prefab_ops_test::test_op_data_component::val>("val")
                .field<&tempest_prefab_ops_test::test_op_data_component::weight>("weight");
        }
    };

    template <>
    struct reflect<tempest_prefab_ops_test::test_op_extra_component>
    {
        static void describe(type_builder<tempest_prefab_ops_test::test_op_extra_component>& builder)
        {
            builder.version(1).field<&tempest_prefab_ops_test::test_op_extra_component::extra_value>("extra_value");
        }
    };
} // namespace tempest::ecs

//==============================================================================
// Prefab Save As Operation Tests
//==============================================================================

/// @brief Verifies that save_as_prefab creates a template, registers it in cache, writes a valid
/// .tprefab JSON document, tags live entities in-place with member/instance components, and preserves root transform.
TEST(prefab_operations_test, save_as_prefab_links_in_place_and_writes_file)
{
    // Setup: Author live entity hierarchy with root and child
    auto ctx = tempest_prefab_ops_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto cache = tempest::scene::prefab_cache{};

    const auto root_ent = registry.create();
    registry.name(root_ent, "AuthorRoot");
    registry.assign(root_ent, tempest_prefab_ops_test::test_op_data_component{.val = 42, .weight = 1.5F});

    auto root_tf = tempest::ecs::transform_component{};
    root_tf.position(tempest::math::vec3<float>{10.0F, 20.0F, 30.0F});
    registry.assign(root_ent, root_tf);

    const auto child_ent = registry.create();
    registry.name(child_ent, "AuthorChild");
    registry.assign(child_ent, tempest_prefab_ops_test::test_op_data_component{.val = 84, .weight = 3.0F});
    tempest::ecs::create_parent_child_relationship(registry, root_ent, child_ent);

    const auto prefab_guid = tempest::guid::generate_random_guid();
    const auto temp_file_path = tempest::string{"bin/Debug/test_author_prefab.tprefab"};

    // Act: Save as prefab
    const auto save_res =
        tempest::scene::prefab_operations::save_as_prefab(registry, root_ent, prefab_guid, temp_file_path, cache);

    // Assert: Verify template generation and in-place component linkage
    ASSERT_TRUE(save_res.has_value());
    const auto& tmpl = *save_res;
    EXPECT_EQ(tmpl.entity_count(), 2U);

    // Cache registration
    EXPECT_TRUE(cache.has_template(prefab_guid));

    // Root entity tagged as prefab instance
    const auto* inst_comp = registry.try_get<tempest::scene::prefab_instance_component>(root_ent);
    ASSERT_NE(inst_comp, nullptr);
    EXPECT_EQ(inst_comp->prefab_guid, prefab_guid);
    EXPECT_TRUE(inst_comp->overrides.empty());

    // Both root and child tagged with prefab_member_component
    const auto* root_member = registry.try_get<tempest::scene::prefab_member_component>(root_ent);
    ASSERT_NE(root_member, nullptr);
    EXPECT_TRUE(root_member->template_local_id.is_valid());

    const auto* child_member = registry.try_get<tempest::scene::prefab_member_component>(child_ent);
    ASSERT_NE(child_member, nullptr);
    EXPECT_TRUE(child_member->template_local_id.is_valid());
    EXPECT_NE(root_member->template_local_id, child_member->template_local_id);

    // Root transform component preserved on live entity (Requirement 7c)
    const auto* live_tf = registry.try_get<tempest::ecs::transform_component>(root_ent);
    ASSERT_NE(live_tf, nullptr);
    EXPECT_FLOAT_EQ(live_tf->position().x, 10.0F);
    EXPECT_FLOAT_EQ(live_tf->position().y, 20.0F);
    EXPECT_FLOAT_EQ(live_tf->position().z, 30.0F);

    // Hierarchy intact
    const auto* child_rel = registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(child_ent);
    ASSERT_NE(child_rel, nullptr);
    EXPECT_EQ(child_rel->parent, root_ent);

    // Disk file was written and is valid JSON .tprefab
    const auto file_content_res = tempest::read_file_to_string(tempest::filesystem::path{temp_file_path});
    ASSERT_TRUE(file_content_res.has_value());
    const auto deser_res = tempest::scene::prefab_codec::deserialize(*file_content_res, ctx.type_reg);
    ASSERT_TRUE(deser_res.has_value());
    EXPECT_TRUE(deser_res->success);
    EXPECT_EQ(deser_res->prefab_guid, prefab_guid);
    EXPECT_EQ(deser_res->template_record.entity_count(), 2U);

    // Clean up temporary file
    tempest::filesystem::remove(tempest::filesystem::path{temp_file_path});
}

//==============================================================================
// Prefab Revert Operation Tests
//==============================================================================

/// @brief Verifies that revert_field_override restores component fields to template defaults
/// and removes the field override record from the instance component.
TEST(prefab_operations_test, revert_field_override_restores_template_value)
{
    // Setup: Build template, register in cache, and instantiate with field override
    auto ctx = tempest_prefab_ops_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto cache = tempest::scene::prefab_cache{};

    const auto* comp_info = ctx.type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_ops_test::test_op_data_component>());
    ASSERT_NE(comp_info, nullptr);

    auto builder = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder.add_entity(tempest::scene::entity_local_id{1}, "TestRoot");
    const auto child_idx = builder.add_entity(tempest::scene::entity_local_id{2}, "TestChild", root_idx);

    const auto root_data = tempest_prefab_ops_test::test_op_data_component{.val = 10, .weight = 1.0F};
    const auto child_data = tempest_prefab_ops_test::test_op_data_component{.val = 20, .weight = 2.0F};
    builder.add_component(root_idx, comp_info, tempest_prefab_ops_test::to_bytes(root_data));
    builder.add_component(child_idx, comp_info, tempest_prefab_ops_test::to_bytes(child_data));

    const auto tmpl = builder.build();
    const auto prefab_guid = tempest::guid::generate_random_guid();
    cache.register_template(prefab_guid, tmpl);

    auto overrides = tempest::scene::prefab_overrides{};
    auto child_path = tempest::scene::id_path{};
    child_path.push_back(tempest::scene::entity_local_id{2});

    overrides.field_overrides.push_back(tempest::scene::field_override{
        .target = child_path,
        .component_name = "test_op_data_component",
        .field_name = "val",
        .value_bytes = tempest_prefab_ops_test::to_bytes(int32_t{777}),
    });

    const auto inst_res = tempest::scene::prefab_instantiator::instantiate(prefab_guid, cache, registry, overrides);
    ASSERT_TRUE(inst_res.has_value());
    const auto root_ent = inst_res->root_entity;
    const auto child_iter = inst_res->local_to_entity.find(2ULL);
    ASSERT_NE(child_iter, inst_res->local_to_entity.end());
    const auto child_ent = child_iter->second;

    // Verify child has overridden val = 777
    const auto* child_comp_before = registry.try_get<tempest_prefab_ops_test::test_op_data_component>(child_ent);
    ASSERT_NE(child_comp_before, nullptr);
    EXPECT_EQ(child_comp_before->val, 777);

    // Act: Revert field override on child
    const auto revert_res = tempest::scene::prefab_operations::revert_field_override(
        registry, root_ent, child_path, "test_op_data_component", "val", cache);

    // Assert: Value reverted to template default 20 and override removed
    ASSERT_TRUE(revert_res.has_value());

    const auto* child_comp_after = registry.try_get<tempest_prefab_ops_test::test_op_data_component>(child_ent);
    ASSERT_NE(child_comp_after, nullptr);
    EXPECT_EQ(child_comp_after->val, 20);

    const auto* inst_comp = registry.try_get<tempest::scene::prefab_instance_component>(root_ent);
    ASSERT_NE(inst_comp, nullptr);
    EXPECT_TRUE(inst_comp->overrides.field_overrides.empty());
}

/// @brief Verifies reverting both added components (removed from entity) and removed components (restored onto entity).
TEST(prefab_operations_test, revert_added_and_removed_components)
{
    // Setup: Build template with data component and extra component
    auto ctx = tempest_prefab_ops_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto cache = tempest::scene::prefab_cache{};

    const auto* data_info = ctx.type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_ops_test::test_op_data_component>());
    const auto* extra_info = ctx.type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_ops_test::test_op_extra_component>());
    ASSERT_NE(data_info, nullptr);
    ASSERT_NE(extra_info, nullptr);

    auto builder = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder.add_entity(tempest::scene::entity_local_id{1}, "RootEntity");
    const auto root_data = tempest_prefab_ops_test::test_op_data_component{.val = 100, .weight = 1.0F};
    const auto root_extra = tempest_prefab_ops_test::test_op_extra_component{.extra_value = 500};
    builder.add_component(root_idx, data_info, tempest_prefab_ops_test::to_bytes(root_data));
    builder.add_component(root_idx, extra_info, tempest_prefab_ops_test::to_bytes(root_extra));

    const auto tmpl = builder.build();
    const auto prefab_guid = tempest::guid::generate_random_guid();
    cache.register_template(prefab_guid, tmpl);

    // Instantiate with removed test_op_extra_component
    auto overrides = tempest::scene::prefab_overrides{};
    overrides.removed_components.push_back(tempest::scene::removed_component{
        .target = {},
        .component_name = "test_op_extra_component",
    });

    const auto inst_res = tempest::scene::prefab_instantiator::instantiate(prefab_guid, cache, registry, overrides);
    ASSERT_TRUE(inst_res.has_value());
    const auto root_ent = inst_res->root_entity;

    // Verify test_op_extra_component was removed by instantiate
    EXPECT_FALSE(registry.has<tempest_prefab_ops_test::test_op_extra_component>(root_ent));

    // Act Part A: Revert removed component
    const auto revert_rm_res = tempest::scene::prefab_operations::revert_removed_component(
        registry, root_ent, {}, "test_op_extra_component", cache);
    ASSERT_TRUE(revert_rm_res.has_value());

    // Assert Part A: Component restored with template value and override removed
    EXPECT_TRUE(registry.has<tempest_prefab_ops_test::test_op_extra_component>(root_ent));
    const auto* restored_extra = registry.try_get<tempest_prefab_ops_test::test_op_extra_component>(root_ent);
    ASSERT_NE(restored_extra, nullptr);
    EXPECT_EQ(restored_extra->extra_value, 500);

    const auto* inst_comp_after_rm = registry.try_get<tempest::scene::prefab_instance_component>(root_ent);
    ASSERT_NE(inst_comp_after_rm, nullptr);
    EXPECT_TRUE(inst_comp_after_rm->overrides.removed_components.empty());

    // Act Part B: Add component override, then revert it
    auto staged_added = tempest::scene::staged_component{
        .type_info = extra_info,
        .data = tempest_prefab_ops_test::to_bytes(tempest_prefab_ops_test::test_op_extra_component{.extra_value = 888}),
    };
    auto updated_inst = *inst_comp_after_rm;
    updated_inst.overrides.added_components.push_back(tempest::scene::added_component{
        .target = {},
        .component = staged_added,
    });
    registry.assign_or_replace(root_ent, tempest::move(updated_inst));
    registry.assign_or_replace(root_ent, tempest_prefab_ops_test::test_op_extra_component{.extra_value = 888});
    EXPECT_TRUE(registry.has<tempest_prefab_ops_test::test_op_extra_component>(root_ent));

    const auto revert_add_res = tempest::scene::prefab_operations::revert_added_component(
        registry, root_ent, {}, "test_op_extra_component");
    ASSERT_TRUE(revert_add_res.has_value());

    // Assert Part B: Component removed from entity and override removed
    EXPECT_FALSE(registry.has<tempest_prefab_ops_test::test_op_extra_component>(root_ent));
    const auto* final_inst = registry.try_get<tempest::scene::prefab_instance_component>(root_ent);
    ASSERT_NE(final_inst, nullptr);
    EXPECT_TRUE(final_inst->overrides.added_components.empty());
}

//==============================================================================
// Prefab Apply Operation Tests
//==============================================================================

/// @brief Verifies that apply_to_prefab incorporates live modifications into the template,
/// updates the cache and file on disk, and clears overrides from the instance component.
TEST(prefab_operations_test, apply_to_prefab_updates_template_and_clears_overrides)
{
    // Setup: Author live hierarchy, save as prefab, then apply modifications
    auto ctx = tempest_prefab_ops_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto cache = tempest::scene::prefab_cache{};

    const auto root_ent = registry.create();
    registry.name(root_ent, "ApplyRoot");
    registry.assign(root_ent, tempest_prefab_ops_test::test_op_data_component{.val = 111, .weight = 1.0F});

    const auto child_ent = registry.create();
    registry.name(child_ent, "ApplyChild");
    registry.assign(child_ent, tempest_prefab_ops_test::test_op_data_component{.val = 222, .weight = 2.0F});
    tempest::ecs::create_parent_child_relationship(registry, root_ent, child_ent);

    const auto prefab_guid = tempest::guid::generate_random_guid();
    const auto temp_file_path = tempest::string{"bin/Debug/test_apply_prefab.tprefab"};

    const auto save_res =
        tempest::scene::prefab_operations::save_as_prefab(registry, root_ent, prefab_guid, temp_file_path, cache);
    ASSERT_TRUE(save_res.has_value());

    // Modify child's val to 999 and add test_op_extra_component
    registry.replace(child_ent, tempest_prefab_ops_test::test_op_data_component{.val = 999, .weight = 2.0F});
    registry.assign(child_ent, tempest_prefab_ops_test::test_op_extra_component{.extra_value = 777});

    // Record override on root instance
    const auto* inst_comp = registry.try_get<tempest::scene::prefab_instance_component>(root_ent);
    ASSERT_NE(inst_comp, nullptr);
    auto child_path = tempest::scene::id_path{};
    child_path.push_back(tempest::scene::entity_local_id{2});
    auto updated_inst = *inst_comp;
    updated_inst.overrides.field_overrides.push_back(tempest::scene::field_override{
        .target = child_path,
        .component_name = "test_op_data_component",
        .field_name = "val",
        .value_bytes = tempest_prefab_ops_test::to_bytes(int32_t{999}),
    });
    registry.assign_or_replace(root_ent, tempest::move(updated_inst));

    // Act: Apply live modifications to prefab
    const auto apply_res =
        tempest::scene::prefab_operations::apply_to_prefab(registry, root_ent, temp_file_path, cache);
    ASSERT_TRUE(apply_res.has_value());

    // Assert: Overrides cleared on live instance
    const auto* inst_comp_after = registry.try_get<tempest::scene::prefab_instance_component>(root_ent);
    ASSERT_NE(inst_comp_after, nullptr);
    EXPECT_TRUE(inst_comp_after->overrides.empty());

    // Cache template updated with new values
    const auto cached_tmpl_opt = cache.find_template(prefab_guid);
    ASSERT_TRUE(cached_tmpl_opt.has_value());
    const auto& updated_tmpl = *cached_tmpl_opt;
    EXPECT_EQ(updated_tmpl.entity_count(), 2U);

    const auto* child_rec = updated_tmpl.get_record(1);
    ASSERT_NE(child_rec, nullptr);
    EXPECT_EQ(child_rec->name, "ApplyChild");

    // Verify child record has val = 999
    auto found_val = false;
    for (const auto& comp : child_rec->components)
    {
        if (comp.type_info != nullptr && tempest::ends_with(comp.type_info->name, "test_op_data_component"))
        {
            auto restored_data = tempest_prefab_ops_test::test_op_data_component{};
            tempest::memcpy(&restored_data, comp.data.data(), sizeof(restored_data));
            EXPECT_EQ(restored_data.val, 999);
            found_val = true;
        }
    }
    EXPECT_TRUE(found_val);

    // Verify file on disk is updated
    const auto file_str_res = tempest::read_file_to_string(tempest::filesystem::path{temp_file_path});
    ASSERT_TRUE(file_str_res.has_value());
    const auto file_deser_res = tempest::scene::prefab_codec::deserialize(*file_str_res, ctx.type_reg);
    ASSERT_TRUE(file_deser_res.has_value());
    EXPECT_TRUE(file_deser_res->success);

    // Clean up temporary file
    tempest::filesystem::remove(tempest::filesystem::path{temp_file_path});
}

//==============================================================================
// Prefab Unpack Operation Tests
//==============================================================================

/// @brief Verifies that unpack strips prefab components from immediate instance members
/// while leaving nested prefab instances and their children untouched.
TEST(prefab_operations_test, unpack_leaves_nested_prefabs_intact)
{
    // Setup: Author hierarchy with outer root, outer child, and nested prefab instance
    auto ctx = tempest_prefab_ops_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto cache = tempest::scene::prefab_cache{};

    // Outer root
    const auto outer_root = registry.create();
    registry.name(outer_root, "OuterRoot");

    // Outer child
    const auto outer_child = registry.create();
    registry.name(outer_child, "OuterChild");
    tempest::ecs::create_parent_child_relationship(registry, outer_root, outer_child);

    // Nested prefab root and child
    const auto nested_root = registry.create();
    registry.name(nested_root, "NestedRoot");
    tempest::ecs::create_parent_child_relationship(registry, outer_child, nested_root);

    const auto nested_child = registry.create();
    registry.name(nested_child, "NestedChild");
    tempest::ecs::create_parent_child_relationship(registry, nested_root, nested_child);

    const auto outer_guid = tempest::guid::generate_random_guid();
    const auto inner_guid = tempest::guid::generate_random_guid();

    // Assign nested prefab components
    registry.assign(nested_root, tempest::scene::prefab_instance_component{
                                     .prefab_guid = inner_guid,
                                     .instance_local_id = tempest::scene::entity_local_id{10},
                                 });
    registry.assign(nested_root, tempest::scene::prefab_member_component{
                                     .template_local_id = tempest::scene::entity_local_id{10},
                                 });
    registry.assign(nested_child, tempest::scene::prefab_member_component{
                                      .template_local_id = tempest::scene::entity_local_id{20},
                                  });

    // Assign outer prefab components
    registry.assign(outer_root, tempest::scene::prefab_instance_component{
                                    .prefab_guid = outer_guid,
                                    .instance_local_id = tempest::scene::entity_local_id{1},
                                });
    registry.assign(outer_root, tempest::scene::prefab_member_component{
                                    .template_local_id = tempest::scene::entity_local_id{1},
                                });
    registry.assign(outer_child, tempest::scene::prefab_member_component{
                                     .template_local_id = tempest::scene::entity_local_id{2},
                                 });

    // Act: Unpack immediate outer instance
    const auto unpack_res = tempest::scene::prefab_operations::unpack(registry, outer_root);
    ASSERT_TRUE(unpack_res.has_value());

    // Assert: Outer instance stripped, nested instance preserved
    EXPECT_FALSE(registry.has<tempest::scene::prefab_instance_component>(outer_root));
    EXPECT_FALSE(registry.has<tempest::scene::prefab_member_component>(outer_root));
    EXPECT_FALSE(registry.has<tempest::scene::prefab_member_component>(outer_child));

    // Nested prefab instance retains its components
    EXPECT_TRUE(registry.has<tempest::scene::prefab_instance_component>(nested_root));
    EXPECT_TRUE(registry.has<tempest::scene::prefab_member_component>(nested_root));
    EXPECT_TRUE(registry.has<tempest::scene::prefab_member_component>(nested_child));

    // Hierarchy intact
    const auto* nested_root_rel =
        registry.try_get<tempest::ecs::relationship_component<tempest::ecs::entity>>(nested_root);
    ASSERT_NE(nested_root_rel, nullptr);
    EXPECT_EQ(nested_root_rel->parent, outer_child);
}

/// @brief Verifies that unpack_completely strips all prefab instance and member components
/// from both outer and nested prefab instances throughout the entire subtree.
TEST(prefab_operations_test, unpack_completely_strips_all_instances)
{
    // Setup: Author hierarchy with outer root and nested prefab instance
    auto ctx = tempest_prefab_ops_test::test_registry_context{};
    auto& registry = ctx.registry;

    const auto outer_root = registry.create();
    registry.name(outer_root, "OuterRoot");

    const auto outer_child = registry.create();
    registry.name(outer_child, "OuterChild");
    tempest::ecs::create_parent_child_relationship(registry, outer_root, outer_child);

    const auto nested_root = registry.create();
    registry.name(nested_root, "NestedRoot");
    tempest::ecs::create_parent_child_relationship(registry, outer_child, nested_root);

    const auto nested_child = registry.create();
    registry.name(nested_child, "NestedChild");
    tempest::ecs::create_parent_child_relationship(registry, nested_root, nested_child);

    // Assign prefab components
    registry.assign(outer_root, tempest::scene::prefab_instance_component{
                                    .prefab_guid = tempest::guid::generate_random_guid(),
                                    .instance_local_id = tempest::scene::entity_local_id{1},
                                });
    registry.assign(outer_root, tempest::scene::prefab_member_component{
                                    .template_local_id = tempest::scene::entity_local_id{1},
                                });
    registry.assign(outer_child, tempest::scene::prefab_member_component{
                                     .template_local_id = tempest::scene::entity_local_id{2},
                                 });

    registry.assign(nested_root, tempest::scene::prefab_instance_component{
                                     .prefab_guid = tempest::guid::generate_random_guid(),
                                     .instance_local_id = tempest::scene::entity_local_id{10},
                                 });
    registry.assign(nested_root, tempest::scene::prefab_member_component{
                                     .template_local_id = tempest::scene::entity_local_id{10},
                                 });
    registry.assign(nested_child, tempest::scene::prefab_member_component{
                                      .template_local_id = tempest::scene::entity_local_id{20},
                                  });

    // Act: Completely unpack entire subtree
    const auto unpack_res = tempest::scene::prefab_operations::unpack_completely(registry, outer_root);
    ASSERT_TRUE(unpack_res.has_value());

    // Assert: All prefab components stripped across the entire hierarchy
    EXPECT_FALSE(registry.has<tempest::scene::prefab_instance_component>(outer_root));
    EXPECT_FALSE(registry.has<tempest::scene::prefab_member_component>(outer_root));

    EXPECT_FALSE(registry.has<tempest::scene::prefab_instance_component>(outer_child));
    EXPECT_FALSE(registry.has<tempest::scene::prefab_member_component>(outer_child));

    EXPECT_FALSE(registry.has<tempest::scene::prefab_instance_component>(nested_root));
    EXPECT_FALSE(registry.has<tempest::scene::prefab_member_component>(nested_root));

    EXPECT_FALSE(registry.has<tempest::scene::prefab_instance_component>(nested_child));
    EXPECT_FALSE(registry.has<tempest::scene::prefab_member_component>(nested_child));

    // Entities and hierarchy remain valid
    EXPECT_TRUE(registry.is_valid(outer_root));
    EXPECT_TRUE(registry.is_valid(outer_child));
    EXPECT_TRUE(registry.is_valid(nested_root));
    EXPECT_TRUE(registry.is_valid(nested_child));
}

//==============================================================================
// Error Handling & Edge Case Tests
//==============================================================================

/// @brief Verifies appropriate error codes for invalid entities, missing components, and missing templates.
TEST(prefab_operations_test, error_handling_and_validation)
{
    // Setup
    auto ctx = tempest_prefab_ops_test::test_registry_context{};
    auto& registry = ctx.registry;
    auto cache = tempest::scene::prefab_cache{};

    const auto invalid_ent = tempest::ecs::entity{tempest::ecs::tombstone};
    const auto valid_ent = registry.create();

    // Act & Assert: Invalid entity passed to operations
    const auto bad_save = tempest::scene::prefab_operations::save_as_prefab(
        registry, invalid_ent, tempest::guid{}, {}, cache);
    ASSERT_FALSE(bad_save.has_value());
    EXPECT_EQ(bad_save.error(), tempest::scene::prefab_operation_error::invalid_entity);

    const auto bad_unpack = tempest::scene::prefab_operations::unpack(registry, invalid_ent);
    ASSERT_FALSE(bad_unpack.has_value());
    EXPECT_EQ(bad_unpack.error(), tempest::scene::prefab_operation_error::invalid_entity);

    // Entity without prefab_instance_component
    const auto not_inst_unpack = tempest::scene::prefab_operations::unpack(registry, valid_ent);
    ASSERT_FALSE(not_inst_unpack.has_value());
    EXPECT_EQ(not_inst_unpack.error(), tempest::scene::prefab_operation_error::not_a_prefab_instance);

    const auto not_inst_revert = tempest::scene::prefab_operations::revert_field_override(
        registry, valid_ent, {}, "comp", "field", cache);
    ASSERT_FALSE(not_inst_revert.has_value());
    EXPECT_EQ(not_inst_revert.error(), tempest::scene::prefab_operation_error::not_a_prefab_instance);
}
