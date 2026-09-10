#include <tempest/print.hpp>

#include <gtest/gtest.h>

namespace
{
    struct mock_memory_sink
    {
        tempest::string content;

        auto write(tempest::string_view text) -> void
        {
            content.append(text.data(), text.size());
        }
    };

    static_assert(tempest::writable_target<mock_memory_sink>, "mock_memory_sink must satisfy writable_target");
} // namespace

//=============================================================================
// Section: Custom Writable Target Tests
//=============================================================================

/// @brief Verifies that print_to formats arguments and writes directly to custom writable targets.
TEST(print_test, custom_target_print_to)
{
    // 1. Setup
    auto sink = mock_memory_sink{};

    // 2. Act
    tempest::print_to(sink, "Value: {} and {}", 42, "hello");

    // 3. Assert
    EXPECT_EQ(sink.content, "Value: 42 and hello");
}

/// @brief Verifies that println_to appends a trailing newline to custom writable targets.
TEST(print_test, custom_target_println_to)
{
    // 1. Setup
    auto sink = mock_memory_sink{};

    // 2. Act
    tempest::println_to(sink, "Line 1: {}", 100);
    tempest::println_to(sink);
    tempest::println_to(sink, "Line 2");

    // 3. Assert
    EXPECT_EQ(sink.content, "Line 1: 100\n\nLine 2\n");
}

//=============================================================================
// Section: Standard Streams Smoke Tests
//=============================================================================

/// @brief Verifies standard stream targets and low-level writers execute without fault.
TEST(print_test, standard_streams_smoke)
{
    // 1. Setup & Act: Smoke test stdout/stderr low-level and high-level outputs
    tempest::write_stdout("");
    tempest::write_stderr("");
    tempest::print_to(tempest::stdout_stream, "");
    tempest::print_to(tempest::stderr_stream, "");
    tempest::println_to(tempest::stdout_stream, "");
    tempest::println_to(tempest::stderr_stream, "");

    // 2. Assert: Functions completed without crash or exception
    SUCCEED();
}
