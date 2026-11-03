#include <tempest/shared_library.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

// ============================================================================
// Shared Library Loading and Symbol Resolution Safety Tests
// ============================================================================

/// @brief Verifies that loading a non-existent library fails cleanly with expected error.
TEST(tempest_shared_library, load_nonexistent_library)
{
    // Setup & Act: Attempt to load an invalid library
    const auto lib_result = tempest::shared_library::load("tempest_nonexistent_library_9999.dll");

    // Assert: Fails cleanly
    EXPECT_FALSE(lib_result.has_value());
#if defined(TEMPEST_PLATFORM_WINDOWS)
    EXPECT_EQ(lib_result.error(), tempest::shared_library::load_error::missing_dependency);
#else
    EXPECT_EQ(lib_result.error(), tempest::shared_library::load_error::file_not_found);
#endif
}

/// @brief Verifies that function lookup with a non-null-terminated string_view succeeds without buffer overrun.
TEST(tempest_shared_library, non_null_terminated_string_view_lookup)
{
#if defined(TEMPEST_PLATFORM_WINDOWS)
    // Setup: Load kernel32.dll on Windows
    auto lib_result = tempest::shared_library::load("kernel32.dll");
    ASSERT_TRUE(lib_result.has_value());
    const auto& lib = *lib_result;

    // Construct a non-null-terminated string_view slice
    const char buffer[] = "GetCurrentProcessIdGARBAGE_TRAILING_BYTES_WITHOUT_NULL";
    const auto sliced_view = tempest::string_view{buffer, 19};
    ASSERT_NE(buffer[19], '\0');

    // Act: Resolve function handle using sliced view
    const auto fn_result = lib.get_function_handle<uint32_t>(sliced_view);

    // Assert: Slices are properly null-terminated and resolve correctly
    ASSERT_TRUE(fn_result.has_value());
    const auto fn = *fn_result;
    EXPECT_EQ(fn(), GetCurrentProcessId());
#endif
}

/// @brief Verifies that empty and long symbol names fail safely without memory corruption.
TEST(tempest_shared_library, edge_case_symbol_names)
{
#if defined(TEMPEST_PLATFORM_WINDOWS)
    auto lib_result = tempest::shared_library::load("kernel32.dll");
    ASSERT_TRUE(lib_result.has_value());
    const auto& lib = *lib_result;

    // Test: Empty string view
    const auto empty_view = tempest::string_view{};
    const auto empty_res = lib.get_function_handle<void>(empty_view);
    EXPECT_FALSE(empty_res.has_value());
    EXPECT_EQ(empty_res.error(), tempest::shared_library::function_error::function_not_found);

    // Test: Exceptionally long symbol name (> 256 bytes) falling back to heap string
    auto long_name = tempest::string(300, 'x');
    const auto long_res = lib.get_function_handle<void>(tempest::string_view{long_name.data(), long_name.size()});
    EXPECT_FALSE(long_res.has_value());
    EXPECT_EQ(long_res.error(), tempest::shared_library::function_error::function_not_found);
#endif
}

