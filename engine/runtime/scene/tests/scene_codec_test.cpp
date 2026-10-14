#include <gtest/gtest.h>

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/json.hpp>
#include <tempest/memory.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/scene_codec.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_types.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

#if defined(_MSC_VER)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

namespace tempest_test
{
    enum class test_status_type : uint32_t
    {
        stopped = 0,
        running = 1,
        paused = 2,
    };

    struct test_link_component
    {
        tempest::ecs::entity target = tempest::ecs::tombstone;
        int32_t tag = 0;
    };

    static_assert(tempest::ecs::component<test_link_component>);

    struct test_actor_component
    {
        bool active = true;
        int32_t health = 100;
        float speed = 5.5F;
        test_status_type status = test_status_type::running;
        int32_t transient_cache = 999;
    };

    static_assert(tempest::ecs::component<test_actor_component>);
} // namespace tempest_test

using namespace tempest_test;

namespace tempest::ecs
{
    template <>
    struct reflect<test_link_component>
    {
        static void describe(type_builder<test_link_component>& builder)
        {
            builder.version(1)
                .field<&test_link_component::target>("target")
                .field<&test_link_component::tag>("tag");
        }
    };

    template <>
    struct reflect<test_actor_component>
    {
        static void describe(type_builder<test_actor_component>& builder)
        {
            static constexpr auto status_entries = tempest::array<enum_entry, 3>{{
                {string_view{"stopped"}, 0},
                {string_view{"running"}, 1},
                {string_view{"paused"}, 2},
            }};

            builder.version(1)
                .field<&test_actor_component::active>("active")
                .field<&test_actor_component::health>("health")
                .field<&test_actor_component::speed>("speed")
                .field<&test_actor_component::status>("status",
                                                      span<const enum_entry>{status_entries.data(), status_entries.size()})
                .field<&test_actor_component::transient_cache>("transient_cache", field_flags::transient);
        }
    };
} // namespace tempest::ecs

namespace
{
    struct test_registry_context
    {
        tempest::event::event_registry event_reg;
        tempest::ecs::component_type_registry type_reg;
        tempest::ecs::basic_archetype_registry registry{event_reg, type_reg};

        test_registry_context()
        {
            const auto link_idx = type_reg.ensure<test_link_component>();
            const auto actor_idx = type_reg.ensure<test_actor_component>();
            const auto tf_idx = type_reg.ensure<tempest::ecs::transform_component>();

            type_reg.add_alias("test_link_component", type_reg.type_info(link_idx).name);
            type_reg.add_alias("test_actor_component", type_reg.type_info(actor_idx).name);
            type_reg.add_alias("transform_component", type_reg.type_info(tf_idx).name);
        }
    };
} // namespace

//==============================================================================
// Local ID Formatting and Parsing Tests
//==============================================================================

