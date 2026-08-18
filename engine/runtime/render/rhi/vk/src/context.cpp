#if defined(TEMPEST_PLATFORM_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(TEMPEST_PLATFORM_LINUX)
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include <tempest/vk/context.hpp>

#include <VkBootstrap.h>
#include <VkBootstrapDispatch.h>
#include <algorithm>
#include <tempest/rhi.hpp>
#include <tempest/vk/device.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_structs.hpp>

namespace tempest::rhi::vk
{
    namespace
    {
        constexpr uint32_t VENDOR_ID_AMD = 0x1002;
        constexpr uint32_t VENDOR_ID_APPLE = 0x106B;
        constexpr uint32_t VENDOR_ID_ARM = 0x13B5;
        constexpr uint32_t VENDOR_ID_IMGTEC = 0x1010;
        constexpr uint32_t VENDOR_ID_INTEL = 0x8086;
        constexpr uint32_t VENDOR_ID_KHRONOS_MIN = 0x10000;
        constexpr uint32_t VENDOR_ID_KHRONOS_MAX = 0x10006;
        constexpr uint32_t VENDOR_ID_NVIDIA = 0x10DE;
        constexpr uint32_t VENDOR_ID_QUALCOMM = 0x5143;

        auto get_device_vendor_from_vk_vendor_id(uint32_t vk_vendor_id) -> rhi::device_vendor
        {
            switch (vk_vendor_id)
            {
            case VENDOR_ID_AMD:
                return rhi::device_vendor::amd;
            case VENDOR_ID_APPLE:
                return rhi::device_vendor::apple;
            case VENDOR_ID_ARM:
                return rhi::device_vendor::arm;
            case VENDOR_ID_IMGTEC:
                return rhi::device_vendor::imgtec;
            case VENDOR_ID_INTEL:
                return rhi::device_vendor::intel;
            case VENDOR_ID_NVIDIA:
                return rhi::device_vendor::nvidia;
            case VENDOR_ID_QUALCOMM:
                return rhi::device_vendor::qualcomm;
            default: {
                if (vk_vendor_id >= VENDOR_ID_KHRONOS_MIN && vk_vendor_id <= VENDOR_ID_KHRONOS_MAX)
                {
                    return rhi::device_vendor::khronos;
                }
                return rhi::device_vendor::unknown;
            }
            }
        }

