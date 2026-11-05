#include <gtest/gtest.h>

#include <tempest/serial.hpp>
#include <tempest/string.hpp>

TEST(binary_archive, write_float)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const float value = 3.14F;
    archiver.serialize(value);
    
    const auto deserialized_value = archiver.deserialize<float>();
    EXPECT_FLOAT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_double)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const double value = 3.14;
    archiver.serialize(value);
    
    const auto deserialized_value = archiver.deserialize<double>();
    EXPECT_DOUBLE_EQ(value, deserialized_value);
}

TEST(binary_archive, write_char)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const char value = 'A';
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<char>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_int8)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const int8_t value = -42;
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<int8_t>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_uint8)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const uint8_t value = 42;
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<uint8_t>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_int16)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const int16_t value = -12345;
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<int16_t>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_uint16)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const uint16_t value = 12345;
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<uint16_t>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_int32)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const int32_t value = -123456789;
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<int32_t>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_uint32)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const uint32_t value = 123456789;
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<uint32_t>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_int64)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const int64_t value = -1234567890123456789LL;
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<int64_t>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_uint64)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const uint64_t value = 1234567890123456789ULL;
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<uint64_t>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_string)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const tempest::string value = "Hello, World!";
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<tempest::string>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_wide_string)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const tempest::wstring value = L"Hello, World!";
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<tempest::wstring>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_empty_string)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const tempest::string value = "";
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<tempest::string>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_long_string)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    const tempest::string value(1000, 'A');
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<tempest::string>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_vector_of_ints)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    auto value =tempest::vector<int>{};
    value.push_back(1);
    value.push_back(2);
    value.push_back(3);
    value.push_back(4);
     
    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<tempest::vector<int>>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_vector_of_strings)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    tempest::vector<tempest::string> value;
    value.emplace_back("Hello");
    value.emplace_back("World");
    value.emplace_back("!");

    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<tempest::vector<tempest::string>>();
    EXPECT_EQ(value, deserialized_value);
}

TEST(binary_archive, write_flat_unordered_map_trivial_key_value)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    auto value = tempest::flat_unordered_map<int, int>{};

    // NOLINTBEGIN
    value.insert({1, 10});
    value.insert({2, 20});
    value.insert({3, 30});
    // NOLINTEND

    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<tempest::flat_unordered_map<int, int>>();
    EXPECT_EQ(value.size(), deserialized_value.size());
    for (const auto& [key, val] : value)
    {
        const auto iter = deserialized_value.find(key);
        EXPECT_NE(iter, deserialized_value.end());
        EXPECT_EQ(val, iter->second);
    }
}

TEST(binary_archive, write_flat_unordered_map_string_key_value)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    auto value = tempest::flat_unordered_map<tempest::string, tempest::string>{};

    value.insert({"Hello", "World"});
    value.insert({"Foo", "Bar"});
    value.insert({"Key", "Value"});

    archiver.serialize(value);

    const auto deserialized_value = archiver.deserialize<tempest::flat_unordered_map<tempest::string, tempest::string>>();
    EXPECT_EQ(value.size(), deserialized_value.size());
    for (const auto& [key, val] : value)
    {
        const auto iter = deserialized_value.find(key);
        EXPECT_NE(iter, deserialized_value.end());
        EXPECT_EQ(val, iter->second);
    }
}

//==============================================================================
// Binary Archive Safety & Overflow Guards
//==============================================================================

/// @brief Verifies that deserializing a primitive from an empty archive value-initializes
/// the output without crashing or triggering null pointer undefined behavior in memcpy.
TEST(binary_archive, empty_archive_deserialize_primitive)
{
    // Setup empty archive
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    // Act
    const auto val = archiver.deserialize<int32_t>();

    // Assert
    EXPECT_EQ(val, 0);
}

/// @brief Verifies that deserializing a vector from an empty archive returns an empty container
/// without attempting to memcpy from a null buffer.
TEST(binary_archive, empty_archive_deserialize_vector)
{
    // Setup empty archive
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    // Act
    const auto vec = archiver.deserialize<tempest::vector<int32_t>>();

    // Assert
    EXPECT_TRUE(vec.empty());
}