/// @brief Verify formatting and parsing 64-bit local entity identifiers to and from hex strings.
TEST(scene_codec_tests, local_id_hex_formatting_and_parsing)
{
    // Setup: Known 64-bit test identifiers
    constexpr auto test_id_one = uint64_t{0x1a2b3c4d5e6f7081ULL};
    constexpr auto test_id_zero = uint64_t{0ULL};
    constexpr auto test_id_max = uint64_t{~uint64_t{0}};

    // Act: Format to hex strings
    const auto str_one = tempest::scene::to_hex_string(test_id_one);
    const auto str_zero = tempest::scene::to_hex_string(test_id_zero);
    const auto str_max = tempest::scene::to_hex_string(test_id_max);

    // Assert: Expected formatting with 0x prefix and lowercase hex digits
    EXPECT_EQ(str_one, "0x1a2b3c4d5e6f7081");
    EXPECT_EQ(str_zero, "0x0000000000000000");
    EXPECT_EQ(str_max, "0xffffffffffffffff");

    // Act: Parse back from hex strings
    const auto parsed_one = tempest::scene::parse_hex_string(str_one);
    const auto parsed_zero = tempest::scene::parse_hex_string(str_zero);
    const auto parsed_max = tempest::scene::parse_hex_string(str_max);
    const auto parsed_raw = tempest::scene::parse_hex_string("1a2b3c4d5e6f7081");
    const auto parsed_upper = tempest::scene::parse_hex_string("0X1A2B3C4D5E6F7081");
    const auto parsed_invalid = tempest::scene::parse_hex_string("not_hex");
    const auto parsed_empty = tempest::scene::parse_hex_string("");
    const auto parsed_prefix_only = tempest::scene::parse_hex_string("0x");
    const auto parsed_overflow = tempest::scene::parse_hex_string("0x12345678901234567");

    // Assert: Parsed values match originals and invalid cases return expected errors
    ASSERT_TRUE(parsed_one.has_value());
    EXPECT_EQ(*parsed_one, test_id_one);
    ASSERT_TRUE(parsed_zero.has_value());
    EXPECT_EQ(*parsed_zero, test_id_zero);
    ASSERT_TRUE(parsed_max.has_value());
    EXPECT_EQ(*parsed_max, test_id_max);
    ASSERT_TRUE(parsed_raw.has_value());
    EXPECT_EQ(*parsed_raw, test_id_one);
    ASSERT_TRUE(parsed_upper.has_value());
    EXPECT_EQ(*parsed_upper, test_id_one);

    EXPECT_FALSE(parsed_invalid.has_value());
    EXPECT_EQ(parsed_invalid.error(), tempest::scene::parse_hex_error::invalid_digit);
    EXPECT_FALSE(parsed_empty.has_value());
    EXPECT_EQ(parsed_empty.error(), tempest::scene::parse_hex_error::empty_input);
    EXPECT_FALSE(parsed_prefix_only.has_value());
    EXPECT_EQ(parsed_prefix_only.error(), tempest::scene::parse_hex_error::invalid_prefix);
    EXPECT_FALSE(parsed_overflow.has_value());
    EXPECT_EQ(parsed_overflow.error(), tempest::scene::parse_hex_error::overflow_too_many_digits);

    // Act & Assert: Strongly typed entity_local_id and parse_local_id
    const auto local_id = tempest::scene::entity_local_id{test_id_one};
    EXPECT_TRUE(local_id.is_valid());
    EXPECT_EQ(tempest::scene::to_hex_string(local_id), str_one);
    EXPECT_EQ(tempest::format("{}", local_id), str_one);

    const auto default_id = tempest::scene::entity_local_id{};
    EXPECT_FALSE(default_id.is_valid());

    const auto parsed_local_id = tempest::scene::parse_local_id(str_one);
    ASSERT_TRUE(parsed_local_id.has_value());
    EXPECT_TRUE(parsed_local_id->is_valid());
    EXPECT_EQ(parsed_local_id->value, test_id_one);
}

//==============================================================================
// Round-Trip Byte Identity Tests
//==============================================================================

/// @brief Verify that save -> load -> save generates identical JSON output byte-for-byte.
TEST(scene_codec_tests, round_trip_byte_identity)
{
    // Setup: Construct entities with transform, actor, and link components
    auto context1 = test_registry_context{};
    auto& reg1 = context1.registry;

    const auto root_entity = reg1.create();
    reg1.name(root_entity, "Player");
    auto transform1 = tempest::ecs::transform_component{};
    transform1.position(tempest::math::vec3<float>{10.0F, 20.0F, 30.0F});
    transform1.scale(tempest::math::vec3<float>{2.0F, 2.0F, 2.0F});
    reg1.assign(root_entity, transform1);

    auto actor1 = test_actor_component{
        .active = true,
        .health = 85,
        .speed = 7.25F,
        .status = test_status_type::paused,
        .transient_cache = 42,
    };
    reg1.assign(root_entity, actor1);

    const auto child_entity = reg1.create();
    reg1.name(child_entity, "Weapon");
    reg1.reparent(child_entity, root_entity);

    auto link1 = test_link_component{
        .target = root_entity,
        .tag = 501,
    };
    reg1.assign(child_entity, link1);

    auto root_entities = tempest::vector<tempest::ecs::entity>{};
    root_entities.push_back(root_entity);

    // Act: First serialization
    const auto json_output1 = tempest::scene::scene_codec::serialize(
        reg1, root_entities, {}, {.format = tempest::json_format::pretty});
    EXPECT_FALSE(json_output1.empty());

    // Act: Deserialize into clean second registry
    auto context2 = test_registry_context{};
    auto& reg2 = context2.registry;
    const auto scene_id = tempest::scene::scene_handle{1, 1};
    const auto deserialize_res = tempest::scene::scene_codec::deserialize(reg2, scene_id, json_output1);

    // Assert: Deserialization succeeded and entities were recreated
    ASSERT_TRUE(deserialize_res.success);
    ASSERT_EQ(deserialize_res.root_entities.size(), 1U);
    ASSERT_EQ(deserialize_res.all_entities.size(), 2U);

    // Act: Second serialization from deserialized state
    const auto json_output2 = tempest::scene::scene_codec::serialize(
        reg2, deserialize_res.root_entities, deserialize_res.unrecognized_data,
        {.format = tempest::json_format::pretty});

    // Assert: Byte identity between first and second serialization
    EXPECT_EQ(json_output1, json_output2);
}

