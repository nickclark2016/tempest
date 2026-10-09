#include <gtest/gtest.h>

#include <tempest/meta.hpp>

struct Foo
{
};

struct Bar
{
};

TEST(tempest_meta, test_type_info)
{
    auto foo_type_info = tempest::core::type_info(tempest::in_place_type_t<Foo>{});
    auto bar_type_info = tempest::core::type_info(tempest::in_place_type_t<Bar>{});

    ASSERT_EQ(foo_type_info.name(), "Foo");
    ASSERT_EQ(bar_type_info.name(), "Bar");
    ASSERT_NE(foo_type_info.hash(), bar_type_info.hash());
    ASSERT_NE(foo_type_info.index(), bar_type_info.index());
}

TEST(tempest_meta, test_type_info_with_const)
{
    auto foo_type_info = tempest::core::type_id<Foo>();
    auto cfoo_type_info = tempest::core::type_id<const Foo>();

    ASSERT_EQ(foo_type_info.name(), "Foo");
    ASSERT_EQ(cfoo_type_info.name(), "Foo");
    ASSERT_EQ(foo_type_info.index(), cfoo_type_info.index());
    ASSERT_EQ(foo_type_info.hash(), cfoo_type_info.hash());
}

// ============================================================================
// Test Support Types
// ============================================================================

namespace test_namespace
{
    struct namespaced_struct
    {
    };

    namespace nested_namespace
    {
        struct nested_struct
        {
        };
    } // namespace nested_namespace
} // namespace test_namespace

class private_member_class
{
  public:
    constexpr private_member_class() = default;

  private:
    [[maybe_unused]] int _secret = 0;
};

enum class test_entity : uint64_t
{
    none = 0,
};

template <typename EntityType>
struct test_relationship_component
{
};

template <typename First, typename Second, typename Third>
struct multi_arg_template
{
};

template <typename Element>
struct outer_template
{
};

template <typename Element>
struct inner_template
{
};

struct my_struct_class
{
};

struct classifier
{
};

struct unionizer
{
};

namespace
{
    struct anon_namespace_struct
    {
    };
} // namespace

// ============================================================================
// Compile-Time static_assert Validations
// ============================================================================

static_assert(tempest::core::normalized_type_name<int32_t>() == "int");
static_assert(tempest::core::normalized_type_name<float>() == "float");
static_assert(tempest::core::normalized_type_name<uint64_t>() == "unsigned long long");
static_assert(tempest::core::normalized_type_name<Foo>() == "Foo");
static_assert(tempest::core::normalized_type_name<const Foo>() == "const Foo");
static_assert(tempest::core::normalized_type_name<test_namespace::namespaced_struct>() ==
              "test_namespace::namespaced_struct");
static_assert(tempest::core::normalized_type_name<test_namespace::nested_namespace::nested_struct>() ==
              "test_namespace::nested_namespace::nested_struct");
static_assert(tempest::core::normalized_type_name<private_member_class>() == "private_member_class");
static_assert(tempest::core::normalized_type_name<test_relationship_component<test_entity>>() ==
              "test_relationship_component<test_entity>");
static_assert(tempest::core::normalized_type_name<multi_arg_template<Foo, Bar, float>>() ==
              "multi_arg_template<Foo,Bar,float>");
static_assert(tempest::core::normalized_type_name<outer_template<inner_template<Foo>>>() ==
              "outer_template<inner_template<Foo>>");
static_assert(tempest::core::normalized_type_name<my_struct_class>() == "my_struct_class");
static_assert(tempest::core::normalized_type_name<classifier>() == "classifier");
static_assert(tempest::core::normalized_type_name<unionizer>() == "unionizer");

static_assert(tempest::core::normalized_type_hash_v<int32_t> != 0);
static_assert(tempest::core::normalized_type_hash_v<Foo> != 0);
static_assert(tempest::core::normalized_type_hash_v<Foo> != tempest::core::normalized_type_hash_v<Bar>);