/// @brief Verifies that serializing and deserializing an empty vector preserves the empty state
/// without executing null-pointer memcpy.
TEST(binary_archive, serialize_and_deserialize_empty_vector)
{
    // Setup
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    const auto original = tempest::vector<int32_t>{};

    // Act
    archiver.serialize(original);
    const auto deserialized = archiver.deserialize<tempest::vector<int32_t>>();

    // Assert
    EXPECT_TRUE(deserialized.empty());
}

/// @brief Verifies that read requests with count wrapping size_t return an empty span
/// and do not advance the read offset.
TEST(binary_archive, read_integer_overflow_guard)
{
    // Setup archive with 16 bytes
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(int64_t{100});
    archiver.serialize(int64_t{200});
    const auto original_size = archive.written_size();

    // Act
    const auto overflow_span = archive.read(static_cast<size_t>(-1) - 8);

    // Assert
    EXPECT_TRUE(overflow_span.empty());
    EXPECT_EQ(overflow_span.data(), nullptr);
    EXPECT_EQ(archive.written_size(), original_size);
}

/// @brief Verifies that writing an empty span is a safe no-op that does not crash or allocate.
TEST(binary_archive, write_empty_span_safety)
{
    // Setup
    auto archive = tempest::serialization::binary_archive{};

    // Act
    archive.write(tempest::span<const tempest::byte>{});

    // Assert
    EXPECT_EQ(archive.written_size(), 0);
}

/// @brief Verifies that requesting more bytes than available returns an empty span
/// and does not advance the read offset.
TEST(binary_archive, truncated_read_preserves_offset)
{
    // Setup archive with 4 bytes
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint32_t{42});

    // Act 1: Request 8 bytes (exceeds buffer of 4 bytes)
    const auto truncated_span = archive.read(8);

    // Assert 1
    EXPECT_TRUE(truncated_span.empty());
    EXPECT_EQ(truncated_span.data(), nullptr);

    // Act 2: Valid read of 4 bytes succeeds
    const auto valid_span = archive.read(4);
    EXPECT_EQ(valid_span.size(), 4U);
    EXPECT_NE(valid_span.data(), nullptr);
    auto read_val = uint32_t{0};
    tempest::memcpy(&read_val, valid_span.data(), sizeof(uint32_t));
    EXPECT_EQ(read_val, 42U);
}

/// @brief Verifies that deserializing a primitive from a truncated buffer returns the default value.
TEST(binary_archive, truncated_primitive_deserialization)
{
    // Setup archive with 3 bytes (int64_t requires 8 bytes)
    auto archive = tempest::serialization::binary_archive{};
    const auto raw_bytes = tempest::array<tempest::byte, 3>{tempest::byte{1}, tempest::byte{2}, tempest::byte{3}};
    archive.write(tempest::span<const tempest::byte>{raw_bytes.data(), raw_bytes.size()});

    auto archiver = tempest::serialization::archiver{archive};

    // Act
    const auto val = archiver.deserialize<int64_t>();

    // Assert: Value-initialized default without crash
    EXPECT_EQ(val, 0);
}

/// @brief Verifies that deserializing a vector with insufficient payload returns an empty container.
TEST(binary_archive, truncated_vector_deserialization)
{
    // Setup archive claiming 10 elements but having only 1 element's payload
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint64_t{10});
    archiver.serialize(int32_t{99});

    // Act
    const auto vec = archiver.deserialize<tempest::vector<int32_t>>();

    // Assert: Returns empty container because byte count was truncated
    EXPECT_TRUE(vec.empty());
}

/// @brief Verifies that deserializing a string with insufficient character payload returns an empty string.
TEST(binary_archive, truncated_string_deserialization)
{
    // Setup archive claiming 50 chars but having only 5 bytes
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint64_t{50});
    const auto partial_chars = tempest::string_view{"Hello"};
    archive.write(tempest::span<const tempest::byte>{
        reinterpret_cast<const tempest::byte*>(partial_chars.data()), partial_chars.size()});

    // Act
    const auto str = archiver.deserialize<tempest::string>();

    // Assert
    EXPECT_TRUE(str.empty());
}

