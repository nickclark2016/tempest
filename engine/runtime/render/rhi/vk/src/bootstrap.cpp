#include <tempest/vk/bootstrap.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/assert.hpp>
#include <tempest/logger.hpp>

#include <tempest/string_view.hpp>

#if defined(TEMPEST_PLATFORM_WINDOWS) || defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <vulkan/vulkan_win32.h>
#include <windows.h>
#elif defined(TEMPEST_PLATFORM_LINUX)
// clang-format off
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_xcb.h>
// clang-format on
#if defined(None)
#undef None
#endif
#if defined(Success)
#undef Success
#endif
#if defined(Always)
#undef Always
#endif
#else
#include <dlfcn.h>
#endif

namespace tempest::rhi::vk
{
    namespace
    {
        auto load_vulkan_library() -> void*
        {
#ifdef _WIN32
            return static_cast<void*>(LoadLibraryA("vulkan-1.dll"));
#elif defined(__APPLE__)
            void* lib = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
            if (!lib)
            {
                lib = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
            }
            if (!lib)
            {
                lib = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
            }
            return lib;
#else
            void* lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
            if (!lib)
            {
                lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
            }
            return lib;
#endif
        }

        auto free_vulkan_library(void* handle) -> void
        {
            if (handle == nullptr)
            {
                return;
            }
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
        }

        auto get_vulkan_proc_address(void* handle, const char* name) -> void*
        {
            if (handle == nullptr)
            {
                return nullptr;
            }
#ifdef _WIN32
            return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
            return dlsym(handle, name);
#endif
        }

        VKAPI_ATTR auto VKAPI_CALL debug_utils_messenger_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
            [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_types,
            const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) -> VkBool32
        {
            if (user_data != nullptr && callback_data != nullptr && callback_data->pMessage != nullptr)
            {
                auto* const log = static_cast<logger*>(user_data);
                auto msg = string_view{callback_data->pMessage};

                if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
                {
                    log->error(msg);
                }
                else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
                {
                    log->warn(msg);
                }
                else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0)
                {
                    log->info(msg);
                }
                else
                {
                    log->trace(msg);
                }
            }
            return VK_FALSE;
        }

        template <typename T>
        auto load_proc(VkInstance inst, PFN_vkGetInstanceProcAddr get_proc, const char* name, T& out) -> void
        {
            out = reinterpret_cast<T>(get_proc(inst, name));
        }

