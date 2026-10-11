#include <tempest/component_type_registry.hpp>
#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/guid.hpp>
#include <tempest/quat.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

namespace
{
    enum class test_status_code : uint32_t
    {
        inactive = 0,
        pending = 10,
        active = 20,
    };

    struct all_primitives_component
    {
        bool flag = false;
        int32_t int32_value = 0;
        uint32_t uint32_value = 0;
        int64_t int64_value = 0;
        uint64_t uint64_value = 0;
        float float_value = 0.0F;
        double double_value = 0.0;
        tempest::math::vec2<float> vec2_value = {0.0F, 0.0F};
        tempest::math::vec3<float> vec3_value = {0.0F, 0.0F, 0.0F};
        tempest::math::vec4<float> vec4_value = {0.0F, 0.0F, 0.0F, 0.0F};
        tempest::math::quat<float> quat_value = {0.0F, 0.0F, 0.0F, 1.0F};
        tempest::ecs::entity entity_value = static_cast<tempest::ecs::entity>(0);
        tempest::guid guid_value = {};
        test_status_code status = test_status_code::inactive;
    };

    struct property_test_component
    {
        int32_t secret_value = 42;
        float multiplier = 2.0F;

        [[nodiscard]] auto get_value() const noexcept -> int32_t
        {
            return secret_value;
        }

        void set_value(int32_t new_value) noexcept
        {
            secret_value = new_value;
        }
    };
} // namespace

namespace tempest::ecs
{
    template <>
    struct reflect<all_primitives_component>
    {
        static void describe(type_builder<all_primitives_component>& builder)
        {
            static constexpr auto status_entries = tempest::array<enum_entry, 3>{{
                {string_view{"inactive"}, 0},
                {string_view{"pending"}, 10},
                {string_view{"active"}, 20},
            }};

            builder.version(2)
                .field<&all_primitives_component::flag>("flag")
                .field<&all_primitives_component::int32_value>("int32_value")
                .field<&all_primitives_component::uint32_value>("uint32_value")
                .field<&all_primitives_component::int64_value>("int64_value")
                .field<&all_primitives_component::uint64_value>("uint64_value")
                .field<&all_primitives_component::float_value>("float_value")
                .field<&all_primitives_component::double_value>("double_value")
                .field<&all_primitives_component::vec2_value>("vec2_value")
                .field<&all_primitives_component::vec3_value>("vec3_value")
                .field<&all_primitives_component::vec4_value>("vec4_value")
                .field<&all_primitives_component::quat_value>("quat_value")
                .field<&all_primitives_component::entity_value>("entity_value")
                .field<&all_primitives_component::guid_value>("guid_value")
                .field<&all_primitives_component::status>("status", span<const enum_entry>{status_entries.data(), status_entries.size()});
        }
    };

    template <>
    struct reflect<property_test_component>
    {
        static void describe(type_builder<property_test_component>& builder)
        {
            builder.property<&property_test_component::get_value, &property_test_component::set_value>("secret")
                .property(
                    "multiplier",
                    [](const property_test_component& comp) -> float {
                        return comp.multiplier;
                    },
                    [](property_test_component& comp, float value) {
                        comp.multiplier = value;
                    });
        }
    };
} // namespace tempest::ecs

// ============================================================================
// Field Metadata & Layout Reflection Tests
// ============================================================================