/// @brief Verifies that deserializing an optional with missing payload returns default value.
TEST(binary_archive, truncated_optional_deserialization)
{
    // Setup archive with has_value = true but no following data
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(true);

    // Act
    const auto opt = archiver.deserialize<tempest::optional<int32_t>>();

    // Assert: Optional is present with default-initialized payload (0)
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 0);
}

/// @brief Verifies that serializing and deserializing an empty string roundtrips cleanly.
TEST(binary_archive, empty_string_roundtrip)
{
    // Setup
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    const auto empty_str = tempest::string{};

    // Act
    archiver.serialize(empty_str);
    EXPECT_EQ(archive.written_size(), sizeof(uint64_t));
    const auto deserialized = archiver.deserialize<tempest::string>();

    // Assert
    EXPECT_TRUE(deserialized.empty());
}

/// @brief Verifies that serializing and deserializing an empty vector of non-trivial strings roundtrips cleanly.
TEST(binary_archive, empty_vector_of_strings_roundtrip)
{
    // Setup
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    const auto empty_vec = tempest::vector<tempest::string>{};

    // Act
    archiver.serialize(empty_vec);
    EXPECT_EQ(archive.written_size(), sizeof(uint64_t));
    const auto deserialized = archiver.deserialize<tempest::vector<tempest::string>>();

    // Assert
    EXPECT_TRUE(deserialized.empty());
}

/// @brief Verifies that a truncated non-trivial vector deserializes available items and defaults remainder.
TEST(binary_archive, complex_non_trivial_vector_incomplete_buffer)
{
    // Setup: count header is 4, but only 2 strings are serialized
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint64_t{4});
    archiver.serialize(tempest::string{"first"});
    archiver.serialize(tempest::string{"second"});

    // Act
    const auto vec = archiver.deserialize<tempest::vector<tempest::string>>();

    // Assert: 4 elements populated, first two intact, remaining two are empty strings
    ASSERT_EQ(vec.size(), 4U);
    EXPECT_EQ(vec[0], "first");
    EXPECT_EQ(vec[1], "second");
    EXPECT_EQ(vec[2], "");
    EXPECT_EQ(vec[3], "");
}

/// @brief Verifies that enormous count near SIZE_MAX is rejected for trivially-copyable vector.
TEST(binary_archive, enormous_count_vector_arithmetic_overflow)
{
    // Setup
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint64_t{static_cast<size_t>(-1)});

    // Act
    const auto vec = archiver.deserialize<tempest::vector<int32_t>>();

    // Assert
    EXPECT_TRUE(vec.empty());
}

/// @brief Verifies that count exceeding SIZE_MAX / sizeof(T) is rejected for vector.
TEST(binary_archive, enormous_count_vector_element_overflow)
{
    // Setup
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    const auto dangerous_count = (static_cast<size_t>(-1) / sizeof(int32_t)) + 1ULL;
    archiver.serialize(uint64_t{dangerous_count});

    // Act
    const auto vec = archiver.deserialize<tempest::vector<int32_t>>();

    // Assert
    EXPECT_TRUE(vec.empty());
}

/// @brief Verifies that count near SIZE_MAX is rejected for string.
TEST(binary_archive, enormous_count_string_overflow)
{
    // Setup
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint64_t{static_cast<size_t>(-1)});

    // Act
    const auto str = archiver.deserialize<tempest::string>();

    // Assert
    EXPECT_TRUE(str.empty());
}

/// @brief Verifies alternating write and read operations across primitives and containers.
TEST(binary_archive, alternating_writes_and_reads_exact_alignment)
{
    // Setup
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    // Act & Assert interleaved
    archiver.serialize(int32_t{12345});
    EXPECT_EQ(archiver.deserialize<int32_t>(), 12345);

    archiver.serialize(tempest::string{"streaming_test"});
    EXPECT_EQ(archiver.deserialize<tempest::string>(), "streaming_test");

    archiver.serialize(int64_t{9876543210LL});
    EXPECT_EQ(archiver.deserialize<int64_t>(), 9876543210LL);
}