        template <typename T>
        auto load_dev_proc(VkDevice dev, PFN_vkGetDeviceProcAddr get_proc, const char* name, T& out) -> void
        {
            out = reinterpret_cast<T>(get_proc(dev, name));
        }
    } // namespace

    auto instance_dispatch_table::init(VkInstance inst, PFN_vkGetInstanceProcAddr get_proc_addr) noexcept -> void
    {
        instance = inst;
        fp_vkGetInstanceProcAddr = get_proc_addr;

        if (get_proc_addr == nullptr || inst == VK_NULL_HANDLE)
        {
            return;
        }

        load_proc(inst, get_proc_addr, "vkDestroyInstance", fp_vkDestroyInstance);
        load_proc(inst, get_proc_addr, "vkEnumeratePhysicalDevices", fp_vkEnumeratePhysicalDevices);
        load_proc(inst, get_proc_addr, "vkGetPhysicalDeviceProperties", fp_vkGetPhysicalDeviceProperties);
        load_proc(inst, get_proc_addr, "vkGetPhysicalDeviceProperties2", fp_vkGetPhysicalDeviceProperties2);
        load_proc(inst, get_proc_addr, "vkGetPhysicalDeviceFeatures", fp_vkGetPhysicalDeviceFeatures);
        load_proc(inst, get_proc_addr, "vkGetPhysicalDeviceFeatures2", fp_vkGetPhysicalDeviceFeatures2);
        load_proc(inst, get_proc_addr, "vkGetPhysicalDeviceQueueFamilyProperties",
                  fp_vkGetPhysicalDeviceQueueFamilyProperties);
        load_proc(inst, get_proc_addr, "vkGetPhysicalDeviceMemoryProperties", fp_vkGetPhysicalDeviceMemoryProperties);
        load_proc(inst, get_proc_addr, "vkEnumerateDeviceExtensionProperties", fp_vkEnumerateDeviceExtensionProperties);
        load_proc(inst, get_proc_addr, "vkCreateDevice", fp_vkCreateDevice);
        load_proc(inst, get_proc_addr, "vkGetDeviceProcAddr", fp_vkGetDeviceProcAddr);
        load_proc(inst, get_proc_addr, "vkDestroySurfaceKHR", fp_vkDestroySurfaceKHR);
        load_proc(inst, get_proc_addr, "vkGetPhysicalDeviceSurfaceSupportKHR", fp_vkGetPhysicalDeviceSurfaceSupportKHR);
        load_proc(inst, get_proc_addr, "vkGetPhysicalDeviceSurfaceFormatsKHR", fp_vkGetPhysicalDeviceSurfaceFormatsKHR);
        load_proc(inst, get_proc_addr, "vkGetPhysicalDeviceSurfacePresentModesKHR",
                  fp_vkGetPhysicalDeviceSurfacePresentModesKHR);
        load_proc(inst, get_proc_addr, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
                  fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
        load_proc(inst, get_proc_addr, "vkCreateDebugUtilsMessengerEXT", fp_vkCreateDebugUtilsMessengerEXT);
        load_proc(inst, get_proc_addr, "vkDestroyDebugUtilsMessengerEXT", fp_vkDestroyDebugUtilsMessengerEXT);

#ifdef TEMPEST_PLATFORM_WINDOWS
        fp_vkCreateWin32SurfaceKHR = reinterpret_cast<void*>(get_proc_addr(inst, "vkCreateWin32SurfaceKHR"));
#elif defined(TEMPEST_PLATFORM_LINUX)
        fp_vkCreateXlibSurfaceKHR = reinterpret_cast<void*>(get_proc_addr(inst, "vkCreateXlibSurfaceKHR"));
        fp_vkCreateXcbSurfaceKHR = reinterpret_cast<void*>(get_proc_addr(inst, "vkCreateXcbSurfaceKHR"));
#endif
    }

    auto dispatch_table::init(VkDevice dev, PFN_vkGetDeviceProcAddr get_device_proc_addr) noexcept -> void
    {
        device = dev;
        fp_vkGetDeviceProcAddr = get_device_proc_addr;

        if (get_device_proc_addr == nullptr || dev == VK_NULL_HANDLE)
        {
            return;
        }

        load_dev_proc(dev, get_device_proc_addr, "vkDestroyDevice", fp_vkDestroyDevice);
        load_dev_proc(dev, get_device_proc_addr, "vkGetDeviceQueue", fp_vkGetDeviceQueue);
        load_dev_proc(dev, get_device_proc_addr, "vkDeviceWaitIdle", fp_vkDeviceWaitIdle);
        load_dev_proc(dev, get_device_proc_addr, "vkCreateCommandPool", fp_vkCreateCommandPool);
        load_dev_proc(dev, get_device_proc_addr, "vkDestroyCommandPool", fp_vkDestroyCommandPool);
        load_dev_proc(dev, get_device_proc_addr, "vkResetCommandPool", fp_vkResetCommandPool);
        load_dev_proc(dev, get_device_proc_addr, "vkAllocateCommandBuffers", fp_vkAllocateCommandBuffers);
        load_dev_proc(dev, get_device_proc_addr, "vkFreeCommandBuffers", fp_vkFreeCommandBuffers);
        load_dev_proc(dev, get_device_proc_addr, "vkBeginCommandBuffer", fp_vkBeginCommandBuffer);
        load_dev_proc(dev, get_device_proc_addr, "vkEndCommandBuffer", fp_vkEndCommandBuffer);
        load_dev_proc(dev, get_device_proc_addr, "vkCreateSemaphore", fp_vkCreateSemaphore);
        load_dev_proc(dev, get_device_proc_addr, "vkDestroySemaphore", fp_vkDestroySemaphore);
        load_dev_proc(dev, get_device_proc_addr, "vkGetSemaphoreCounterValue", fp_vkGetSemaphoreCounterValue);
        load_dev_proc(dev, get_device_proc_addr, "vkWaitSemaphores", fp_vkWaitSemaphores);
        load_dev_proc(dev, get_device_proc_addr, "vkSignalSemaphore", fp_vkSignalSemaphore);
        load_dev_proc(dev, get_device_proc_addr, "vkCreateEvent", fp_vkCreateEvent);
        load_dev_proc(dev, get_device_proc_addr, "vkDestroyEvent", fp_vkDestroyEvent);
        load_dev_proc(dev, get_device_proc_addr, "vkCreateImageView", fp_vkCreateImageView);
        load_dev_proc(dev, get_device_proc_addr, "vkDestroyImageView", fp_vkDestroyImageView);
        load_dev_proc(dev, get_device_proc_addr, "vkCreateSampler", fp_vkCreateSampler);
        load_dev_proc(dev, get_device_proc_addr, "vkDestroySampler", fp_vkDestroySampler);
        load_dev_proc(dev, get_device_proc_addr, "vkCreateShaderModule", fp_vkCreateShaderModule);
        load_dev_proc(dev, get_device_proc_addr, "vkDestroyShaderModule", fp_vkDestroyShaderModule);
        load_dev_proc(dev, get_device_proc_addr, "vkCreateGraphicsPipelines", fp_vkCreateGraphicsPipelines);
        load_dev_proc(dev, get_device_proc_addr, "vkCreateComputePipelines", fp_vkCreateComputePipelines);
        load_dev_proc(dev, get_device_proc_addr, "vkDestroyPipeline", fp_vkDestroyPipeline);
        load_dev_proc(dev, get_device_proc_addr, "vkCreatePipelineLayout", fp_vkCreatePipelineLayout);
        load_dev_proc(dev, get_device_proc_addr, "vkDestroyPipelineLayout", fp_vkDestroyPipelineLayout);
        load_dev_proc(dev, get_device_proc_addr, "vkCreateDescriptorSetLayout", fp_vkCreateDescriptorSetLayout);
        load_dev_proc(dev, get_device_proc_addr, "vkDestroyDescriptorSetLayout", fp_vkDestroyDescriptorSetLayout);
        load_dev_proc(dev, get_device_proc_addr, "vkCreateQueryPool", fp_vkCreateQueryPool);
        load_dev_proc(dev, get_device_proc_addr, "vkDestroyQueryPool", fp_vkDestroyQueryPool);
        load_dev_proc(dev, get_device_proc_addr, "vkGetQueryPoolResults", fp_vkGetQueryPoolResults);
        load_dev_proc(dev, get_device_proc_addr, "vkResetQueryPool", fp_vkResetQueryPool);
        load_dev_proc(dev, get_device_proc_addr, "vkResetQueryPoolEXT", fp_vkResetQueryPoolEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkGetBufferDeviceAddress", fp_vkGetBufferDeviceAddress);
        load_dev_proc(dev, get_device_proc_addr, "vkCreateSwapchainKHR", fp_vkCreateSwapchainKHR);
        load_dev_proc(dev, get_device_proc_addr, "vkDestroySwapchainKHR", fp_vkDestroySwapchainKHR);
        load_dev_proc(dev, get_device_proc_addr, "vkGetSwapchainImagesKHR", fp_vkGetSwapchainImagesKHR);
        load_dev_proc(dev, get_device_proc_addr, "vkAcquireNextImageKHR", fp_vkAcquireNextImageKHR);
        load_dev_proc(dev, get_device_proc_addr, "vkQueuePresentKHR", fp_vkQueuePresentKHR);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdPipelineBarrier2", fp_vkCmdPipelineBarrier2);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdSetEvent2", fp_vkCmdSetEvent2);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdWaitEvents2", fp_vkCmdWaitEvents2);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdResetEvent2", fp_vkCmdResetEvent2);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdPushConstants", fp_vkCmdPushConstants);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdBeginRendering", fp_vkCmdBeginRendering);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdEndRendering", fp_vkCmdEndRendering);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdBindPipeline", fp_vkCmdBindPipeline);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdSetViewport", fp_vkCmdSetViewport);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdSetScissor", fp_vkCmdSetScissor);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdClearAttachments", fp_vkCmdClearAttachments);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdSetDepthBias", fp_vkCmdSetDepthBias);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdSetStencilReference", fp_vkCmdSetStencilReference);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdSetStencilCompareMask", fp_vkCmdSetStencilCompareMask);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdSetStencilWriteMask", fp_vkCmdSetStencilWriteMask);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdBindIndexBuffer", fp_vkCmdBindIndexBuffer);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdDraw", fp_vkCmdDraw);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdDrawIndexed", fp_vkCmdDrawIndexed);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdDrawIndirect", fp_vkCmdDrawIndirect);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdDrawIndexedIndirect", fp_vkCmdDrawIndexedIndirect);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdDrawIndirectCount", fp_vkCmdDrawIndirectCount);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdDrawIndexedIndirectCount", fp_vkCmdDrawIndexedIndirectCount);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdDispatch", fp_vkCmdDispatch);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdDispatchIndirect", fp_vkCmdDispatchIndirect);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdCopyBuffer2", fp_vkCmdCopyBuffer2);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdCopyBufferToImage2", fp_vkCmdCopyBufferToImage2);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdCopyImageToBuffer2", fp_vkCmdCopyImageToBuffer2);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdBlitImage2", fp_vkCmdBlitImage2);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdWriteTimestamp2", fp_vkCmdWriteTimestamp2);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdWriteTimestamp", fp_vkCmdWriteTimestamp);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdBeginQuery", fp_vkCmdBeginQuery);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdEndQuery", fp_vkCmdEndQuery);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdResetQueryPool", fp_vkCmdResetQueryPool);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdBindDescriptorBuffersEXT", fp_vkCmdBindDescriptorBuffersEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdSetDescriptorBufferOffsetsEXT",
                      fp_vkCmdSetDescriptorBufferOffsetsEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkGetDescriptorSetLayoutSizeEXT", fp_vkGetDescriptorSetLayoutSizeEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkGetDescriptorSetLayoutBindingOffsetEXT",
                      fp_vkGetDescriptorSetLayoutBindingOffsetEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkGetDescriptorEXT", fp_vkGetDescriptorEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkGetCalibratedTimestampsEXT", fp_vkGetCalibratedTimestampsEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkGetCalibratedTimestampsKHR", fp_vkGetCalibratedTimestampsKHR);
        load_dev_proc(dev, get_device_proc_addr, "vkSetDebugUtilsObjectNameEXT", fp_vkSetDebugUtilsObjectNameEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdBeginDebugUtilsLabelEXT", fp_vkCmdBeginDebugUtilsLabelEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdEndDebugUtilsLabelEXT", fp_vkCmdEndDebugUtilsLabelEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkCmdInsertDebugUtilsLabelEXT", fp_vkCmdInsertDebugUtilsLabelEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkQueueWaitIdle", fp_vkQueueWaitIdle);
        load_dev_proc(dev, get_device_proc_addr, "vkQueueSubmit2", fp_vkQueueSubmit2);
        if (fp_vkQueueSubmit2 == nullptr)
        {
            load_dev_proc(dev, get_device_proc_addr, "vkQueueSubmit2KHR", fp_vkQueueSubmit2);
        }
        load_dev_proc(dev, get_device_proc_addr, "vkQueueBeginDebugUtilsLabelEXT", fp_vkQueueBeginDebugUtilsLabelEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkQueueEndDebugUtilsLabelEXT", fp_vkQueueEndDebugUtilsLabelEXT);
        load_dev_proc(dev, get_device_proc_addr, "vkQueueInsertDebugUtilsLabelEXT", fp_vkQueueInsertDebugUtilsLabelEXT);
    }

    auto create_instance(const context_desc& desc, logger& log) -> expected<native_instance, context_creation_error>
    {
        if (desc.api != graphics_api::vulkan)
        {
            return unexpected{.value = context_creation_error::unsupported_api};
        }

        auto* const loader_lib = load_vulkan_library();
        if (loader_lib == nullptr)
        {
            log.error("Failed to load Vulkan loader library");
            return unexpected{.value = context_creation_error::context_creation_failed};
        }

        const auto get_proc =
            reinterpret_cast<PFN_vkGetInstanceProcAddr>(get_vulkan_proc_address(loader_lib, "vkGetInstanceProcAddr"));
        if (get_proc == nullptr)
        {
            log.error("Failed to find vkGetInstanceProcAddr in Vulkan loader library");
            free_vulkan_library(loader_lib);
            return unexpected{.value = context_creation_error::context_creation_failed};
        }

        const auto fp_vkEnumerateInstanceLayerProperties = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
            get_proc(VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties"));
        const auto fp_vkEnumerateInstanceExtensionProperties =
            reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
                get_proc(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
        const auto fp_vkCreateInstance =
            reinterpret_cast<PFN_vkCreateInstance>(get_proc(VK_NULL_HANDLE, "vkCreateInstance"));

        if (fp_vkCreateInstance == nullptr)
        {
            log.error("Failed to load vkCreateInstance from vkGetInstanceProcAddr");
            free_vulkan_library(loader_lib);
            return unexpected{.value = context_creation_error::context_creation_failed};
        }

        // 1. Enumerate available layers and extensions
        auto layer_count = uint32_t{0};
        if (fp_vkEnumerateInstanceLayerProperties != nullptr)
        {
            fp_vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        }
        auto available_layers = vector<VkLayerProperties>{};
        available_layers.resize(layer_count);
        if (layer_count > 0 && fp_vkEnumerateInstanceLayerProperties != nullptr)
        {
            fp_vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
        }

        auto extension_count = uint32_t{0};
        if (fp_vkEnumerateInstanceExtensionProperties != nullptr)
        {
            fp_vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
        }
        auto available_extensions = vector<VkExtensionProperties>{};
        available_extensions.resize(extension_count);
        if (extension_count > 0 && fp_vkEnumerateInstanceExtensionProperties != nullptr)
        {
            fp_vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data());
        }

        const auto has_layer = [&available_layers](const char* name) -> bool {
            return tempest::any_of(available_layers.begin(), available_layers.end(),
                                   [name](const auto& layer) -> auto { return string_view{layer.layerName} == name; });
        };

        const auto has_extension = [&available_extensions](const char* name) -> bool {
            return tempest::any_of(
                available_extensions.begin(), available_extensions.end(),
                [name](const auto& extension) -> auto { return string_view{extension.extensionName} == name; });
        };

        auto enabled_layers = vector<const char*>{};
        auto enabled_extensions = vector<const char*>{};

        auto validation_enabled = false;
        if (desc.enable_api_validation)
        {
            constexpr const char* k_validation_layer = "VK_LAYER_KHRONOS_validation";
            if (has_layer(k_validation_layer))
            {
                enabled_layers.push_back(k_validation_layer);
                validation_enabled = true;
            }
            else
            {
                log.warn("Validation layer requested but VK_LAYER_KHRONOS_validation is unavailable");
            }
        }

        // Required surface extensions
        if (has_extension(VK_KHR_SURFACE_EXTENSION_NAME))
        {
            enabled_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        }

#ifdef TEMPEST_PLATFORM_WINDOWS
        if (has_extension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME))
        {
            enabled_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        }
#elif defined(TEMPEST_PLATFORM_LINUX)
        if (has_extension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME))
        {
            enabled_extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
        }
        if (has_extension(VK_KHR_XCB_SURFACE_EXTENSION_NAME))
        {
            enabled_extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
        }
