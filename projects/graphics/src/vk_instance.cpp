#include "vk_instance.hpp"

#include <iostream>

namespace tempest::graphics::vk
{
    namespace
    {
        VKAPI_ATTR VkBool32 VKAPI_CALL vk_dbg_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                       void* pUserData)
        {

            if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            {
                std::cout << "[ERROR] Vulkan Validation Message: " << pCallbackData->pMessage << "\n";
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            {
                std::cout << "[WARN] Vulkan Validation Message: " << pCallbackData->pMessage << "\n";
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            {
                std::cout << "[INFO] Vulkan Validation Message: " << pCallbackData->pMessage << "\n";
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            {
                std::cout << "[DEBUG] Vulkan Validation Message: " << pCallbackData->pMessage << "\n";
            }
            else
            {
                std::cout << "[OTHER] Vulkan Validation Message: " << pCallbackData->pMessage << "\n";
            }

            return VK_FALSE;
        }

        vkb::Instance create_instance(const instance_factory::create_info& info)
        {
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
    } // namespace

    instance::instance(const instance_factory::create_info& info) : _inst{create_instance(info)}
    {
    }

    instance::instance(instance&& other) noexcept : _inst{std::move(other._inst)}
    {
        other._inst = {};
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

        return *this;
    }
    
    void instance::_release()
    {
        if (_inst)
        {
            vkb::destroy_instance(_inst);
            _inst = {};
        }
    }
} // namespace tempest::graphics::vk