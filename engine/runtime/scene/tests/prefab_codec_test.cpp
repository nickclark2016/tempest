#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_codec.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_instantiator.hpp>
#include <tempest/scene/prefab_overrides.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_codec.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

#if defined(_MSC_VER)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

namespace tempest_prefab_codec_test
{
    struct test_gameplay_component
    {
        int32_t level = 1;
        float power = 10.0F;
    };

    static_assert(tempest::ecs::component<test_gameplay_component>);

    struct test_metadata_component
    {
        int32_t tag_id = 99;
    };

    static_assert(tempest::ecs::component<test_metadata_component>);

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
        tempest::ecs::basic_archetype_registry registry{event_reg, type_reg};

        test_registry_context()
        {
            const auto gp_idx = type_reg.ensure<test_gameplay_component>();
            const auto meta_idx = type_reg.ensure<test_metadata_component>();
            const auto tf_idx = type_reg.ensure<tempest::ecs::transform_component>();
            type_reg.ensure<tempest::scene::prefab_instance_component>();
            type_reg.ensure<tempest::scene::prefab_member_component>();
            type_reg.ensure<tempest::scene::scene_entity_component>();

            type_reg.add_alias("test_gameplay_component", type_reg.type_info(gp_idx).name);
            type_reg.add_alias("test_metadata_component", type_reg.type_info(meta_idx).name);
            type_reg.add_alias("transform_component", type_reg.type_info(tf_idx).name);
        }
    };
} // namespace tempest_prefab_codec_test

namespace tempest::ecs
{
    template <>
    struct reflect<tempest_prefab_codec_test::test_gameplay_component>
    {
        static void describe(type_builder<tempest_prefab_codec_test::test_gameplay_component>& builder)
        {
            builder.version(1)
                .field<&tempest_prefab_codec_test::test_gameplay_component::level>("level")
                .field<&tempest_prefab_codec_test::test_gameplay_component::power>("power");
        }
    };

    template <>
    struct reflect<tempest_prefab_codec_test::test_metadata_component>
    {
        static void describe(type_builder<tempest_prefab_codec_test::test_metadata_component>& builder)
        {
            builder.version(1).field<&tempest_prefab_codec_test::test_metadata_component::tag_id>("tag_id");
        }
    };
} // namespace tempest::ecs

//==============================================================================
// Requirement 7c: Root Transform Omission & Enforcement Tests
//==============================================================================

/// @brief Verifies that serializing an entity hierarchy to .tprefab never persists
/// a transform_component on the root entity, and that deserialization rejects root transforms.
TEST(prefab_codec_test, root_transform_never_persisted_to_file)
{
    // Setup: Author an entity with transform_component and gameplay component
    auto ctx = tempest_prefab_codec_test::test_registry_context{};
    auto& registry = ctx.registry;
    const auto root_ent = registry.create();
    registry.name(root_ent, "RootPrefabEntity");

    auto tx = tempest::ecs::transform_component{};
    tx.position(tempest::math::vec3<float>{10.0F, 20.0F, 30.0F});
    registry.assign(root_ent, tx);

    const auto gameplay = tempest_prefab_codec_test::test_gameplay_component{
        .level = 5,
        .power = 99.5F,
    };
    registry.assign(root_ent, gameplay);

    // Act: Serialize entity hierarchy to .tprefab JSON
    const auto prefab_guid = *tempest::parse_guid("12345678-1234-5678-0102-030405060708");
    const auto json_str = tempest::scene::prefab_codec::serialize(registry, root_ent, prefab_guid);

    // Assert: JSON output must NOT contain transform_component on root
    EXPECT_FALSE(json_str.empty());
    EXPECT_FALSE(tempest::contains(tempest::string_view{json_str.data(), json_str.size()}, "transform_component"));

    auto alloc = tempest::system_allocator{};
    const auto doc_res = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc_res.has_value());
    const auto root_obj = *doc_res->root().as_object();
    EXPECT_EQ(root_obj["format"].as_string().value(), "tempest.prefab");
    EXPECT_EQ(root_obj["format_version"].as_int64().value(), 1);

    const auto entities_arr = *root_obj["entities"].as_array();
    ASSERT_EQ(entities_arr.size(), 1U);
    const auto entity_obj = *entities_arr[0].as_object();
    const auto comps_obj = *entity_obj["components"].as_object();
    EXPECT_FALSE(comps_obj.contains("tempest::ecs::transform_component"));
    EXPECT_FALSE(comps_obj.contains("transform_component"));
    EXPECT_TRUE(comps_obj.contains("tempest_prefab_codec_test::test_gameplay_component") ||
                comps_obj.contains("test_gameplay_component"));

    // Act & Assert: Deserialization of a .tprefab containing root transform_component is rejected
    const auto invalid_json = tempest::string_view{
        R"({
            "format": "tempest.prefab",
            "format_version": 1,
            "guid": "00000000-0000-0000-0000-000000000001",
            "entities": [
                {
                    "id": "0000000000000001",
                    "name": "InvalidRoot",
                    "parent": null,
                    "components": {
                        "tempest::ecs::transform_component": {
                            "position": [1.0, 2.0, 3.0]
                        }
                    }
                }
            ]
        })"};
    const auto deserialize_res = tempest::scene::prefab_codec::deserialize(invalid_json, ctx.type_reg);
    EXPECT_FALSE(deserialize_res.has_value());
    EXPECT_EQ(deserialize_res.error(), tempest::scene::prefab_codec_error::root_transform_forbidden);
}