//==============================================================================
// Nested Hierarchy Preservation Tests
//==============================================================================

/// @brief Verify multi-level parent-child-sibling relationships are strictly preserved.
TEST(scene_codec_tests, nested_hierarchy_preservation)
{
    // Setup: Build multi-root, multi-level hierarchy:
    // Root1 ("World")
    //   Child1A ("ZoneA")
    //     Grandchild1A1 ("Prop1")
    //     Grandchild1A2 ("Prop2")
    //   Child1B ("ZoneB")
    //   Child1C ("ZoneC")
    // Root2 ("Sky")
    auto context1 = test_registry_context{};
    auto& reg1 = context1.registry;

    const auto root1 = reg1.create();
    reg1.name(root1, "World");

    const auto zone_a = reg1.create();
    reg1.name(zone_a, "ZoneA");
    reg1.reparent(zone_a, root1);

    const auto prop1 = reg1.create();
    reg1.name(prop1, "Prop1");
    reg1.reparent(prop1, zone_a);

    const auto prop2 = reg1.create();
    reg1.name(prop2, "Prop2");
    reg1.reparent(prop2, zone_a);

    const auto zone_b = reg1.create();
    reg1.name(zone_b, "ZoneB");
    reg1.reparent(zone_b, root1);

    const auto zone_c = reg1.create();
    reg1.name(zone_c, "ZoneC");
    reg1.reparent(zone_c, root1);

    const auto root2 = reg1.create();
    reg1.name(root2, "Sky");

    auto roots = tempest::vector<tempest::ecs::entity>{};
    roots.push_back(root1);
    roots.push_back(root2);

    // Act: Serialize and deserialize
    const auto json_content = tempest::scene::scene_codec::serialize(reg1, roots);
    auto context2 = test_registry_context{};
    auto& reg2 = context2.registry;
    const auto res = tempest::scene::scene_codec::deserialize(reg2, tempest::scene::scene_handle{1, 1}, json_content);

    // Assert: Root structure preserved
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.root_entities.size(), 2U);
    ASSERT_EQ(res.all_entities.size(), 7U);

    const auto new_root1 = res.root_entities[0];
    const auto new_root2 = res.root_entities[1];
    EXPECT_EQ(reg2.name(new_root1).value(), "World");
    EXPECT_EQ(reg2.name(new_root2).value(), "Sky");

    // Assert: Sibling order under World: ZoneA -> ZoneB -> ZoneC
    using rel_t = tempest::ecs::relationship_component<tempest::ecs::entity>;
    const auto* root1_rel = reg2.try_get<rel_t>(new_root1);
    ASSERT_NE(root1_rel, nullptr);

    const auto child_a = root1_rel->first_child;
    ASSERT_TRUE(child_a != tempest::ecs::tombstone);
    EXPECT_EQ(reg2.name(child_a).value(), "ZoneA");

    const auto* child_a_rel = reg2.try_get<rel_t>(child_a);
    ASSERT_NE(child_a_rel, nullptr);
    const auto child_b = child_a_rel->next_sibling;
    ASSERT_TRUE(child_b != tempest::ecs::tombstone);
    EXPECT_EQ(reg2.name(child_b).value(), "ZoneB");

    const auto* child_b_rel = reg2.try_get<rel_t>(child_b);
    ASSERT_NE(child_b_rel, nullptr);
    const auto child_c = child_b_rel->next_sibling;
    ASSERT_TRUE(child_c != tempest::ecs::tombstone);
    EXPECT_EQ(reg2.name(child_c).value(), "ZoneC");

    const auto* child_c_rel = reg2.try_get<rel_t>(child_c);
    ASSERT_NE(child_c_rel, nullptr);
    EXPECT_TRUE(child_c_rel->next_sibling == tempest::ecs::tombstone);

    // Assert: ZoneA children: Prop1 -> Prop2
    const auto prop_1 = child_a_rel->first_child;
    ASSERT_TRUE(prop_1 != tempest::ecs::tombstone);
    EXPECT_EQ(reg2.name(prop_1).value(), "Prop1");

    const auto* prop_1_rel = reg2.try_get<rel_t>(prop_1);
    ASSERT_NE(prop_1_rel, nullptr);
    const auto prop_2 = prop_1_rel->next_sibling;
    ASSERT_TRUE(prop_2 != tempest::ecs::tombstone);
    EXPECT_EQ(reg2.name(prop_2).value(), "Prop2");

    const auto* prop_2_rel = reg2.try_get<rel_t>(prop_2);
    ASSERT_NE(prop_2_rel, nullptr);
    EXPECT_TRUE(prop_2_rel->next_sibling == tempest::ecs::tombstone);

    // Assert: Sky has no children
    const auto* root2_rel = reg2.try_get<rel_t>(new_root2);
    if (root2_rel != nullptr)
    {
        EXPECT_TRUE(root2_rel->first_child == tempest::ecs::tombstone);
    }
}

