#include <gtest/gtest.h>

#include <tempest/logger.hpp>
#include <tempest/vk/context.hpp>

namespace tempest::rhi::vk
{
    /// @brief Test fixture providing a logger for Vulkan RHI context tests.
    class context_test : public ::testing::Test
    {
      protected:
        stdout_log_sink log_sink{};
        logger test_logger{log_sink};
    };

    /// @brief Verifies that a Vulkan RHI context can be created successfully.
    TEST_F(context_test, create_context)
    {
        // 1. Setup context description
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        // 2. Act: create context
        auto result = vk::create_context(ctx_desc, test_logger);

        // 3. Assert: context creation succeeded
        ASSERT_TRUE(result.has_value());
    }

    /// @brief Verifies that physical devices can be enumerated from the created context.
    TEST_F(context_test, enumerate_devices)
    {
        // 1. Setup context description
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        // 2. Act: create context and enumerate devices
        auto result = vk::create_context(ctx_desc, test_logger);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();

        // 3. Assert: at least one device is found
        ASSERT_FALSE(devices.empty());
    }

    /// @brief Verifies that a logical device can be created from an enumerated physical device.
    TEST_F(context_test, create_device)
    {
        // 1. Setup context description
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        // 2. Act: create context and enumerate devices
        auto result = vk::create_context(ctx_desc, test_logger);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device_result = context->create_device(devices[0].device_uuid);

        // 3. Assert: logical device creation succeeded
        ASSERT_TRUE(device_result != nullptr);
    }
} // namespace tempest::rhi::vk
