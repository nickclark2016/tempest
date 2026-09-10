#include <gtest/gtest.h>

#include <tempest/json.hpp>
#include <tempest/limits.hpp>
#include <tempest/memory.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

//==============================================================================
// Tracking Test Allocator
//==============================================================================

namespace
{
    class tracking_allocator final : public tempest::abstract_allocator
    {
      public:
        tracking_allocator() = default;
        ~tracking_allocator() override = default;

        auto allocate(size_t size, size_t alignment,
                      [[maybe_unused]] tempest::source_location loc = tempest::source_location::current())
            -> void* override
        {
            if (size == 0)
            {
                return nullptr;
            }
            ++active_allocations;
            ++total_allocations;
            return _sys_alloc.allocate(size, alignment, loc);
        }

        void deallocate(void* ptr) override
        {
            if (ptr != nullptr)
            {
                --active_allocations;
                ++total_deallocations;
                _sys_alloc.deallocate(ptr);
            }
        }

        size_t active_allocations{0};
        size_t total_allocations{0};
        size_t total_deallocations{0};

      private:
        tempest::system_allocator _sys_alloc;
    };
} // namespace

//==============================================================================
// Document Parsing & Lifecycle Tests
//==============================================================================

/// @brief Verify parsing valid JSON strings returns expected root elements.
TEST(json_tests, document_from_string_valid)
{
    // 1. Setup: Allocator and test JSON string
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{R"({ "name": "tempest", "count": 42, "enabled": true })"};

    // 2. Act: Parse document from string
    auto doc_res = tempest::json_document::from_string(json_str, alloc);

    // 3. Assert: Document parsed successfully and properties are valid
    ASSERT_TRUE(doc_res.has_value());
    auto root = doc_res->root();
    EXPECT_TRUE(root.is_valid());
    EXPECT_TRUE(root.is_object());
    EXPECT_FALSE(root.is_null());
}

/// @brief Verify parsing JSON from a byte span.
TEST(json_tests, document_from_bytes_valid)
{
    // 1. Setup: Allocator and byte buffer
    auto alloc = tempest::system_allocator{};
    constexpr char raw_str[] = R"([1, 2, 3, "test"])";
    auto bytes =
        tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(raw_str), sizeof(raw_str) - 1};

    // 2. Act: Parse document from bytes
    auto doc_res = tempest::json_document::from_bytes(bytes, alloc);

    // 3. Assert: Document parsed successfully and has root array
    ASSERT_TRUE(doc_res.has_value());
    auto root = doc_res->root();
    EXPECT_TRUE(root.is_valid());
    EXPECT_TRUE(root.is_array());
}

/// @brief Verify syntax errors, incomplete tokens, and empty strings return parse_error.
TEST(json_tests, document_parse_syntax_errors)
{
    // 1. Setup: Allocator and malformed JSON payloads
    auto alloc = tempest::system_allocator{};

    // 2. Act & Assert: Empty string fails
    auto res_empty = tempest::json_document::from_string("", alloc);
    EXPECT_FALSE(res_empty.has_value());
    EXPECT_EQ(res_empty.error(), tempest::json_error::parse_error);

    // 3. Act & Assert: Unclosed braces fail
    auto res_unclosed = tempest::json_document::from_string(R"({ "key": 1 )", alloc);
    EXPECT_FALSE(res_unclosed.has_value());
    EXPECT_EQ(res_unclosed.error(), tempest::json_error::parse_error);

    // 4. Act & Assert: Random binary garbage fails
    auto res_junk = tempest::json_document::from_string("NOT JSON AT ALL", alloc);
    EXPECT_FALSE(res_junk.has_value());
    EXPECT_EQ(res_junk.error(), tempest::json_error::parse_error);
}

/// @brief Verify document move construction and move assignment transfer ownership.
TEST(json_tests, document_move_semantics)
{
    // 1. Setup: Parse initial document
    auto alloc = tempest::system_allocator{};
    auto doc1_res = tempest::json_document::from_string(R"({ "x": 10 })", alloc);
    ASSERT_TRUE(doc1_res.has_value());

    // 2. Act: Move construct doc2 from doc1
    auto doc2 = tempest::move(*doc1_res);

    // 3. Assert: doc1 is empty/invalid and doc2 owns the root
    EXPECT_FALSE(doc1_res->is_valid());
    EXPECT_TRUE(doc2.is_valid());
    EXPECT_EQ(doc2.root()["x"].as_int32().value(), 10);

    // 4. Act: Move assign doc3 from doc2
    auto doc3 = tempest::json_document{};
    doc3 = tempest::move(doc2);

    // 5. Assert: doc2 is empty and doc3 owns the root
    EXPECT_FALSE(doc2.is_valid());
    EXPECT_TRUE(doc3.is_valid());
    EXPECT_EQ(doc3.root()["x"].as_int32().value(), 10);
}