/// @brief Adversarially stress test string_view buffer boundaries around stack_symbol_capacity (256).
TEST(tempest_shared_library, adversarial_symbol_length_boundaries)
{
#if defined(TEMPEST_PLATFORM_WINDOWS)
    auto lib_result = tempest::shared_library::load("kernel32.dll");
    ASSERT_TRUE(lib_result.has_value());
    const auto& lib = *lib_result;

    // Setup: Construct large non-null-terminated buffer with filler bytes
    constexpr auto buffer_size = 4096;
    auto filler_buffer = tempest::vector<char>(buffer_size, 'Z');

    // Test: Substring slice of length 254 (fits in stack with null terminator)
    const auto slice_254 = tempest::string_view{filler_buffer.data(), 254};
    const auto res_254 = lib.get_function_handle<void>(slice_254);
    EXPECT_FALSE(res_254.has_value());
    EXPECT_EQ(res_254.error(), tempest::shared_library::function_error::function_not_found);

    // Test: Substring slice of length 255 (exact maximum capacity of inplace_vector: 255 chars + null byte = 256)
    const auto slice_255 = tempest::string_view{filler_buffer.data(), 255};
    const auto res_255 = lib.get_function_handle<void>(slice_255);
    EXPECT_FALSE(res_255.has_value());
    EXPECT_EQ(res_255.error(), tempest::shared_library::function_error::function_not_found);

    // Test: Substring slice of length 256 (exact threshold triggering heap fallback)
    const auto slice_256 = tempest::string_view{filler_buffer.data(), 256};
    const auto res_256 = lib.get_function_handle<void>(slice_256);
    EXPECT_FALSE(res_256.has_value());
    EXPECT_EQ(res_256.error(), tempest::shared_library::function_error::function_not_found);

    // Test: Substring slice of length 257 (heap fallback)
    const auto slice_257 = tempest::string_view{filler_buffer.data(), 257};
    const auto res_257 = lib.get_function_handle<void>(slice_257);
    EXPECT_FALSE(res_257.has_value());
    EXPECT_EQ(res_257.error(), tempest::shared_library::function_error::function_not_found);

    // Test: Large symbol slice of length 1024 (heap fallback)
    const auto slice_1024 = tempest::string_view{filler_buffer.data(), 1024};
    const auto res_1024 = lib.get_function_handle<void>(slice_1024);
    EXPECT_FALSE(res_1024.has_value());
    EXPECT_EQ(res_1024.error(), tempest::shared_library::function_error::function_not_found);

    // Test: Exceptionally large slice of 4096 bytes without null terminator
    const auto slice_4096 = tempest::string_view{filler_buffer.data(), buffer_size};
    const auto res_4096 = lib.get_function_handle<void>(slice_4096);
    EXPECT_FALSE(res_4096.has_value());
    EXPECT_EQ(res_4096.error(), tempest::shared_library::function_error::function_not_found);
#endif
}

/// @brief Adversarially verify valid symbol resolution embedded in large non-null-terminated buffers.
TEST(tempest_shared_library, adversarial_sliced_valid_symbol_with_heap_and_embedded_null)
{
#if defined(TEMPEST_PLATFORM_WINDOWS)
    auto lib_result = tempest::shared_library::load("kernel32.dll");
    ASSERT_TRUE(lib_result.has_value());
    auto lib = tempest::move(*lib_result);

    // Test: Valid symbol sliced from a buffer where trailing bytes are non-null
    const char long_buffer[] = "GetCurrentProcessIdTRAILING_GARBAGE_THAT_WOULD_CORRUPT_LOOKUP";
    const auto valid_slice = tempest::string_view{long_buffer, 19};
    ASSERT_NE(long_buffer[19], '\0');

    const auto fn_slice_result = lib.get_function_handle<uint32_t>(valid_slice);
    ASSERT_TRUE(fn_slice_result.has_value());
    const auto fn_slice = *fn_slice_result;
    EXPECT_EQ(fn_slice(), GetCurrentProcessId());

    // Test: Valid symbol with embedded null byte in string_view
    const char embedded_null_buffer[] = "GetCurrentProcessId\0TrailingIgnoredData";
    const auto embedded_null_view = tempest::string_view{embedded_null_buffer, sizeof(embedded_null_buffer) - 1};
    const auto fn_null_result = lib.get_function_handle<uint32_t>(embedded_null_view);
    ASSERT_TRUE(fn_null_result.has_value());
    const auto fn_null = *fn_null_result;
    EXPECT_EQ(fn_null(), GetCurrentProcessId());

    // Test: Move assignment and invocation from moved-to shared_library
    auto moved_lib = tempest::shared_library::load("kernel32.dll");
    ASSERT_TRUE(moved_lib.has_value());
    auto target_lib = tempest::move(*moved_lib);
    const auto fn_moved_result = target_lib.get_function_handle<uint32_t>("GetCurrentProcessId");
    ASSERT_TRUE(fn_moved_result.has_value());
    const auto fn_moved = *fn_moved_result;
    EXPECT_EQ(fn_moved(), GetCurrentProcessId());
#endif
}