/// @brief Verifies that non-root child entities in a prefab template persist their transform_component.
TEST(prefab_codec_test, child_transform_is_persisted)
{
    // Setup: Author root entity and child entity, child having transform_component
    auto ctx = tempest_prefab_codec_test::test_registry_context{};
    auto& registry = ctx.registry;

    const auto root_ent = registry.create();
    registry.name(root_ent, "ParentEntity");
    const auto gameplay = tempest_prefab_codec_test::test_gameplay_component{.level = 1, .power = 10.0F};
    registry.assign(root_ent, gameplay);

    const auto child_ent = registry.create();
    registry.name(child_ent, "ChildEntity");
    registry.reparent(child_ent, root_ent);

    auto child_tx = tempest::ecs::transform_component{};
    child_tx.position(tempest::math::vec3<float>{5.0F, 10.0F, 15.0F});
    registry.assign(child_ent, child_tx);

    // Act: Serialize to .tprefab JSON
    const auto prefab_guid = *tempest::parse_guid("abcdef01-2345-6789-0001-020304050607");
    const auto json_str = tempest::scene::prefab_codec::serialize(registry, root_ent, prefab_guid);

    // Assert: Child transform is preserved in JSON and root transform is absent
    EXPECT_FALSE(json_str.empty());
    auto alloc = tempest::system_allocator{};
    const auto doc_res = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc_res.has_value());

    const auto root_obj = *doc_res->root().as_object();
    const auto entities_arr = *root_obj["entities"].as_array();
    ASSERT_EQ(entities_arr.size(), 1U); // 1 root entity in entities_arr
    const auto parent_obj = *entities_arr[0].as_object();
    const auto parent_comps = *parent_obj["components"].as_object();
    EXPECT_FALSE(parent_comps.contains("transform_component"));

    const auto children_arr = *parent_obj["children"].as_array();
    ASSERT_EQ(children_arr.size(), 1U);
    const auto child_obj = *children_arr[0].as_object();
    const auto child_comps = *child_obj["components"].as_object();
    EXPECT_TRUE(child_comps.contains("tempest::ecs::transform_component") ||
                child_comps.contains("transform_component"));

    // Act: Deserialize the prefab back
    const auto decode_res = tempest::scene::prefab_codec::deserialize(json_str, ctx.type_reg);
    ASSERT_TRUE(decode_res.has_value());
    const auto& tmpl = decode_res->template_record;
    ASSERT_EQ(tmpl.entity_count(), 2U);

    // Root record should have gameplay component only
    const auto* root_rec = tmpl.get_record(0);
    ASSERT_NE(root_rec, nullptr);
    EXPECT_FALSE(root_rec->parent_index.has_value());
    EXPECT_EQ(root_rec->components.size(), 1U);

    // Child record should have transform_component
    const auto* child_rec = tmpl.get_record(1);
    ASSERT_NE(child_rec, nullptr);
    ASSERT_TRUE(child_rec->parent_index.has_value());
    EXPECT_EQ(*child_rec->parent_index, 0U);
    ASSERT_EQ(child_rec->components.size(), 1U);
    EXPECT_TRUE(tempest::ends_with(child_rec->components[0].type_info->name, "transform_component"));
}