/// @brief Verifies that flat_unordered_map deserialization from truncated buffer does not crash.
TEST(binary_archive, truncated_map_deserialization)
{
    // Setup: claims 3 entries, but contains only 1 entry
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint64_t{3});
    archiver.serialize(int32_t{10});
    archiver.serialize(int32_t{20});

    // Act
    const auto map = archiver.deserialize<tempest::flat_unordered_map<int32_t, int32_t>>();

    // Assert: Deserialized without memory corruption
    EXPECT_GE(map.size(), 1U);
    const auto it = map.find(10);
    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->second, 20);
}

/// @brief Verifies that an enormous count near SIZE_MAX for non-trivially-copyable vector
/// does not crash with a null-pointer dereference or unconstrained allocation.
TEST(binary_archive, enormous_count_vector_of_strings_vulnerability_probe)
{
    // Setup: write SIZE_MAX count for vector<string>
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint64_t{static_cast<size_t>(-1)});

    // Act
    const auto vec = archiver.deserialize<tempest::vector<tempest::string>>();

    // Assert
    EXPECT_TRUE(vec.empty());
}

/// @brief Verifies that the exact boundary count (SIZE_MAX / sizeof(T)) for complex non-trivially-copyable
/// vector does not crash or execute null-pointer dereference in reserve/push_back.
TEST(binary_archive, boundary_count_vector_of_strings_vulnerability_probe)
{
    // Setup: write (SIZE_MAX / sizeof(string)) count for vector<string>
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    const auto boundary_count = static_cast<size_t>(-1) / sizeof(tempest::string);
    archiver.serialize(uint64_t{boundary_count});

    // Act
    const auto vec = archiver.deserialize<tempest::vector<tempest::string>>();

    // Assert
    EXPECT_TRUE(vec.empty());
}

/// @brief Verifies that boundary minus one count for vector<string> does not crash or allocate.
TEST(binary_archive, boundary_minus_one_vector_of_strings_vulnerability_probe)
{
    // Setup: write (SIZE_MAX / sizeof(string)) - 1 count for vector<string>
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    const auto boundary_count = (static_cast<size_t>(-1) / sizeof(tempest::string)) - 1ULL;
    archiver.serialize(uint64_t{boundary_count});

    // Act
    const auto vec = archiver.deserialize<tempest::vector<tempest::string>>();

    // Assert
    EXPECT_TRUE(vec.empty());
}

/// @brief Verifies that boundary count for nested vectors does not crash or allocate.
TEST(binary_archive, boundary_count_nested_vector_vulnerability_probe)
{
    // Setup: write (SIZE_MAX / sizeof(vector<int>)) count for vector<vector<int>>
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    const auto boundary_count = static_cast<size_t>(-1) / sizeof(tempest::vector<int>);
    archiver.serialize(uint64_t{boundary_count});

    // Act
    const auto vec = archiver.deserialize<tempest::vector<tempest::vector<int>>>();

    // Assert
    EXPECT_TRUE(vec.empty());
}

/// @brief Verifies that boundary count for primitive vector<int> does not allocate or crash.
TEST(binary_archive, boundary_count_primitive_vector_probe)
{
    // Setup: write (SIZE_MAX / sizeof(int)) count for vector<int>
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    const auto boundary_count = static_cast<size_t>(-1) / sizeof(int);
    archiver.serialize(uint64_t{boundary_count});

    // Act
    const auto vec = archiver.deserialize<tempest::vector<int>>();

    // Assert
    EXPECT_TRUE(vec.empty());
}

/// @brief Verifies that boundary count for primitive vector<float> does not allocate or crash.
TEST(binary_archive, boundary_count_float_vector_probe)
{
    // Setup: write (SIZE_MAX / sizeof(float)) count for vector<float>
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    const auto boundary_count = static_cast<size_t>(-1) / sizeof(float);
    archiver.serialize(uint64_t{boundary_count});

    // Act
    const auto vec = archiver.deserialize<tempest::vector<float>>();

    // Assert
    EXPECT_TRUE(vec.empty());
}

