#include <tempest/renderer.hpp>

#include "../glfw_window.hpp"
#include "vk_renderer.hpp"

#include <tempest/logger.hpp>

namespace tempest::graphics
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"[tempest::graphics::vk_renderer]"}});

        vkb::Instance create_instance(const core::version& info)
        {
            vkb::InstanceBuilder bldr = vkb::InstanceBuilder{}
                                            .set_engine_name("Tempest Rendering Engine")
                                            .set_engine_version(0, 0, 1)
                                            .set_app_name("Tempest Rendering Application")
                                            .set_app_version(info.major, info.minor, info.patch)
                                            .require_api_version(1, 3, 0);
#ifdef _DEBUG
            bldr.request_validation_layers().set_debug_callback(
                [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) -> VkBool32 {
                    switch (messageSeverity)
                    {
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: {
                        logger->error("{0}", pCallbackData->pMessage);
                        break;
                    }
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: {
                        logger->warn("{0}", pCallbackData->pMessage);
                        break;
                    }
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: {
                        logger->info("{0}", pCallbackData->pMessage);
                        break;
                    }
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: {
                        logger->debug("{0}", pCallbackData->pMessage);
                        break;
                    }
                    }
                    return VK_FALSE;
                });
#endif
            auto result = bldr.build();
            if (!result)
            {
                logger->error("Failed to create VkInstance. VkResult: {0}",
                              static_cast<std::underlying_type_t<VkResult>>(result.full_error().vk_result));
            }

            return *result;
        }

        vkb::PhysicalDevice select_physical_device(const vkb::Instance& instance)
        {
            auto result = vkb::PhysicalDeviceSelector{instance}
                              .add_required_extensions({VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
                                                        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME})
                              .require_present()
                              .defer_surface_initialization()
                              .set_required_features({
                                  .shaderInt64{VK_TRUE},
                              })
                              .set_required_features_11({
                                  .shaderDrawParameters{VK_TRUE},
                              })
                              .set_required_features_12({
                                  .shaderSampledImageArrayNonUniformIndexing{VK_TRUE},
                                  .descriptorBindingUpdateUnusedWhilePending{VK_TRUE},
                                  .descriptorBindingPartiallyBound{VK_TRUE},
                                  .descriptorBindingVariableDescriptorCount{VK_TRUE},
                                  .runtimeDescriptorArray{VK_TRUE},
                                  .hostQueryReset{VK_TRUE},
                                  .timelineSemaphore{VK_TRUE},
                                  .bufferDeviceAddress{VK_TRUE},
                                  .shaderOutputLayer{VK_TRUE},
                              })
                              .set_minimum_version(1, 2)
                              .select();
            if (!result)
            {
                logger->error("Failed to fetch suitable VkPhysicalDevice: {0}",
                              static_cast<std::underlying_type_t<VkResult>>(result.full_error().vk_result));
            }

            return *result;
        }

        vkb::Device create_device(const vkb::PhysicalDevice& physical)
        {
            VkPhysicalDeviceSynchronization2Features sync_feats = {
                .sType{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES},
                .synchronization2{VK_TRUE},
            };

            auto result = vkb::DeviceBuilder{physical}.add_pNext(&sync_feats).build();
            if (!result)
            {
                logger->error("Failed to build VkDevice: {0}",
                              static_cast<std::underlying_type_t<VkResult>>(result.full_error().vk_result));
            }

            return *result;
        }
    } // namespace

    std::unique_ptr<irenderer> irenderer::create(const core::version& version_info, iwindow& win)
    {
        return std::unique_ptr<irenderer>(new irenderer(version_info, win));
    }

    irenderer::irenderer(const core::version& version_info, iwindow& win) : _impl{new impl()}
    {
        _impl->instance = create_instance(version_info);
        _impl->physical_device = select_physical_device(_impl->instance);
        _impl->logical_device = create_device(_impl->physical_device);

        auto& glfw_win = static_cast<glfw::window&>(win);
        auto raw = glfw_win.raw();
    }

    irenderer::~irenderer()
    {
        vkb::destroy_device(_impl->logical_device);
        vkb::destroy_instance(_impl->instance);
    }

    void irenderer::render()
    {
    }
} // namespace tempest::graphics