/// @brief Verifies that type descriptors accurately deduce field kinds, offsets,
/// sizes, and alignments across primitive and math types.
TEST(reflection_tests, type_descriptor_accurately_reflects_primitives_layout)
{
    // 1. Setup: build a type descriptor for all_primitives_component.
    auto builder = tempest::ecs::type_builder<all_primitives_component>{"all_primitives"};
    tempest::ecs::reflect<all_primitives_component>::describe(builder);
    const auto descriptor = builder.build();

    // 2. Assert: verify descriptor header properties.
    ASSERT_EQ(descriptor.name, "all_primitives");
    ASSERT_EQ(descriptor.size, sizeof(all_primitives_component));
    ASSERT_EQ(descriptor.alignment, alignof(all_primitives_component));
    ASSERT_EQ(descriptor.version, 2U);
    ASSERT_EQ(descriptor.fields.size(), 14U);

    // 3. Assert: verify non-existent field lookup returns nullptr.
    const auto* missing_field = descriptor.find_field("non_existent");
    ASSERT_EQ(missing_field, nullptr);

    // 4. Assert: verify each field descriptor metadata.
    const auto* flag_field = descriptor.find_field("flag");
    ASSERT_NE(flag_field, nullptr);
    ASSERT_EQ(flag_field->kind, tempest::ecs::field_kind::boolean);
    ASSERT_EQ(flag_field->size, sizeof(bool));
    ASSERT_NE(flag_field->get, nullptr);
    ASSERT_NE(flag_field->set, nullptr);

    const auto* int32_field = descriptor.find_field("int32_value");
    ASSERT_NE(int32_field, nullptr);
    ASSERT_EQ(int32_field->kind, tempest::ecs::field_kind::int32);
    ASSERT_EQ(int32_field->size, sizeof(int32_t));

    const auto* uint32_field = descriptor.find_field("uint32_value");
    ASSERT_NE(uint32_field, nullptr);
    ASSERT_EQ(uint32_field->kind, tempest::ecs::field_kind::uint32);
    ASSERT_EQ(uint32_field->size, sizeof(uint32_t));

    const auto* int64_field = descriptor.find_field("int64_value");
    ASSERT_NE(int64_field, nullptr);
    ASSERT_EQ(int64_field->kind, tempest::ecs::field_kind::int64);
    ASSERT_EQ(int64_field->size, sizeof(int64_t));

    const auto* uint64_field = descriptor.find_field("uint64_value");
    ASSERT_NE(uint64_field, nullptr);
    ASSERT_EQ(uint64_field->kind, tempest::ecs::field_kind::uint64);
    ASSERT_EQ(uint64_field->size, sizeof(uint64_t));

    const auto* float_field = descriptor.find_field("float_value");
    ASSERT_NE(float_field, nullptr);
    ASSERT_EQ(float_field->kind, tempest::ecs::field_kind::float32);
    ASSERT_EQ(float_field->size, sizeof(float));

    const auto* double_field = descriptor.find_field("double_value");
    ASSERT_NE(double_field, nullptr);
    ASSERT_EQ(double_field->kind, tempest::ecs::field_kind::float64);
    ASSERT_EQ(double_field->size, sizeof(double));

    const auto* vec2_field = descriptor.find_field("vec2_value");
    ASSERT_NE(vec2_field, nullptr);
    ASSERT_EQ(vec2_field->kind, tempest::ecs::field_kind::vec2);
    ASSERT_EQ(vec2_field->size, sizeof(tempest::math::vec2<float>));

    const auto* vec3_field = descriptor.find_field("vec3_value");
    ASSERT_NE(vec3_field, nullptr);
    ASSERT_EQ(vec3_field->kind, tempest::ecs::field_kind::vec3);
    ASSERT_EQ(vec3_field->size, sizeof(tempest::math::vec3<float>));

    const auto* vec4_field = descriptor.find_field("vec4_value");
    ASSERT_NE(vec4_field, nullptr);
    ASSERT_EQ(vec4_field->kind, tempest::ecs::field_kind::vec4);
    ASSERT_EQ(vec4_field->size, sizeof(tempest::math::vec4<float>));

    const auto* quat_field = descriptor.find_field("quat_value");
    ASSERT_NE(quat_field, nullptr);
    ASSERT_EQ(quat_field->kind, tempest::ecs::field_kind::quat);
    ASSERT_EQ(quat_field->size, sizeof(tempest::math::quat<float>));

    const auto* entity_field = descriptor.find_field("entity_value");
    ASSERT_NE(entity_field, nullptr);
    ASSERT_EQ(entity_field->kind, tempest::ecs::field_kind::entity_ref);
    ASSERT_EQ(entity_field->size, sizeof(tempest::ecs::entity));

    const auto* guid_field = descriptor.find_field("guid_value");
    ASSERT_NE(guid_field, nullptr);
    ASSERT_EQ(guid_field->kind, tempest::ecs::field_kind::guid);
    ASSERT_EQ(guid_field->size, sizeof(tempest::guid));

    const auto* status_field = descriptor.find_field("status");
    ASSERT_NE(status_field, nullptr);
    ASSERT_EQ(status_field->kind, tempest::ecs::field_kind::enumeration);
    ASSERT_EQ(status_field->size, sizeof(test_status_code));
    ASSERT_EQ(status_field->enum_entries.size(), 3U);
    ASSERT_EQ(status_field->enum_entries[0].name, "inactive");
    ASSERT_EQ(status_field->enum_entries[0].value, 0);
    ASSERT_EQ(status_field->enum_entries[1].name, "pending");
    ASSERT_EQ(status_field->enum_entries[1].value, 10);
    ASSERT_EQ(status_field->enum_entries[2].name, "active");
    ASSERT_EQ(status_field->enum_entries[2].value, 20);
}