/// @brief Verifies that an untrusted payload claiming 10,000 strings without payload does not inflate empty strings.
TEST(binary_archive, truncated_stream_complex_vector_inflation_probe)
{
    // Setup: write count of 10,000 strings, but no string payload
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint64_t{10000});

    // Act
    const auto vec = archiver.deserialize<tempest::vector<tempest::string>>();

    // Assert: should reject truncated payload rather than generating 10,000 empty strings
    EXPECT_TRUE(vec.empty());
}

/// @brief Verifies that read_offset and remaining_bytes accurately track archive read progress.
TEST(binary_archive, read_offset_and_remaining_bytes_tracking)
{
    // Setup: archive with written payload
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint32_t{42});
    archiver.serialize(uint64_t{100});

    // Assert: initial state before reading
    EXPECT_EQ(archive.written_size(), 12U);
    EXPECT_EQ(archive.read_offset(), 0U);
    EXPECT_EQ(archive.remaining_bytes(), 12U);

    // Act: read first item
    const auto first_value = archiver.deserialize<uint32_t>();
    EXPECT_EQ(first_value, 42U);

    // Assert: state after reading 4 bytes
    EXPECT_EQ(archive.read_offset(), 4U);
    EXPECT_EQ(archive.remaining_bytes(), 8U);

    // Act: read second item
    const auto second_value = archiver.deserialize<uint64_t>();
    EXPECT_EQ(second_value, 100ULL);

    // Assert: state after reading all bytes
    EXPECT_EQ(archive.read_offset(), 12U);
    EXPECT_EQ(archive.remaining_bytes(), 0U);
}

// -----------------------------------------------------------------------------
// Adversarial Stress Probes & Fuzzing Scenarios
// -----------------------------------------------------------------------------

/// @brief Verifies that deserializing all supported types from a zero-byte archive produces safe defaults.
TEST(binary_archive, truncated_stream_zero_byte_archive_all_types)
{
    // Setup: empty archive with zero bytes
    auto empty_archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{empty_archive};

    // Act & Assert primitives
    EXPECT_EQ(archiver.deserialize<int8_t>(), 0);
    EXPECT_EQ(archiver.deserialize<uint16_t>(), 0U);
    EXPECT_EQ(archiver.deserialize<int32_t>(), 0);
    EXPECT_EQ(archiver.deserialize<uint64_t>(), 0ULL);
    EXPECT_FLOAT_EQ(archiver.deserialize<float>(), 0.0F);
    EXPECT_DOUBLE_EQ(archiver.deserialize<double>(), 0.0);

    // Act & Assert containers
    const auto empty_string_result = archiver.deserialize<tempest::string>();
    EXPECT_TRUE(empty_string_result.empty());

    const auto empty_vector_primitive = archiver.deserialize<tempest::vector<int32_t>>();
    EXPECT_TRUE(empty_vector_primitive.empty());

    const auto empty_vector_complex = archiver.deserialize<tempest::vector<tempest::string>>();
    EXPECT_TRUE(empty_vector_complex.empty());

    const auto empty_nested_vector = archiver.deserialize<tempest::vector<tempest::vector<int32_t>>>();
    EXPECT_TRUE(empty_nested_vector.empty());

    const auto empty_map = archiver.deserialize<tempest::flat_unordered_map<int32_t, int32_t>>();
    EXPECT_TRUE(empty_map.empty());

    const auto empty_map_string = archiver.deserialize<tempest::flat_unordered_map<tempest::string, tempest::string>>();
    EXPECT_TRUE(empty_map_string.empty());

    // Act & Assert optionals
    const auto empty_optional_int = archiver.deserialize<tempest::optional<int32_t>>();
    EXPECT_FALSE(empty_optional_int.has_value());

    const auto empty_optional_string = archiver.deserialize<tempest::optional<tempest::string>>();
    EXPECT_FALSE(empty_optional_string.has_value());
}

