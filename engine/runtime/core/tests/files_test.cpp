#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>

#include <gtest/gtest.h>

namespace fs = tempest::filesystem;

namespace
{
    auto make_test_path(const char* filename) -> fs::path
    {
        return fs::temp_directory_path() / filename;
    }
} // namespace

//=============================================================================
// Section: File Binary and Text Roundtrip Tests
//=============================================================================

/// @brief Verifies lossless roundtrip binary write and read using write_file_from_bytes and read_file_to_vector.
TEST(files_test, binary_roundtrip)
{
    // 1. Setup
    auto test_path = make_test_path("tempest_test_bin_roundtrip.dat");
    auto original_data = tempest::vector<tempest::byte>{};
    for (size_t i = 0; i < 256; ++i)
    {
        original_data.push_back(static_cast<tempest::byte>(i));
    }

    // 2. Act
    auto write_res = tempest::write_file_from_bytes(test_path, original_data);
    auto read_res = tempest::read_file_to_vector(test_path);

    // 3. Assert
    EXPECT_TRUE(write_res.has_value());
    ASSERT_TRUE(read_res.has_value());
    EXPECT_EQ(read_res->size(), original_data.size());
    EXPECT_EQ(*read_res, original_data);

    // Cleanup
    fs::remove(test_path);
}

/// @brief Verifies lossless text write and read using write_file_from_bytes and read_file_to_string.
TEST(files_test, text_roundtrip)
{
    // 1. Setup
    auto test_path = make_test_path("tempest_test_text_roundtrip.txt");
    auto text_content = tempest::string{R"(Hello World!
Line 2 with special chars: 12345!@#$%^&*()_+
)"};
    auto byte_span = tempest::span<const tempest::byte>{reinterpret_cast<const tempest::byte*>(text_content.data()),
                                                        text_content.size()};

    // 2. Act
    auto write_res = tempest::write_file_from_bytes(test_path, byte_span);
    auto read_res = tempest::read_file_to_string(test_path);

    // 3. Assert
    EXPECT_TRUE(write_res.has_value());
    ASSERT_TRUE(read_res.has_value());
    EXPECT_EQ(*read_res, text_content);

    // Cleanup
    fs::remove(test_path);
}

//=============================================================================
// Section: Edge Cases and Error Handling
//=============================================================================

/// @brief Verifies writing and reading an empty 0-byte file returns an engaged empty container, not an error.
TEST(files_test, empty_file_handling)
{
    // 1. Setup
    auto test_path = make_test_path("tempest_test_empty.bin");

    // 2. Act
    auto write_res = tempest::write_file_from_bytes(test_path, tempest::span<const tempest::byte>{});
    auto read_vec = tempest::read_file_to_vector(test_path);
    auto read_str = tempest::read_file_to_string(test_path);

    // 3. Assert
    EXPECT_TRUE(write_res.has_value());
    ASSERT_TRUE(read_vec.has_value());
    EXPECT_TRUE(read_vec->empty());
    ASSERT_TRUE(read_str.has_value());
    EXPECT_TRUE(read_str->empty());

    // Cleanup
    fs::remove(test_path);
}

/// @brief Verifies reading a non-existent file path returns not_found error.
TEST(files_test, non_existent_file_returns_not_found)
{
    // 1. Setup
    auto test_path = make_test_path("tempest_non_existent_file_12345.xyz");

    // 2. Act
    auto read_vec = tempest::read_file_to_vector(test_path);
    auto read_str = tempest::read_file_to_string(test_path);

    // 3. Assert
    ASSERT_FALSE(read_vec.has_value());
    EXPECT_EQ(read_vec.error(), tempest::file_error::not_found);
    ASSERT_FALSE(read_str.has_value());
    EXPECT_EQ(read_str.error(), tempest::file_error::not_found);
}

/// @brief Verifies that overwriting an existing file truncates older longer content cleanly.
TEST(files_test, file_truncation_on_overwrite)
{
    // 1. Setup
    auto test_path = make_test_path("tempest_test_truncate.bin");
    auto initial_large = tempest::vector<tempest::byte>(4096, static_cast<tempest::byte>('A'));
    auto smaller_payload = tempest::vector<tempest::byte>(16, static_cast<tempest::byte>('B'));

    // 2. Act
    auto write_first = tempest::write_file_from_bytes(test_path, initial_large);
    EXPECT_TRUE(write_first.has_value());

    auto write_second = tempest::write_file_from_bytes(test_path, smaller_payload);
    EXPECT_TRUE(write_second.has_value());

    auto read_res = tempest::read_file_to_vector(test_path);

    // 3. Assert
    ASSERT_TRUE(read_res.has_value());
    EXPECT_EQ(read_res->size(), smaller_payload.size());
    EXPECT_EQ(*read_res, smaller_payload);

    // Cleanup
    fs::remove(test_path);
}

