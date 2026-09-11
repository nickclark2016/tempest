#ifndef TEMPEST_RHI_VK_BOOTSTRAP_HPP
#define TEMPEST_RHI_VK_BOOTSTRAP_HPP

#ifdef TEMPEST_PLATFORM_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(TEMPEST_PLATFORM_LINUX)
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

#include <vulkan/vulkan_core.h>

namespace tempest
{
    class logger;
}

namespace tempest::rhi::vk
{
    enum class queue_type : uint8_t
    {
        present,
        graphics,
        compute,
        transfer,
    };

    struct queue_family_indices
    {
        uint32_t graphics = ~0U;
        uint32_t compute = ~0U;
        uint32_t transfer = ~0U;
        uint32_t present = ~0U;

        [[nodiscard]] auto is_complete() const noexcept -> bool
        {
            return graphics != ~0U;
        }
    };

    struct physical_device_info
    {
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties properties{};
        VkPhysicalDeviceFeatures features{};
        VkPhysicalDeviceMemoryProperties memory_properties{};
        vector<VkQueueFamilyProperties> queue_families;
        vector<string> supported_extensions;

        [[nodiscard]] auto has_extension(string_view extension_name) const noexcept -> bool
        {
            return tempest::any_of(supported_extensions.begin(), supported_extensions.end(),
                                   [extension_name](const string& ext) -> bool { return string_view{ext} == extension_name; });
        }

        [[nodiscard]] auto get_extensions() const noexcept -> span<const string>
        {
            return supported_extensions;
        }
    };

    struct TEMPEST_API instance_dispatch_table
    {
        VkInstance instance = VK_NULL_HANDLE;
        PFN_vkGetInstanceProcAddr fp_vkGetInstanceProcAddr = nullptr;
        PFN_vkDestroyInstance fp_vkDestroyInstance = nullptr;
        PFN_vkEnumeratePhysicalDevices fp_vkEnumeratePhysicalDevices = nullptr;
        PFN_vkGetPhysicalDeviceProperties fp_vkGetPhysicalDeviceProperties = nullptr;
        PFN_vkGetPhysicalDeviceProperties2 fp_vkGetPhysicalDeviceProperties2 = nullptr;
        PFN_vkGetPhysicalDeviceFeatures fp_vkGetPhysicalDeviceFeatures = nullptr;
        PFN_vkGetPhysicalDeviceFeatures2 fp_vkGetPhysicalDeviceFeatures2 = nullptr;
        PFN_vkGetPhysicalDeviceQueueFamilyProperties fp_vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
        PFN_vkGetPhysicalDeviceMemoryProperties fp_vkGetPhysicalDeviceMemoryProperties = nullptr;
        PFN_vkEnumerateDeviceExtensionProperties fp_vkEnumerateDeviceExtensionProperties = nullptr;
        PFN_vkCreateDevice fp_vkCreateDevice = nullptr;
        PFN_vkGetDeviceProcAddr fp_vkGetDeviceProcAddr = nullptr;
        PFN_vkDestroySurfaceKHR fp_vkDestroySurfaceKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceSupportKHR fp_vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fp_vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fp_vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
        PFN_vkCreateDebugUtilsMessengerEXT fp_vkCreateDebugUtilsMessengerEXT = nullptr;
        PFN_vkDestroyDebugUtilsMessengerEXT fp_vkDestroyDebugUtilsMessengerEXT = nullptr;
        void* fp_vkCreateWin32SurfaceKHR = nullptr;
        void* fp_vkCreateXlibSurfaceKHR = nullptr;
        void* fp_vkCreateXcbSurfaceKHR = nullptr;

        auto init(VkInstance inst, PFN_vkGetInstanceProcAddr get_proc_addr) noexcept -> void;

        [[nodiscard]] auto getInstanceProcAddr(const char* name) const noexcept -> PFN_vkVoidFunction
        {
            return (fp_vkGetInstanceProcAddr != nullptr) ? fp_vkGetInstanceProcAddr(instance, name) : nullptr;
        }

        auto getPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                          VkPhysicalDeviceProperties2* pProperties) const noexcept -> void
        {
            if (fp_vkGetPhysicalDeviceProperties2 != nullptr)
            {
                fp_vkGetPhysicalDeviceProperties2(physicalDevice, pProperties);
            }
        }