/// @brief Verifies that archives with 1 to 7 bytes (insufficient for 8-byte uint64_t headers) reject gracefully.
TEST(binary_archive, truncated_stream_sub_header_partial_bytes)
{
    for (size_t byte_count = 1; byte_count < 8; ++byte_count)
    {
        // Setup: archive with exactly byte_count bytes of garbage
        auto archive = tempest::serialization::binary_archive{};
        auto raw_bytes = tempest::vector<tempest::byte>{};
        raw_bytes.resize(byte_count);
        archive.write(raw_bytes);

        auto archiver = tempest::serialization::archiver{archive};

        // Act & Assert: All container headers require at least 8 bytes
        const auto vec_result = archiver.deserialize<tempest::vector<int32_t>>();
        EXPECT_TRUE(vec_result.empty());

        const auto string_result = archiver.deserialize<tempest::string>();
        EXPECT_TRUE(string_result.empty());

        const auto map_result = archiver.deserialize<tempest::flat_unordered_map<int32_t, int32_t>>();
        EXPECT_TRUE(map_result.empty());
    }
}

/// @brief Verifies that boundary power-of-two and extreme uint64_t counts are rejected without memory corruption.
TEST(binary_archive, boundary_counts_power_of_two_and_extreme)
{
    const auto boundary_counts = tempest::array<uint64_t, 14>{
        1ULL << 63,
        (1ULL << 63) - 1ULL,
        (1ULL << 63) + 1ULL,
        1ULL << 32,
        (1ULL << 32) - 1ULL,
        (1ULL << 32) + 1ULL,
        static_cast<uint64_t>(-1),
        static_cast<uint64_t>(-2),
        static_cast<uint64_t>(-1) / sizeof(tempest::string),
        (static_cast<uint64_t>(-1) / sizeof(tempest::string)) - 1ULL,
        (static_cast<uint64_t>(-1) / sizeof(tempest::string)) + 1ULL,
        static_cast<uint64_t>(-1) / sizeof(int32_t),
        (static_cast<uint64_t>(-1) / sizeof(int32_t)) - 1ULL,
        (static_cast<uint64_t>(-1) / sizeof(int32_t)) + 1ULL,
    };

    for (const auto count : boundary_counts)
    {
        // Setup: write boundary count with 0 following payload bytes
        auto archive = tempest::serialization::binary_archive{};
        auto archiver = tempest::serialization::archiver{archive};
        archiver.serialize(count);

        // Act & Assert: Primitive vector rejection
        const auto primitive_vec = archiver.deserialize<tempest::vector<int32_t>>();
        EXPECT_TRUE(primitive_vec.empty());

        // Reset archive with same count for next deserializer
        archive = tempest::serialization::binary_archive{};
        archiver = tempest::serialization::archiver{archive};
        archiver.serialize(count);

        // Act & Assert: Complex vector rejection
        const auto complex_vec = archiver.deserialize<tempest::vector<tempest::string>>();
        EXPECT_TRUE(complex_vec.empty());

        // Reset archive with same count for string
        archive = tempest::serialization::binary_archive{};
        archiver = tempest::serialization::archiver{archive};
        archiver.serialize(count);

        // Act & Assert: String rejection
        const auto str_result = archiver.deserialize<tempest::string>();
        EXPECT_TRUE(str_result.empty());

        // Reset archive with same count for map
        archive = tempest::serialization::binary_archive{};
        archiver = tempest::serialization::archiver{archive};
        archiver.serialize(count);

        // Act & Assert: Map rejection
        const auto map_result = archiver.deserialize<tempest::flat_unordered_map<int32_t, int32_t>>();
        EXPECT_TRUE(map_result.empty());
    }
}

/// @brief Verifies that maliciously nested vectors with truncated or boundary inner payloads do not crash.
TEST(binary_archive, malicious_nested_vector_payloads)
{
    // Setup: outer vector claims 3 elements, but inner payload is immediately truncated
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint64_t{3});
    archiver.serialize(uint64_t{static_cast<size_t>(-1)});
    archiver.serialize(uint64_t{1000000ULL});
    archiver.serialize(uint64_t{0});

    // Act
    const auto nested_vec = archiver.deserialize<tempest::vector<tempest::vector<int32_t>>>();

    // Assert: Handled safely without access violations
    EXPECT_LE(nested_vec.size(), 3U);
    for (const auto& inner : nested_vec)
    {
        EXPECT_TRUE(inner.empty());
    }
}

