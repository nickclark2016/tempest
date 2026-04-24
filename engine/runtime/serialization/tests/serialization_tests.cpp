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