        auto getPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                                     VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) const noexcept
            -> VkResult
        {
            if (fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR != nullptr)
            {
                return fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, pSurfaceCapabilities);
            }
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        auto getPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                                uint32_t* pSurfaceFormatCount,
                                                VkSurfaceFormatKHR* pSurfaceFormats) const noexcept -> VkResult
        {
            if (fp_vkGetPhysicalDeviceSurfaceFormatsKHR != nullptr)
            {
                return fp_vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, pSurfaceFormatCount,
                                                               pSurfaceFormats);
            }
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        auto getPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                                     uint32_t* pPresentModeCount,
                                                     VkPresentModeKHR* pPresentModes) const noexcept -> VkResult
        {
            if (fp_vkGetPhysicalDeviceSurfacePresentModesKHR != nullptr)
            {
                return fp_vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, pPresentModeCount,
                                                                    pPresentModes);
            }
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroySurfaceKHR(VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator = nullptr) const noexcept
            -> void
        {
            if ((fp_vkDestroySurfaceKHR != nullptr) && surface != VK_NULL_HANDLE)
            {
                fp_vkDestroySurfaceKHR(instance, surface, pAllocator);
            }
        }

        auto destroyInstance(const VkAllocationCallbacks* pAllocator = nullptr) const noexcept -> void
        {
            if ((fp_vkDestroyInstance != nullptr) && instance != VK_NULL_HANDLE)
            {
                fp_vkDestroyInstance(instance, pAllocator);
            }
        }
    };

    struct TEMPEST_API dispatch_table
    {
        VkDevice device = VK_NULL_HANDLE;
        PFN_vkGetDeviceProcAddr fp_vkGetDeviceProcAddr = nullptr;
        PFN_vkDestroyDevice fp_vkDestroyDevice = nullptr;
        PFN_vkGetDeviceQueue fp_vkGetDeviceQueue = nullptr;
        PFN_vkDeviceWaitIdle fp_vkDeviceWaitIdle = nullptr;
        PFN_vkCreateCommandPool fp_vkCreateCommandPool = nullptr;
        PFN_vkDestroyCommandPool fp_vkDestroyCommandPool = nullptr;
        PFN_vkResetCommandPool fp_vkResetCommandPool = nullptr;
        PFN_vkAllocateCommandBuffers fp_vkAllocateCommandBuffers = nullptr;
        PFN_vkFreeCommandBuffers fp_vkFreeCommandBuffers = nullptr;
        PFN_vkBeginCommandBuffer fp_vkBeginCommandBuffer = nullptr;
        PFN_vkEndCommandBuffer fp_vkEndCommandBuffer = nullptr;
        PFN_vkCreateSemaphore fp_vkCreateSemaphore = nullptr;
        PFN_vkDestroySemaphore fp_vkDestroySemaphore = nullptr;
        PFN_vkGetSemaphoreCounterValue fp_vkGetSemaphoreCounterValue = nullptr;
        PFN_vkWaitSemaphores fp_vkWaitSemaphores = nullptr;
        PFN_vkSignalSemaphore fp_vkSignalSemaphore = nullptr;
        PFN_vkCreateEvent fp_vkCreateEvent = nullptr;
        PFN_vkDestroyEvent fp_vkDestroyEvent = nullptr;
        PFN_vkCreateImageView fp_vkCreateImageView = nullptr;
        PFN_vkDestroyImageView fp_vkDestroyImageView = nullptr;
        PFN_vkCreateSampler fp_vkCreateSampler = nullptr;
        PFN_vkDestroySampler fp_vkDestroySampler = nullptr;
        PFN_vkCreateShaderModule fp_vkCreateShaderModule = nullptr;
        PFN_vkDestroyShaderModule fp_vkDestroyShaderModule = nullptr;
        PFN_vkCreateGraphicsPipelines fp_vkCreateGraphicsPipelines = nullptr;
        PFN_vkCreateComputePipelines fp_vkCreateComputePipelines = nullptr;
        PFN_vkDestroyPipeline fp_vkDestroyPipeline = nullptr;
        PFN_vkCreatePipelineLayout fp_vkCreatePipelineLayout = nullptr;
        PFN_vkDestroyPipelineLayout fp_vkDestroyPipelineLayout = nullptr;
        PFN_vkCreateDescriptorSetLayout fp_vkCreateDescriptorSetLayout = nullptr;
        PFN_vkDestroyDescriptorSetLayout fp_vkDestroyDescriptorSetLayout = nullptr;
        PFN_vkCreateQueryPool fp_vkCreateQueryPool = nullptr;
        PFN_vkDestroyQueryPool fp_vkDestroyQueryPool = nullptr;
        PFN_vkGetQueryPoolResults fp_vkGetQueryPoolResults = nullptr;
        PFN_vkResetQueryPool fp_vkResetQueryPool = nullptr;
        PFN_vkResetQueryPoolEXT fp_vkResetQueryPoolEXT = nullptr;
        PFN_vkGetBufferDeviceAddress fp_vkGetBufferDeviceAddress = nullptr;
        PFN_vkCreateSwapchainKHR fp_vkCreateSwapchainKHR = nullptr;
        PFN_vkDestroySwapchainKHR fp_vkDestroySwapchainKHR = nullptr;
        PFN_vkGetSwapchainImagesKHR fp_vkGetSwapchainImagesKHR = nullptr;
        PFN_vkAcquireNextImageKHR fp_vkAcquireNextImageKHR = nullptr;
        PFN_vkQueuePresentKHR fp_vkQueuePresentKHR = nullptr;
        PFN_vkCmdPipelineBarrier2 fp_vkCmdPipelineBarrier2 = nullptr;
        PFN_vkCmdSetEvent2 fp_vkCmdSetEvent2 = nullptr;
        PFN_vkCmdWaitEvents2 fp_vkCmdWaitEvents2 = nullptr;
        PFN_vkCmdResetEvent2 fp_vkCmdResetEvent2 = nullptr;
        PFN_vkCmdPushConstants fp_vkCmdPushConstants = nullptr;
        PFN_vkCmdBeginRendering fp_vkCmdBeginRendering = nullptr;
        PFN_vkCmdEndRendering fp_vkCmdEndRendering = nullptr;
        PFN_vkCmdBindPipeline fp_vkCmdBindPipeline = nullptr;
        PFN_vkCmdSetViewport fp_vkCmdSetViewport = nullptr;
        PFN_vkCmdSetScissor fp_vkCmdSetScissor = nullptr;
        PFN_vkCmdSetDepthBias fp_vkCmdSetDepthBias = nullptr;
        PFN_vkCmdSetStencilReference fp_vkCmdSetStencilReference = nullptr;
        PFN_vkCmdSetStencilCompareMask fp_vkCmdSetStencilCompareMask = nullptr;
        PFN_vkCmdSetStencilWriteMask fp_vkCmdSetStencilWriteMask = nullptr;
        PFN_vkCmdBindIndexBuffer fp_vkCmdBindIndexBuffer = nullptr;
        PFN_vkCmdDraw fp_vkCmdDraw = nullptr;
        PFN_vkCmdDrawIndexed fp_vkCmdDrawIndexed = nullptr;
        PFN_vkCmdDrawIndirect fp_vkCmdDrawIndirect = nullptr;
        PFN_vkCmdDrawIndexedIndirect fp_vkCmdDrawIndexedIndirect = nullptr;
        PFN_vkCmdDrawIndirectCount fp_vkCmdDrawIndirectCount = nullptr;
        PFN_vkCmdDrawIndexedIndirectCount fp_vkCmdDrawIndexedIndirectCount = nullptr;
        PFN_vkCmdDispatch fp_vkCmdDispatch = nullptr;
        PFN_vkCmdDispatchIndirect fp_vkCmdDispatchIndirect = nullptr;
        PFN_vkCmdCopyBuffer2 fp_vkCmdCopyBuffer2 = nullptr;
        PFN_vkCmdCopyBufferToImage2 fp_vkCmdCopyBufferToImage2 = nullptr;
        PFN_vkCmdCopyImageToBuffer2 fp_vkCmdCopyImageToBuffer2 = nullptr;
        PFN_vkCmdBlitImage2 fp_vkCmdBlitImage2 = nullptr;
        PFN_vkCmdWriteTimestamp2 fp_vkCmdWriteTimestamp2 = nullptr;
        PFN_vkCmdWriteTimestamp fp_vkCmdWriteTimestamp = nullptr;
        PFN_vkCmdBeginQuery fp_vkCmdBeginQuery = nullptr;
        PFN_vkCmdEndQuery fp_vkCmdEndQuery = nullptr;
        PFN_vkCmdResetQueryPool fp_vkCmdResetQueryPool = nullptr;
        PFN_vkCmdBindDescriptorBuffersEXT fp_vkCmdBindDescriptorBuffersEXT = nullptr;
        PFN_vkCmdSetDescriptorBufferOffsetsEXT fp_vkCmdSetDescriptorBufferOffsetsEXT = nullptr;
        PFN_vkGetDescriptorSetLayoutSizeEXT fp_vkGetDescriptorSetLayoutSizeEXT = nullptr;
        PFN_vkGetDescriptorSetLayoutBindingOffsetEXT fp_vkGetDescriptorSetLayoutBindingOffsetEXT = nullptr;
        PFN_vkGetDescriptorEXT fp_vkGetDescriptorEXT = nullptr;
        PFN_vkGetCalibratedTimestampsEXT fp_vkGetCalibratedTimestampsEXT = nullptr;
        PFN_vkGetCalibratedTimestampsKHR fp_vkGetCalibratedTimestampsKHR = nullptr;
        PFN_vkSetDebugUtilsObjectNameEXT fp_vkSetDebugUtilsObjectNameEXT = nullptr;
        PFN_vkCmdBeginDebugUtilsLabelEXT fp_vkCmdBeginDebugUtilsLabelEXT = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT fp_vkCmdEndDebugUtilsLabelEXT = nullptr;
        PFN_vkCmdInsertDebugUtilsLabelEXT fp_vkCmdInsertDebugUtilsLabelEXT = nullptr;
        PFN_vkQueueWaitIdle fp_vkQueueWaitIdle = nullptr;
        PFN_vkQueueSubmit2 fp_vkQueueSubmit2 = nullptr;
        PFN_vkQueueBeginDebugUtilsLabelEXT fp_vkQueueBeginDebugUtilsLabelEXT = nullptr;
        PFN_vkQueueEndDebugUtilsLabelEXT fp_vkQueueEndDebugUtilsLabelEXT = nullptr;
        PFN_vkQueueInsertDebugUtilsLabelEXT fp_vkQueueInsertDebugUtilsLabelEXT = nullptr;

        auto init(VkDevice dev, PFN_vkGetDeviceProcAddr get_device_proc_addr) noexcept -> void;

        auto destroyDevice(const VkAllocationCallbacks* pAllocator = nullptr) const noexcept -> void
        {
            if ((fp_vkDestroyDevice != nullptr) && device != VK_NULL_HANDLE)
            {
                fp_vkDestroyDevice(device, pAllocator);
            }
        }

        [[nodiscard]] auto deviceWaitIdle() const noexcept -> VkResult
        {
            return (fp_vkDeviceWaitIdle != nullptr) ? fp_vkDeviceWaitIdle(device) : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto waitSemaphores(const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) const noexcept -> VkResult
        {
            return (fp_vkWaitSemaphores != nullptr) ? fp_vkWaitSemaphores(device, pWaitInfo, timeout)
                                                    : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto signalSemaphore(const VkSemaphoreSignalInfo* pSignalInfo) const noexcept -> VkResult
        {
            return (fp_vkSignalSemaphore != nullptr) ? fp_vkSignalSemaphore(device, pSignalInfo)
                                                     : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto getSemaphoreCounterValue(VkSemaphore semaphore, uint64_t* pValue) const noexcept -> VkResult
        {
            return (fp_vkGetSemaphoreCounterValue != nullptr) ? fp_vkGetSemaphoreCounterValue(device, semaphore, pValue)
                                                              : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto createImageView(const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator,
                             VkImageView* pView) const noexcept -> VkResult
        {
            return (fp_vkCreateImageView != nullptr) ? fp_vkCreateImageView(device, pCreateInfo, pAllocator, pView)
                                                     : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroyImageView(VkImageView imageView, const VkAllocationCallbacks* pAllocator = nullptr) const noexcept
            -> void
        {
            if (fp_vkDestroyImageView != nullptr)
            {
                fp_vkDestroyImageView(device, imageView, pAllocator);
            }
        }

        auto createSampler(const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator,
                           VkSampler* pSampler) const noexcept -> VkResult
        {
            return (fp_vkCreateSampler != nullptr) ? fp_vkCreateSampler(device, pCreateInfo, pAllocator, pSampler)
                                                   : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroySampler(VkSampler sampler, const VkAllocationCallbacks* pAllocator = nullptr) const noexcept -> void
        {
            if (fp_vkDestroySampler != nullptr)
            {
                fp_vkDestroySampler(device, sampler, pAllocator);
            }
        }

        auto createShaderModule(const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator,
                                VkShaderModule* pShaderModule) const noexcept -> VkResult
        {
            return (fp_vkCreateShaderModule != nullptr)
                       ? fp_vkCreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroyShaderModule(VkShaderModule shaderModule,
                                 const VkAllocationCallbacks* pAllocator = nullptr) const noexcept -> void
        {
            if (fp_vkDestroyShaderModule != nullptr)
            {
                fp_vkDestroyShaderModule(device, shaderModule, pAllocator);
            }
        }

        auto createGraphicsPipelines(VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                     const VkGraphicsPipelineCreateInfo* pCreateInfos,
                                     const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) const noexcept
            -> VkResult
        {
            return (fp_vkCreateGraphicsPipelines != nullptr)
                       ? fp_vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator,
                                                      pPipelines)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto createComputePipelines(VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                    const VkComputePipelineCreateInfo* pCreateInfos,
                                    const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) const noexcept
            -> VkResult
        {
            return (fp_vkCreateComputePipelines != nullptr)
                       ? fp_vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator,
                                                     pPipelines)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroyPipeline(VkPipeline pipeline, const VkAllocationCallbacks* pAllocator = nullptr) const noexcept
            -> void
        {
            if (fp_vkDestroyPipeline != nullptr)
            {
                fp_vkDestroyPipeline(device, pipeline, pAllocator);
            }
        }

        auto createPipelineLayout(const VkPipelineLayoutCreateInfo* pCreateInfo,
                                  const VkAllocationCallbacks* pAllocator,
                                  VkPipelineLayout* pPipelineLayout) const noexcept -> VkResult
        {
            return (fp_vkCreatePipelineLayout != nullptr)
                       ? fp_vkCreatePipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroyPipelineLayout(VkPipelineLayout pipelineLayout,
                                   const VkAllocationCallbacks* pAllocator = nullptr) const noexcept -> void
        {
            if (fp_vkDestroyPipelineLayout != nullptr)
            {
                fp_vkDestroyPipelineLayout(device, pipelineLayout, pAllocator);
            }
        }

        auto createDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
                                       const VkAllocationCallbacks* pAllocator,
                                       VkDescriptorSetLayout* pSetLayout) const noexcept -> VkResult
        {
            return (fp_vkCreateDescriptorSetLayout != nullptr)
                       ? fp_vkCreateDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroyDescriptorSetLayout(VkDescriptorSetLayout descriptorSetLayout,
                                        const VkAllocationCallbacks* pAllocator = nullptr) const noexcept -> void
        {
            if (fp_vkDestroyDescriptorSetLayout != nullptr)
            {
                fp_vkDestroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
            }
        }

        auto createEvent(const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator,
                         VkEvent* pEvent) const noexcept -> VkResult
        {
            return (fp_vkCreateEvent != nullptr) ? fp_vkCreateEvent(device, pCreateInfo, pAllocator, pEvent)
                                                 : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroyEvent(VkEvent event, const VkAllocationCallbacks* pAllocator = nullptr) const noexcept -> void
        {
            if (fp_vkDestroyEvent != nullptr)
            {
                fp_vkDestroyEvent(device, event, pAllocator);
            }
        }

        auto createSemaphore(const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator,
                             VkSemaphore* pSemaphore) const noexcept -> VkResult
        {
            return (fp_vkCreateSemaphore != nullptr) ? fp_vkCreateSemaphore(device, pCreateInfo, pAllocator, pSemaphore)
                                                     : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroySemaphore(VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator = nullptr) const noexcept
            -> void
        {
            if (fp_vkDestroySemaphore != nullptr)
            {
                fp_vkDestroySemaphore(device, semaphore, pAllocator);
            }
        }

        auto createQueryPool(const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator,
                             VkQueryPool* pQueryPool) const noexcept -> VkResult
        {
            return (fp_vkCreateQueryPool != nullptr) ? fp_vkCreateQueryPool(device, pCreateInfo, pAllocator, pQueryPool)
                                                     : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroyQueryPool(VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator = nullptr) const noexcept
            -> void
        {
            if (fp_vkDestroyQueryPool != nullptr)
            {
                fp_vkDestroyQueryPool(device, queryPool, pAllocator);
            }
        }

        auto getQueryPoolResults(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize,
                                 void* pData, VkDeviceSize stride, VkQueryResultFlags flags) const noexcept -> VkResult
        {
            return (fp_vkGetQueryPoolResults != nullptr)
                       ? fp_vkGetQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData, stride,
                                                  flags)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto resetQueryPool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) const noexcept -> void
        {
            if (fp_vkResetQueryPool != nullptr)
            {
                fp_vkResetQueryPool(device, queryPool, firstQuery, queryCount);
            }
        }

        auto resetQueryPoolEXT(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) const noexcept -> void
        {
            if (fp_vkResetQueryPoolEXT != nullptr)
            {
                fp_vkResetQueryPoolEXT(device, queryPool, firstQuery, queryCount);
            }
        }

        auto getBufferDeviceAddress(const VkBufferDeviceAddressInfo* pInfo) const noexcept -> VkDeviceAddress
        {
            return (fp_vkGetBufferDeviceAddress != nullptr) ? fp_vkGetBufferDeviceAddress(device, pInfo) : 0;
        }

        auto createSwapchainKHR(const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator,
                                VkSwapchainKHR* pSwapchain) const noexcept -> VkResult
        {
            return (fp_vkCreateSwapchainKHR != nullptr)
                       ? fp_vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroySwapchainKHR(VkSwapchainKHR swapchain,
                                 const VkAllocationCallbacks* pAllocator = nullptr) const noexcept -> void
        {
            if ((fp_vkDestroySwapchainKHR != nullptr) && swapchain != VK_NULL_HANDLE)
            {
                fp_vkDestroySwapchainKHR(device, swapchain, pAllocator);
            }
        }

        auto getSwapchainImagesKHR(VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount,
                                   VkImage* pSwapchainImages) const noexcept -> VkResult
        {
            return (fp_vkGetSwapchainImagesKHR != nullptr)
                       ? fp_vkGetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto acquireNextImageKHR(VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence,
                                 uint32_t* pImageIndex) const noexcept -> VkResult
        {
            return (fp_vkAcquireNextImageKHR != nullptr)
                       ? fp_vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto queuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) const noexcept -> VkResult
        {
            return (fp_vkQueuePresentKHR != nullptr) ? fp_vkQueuePresentKHR(queue, pPresentInfo)
                                                     : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto createCommandPool(const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator,
                               VkCommandPool* pCommandPool) const noexcept -> VkResult
        {
            return (fp_vkCreateCommandPool != nullptr)
                       ? fp_vkCreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto destroyCommandPool(VkCommandPool commandPool,
                                const VkAllocationCallbacks* pAllocator = nullptr) const noexcept -> void
        {
            if (fp_vkDestroyCommandPool != nullptr)
            {
                fp_vkDestroyCommandPool(device, commandPool, pAllocator);
            }
        }

        auto resetCommandPool(VkCommandPool commandPool, VkCommandPoolResetFlags flags) const noexcept -> VkResult
        {
            return (fp_vkResetCommandPool != nullptr) ? fp_vkResetCommandPool(device, commandPool, flags)
                                                      : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto allocateCommandBuffers(const VkCommandBufferAllocateInfo* pAllocateInfo,
                                    VkCommandBuffer* pCommandBuffers) const noexcept -> VkResult
        {
            return (fp_vkAllocateCommandBuffers != nullptr)
                       ? fp_vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto freeCommandBuffers(VkCommandPool commandPool, uint32_t commandBufferCount,
                                const VkCommandBuffer* pCommandBuffers) const noexcept -> void
        {
            if (fp_vkFreeCommandBuffers != nullptr)
            {
                fp_vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
            }
        }

        auto beginCommandBuffer(VkCommandBuffer commandBuffer,
                                const VkCommandBufferBeginInfo* pBeginInfo) const noexcept -> VkResult
        {
            return (fp_vkBeginCommandBuffer != nullptr) ? fp_vkBeginCommandBuffer(commandBuffer, pBeginInfo)
                                                        : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto endCommandBuffer(VkCommandBuffer commandBuffer) const noexcept -> VkResult
        {
            return (fp_vkEndCommandBuffer != nullptr) ? fp_vkEndCommandBuffer(commandBuffer)
                                                      : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto cmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) const noexcept
            -> void
        {
            if (fp_vkCmdPipelineBarrier2 != nullptr)
            {
                fp_vkCmdPipelineBarrier2(commandBuffer, pDependencyInfo);
            }
        }

        auto cmdSetEvent2(VkCommandBuffer commandBuffer, VkEvent event,
                          const VkDependencyInfo* pDependencyInfo) const noexcept -> void
        {
            if (fp_vkCmdSetEvent2 != nullptr)
            {
                fp_vkCmdSetEvent2(commandBuffer, event, pDependencyInfo);
            }
        }

        auto cmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents,
                            const VkDependencyInfo* pDependencyInfos) const noexcept -> void
        {
            if (fp_vkCmdWaitEvents2 != nullptr)
            {
                fp_vkCmdWaitEvents2(commandBuffer, eventCount, pEvents, pDependencyInfos);
            }
        }

        auto cmdResetEvent2(VkCommandBuffer commandBuffer, VkEvent event,
                            VkPipelineStageFlags2 stageMask) const noexcept -> void
        {
            if (fp_vkCmdResetEvent2 != nullptr)
            {
                fp_vkCmdResetEvent2(commandBuffer, event, stageMask);
            }
        }

        auto cmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags,
                              uint32_t offset, uint32_t size, const void* pValues) const noexcept -> void
        {
            if (fp_vkCmdPushConstants != nullptr)
            {
                fp_vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
            }
        }

        auto cmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) const noexcept
            -> void
        {
            if (fp_vkCmdBeginRendering != nullptr)
            {
                fp_vkCmdBeginRendering(commandBuffer, pRenderingInfo);
            }
        }

        auto cmdEndRendering(VkCommandBuffer commandBuffer) const noexcept -> void
        {
            if (fp_vkCmdEndRendering != nullptr)
            {
                fp_vkCmdEndRendering(commandBuffer);
            }
        }

        auto cmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
                             VkPipeline pipeline) const noexcept -> void
        {
            if (fp_vkCmdBindPipeline != nullptr)
            {
                fp_vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
            }
        }

        auto cmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount,
                            const VkViewport* pViewports) const noexcept -> void
        {
            if (fp_vkCmdSetViewport != nullptr)
            {
                fp_vkCmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
            }
        }

        auto cmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount,
                           const VkRect2D* pScissors) const noexcept -> void
        {
            if (fp_vkCmdSetScissor != nullptr)
            {
                fp_vkCmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
            }
        }

        auto cmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp,
                             float depthBiasSlopeFactor) const noexcept -> void
        {
            if (fp_vkCmdSetDepthBias != nullptr)
            {
                fp_vkCmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
            }
        }

        auto cmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask,
                                    uint32_t reference) const noexcept -> void
        {
            if (fp_vkCmdSetStencilReference != nullptr)
            {
                fp_vkCmdSetStencilReference(commandBuffer, faceMask, reference);
            }
        }

        auto cmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask,
                                      uint32_t compareMask) const noexcept -> void
        {
            if (fp_vkCmdSetStencilCompareMask != nullptr)
            {
                fp_vkCmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
            }
        }

        auto cmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask,
                                    uint32_t writeMask) const noexcept -> void
        {
            if (fp_vkCmdSetStencilWriteMask != nullptr)
            {
                fp_vkCmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
            }
        }

        auto cmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
                                VkIndexType indexType) const noexcept -> void
        {
            if (fp_vkCmdBindIndexBuffer != nullptr)
            {
                fp_vkCmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
            }
        }

        auto cmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
                     uint32_t firstInstance) const noexcept -> void
        {
            if (fp_vkCmdDraw != nullptr)
            {
                fp_vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
            }
        }

        auto cmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount,
                            uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) const noexcept -> void
        {
            if (fp_vkCmdDrawIndexed != nullptr)
            {
                fp_vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
            }
        }

        auto cmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount,
                             uint32_t stride) const noexcept -> void
        {
            if (fp_vkCmdDrawIndirect != nullptr)
            {
                fp_vkCmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
            }
        }

        auto cmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
                                    uint32_t drawCount, uint32_t stride) const noexcept -> void
        {
            if (fp_vkCmdDrawIndexedIndirect != nullptr)
            {
                fp_vkCmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
            }
        }

        auto cmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
                                  VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                  uint32_t stride) const noexcept -> void
        {
            if (fp_vkCmdDrawIndirectCount != nullptr)
            {
                fp_vkCmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount,
                                          stride);
            }
        }

        auto cmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
                                         VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                         uint32_t stride) const noexcept -> void
        {
            if (fp_vkCmdDrawIndexedIndirectCount != nullptr)
            {
                fp_vkCmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset,
                                                 maxDrawCount, stride);
            }
        }

        auto cmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY,
                         uint32_t groupCountZ) const noexcept -> void
        {
            if (fp_vkCmdDispatch != nullptr)
            {
                fp_vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
            }
        }

        auto cmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) const noexcept
            -> void
        {
            if (fp_vkCmdDispatchIndirect != nullptr)
            {
                fp_vkCmdDispatchIndirect(commandBuffer, buffer, offset);
            }
        }

        auto cmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) const noexcept
            -> void
        {
            if (fp_vkCmdCopyBuffer2 != nullptr)
            {
                fp_vkCmdCopyBuffer2(commandBuffer, pCopyBufferInfo);
            }
        }

        auto cmdCopyBufferToImage2(VkCommandBuffer commandBuffer,
                                   const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) const noexcept -> void
        {
            if (fp_vkCmdCopyBufferToImage2 != nullptr)
            {
                fp_vkCmdCopyBufferToImage2(commandBuffer, pCopyBufferToImageInfo);
            }
        }

        auto cmdCopyImageToBuffer2(VkCommandBuffer commandBuffer,
                                   const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) const noexcept -> void
        {
            if (fp_vkCmdCopyImageToBuffer2 != nullptr)
            {
                fp_vkCmdCopyImageToBuffer2(commandBuffer, pCopyImageToBufferInfo);
            }
        }

        auto cmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) const noexcept -> void
        {
            if (fp_vkCmdBlitImage2 != nullptr)
            {
                fp_vkCmdBlitImage2(commandBuffer, pBlitImageInfo);
            }
        }

        auto cmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool,
                                uint32_t query) const noexcept -> void
        {
            if (fp_vkCmdWriteTimestamp2 != nullptr)
            {
                fp_vkCmdWriteTimestamp2(commandBuffer, stage, queryPool, query);
            }
        }

        auto cmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage,
                               VkQueryPool queryPool, uint32_t query) const noexcept -> void
        {
            if (fp_vkCmdWriteTimestamp != nullptr)
            {
                fp_vkCmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
            }
        }

        auto cmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query,
                           VkQueryControlFlags flags) const noexcept -> void
        {
            if (fp_vkCmdBeginQuery != nullptr)
            {
                fp_vkCmdBeginQuery(commandBuffer, queryPool, query, flags);
            }
        }

        auto cmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) const noexcept -> void
        {
            if (fp_vkCmdEndQuery != nullptr)
            {
                fp_vkCmdEndQuery(commandBuffer, queryPool, query);
            }
        }

        auto cmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery,
                               uint32_t queryCount) const noexcept -> void
        {
            if (fp_vkCmdResetQueryPool != nullptr)
            {
                fp_vkCmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
            }
        }

        auto cmdBindDescriptorBuffersEXT(VkCommandBuffer commandBuffer, uint32_t bufferCount,
                                         const VkDescriptorBufferBindingInfoEXT* pBindingInfos) const noexcept -> void
        {
            if (fp_vkCmdBindDescriptorBuffersEXT != nullptr)
            {
                fp_vkCmdBindDescriptorBuffersEXT(commandBuffer, bufferCount, pBindingInfos);
            }
        }

        auto cmdSetDescriptorBufferOffsetsEXT(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
                                              VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount,
                                              const uint32_t* pBufferIndices,
                                              const VkDeviceSize* pOffsets) const noexcept -> void
        {
            if (fp_vkCmdSetDescriptorBufferOffsetsEXT != nullptr)
            {
                fp_vkCmdSetDescriptorBufferOffsetsEXT(commandBuffer, pipelineBindPoint, layout, firstSet, setCount,
                                                      pBufferIndices, pOffsets);
            }
        }

        auto getDescriptorSetLayoutSizeEXT(VkDescriptorSetLayout layout,
                                           VkDeviceSize* pLayoutSizeInBytes) const noexcept -> void
        {
            if (fp_vkGetDescriptorSetLayoutSizeEXT != nullptr)
            {
                fp_vkGetDescriptorSetLayoutSizeEXT(device, layout, pLayoutSizeInBytes);
            }
        }

        auto getDescriptorSetLayoutBindingOffsetEXT(VkDescriptorSetLayout layout, uint32_t binding,
                                                    VkDeviceSize* pOffset) const noexcept -> void
        {
            if (fp_vkGetDescriptorSetLayoutBindingOffsetEXT != nullptr)
            {
                fp_vkGetDescriptorSetLayoutBindingOffsetEXT(device, layout, binding, pOffset);
            }
        }

        auto getDescriptorEXT(const VkDescriptorGetInfoEXT* pDescriptorInfo, size_t dataSize,
                              void* pDescriptor) const noexcept -> void
        {
            if (fp_vkGetDescriptorEXT != nullptr)
            {
                fp_vkGetDescriptorEXT(device, pDescriptorInfo, dataSize, pDescriptor);
            }
        }

        auto getCalibratedTimestampsEXT(uint32_t timestampCount, const VkCalibratedTimestampInfoEXT* pTimestampInfos,
                                        uint64_t* pTimestamps, uint64_t* pMaxDeviation) const noexcept -> VkResult
        {
            return (fp_vkGetCalibratedTimestampsEXT != nullptr)
                       ? fp_vkGetCalibratedTimestampsEXT(device, timestampCount, pTimestampInfos, pTimestamps,
                                                         pMaxDeviation)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto getCalibratedTimestampsKHR(uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos,
                                        uint64_t* pTimestamps, uint64_t* pMaxDeviation) const noexcept -> VkResult
        {
            return (fp_vkGetCalibratedTimestampsKHR != nullptr)
                       ? fp_vkGetCalibratedTimestampsKHR(device, timestampCount, pTimestampInfos, pTimestamps,
                                                         pMaxDeviation)
                       : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto setDebugUtilsObjectNameEXT(const VkDebugUtilsObjectNameInfoEXT* pNameInfo) const noexcept -> VkResult
        {
            return (fp_vkSetDebugUtilsObjectNameEXT != nullptr) ? fp_vkSetDebugUtilsObjectNameEXT(device, pNameInfo)
                                                                : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto cmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer,
                                        const VkDebugUtilsLabelEXT* pLabelInfo) const noexcept -> void
        {
            if (fp_vkCmdBeginDebugUtilsLabelEXT != nullptr)
            {
                fp_vkCmdBeginDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
            }
        }

        auto cmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer) const noexcept -> void
        {
            if (fp_vkCmdEndDebugUtilsLabelEXT != nullptr)
            {
                fp_vkCmdEndDebugUtilsLabelEXT(commandBuffer);
            }
        }

        auto cmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer,
                                         const VkDebugUtilsLabelEXT* pLabelInfo) const noexcept -> void
        {
            if (fp_vkCmdInsertDebugUtilsLabelEXT != nullptr)
            {
                fp_vkCmdInsertDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
            }
        }

        auto queueWaitIdle(VkQueue queue) const noexcept -> VkResult
        {
            return (fp_vkQueueWaitIdle != nullptr) ? fp_vkQueueWaitIdle(queue) : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto queueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits,
                          VkFence fence) const noexcept -> VkResult
        {
            return (fp_vkQueueSubmit2 != nullptr) ? fp_vkQueueSubmit2(queue, submitCount, pSubmits, fence)
                                                  : VK_ERROR_INITIALIZATION_FAILED;
        }

        auto queueBeginDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) const noexcept -> void
        {
            if (fp_vkQueueBeginDebugUtilsLabelEXT != nullptr)
            {
                fp_vkQueueBeginDebugUtilsLabelEXT(queue, pLabelInfo);
            }
        }

        auto queueEndDebugUtilsLabelEXT(VkQueue queue) const noexcept -> void
        {
            if (fp_vkQueueEndDebugUtilsLabelEXT != nullptr)
            {
                fp_vkQueueEndDebugUtilsLabelEXT(queue);
            }
        }

        auto queueInsertDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) const noexcept -> void
        {
            if (fp_vkQueueInsertDebugUtilsLabelEXT != nullptr)
            {
                fp_vkQueueInsertDebugUtilsLabelEXT(queue, pLabelInfo);
            }
        }
    };

    struct native_instance
    {
        VkInstance instance = VK_NULL_HANDLE;
        instance_dispatch_table dispatch{};
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
        void* loader_handle = nullptr;
    };

    struct native_device
    {
        VkDevice device = VK_NULL_HANDLE;
        physical_device_info physical_device{};
        queue_family_indices queues{};
        VkQueue graphics_queue = VK_NULL_HANDLE;
        VkQueue compute_queue = VK_NULL_HANDLE;
        VkQueue transfer_queue = VK_NULL_HANDLE;
        uint32_t graphics_queue_index = ~0U;
        uint32_t compute_queue_index = ~0U;
        uint32_t transfer_queue_index = ~0U;
        dispatch_table dispatch{};
        PFN_vkGetDeviceProcAddr fp_vkGetDeviceProcAddr = nullptr;

        [[nodiscard]] auto get_queue(queue_type type) const noexcept -> optional<VkQueue>
        {
            switch (type)
            {
            case queue_type::graphics:
            case queue_type::present:
                return graphics_queue != VK_NULL_HANDLE ? optional<VkQueue>{graphics_queue} : nullopt;
            case queue_type::compute:
                return compute_queue != VK_NULL_HANDLE ? optional<VkQueue>{compute_queue} : nullopt;
            case queue_type::transfer:
                return transfer_queue != VK_NULL_HANDLE ? optional<VkQueue>{transfer_queue} : nullopt;
            }
            return nullopt;
        }

        [[nodiscard]] auto get_queue_index(queue_type type) const noexcept -> optional<uint32_t>
        {
            switch (type)
            {
            case queue_type::graphics:
            case queue_type::present:
                return graphics_queue_index != ~0U ? optional<uint32_t>{graphics_queue_index} : nullopt;
            case queue_type::compute:
                return compute_queue_index != ~0U ? optional<uint32_t>{compute_queue_index} : nullopt;
            case queue_type::transfer:
                return transfer_queue_index != ~0U ? optional<uint32_t>{transfer_queue_index} : nullopt;
            }
            return nullopt;
        }
    };

    struct swapchain_support_details
    {
        VkSurfaceCapabilitiesKHR capabilities{}; // NOLINT(bugprone-invalid-enum-default-initialization)
        vector<VkSurfaceFormatKHR> formats;
        vector<VkPresentModeKHR> present_modes;
    };

    struct swapchain_build_desc
    {
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t preferred_image_count = 3;
        VkFormat desired_format = VK_FORMAT_B8G8R8A8_UNORM;
        VkColorSpaceKHR desired_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkPresentModeKHR desired_present_mode = VK_PRESENT_MODE_FIFO_KHR;
        VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkSwapchainKHR old_swapchain = VK_NULL_HANDLE;
        uint32_t array_layers = 1;
    };

    struct native_swapchain
    {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkExtent2D extent = {.width = 0, .height = 0};
        vector<VkImage> images;
    };

    TEMPEST_API auto create_instance(const context_desc& desc, logger& log)
        -> expected<native_instance, context_creation_error>;
    TEMPEST_API auto destroy_instance(native_instance& inst) -> void;

    TEMPEST_API auto enumerate_physical_devices(const native_instance& inst)
        -> expected<vector<physical_device_info>, context_creation_error>;

    TEMPEST_API auto find_queue_families(const native_instance& inst, VkPhysicalDevice physical_device,
                                         VkSurfaceKHR surface = VK_NULL_HANDLE) -> queue_family_indices;

    TEMPEST_API auto create_device(const native_instance& inst, const physical_device_info& phys_dev,
                                   span<const char* const> extra_extensions = {})
        -> expected<native_device, context_creation_error>;
    TEMPEST_API auto destroy_device(native_device& dev) -> void;

    TEMPEST_API auto query_swapchain_support(const instance_dispatch_table& instance_table,
                                             VkPhysicalDevice physical_device, VkSurfaceKHR surface)
        -> swapchain_support_details;

    TEMPEST_API auto create_swapchain(const instance_dispatch_table& inst_table, const dispatch_table& dev_table,
                                      const swapchain_build_desc& desc) -> expected<native_swapchain, VkResult>;

    TEMPEST_API auto destroy_swapchain(const dispatch_table& dev_table, VkSwapchainKHR swapchain,
                                       const VkAllocationCallbacks* allocator = nullptr) -> void;
} // namespace tempest::rhi::vk

#endif // TEMPEST_RHI_VK_BOOTSTRAP_HPP