#endif

        auto debug_utils_enabled = false;
        if (has_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            debug_utils_enabled = true;
        }

        const auto app_info = VkApplicationInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = desc.application_name.empty() ? "Tempest" : desc.application_name.data(),
            .applicationVersion = VK_MAKE_VERSION(desc.version_major, desc.version_minor, desc.version_patch),
            .pEngineName = "Tempest Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_3,
        };

        const auto debug_messenger_ci = VkDebugUtilsMessengerCreateInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .flags = 0,
            .messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_utils_messenger_callback,
            .pUserData = &log,
        };

        const auto instance_ci = VkInstanceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = (validation_enabled && debug_utils_enabled) ? &debug_messenger_ci : nullptr,
            .flags = 0,
            .pApplicationInfo = &app_info,
            .enabledLayerCount = static_cast<uint32_t>(enabled_layers.size()),
            .ppEnabledLayerNames = enabled_layers.empty() ? nullptr : enabled_layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size()),
            .ppEnabledExtensionNames = enabled_extensions.empty() ? nullptr : enabled_extensions.data(),
        };

        auto* instance = VkInstance{VK_NULL_HANDLE};
        const auto create_result = fp_vkCreateInstance(&instance_ci, nullptr, &instance);
        if (create_result != VK_SUCCESS)
        {
            free_vulkan_library(loader_lib);
            return unexpected{.value = context_creation_error::context_creation_failed};
        }

        auto result = native_instance{};
        result.instance = instance;
        result.loader_handle = loader_lib;
        result.dispatch.init(instance, get_proc);

        if (validation_enabled && debug_utils_enabled && result.dispatch.fp_vkCreateDebugUtilsMessengerEXT != nullptr)
        {
            result.dispatch.fp_vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_ci, nullptr,
                                                              &result.debug_messenger);
        }

        return result;
    }

    auto destroy_instance(native_instance& inst) -> void
    {
        if (inst.instance == VK_NULL_HANDLE)
        {
            if (inst.loader_handle != nullptr)
            {
                free_vulkan_library(inst.loader_handle);
                inst.loader_handle = nullptr;
            }
            return;
        }

        if (inst.debug_messenger != VK_NULL_HANDLE && inst.dispatch.fp_vkDestroyDebugUtilsMessengerEXT != nullptr)
        {
            inst.dispatch.fp_vkDestroyDebugUtilsMessengerEXT(inst.instance, inst.debug_messenger, nullptr);
            inst.debug_messenger = VK_NULL_HANDLE;
        }

        inst.dispatch.destroyInstance(nullptr);
        inst.instance = VK_NULL_HANDLE;

        if (inst.loader_handle != nullptr)
        {
            free_vulkan_library(inst.loader_handle);
            inst.loader_handle = nullptr;
        }
    }

    auto enumerate_physical_devices(const native_instance& inst)
        -> expected<vector<physical_device_info>, context_creation_error>
    {
        if (inst.instance == VK_NULL_HANDLE || inst.dispatch.fp_vkEnumeratePhysicalDevices == nullptr)
        {
            return unexpected{.value = context_creation_error::no_valid_devices_found};
        }

        auto physical_device_count = uint32_t{0};
        inst.dispatch.fp_vkEnumeratePhysicalDevices(inst.instance, &physical_device_count, nullptr);
        if (physical_device_count == 0)
        {
            return unexpected{.value = context_creation_error::no_valid_devices_found};
        }

        auto vk_devices = vector<VkPhysicalDevice>{};
        vk_devices.resize(physical_device_count);
        inst.dispatch.fp_vkEnumeratePhysicalDevices(inst.instance, &physical_device_count, vk_devices.data());

        auto valid_devices = vector<physical_device_info>{};

        for (auto* vk_dev : vk_devices)
        {
            auto props = VkPhysicalDeviceProperties{};
            inst.dispatch.fp_vkGetPhysicalDeviceProperties(vk_dev, &props);

            if (props.apiVersion < VK_API_VERSION_1_3)
            {
                continue;
            }

            // Check extensions
            auto ext_count = uint32_t{0};
            inst.dispatch.fp_vkEnumerateDeviceExtensionProperties(vk_dev, nullptr, &ext_count, nullptr);
            auto ext_props = vector<VkExtensionProperties>{};
            ext_props.resize(ext_count);
            inst.dispatch.fp_vkEnumerateDeviceExtensionProperties(vk_dev, nullptr, &ext_count, ext_props.data());

            auto supported_extensions = vector<string>{};
            for (const auto& ext : ext_props)
            {
                supported_extensions.emplace_back(ext.extensionName);
            }

            auto dev_has_ext = [&supported_extensions](string_view name) -> bool {
                return tempest::any_of(supported_extensions.begin(), supported_extensions.end(),
                                       [name](const auto& ext) -> auto { return string_view{ext} == name; });
            };

            if (!dev_has_ext(VK_KHR_SWAPCHAIN_EXTENSION_NAME) ||
                !dev_has_ext(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME) ||
                !dev_has_ext(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME))
            {
                continue;
            }

            // Check features
            auto feat11 = VkPhysicalDeviceVulkan11Features{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
                .pNext = nullptr,
            };
            auto feat12 = VkPhysicalDeviceVulkan12Features{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                .pNext = &feat11,
            };
            auto feat13 = VkPhysicalDeviceVulkan13Features{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                .pNext = &feat12,
            };
            auto desc_buffer_feat = VkPhysicalDeviceDescriptorBufferFeaturesEXT{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
                .pNext = &feat13,
            };
            auto interlock_feat = VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT,
                .pNext = &desc_buffer_feat,
            };
            auto features2 = VkPhysicalDeviceFeatures2{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &interlock_feat,
            };

            if (inst.dispatch.fp_vkGetPhysicalDeviceFeatures2 != nullptr)
            {
                inst.dispatch.fp_vkGetPhysicalDeviceFeatures2(vk_dev, &features2);
            }
            else
            {
                inst.dispatch.fp_vkGetPhysicalDeviceFeatures(vk_dev, &features2.features);
            }

            // Verify Vulkan 1.3 core features
            if ((feat13.dynamicRendering == 0U) || (feat13.synchronization2 == 0U) ||
                (feat12.timelineSemaphore == 0U) || (feat12.bufferDeviceAddress == 0U) ||
                (desc_buffer_feat.descriptorBuffer == 0U) || (interlock_feat.fragmentShaderPixelInterlock == 0U))
            {
                continue;
            }

            // Query queue families
            auto queue_family_count = uint32_t{0};
            inst.dispatch.fp_vkGetPhysicalDeviceQueueFamilyProperties(vk_dev, &queue_family_count, nullptr);
            auto queue_families = vector<VkQueueFamilyProperties>{};
            queue_families.resize(queue_family_count);
            inst.dispatch.fp_vkGetPhysicalDeviceQueueFamilyProperties(vk_dev, &queue_family_count,
                                                                      queue_families.data());

            // Check for graphics queue
            const auto has_graphics =
                tempest::any_of(queue_families.begin(), queue_families.end(), [](const auto& queue_family) -> auto {
                    return (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
                });

            if (!has_graphics)
            {
                continue;
            }

            auto mem_props = VkPhysicalDeviceMemoryProperties{};
            inst.dispatch.fp_vkGetPhysicalDeviceMemoryProperties(vk_dev, &mem_props);

            valid_devices.push_back(physical_device_info{
                .physical_device = vk_dev,
                .properties = props,
                .features = features2.features,
                .memory_properties = mem_props,
                .queue_families = tempest::move(queue_families),
                .supported_extensions = tempest::move(supported_extensions),
            });
        }

        if (valid_devices.empty())
        {
            return unexpected{.value = context_creation_error::no_valid_devices_found};
        }

        // Rank devices: prefer discrete GPUs
        constexpr int discrete_gpu_score = 1000;
        constexpr int integrated_gpu_score = 500;
        constexpr int virtual_gpu_score = 250;
        constexpr int cpu_score = 100;
        tempest::sort(valid_devices.begin(), valid_devices.end(),
                      [](const physical_device_info& lhs, const physical_device_info& rhs) -> bool {
                          auto score = [](VkPhysicalDeviceType type) -> int {
                              switch (type)
                              {
                              case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                                  return discrete_gpu_score;
                              case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                                  return integrated_gpu_score;
                              case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                                  return virtual_gpu_score;
                              case VK_PHYSICAL_DEVICE_TYPE_CPU:
                                  return cpu_score;
                              default:
                                  return 0;
                              }
                          };
                          return score(lhs.properties.deviceType) > score(rhs.properties.deviceType);
                      });

        return valid_devices;
    }

    namespace
    {
        auto find_graphics_queue_family(span<const VkQueueFamilyProperties> families) -> uint32_t
        {
            for (uint32_t i = 0; i < families.size(); ++i)
            {
                if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
                {
                    return i;
                }
            }
            return ~0U;
        }

        auto find_compute_queue_family(span<const VkQueueFamilyProperties> families, uint32_t fallback) -> uint32_t
        {
            for (uint32_t i = 0; i < families.size(); ++i)
            {
                if ((families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 &&
                    (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
                {
                    return i;
                }
            }
            return fallback;
        }

        auto find_transfer_queue_family(span<const VkQueueFamilyProperties> families, uint32_t fallback) -> uint32_t
        {
            for (uint32_t i = 0; i < families.size(); ++i)
            {
                if ((families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0 &&
                    (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
                    (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)
                {
                    return i;
                }
            }
            for (uint32_t i = 0; i < families.size(); ++i)
            {
                if ((families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0 &&
                    (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
                {
                    return i;
                }
            }
            return fallback;
        }

        auto find_present_queue_family(const native_instance& inst, VkPhysicalDevice physical_device,
                                       VkSurfaceKHR surface, span<const VkQueueFamilyProperties> families,
                                       uint32_t fallback) -> uint32_t
        {
            if (surface == VK_NULL_HANDLE || inst.dispatch.fp_vkGetPhysicalDeviceSurfaceSupportKHR == nullptr)
            {
                return fallback;
            }
            for (uint32_t i = 0; i < families.size(); ++i)
            {
                auto present_support = VkBool32{VK_FALSE};
                inst.dispatch.fp_vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
                if (present_support == VK_TRUE)
                {
                    return i;
                }
            }
            return fallback;
        }
    } // namespace

    auto find_queue_families(const native_instance& inst, VkPhysicalDevice physical_device, VkSurfaceKHR surface)
        -> queue_family_indices
    {
        auto queue_count = uint32_t{0};
        inst.dispatch.fp_vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, nullptr);
        auto families = vector<VkQueueFamilyProperties>{};
        families.resize(queue_count);
        if (queue_count > 0)
        {
            inst.dispatch.fp_vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, families.data());
        }

        auto indices = queue_family_indices{};
        indices.graphics = find_graphics_queue_family(families);
        indices.compute = find_compute_queue_family(families, indices.graphics);
        const auto transfer_fallback = indices.compute != ~0U ? indices.compute : indices.graphics;
        indices.transfer = find_transfer_queue_family(families, transfer_fallback);
        indices.present = find_present_queue_family(inst, physical_device, surface, families, indices.graphics);

        return indices;
    }

    auto create_device(const native_instance& inst, const physical_device_info& phys_dev,
                       span<const char* const> extra_extensions) -> expected<native_device, context_creation_error>
    {
        const auto queue_indices = find_queue_families(inst, phys_dev.physical_device);
        if (!queue_indices.is_complete())
        {
            return unexpected{context_creation_error::context_creation_failed};
        }

        // Collect unique queue families
        auto unique_queue_families = vector<uint32_t>{};
        const auto add_unique = [&unique_queue_families](uint32_t idx) -> void {
            if (idx != ~0U && tempest::find(unique_queue_families.begin(), unique_queue_families.end(), idx) ==
                                  unique_queue_families.end())
            {
                unique_queue_families.push_back(idx);
            }
        };

        add_unique(queue_indices.graphics);
        add_unique(queue_indices.compute);
        add_unique(queue_indices.transfer);

        constexpr auto queue_priority = 1.0F;
        auto queue_cis = vector<VkDeviceQueueCreateInfo>{};
        for (auto qf_index : unique_queue_families)
        {
            queue_cis.push_back(VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueFamilyIndex = qf_index,
                .queueCount = 1,
                .pQueuePriorities = &queue_priority,
            });
        }

        // Enable required device extensions
        auto extensions_to_enable = vector<const char*>{};
        extensions_to_enable.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        extensions_to_enable.push_back(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
        extensions_to_enable.push_back(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME);
        extensions_to_enable.push_back(VK_KHR_SHADER_RELAXED_EXTENDED_INSTRUCTION_EXTENSION_NAME);

        if (phys_dev.has_extension(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME))
        {
            extensions_to_enable.push_back(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
        }
        else if (phys_dev.has_extension(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME))
        {
            extensions_to_enable.push_back(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
        }

        if (phys_dev.has_extension(VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME))
        {
            extensions_to_enable.push_back(VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME);
        }

        for (const auto* const extra : extra_extensions)
        {
            if (phys_dev.has_extension(extra))
            {
                extensions_to_enable.push_back(extra);
            }
        }

        // Feature chain
        auto* pnext_chain_ptr = static_cast<void*>(nullptr); 
#if defined(TEMPEST_DEBUG_SHADERS) || defined(TEMPEST_CONFIG_DEBUG) || defined(TEMPEST_CONFIG_RELWITHDEBUGINFO)
        auto relaxed_features = VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_RELAXED_EXTENDED_INSTRUCTION_FEATURES_KHR,
            .pNext = pnext_chain_ptr,
            .shaderRelaxedExtendedInstruction = VK_TRUE,
        };

        pnext_chain_ptr = &relaxed_features;
#endif

        auto feat_interlock = VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT,
            .pNext = pnext_chain_ptr,
            .fragmentShaderPixelInterlock = VK_TRUE,
        };
        pnext_chain_ptr = &feat_interlock;

        auto feat_desc_buffer = VkPhysicalDeviceDescriptorBufferFeaturesEXT{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
            .pNext = pnext_chain_ptr,
            .descriptorBuffer = VK_TRUE,
        };
        pnext_chain_ptr = &feat_desc_buffer;

        auto feat_13 = VkPhysicalDeviceVulkan13Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = pnext_chain_ptr,
            .shaderDemoteToHelperInvocation = VK_TRUE,
            .synchronization2 = VK_TRUE,
            .dynamicRendering = VK_TRUE,
            .maintenance4 = VK_TRUE,
        };
        pnext_chain_ptr = &feat_13;

        auto feat_12 = VkPhysicalDeviceVulkan12Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = pnext_chain_ptr,
            .drawIndirectCount = VK_TRUE,
            .storageBuffer8BitAccess = VK_TRUE,
            .uniformAndStorageBuffer8BitAccess = VK_FALSE,
            .shaderFloat16 = VK_TRUE,
            .shaderUniformBufferArrayNonUniformIndexing = VK_TRUE,
            .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
            .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
            .shaderStorageImageArrayNonUniformIndexing = VK_TRUE,
            .descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
            .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
            .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
            .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
            .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
            .descriptorBindingPartiallyBound = VK_TRUE,
            .descriptorBindingVariableDescriptorCount = VK_TRUE,
            .runtimeDescriptorArray = VK_TRUE,
            .scalarBlockLayout = VK_TRUE,
            .uniformBufferStandardLayout = VK_TRUE,
            .shaderSubgroupExtendedTypes = VK_TRUE,
            .separateDepthStencilLayouts = VK_TRUE,
            .hostQueryReset = VK_TRUE,
            .timelineSemaphore = VK_TRUE,
            .bufferDeviceAddress = VK_TRUE,
            .bufferDeviceAddressCaptureReplay = VK_TRUE,
            .vulkanMemoryModel = VK_TRUE,
            .vulkanMemoryModelDeviceScope = VK_TRUE,
            .vulkanMemoryModelAvailabilityVisibilityChains = VK_TRUE,
        };
        pnext_chain_ptr = &feat_12;

        auto feat_11 = VkPhysicalDeviceVulkan11Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .pNext = pnext_chain_ptr,
            .storageBuffer16BitAccess = VK_TRUE,
            .uniformAndStorageBuffer16BitAccess = VK_TRUE,
            .shaderDrawParameters = VK_TRUE,
        };
        pnext_chain_ptr = &feat_11;

        const auto feat_10 = VkPhysicalDeviceFeatures{
            .independentBlend = VK_TRUE,
            .multiDrawIndirect = VK_TRUE,
            .depthClamp = VK_TRUE,
            .depthBiasClamp = VK_TRUE,
            .fillModeNonSolid = VK_TRUE,
            .depthBounds = VK_TRUE,
            .samplerAnisotropy = VK_TRUE,
            .pipelineStatisticsQuery = VK_TRUE,
            .fragmentStoresAndAtomics = VK_TRUE,
            .shaderUniformBufferArrayDynamicIndexing = VK_TRUE,
            .shaderSampledImageArrayDynamicIndexing = VK_TRUE,
            .shaderStorageBufferArrayDynamicIndexing = VK_TRUE,
            .shaderStorageImageArrayDynamicIndexing = VK_TRUE,
            .shaderInt64 = VK_TRUE,
            .shaderInt16 = VK_TRUE,
        };

        auto features2 = VkPhysicalDeviceFeatures2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = pnext_chain_ptr,
            .features = feat_10,
        };
        pnext_chain_ptr = &features2;

        const auto device_ci = VkDeviceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = pnext_chain_ptr,
            .flags = 0,
            .queueCreateInfoCount = static_cast<uint32_t>(queue_cis.size()),
            .pQueueCreateInfos = queue_cis.data(),
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(extensions_to_enable.size()),
            .ppEnabledExtensionNames = extensions_to_enable.data(),
            .pEnabledFeatures = nullptr,
        };

        auto* dev = VkDevice{VK_NULL_HANDLE};
        const auto res = inst.dispatch.fp_vkCreateDevice(phys_dev.physical_device, &device_ci, nullptr, &dev);
        if (res != VK_SUCCESS)
        {
            return unexpected{context_creation_error::context_creation_failed};
        }

        auto result = native_device{};
        result.device = dev;
        result.physical_device = phys_dev;
        result.queues = queue_indices;
        result.graphics_queue_index = queue_indices.graphics;
        result.compute_queue_index = queue_indices.compute;
        result.transfer_queue_index = queue_indices.transfer;
        result.fp_vkGetDeviceProcAddr = inst.dispatch.fp_vkGetDeviceProcAddr;

        result.dispatch.init(dev, inst.dispatch.fp_vkGetDeviceProcAddr);

        if (result.dispatch.fp_vkGetDeviceQueue != nullptr)
        {
            result.dispatch.fp_vkGetDeviceQueue(dev, queue_indices.graphics, 0, &result.graphics_queue);
            result.dispatch.fp_vkGetDeviceQueue(dev, queue_indices.compute, 0, &result.compute_queue);
            result.dispatch.fp_vkGetDeviceQueue(dev, queue_indices.transfer, 0, &result.transfer_queue);
        }

        return result;
    }

    auto destroy_device(native_device& dev) -> void
    {
        if (dev.device != VK_NULL_HANDLE)
        {
            dev.dispatch.destroyDevice(nullptr);
            dev.device = VK_NULL_HANDLE;
        }
    }

    auto query_swapchain_support(const instance_dispatch_table& instance_table, VkPhysicalDevice physical_device,
                                 VkSurfaceKHR surface) -> swapchain_support_details
    {
        auto details = swapchain_support_details{};
        instance_table.getPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &details.capabilities);

        auto format_count = uint32_t{0};
        instance_table.getPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
        if (format_count > 0)
        {
            details.formats.resize(format_count);
            instance_table.getPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count,
                                                              details.formats.data());
        }

        auto present_mode_count = uint32_t{0};
        instance_table.getPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
        if (present_mode_count > 0)
        {
            details.present_modes.resize(present_mode_count);
            instance_table.getPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count,
                                                                   details.present_modes.data());
        }

        return details;
    }

    auto create_swapchain(const instance_dispatch_table& inst_table, const dispatch_table& dev_table,
                          const swapchain_build_desc& desc) -> expected<native_swapchain, VkResult>
    {
        const auto support = query_swapchain_support(inst_table, desc.physical_device, desc.surface);

        // 1. Min image count
        auto image_count = tempest::max(desc.preferred_image_count, support.capabilities.minImageCount);
        if (support.capabilities.maxImageCount > 0 && image_count > support.capabilities.maxImageCount)
        {
            image_count = support.capabilities.maxImageCount;
        }

        // 2. Extent
        constexpr uint32_t undefined_extent_dimension = 0xFFFFFFFFU;
        auto extent = VkExtent2D{};
        if (support.capabilities.currentExtent.width != undefined_extent_dimension)
        {
            extent = support.capabilities.currentExtent;
        }
        else
        {
            extent.width = tempest::clamp(desc.width, support.capabilities.minImageExtent.width,
                                          support.capabilities.maxImageExtent.width);
            extent.height = tempest::clamp(desc.height, support.capabilities.minImageExtent.height,
                                           support.capabilities.maxImageExtent.height);
        }

        // 3. Surface format
        auto chosen_format = support.formats.empty()
                                 ? VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_UNORM,
                                                      .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
                                 : support.formats[0];
        for (const auto& fmt : support.formats)
        {
            if (fmt.format == desc.desired_format && fmt.colorSpace == desc.desired_color_space)
            {
                chosen_format = fmt;
                break;
            }
        }

        // 4. Present mode
        auto chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& mode : support.present_modes)
        {
            if (mode == desc.desired_present_mode)
            {
                chosen_present_mode = mode;
                break;
            }
        }

        // 5. Composite alpha
        auto composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        const auto alpha_flags = array<VkCompositeAlphaFlagBitsKHR, 4>{
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        for (auto flag : alpha_flags)
        {
            if ((support.capabilities.supportedCompositeAlpha & flag) != 0)
            {
                composite_alpha = flag;
                break;
            }
        }

        const auto swapchain_ci = VkSwapchainCreateInfoKHR{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = desc.surface,
            .minImageCount = image_count,
            .imageFormat = chosen_format.format,
            .imageColorSpace = chosen_format.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = desc.array_layers,
            .imageUsage = desc.image_usage,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = support.capabilities.currentTransform,
            .compositeAlpha = composite_alpha,
            .presentMode = chosen_present_mode,
            .clipped = VK_TRUE,
            .oldSwapchain = desc.old_swapchain,
        };

        auto* swapchain = VkSwapchainKHR{VK_NULL_HANDLE};
        const auto res = dev_table.createSwapchainKHR(&swapchain_ci, nullptr, &swapchain);
        if (res != VK_SUCCESS)
        {
            return unexpected{.value = res};
        }

        auto actual_image_count = uint32_t{0};
        dev_table.getSwapchainImagesKHR(swapchain, &actual_image_count, nullptr);
        auto images = vector<VkImage>{};
        images.resize(actual_image_count);
        dev_table.getSwapchainImagesKHR(swapchain, &actual_image_count, images.data());

        return native_swapchain{
            .swapchain = swapchain,
            .surface = desc.surface,
            .format = chosen_format.format,
            .color_space = chosen_format.colorSpace,
            .extent = extent,
            .images = tempest::move(images),
        };
    }

    auto destroy_swapchain(const dispatch_table& dev_table, VkSwapchainKHR swapchain,
                           const VkAllocationCallbacks* allocator) -> void
    {
        if (swapchain != VK_NULL_HANDLE)
        {
            dev_table.destroySwapchainKHR(swapchain, allocator);
        }
    }
} // namespace tempest::rhi::vk