/// @brief Verify all allocated memory is completely freed upon document destruction.
TEST(json_tests, document_custom_allocator_lifecycle)
{
    // 1. Setup: Instantiate tracking allocator
    auto track_alloc = tracking_allocator{};

    // 2. Act: Parse document and query values within nested scope
    {
        auto doc_res = tempest::json_document::from_string(
            R"({ "mesh": "Cube", "indices": [0, 1, 2, 3], "nested": { "val": 3.14 } })", track_alloc);
        ASSERT_TRUE(doc_res.has_value());
        EXPECT_GT(track_alloc.active_allocations, 0U);

        auto root = doc_res->root();
        EXPECT_TRUE(root["mesh"].is_string());
        EXPECT_TRUE(root["indices"].is_array());
    }

    // 3. Assert: After document destruction, active allocations must be exactly 0
    EXPECT_EQ(track_alloc.active_allocations, 0U);
    EXPECT_GT(track_alloc.total_allocations, 0U);
    EXPECT_EQ(track_alloc.total_allocations, track_alloc.total_deallocations);
}

//==============================================================================
// Value Types & Strict Numeric Matching Tests
//==============================================================================

/// @brief Verify type query predicates accurately identify JSON primitives.
TEST(json_tests, value_type_predicates)
{
    // 1. Setup: Parse document with diverse types
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{
        R"({ "null_val": null, "bool_val": false, "int_val": 42, "real_val": 3.14, "str_val": "text", "arr_val": [], "obj_val": {} })"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    auto root = doc->root();

    // 2. Act & Assert: Check each field's predicates
    EXPECT_TRUE(root["null_val"].is_null());
    EXPECT_FALSE(root["null_val"].is_bool());

    EXPECT_TRUE(root["bool_val"].is_bool());
    EXPECT_FALSE(root["bool_val"].is_number());

    EXPECT_TRUE(root["int_val"].is_number());
    EXPECT_TRUE(root["int_val"].is_int());
    EXPECT_FALSE(root["int_val"].is_real());

    EXPECT_TRUE(root["real_val"].is_number());
    EXPECT_TRUE(root["real_val"].is_real());
    EXPECT_FALSE(root["real_val"].is_int());

    EXPECT_TRUE(root["str_val"].is_string());
    EXPECT_TRUE(root["arr_val"].is_array());
    EXPECT_TRUE(root["obj_val"].is_object());
}

/// @brief Verify boolean and string conversions, including error cases on mismatched types.
TEST(json_tests, value_bool_and_string)
{
    // 1. Setup: Parse JSON with boolean and string fields
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{R"({ "flag": true, "msg": "hello \"world\"" })"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    auto root = doc->root();

    // 2. Act & Assert: Boolean conversions
    auto bool_val = root["flag"].as_bool();
    ASSERT_TRUE(bool_val.has_value());
    EXPECT_TRUE(*bool_val);

    auto bool_mismatch = root["msg"].as_bool();
    EXPECT_FALSE(bool_mismatch.has_value());
    EXPECT_EQ(bool_mismatch.error(), tempest::json_error::type_mismatch);

    // 3. Act & Assert: String conversions
    auto str_val = root["msg"].as_string();
    ASSERT_TRUE(str_val.has_value());
    EXPECT_EQ(*str_val, "hello \"world\"");

    auto str_mismatch = root["flag"].as_string();
    EXPECT_FALSE(str_mismatch.has_value());
    EXPECT_EQ(str_mismatch.error(), tempest::json_error::type_mismatch);
}

