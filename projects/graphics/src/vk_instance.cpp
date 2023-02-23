#include "vk_instance.hpp"

#include <algorithm>
#include <iostream>

#include <tempest/logger.hpp>

namespace tempest::graphics::vk
{
    namespace
    {
        std::unique_ptr<logger::ilogger> logger;

        VKAPI_ATTR VkBool32 VKAPI_CALL vk_dbg_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                       void* pUserData)
        {

            if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            {
                logger->error("Vulkan Validation Message: {0}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            {
                logger->warn("Vulkan Validation Message: {0}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            {
                logger->info("Vulkan Validation Message: {0}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            {
                logger->debug("Vulkan Validation Message: {0}", pCallbackData->pMessage);
            }
            else
            {
                logger->info("Vulkan Validation Message: {0}", pCallbackData->pMessage);
            }

            return VK_FALSE;
        }

        vkb::Instance create_instance(const instance_factory::create_info& info)
        {
            logger = logger::logger_factory::create({
                .prefix{"VKInstance"},
            });

            vkb::InstanceBuilder bldr = vkb::InstanceBuilder{}
                                            .set_engine_name("Tempest Engine")
                                            .set_engine_version(0, 0, 1)
                                            .set_app_name(info.name.data())
                                            .set_app_version(info.version_major, info.version_minor, info.version_patch)
                                            .require_api_version(VKB_VK_API_VERSION_1_3);

#ifdef _DEBUG
            bldr.set_debug_callback(vk_dbg_callback)
                .set_debug_messenger_severity(
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
                .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT);
#endif
            auto result = bldr.build();
            assert(result && "Failed to create vkb::Instance.");

            return *result;
        }

        std::vector<std::unique_ptr<idevice>> create_devices(vkb::Instance inst)
        {
            vkb::PhysicalDeviceSelector selector =
                vkb::PhysicalDeviceSelector{inst}
                    .set_minimum_version(1, 3)
                    .set_required_features({
                        .independentBlend{VK_TRUE},
                        .logicOp{VK_TRUE},
                        .depthClamp{VK_TRUE},
                        .depthBiasClamp{VK_TRUE},
                        .fillModeNonSolid{VK_TRUE},
                        .depthBounds{VK_TRUE},
                        .alphaToOne{VK_TRUE},
                    })
                    .set_required_features_11({
                        .sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES},
                        .pNext{nullptr},
                    })
                    .set_required_features_12({
                        .sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES},
                        .pNext{nullptr},
                        .drawIndirectCount{VK_TRUE},
                        .imagelessFramebuffer{VK_TRUE},
                        .separateDepthStencilLayouts{VK_TRUE},
                    })
                    .set_required_features_13({
                        .sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES},
                        .pNext{nullptr},
                        .dynamicRendering{VK_TRUE},
                    })
                    .require_present()
                    .defer_surface_initialization();

            auto physical_devices_result = selector.select_devices();
            assert(physical_devices_result && "Failed to select suitable Vulkan physical devices.");

            std::vector<std::unique_ptr<idevice>> devices;
            std::transform(physical_devices_result->begin(), physical_devices_result->end(),
                           std::back_inserter(devices), [&inst](const vkb::PhysicalDevice& dev) {
                               auto vk_device_result = vkb::DeviceBuilder{dev}.build();
                               assert(vk_device_result && "Failed to build Vulkan device.");
                               return std::make_unique<device>(std::move(*vk_device_result));
                           });

            return devices;
        }
    } // namespace

    device::device(vkb::Device&& dev) : _dev{std::move(dev)}, _dispatch{_dev.make_table()}
    {
    }

    device::device(device&& other) noexcept : _dev{std::move(other._dev)}, _dispatch{std::move(other._dispatch)}
    {
        other._dev = {};
        other._dispatch = {};
    }

    device::~device()
    {
        _release();
    }

    device& device::operator=(device&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release();

        std::swap(_dev, rhs._dev);
        std::swap(_dispatch, rhs._dispatch);

        return *this;
    }

    void device::_release()
    {
        if (_dev)
        {
            vkb::destroy_device(_dev);
            _dev = {};
            _dispatch = {};
        }
    }

    instance::instance(const instance_factory::create_info& info)
        : _inst{create_instance(info)}, _devices{create_devices(_inst)}
    {
    }

    instance::instance(instance&& other) noexcept : _inst{std::move(other._inst)}, _devices{std::move(other._devices)}
    {
        other._inst = {};
        other._devices = std::vector<std::unique_ptr<idevice>>();
    }

    instance::~instance()
    {
        _release();
    }

    instance& instance::operator=(instance&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release();

        std::swap(_inst, rhs._inst);
        std::swap(_devices, rhs._devices);

        return *this;
    }

    std::span<const std::unique_ptr<idevice>> instance::get_devices() const noexcept
    {
        return std::span{_devices};
    }

    void instance::_release()
    {
        _devices.clear();

        if (_inst)
        {
            vkb::destroy_instance(_inst);
            _inst = {};
        }
    }
} // namespace tempest::graphics::vk