/// @brief Verifies writing to an invalid path fails gracefully returning an error.
TEST(files_test, invalid_path_returns_error)
{
    // 1. Setup: non-existent nested directory path without creating parents
    auto test_path = fs::temp_directory_path() / "non_existent_dir_xyz_123" / "sub" / "file.bin";
    auto data = tempest::vector<tempest::byte>(8, static_cast<tempest::byte>(1));

    // 2. Act
    auto write_res = tempest::write_file_from_bytes(test_path, data);

    // 3. Assert
    EXPECT_FALSE(write_res.has_value());
}

/// @brief Verifies writing and reading large buffers across multiple page boundaries.
TEST(files_test, large_payload_roundtrip)
{
    // 1. Setup: 1 MB buffer with repeating pattern
    auto test_path = make_test_path("tempest_test_large.bin");
    auto payload_size = size_t{1024 * 1024};
    auto large_data = tempest::vector<tempest::byte>{};
    large_data.resize(payload_size);
    for (size_t i = 0; i < payload_size; ++i)
    {
        large_data[i] = static_cast<tempest::byte>(i & 0xFF);
    }

    // 2. Act
    auto write_res = tempest::write_file_from_bytes(test_path, large_data);
    auto read_res = tempest::read_file_to_vector(test_path);

    // 3. Assert
    EXPECT_TRUE(write_res.has_value());
    ASSERT_TRUE(read_res.has_value());
    EXPECT_EQ(read_res->size(), payload_size);
    EXPECT_EQ(*read_res, large_data);

    // Cleanup
    fs::remove(test_path);
}

/// @brief Verifies that calling read_file_to_vector and read_file_to_string on a directory fails with invalid_argument.
TEST(files_test, reading_directory_returns_error)
{
    // 1. Setup
    auto dir_path = fs::temp_directory_path();

    // 2. Act
    auto read_vec = tempest::read_file_to_vector(dir_path);
    auto read_str = tempest::read_file_to_string(dir_path);

    // 3. Assert
    ASSERT_FALSE(read_vec.has_value());
    EXPECT_EQ(read_vec.error(), tempest::file_error::invalid_argument);
    ASSERT_FALSE(read_str.has_value());
    EXPECT_EQ(read_str.error(), tempest::file_error::invalid_argument);
}

/// @brief Verifies writing and reading files with non-ASCII UTF-8 characters in their filenames.
TEST(files_test, unicode_filename_roundtrip)
{
    // 1. Setup: non-ASCII UTF-8 path
    auto test_path = make_test_path("tempest_test_日本語_файл.txt");
    auto text_content = tempest::string{"UTF-8 encoded payload: \xc3\xa9\xc3\xa0\xc3\xb1\xe6\x97\xa5\xd1\x84"};
    auto byte_span = tempest::span<const tempest::byte>{
        reinterpret_cast<const tempest::byte*>(text_content.data()), text_content.size()};

    // 2. Act
    auto write_res = tempest::write_file_from_bytes(test_path, byte_span);
    auto read_res = tempest::read_file_to_string(test_path);

    // 3. Assert
    EXPECT_TRUE(write_res.has_value());
    ASSERT_TRUE(read_res.has_value());
    EXPECT_EQ(*read_res, text_content);

    // Cleanup
    fs::remove(test_path);
}

/// @brief Verifies temp_directory_path returns a non-empty, existing directory.
TEST(files_test, temp_directory_path_validity)
{
    // 1. Act
    auto temp_dir = fs::temp_directory_path();

    // 2. Assert
    EXPECT_FALSE(temp_dir.empty());
    EXPECT_TRUE(fs::exists(temp_dir));
}

/// @brief Verifies that remove returns false when deleting a non-existent file.
TEST(files_test, remove_non_existent_file_returns_false)
{
    // 1. Setup
    auto non_existent = make_test_path("tempest_definitely_does_not_exist_98765.tmp");

    // 2. Act & Assert
    EXPECT_FALSE(fs::remove(non_existent));
}