static_assert(tempest::core::is_normalized_type_name_valid_v<Foo>);
static_assert(tempest::core::is_normalized_type_name_valid_v<int32_t>);
static_assert(!tempest::core::is_normalized_type_name_valid_v<anon_namespace_struct>);

// ============================================================================
// Type Name Normalization Tests
// ============================================================================

/// @brief Verifies that normalized_type_name produces identical and canonical
/// names for primitive integer and floating-point types across toolchains.
TEST(tempest_meta, test_normalized_primitive_types)
{
    // 1. Setup & Act: Query normalized names for primitive scalar types.
    const auto int32_name = tempest::core::normalized_type_name<int32_t>();
    const auto float_name = tempest::core::normalized_type_name<float>();
    const auto uint64_name = tempest::core::normalized_type_name<uint64_t>();

    // 2. Assert: Verify canonical representations match expected exact strings.
    ASSERT_EQ(int32_name, "int");
    ASSERT_EQ(float_name, "float");
    ASSERT_EQ(uint64_name, "unsigned long long");
}

/// @brief Verifies that simple structs have elaborated keywords (struct/class)
/// stripped and match their unqualified names.
TEST(tempest_meta, test_normalized_simple_structs)
{
    // 1. Setup & Act: Query normalized names for plain global structs.
    const auto foo_name = tempest::core::normalized_type_name<Foo>();
    const auto bar_name = tempest::core::normalized_type_name<Bar>();

    // 2. Assert: Verify elaborated keywords are completely stripped.
    ASSERT_EQ(foo_name, "Foo");
    ASSERT_EQ(bar_name, "Bar");
}

/// @brief Verifies shape test invariants: namespaced structs, nested namespaces,
/// classes with private members, templates with enum class parameters, multi-argument
/// templates, nested template brackets, and const qualifiers.
TEST(tempest_meta, test_normalized_shape_tests)
{
    // 1. Setup & Act: Query normalized names across diverse C++ language shapes.
    const auto namespaced_name = tempest::core::normalized_type_name<test_namespace::namespaced_struct>();
    const auto nested_name = tempest::core::normalized_type_name<test_namespace::nested_namespace::nested_struct>();
    const auto private_class_name = tempest::core::normalized_type_name<private_member_class>();
    const auto rel_comp_name = tempest::core::normalized_type_name<test_relationship_component<test_entity>>();
    const auto multi_arg_name = tempest::core::normalized_type_name<multi_arg_template<Foo, Bar, float>>();
    const auto nested_templ_name = tempest::core::normalized_type_name<outer_template<inner_template<Foo>>>();
    const auto const_foo_name = tempest::core::normalized_type_name<const Foo>();

    // 2. Assert: Exact string matching pins cross-toolchain identical formatting.
    ASSERT_EQ(namespaced_name, "test_namespace::namespaced_struct");
    ASSERT_EQ(nested_name, "test_namespace::nested_namespace::nested_struct");
    ASSERT_EQ(private_class_name, "private_member_class");
    ASSERT_EQ(rel_comp_name, "test_relationship_component<test_entity>");
    ASSERT_EQ(multi_arg_name, "multi_arg_template<Foo,Bar,float>");
    ASSERT_EQ(nested_templ_name, "outer_template<inner_template<Foo>>");
    ASSERT_EQ(const_foo_name, "const Foo");
}

/// @brief Verifies that identifiers containing keyword substrings ('struct', 'class', 'union')
/// are preserved as whole identifiers and not corrupted by substring stripping.
TEST(tempest_meta, test_normalized_keyword_substring_identifiers)
{
    // 1. Setup & Act: Query normalized names for types with keyword substrings.
    const auto struct_class_name = tempest::core::normalized_type_name<my_struct_class>();
    const auto classifier_name = tempest::core::normalized_type_name<classifier>();
    const auto unionizer_name = tempest::core::normalized_type_name<unionizer>();

    // 2. Assert: Verify identifiers remain intact.
    ASSERT_EQ(struct_class_name, "my_struct_class");
    ASSERT_EQ(classifier_name, "classifier");
    ASSERT_EQ(unionizer_name, "unionizer");
}