//==============================================================================
// Prefab Codec Round-Trip Tests
//==============================================================================

/// @brief Verifies that a prefab template round-trips through serialize and deserialize cleanly.
TEST(prefab_codec_test, prefab_codec_round_trip)
{
    // Setup: Build a multi-entity template with components
    auto ctx = tempest_prefab_codec_test::test_registry_context{};
    auto builder = tempest::scene::prefab_template_builder{};

    const auto local_root_id = tempest::scene::entity_local_id{0x1000};
    const auto local_child_id = tempest::scene::entity_local_id{0x2000};

    const auto root_idx = builder.add_entity(local_root_id, "TemplateRoot");
    const auto child_idx = builder.add_entity(local_child_id, "TemplateChild", root_idx);

    const auto gp_info = ctx.type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_codec_test::test_gameplay_component>());
    const auto meta_info = ctx.type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_codec_test::test_metadata_component>());

    const auto gp_data = tempest_prefab_codec_test::test_gameplay_component{.level = 7, .power = 49.0F};
    const auto meta_data = tempest_prefab_codec_test::test_metadata_component{.tag_id = 777};

    builder.add_component(root_idx, gp_info, tempest_prefab_codec_test::to_bytes(gp_data));
    builder.add_component(child_idx, meta_info, tempest_prefab_codec_test::to_bytes(meta_data));

    const auto original_tmpl = builder.build();
    ASSERT_EQ(original_tmpl.entity_count(), 2U);

    const auto source_guid = *tempest::parse_guid("55555555-6666-7777-0809-0a0b0c0d0e0f");

    // Act: Serialize template to .tprefab JSON
    const auto json_str = tempest::scene::prefab_codec::serialize(original_tmpl, source_guid);
    EXPECT_FALSE(json_str.empty());

    // Act: Deserialize back into a template
    const auto decode_res = tempest::scene::prefab_codec::deserialize(json_str, ctx.type_reg);
    ASSERT_TRUE(decode_res.has_value());
    const auto& decoded_tmpl = decode_res->template_record;

    // Assert: Entity count, local IDs, names, hierarchy, and component payloads match
    EXPECT_EQ(decode_res->prefab_guid, source_guid);
    ASSERT_EQ(decoded_tmpl.entity_count(), 2U);

    const auto* root_rec = decoded_tmpl.find_record(local_root_id);
    ASSERT_NE(root_rec, nullptr);
    EXPECT_EQ(root_rec->name, "TemplateRoot");
    EXPECT_FALSE(root_rec->parent_index.has_value());
    ASSERT_EQ(root_rec->components.size(), 1U);

    auto decoded_gp = tempest_prefab_codec_test::test_gameplay_component{};
    tempest::memcpy(&decoded_gp, root_rec->components[0].data.data(), sizeof(decoded_gp));
    EXPECT_EQ(decoded_gp.level, 7);
    EXPECT_FLOAT_EQ(decoded_gp.power, 49.0F);

    const auto* child_rec = decoded_tmpl.find_record(local_child_id);
    ASSERT_NE(child_rec, nullptr);
    EXPECT_EQ(child_rec->name, "TemplateChild");
    ASSERT_TRUE(child_rec->parent_index.has_value());
    EXPECT_EQ(*child_rec->parent_index, 0U);
    ASSERT_EQ(child_rec->components.size(), 1U);

    auto decoded_meta = tempest_prefab_codec_test::test_metadata_component{};
    tempest::memcpy(&decoded_meta, child_rec->components[0].data.data(), sizeof(decoded_meta));
    EXPECT_EQ(decoded_meta.tag_id, 777);
}

//==============================================================================
// Scene Codec Prefab Instance Block Tests
//==============================================================================