// ============================================================================
// Field Getter / Setter Invocation Tests
// ============================================================================

/// @brief Verifies reading and writing field values via generic function pointers
/// on primitive and composite fields.
TEST(reflection_tests, field_getter_and_setter_mutate_instance_correctly)
{
    // 1. Setup: instance of all_primitives_component and its descriptor.
    auto instance = all_primitives_component{};
    auto builder = tempest::ecs::type_builder<all_primitives_component>{"all_primitives"};
    tempest::ecs::reflect<all_primitives_component>::describe(builder);
    const auto descriptor = builder.build();

    // 2. Act & Assert: test boolean field access.
    const auto* flag_field = descriptor.find_field("flag");
    ASSERT_NE(flag_field, nullptr);
    auto read_flag = false;
    flag_field->get(&instance, &read_flag);
    ASSERT_FALSE(read_flag);

    const auto write_flag = true;
    flag_field->set(&instance, &write_flag);
    ASSERT_TRUE(instance.flag);

    // 3. Act & Assert: test integer fields.
    const auto* int32_field = descriptor.find_field("int32_value");
    ASSERT_NE(int32_field, nullptr);
    const auto write_int32 = int32_t{-12345};
    int32_field->set(&instance, &write_int32);
    ASSERT_EQ(instance.int32_value, -12345);

    auto read_int32 = int32_t{0};
    int32_field->get(&instance, &read_int32);
    ASSERT_EQ(read_int32, -12345);

    // 4. Act & Assert: test float and double fields.
    const auto* float_field = descriptor.find_field("float_value");
    ASSERT_NE(float_field, nullptr);
    const auto write_float = 3.14159F;
    float_field->set(&instance, &write_float);
    ASSERT_FLOAT_EQ(instance.float_value, 3.14159F);

    const auto* double_field = descriptor.find_field("double_value");
    ASSERT_NE(double_field, nullptr);
    const auto write_double = 2.718281828459;
    double_field->set(&instance, &write_double);
    ASSERT_DOUBLE_EQ(instance.double_value, 2.718281828459);

    // 5. Act & Assert: test math vectors and quaternions.
    const auto* vec3_field = descriptor.find_field("vec3_value");
    ASSERT_NE(vec3_field, nullptr);
    const auto write_vec3 = tempest::math::vec3<float>{1.5F, 2.5F, 3.5F};
    vec3_field->set(&instance, &write_vec3);
    ASSERT_EQ(instance.vec3_value.x, 1.5F);
    ASSERT_EQ(instance.vec3_value.y, 2.5F);
    ASSERT_EQ(instance.vec3_value.z, 3.5F);

    const auto* quat_field = descriptor.find_field("quat_value");
    ASSERT_NE(quat_field, nullptr);
    const auto write_quat = tempest::math::quat<float>{0.1F, 0.2F, 0.3F, 0.9F};
    quat_field->set(&instance, &write_quat);
    ASSERT_EQ(instance.quat_value.w, 0.9F);

    // 6. Act & Assert: test entity and guid.
    const auto* entity_field = descriptor.find_field("entity_value");
    ASSERT_NE(entity_field, nullptr);
    const auto write_entity = static_cast<tempest::ecs::entity>(9999);
    entity_field->set(&instance, &write_entity);
    ASSERT_EQ(instance.entity_value, static_cast<tempest::ecs::entity>(9999));

    const auto* guid_field = descriptor.find_field("guid_value");
    ASSERT_NE(guid_field, nullptr);
    auto write_guid = tempest::guid{};
    write_guid.data[0] = static_cast<tempest::byte>(0xAB);
    write_guid.data[15] = static_cast<tempest::byte>(0xCD);
    guid_field->set(&instance, &write_guid);
    ASSERT_EQ(instance.guid_value.data[0], static_cast<tempest::byte>(0xAB));
    ASSERT_EQ(instance.guid_value.data[15], static_cast<tempest::byte>(0xCD));

    // 7. Act & Assert: test enum field.
    const auto* status_field = descriptor.find_field("status");
    ASSERT_NE(status_field, nullptr);
    const auto write_status = test_status_code::active;
    status_field->set(&instance, &write_status);
    ASSERT_EQ(instance.status, test_status_code::active);
}

// ============================================================================
// Property Reflection Tests
// ============================================================================