//==============================================================================
// Entity Reference Remapping Tests
//==============================================================================

/// @brief Verify forward, backward, and cyclic entity references remap to live handles in Pass 2.
TEST(scene_codec_tests, entity_ref_remapping)
{
    // Setup: Create 3 entities with mutual cross-references:
    // Alpha -> Gamma (forward reference)
    // Beta -> Alpha (backward reference)
    // Gamma -> Beta (backward reference)
    auto context1 = test_registry_context{};
    auto& reg1 = context1.registry;

    const auto entity_alpha = reg1.create();
    reg1.name(entity_alpha, "Alpha");

    const auto entity_beta = reg1.create();
    reg1.name(entity_beta, "Beta");

    const auto entity_gamma = reg1.create();
    reg1.name(entity_gamma, "Gamma");

    reg1.assign(entity_alpha, test_link_component{.target = entity_gamma, .tag = 10});
    reg1.assign(entity_beta, test_link_component{.target = entity_alpha, .tag = 20});
    reg1.assign(entity_gamma, test_link_component{.target = entity_beta, .tag = 30});

    auto roots = tempest::vector<tempest::ecs::entity>{};
    roots.push_back(entity_alpha);
    roots.push_back(entity_beta);
    roots.push_back(entity_gamma);

    // Act: Serialize to JSON and deserialize into new registry
    const auto json_str = tempest::scene::scene_codec::serialize(reg1, roots);
    auto context2 = test_registry_context{};
    auto& reg2 = context2.registry;
    const auto res = tempest::scene::scene_codec::deserialize(reg2, tempest::scene::scene_handle{1, 1}, json_str);

    // Assert: Deserialization successful
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.root_entities.size(), 3U);

    const auto new_alpha = reg2.find_first_with_name("Alpha").value();
    const auto new_beta = reg2.find_first_with_name("Beta").value();
    const auto new_gamma = reg2.find_first_with_name("Gamma").value();

    const auto& link_alpha = reg2.get<test_link_component>(new_alpha);
    const auto& link_beta = reg2.get<test_link_component>(new_beta);
    const auto& link_gamma = reg2.get<test_link_component>(new_gamma);

    // Assert: Entity handles correctly resolved across all cyclic links
    EXPECT_EQ(link_alpha.target, new_gamma);
    EXPECT_EQ(link_alpha.tag, 10);

    EXPECT_EQ(link_beta.target, new_alpha);
    EXPECT_EQ(link_beta.tag, 20);

    EXPECT_EQ(link_gamma.target, new_beta);
    EXPECT_EQ(link_gamma.tag, 30);
}