/// @brief Verifies that scene_codec::serialize emits a complete "prefab" block
/// when an entity possesses prefab_instance_component.
TEST(prefab_codec_test, scene_codec_serializes_prefab_instance_block)
{
    // Setup: Create registry with an entity possessing prefab_instance_component
    auto ctx = tempest_prefab_codec_test::test_registry_context{};
    auto& reg = ctx.registry;

    const auto instance_ent = reg.create();
    reg.name(instance_ent, "ChestInstance");
    const auto inst_local_id = tempest::scene::entity_local_id{0x9000};
    reg.assign(instance_ent, tempest::scene::scene_entity_component{
                                 .scene = tempest::scene::scene_handle{1, 1},
                                 .local_id = inst_local_id,
                             });

    const auto prefab_guid = *tempest::parse_guid("11112222-3333-4444-0506-0708090a0b0c");

    auto overrides = tempest::scene::prefab_overrides{};
    auto field_ov = tempest::scene::field_override{
        .target = {},
        .component_name = "test_gameplay_component",
        .field_name = "power",
        .value_bytes = tempest_prefab_codec_test::to_bytes(250.0F),
    };
    overrides.field_overrides.push_back(tempest::move(field_ov));
    overrides.removed_components.push_back(tempest::scene::removed_component{
        .target = {},
        .component_name = "test_metadata_component",
    });
    overrides.added_children.push_back(tempest::scene::entity_local_id{0x9999});

    reg.assign(instance_ent, tempest::scene::prefab_instance_component{
                                 .prefab_guid = prefab_guid,
                                 .instance_local_id = inst_local_id,
                                 .overrides = tempest::move(overrides),
                             });

    auto root_entities = tempest::vector<tempest::ecs::entity>{};
    root_entities.push_back(instance_ent);

    // Act: Serialize scene
    const auto scene_json = tempest::scene::scene_codec::serialize(reg, root_entities);
    EXPECT_FALSE(scene_json.empty());

    // Assert: Verify the "prefab" block in JSON output
    auto alloc = tempest::system_allocator{};
    const auto doc_res = tempest::json_document::from_string(scene_json, alloc);
    ASSERT_TRUE(doc_res.has_value());

    const auto root_obj = *doc_res->root().as_object();
    const auto entities_arr = *root_obj["entities"].as_array();
    ASSERT_EQ(entities_arr.size(), 1U);

    const auto ent_obj = *entities_arr[0].as_object();
    ASSERT_TRUE(ent_obj.contains("prefab"));
    const auto prefab_obj = *ent_obj["prefab"].as_object();

    EXPECT_EQ(prefab_obj["guid"].as_string().value(), tempest::to_string(prefab_guid));
    EXPECT_EQ(prefab_obj["instance_id"].as_string().value(), tempest::scene::to_hex_string(inst_local_id));

    const auto overrides_arr = *prefab_obj["overrides"].as_array();
    ASSERT_EQ(overrides_arr.size(), 1U);
    const auto ov_item = *overrides_arr[0].as_object();
    EXPECT_EQ(ov_item["component"].as_string().value(), "test_gameplay_component");
    EXPECT_EQ(ov_item["field"].as_string().value(), "power");
    EXPECT_FLOAT_EQ(ov_item["value"].as_float().value(), 250.0F);

    const auto removed_arr = *prefab_obj["removed_components"].as_array();
    ASSERT_EQ(removed_arr.size(), 1U);
    const auto rem_item = *removed_arr[0].as_object();
    EXPECT_EQ(rem_item["component"].as_string().value(), "test_metadata_component");

    const auto added_children_arr = *prefab_obj["added_children"].as_array();
    ASSERT_EQ(added_children_arr.size(), 1U);
    EXPECT_EQ(added_children_arr[0].as_string().value(),
              tempest::scene::to_hex_string(tempest::scene::entity_local_id{0x9999}));
}