/// @brief Verifies that properties declared via member function pointers and lambdas
/// can be read and written without requiring direct member memory offsets.
TEST(reflection_tests, property_accessors_invoke_functions_and_lambdas)
{
    // 1. Setup: instance of property_test_component and its descriptor.
    auto instance = property_test_component{};
    auto builder = tempest::ecs::type_builder<property_test_component>{"property_component"};
    tempest::ecs::reflect<property_test_component>::describe(builder);
    const auto descriptor = builder.build();

    // 2. Assert: verify fields have no_offset.
    const auto* secret_field = descriptor.find_field("secret");
    ASSERT_NE(secret_field, nullptr);
    ASSERT_EQ(secret_field->offset, tempest::ecs::field_descriptor::no_offset);
    ASSERT_EQ(secret_field->kind, tempest::ecs::field_kind::int32);

    const auto* multiplier_field = descriptor.find_field("multiplier");
    ASSERT_NE(multiplier_field, nullptr);
    ASSERT_EQ(multiplier_field->offset, tempest::ecs::field_descriptor::no_offset);
    ASSERT_EQ(multiplier_field->kind, tempest::ecs::field_kind::float32);

    // 3. Act & Assert: test member function property getter and setter.
    auto read_secret = int32_t{0};
    secret_field->get(&instance, &read_secret);
    ASSERT_EQ(read_secret, 42);

    const auto write_secret = int32_t{100};
    secret_field->set(&instance, &write_secret);
    ASSERT_EQ(instance.secret_value, 100);

    secret_field->get(&instance, &read_secret);
    ASSERT_EQ(read_secret, 100);

    // 4. Act & Assert: test lambda property getter and setter.
    auto read_multiplier = 0.0F;
    multiplier_field->get(&instance, &read_multiplier);
    ASSERT_FLOAT_EQ(read_multiplier, 2.0F);

    const auto write_multiplier = 5.5F;
    multiplier_field->set(&instance, &write_multiplier);
    ASSERT_FLOAT_EQ(instance.multiplier, 5.5F);

    multiplier_field->get(&instance, &read_multiplier);
    ASSERT_FLOAT_EQ(read_multiplier, 5.5F);
}

// ============================================================================
// Transform Component Reflection & Lifecycle Tests
// ============================================================================

/// @brief Verifies that transform_component registers into component_type_registry,
/// exposes reflected fields, and executes post_load to update its internal transform matrix.
TEST(reflection_tests, transform_component_reflection_and_post_load_matrix_update)
{
    // 1. Setup: create registry and register transform_component.
    auto registry = tempest::ecs::component_type_registry{};
    const auto type_index = registry.register_reflected<tempest::ecs::transform_component>();

    // 2. Assert: verify type info and descriptor lookup.
    const auto& info = registry.type_info(type_index);
    ASSERT_TRUE(info.descriptor.has_value());
    ASSERT_TRUE(info.is_serializable);

    const auto* descriptor = registry.descriptor(type_index);
    ASSERT_NE(descriptor, nullptr);
    ASSERT_EQ(descriptor->version, 1U);

    // 3. Assert: verify reflected fields exist.
    const auto* pos_field = descriptor->find_field("position");
    ASSERT_NE(pos_field, nullptr);
    ASSERT_EQ(pos_field->kind, tempest::ecs::field_kind::vec3);

    const auto* rot_field = descriptor->find_field("rotation");
    ASSERT_NE(rot_field, nullptr);
    ASSERT_EQ(rot_field->kind, tempest::ecs::field_kind::quat);

    const auto* scale_field = descriptor->find_field("scale");
    ASSERT_NE(scale_field, nullptr);
    ASSERT_EQ(scale_field->kind, tempest::ecs::field_kind::vec3);

    ASSERT_NE(descriptor->post_load, nullptr);

    // 4. Act: instantiate transform and mutate through field descriptors.
    auto transform = tempest::ecs::transform_component{};
    const auto initial_matrix = transform.matrix();

    const auto target_pos = tempest::math::vec3<float>{10.0F, 20.0F, 30.0F};
    pos_field->set(&transform, &target_pos);

    const auto target_scale = tempest::math::vec3<float>{2.0F, 2.0F, 2.0F};
    scale_field->set(&transform, &target_scale);

    // 5. Act: verify readback through getter.
    auto read_pos = tempest::math::vec3<float>{0.0F, 0.0F, 0.0F};
    pos_field->get(&transform, &read_pos);
    ASSERT_EQ(read_pos.x, 10.0F);
    ASSERT_EQ(read_pos.y, 20.0F);
    ASSERT_EQ(read_pos.z, 30.0F);

    // 6. Act: invoke post_load to recompute the cached matrix.
    descriptor->post_load(&transform);
    const auto updated_matrix = transform.matrix();

    // 7. Assert: verify the transform matrix reflects new position and scale.
    ASSERT_NE(initial_matrix, updated_matrix);
    ASSERT_FLOAT_EQ(updated_matrix[3].x, 10.0F);
    ASSERT_FLOAT_EQ(updated_matrix[3].y, 20.0F);
    ASSERT_FLOAT_EQ(updated_matrix[3].z, 30.0F);
    ASSERT_FLOAT_EQ(updated_matrix[0].x, 2.0F);
    ASSERT_FLOAT_EQ(updated_matrix[1].y, 2.0F);
    ASSERT_FLOAT_EQ(updated_matrix[2].z, 2.0F);
}