/// @brief Verifies that normalized_type_hash_v produces deterministic 64-bit FNV-1a
/// hashes matching the normalized string, and discriminates distinct types.
TEST(tempest_meta, test_normalized_type_hash)
{
    // 1. Setup: Calculate runtime FNV-1a hashes of the expected normalized strings.
    const auto expected_foo_hash = tempest::core::detail::fnv1a_64("Foo");
    const auto expected_bar_hash = tempest::core::detail::fnv1a_64("Bar");
    const auto expected_rel_hash =
        tempest::core::detail::fnv1a_64("test_relationship_component<test_entity>");

    // 2. Act: Retrieve compile-time normalized_type_hash_v values.
    const auto foo_hash = tempest::core::normalized_type_hash_v<Foo>;
    const auto bar_hash = tempest::core::normalized_type_hash_v<Bar>;
    const auto rel_hash = tempest::core::normalized_type_hash_v<test_relationship_component<test_entity>>;

    // 3. Assert: Hashes must match the normalized string FNV-1a hash and distinguish types.
    ASSERT_EQ(foo_hash, expected_foo_hash);
    ASSERT_EQ(bar_hash, expected_bar_hash);
    ASSERT_EQ(rel_hash, expected_rel_hash);
    ASSERT_NE(foo_hash, bar_hash);
    ASSERT_NE(foo_hash, rel_hash);
}

/// @brief Verifies that types declared in invalid scopes (anonymous namespaces
/// and lambda scopes) are detected and flagged invalid.
TEST(tempest_meta, test_invalid_scope_rejection)
{
    // 1. Setup: Define a lambda.
    auto sample_lambda = []() {
    };

    // 2. Act & Assert: Verify that invalid scopes (anonymous namespace, lambda) are rejected.
    ASSERT_FALSE(tempest::core::is_normalized_type_name_valid_v<anon_namespace_struct>);
    ASSERT_FALSE(tempest::core::is_normalized_type_name_valid_v<decltype(sample_lambda)>);

    // 3. Assert: Valid global and namespaced types remain accepted.
    ASSERT_TRUE(tempest::core::is_normalized_type_name_valid_v<Foo>);
    ASSERT_TRUE(tempest::core::is_normalized_type_name_valid_v<int32_t>);
    ASSERT_TRUE(tempest::core::is_normalized_type_name_valid_v<test_namespace::namespaced_struct>);
}

// ============================================================================
// Fixed String Buffer & Iterator Helper Tests
// ============================================================================