/// @brief Verifies that scene_codec::deserialize correctly uses prefab_cache to instantiate
/// prefab templates and applies field overrides upon loading.
TEST(prefab_codec_test, scene_codec_deserializes_prefab_instance_with_cache)
{
    // Setup: Register template in prefab_cache
    auto ctx = tempest_prefab_codec_test::test_registry_context{};
    auto cache = tempest::scene::prefab_cache{};

    const auto prefab_guid = *tempest::parse_guid("cafecafe-1234-5678-0102-030405060708");
    const auto local_root_id = tempest::scene::entity_local_id{0x100};

    auto builder = tempest::scene::prefab_template_builder{};
    const auto root_idx = builder.add_entity(local_root_id, "BasePrefab");
    const auto gp_info = ctx.type_reg.type_info(
        tempest::core::normalized_type_name<tempest_prefab_codec_test::test_gameplay_component>());
    const auto base_gp = tempest_prefab_codec_test::test_gameplay_component{.level = 1, .power = 10.0F};
    builder.add_component(root_idx, gp_info, tempest_prefab_codec_test::to_bytes(base_gp));

    cache.register_template(prefab_guid, builder.build());

    // Act: Craft scene JSON referencing this prefab with an override power = 88.0F
    const auto scene_json = tempest::format(
        R"({{
            "version": 1,
            "entities": [
                {{
                    "id": "0000000000000500",
                    "name": "InstantiatedPrefabInScene",
                    "components": {{}},
                    "prefab": {{
                        "guid": "{}",
                        "instance_id": "0000000000000500",
                        "overrides": [
                            {{
                                "target": [],
                                "component": "test_gameplay_component",
                                "field": "power",
                                "value": 88.0
                            }}
                        ],
                        "added_components": [],
                        "removed_components": [],
                        "added_children": []
                    }}
                }}
            ]
        }})",
        tempest::to_string(prefab_guid));

    const auto scene_id = tempest::scene::scene_handle{2, 1};
    const auto des_res = tempest::scene::scene_codec::deserialize(ctx.registry, scene_id, scene_json, &cache);

    // Assert: Deserialization succeeded and override was applied to instantiated entity
    ASSERT_TRUE(des_res.success);
    ASSERT_EQ(des_res.root_entities.size(), 1U);
    const auto instantiated_ent = des_res.root_entities[0];

    EXPECT_TRUE(ctx.registry.has<tempest::scene::prefab_instance_component>(instantiated_ent));
    EXPECT_TRUE(ctx.registry.has<tempest::scene::scene_entity_component>(instantiated_ent));

    const auto* gp_comp = ctx.registry.try_get<tempest_prefab_codec_test::test_gameplay_component>(instantiated_ent);
    ASSERT_NE(gp_comp, nullptr);
    EXPECT_EQ(gp_comp->level, 1);
    EXPECT_FLOAT_EQ(gp_comp->power, 88.0F); // Overridden value!
}

//==============================================================================
// ID Path Depth Exceeded Tests
//==============================================================================

/// @brief Verifies that parse_id_path strictly rejects paths exceeding max_id_path_depth (8).
TEST(prefab_codec_test, id_path_depth_exceeded_rejected)
{
    // Act & Assert: Valid 8-depth path succeeds
    const auto valid_8_path = tempest::string_view{"1/2/3/4/5/6/7/8"};
    const auto valid_res = tempest::scene::parse_id_path(valid_8_path);
    ASSERT_TRUE(valid_res.has_value());
    EXPECT_EQ(valid_res->size(), 8U);

    // Act & Assert: 9-depth slash path is rejected
    const auto invalid_9_path = tempest::string_view{"1/2/3/4/5/6/7/8/9"};
    const auto invalid_res = tempest::scene::parse_id_path(invalid_9_path);
    EXPECT_FALSE(invalid_res.has_value());
    EXPECT_EQ(invalid_res.error(), tempest::scene::prefab_codec_error::depth_exceeded);

    // Act & Assert: 9-element JSON array is rejected
    const auto invalid_json_arr =
        tempest::string_view{R"(["1","2","3","4","5","6","7","8","9"])"};
    const auto json_arr_res = tempest::scene::parse_id_path(invalid_json_arr);
    EXPECT_FALSE(json_arr_res.has_value());
    EXPECT_EQ(json_arr_res.error(), tempest::scene::prefab_codec_error::depth_exceeded);
}

//==============================================================================
// Prefab Codec Validation Tests
//==============================================================================

