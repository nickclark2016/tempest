#include <tempest/format.hpp>

#include <gtest/gtest.h>
#include <tempest/array.hpp>
#include <tempest/guid.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

//=============================================================================
// Positional & Escape Tests
//=============================================================================

/// @brief Tests basic string formatting and escaped braces `{{` and `}}`.
TEST(format_test, escaped_braces)
{
    // 1. Setup & Act
    auto s1 = tempest::format("Hello {{world}}");
    auto s2 = tempest::format("Braces: {{{}}}", 42);

    // 2. Assert
    EXPECT_EQ(s1, "Hello {world}");
    EXPECT_EQ(s2, "Braces: {42}");
}

/// @brief Tests automatic positional placeholders `{}` in sequence.
TEST(format_test, automatic_indexing)
{
    // 1. Setup & Act
    auto res = tempest::format("Opening profiler Web UI at http://{}:{}", "localhost", 8080);

    // 2. Assert
    EXPECT_EQ(res, "Opening profiler Web UI at http://localhost:8080");
}

/// @brief Tests explicit positional placeholders `{0}`, `{1}`, and reordering.
TEST(format_test, explicit_indexing)
{
    // 1. Setup & Act
    auto res = tempest::format("{1} and {0}", "first", "second");

    // 2. Assert
    EXPECT_EQ(res, "second and first");
}

//=============================================================================
// Compile-Time & Runtime Error Tests
//=============================================================================

struct unformattable_type
{
    int a;
};

/// @brief Verifies that types without a formatter specialization fail the formattable concept at compile-time.
TEST(format_test, unformattable_type_fails_concept)
{
    // 1. Assert: formattable concept is false for unformattable_type
    static_assert(!tempest::formattable<unformattable_type>);
    EXPECT_FALSE((tempest::formattable<unformattable_type>));

    // 2. Assert: formattable concept is true for fundamental types
    static_assert(tempest::formattable<int>);
    static_assert(tempest::formattable<double>);
    static_assert(tempest::formattable<tempest::string>);
    EXPECT_TRUE((tempest::formattable<int>));
}

/// @brief Verifies that mismatched compile-time format strings fail concept constraints at compile-time.
TEST(format_test, mismatched_compile_time_args_fails_concept)
{
    // 1. Assert: Too few arguments fails constraint
    static_assert(!tempest::valid_format_literal<"{} {}", int>);
    EXPECT_FALSE((tempest::valid_format_literal<"{} {}", int>));

    // 2. Assert: Too many arguments fails constraint
    static_assert(!tempest::valid_format_literal<"{}", int, int>);
    EXPECT_FALSE((tempest::valid_format_literal<"{}", int, int>));

    // 3. Assert: Out of bounds explicit index fails constraint
    static_assert(!tempest::valid_format_literal<"{2}", int, int>);
    EXPECT_FALSE((tempest::valid_format_literal<"{2}", int, int>));

    // 4. Assert: Unclosed brace fails constraint
    static_assert(!tempest::valid_format_literal<"{", int>);
    EXPECT_FALSE((tempest::valid_format_literal<"{", int>));

    // 5. Assert: Correct arguments succeeds constraint
    static_assert(tempest::valid_format_literal<"{}", int>);
    EXPECT_TRUE((tempest::valid_format_literal<"{}", int>));

    static_assert(tempest::valid_format_literal<"{1} {0}", int, double>);
    EXPECT_TRUE((tempest::valid_format_literal<"{1} {0}", int, double>));
}

/// @brief Verifies that runtime format strings detect argument mismatches and syntax errors gracefully.
TEST(format_test, runtime_format_error_detection)
{
    // 1. Act: Too few arguments at runtime
    auto err1 = tempest::format(tempest::runtime_format("{} {}"), 42);

    // 2. Assert: returns argument_index_out_of_range
    EXPECT_FALSE(err1.has_value());
    EXPECT_EQ(err1.error(), tempest::format_error::argument_index_out_of_range);

    // 3. Act: Unclosed brace at runtime
    auto err2 = tempest::format(tempest::runtime_format("Hello {"), 42);

    // 4. Assert: returns unmatched_brace
    EXPECT_FALSE(err2.has_value());
    EXPECT_EQ(err2.error(), tempest::format_error::unmatched_brace);

    // 5. Act: Valid runtime format
    auto ok = tempest::format(tempest::runtime_format("Hello {}"), 42);
    EXPECT_TRUE(ok.has_value());
    EXPECT_EQ(*ok, "Hello 42");
}