/// @brief Verifies that fixed_string_buffer conforms to standard container
/// accessor and iterator semantics (begin, end, cbegin, cend, size, empty, back, pop_back).
TEST(tempest_meta, test_fixed_string_buffer_container_accessors)
{
    // 1. Setup: Construct an empty fixed_string_buffer.
    auto buffer = tempest::core::detail::fixed_string_buffer<64>{};

    // 2. Assert Initial State: Verify buffer starts empty with matching iterators.
    ASSERT_TRUE(buffer.empty());
    ASSERT_EQ(buffer.size(), 0);
    ASSERT_EQ(buffer.begin(), buffer.end());
    ASSERT_EQ(buffer.cbegin(), buffer.cend());

    // 3. Act: Append characters and inspect container accessors.
    buffer.push_back('H');
    ASSERT_FALSE(buffer.empty());
    ASSERT_EQ(buffer.size(), 1);
    ASSERT_EQ(buffer.back(), 'H');

    buffer.append("ello");
    ASSERT_EQ(buffer.size(), 5);
    ASSERT_EQ(buffer.back(), 'o');
    ASSERT_EQ(buffer.view(), "Hello");

    // 4. Assert Iterators: Verify forward traversal matches expected sequence.
    const auto& const_buffer = buffer;
    auto character_index = size_t{0};
    const auto expected_characters = "Hello";
    for (const auto character : const_buffer)
    {
        ASSERT_EQ(character, expected_characters[character_index]);
        ++character_index;
    }
    ASSERT_EQ(character_index, 5);

    // 5. Act: Modify through back() reference and verify change.
    buffer.back() = '!';
    ASSERT_EQ(buffer.view(), "Hell!");

    // 6. Act: Pop elements and verify size and back progression.
    buffer.pop_back();
    ASSERT_EQ(buffer.size(), 4);
    ASSERT_EQ(buffer.back(), 'l');
    ASSERT_EQ(buffer.view(), "Hell");

    buffer.pop_back();
    buffer.pop_back();
    buffer.pop_back();
    buffer.pop_back();
    ASSERT_TRUE(buffer.empty());
    ASSERT_EQ(buffer.size(), 0);
    ASSERT_EQ(buffer.begin(), buffer.end());
}

/// @brief Verifies decomposed normalization helpers: word boundaries, space skipping,
/// modifier/keyword consumption, and trailing space trimming.
TEST(tempest_meta, test_normalization_decomposed_helpers)
{
    // 1. Setup & Act: Test is_word_boundary.
    const auto sample_text = tempest::string_view{"struct Foo_Bar"};
    ASSERT_TRUE(tempest::core::detail::is_word_boundary(sample_text.begin(), sample_text.begin()));
    ASSERT_FALSE(tempest::core::detail::is_word_boundary(sample_text.begin(), sample_text.begin() + 1));
    ASSERT_TRUE(tempest::core::detail::is_word_boundary(sample_text.begin(), sample_text.begin() + 7));

    // 2. Setup & Act: Test skip_spaces.
    const auto spaced_text = tempest::string_view{"   target"};
    const auto skipped = tempest::core::detail::skip_spaces(spaced_text.begin(), spaced_text.end());
    ASSERT_EQ(skipped, spaced_text.begin() + 3);
    ASSERT_EQ(*skipped, 't');

    // 3. Setup & Act: Test try_consume_modifier.
    const auto cdecl_text = tempest::string_view{"__cdecl void"};
    const auto after_modifier = tempest::core::detail::try_consume_modifier(cdecl_text.begin(), cdecl_text.end());
    ASSERT_NE(after_modifier, nullptr);
    ASSERT_EQ(*after_modifier, 'v');

    // 4. Setup & Act: Test try_consume_elaborated_keyword.
    const auto struct_text = tempest::string_view{"struct MyType"};
    const auto after_keyword =
        tempest::core::detail::try_consume_elaborated_keyword(struct_text.begin(), struct_text.end());
    ASSERT_NE(after_keyword, nullptr);
    ASSERT_EQ(*after_keyword, 'M');

    // 5. Setup & Act: Test try_consume_type_replacement.
    auto replacement_buffer = tempest::core::detail::fixed_string_buffer<64>{};
    const auto int64_text = tempest::string_view{"__int64 value"};
    const auto after_replacement = tempest::core::detail::try_consume_type_replacement(
        int64_text.begin(), int64_text.end(), replacement_buffer);
    ASSERT_NE(after_replacement, nullptr);
    ASSERT_EQ(replacement_buffer.view(), "long long");
    ASSERT_EQ(*after_replacement, ' ');

    // 6. Setup & Act: Test trim_trailing_spaces.
    auto space_buffer = tempest::core::detail::fixed_string_buffer<64>{};
    space_buffer.append("TrimMe   ");
    tempest::core::detail::trim_trailing_spaces(space_buffer);
    ASSERT_EQ(space_buffer.view(), "TrimMe");
    ASSERT_EQ(space_buffer.size(), 6);
}