/// @brief Verify strict numeric matching differentiates integers and reals and guards against overflow.
TEST(json_tests, value_strict_numeric_matching)
{
    // 1. Setup: Parse numbers of varying representations
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{
        R"({ "real_num": 100.5, "zero_real": 0.0, "int_pos": 42, "int_neg": -10, "int_large": 9223372036854775807 })"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    auto root = doc->root();

    // 2. Act & Assert: Real numbers succeed with as_double/as_float
    auto double_val = root["real_num"].as_double();
    ASSERT_TRUE(double_val.has_value());
    EXPECT_DOUBLE_EQ(*double_val, 100.5);

    auto float_val = root["real_num"].as_float();
    ASSERT_TRUE(float_val.has_value());
    EXPECT_FLOAT_EQ(*float_val, 100.5F);

    // 3. Act & Assert: Real numbers fail on as_int64 (strict type matching)
    auto real_as_int = root["real_num"].as_int64();
    EXPECT_FALSE(real_as_int.has_value());
    EXPECT_EQ(real_as_int.error(), tempest::json_error::type_mismatch);

    // 4. Act & Assert: Integers fail on as_double (strict type matching)
    auto int_as_double = root["int_pos"].as_double();
    EXPECT_FALSE(int_as_double.has_value());
    EXPECT_EQ(int_as_double.error(), tempest::json_error::type_mismatch);

    // 5. Act & Assert: Signed integers extract properly
    auto neg_val = root["int_neg"].as_int64();
    ASSERT_TRUE(neg_val.has_value());
    EXPECT_EQ(*neg_val, -10);

    // Negative integer fails on as_uint64 (out_of_range)
    auto neg_as_uint = root["int_neg"].as_uint64();
    EXPECT_FALSE(neg_as_uint.has_value());
    EXPECT_EQ(neg_as_uint.error(), tempest::json_error::out_of_range);

    // 6. Act & Assert: Integer bounds checking
    auto large_as_int32 = root["int_large"].as_int32();
    EXPECT_FALSE(large_as_int32.has_value());
    EXPECT_EQ(large_as_int32.error(), tempest::json_error::out_of_range);

    auto pos_as_uint32 = root["int_pos"].as_uint32();
    ASSERT_TRUE(pos_as_uint32.has_value());
    EXPECT_EQ(*pos_as_uint32, 42U);
}

/// @brief Verify ergonomic get(out) helper assigns on success and leaves out unchanged on failure.
TEST(json_tests, value_ergonomic_get_helpers)
{
    // 1. Setup: Parse JSON
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{R"({ "count": 100, "name": "sample" })"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    auto root = doc->root();

    // 2. Act & Assert: Successful get
    auto count = tempest::uint32_t{0};
    auto count_ok = root["count"].get(count);
    EXPECT_TRUE(count_ok);
    EXPECT_EQ(count, 100U);

    // 3. Act & Assert: Failed get leaves out untouched
    auto str_out = tempest::string_view{"unchanged"};
    auto fail_ok = root["count"].get(str_out);
    EXPECT_FALSE(fail_ok);
    EXPECT_EQ(str_out, "unchanged");
}

//==============================================================================
// Null-Object Chaining & Subscripting Tests
//==============================================================================

/// @brief Verify indexing missing keys or out-of-bounds indices safely chains null json_value.
TEST(json_tests, value_subscript_null_object_chaining)
{
    // 1. Setup: Parse nested structure
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{R"({ "a": { "b": [10, 20] } })"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    auto root = doc->root();

    // 2. Act & Assert: Valid access
    EXPECT_EQ(root["a"]["b"][1].as_int32().value(), 20);

    // 3. Act & Assert: Missing intermediate key does not crash and chains safely
    auto missing = root["non_existent"]["further"][99]["leaf"];
    EXPECT_FALSE(missing.is_valid());
    EXPECT_TRUE(missing.is_null());

    auto extract_fail = missing.as_string();
    EXPECT_FALSE(extract_fail.has_value());
    EXPECT_EQ(extract_fail.error(), tempest::json_error::type_mismatch);

    // 4. Act & Assert: Subscripting non-object / non-array primitives returns null value
    auto invalid_subscript = root["a"]["b"][0]["cannot_index_int"];
    EXPECT_FALSE(invalid_subscript.is_valid());
}

//==============================================================================
// Object Iteration & Membership Tests
//==============================================================================

/// @brief Verify json_object iteration, structured bindings, and size queries.
TEST(json_tests, object_iteration_and_structured_bindings)
{
    // 1. Setup: Parse multi-field object
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{R"({ "first": 1, "second": 2, "third": 3 })"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    // 2. Act: Obtain json_object
    auto obj_res = doc->root().as_object();
    ASSERT_TRUE(obj_res.has_value());
    auto obj = *obj_res;

    // 3. Assert: Size and containment
    EXPECT_EQ(obj.size(), 3U);
    EXPECT_FALSE(obj.empty());
    EXPECT_TRUE(obj.contains("first"));
    EXPECT_TRUE(obj.contains("second"));
    EXPECT_FALSE(obj.contains("fourth"));

    // 4. Act & Assert: Range-based for loop with structured bindings
    auto keys = tempest::vector<tempest::string_view>{};
    auto values = tempest::vector<tempest::int32_t>{};

    for (auto [key, val] : obj)
    {
        keys.push_back(key);
        values.push_back(val.as_int32().value());
    }

    ASSERT_EQ(keys.size(), 3U);
    EXPECT_EQ(keys[0], "first");
    EXPECT_EQ(keys[1], "second");
    EXPECT_EQ(keys[2], "third");
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
}

//==============================================================================
// Array Iteration & Indexing Tests
//==============================================================================

/// @brief Verify json_array iteration, indexing, and size queries.
TEST(json_tests, array_iteration_and_indexing)
{
    // 1. Setup: Parse array
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{R"([100, 200, 300])"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    // 2. Act: Obtain json_array
    auto arr_res = doc->root().as_array();
    ASSERT_TRUE(arr_res.has_value());
    auto arr = *arr_res;

    // 3. Assert: Size and index access
    EXPECT_EQ(arr.size(), 3U);
    EXPECT_FALSE(arr.empty());
    EXPECT_EQ(arr[0].as_int32().value(), 100);
    EXPECT_EQ(arr[1].as_int32().value(), 200);
    EXPECT_EQ(arr[2].as_int32().value(), 300);
    EXPECT_FALSE(arr[3].is_valid());

    // 4. Act & Assert: Range-based for loop
    auto total = tempest::int32_t{0};
    auto count = tempest::size_t{0};
    for (auto val : arr)
    {
        total += val.as_int32().value();
        ++count;
    }

    EXPECT_EQ(count, 3U);
    EXPECT_EQ(total, 600);
}

//==============================================================================
// 8-bit & 16-bit Integer Range and Overflow Tests
//==============================================================================

/// @brief Verify as_int8, as_uint8, as_int16, as_uint16 correctly extract in-range boundaries and reject out-of-range
/// values.
TEST(json_tests, narrow_integer_conversions_and_bounds)
{
    // 1. Setup: JSON payload containing valid boundaries and out-of-range values
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{R"({
        "i8_min": -128,
        "i8_max": 127,
        "i8_underflow": -129,
        "i8_overflow": 128,
        "u8_min": 0,
        "u8_max": 255,
        "u8_overflow": 256,
        "u8_neg": -1,
        "i16_min": -32768,
        "i16_max": 32767,
        "i16_underflow": -32769,
        "i16_overflow": 32768,
        "u16_min": 0,
        "u16_max": 65535,
        "u16_overflow": 65536,
        "u16_neg": -5
    })"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    auto root = doc->root();

    // 2. Act & Assert: 8-bit signed integer conversions
    EXPECT_EQ(root["i8_min"].as_int8().value(), -128);
    EXPECT_EQ(root["i8_max"].as_int8().value(), 127);
    EXPECT_EQ(root["i8_underflow"].as_int8().error(), tempest::json_error::out_of_range);
    EXPECT_EQ(root["i8_overflow"].as_int8().error(), tempest::json_error::out_of_range);

    // 3. Act & Assert: 8-bit unsigned integer conversions
    EXPECT_EQ(root["u8_min"].as_uint8().value(), 0U);
    EXPECT_EQ(root["u8_max"].as_uint8().value(), 255U);
    EXPECT_EQ(root["u8_overflow"].as_uint8().error(), tempest::json_error::out_of_range);
    EXPECT_EQ(root["u8_neg"].as_uint8().error(), tempest::json_error::out_of_range);

    // 4. Act & Assert: 16-bit signed integer conversions
    EXPECT_EQ(root["i16_min"].as_int16().value(), -32768);
    EXPECT_EQ(root["i16_max"].as_int16().value(), 32767);
    EXPECT_EQ(root["i16_underflow"].as_int16().error(), tempest::json_error::out_of_range);
    EXPECT_EQ(root["i16_overflow"].as_int16().error(), tempest::json_error::out_of_range);

    // 5. Act & Assert: 16-bit unsigned integer conversions
    EXPECT_EQ(root["u16_min"].as_uint16().value(), 0U);
    EXPECT_EQ(root["u16_max"].as_uint16().value(), 65535U);
    EXPECT_EQ(root["u16_overflow"].as_uint16().error(), tempest::json_error::out_of_range);
    EXPECT_EQ(root["u16_neg"].as_uint16().error(), tempest::json_error::out_of_range);
}

//==============================================================================
// Floating-Point Limits & as_number Coercion Tests
//==============================================================================

/// @brief Verify as_float detects float overflow and as_number coerces any numeric representation to double.
TEST(json_tests, float_limits_and_as_number_coercion)
{
    // 1. Setup: Document with float overflow and mixed numeric representations
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{R"({
        "huge_double": 1e50,
        "neg_huge_double": -1e50,
        "valid_float": 3.14159,
        "int_val": 42,
        "neg_int_val": -100,
        "real_val": 2.71828
    })"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    auto root = doc->root();

    // 2. Act & Assert: Float bounds checking
    EXPECT_EQ(root["huge_double"].as_float().error(), tempest::json_error::out_of_range);
    EXPECT_EQ(root["neg_huge_double"].as_float().error(), tempest::json_error::out_of_range);

    auto valid_flt = root["valid_float"].as_float();
    ASSERT_TRUE(valid_flt.has_value());
    EXPECT_NEAR(*valid_flt, 3.14159F, 1e-5F);

    // 3. Act & Assert: as_number extracts integers, negative integers, and reals as double
    auto num_from_int = root["int_val"].as_number();
    ASSERT_TRUE(num_from_int.has_value());
    EXPECT_DOUBLE_EQ(*num_from_int, 42.0);

    auto num_from_neg = root["neg_int_val"].as_number();
    ASSERT_TRUE(num_from_neg.has_value());
    EXPECT_DOUBLE_EQ(*num_from_neg, -100.0);

    auto num_from_real = root["real_val"].as_number();
    ASSERT_TRUE(num_from_real.has_value());
    EXPECT_DOUBLE_EQ(*num_from_real, 2.71828);

    // 4. Act & Assert: Non-numbers fail on as_number
    EXPECT_EQ(doc->root().as_number().error(), tempest::json_error::type_mismatch);
}

//==============================================================================
// Template as<T>() Specialization Tests
//==============================================================================

/// @brief Verify as<T>() template specializations dispatch to corresponding conversion methods.
TEST(json_tests, template_as_specializations)
{
    // 1. Setup: Document with various typed fields
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{R"({
        "b": true,
        "i64": -9000000000,
        "u64": 18000000000,
        "i32": -123456,
        "u32": 123456,
        "i16": -1000,
        "u16": 1000,
        "i8": -42,
        "u8": 42,
        "dbl": 12.34,
        "flt": 5.67,
        "str": "sample_text",
        "arr": [1, 2],
        "obj": { "nested": true }
    })"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    auto root = doc->root();

    // 2. Act & Assert: Dispatch through as<T>()
    EXPECT_EQ(root["b"].as<bool>().value(), true);
    EXPECT_EQ(root["i64"].as<tempest::int64_t>().value(), -9000000000LL);
    EXPECT_EQ(root["u64"].as<tempest::uint64_t>().value(), 18000000000ULL);
    EXPECT_EQ(root["i32"].as<tempest::int32_t>().value(), -123456);
    EXPECT_EQ(root["u32"].as<tempest::uint32_t>().value(), 123456U);
    EXPECT_EQ(root["i16"].as<tempest::int16_t>().value(), -1000);
    EXPECT_EQ(root["u16"].as<tempest::uint16_t>().value(), 1000U);
    EXPECT_EQ(root["i8"].as<tempest::int8_t>().value(), -42);
    EXPECT_EQ(root["u8"].as<tempest::uint8_t>().value(), 42U);
    EXPECT_DOUBLE_EQ(root["dbl"].as<double>().value(), 12.34);
    EXPECT_FLOAT_EQ(root["flt"].as<float>().value(), 5.67F);
    EXPECT_EQ(root["str"].as<tempest::string_view>().value(), "sample_text");

    auto arr_res = root["arr"].as<tempest::json_array>();
    ASSERT_TRUE(arr_res.has_value());
    EXPECT_EQ(arr_res->size(), 2U);

    auto obj_res = root["obj"].as<tempest::json_object>();
    ASSERT_TRUE(obj_res.has_value());
    EXPECT_TRUE(obj_res->contains("nested"));
}

//==============================================================================
// Container Ergonomics & Truthiness Tests
//==============================================================================

/// @brief Verify contains, size, empty, and explicit operator bool semantics on json_value and containers.
TEST(json_tests, container_ergonomics_and_truthiness)
{
    // 1. Setup: Document with nested containers and empty elements
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{R"({
        "items": [10, 20, 30],
        "empty_arr": [],
        "config": { "alpha": 1 },
        "empty_obj": {},
        "flag": false
    })"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    // 2. Act & Assert: json_document truthiness
    EXPECT_TRUE(static_cast<bool>(*doc));
    auto empty_doc = tempest::json_document{};
    EXPECT_FALSE(static_cast<bool>(empty_doc));

    auto root = doc->root();

    // 3. Act & Assert: json_value size, empty, and contains
    EXPECT_EQ(root["items"].size(), 3U);
    EXPECT_FALSE(root["items"].empty());
    EXPECT_EQ(root["empty_arr"].size(), 0U);
    EXPECT_TRUE(root["empty_arr"].empty());

    EXPECT_EQ(root["config"].size(), 1U);
    EXPECT_FALSE(root["config"].empty());
    EXPECT_TRUE(root["config"].contains("alpha"));
    EXPECT_FALSE(root["config"].contains("beta"));

    EXPECT_EQ(root["empty_obj"].size(), 0U);
    EXPECT_TRUE(root["empty_obj"].empty());

    // Primitives return 0 for size and true for empty
    EXPECT_EQ(root["flag"].size(), 0U);
    EXPECT_TRUE(root["flag"].empty());
    EXPECT_FALSE(root["flag"].contains("key"));

    // 4. Act & Assert: json_object and json_array truthiness
    auto items_arr = root["items"].as_array().value();
    EXPECT_TRUE(static_cast<bool>(items_arr));
    auto empty_arr_obj = root["empty_arr"].as_array().value();
    EXPECT_TRUE(static_cast<bool>(empty_arr_obj));
    auto default_arr = tempest::json_array{};
    EXPECT_FALSE(static_cast<bool>(default_arr));

    auto cfg_obj = root["config"].as_object().value();
    EXPECT_TRUE(static_cast<bool>(cfg_obj));
    auto default_obj = tempest::json_object{};
    EXPECT_FALSE(static_cast<bool>(default_obj));

    // 5. Act & Assert: Iterating empty containers is safe and executes 0 times
    auto empty_arr_count = 0U;
    for ([[maybe_unused]] auto val : empty_arr_obj)
    {
        ++empty_arr_count;
    }
    EXPECT_EQ(empty_arr_count, 0U);

    auto empty_obj_count = 0U;
    for ([[maybe_unused]] auto [k, v] : root["empty_obj"].as_object().value())
    {
        ++empty_obj_count;
    }
    EXPECT_EQ(empty_obj_count, 0U);
}

//==============================================================================
// Escaped String Tests
//==============================================================================

/// @brief Verify unescaping of standard escape sequences and UTF-8 characters.
TEST(json_tests, string_escape_sequences)
{
    // 1. Setup: JSON string with special escapes: quotes, backslashes, tabs, newlines, and unicode
    auto alloc = tempest::system_allocator{};
    constexpr auto json_str = tempest::string_view{R"({
        "escapes": "line1\nline2\ttabbed\\path\"quoted\"",
        "unicode": "\u0048\u0065\u006c\u006c\u006f"
    })"};
    auto doc = tempest::json_document::from_string(json_str, alloc);
    ASSERT_TRUE(doc.has_value());

    auto root = doc->root();

    // 2. Act & Assert: Escapes correctly unescaped
    auto escapes_res = root["escapes"].as_string();
    ASSERT_TRUE(escapes_res.has_value());
    EXPECT_EQ(*escapes_res, "line1\nline2\ttabbed\\path\"quoted\"");

    // 3. Act & Assert: Unicode escape \u0048\u0065\u006c\u006c\u006f decodes to "Hello"
    auto unicode_res = root["unicode"].as_string();
    ASSERT_TRUE(unicode_res.has_value());
    EXPECT_EQ(*unicode_res, "Hello");
}
