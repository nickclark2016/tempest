#include <gtest/gtest.h>

#include <tempest/vk/context.hpp>

namespace tempest::rhi::vk
{
    TEST(context_test, create_context)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());
    }

    TEST(context_test, enumerate_devices)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());
    }

    TEST(context_test, create_device)
    {
        auto ctx_desc = context_desc{};
        ctx_desc.application_name = "Tempest Test Application";
        ctx_desc.api = graphics_api::vulkan;

        auto result = vk::create_context(ctx_desc);
        ASSERT_TRUE(result.has_value());

        auto context = tempest::move(result).value();
        auto devices = context->enumerate_devices();
        ASSERT_FALSE(devices.empty());

        auto device_result = context->create_device(devices[0].device_uuid);
        ASSERT_TRUE(device_result != nullptr);
    }
}