// ============================================================================
// Component Serialization Policy Tests
// ============================================================================

/// @brief Verifies that engine components correctly advertise their serialization policies
/// via the is_serializable trait and component_type_registry.
TEST(reflection_tests, serialization_policy_marks_internal_components_appropriately)
{
    // 1. Assert: verify compile-time trait values.
    static_assert(!tempest::ecs::is_serializable_v<tempest::ecs::relationship_component<tempest::ecs::entity>>,
                  "relationship_component must not be serializable");
    static_assert(!tempest::ecs::is_serializable_v<tempest::ecs::self_component>,
                  "self_component must not be serializable");
    static_assert(!tempest::ecs::is_serializable_v<tempest::ecs::transform_history_component>,
                  "transform_history_component must not be serializable");
    static_assert(tempest::ecs::is_serializable_v<tempest::ecs::transform_component>,
                  "transform_component must be serializable");

    // 2. Setup: register all components in component_type_registry.
    auto registry = tempest::ecs::component_type_registry{};
    const auto rel_index = registry.register_type<tempest::ecs::relationship_component<tempest::ecs::entity>>();
    const auto self_index = registry.register_type<tempest::ecs::self_component>();
    const auto history_index = registry.register_type<tempest::ecs::transform_history_component>();
    const auto transform_index = registry.register_type<tempest::ecs::transform_component>();

    // 3. Assert: verify runtime registry flags match compile-time traits.
    ASSERT_FALSE(registry.type_info(rel_index).is_serializable);
    ASSERT_FALSE(registry.type_info(self_index).is_serializable);
    ASSERT_FALSE(registry.type_info(history_index).is_serializable);
    ASSERT_TRUE(registry.type_info(transform_index).is_serializable);
}

// ============================================================================
// UI Hints & Presentation Integration Tests
// ============================================================================