        auto enumerate_supported_devices(vkb::Instance& instance)
            -> expected<vector<vkb::PhysicalDevice>, context_creation_error>
        {
            auto vkb_physical_devices_result =
                vkb::PhysicalDeviceSelector(instance)
                    .set_minimum_version(1, 3)
                    .require_present(true)
                    .defer_surface_initialization()
                    .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                    .set_required_features(::vk::PhysicalDeviceFeatures()
                                               .setLogicOp(VK_TRUE)
                                               .setIndependentBlend(VK_TRUE)
                                               .setDepthClamp(VK_TRUE)
                                               .setDepthBiasClamp(VK_TRUE)
                                               .setFillModeNonSolid(VK_TRUE)
                                               .setDepthBounds(VK_TRUE)
                                               .setSamplerAnisotropy(VK_TRUE)
                                               .setPipelineStatisticsQuery(VK_TRUE)
                                               .setFragmentStoresAndAtomics(VK_TRUE)
                                               .setShaderUniformBufferArrayDynamicIndexing(VK_TRUE)
                                               .setShaderSampledImageArrayDynamicIndexing(VK_TRUE)
                                               .setShaderStorageBufferArrayDynamicIndexing(VK_TRUE)
                                               .setShaderStorageImageArrayDynamicIndexing(VK_TRUE)
                                               .setShaderInt16(VK_TRUE)
                                               .setShaderInt64(VK_TRUE)
                                               .setMultiDrawIndirect(VK_TRUE))
                    .set_required_features_11(::vk::PhysicalDeviceVulkan11Features()
                                                  .setStorageBuffer16BitAccess(VK_TRUE)
                                                  .setUniformAndStorageBuffer16BitAccess(VK_TRUE)
                                                  .setShaderDrawParameters(VK_TRUE))
                    .set_required_features_12(::vk::PhysicalDeviceVulkan12Features()
                                                  .setDrawIndirectCount(VK_TRUE)
                                                  .setScalarBlockLayout(VK_TRUE)
                                                  .setStorageBuffer8BitAccess(VK_TRUE)
                                                  .setShaderFloat16(VK_TRUE)
                                                  .setShaderUniformBufferArrayNonUniformIndexing(VK_TRUE)
                                                  .setShaderSampledImageArrayNonUniformIndexing(VK_TRUE)
                                                  .setShaderStorageBufferArrayNonUniformIndexing(VK_TRUE)
                                                  .setShaderStorageImageArrayNonUniformIndexing(VK_TRUE)
                                                  .setDescriptorBindingSampledImageUpdateAfterBind(VK_TRUE)
                                                  .setDescriptorBindingStorageImageUpdateAfterBind(VK_TRUE)
                                                  .setDescriptorBindingStorageBufferUpdateAfterBind(VK_TRUE)
                                                  .setDescriptorBindingUniformBufferUpdateAfterBind(VK_TRUE)
                                                  .setDescriptorBindingUpdateUnusedWhilePending(VK_TRUE)
                                                  .setDescriptorBindingPartiallyBound(VK_TRUE)
                                                  .setDescriptorBindingVariableDescriptorCount(VK_TRUE)
                                                  .setSeparateDepthStencilLayouts(VK_TRUE)
                                                  .setRuntimeDescriptorArray(VK_TRUE)
                                                  .setHostQueryReset(VK_TRUE)
                                                  .setUniformBufferStandardLayout(VK_TRUE)
                                                  .setTimelineSemaphore(VK_TRUE)
                                                  .setBufferDeviceAddress(VK_TRUE)
                                                  .setVulkanMemoryModel(VK_TRUE)
                                                  .setVulkanMemoryModelDeviceScope(VK_TRUE)
                                                  .setVulkanMemoryModelAvailabilityVisibilityChains(VK_TRUE))
                    .set_required_features_13(::vk::PhysicalDeviceVulkan13Features()
                                                  .setDynamicRendering(VK_TRUE)
                                                  .setShaderDemoteToHelperInvocation(VK_TRUE)
                                                  .setSynchronization2(VK_TRUE)
                                                  .setMaintenance4(VK_TRUE))
                    .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
                    .add_required_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)
                    .add_required_extension_features(VkPhysicalDeviceDescriptorBufferFeaturesEXT{
                        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
                        .pNext = nullptr,
                        .descriptorBuffer = VK_TRUE,
                    })
                    .add_required_extension(VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME)
                    .select_devices();

            if (!vkb_physical_devices_result)
            {
                return unexpected{.value = context_creation_error::no_valid_devices_found};
            }

            const auto& vkb_physical_devices = vkb_physical_devices_result.value();

            auto devices = vector<vkb::PhysicalDevice>();
            for (const auto& vkb_physical_device : vkb_physical_devices)
            {
                devices.push_back(vkb_physical_device);
            }
            return devices;
        }