/// @brief Verifies that prefab_codec strictly rejects templates with duplicate entity IDs,
/// non-existent parent references, or multiple roots.
TEST(prefab_codec_test, validation_rules_enforce_single_root_and_valid_references)
{
    // Setup: Registry context
    auto ctx = tempest_prefab_codec_test::test_registry_context{};

    // Act & Assert: Duplicate entity ID is rejected
    const auto duplicate_id_json = tempest::string_view{R"({
        "format": "tempest.prefab",
        "format_version": 1,
        "guid": "11111111-2222-3333-4444-555555555555",
        "entities": [
            { "id": "0000000000000001", "name": "Root", "parent": null, "components": {} },
            { "id": "0000000000000001", "name": "Child", "parent": "0000000000000001", "components": {} }
        ]
    })"};
    const auto dup_res = tempest::scene::prefab_codec::deserialize(duplicate_id_json, ctx.type_reg);
    EXPECT_FALSE(dup_res.has_value());
    EXPECT_EQ(dup_res.error(), tempest::scene::prefab_codec_error::invalid_entity_record);

    // Act & Assert: Non-existent parent reference is rejected
    const auto invalid_parent_json = tempest::string_view{R"({
        "format": "tempest.prefab",
        "format_version": 1,
        "guid": "11111111-2222-3333-4444-555555555555",
        "entities": [
            { "id": "0000000000000001", "name": "Root", "parent": null, "components": {} },
            { "id": "0000000000000002", "name": "Child", "parent": "0000000000009999", "components": {} }
        ]
    })"};
    const auto parent_res = tempest::scene::prefab_codec::deserialize(invalid_parent_json, ctx.type_reg);
    EXPECT_FALSE(parent_res.has_value());
    EXPECT_EQ(parent_res.error(), tempest::scene::prefab_codec_error::invalid_entity_record);

    // Act & Assert: Multiple roots are rejected
    const auto multi_root_json = tempest::string_view{R"({
        "format": "tempest.prefab",
        "format_version": 1,
        "guid": "11111111-2222-3333-4444-555555555555",
        "entities": [
            { "id": "0000000000000001", "name": "Root1", "parent": null, "components": {} },
            { "id": "0000000000000002", "name": "Root2", "parent": null, "components": {} }
        ]
    })"};
    const auto multi_res = tempest::scene::prefab_codec::deserialize(multi_root_json, ctx.type_reg);
    EXPECT_FALSE(multi_res.has_value());
    EXPECT_EQ(multi_res.error(), tempest::scene::prefab_codec_error::invalid_entity_record);
}

//==============================================================================
// Prefab Instantiator Override Ordering Tests
//==============================================================================

/// @brief Verifies that prefab_instantiator applies added_components before field_overrides,
/// allowing field overrides to target components dynamically added to an instance.
TEST(prefab_codec_test, added_components_applied_before_field_overrides)
{
    // Setup: Registry, cache, and simple prefab template
    auto ctx = tempest_prefab_codec_test::test_registry_context{};
    auto cache = tempest::scene::prefab_cache{};

    const auto tmpl_json = tempest::string_view{R"({
        "format": "tempest.prefab",
        "format_version": 1,
        "guid": "11111111-2222-3333-4444-555555555555",
        "entities": [
            {
                "id": "0000000000000001",
                "name": "Root",
                "parent": null,
                "components": {
                    "test_gameplay_component": {
                        "level": 1,
                        "power": 10.0
                    }
                }
            }
        ]
    })"};
    auto tmpl_res = tempest::scene::prefab_codec::deserialize(tmpl_json, ctx.type_reg);
    ASSERT_TRUE(tmpl_res.has_value());
    const auto prefab_guid = tmpl_res->prefab_guid;
    cache.register_template(prefab_guid, tempest::move(tmpl_res->template_record));

    // Act: Configure instance overrides where test_metadata_component is added,
    // and field_overrides targets tag_id = 777 on the newly added component.
    auto overrides = tempest::scene::prefab_overrides{};

    auto added = tempest::scene::added_component{};
    added.target = tempest::scene::id_path{};
    added.component.type_info = ctx.type_reg.type_info("test_metadata_component");
    auto meta = tempest_prefab_codec_test::test_metadata_component{.tag_id = 42};
    added.component.data = tempest_prefab_codec_test::to_bytes(meta);
    overrides.added_components.push_back(tempest::move(added));

    const auto new_tag_val = int32_t{777};
    auto field_ov = tempest::scene::field_override{};
    field_ov.target = tempest::scene::id_path{};
    field_ov.component_name = "test_metadata_component";
    field_ov.field_name = "tag_id";
    field_ov.value_bytes = tempest_prefab_codec_test::to_bytes(new_tag_val);
    overrides.field_overrides.push_back(tempest::move(field_ov));

    const auto inst_res = tempest::scene::prefab_instantiator::instantiate(
        prefab_guid, cache, ctx.registry, overrides);

    // Assert: Entity was instantiated with the added component having the overridden field value
    ASSERT_TRUE(inst_res.has_value());
    const auto ent = inst_res->root_entity;
    EXPECT_TRUE(ctx.registry.has<tempest_prefab_codec_test::test_metadata_component>(ent));

    const auto* meta_comp = ctx.registry.try_get<tempest_prefab_codec_test::test_metadata_component>(ent);
    ASSERT_NE(meta_comp, nullptr);
    EXPECT_EQ(meta_comp->tag_id, 777);
}