//==============================================================================
// Unrecognized Schema Data Preservation Tests
//==============================================================================

/// @brief Verify unknown components and extra unrecognized fields are preserved intact across round-trips.
TEST(scene_codec_tests, preserves_unrecognized_schema_data)
{
    // Setup: Author JSON containing an unrecognized component and an extra field on test_actor_component
    constexpr auto authored_json = tempest::string_view{R"({
  "version": 1,
  "entities": [
    {
      "id": "0x0000000000000001",
      "name": "AlienBoss",
      "components": {
        "alien_laser_component": {
          "beams": 4,
          "color": [1, 0, 0]
        },
        "test_actor_component": {
          "active": true,
          "future_shield_rating": 999,
          "health": 75,
          "speed": 6.5,
          "status": "running"
        }
      }
    }
  ]
})"};

    // Act: Deserialize into registry
    auto context = test_registry_context{};
    auto& reg = context.registry;
    const auto res = tempest::scene::scene_codec::deserialize(reg, tempest::scene::scene_handle{1, 1}, authored_json);

    // Assert: Entity created and recognized component deserialized
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.root_entities.size(), 1U);
    const auto boss_entity = res.root_entities[0];
    EXPECT_EQ(reg.name(boss_entity).value(), "AlienBoss");

    const auto& actor = reg.get<test_actor_component>(boss_entity);
    EXPECT_TRUE(actor.active);
    EXPECT_EQ(actor.health, 75);
    EXPECT_FLOAT_EQ(actor.speed, 6.5F);
    EXPECT_EQ(actor.status, test_status_type::running);

    // Assert: Side-table captured both unknown component and extra field
    ASSERT_EQ(res.unrecognized_data.entities.size(), 1U);
    const auto unrec_it = res.unrecognized_data.entities.find(1ULL);
    ASSERT_NE(unrec_it, res.unrecognized_data.entities.end());

    const auto& unrec = unrec_it->second;
    ASSERT_EQ(unrec.unknown_components.size(), 1U);
    EXPECT_EQ(unrec.unknown_components[0].name, "alien_laser_component");

    const auto fields_it = unrec.unknown_fields.find("test_actor_component");
    ASSERT_NE(fields_it, unrec.unknown_fields.end());
    ASSERT_EQ(fields_it->second.size(), 1U);
    EXPECT_EQ(fields_it->second[0].field_name, "future_shield_rating");
    EXPECT_EQ(fields_it->second[0].json_payload, "999");

    // Act: Serialize back to JSON with unrecognized_data
    const auto roundtrip_json = tempest::scene::scene_codec::serialize(
        reg, res.root_entities, res.unrecognized_data, {.format = tempest::json_format::pretty});

    // Assert: Output JSON contains both unknown component and extra field
    const auto roundtrip_view = tempest::string_view{roundtrip_json.data(), roundtrip_json.size()};
    EXPECT_NE(tempest::search(roundtrip_view, "\"alien_laser_component\""), roundtrip_view.end());
    EXPECT_NE(tempest::search(roundtrip_view, "\"future_shield_rating\""), roundtrip_view.end());
    EXPECT_NE(tempest::search(roundtrip_view, "999"), roundtrip_view.end());
    EXPECT_NE(tempest::search(roundtrip_view, "\"beams\""), roundtrip_view.end());

    // Act: Second round-trip deserialize and re-serialize to verify byte stability
    auto context2 = test_registry_context{};
    auto& reg2 = context2.registry;
    const auto res2 = tempest::scene::scene_codec::deserialize(reg2, tempest::scene::scene_handle{1, 1}, roundtrip_json);
    ASSERT_TRUE(res2.success);
    const auto final_json = tempest::scene::scene_codec::serialize(
        reg2, res2.root_entities, res2.unrecognized_data, {.format = tempest::json_format::pretty});

    // Assert: Output is completely stable across round-trips
    EXPECT_EQ(roundtrip_json, final_json);
}