        auto fetch_device_desc(const vkb::InstanceDispatchTable& dispatch_table, vkb::PhysicalDevice physical_device)
        {
            auto dev_desc = device_desc{};

            const auto& vk_physical_device = physical_device.physical_device;

            // Get the device UUID from the physical device properties
            auto physical_device_id_props = ::vk::PhysicalDeviceIDProperties();
            auto physical_device_props2 = VkPhysicalDeviceProperties2{};
            physical_device_props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            physical_device_props2.pNext = &physical_device_id_props;

            dispatch_table.getPhysicalDeviceProperties2(vk_physical_device, &physical_device_props2);

            static_assert(sizeof(dev_desc.device_uuid) == sizeof(physical_device_id_props.deviceUUID),
                          "Device UUID size mismatch");
            std::memcpy(dev_desc.device_uuid.data.data(), physical_device_id_props.deviceUUID,
                        sizeof(dev_desc.device_uuid));

            // Get the device features from the physical device
            auto supported_extensions = physical_device.get_extensions();

            // Check for ray query
            dev_desc.features.ray_query =
                std::ranges::find(supported_extensions, VK_KHR_RAY_QUERY_EXTENSION_NAME) != supported_extensions.end();

            // Check for ray tracing
            dev_desc.features.ray_tracing =
                std::ranges::find(supported_extensions, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) !=
                    supported_extensions.end() &&
                std::ranges::find(supported_extensions, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) !=
                    supported_extensions.end();

            // Check for mesh shading
            dev_desc.features.mesh_shading =
                std::ranges::find(supported_extensions, VK_NV_MESH_SHADER_EXTENSION_NAME) != supported_extensions.end();

            // Get the device name from the physical device properties
            auto physical_device_props = physical_device.properties;
            dev_desc.name = physical_device_props.deviceName;

            // Get the vendor from the physical device properties
            dev_desc.vendor = get_device_vendor_from_vk_vendor_id(physical_device_props.vendorID);
            return dev_desc;
        }
    } // namespace

    auto create_context(const context_desc& desc) -> expected<unique_ptr<rhi::context>, context_creation_error>
    {
        return context::create(desc);
    }

    auto context::create(const context_desc& desc) -> expected<unique_ptr<rhi::context>, context_creation_error>
    {
        auto vkb_instance_builder =
            vkb::InstanceBuilder()
                .set_app_name(desc.application_name.data())
                .set_engine_name("Tempest Engine")
                .require_api_version(VK_API_VERSION_1_3)
                .set_engine_version(VK_MAKE_VERSION(0, 0, 1))
                .set_app_version(VK_MAKE_VERSION(desc.version_major, desc.version_minor, desc.version_patch));

        if (desc.enable_api_validation)
        {
            vkb_instance_builder.enable_validation_layers();
        }

#if defined(TEMPEST_PLATFORM_WINDOWS)
        vkb_instance_builder.enable_extension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(TEMPEST_PLATFORM_LINUX)
        vkb_instance_builder.enable_extension(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

        auto vkb_instance_result = vkb_instance_builder.build();
        if (!vkb_instance_result)
        {
            return unexpected{.value = context_creation_error::context_creation_failed};
        }

        auto vkb_instance = vkb_instance_result.value();

        auto physical_devices_result = enumerate_supported_devices(vkb_instance);
        if (!physical_devices_result)
        {
            return unexpected{.value = context_creation_error::no_valid_devices_found};
        }

        auto instance_table = vkb_instance.make_table();
        auto devices = vector<device_desc>();

        for (const auto& vkb_physical_device : *physical_devices_result)
        {
            devices.push_back(fetch_device_desc(instance_table, vkb_physical_device));
        }

        return unique_ptr<rhi::context>{new context{tempest::move(vkb_instance), tempest::move(devices)}};
    }

    context::~context()
    {
        vkb::destroy_instance(_instance);
    }

    auto context::enumerate_devices() -> span<const device_desc>
    {
        return _devices;
    }

    auto context::create_device(guid device_uuid) -> unique_ptr<rhi::device>
    {
        // Find the physical device with the matching UUID
        auto physical_devices = enumerate_supported_devices(_instance);
        if (!physical_devices)
        {
            return nullptr;
        }

        auto* physical_device_iter =
            tempest::find_if(physical_devices->begin(), physical_devices->end(),
                             [&device_uuid, this](const vkb::PhysicalDevice& vkb_physical_device) {
                                 auto physical_device_id_props = ::vk::PhysicalDeviceIDProperties();
                                 auto physical_device_props2 = VkPhysicalDeviceProperties2{};
                                 physical_device_props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                                 physical_device_props2.pNext = &physical_device_id_props;

                                 auto instance_table = _instance.make_table();
                                 instance_table.getPhysicalDeviceProperties2(vkb_physical_device.physical_device,
                                                                             &physical_device_props2);

                                 return std::memcmp(physical_device_id_props.deviceUUID, device_uuid.data.data(),
                                                    sizeof(device_uuid)) == 0;
                             });

        if (physical_device_iter == physical_devices->end())
        {
            return nullptr;
        }

        auto vkb_physical_device = *physical_device_iter;
        auto device_builder = vkb::DeviceBuilder(vkb_physical_device);
        auto device_result = device_builder.build();
        if (!device_result)
        {
            return nullptr;
        }

        const auto& vkb_device = device_result.value();
        auto instance_table = _instance.make_table();

        return device::create(_instance, vkb_physical_device, vkb_device,
                              fetch_device_desc(instance_table, vkb_physical_device));
    }

    context::context(vkb::Instance instance, vector<device_desc> devices)
        : _instance{tempest::move(instance)}, _devices{tempest::move(devices)}
    {
    }
} // namespace tempest::rhi::vk