/// @brief Verifies that extreme archive capacity reservation values do not crash or corrupt state.
TEST(binary_archive, reserve_extreme_values_safety)
{
    auto archive = tempest::serialization::binary_archive{};

    // Act & Assert: Reservation with unallocatable capacities handled gracefully
    archive.reserve(0);
    EXPECT_EQ(archive.written_size(), 0U);

    archive.reserve(1024);
    EXPECT_EQ(archive.written_size(), 0U);

    archive.reserve(static_cast<size_t>(-1));
    EXPECT_EQ(archive.written_size(), 0U);

    archive.reserve(1ULL << 63);
    EXPECT_EQ(archive.written_size(), 0U);
}

/// @brief Verifies repeated reads past the end of the archive do not corrupt offset or buffer.
TEST(binary_archive, read_past_end_repeated_calls)
{
    // Setup: 4-byte archive
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint32_t{0x12345678});

    // Act: read all 4 bytes
    const auto value = archiver.deserialize<uint32_t>();
    EXPECT_EQ(value, 0x12345678U);
    EXPECT_EQ(archive.read_offset(), 4U);
    EXPECT_EQ(archive.remaining_bytes(), 0U);

    // Act: Repeated over-reads
    const auto over_span1 = archive.read(1);
    EXPECT_TRUE(over_span1.empty());
    EXPECT_EQ(archive.read_offset(), 4U);
    EXPECT_EQ(archive.remaining_bytes(), 0U);

    const auto over_span2 = archive.read(100);
    EXPECT_TRUE(over_span2.empty());
    EXPECT_EQ(archive.read_offset(), 4U);
    EXPECT_EQ(archive.remaining_bytes(), 0U);

    const auto over_span3 = archive.read(static_cast<size_t>(-1));
    EXPECT_TRUE(over_span3.empty());
    EXPECT_EQ(archive.read_offset(), 4U);
    EXPECT_EQ(archive.remaining_bytes(), 0U);

    const auto zero_span = archive.read(0);
    EXPECT_TRUE(zero_span.empty());
    EXPECT_EQ(archive.read_offset(), 4U);
    EXPECT_EQ(archive.remaining_bytes(), 0U);
}

/// @brief Verifies that alternating writes, large reallocations, and reads preserve buffer integrity.
TEST(binary_archive, alternating_write_read_large_reallocation_stress)
{
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};

    // Setup: write small string
    archiver.serialize(tempest::string{"initial_data"});

    // Act: read small string
    const auto first_read = archiver.deserialize<tempest::string>();
    EXPECT_EQ(first_read, "initial_data");
    EXPECT_EQ(archive.remaining_bytes(), 0U);

    // Act: write large payload triggering buffer expansion
    auto large_vector = tempest::vector<int32_t>{};
    for (int32_t element_val = 0; element_val < 5000; ++element_val)
    {
        large_vector.push_back(element_val);
    }
    archiver.serialize(large_vector);

    // Assert: remaining bytes match large vector
    EXPECT_GT(archive.remaining_bytes(), 5000U * sizeof(int32_t));

    // Act: deserialize large vector
    const auto read_large_vector = archiver.deserialize<tempest::vector<int32_t>>();
    EXPECT_EQ(read_large_vector.size(), 5000U);
    EXPECT_EQ(read_large_vector[0], 0);
    EXPECT_EQ(read_large_vector[4999], 4999);
    EXPECT_EQ(archive.remaining_bytes(), 0U);
}

/// @brief Verifies that map deserialization with matched remaining bytes but truncated elements does not crash.
TEST(binary_archive, flat_unordered_map_adversarial_truncated_payload)
{
    // Setup: 50 bytes of zeros. Count claims 50 entries (so count <= remaining_bytes passes).
    // But each entry is a pair of strings requiring at least 16 bytes.
    auto archive = tempest::serialization::binary_archive{};
    auto archiver = tempest::serialization::archiver{archive};
    archiver.serialize(uint64_t{50});

    auto zero_padding = tempest::vector<tempest::byte>{};
    zero_padding.resize(50);
    archive.write(zero_padding);

    // Act: Deserializing map with complex key/value
    const auto map = archiver.deserialize<tempest::flat_unordered_map<tempest::string, tempest::string>>();

    // Assert: Safely handled without memory corruption or excessive iterations
    EXPECT_LE(map.size(), 50U);
}
