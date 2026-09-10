#include <gtest/gtest.h>

#include <tempest/logger.hpp>
#include <tempest/vk/bootstrap.hpp>

namespace tempest::rhi::vk
{
    namespace
    {
        /// @brief Test fixture providing a logger for Vulkan bootstrap tests.
        class bootstrap_test : public ::testing::Test
        {
          protected:
            stdout_log_sink log_sink{};
            logger test_logger{log_sink};
        };
    } // namespace

    // =========================================================================
    // Instance Creation & Destruction Tests
    // =========================================================================

    /// @brief Verifies that native Vulkan 1.3 instance creation succeeds with valid configuration.
    TEST_F(bootstrap_test, create_instance_success)
    {
        // 1. Setup instance description
        const auto ctx_desc = context_desc{
            .application_name = "Tempest Bootstrap Instance Test",
            .version_major = 1,
            .version_minor = 0,
            .version_patch = 0,
            .enable_api_validation = true,
            .api = graphics_api::vulkan,
        };

        // 2. Act: create native instance
        auto inst_res = create_instance(ctx_desc, test_logger);

        // 3. Assert: instance handle and dispatch table are populated
        ASSERT_TRUE(inst_res.has_value());
        auto inst = tempest::move(inst_res).value();
        EXPECT_NE(inst.instance, VK_NULL_HANDLE);
        EXPECT_NE(inst.dispatch.fp_vkEnumeratePhysicalDevices, nullptr);
        EXPECT_NE(inst.dispatch.fp_vkCreateDevice, nullptr);

        // 4. Teardown
        destroy_instance(inst);
        EXPECT_EQ(inst.instance, VK_NULL_HANDLE);
    }

    /// @brief Verifies that instance creation fails gracefully when requested API is not Vulkan.
    TEST_F(bootstrap_test, create_instance_unsupported_api)
    {
        // 1. Setup instance description with non-Vulkan API
        const auto ctx_desc = context_desc{
            .application_name = "Tempest Bootstrap Invalid API Test",
            .version_major = 1,
            .version_minor = 0,
            .version_patch = 0,
            .enable_api_validation = false,
            .api = static_cast<graphics_api>(999),
        };

        // 2. Act: create native instance
        auto inst_res = create_instance(ctx_desc, test_logger);

        // 3. Assert: returns unsupported_api error
        ASSERT_FALSE(inst_res.has_value());
        EXPECT_EQ(inst_res.error(), context_creation_error::unsupported_api);
    }

    // =========================================================================
    // Physical Device Enumeration & Filtering Tests
    // =========================================================================

    /// @brief Verifies that physical devices supporting Vulkan 1.3 and required extensions are discovered.
    TEST_F(bootstrap_test, enumerate_physical_devices)
    {
        // 1. Setup instance
        const auto ctx_desc = context_desc{
            .application_name = "Tempest Bootstrap PhysDev Test",
            .version_major = 1,
            .version_minor = 0,
            .version_patch = 0,
            .enable_api_validation = true,
            .api = graphics_api::vulkan,
        };
        auto inst_res = create_instance(ctx_desc, test_logger);
        ASSERT_TRUE(inst_res.has_value());
        auto inst = tempest::move(inst_res).value();

        // 2. Act: enumerate physical devices
        auto devs_res = enumerate_physical_devices(inst);

        // 3. Assert: at least one valid Vulkan 1.3 physical device found
        ASSERT_TRUE(devs_res.has_value());
        const auto& devs = devs_res.value();
        ASSERT_FALSE(devs.empty());

        for (const auto& dev : devs)
        {
            EXPECT_NE(dev.physical_device, VK_NULL_HANDLE);
            EXPECT_GE(dev.properties.apiVersion, VK_API_VERSION_1_3);
            EXPECT_TRUE(dev.has_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME));
            EXPECT_TRUE(dev.has_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME));
            EXPECT_TRUE(dev.has_extension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME));
        }

        // 4. Teardown
        destroy_instance(inst);
    }

    // =========================================================================
    // Queue Family Discovery Tests
    // =========================================================================

    /// @brief Verifies that graphics queue family is correctly identified on the selected device.
    TEST_F(bootstrap_test, find_queue_families)
    {
        // 1. Setup instance and enumerate physical devices
        const auto ctx_desc = context_desc{
            .application_name = "Tempest Bootstrap Queue Test",
            .version_major = 1,
            .version_minor = 0,
            .version_patch = 0,
            .enable_api_validation = true,
            .api = graphics_api::vulkan,
        };
        auto inst_res = create_instance(ctx_desc, test_logger);
        ASSERT_TRUE(inst_res.has_value());
        auto inst = tempest::move(inst_res).value();

        auto devs_res = enumerate_physical_devices(inst);
        ASSERT_TRUE(devs_res.has_value());
        const auto& devs = devs_res.value();
        ASSERT_FALSE(devs.empty());

        // 2. Act: find queue families
        const auto queue_families = find_queue_families(inst, devs[0].physical_device);

        // 3. Assert: graphics queue family is complete and valid
        EXPECT_TRUE(queue_families.is_complete());
        EXPECT_NE(queue_families.graphics, ~0U);
        EXPECT_NE(queue_families.compute, ~0U);
        EXPECT_NE(queue_families.transfer, ~0U);

        // 4. Teardown
        destroy_instance(inst);
    }

    // =========================================================================
    // Logical Device Creation & Destruction Tests
    // =========================================================================

    /// @brief Verifies that a native logical device with dispatch table is successfully created.
    TEST_F(bootstrap_test, create_logical_device)
    {
        // 1. Setup instance and enumerate physical devices
        const auto ctx_desc = context_desc{
            .application_name = "Tempest Bootstrap Logical Device Test",
            .version_major = 1,
            .version_minor = 0,
            .version_patch = 0,
            .enable_api_validation = true,
            .api = graphics_api::vulkan,
        };
        auto inst_res = create_instance(ctx_desc, test_logger);
        ASSERT_TRUE(inst_res.has_value());
        auto inst = tempest::move(inst_res).value();

        auto devs_res = enumerate_physical_devices(inst);
        ASSERT_TRUE(devs_res.has_value());
        const auto& devs = devs_res.value();
        ASSERT_FALSE(devs.empty());

        // 2. Act: create logical device
        auto dev_res = create_device(inst, devs[0]);

        // 3. Assert: device and queues are properly initialized
        ASSERT_TRUE(dev_res.has_value());
        auto dev = tempest::move(dev_res).value();
        EXPECT_NE(dev.device, VK_NULL_HANDLE);
        EXPECT_NE(dev.graphics_queue, VK_NULL_HANDLE);
        EXPECT_NE(dev.graphics_queue_index, ~0U);
        EXPECT_NE(dev.dispatch.fp_vkAllocateCommandBuffers, nullptr);
        EXPECT_NE(dev.dispatch.fp_vkCreateSwapchainKHR, nullptr);

        const auto g_q = dev.get_queue(queue_type::graphics);
        EXPECT_TRUE(g_q.has_value());
        EXPECT_EQ(g_q.value(), dev.graphics_queue);

        const auto g_idx = dev.get_queue_index(queue_type::graphics);
        EXPECT_TRUE(g_idx.has_value());
        EXPECT_EQ(g_idx.value(), dev.graphics_queue_index);

        // 4. Teardown: destroy device then instance
        destroy_device(dev);
        EXPECT_EQ(dev.device, VK_NULL_HANDLE);

        destroy_instance(inst);
    }
} // namespace tempest::rhi::vk