//=============================================================================
// Integer Formatting Tests
//=============================================================================

/// @brief Tests integer formatting across bases, signs, widths, and padding.
TEST(format_test, integer_formatting)
{
    // 1. Act & Assert: Basic decimal and INT64_MIN
    EXPECT_EQ(tempest::format("{}", 0), "0");
    EXPECT_EQ(tempest::format("{}", -42), "-42");
    EXPECT_EQ(tempest::format("{}", tempest::numeric_limits<int64_t>::min()), "-9223372036854775808");

    // 2. Act & Assert: Hexadecimal lower and upper
    EXPECT_EQ(tempest::format("{:x}", 42), "2a");
    EXPECT_EQ(tempest::format("{:X}", 42), "2A");
    EXPECT_EQ(tempest::format("{:#x}", 42), "0x2a");
    EXPECT_EQ(tempest::format("{:#X}", 42), "0X2A");

    // 3. Act & Assert: Zero padding and width
    EXPECT_EQ(tempest::format("{:04d}", 42), "0042");
    EXPECT_EQ(tempest::format("{:#010x}", 42), "0x0000002a");
    EXPECT_EQ(tempest::format("{:#010x}", -42), "-0x000002a");
    EXPECT_EQ(tempest::format("{:>6}", 42), "    42");
    EXPECT_EQ(tempest::format("{:<6}", 42), "42    ");
    EXPECT_EQ(tempest::format("{:^6}", 42), "  42  ");

    // 4. Act & Assert: Binary
    EXPECT_EQ(tempest::format("{:b}", 5), "101");
    EXPECT_EQ(tempest::format("{:#b}", 5), "0b101");
}

//=============================================================================
// Floating-Point Formatting Tests
//=============================================================================

/// @brief Tests floating-point formatting: fixed, scientific, general, NaN, Inf, and -0.0.
TEST(format_test, float_formatting)
{
    // 1. Act & Assert: Fixed precision
    EXPECT_EQ(tempest::format("{:.2f}", 3.14159), "3.14");
    EXPECT_EQ(tempest::format("{:.0f}", 3.7), "4");

    // 2. Act & Assert: Scientific notation
    EXPECT_EQ(tempest::format("{:.2e}", 12345.0), "1.23e+04");

    // 3. Act & Assert: General format
    EXPECT_EQ(tempest::format("{}", 1.25), "1.25");
    EXPECT_EQ(tempest::format("{}", 100.0), "100");

    // 4. Act & Assert: Sign formatting
    EXPECT_EQ(tempest::format("{:+f}", 0.0), "+0");
    EXPECT_EQ(tempest::format("{:+.1f}", -0.0), "-0.0");
    EXPECT_EQ(tempest::format("{:+f}", 3.5), "+3.5");

    // 5. Act & Assert: Special values
    auto nan_val = tempest::numeric_limits<double>::quiet_NaN();
    auto inf_val = tempest::numeric_limits<double>::infinity();
    EXPECT_EQ(tempest::format("{}", nan_val), "nan");
    EXPECT_EQ(tempest::format("{}", inf_val), "inf");
    EXPECT_EQ(tempest::format("{}", -inf_val), "-inf");

    // 6. Act & Assert: Negative zero
    EXPECT_EQ(tempest::format("{:.1f}", -0.0), "-0.0");
}

//=============================================================================
// Strings, GUIDs, and Custom Type Tests
//=============================================================================

struct player_entity
{
    uint32_t id;
    tempest::string name;
};

template <>
struct tempest::formatter<player_entity>
{
    template <typename FormatContext>
    static auto format(const player_entity& val, FormatContext& ctx) -> void
    {
        ctx.write("Player(id=");
        char buf[32];
        auto res = tempest::to_chars(buf, buf + sizeof(buf), val.id);
        ctx.write(tempest::string_view(buf, res.ptr - buf));
        ctx.write(", name=");
        ctx.write(tempest::string_view(val.name));
        ctx.write(")");
    }
};