//==============================================================================
// Prefab Instance Unique Scene Local ID Tests
//==============================================================================

/// @brief Verifies that instantiating multiple instances of the same multi-entity prefab
/// in a scene produces unique, collision-free local IDs across all child entities upon reload.
TEST(prefab_codec_test, multiple_instances_receive_unique_scene_local_ids)
{
    // Setup: Registry, cache, and 2-entity prefab template (Root -> Child)
    auto ctx = tempest_prefab_codec_test::test_registry_context{};
    auto cache = tempest::scene::prefab_cache{};

    const auto tmpl_json = tempest::string_view{R"({
        "format": "tempest.prefab",
        "format_version": 1,
        "guid": "22222222-3333-4444-5555-666666666666",
        "entities": [
            {
                "id": "0000000000000001",
                "name": "PrefabRoot",
                "parent": null,
                "components": {
                    "test_gameplay_component": {
                        "level": 1,
                        "power": 10.0
                    }
                }
            },
            {
                "id": "0000000000000002",
                "name": "PrefabChild",
                "parent": "0000000000000001",
                "components": {
                    "test_metadata_component": {
                        "tag_id": 99
                    }
                }
            }
        ]
    })"};
    auto tmpl_res = tempest::scene::prefab_codec::deserialize(tmpl_json, ctx.type_reg);
    ASSERT_TRUE(tmpl_res.has_value());
    const auto prefab_guid = tmpl_res->prefab_guid;
    cache.register_template(prefab_guid, tempest::move(tmpl_res->template_record));

    // Act: Instantiate two instances into scene registry
    const auto inst1_res = tempest::scene::prefab_instantiator::instantiate(
        prefab_guid, cache, ctx.registry, tempest::scene::prefab_overrides{});
    ASSERT_TRUE(inst1_res.has_value());

    const auto inst2_res = tempest::scene::prefab_instantiator::instantiate(
        prefab_guid, cache, ctx.registry, tempest::scene::prefab_overrides{});
    ASSERT_TRUE(inst2_res.has_value());

    // Assign scene entity components to roots with distinct scene local IDs
    ctx.registry.assign_or_replace<tempest::scene::scene_entity_component>(
        inst1_res->root_entity,
        tempest::scene::scene_entity_component{.local_id = tempest::scene::entity_local_id{0x1000}});
    ctx.registry.assign_or_replace<tempest::scene::scene_entity_component>(
        inst2_res->root_entity,
        tempest::scene::scene_entity_component{.local_id = tempest::scene::entity_local_id{0x2000}});

    // Serialize scene
    auto root_entities = tempest::vector<tempest::ecs::entity>{};
    root_entities.push_back(inst1_res->root_entity);
    root_entities.push_back(inst2_res->root_entity);
    const auto ser_json = tempest::scene::scene_codec::serialize(ctx.registry, root_entities);
    ASSERT_FALSE(ser_json.empty());

    // Deserialize into fresh registry
    auto ctx2 = tempest_prefab_codec_test::test_registry_context{};
    const auto scene_id = tempest::scene::scene_handle{1, 1};
    const auto des_res = tempest::scene::scene_codec::deserialize(
        ctx2.registry, scene_id, ser_json, &cache);
    ASSERT_TRUE(des_res.success);

    // Assert: All deserialized entities have unique local_ids (4 entities total)
    auto seen_ids = tempest::vector<tempest::scene::entity_local_id>{};
    for (const auto ent : des_res.all_entities)
    {
        const auto* sec = ctx2.registry.try_get<tempest::scene::scene_entity_component>(ent);
        ASSERT_NE(sec, nullptr);
        for (const auto prev_id : seen_ids)
        {
            EXPECT_NE(sec->local_id, prev_id);
        }
        seen_ids.push_back(sec->local_id);
    }
    EXPECT_EQ(seen_ids.size(), 4U);
}