/// @brief Verifies that field_ui_hints supports diverse types (integers, enums,
/// floating point, composite vectors like float3) and custom widget presentations.
TEST(reflection_tests, ui_hints_support_integers_enums_and_composite_float3)
{
    // 1. Setup: declare a component with diverse UI hints.
    struct custom_ui_component
    {
        int32_t health = 100;
        float alpha = 1.0F;
        tempest::math::vec3<float> position = {0.0F, 0.0F, 0.0F};
        tempest::math::vec3<float> color = {1.0F, 1.0F, 1.0F};
        test_status_code status = test_status_code::active;
    };

    auto builder = tempest::ecs::type_builder<custom_ui_component>{"custom_ui"};

    // Integer field with integer min/max and slider widget.
    const auto health_hints = tempest::ecs::field_ui_hints{
        .min_value = int64_t{0},
        .max_value = int64_t{100},
        .drag_speed = 1.0F,
        .widget = tempest::ecs::ui_widget_type::slider,
        .units = "hp",
        .tooltip = "Player health points",
        .display_name = "Health",
    };
    builder.field<&custom_ui_component::health>("health", tempest::ecs::field_flags::none, health_hints);

    // Float field with double min/max and slider widget.
    const auto alpha_hints = tempest::ecs::field_ui_hints{
        .min_value = 0.0,
        .max_value = 1.0,
        .drag_speed = 0.01F,
        .widget = tempest::ecs::ui_widget_type::slider,
    };
    builder.field<&custom_ui_component::alpha>("alpha", tempest::ecs::field_flags::none, alpha_hints);

    // Composite float3 field with per-component vec3 min/max and 3D position drag.
    const auto position_hints = tempest::ecs::field_ui_hints{
        .min_value = tempest::math::vec3<float>{-100.0F, -50.0F, -100.0F},
        .max_value = tempest::math::vec3<float>{100.0F, 50.0F, 100.0F},
        .drag_speed = 0.5F,
        .widget = tempest::ecs::ui_widget_type::drag,
        .units = "m",
        .display_name = "World Position",
    };
    builder.field<&custom_ui_component::position>("position", tempest::ecs::field_flags::none, position_hints);

    // Composite float3 field representing RGB color with color_picker widget.
    const auto color_hints = tempest::ecs::field_ui_hints{
        .min_value = 0.0,
        .max_value = 1.0,
        .widget = tempest::ecs::ui_widget_type::color_picker,
        .display_name = "Tint Color",
    };
    builder.field<&custom_ui_component::color>("color", tempest::ecs::field_flags::none, color_hints);

    // Enum field with dropdown widget.
    const auto status_hints = tempest::ecs::field_ui_hints{
        .widget = tempest::ecs::ui_widget_type::dropdown,
        .display_name = "Account Status",
    };
    builder.field<&custom_ui_component::status>("status", tempest::ecs::field_flags::none, status_hints);

    const auto descriptor = builder.build();

    // 2. Assert: verify health hints.
    const auto* health_field = descriptor.find_field("health");
    ASSERT_NE(health_field, nullptr);
    ASSERT_TRUE(health_field->hints.min_value.has_value());
    ASSERT_TRUE(health_field->hints.max_value.has_value());
    ASSERT_TRUE(tempest::holds_alternative<int64_t>(*health_field->hints.min_value));
    ASSERT_EQ(tempest::get<int64_t>(*health_field->hints.min_value), 0);
    ASSERT_EQ(tempest::get<int64_t>(*health_field->hints.max_value), 100);
    ASSERT_EQ(health_field->hints.widget, tempest::ecs::ui_widget_type::slider);
    ASSERT_EQ(health_field->hints.units, "hp");
    ASSERT_EQ(health_field->hints.display_name, "Health");

    // 3. Assert: verify alpha hints.
    const auto* alpha_field = descriptor.find_field("alpha");
    ASSERT_NE(alpha_field, nullptr);
    ASSERT_TRUE(tempest::holds_alternative<double>(*alpha_field->hints.min_value));
    ASSERT_DOUBLE_EQ(tempest::get<double>(*alpha_field->hints.min_value), 0.0);
    ASSERT_DOUBLE_EQ(tempest::get<double>(*alpha_field->hints.max_value), 1.0);

    // 4. Assert: verify composite float3 position hints with per-component vec3 bounds.
    const auto* pos_field = descriptor.find_field("position");
    ASSERT_NE(pos_field, nullptr);
    ASSERT_TRUE(pos_field->hints.min_value.has_value());
    ASSERT_TRUE(tempest::holds_alternative<tempest::math::vec3<float>>(*pos_field->hints.min_value));
    const auto pos_min = tempest::get<tempest::math::vec3<float>>(*pos_field->hints.min_value);
    const auto pos_max = tempest::get<tempest::math::vec3<float>>(*pos_field->hints.max_value);
    ASSERT_FLOAT_EQ(pos_min.x, -100.0F);
    ASSERT_FLOAT_EQ(pos_min.y, -50.0F);
    ASSERT_FLOAT_EQ(pos_max.x, 100.0F);
    ASSERT_FLOAT_EQ(pos_max.y, 50.0F);
    ASSERT_EQ(pos_field->hints.widget, tempest::ecs::ui_widget_type::drag);
    ASSERT_FLOAT_EQ(pos_field->hints.drag_speed, 0.5F);

    // 5. Assert: verify composite float3 color hints with color_picker widget.
    const auto* color_field = descriptor.find_field("color");
    ASSERT_NE(color_field, nullptr);
    ASSERT_EQ(color_field->hints.widget, tempest::ecs::ui_widget_type::color_picker);
    ASSERT_TRUE(tempest::holds_alternative<double>(*color_field->hints.min_value));
    ASSERT_DOUBLE_EQ(tempest::get<double>(*color_field->hints.min_value), 0.0);

    // 6. Assert: verify enum dropdown hints.
    const auto* status_field = descriptor.find_field("status");
    ASSERT_NE(status_field, nullptr);
    ASSERT_EQ(status_field->hints.widget, tempest::ecs::ui_widget_type::dropdown);
    ASSERT_FALSE(status_field->hints.min_value.has_value());
    ASSERT_FALSE(status_field->hints.max_value.has_value());
}