/// @brief Tests string types, GUID formatting, and user custom types.
TEST(format_test, strings_guids_and_custom_types)
{
    // 1. Act & Assert: String and string_view
    tempest::string s = "world";
    tempest::string_view sv = "hello";
    EXPECT_EQ(tempest::format("{} {}", sv, s), "hello world");

    // 2. Act & Assert: GUID formatting
    auto id = tempest::guid{};
    id.data[0] = static_cast<tempest::byte>(0x12);
    id.data[1] = static_cast<tempest::byte>(0x34);
    id.data[2] = static_cast<tempest::byte>(0x56);
    id.data[3] = static_cast<tempest::byte>(0x78);
    id.data[4] = static_cast<tempest::byte>(0x9a);
    id.data[5] = static_cast<tempest::byte>(0xbc);
    id.data[6] = static_cast<tempest::byte>(0xde);
    id.data[7] = static_cast<tempest::byte>(0xf0);
    id.data[8] = static_cast<tempest::byte>(0x11);
    id.data[9] = static_cast<tempest::byte>(0x22);
    id.data[10] = static_cast<tempest::byte>(0x33);
    id.data[11] = static_cast<tempest::byte>(0x44);
    id.data[12] = static_cast<tempest::byte>(0x55);
    id.data[13] = static_cast<tempest::byte>(0x66);
    id.data[14] = static_cast<tempest::byte>(0x77);
    id.data[15] = static_cast<tempest::byte>(0x88);

    EXPECT_EQ(tempest::format("{}", id), "12345678-9abc-def0-1122-334455667788");
    EXPECT_EQ(tempest::format("{:X}", id), "12345678-9ABC-DEF0-1122-334455667788");

    // 3. Act & Assert: Custom type
    player_entity p{.id = 7, .name = "Hero"};
    EXPECT_EQ(tempest::format("Entity: {}", p), "Entity: Player(id=7, name=Hero)");
}

//=============================================================================
// Container / Range Tests
//=============================================================================

/// @brief Tests container and range formatting for vector, array, and span.
TEST(format_test, container_range_formatting)
{
    // 1. Act & Assert: tempest::vector<int>
    tempest::vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    EXPECT_EQ(tempest::format("{}", v), "[1, 2, 3]");

    // 2. Act & Assert: tempest::array<float, 3>
    tempest::array<float, 3> arr{1.5F, 2.5F, 3.5F};
    EXPECT_EQ(tempest::format("{}", arr), "[1.5, 2.5, 3.5]");

    // 3. Act & Assert: tempest::span<int>
    tempest::span<int> sp(v.data(), v.size());
    EXPECT_EQ(tempest::format("{}", sp), "[1, 2, 3]");
}

//=============================================================================
// Buffer Bounds & format_to Tests
//=============================================================================

/// @brief Tests format_to and format_to_buffer boundary safety, truncation, and null termination.
TEST(format_test, buffer_safety_and_truncation)
{
    // 1. Setup: Buffer with size 10
    char buf[10];

    // 2. Act: format fitting string
    auto written = tempest::format_to_buffer(buf, "Num: {}", 42);

    // 3. Assert: exact fit with null terminator
    EXPECT_EQ(written, 7U);
    EXPECT_EQ(tempest::string_view(buf, written), "Num: 42");

    // 4. Act: format exceeding string
    written = tempest::format_to_buffer(buf, "Very long string: {}", 123456789);

    // 5. Assert: truncated to 9 characters + null terminator
    EXPECT_EQ(written, 9U);
    EXPECT_EQ(buf[9], '\0');
    EXPECT_EQ(tempest::string_view(buf, written), "Very long");

    // 6. Act: format_to with pointer
    char out_buf[32];
    auto* end_ptr = tempest::format_to(out_buf, "Point({}, {})", 10, 20);

    // 7. Assert: format_to wrote exact characters
    EXPECT_EQ(tempest::string_view(out_buf, end_ptr - out_buf), "Point(10, 20)");
}

/// @brief Tests formatting long outputs that exceed the 256-byte stack buffer and trigger dynamic resizing.
TEST(format_test, large_buffer_growth)
{
    // 1. Setup: Construct a vector with 100 elements requiring > 400 characters
    tempest::vector<int> large_vec;
    for (int elem_idx = 0; elem_idx < 100; ++elem_idx)
    {
        large_vec.push_back(elem_idx);
    }

    // 2. Act: Format into string
    auto formatted_str = tempest::format("Large range: {}", large_vec);

    // 3. Assert: Exceeds stack buffer (256 bytes) and content matches
    EXPECT_GT(formatted_str.size(), 256U);
    EXPECT_EQ(tempest::substr(tempest::string_view(formatted_str), 0, 20), "Large range: [0, 1, ");
    EXPECT_EQ(tempest::substr(tempest::string_view(formatted_str), formatted_str.size() - 7, 7), "98, 99]");
}
