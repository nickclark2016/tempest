#include "vk_render_device.hpp"

#include "../../windowing/glfw_window.hpp"

#include <tempest/logger.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tempest::graphics::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::vk::render_device"}});

        [[maybe_unused]] VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                      [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                      [[maybe_unused]] void* pUserData)
        {
            if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            {
                logger->error("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            {
                logger->warn("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            {
                logger->info("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            {
                logger->debug("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else
            {
                logger->debug("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }

            return VK_FALSE;
        }

        vkb::Instance build_instance()
        {
            vkb::InstanceBuilder bldr = vkb::InstanceBuilder()
                                            .set_app_name("Tempest Application")
                                            .set_app_version(1, 0, 0)
                                            .set_engine_name("Tempest Engine")
                                            .set_engine_version(1, 0, 0)
                                            .require_api_version(1, 3, 0);

#ifdef _DEBUG
            bldr.enable_validation_layers(true)
                .set_debug_callback(debug_callback)
                .add_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                .add_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
                .add_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
                .add_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
                .add_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
                .add_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
                .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
                .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
#ifdef TEMPEST_ENABLE_GPU_ASSISTED_VALDATION
            bldr.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
#endif
#endif

            auto result = bldr.build();

            return result.value();
        }

        enum class queue_match
        {
            FULL,
            PARTIAL,
            NONE,
        };

        constexpr VkImageType to_vulkan(image_type type)
        {
            switch (type)
            {
            case image_type::IMAGE_1D:
                [[fallthrough]];
            case image_type::IMAGE_1D_ARRAY:
                return VK_IMAGE_TYPE_1D;
            case image_type::IMAGE_2D:
                [[fallthrough]];
            case image_type::IMAGE_2D_ARRAY:
                [[fallthrough]];
            case image_type::IMAGE_CUBE_MAP:
                [[fallthrough]];
            case image_type::IMAGE_CUBE_MAP_ARRAY:
                return VK_IMAGE_TYPE_2D;
            case image_type::IMAGE_3D:
                return VK_IMAGE_TYPE_3D;
            }

            logger->critical("Logic Error: Failed to determine proper VkImageType. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr VkImageViewType to_vulkan_view(image_type type)
        {
            switch (type)
            {
            case image_type::IMAGE_1D:
                return VK_IMAGE_VIEW_TYPE_1D;
            case image_type::IMAGE_1D_ARRAY:
                return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            case image_type::IMAGE_2D:
                return VK_IMAGE_VIEW_TYPE_2D;
            case image_type::IMAGE_2D_ARRAY:
                return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            case image_type::IMAGE_CUBE_MAP:
                return VK_IMAGE_VIEW_TYPE_CUBE;
            case image_type::IMAGE_CUBE_MAP_ARRAY:
                return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            case image_type::IMAGE_3D:
                return VK_IMAGE_VIEW_TYPE_3D;
            }

            logger->critical("Logic Error: Failed to determine proper VkImageViewType. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr VkFormat to_vulkan(resource_format fmt)
        {
            switch (fmt)
            {
            case resource_format::R8_UNORM:
                return VK_FORMAT_R8_UNORM;
            case resource_format::RGBA8_UINT:
                return VK_FORMAT_R8G8B8A8_UINT;
            case resource_format::RGBA8_UNORM:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case resource_format::RGBA8_SRGB:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case resource_format::BGRA8_SRGB:
                return VK_FORMAT_B8G8R8A8_SRGB;
            case resource_format::RGBA16_FLOAT:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case resource_format::RG16_FLOAT:
                return VK_FORMAT_R16G16_SFLOAT;
            case resource_format::RGBA16_UNORM:
                return VK_FORMAT_R16G16B16A16_UNORM;
            case resource_format::R32_FLOAT:
                return VK_FORMAT_R32_SFLOAT;
            case resource_format::R32_UINT:
                return VK_FORMAT_R32_UINT;
            case resource_format::RG32_FLOAT:
                return VK_FORMAT_R32G32_SFLOAT;
            case resource_format::RG32_UINT:
                return VK_FORMAT_R32G32_UINT;
            case resource_format::RGB32_FLOAT:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case resource_format::RGBA32_FLOAT:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case resource_format::D24_FLOAT:
                return VK_FORMAT_X8_D24_UNORM_PACK32;
            case resource_format::D24_S8_FLOAT:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case resource_format::D32_FLOAT:
                return VK_FORMAT_D32_SFLOAT;
            case resource_format::UNKNOWN:
                return VK_FORMAT_UNDEFINED;
            }

            logger->critical("Logic Error: Failed to determine proper VkFormat. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr size_t get_format_size(resource_format fmt)
        {
            return bytes_per_element(fmt);
        }

        constexpr VkSampleCountFlagBits to_vulkan(sample_count samples)
        {
            switch (samples)
            {
            case sample_count::COUNT_1:
                return VK_SAMPLE_COUNT_1_BIT;
            case sample_count::COUNT_2:
                return VK_SAMPLE_COUNT_2_BIT;
            case sample_count::COUNT_4:
                return VK_SAMPLE_COUNT_4_BIT;
            case sample_count::COUNT_8:
                return VK_SAMPLE_COUNT_8_BIT;
            case sample_count::COUNT_16:
                return VK_SAMPLE_COUNT_16_BIT;
            }

            logger->critical("Logic Error: Failed to determine proper VkSampleCountFlagBits. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr VkDescriptorType to_vulkan(descriptor_binding_type type)
        {
            switch (type)
            {
            case descriptor_binding_type::STRUCTURED_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            case descriptor_binding_type::CONSTANT_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            case descriptor_binding_type::STORAGE_IMAGE:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case descriptor_binding_type::SAMPLED_IMAGE:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case descriptor_binding_type::SAMPLER:
                return VK_DESCRIPTOR_TYPE_SAMPLER;
            }

            logger->critical("Logic Error: Failed to determine proper VkDescriptorType. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr VkCompareOp to_vulkan(compare_operation op)
        {
            switch (op)
            {
            case compare_operation::LESS:
                return VK_COMPARE_OP_LESS;
            case compare_operation::LESS_OR_EQUALS:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
            case compare_operation::EQUALS:
                return VK_COMPARE_OP_EQUAL;
            case compare_operation::GREATER_OR_EQUALS:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case compare_operation::GREATER:
                return VK_COMPARE_OP_GREATER;
            case compare_operation::NOT_EQUALS:
                return VK_COMPARE_OP_NOT_EQUAL;
            case compare_operation::NEVER:
                return VK_COMPARE_OP_NEVER;
            case compare_operation::ALWAYS:
                return VK_COMPARE_OP_ALWAYS;
            }

            logger->critical("Logic Error: Failed to determine proper VkCompareOp. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr VkBlendOp to_vulkan(blend_operation op)
        {
            switch (op)
            {
            case blend_operation::ADD:
                return VK_BLEND_OP_ADD;
            case blend_operation::SUB:
                return VK_BLEND_OP_SUBTRACT;
            case blend_operation::MIN:
                return VK_BLEND_OP_MIN;
            case blend_operation::MAX:
                return VK_BLEND_OP_MAX;
            }

            logger->critical("Logic Error: Failed to determine proper VkBlendOp. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr VkBlendFactor to_vulkan(blend_factor factor)
        {
            switch (factor)
            {
            case blend_factor::ZERO:
                return VK_BLEND_FACTOR_ZERO;
            case blend_factor::ONE:
                return VK_BLEND_FACTOR_ONE;
            case blend_factor::SRC_COLOR:
                return VK_BLEND_FACTOR_SRC_COLOR;
            case blend_factor::ONE_MINUS_SRC_COLOR:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case blend_factor::DST_COLOR:
                return VK_BLEND_FACTOR_DST_COLOR;
            case blend_factor::ONE_MINUS_DST_COLOR:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case blend_factor::SRC_ALPHA:
                return VK_BLEND_FACTOR_SRC_ALPHA;
            case blend_factor::ONE_MINUS_SRC_ALPHA:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case blend_factor::DST_ALPHA:
                return VK_BLEND_FACTOR_DST_ALPHA;
            case blend_factor::ONE_MINUS_DST_ALPHA:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            }

            logger->critical("Logic Error: Failed to determine proper VkBlendFactor. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr VkColorComponentFlags compute_blend_write_mask(resource_format fmt)
        {
            switch (fmt)
            {
            case resource_format::R8_UNORM:
                [[fallthrough]];
            case resource_format::R32_FLOAT:
                [[fallthrough]];
            case resource_format::R32_UINT:
                return VK_COLOR_COMPONENT_R_BIT;
            case resource_format::RG16_FLOAT:
                [[fallthrough]];
            case resource_format::RG32_FLOAT:
                [[fallthrough]];
            case resource_format::RG32_UINT:
                return VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
            case resource_format::RGB32_FLOAT:
                return VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
            case resource_format::RGBA8_UNORM:
                [[fallthrough]];
            case resource_format::RGBA8_UINT:
                [[fallthrough]];
            case resource_format::RGBA8_SRGB:
                [[fallthrough]];
            case resource_format::BGRA8_SRGB:
                [[fallthrough]];
            case resource_format::RGBA16_FLOAT:
                [[fallthrough]];
            case resource_format::RGBA16_UNORM:
                [[fallthrough]];
            case resource_format::RGBA32_FLOAT:
                return VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                       VK_COLOR_COMPONENT_A_BIT;
            case resource_format::D32_FLOAT: {
                logger->critical("Logic Error: Cannot compute color component mask of depth format.");
                break;
            }
            case resource_format::UNKNOWN:
                break;
            default:
                break;
            }

            logger->critical("Logic Error: Failed to determine proper VkColorComponentFlags. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr VkFilter to_vulkan(filter f)
        {
            return static_cast<VkFilter>(f);
        }

        constexpr VkSamplerMipmapMode to_vulkan(mipmap_mode m)
        {
            return static_cast<VkSamplerMipmapMode>(m);
        }

        VkImageLayout compute_layout(image_resource_usage usage)
        {
            switch (usage)
            {
            case image_resource_usage::UNDEFINED:
                return VK_IMAGE_LAYOUT_UNDEFINED;
            case image_resource_usage::COLOR_ATTACHMENT:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case image_resource_usage::DEPTH_ATTACHMENT:
                return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            case image_resource_usage::PRESENT:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            case image_resource_usage::SAMPLED:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case image_resource_usage::STORAGE:
                return VK_IMAGE_LAYOUT_GENERAL;
            case image_resource_usage::TRANSFER_DESTINATION:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case image_resource_usage::TRANSFER_SOURCE:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            }
            logger->critical("Failed to compute expected image layout.");
            std::exit(EXIT_FAILURE);
        }

        void name_object([[maybe_unused]] const vkb::DispatchTable& dispatch, [[maybe_unused]] uint64_t object_handle,
                         [[maybe_unused]] VkObjectType type, [[maybe_unused]] const char* name)
        {
#ifdef _DEBUG
            VkDebugUtilsObjectNameInfoEXT name_info = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = nullptr,
                .objectType = type,
                .objectHandle = object_handle,
                .pObjectName = name,
            };

            dispatch.setDebugUtilsObjectNameEXT(&name_info);
#endif
        }

        static constexpr uint32_t IMAGE_POOL_SIZE = 4096;
        static constexpr uint32_t BUFFER_POOL_SIZE = 512;
        static constexpr uint32_t GRAPHICS_PIPELINE_POOL_SIZE = 256;
        static constexpr uint32_t COMPUTE_PIPELINE_POOL_SIZE = 128;
        static constexpr uint32_t SWAPCHAIN_POOL_SIZE = 8;
        static constexpr uint32_t SAMPLER_POOL_SIZE = 128;
    } // namespace

    render_device::render_device(abstract_allocator* alloc, vkb::Instance instance, vkb::PhysicalDevice physical)
        : _alloc{alloc}, _instance{instance}, _physical{physical}
    {
        _images.emplace(_alloc, IMAGE_POOL_SIZE, static_cast<uint32_t>(sizeof(image)));
        _buffers.emplace(_alloc, BUFFER_POOL_SIZE, static_cast<uint32_t>(sizeof(buffer)));
        _graphics_pipelines.emplace(_alloc, GRAPHICS_PIPELINE_POOL_SIZE,
                                    static_cast<uint32_t>(sizeof(graphics_pipeline)));
        _compute_pipelines.emplace(_alloc, COMPUTE_PIPELINE_POOL_SIZE, static_cast<uint32_t>(sizeof(compute_pipeline)));
        _swapchains.emplace(_alloc, SWAPCHAIN_POOL_SIZE, static_cast<uint32_t>(sizeof(swapchain)));
        _samplers.emplace(_alloc, SAMPLER_POOL_SIZE, static_cast<uint32_t>(sizeof(sampler)));
        _delete_queue.emplace(_frames_in_flight);

        auto queue_families = _physical.get_queue_families();
        std::unordered_map<uint32_t, uint32_t> queues_allocated;

        auto family_matcher =
            [&](VkQueueFlags flags) -> std::optional<std::tuple<VkQueueFamilyProperties, uint32_t, uint32_t>> {
            std::optional<std::tuple<VkQueueFamilyProperties, uint32_t, uint32_t>> best_match;

            uint32_t family_idx = 0;
            for (const auto& family : queue_families)
            {
                if (family.queueFlags == flags)
                {
                    auto index = queues_allocated[family_idx] < family.queueCount ? queues_allocated[family_idx]++ : 0;

                    return std::make_tuple(family, family_idx, index);
                }
                else if ((family.queueFlags & flags) == flags)
                {
                    if (best_match)
                    {
                        auto idx = std::get<1>(*best_match);
                        queues_allocated[idx]--;
                    }

                    auto index = queues_allocated[family_idx] < family.queueCount ? queues_allocated[family_idx]++ : 0;

                    best_match = std::make_tuple(family, family_idx, index);
                }

                family_idx++;
            }

            return best_match;
        };

        auto queue_family_info = family_matcher(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

        {
            std::vector<vkb::CustomQueueDescription> queue_setup;

            for (auto& [family_idx, count] : queues_allocated)
            {
                if (count == 0)
                    continue;

                std::vector<float> priorities(count, 1.0f / count);
                queue_setup.emplace_back(family_idx, count, std::move(priorities));
            }
            _device = *vkb::DeviceBuilder(_physical).custom_queue_setup(queue_setup).build();
            _dispatch = _device.make_table();
        }

        _queue = {
            .queue = VK_NULL_HANDLE,
            .queue_family_index = std::get<1>(*queue_family_info),
            .queue_index = std::get<2>(*queue_family_info),
            .flags = std::get<0>(*queue_family_info).queueFlags,
        };
        _dispatch.getDeviceQueue(_queue.queue_family_index, _queue.queue_index, &_queue.queue);

        VmaVulkanFunctions fns = {
            .vkGetInstanceProcAddr = _instance.fp_vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = _device.fp_vkGetDeviceProcAddr,
            .vkGetPhysicalDeviceProperties = nullptr,
            .vkGetPhysicalDeviceMemoryProperties = nullptr,
            .vkAllocateMemory = nullptr,
            .vkFreeMemory = nullptr,
            .vkMapMemory = nullptr,
            .vkUnmapMemory = nullptr,
            .vkFlushMappedMemoryRanges = nullptr,
            .vkInvalidateMappedMemoryRanges = nullptr,
            .vkBindBufferMemory = nullptr,
            .vkBindImageMemory = nullptr,
            .vkGetBufferMemoryRequirements = nullptr,
            .vkGetImageMemoryRequirements = nullptr,
            .vkCreateBuffer = nullptr,
            .vkDestroyBuffer = nullptr,
            .vkCreateImage = nullptr,
            .vkDestroyImage = nullptr,
            .vkCmdCopyBuffer = nullptr,
            .vkGetBufferMemoryRequirements2KHR = nullptr,
            .vkGetImageMemoryRequirements2KHR = nullptr,
            .vkBindBufferMemory2KHR = nullptr,
            .vkBindImageMemory2KHR = nullptr,
            .vkGetPhysicalDeviceMemoryProperties2KHR = nullptr,
            .vkGetDeviceBufferMemoryRequirements = nullptr,
            .vkGetDeviceImageMemoryRequirements = nullptr,
        };

        VmaAllocatorCreateInfo ci = {
            .flags = 0,
            .physicalDevice = physical.physical_device,
            .device = _device.device,
            .preferredLargeHeapBlockSize = 0,
            .pAllocationCallbacks = nullptr,
            .pDeviceMemoryCallbacks = nullptr,
            .pHeapSizeLimit = nullptr,
            .pVulkanFunctions = &fns,
            .instance = _instance.instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
            .pTypeExternalMemoryHandleTypes = nullptr,
        };

        VmaAllocator allocator;
        const auto result = vmaCreateAllocator(&ci, &allocator);
        if (result != VK_SUCCESS)
        {
            logger->critical("Failed to create Vulkan Memory Allocator. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }
        _vk_alloc = allocator;

        _recycled_cmd_buf_pool = command_buffer_recycler{
            .frames_in_flight = _frames_in_flight,
            .queue = _queue,
        };

        _sync_prim_recycler = sync_primitive_recycler{
            .frames_in_flight = _frames_in_flight,
        };

        _executor.emplace(_dispatch, *this);

        _staging_buffer = create_buffer({
            .per_frame = true,
            .loc = graphics::memory_location::HOST,
            .size = 64 * 1024 * 1024 * frames_in_flight(),
            .transfer_source = true,
            .name = "Staging Buffer",
        });

        _supports_aniso_filtering = physical.features.samplerAnisotropy;
        _max_aniso = physical.properties.limits.maxSamplerAnisotropy;
    }

    render_device::~render_device()
    {
        release_buffer(_staging_buffer);

        _dispatch.deviceWaitIdle();

        _delete_queue->flush_all();
        _recycled_cmd_buf_pool.release_all(_dispatch);
        _sync_prim_recycler.release_all(_dispatch);
        _executor = nullopt;

        vmaDestroyAllocator(_vk_alloc);
        vkb::destroy_device(_device);
    }

    void render_device::start_frame() noexcept
    {
    }

    void render_device::end_frame() noexcept
    {
        _sync_prim_recycler.recycle(_current_frame, _dispatch);
        _recycled_cmd_buf_pool.recycle(_current_frame, _dispatch);
        _delete_queue->flush_frame(_current_frame);
        _current_frame++;
    }

    buffer* render_device::access_buffer(buffer_resource_handle handle) noexcept
    {
        return reinterpret_cast<buffer*>(_buffers->access({
            .index = handle.id,
            .generation = handle.generation,
        }));
    }

    const buffer* render_device::access_buffer(buffer_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const buffer*>(_buffers->access({
            .index = handle.id,
            .generation = handle.generation,
        }));
    }

    buffer_resource_handle render_device::allocate_buffer()
    {
        auto key = _buffers->acquire_resource();
        return buffer_resource_handle(key.index, key.generation);
    }

    buffer_resource_handle render_device::create_buffer(const buffer_create_info& ci)
    {
        return create_buffer(ci, allocate_buffer());
    }

    buffer_resource_handle render_device::create_buffer(const buffer_create_info& ci, buffer_resource_handle handle)
    {
        if (!handle)
        {
            return buffer_resource_handle();
        }

        VkBufferUsageFlags usage = 0;
        usage |= ci.index_buffer ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : 0;
        usage |= ci.indirect_buffer ? VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT : 0;
        usage |= ci.storage_buffer ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0;
        usage |= ci.transfer_destination ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0;
        usage |= ci.transfer_source ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0;
        usage |= ci.uniform_buffer ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0;
        usage |= ci.vertex_buffer ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 0;

        if (ci.loc == memory_location::DEVICE)
        {
            usage |= (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        }

        VkBufferCreateInfo buf_ci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = ci.size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };

        VmaMemoryUsage mem_usage = ([ci]() {
            switch (ci.loc)
            {
            case memory_location::DEVICE:
                return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            case memory_location::HOST:
                return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            default:
                return VMA_MEMORY_USAGE_AUTO;
            }
        })();

        VkMemoryPropertyFlags required = 0;
        VkMemoryPropertyFlags preferred = 0;

        if (ci.uniform_buffer || ci.loc == memory_location::HOST)
        {
            required |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            preferred |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }

        VmaAllocationCreateFlags alloc_flags = 0;
        if (required & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            alloc_flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        }

        VmaAllocationCreateInfo alloc_ci = {
            .flags = alloc_flags,
            .usage = mem_usage,
            .requiredFlags = required,
            .preferredFlags = preferred,
            .memoryTypeBits = 0,
            .pool = VK_NULL_HANDLE,
            .pUserData = nullptr,
            .priority = 0.0f,
        };

        buffer buf{
            .per_frame_resource = ci.per_frame,
            .info = buf_ci,
            .name = string{ci.name},
        };

        auto result = vmaCreateBuffer(_vk_alloc, &buf_ci, &alloc_ci, &buf.vk_buffer, &buf.allocation, &buf.alloc_info);
        if (result != VK_SUCCESS)
        {
            _buffers->release_resource({
                .index = handle.id,
                .generation = handle.generation,
            });

            return buffer_resource_handle();
        }

        name_object(_dispatch, std::bit_cast<uint64_t>(buf.vk_buffer), VK_OBJECT_TYPE_BUFFER, ci.name.c_str());

        buffer* buf_ptr = access_buffer(handle);
        std::construct_at(buf_ptr, buf);

        return handle;
    }

    void render_device::release_buffer(buffer_resource_handle handle)
    {
        auto buf = access_buffer(handle);
        if (buf)
        {
            _delete_queue->add_to_queue(_current_frame, [this, buf, handle]() {
                vmaDestroyBuffer(_vk_alloc, buf->vk_buffer, buf->allocation);
                std::destroy_at(buf);
                _buffers->release_resource({.index = handle.id, .generation = handle.generation});
            });
        }
    }

    span<byte> render_device::map_buffer(buffer_resource_handle handle)
    {
        auto vk_buf = access_buffer(handle);
        void* result;
        [[maybe_unused]] auto res = vmaMapMemory(_vk_alloc, vk_buf->allocation, &result);
        assert(res == VK_SUCCESS);
        return span(reinterpret_cast<byte*>(result), vk_buf->info.size);
    }

    span<byte> render_device::map_buffer_frame(buffer_resource_handle handle, uint64_t frame_offset)
    {
        auto vk_buf = access_buffer(handle);
        void* result;
        [[maybe_unused]] auto res = vmaMapMemory(_vk_alloc, vk_buf->allocation, &result);
        assert(res == VK_SUCCESS);

        uint64_t frame = (_current_frame + frame_offset) % _frames_in_flight;

        if (vk_buf->per_frame_resource)
        {
            uint64_t size_per_frame = vk_buf->alloc_info.size / _frames_in_flight;
            return span(reinterpret_cast<byte*>(result) + size_per_frame * frame, size_per_frame);
        }

        logger->warn("Performance Note: Buffer is not a per-frame resource. Use map_buffer instead.");

        return span(reinterpret_cast<byte*>(result), vk_buf->info.size);
    }

    size_t render_device::get_buffer_frame_offset(buffer_resource_handle handle, uint64_t frame_offset)
    {
        auto vk_buf = access_buffer(handle);

        uint64_t frame = (_current_frame + frame_offset) % _frames_in_flight;

        if (vk_buf->per_frame_resource)
        {
            uint64_t size_per_frame = vk_buf->alloc_info.size / _frames_in_flight;
            return frame * size_per_frame;
        }

        return 0;
    }

    void render_device::unmap_buffer(buffer_resource_handle handle)
    {
        auto vk_buf = access_buffer(handle);
        vmaUnmapMemory(_vk_alloc, vk_buf->allocation);
    }

    image* render_device::access_image(image_resource_handle handle) noexcept
    {
        if (!handle)
        {
            return nullptr;
        }

        return reinterpret_cast<image*>(_images->access({
            .index = handle.id,
            .generation = handle.generation,
        }));
    }

    const image* render_device::access_image(image_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const image*>(_images->access({
            .index = handle.id,
            .generation = handle.generation,
        }));
    }

    image_resource_handle render_device::allocate_image()
    {
        auto key = _images->acquire_resource();
        return image_resource_handle(key.index, key.generation);
    }

    image_resource_handle render_device::create_image(const image_create_info& ci)
    {
        return create_image(ci, allocate_image());
    }

    image_resource_handle render_device::create_image(const image_create_info& ci, image_resource_handle handle)
    {
        if (!handle)
        {
            return image_resource_handle();
        }

        VkImageUsageFlags imgUsage = 0;
        imgUsage |= ci.transfer_source ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;
        imgUsage |= ci.transfer_destination ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;
        imgUsage |= ci.sampled ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
        imgUsage |= ci.storage ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
        imgUsage |= ci.color_attachment ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
        imgUsage |= ci.depth_attachment ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;

        VkImageCreateInfo image_ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = to_vulkan(ci.type),
            .format = to_vulkan(ci.format),
            .extent{
                .width = ci.width,
                .height = ci.height,
                .depth = ci.depth,
            },
            .mipLevels = ci.mip_count,
            .arrayLayers = ci.layers,
            .samples = to_vulkan(ci.samples),
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = imgUsage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VmaAllocationCreateFlags alloc_flags = 0;
        if (ci.color_attachment || ci.depth_attachment)
        {
            alloc_flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VmaAllocationCreateInfo alloc_create_info = {
            .flags = alloc_flags,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .preferredFlags = 0,
            .memoryTypeBits = 0,
            .pool = VK_NULL_HANDLE,
            .pUserData = nullptr,
            .priority = 0.0f,
        };

        VkImage img;
        VmaAllocation alloc;
        VmaAllocationInfo alloc_info;

        auto img_result = vmaCreateImage(_vk_alloc, &image_ci, &alloc_create_info, &img, &alloc, &alloc_info);
        if (img_result != VK_SUCCESS)
        {
            _images->release_resource({
                .index = handle.id,
                .generation = handle.generation,
            });
            return image_resource_handle();
        }

        name_object(_dispatch, std::bit_cast<uint64_t>(img), VK_OBJECT_TYPE_IMAGE, ci.name.c_str());

        VkImageAspectFlags aspect = 0;
        aspect |=
            (ci.color_attachment || (ci.sampled && !ci.depth_attachment) || ci.storage) ? VK_IMAGE_ASPECT_COLOR_BIT : 0;
        aspect |= ci.depth_attachment ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;

        VkImageViewCreateInfo view_ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = img,
            .viewType = to_vulkan_view(ci.type),
            .format = to_vulkan(ci.format),
            .components{
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange{
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = ci.mip_count,
                .baseArrayLayer = 0,
                .layerCount = ci.layers,
            },
        };

        VkImageView view;
        auto view_result = _dispatch.createImageView(&view_ci, nullptr, &view);
        if (view_result != VK_SUCCESS)
        {
            vmaDestroyImage(_vk_alloc, img, alloc);

            _images->release_resource({
                .index = handle.id,
                .generation = handle.generation,
            });

            return image_resource_handle();
        }

        name_object(_dispatch, std::bit_cast<uint64_t>(view), VK_OBJECT_TYPE_IMAGE_VIEW, ci.name.c_str());

        image img_info = {
            .allocation = alloc,
            .alloc_info = alloc_info,
            .image = img,
            .view = view,
            .img_info = image_ci,
            .view_info = view_ci,
            .persistent = ci.persistent,
            .name = string(ci.name),
        };

        auto image_ptr = access_image(handle);
        std::construct_at(image_ptr, img_info);

        return handle;
    }

    void render_device::release_image(image_resource_handle handle)
    {
        auto img = access_image(handle);
        if (img)
        {
            _delete_queue->add_to_queue(_current_frame, [this, img, handle]() {
                if (img->allocation)
                {
                    vmaDestroyImage(_vk_alloc, img->image, img->allocation);
                }
                _dispatch.destroyImageView(img->view, nullptr);
                std::destroy_at(img);
                _images->release_resource({.index = handle.id, .generation = handle.generation});
            });
        }
    }

    sampler* render_device::access_sampler(sampler_resource_handle handle) noexcept
    {
        return reinterpret_cast<sampler*>(_samplers->access({
            .index = handle.id,
            .generation = handle.generation,
        }));
    }

    const sampler* render_device::access_sampler(sampler_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const sampler*>(_samplers->access({
            .index = handle.id,
            .generation = handle.generation,
        }));
    }

    sampler_resource_handle render_device::allocate_sampler()
    {
        auto key = _samplers->acquire_resource();
        return sampler_resource_handle(key.index, key.generation);
    }

    sampler_resource_handle render_device::create_sampler(const sampler_create_info& ci)
    {
        return create_sampler(ci, allocate_sampler());
    }

    sampler_resource_handle render_device::create_sampler(const sampler_create_info& ci, sampler_resource_handle handle)
    {
        if (!handle)
        {
            return sampler_resource_handle();
        }

        VkSamplerCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = to_vulkan(ci.mag),
            .minFilter = to_vulkan(ci.min),
            .mipmapMode = to_vulkan(ci.mipmap),
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = ci.mip_lod_bias,
            .anisotropyEnable = (ci.enable_aniso & _supports_aniso_filtering) ? VK_TRUE : VK_FALSE,
            .maxAnisotropy = std::clamp(ci.max_anisotropy, 1.0f, _max_aniso),
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = ci.min_lod,
            .maxLod = ci.max_lod,
            .borderColor = VK_BORDER_COLOR_MAX_ENUM,
            .unnormalizedCoordinates = VK_FALSE,
        };

        VkSampler s;
        auto result = _dispatch.createSampler(&create_info, nullptr, &s);
        if (result != VK_SUCCESS)
        {
            _samplers->release_resource({
                .index = handle.id,
                .generation = handle.generation,
            });

            return sampler_resource_handle();
        }

        sampler smp{
            .vk_sampler = s,
            .info = create_info,
            .name = ci.name,
        };

        auto ptr = access_sampler(handle);
        std::construct_at(ptr, std::move(smp));

        return handle;
    }

    void render_device::release_sampler(sampler_resource_handle handle)
    {
        sampler* smp = access_sampler(handle);
        if (smp)
        {
            _delete_queue->add_to_queue(_current_frame, [this, smp, handle] {
                _dispatch.destroySampler(smp->vk_sampler, nullptr);
                std::destroy_at(smp);
                _samplers->release_resource({.index = handle.id, .generation = handle.generation});
            });
        }
    }

    graphics_pipeline* render_device::access_graphics_pipeline(graphics_pipeline_resource_handle handle) noexcept
    {
        return reinterpret_cast<graphics_pipeline*>(_graphics_pipelines->access({
            .index = handle.id,
            .generation = handle.generation,
        }));
    }

    const graphics_pipeline* render_device::access_graphics_pipeline(
        graphics_pipeline_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const graphics_pipeline*>(_graphics_pipelines->access({
            .index = handle.id,
            .generation = handle.generation,
        }));
    }

    graphics_pipeline_resource_handle render_device::allocate_graphics_pipeline()
    {
        auto key = _graphics_pipelines->acquire_resource();
        return graphics_pipeline_resource_handle(key.index, key.generation);
    }

    graphics_pipeline_resource_handle render_device::create_graphics_pipeline(const graphics_pipeline_create_info& ci)
    {
        return create_graphics_pipeline(ci, allocate_graphics_pipeline());
    }

    graphics_pipeline_resource_handle render_device::create_graphics_pipeline(const graphics_pipeline_create_info& ci,
                                                                              graphics_pipeline_resource_handle handle)
    {
        if (!handle)
        {
            return graphics_pipeline_resource_handle();
        }

        // TODO: Cache Descriptor Set Layouts and Pipeline Layouts
        vector<VkDescriptorSetLayout> set_layouts;
        vector<VkPushConstantRange> ranges;

        for (const auto& info : ci.layout.set_layouts)
        {
            vector<VkDescriptorSetLayoutBinding> bindings;
            vector<VkDescriptorBindingFlags> flags;

            for (const auto& binding : info.bindings)
            {
                bindings.push_back(VkDescriptorSetLayoutBinding{
                    .binding = binding.binding_index,
                    .descriptorType = to_vulkan(binding.type),
                    .descriptorCount = binding.binding_count,
                    .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
                    .pImmutableSamplers = nullptr,
                });

                flags.push_back(binding.binding_count > 1 ? VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT : 0);
            }

            VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = nullptr,
                .bindingCount = static_cast<uint32_t>(flags.size()),
                .pBindingFlags = flags.empty() ? nullptr : flags.data(),
            };

            VkDescriptorSetLayoutCreateInfo set_layout_ci = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &binding_flags,
                .flags = {},
                .bindingCount = static_cast<uint32_t>(bindings.size()),
                .pBindings = bindings.data(),
            };

            VkDescriptorSetLayout layout;
            auto result = _dispatch.createDescriptorSetLayout(&set_layout_ci, nullptr, &layout);
            if (result != VK_SUCCESS)
            {
                logger->error("Failed to create VkDescriptorSetLayout.");
                return graphics_pipeline_resource_handle();
            }

            set_layouts.push_back(layout);
        }

        for (const auto& range : ci.layout.push_constants)
        {
            ranges.push_back(VkPushConstantRange{
                .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
                .offset = range.offset,
                .size = range.range,
            });
        }

        VkPipelineLayoutCreateInfo pipeline_layout_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
            .pSetLayouts = set_layouts.empty() ? nullptr : set_layouts.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(ranges.size()),
            .pPushConstantRanges = ranges.empty() ? nullptr : ranges.data(),
        };

        VkPipelineLayout pipeline_layout;
        auto pipeline_layout_result = _dispatch.createPipelineLayout(&pipeline_layout_ci, nullptr, &pipeline_layout);
        if (pipeline_layout_result != VK_SUCCESS)
        {
            logger->error("Failed to create VkPipelineLayout.");
            return graphics_pipeline_resource_handle();
        }

        vector<VkFormat> color_formats;
        for (auto fmt : ci.target.color_attachment_formats)
        {
            color_formats.push_back(to_vulkan(fmt));
        }

        VkPipelineRenderingCreateInfo dynamic_render = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext = nullptr,
            .viewMask = 0,
            .colorAttachmentCount = static_cast<uint32_t>(color_formats.size()),
            .pColorAttachmentFormats = color_formats.empty() ? nullptr : color_formats.data(),
            .depthAttachmentFormat = to_vulkan(ci.target.depth_attachment_format),
            .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
        };

        VkShaderModuleCreateInfo vertex_ci = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = ci.vertex_shader.bytes.size(),
            .pCode = reinterpret_cast<uint32_t*>(ci.vertex_shader.bytes.data()),
        };

        VkShaderModuleCreateInfo fragment_ci = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = ci.fragment_shader.bytes.size(),
            .pCode = reinterpret_cast<uint32_t*>(ci.fragment_shader.bytes.data()),
        };

        uint32_t shader_count = 1;

        VkShaderModule vertex_module = VK_NULL_HANDLE, fragment_module = VK_NULL_HANDLE;
        auto vertex_module_result = _dispatch.createShaderModule(&vertex_ci, nullptr, &vertex_module);

        name_object(_dispatch, std::bit_cast<uint64_t>(vertex_module), VK_OBJECT_TYPE_SHADER_MODULE,
                    ci.vertex_shader.name.c_str());

        VkPipelineShaderStageCreateInfo vertex_stage_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = {},
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_module,
            .pName = ci.vertex_shader.entrypoint.data(),
            .pSpecializationInfo = nullptr,
        };

        VkPipelineShaderStageCreateInfo fragment_stage_ci = {};
        bool has_fragment_shader = fragment_ci.codeSize > 0;

        if (has_fragment_shader)
        {
            auto fragment_module_result = _dispatch.createShaderModule(&fragment_ci, nullptr, &fragment_module);
            if (vertex_module_result != VK_SUCCESS || fragment_module_result != VK_SUCCESS)
            {
                logger->error("Failed to create VkShaderModules for pipeline.");
                return graphics_pipeline_resource_handle();
            }

            name_object(_dispatch, std::bit_cast<uint64_t>(fragment_module), VK_OBJECT_TYPE_SHADER_MODULE,
                        ci.fragment_shader.name.c_str());

            fragment_stage_ci = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = {},
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragment_module,
                .pName = ci.fragment_shader.entrypoint.data(),
                .pSpecializationInfo = nullptr,
            };

            ++shader_count;
        }

        VkPipelineShaderStageCreateInfo stages_ci[] = {
            vertex_stage_ci,
            fragment_stage_ci,
        };

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
            VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
            VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
            VK_DYNAMIC_STATE_CULL_MODE,
        };

        vector<VkVertexInputBindingDescription> vertex_bindings;
        vector<VkVertexInputAttributeDescription> vertex_attributes;

        std::unordered_map<size_t, uint32_t> binding_sizes;

        for (const auto& element : ci.vertex_layout.elements)
        {
            binding_sizes[element.binding] += static_cast<uint32_t>(get_format_size(element.format));
        }

        for (const auto& element : ci.vertex_layout.elements)
        {
            auto it = std::find_if(std::begin(vertex_bindings), std::end(vertex_bindings),
                                   [element](const auto& binding) { return binding.binding == element.binding; });
            if (it != std::end(vertex_bindings))
            {
                VkVertexInputBindingDescription binding = {
                    .binding = element.binding,
                    .stride = binding_sizes[element.binding],
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                };

                vertex_bindings.push_back(binding);
            }

            VkVertexInputAttributeDescription attrib = {
                .location = element.location,
                .binding = element.binding,
                .format = to_vulkan(element.format),
                .offset = element.offset,
            };

            vertex_attributes.push_back(attrib);
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_bindings.size()),
            .pVertexBindingDescriptions = vertex_bindings.empty() ? nullptr : vertex_bindings.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes.size()),
            .pVertexAttributeDescriptions = vertex_attributes.empty() ? nullptr : vertex_attributes.data(),
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthTestEnable = ci.depth_testing.enable_test ? VK_TRUE : VK_FALSE,
            .depthWriteEnable = ci.depth_testing.enable_write ? VK_TRUE : VK_FALSE,
            .depthCompareOp = to_vulkan(ci.depth_testing.depth_test_op),
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = ci.depth_testing.min_depth_bounds,
            .maxDepthBounds = ci.depth_testing.max_depth_bounds,
        };

        VkPipelineRasterizationStateCreateInfo raster_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthClampEnable = ci.depth_testing.clamp_depth ? VK_TRUE : VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = ci.depth_testing.enable_depth_bias ? VK_TRUE : VK_FALSE,
            .depthBiasConstantFactor = ci.depth_testing.depth_bias_constant_factor,
            .depthBiasClamp = ci.depth_testing.depth_bias_clamp,
            .depthBiasSlopeFactor = ci.depth_testing.depth_bias_slope_factor,
            .lineWidth = 1.0f,
        };

        vector<VkPipelineColorBlendAttachmentState> attachment_blends;
        for (const auto& blend_info : ci.blending.attachment_blend_ops)
        {
            VkPipelineColorBlendAttachmentState state = {
                .blendEnable = blend_info.enabled ? VK_TRUE : VK_FALSE,
                .srcColorBlendFactor = to_vulkan(blend_info.color.src),
                .dstColorBlendFactor = to_vulkan(blend_info.color.dst),
                .colorBlendOp = to_vulkan(blend_info.color.op),
                .srcAlphaBlendFactor = to_vulkan(blend_info.alpha.src),
                .dstAlphaBlendFactor = to_vulkan(blend_info.alpha.dst),
                .alphaBlendOp = to_vulkan(blend_info.alpha.op),
                .colorWriteMask =
                    compute_blend_write_mask(ci.target.color_attachment_formats[attachment_blends.size()]),
            };

            attachment_blends.push_back(state);
        };

        VkPipelineColorBlendStateCreateInfo color_blend_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_NO_OP,
            .attachmentCount = static_cast<uint32_t>(attachment_blends.size()),
            .pAttachments = attachment_blends.empty() ? nullptr : attachment_blends.data(),
            .blendConstants = {},
        };

        VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = static_cast<uint32_t>(sizeof(dynamic_states) / sizeof(VkDynamicState)),
            .pDynamicStates = sizeof(dynamic_states) == 0 ? nullptr : dynamic_states,
        };

        VkPipelineViewportStateCreateInfo viewport = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 0,
            .pViewports = nullptr,
            .scissorCount = 0,
            .pScissors = nullptr,
        };

        VkPipelineMultisampleStateCreateInfo multisample_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 0.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };

        VkGraphicsPipelineCreateInfo pipeline_ci = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &dynamic_render,
            .flags = 0,
            .stageCount = shader_count,
            .pStages = stages_ci,
            .pVertexInputState = &vertex_input_state,
            .pInputAssemblyState = &input_assembly_state,
            .pTessellationState = nullptr,
            .pViewportState = &viewport,
            .pRasterizationState = &raster_state,
            .pMultisampleState = has_fragment_shader ? &multisample_state : nullptr,
            .pDepthStencilState = &depth_stencil_state,
            .pColorBlendState = has_fragment_shader ? &color_blend_state : nullptr,
            .pDynamicState = &dynamic_state,
            .layout = pipeline_layout,
            .renderPass = VK_NULL_HANDLE,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0,
        };

        VkPipeline pipeline;
        auto pipeline_result = _dispatch.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &pipeline);
        if (pipeline_result != VK_SUCCESS)
        {
            return graphics_pipeline_resource_handle();
        }

        name_object(_dispatch, std::bit_cast<uint64_t>(pipeline), VK_OBJECT_TYPE_PIPELINE, ci.name.c_str());

        graphics_pipeline gfx_pipeline = {
            .vertex_module = vertex_module,
            .fragment_module = fragment_module,
            .set_layouts = std::move(set_layouts),
            .pipeline = pipeline,
            .pipeline_layout = pipeline_layout,
            .name = ci.name,
        };

        auto pipeline_ptr = access_graphics_pipeline(handle);
        std::construct_at(pipeline_ptr, gfx_pipeline);

        return handle;
    }

    void render_device::release_graphics_pipeline(graphics_pipeline_resource_handle handle)
    {
        graphics_pipeline* pipeline = access_graphics_pipeline(handle);
        if (pipeline)
        {
            _delete_queue->add_to_queue(_current_frame, [this, pipeline, handle]() {
                _dispatch.destroyPipeline(pipeline->pipeline, nullptr);
                _dispatch.destroyShaderModule(pipeline->vertex_module, nullptr);
                _dispatch.destroyShaderModule(pipeline->fragment_module, nullptr);
                _dispatch.destroyPipelineLayout(pipeline->pipeline_layout, nullptr);
                for (auto layout : pipeline->set_layouts)
                {
                    _dispatch.destroyDescriptorSetLayout(layout, nullptr);
                }
                std::destroy_at(pipeline);
                _graphics_pipelines->release_resource({.index = handle.id, .generation = handle.generation});
            });
        }
    }

    compute_pipeline* render_device::access_compute_pipeline(compute_pipeline_resource_handle handle) noexcept
    {
        return reinterpret_cast<compute_pipeline*>(
            _compute_pipelines->access({.index = handle.id, .generation = handle.generation}));
    }

    const compute_pipeline* render_device::access_compute_pipeline(
        compute_pipeline_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const compute_pipeline*>(
            _compute_pipelines->access({.index = handle.id, .generation = handle.generation}));
    }

    compute_pipeline_resource_handle render_device::allocate_compute_pipeline()
    {
        auto key = _compute_pipelines->acquire_resource();
        return compute_pipeline_resource_handle(key.index, key.generation);
    }

    compute_pipeline_resource_handle render_device::create_compute_pipeline(const compute_pipeline_create_info& ci)
    {
        return create_compute_pipeline(ci, allocate_compute_pipeline());
    }

    compute_pipeline_resource_handle render_device::create_compute_pipeline(const compute_pipeline_create_info& ci,
                                                                            compute_pipeline_resource_handle handle)
    {
        if (!handle)
        {
            return compute_pipeline_resource_handle();
        }

        // TODO: Cache Descriptor Set Layouts and Pipeline Layouts
        vector<VkDescriptorSetLayout> set_layouts;
        vector<VkPushConstantRange> ranges;

        for (const auto& info : ci.layout.set_layouts)
        {
            vector<VkDescriptorSetLayoutBinding> bindings;

            for (const auto& binding : info.bindings)
            {
                bindings.push_back(VkDescriptorSetLayoutBinding{
                    .binding = binding.binding_index,
                    .descriptorType = to_vulkan(binding.type),
                    .descriptorCount = binding.binding_count,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    .pImmutableSamplers = nullptr,
                });
            }

            VkDescriptorSetLayoutCreateInfo set_layout_ci = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = {},
                .bindingCount = static_cast<uint32_t>(bindings.size()),
                .pBindings = bindings.data(),
            };

            VkDescriptorSetLayout layout;
            auto result = _dispatch.createDescriptorSetLayout(&set_layout_ci, nullptr, &layout);
            if (result != VK_SUCCESS)
            {
                logger->error("Failed to create VkDescriptorSetLayout.");
                return compute_pipeline_resource_handle();
            }

            set_layouts.push_back(layout);
        }

        for (const auto& range : ci.layout.push_constants)
        {
            ranges.push_back(VkPushConstantRange{
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = range.offset,
                .size = range.range,
            });
        }

        VkPipelineLayoutCreateInfo pipeline_layout_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
            .pSetLayouts = set_layouts.empty() ? nullptr : set_layouts.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(ranges.size()),
            .pPushConstantRanges = ranges.empty() ? nullptr : ranges.data(),
        };

        VkPipelineLayout pipeline_layout;
        auto pipeline_layout_result = _dispatch.createPipelineLayout(&pipeline_layout_ci, nullptr, &pipeline_layout);
        if (pipeline_layout_result != VK_SUCCESS)
        {
            logger->error("Failed to create VkPipelineLayout.");
            return compute_pipeline_resource_handle();
        }

        name_object(_dispatch, std::bit_cast<uint64_t>(pipeline_layout), VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                    ci.name.c_str());

        VkShaderModuleCreateInfo compute_module_ci = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = ci.compute_shader.bytes.size(),
            .pCode = reinterpret_cast<uint32_t*>(ci.compute_shader.bytes.data()),
        };

        VkShaderModule compute_shader_module{VK_NULL_HANDLE};

        auto module_result = _dispatch.createShaderModule(&compute_module_ci, nullptr, &compute_shader_module);
        if (module_result != VK_SUCCESS)
        {
            logger->error("Failed to create VkShaderModule");
            return compute_pipeline_resource_handle();
        }

        name_object(_dispatch, std::bit_cast<uint64_t>(compute_shader_module), VK_OBJECT_TYPE_SHADER_MODULE,
                    ci.compute_shader.name.c_str());

        VkPipelineShaderStageCreateInfo compute_stage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = compute_shader_module,
            .pName = ci.compute_shader.entrypoint.data(),
            .pSpecializationInfo = nullptr,
        };

        VkComputePipelineCreateInfo compute_ci = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = compute_stage,
            .layout = pipeline_layout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0,
        };

        compute_pipeline pipeline = {
            .compute_module = compute_shader_module,
            .set_layouts = set_layouts,
            .pipeline = VK_NULL_HANDLE,
            .pipeline_layout = pipeline_layout,
            .name = ci.name,
        };

        auto compute_result =
            _dispatch.createComputePipelines(VK_NULL_HANDLE, 1, &compute_ci, nullptr, &pipeline.pipeline);

        if (compute_result != VK_SUCCESS)
        {
            logger->error("Failed to create compute VkPipeline.");
            return compute_pipeline_resource_handle();
        }

        name_object(_dispatch, std::bit_cast<uint64_t>(pipeline.pipeline), VK_OBJECT_TYPE_PIPELINE,
                    pipeline.name.c_str());

        auto compute_ptr = access_compute_pipeline(handle);
        std::construct_at(compute_ptr, std::move(pipeline));

        return handle;
    }

    void render_device::release_compute_pipeline(compute_pipeline_resource_handle handle)
    {
        compute_pipeline* pipeline = access_compute_pipeline(handle);
        if (pipeline)
        {
            _delete_queue->add_to_queue(_current_frame, [this, pipeline, handle]() {
                _dispatch.destroyPipeline(pipeline->pipeline, nullptr);
                _dispatch.destroyShaderModule(pipeline->compute_module, nullptr);
                _dispatch.destroyPipelineLayout(pipeline->pipeline_layout, nullptr);
                for (auto layout : pipeline->set_layouts)
                {
                    _dispatch.destroyDescriptorSetLayout(layout, nullptr);
                }
                std::destroy_at(pipeline);
                _compute_pipelines->release_resource({.index = handle.id, .generation = handle.generation});
            });
        }
    }

    swapchain* render_device::access_swapchain(swapchain_resource_handle handle) noexcept
    {
        return reinterpret_cast<swapchain*>(_swapchains->access({.index = handle.id, .generation = handle.generation}));
    }

    const swapchain* render_device::access_swapchain(swapchain_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const swapchain*>(
            _swapchains->access({.index = handle.id, .generation = handle.generation}));
    }

    swapchain_resource_handle render_device::allocate_swapchain()
    {
        auto key = _swapchains->acquire_resource();
        return swapchain_resource_handle(key.index, key.generation);
    }

    swapchain_resource_handle render_device::create_swapchain(const swapchain_create_info& info)
    {
        return create_swapchain(info, allocate_swapchain());
    }

    swapchain_resource_handle render_device::create_swapchain(const swapchain_create_info& info,
                                                              swapchain_resource_handle handle)
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        glfw::window* win = dynamic_cast<glfw::window*>(info.win);

        uint32_t width = info.win->width();
        uint32_t height = info.win->height();

        if (win)
        {
            auto window = win->raw();
            auto result = glfwCreateWindowSurface(_instance.instance, window, nullptr, &surface);
            if (result != VK_SUCCESS)
            {
                return swapchain_resource_handle();
            }

            int w, h;
            glfwGetFramebufferSize(win->raw(), &w, &h);
            width = static_cast<uint32_t>(w);
            height = static_cast<uint32_t>(h);
        }

        vkb::SwapchainBuilder swap_bldr =
            vkb::SwapchainBuilder(_physical, _device, surface)
                .add_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .set_required_min_image_count(info.desired_frame_count)
                .set_desired_extent(width, height)
                .set_desired_present_mode(info.use_vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR)
                .set_desired_format(
                    {.format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});

        auto result = swap_bldr.build();
        if (!result)
        {
            return swapchain_resource_handle();
        }

        swapchain sc = {
            .win = win,
            .sc = result.value(),
            .surface = surface,
        };

        auto images = sc.sc.get_images().value();
        auto views = sc.sc.get_image_views().value();
        sc.image_handles.reserve(views.size());

        for (uint32_t i = 0; i < sc.sc.image_count; ++i)
        {
            image sc_image = {
                .allocation = nullptr,
                .image = images[i],
                .view = views[i],
                .img_info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = {},
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = VK_FORMAT_UNDEFINED,
                    .extent{
                        .width = sc.sc.extent.width,
                        .height = sc.sc.extent.height,
                        .depth = 1,
                    },
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                    .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                    .queueFamilyIndexCount = 0,
                    .pQueueFamilyIndices = nullptr,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                },
                .view_info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = {},
                    .image = images[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = VK_FORMAT_UNDEFINED,
                    .components =
                        {
                            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                        },
                    .subresourceRange{
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                },
                .name = std::format("swapchain_image_{}", i).c_str(),
            };

            auto sc_image_handle = allocate_image();
            auto sc_image_ptr = access_image(sc_image_handle);
            std::construct_at(sc_image_ptr, std::move(sc_image));

            sc.image_handles.push_back(sc_image_handle);
        }

        std::construct_at(access_swapchain(handle), std::move(sc));

        return handle;
    }

    void render_device::release_swapchain(swapchain_resource_handle handle)
    {
        swapchain* sc = access_swapchain(handle);
        if (sc)
        {
            for (image_resource_handle img : sc->image_handles)
            {
                release_image(img);
            }

            _delete_queue->add_to_queue(_current_frame, [this, sc, handle]() {
                vkb::destroy_swapchain(sc->sc);
                vkb::destroy_surface(_instance, sc->surface);
                std::destroy_at(sc);
                _swapchains->release_resource({.index = handle.id, .generation = handle.generation});
            });
        }
    }

    void render_device::recreate_swapchain(swapchain_resource_handle handle)
    {
        auto sc = access_swapchain(handle);
        if (sc->win->minimized())
        {
            return;
        }

        _dispatch.deviceWaitIdle();
        auto width = sc->win->width();
        auto height = sc->win->height();

        if (auto win = dynamic_cast<glfw::window*>(sc->win))
        {
            int w, h;
            glfwGetFramebufferSize(win->raw(), &w, &h);
            width = static_cast<uint32_t>(w);
            height = static_cast<uint32_t>(h);
        }

        if (width == 0 || height == 0)
        {
            logger->warn("Cannot resize swapchain with 0 sized dimension. Requested dimensions: {0}x{1}", width,
                         height);

            return;
        }

        vkb::Swapchain old_swap = sc->sc;

        vkb::SwapchainBuilder swap_bldr =
            vkb::SwapchainBuilder(_physical, _device, sc->surface)
                .add_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .set_required_min_image_count(sc->sc.image_count)
                .set_desired_extent(width, height)
                .set_desired_present_mode(old_swap.present_mode)
                .set_desired_format(
                    {.format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                .set_old_swapchain(sc->sc);

        auto swap_result = swap_bldr.build();
        if (!swap_result)
        {
            logger->error("Failed to create VkSwapchainKHR for window.");
            return;
        }

        for (auto img_handle : sc->image_handles)
        {
            auto img = *access_image(img_handle);
            _delete_queue->add_to_queue(_current_frame,
                                        [this, img]() { _dispatch.destroyImageView(img.view, nullptr); });
        }

        _delete_queue->add_to_queue(_current_frame, [old_swap]() { vkb::destroy_swapchain(old_swap); });

        sc->sc = *swap_result;

        auto images = sc->sc.get_images().value();
        auto views = sc->sc.get_image_views().value();
        sc->image_handles.reserve(views.size());

        if (views.size() < sc->image_handles.size())
        {
            for (size_t i = views.size(); i < sc->image_handles.size(); ++i)
            {
                release_image(sc->image_handles[i]);
            }
            sc->image_handles.erase(sc->image_handles.begin() + views.size(), sc->image_handles.end());
        }

        for (uint32_t i = 0; i < sc->sc.image_count; ++i)
        {
            image sc_image = {
                .allocation = nullptr,
                .image = images[i],
                .view = views[i],
                .img_info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = {},
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = VK_FORMAT_UNDEFINED,
                    .extent{
                        .width = sc->sc.extent.width,
                        .height = sc->sc.extent.height,
                        .depth = 1,
                    },
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                    .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                    .queueFamilyIndexCount = 0,
                    .pQueueFamilyIndices = nullptr,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                },
                .view_info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = {},
                    .image = images[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = VK_FORMAT_UNDEFINED,
                    .components =
                        {
                            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                        },
                    .subresourceRange{
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                },
                .name = std::format("swapchain_image_{}", i).c_str(),
            };

            if (i < sc->image_handles.size())
            {
                auto sc_image_ptr = access_image(sc->image_handles[i]);
                *sc_image_ptr = sc_image;
            }
            else
            {
                auto sc_image_handle = allocate_image();
                auto sc_image_ptr = access_image(sc_image_handle);
                std::construct_at(sc_image_ptr, std::move(sc_image));

                sc->image_handles.push_back(sc_image_handle);
            }
        }
    }

    image_resource_handle render_device::fetch_current_image(swapchain_resource_handle handle)
    {
        auto sc = access_swapchain(handle);
        return sc->image_handles[sc->image_index];
    }

    VkResult render_device::acquire_next_image(swapchain_resource_handle handle, VkSemaphore sem, VkFence fen)
    {
        auto swap = access_swapchain(handle);
        return _dispatch.acquireNextImageKHR(swap->sc, UINT_MAX, sem, fen, &swap->image_index);
    }

    command_buffer_allocator render_device::acquire_frame_local_command_buffer_allocator()
    {
        return _recycled_cmd_buf_pool.acquire(_dispatch, this);
    }

    void render_device::release_frame_local_command_buffer_allocator(command_buffer_allocator&& allocator)
    {
        _recycled_cmd_buf_pool.release(std::move(allocator), _current_frame);
    }

    render_context::render_context(abstract_allocator* alloc)
        : graphics::render_context{alloc}, _instance{build_instance()}
    {
    }

    render_context::~render_context()
    {
        _devices.clear();
        vkb::destroy_instance(_instance);
    }

    bool render_context::has_suitable_device() const noexcept
    {
        return !_devices.empty();
    }

    uint32_t render_context::device_count() const noexcept
    {
        return static_cast<uint32_t>(_devices.size());
    }

    namespace
    {
        vkb::PhysicalDeviceSelector select_device(vkb::Instance instance)
        {
            VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extended_dynamic_state = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT,
                .pNext = nullptr,
                .extendedDynamicState3TessellationDomainOrigin = VK_FALSE,
                .extendedDynamicState3DepthClampEnable = VK_FALSE,
                .extendedDynamicState3PolygonMode = VK_FALSE,
                .extendedDynamicState3RasterizationSamples = VK_TRUE,
                .extendedDynamicState3SampleMask = VK_FALSE,
                .extendedDynamicState3AlphaToCoverageEnable = VK_FALSE,
                .extendedDynamicState3AlphaToOneEnable = VK_FALSE,
                .extendedDynamicState3LogicOpEnable = VK_FALSE,
                .extendedDynamicState3ColorBlendEnable = VK_FALSE,
                .extendedDynamicState3ColorBlendEquation = VK_FALSE,
                .extendedDynamicState3ColorWriteMask = VK_FALSE,
                .extendedDynamicState3RasterizationStream = VK_FALSE,
                .extendedDynamicState3ConservativeRasterizationMode = VK_FALSE,
                .extendedDynamicState3ExtraPrimitiveOverestimationSize = VK_FALSE,
                .extendedDynamicState3DepthClipEnable = VK_FALSE,
                .extendedDynamicState3SampleLocationsEnable = VK_FALSE,
                .extendedDynamicState3ColorBlendAdvanced = VK_FALSE,
                .extendedDynamicState3ProvokingVertexMode = VK_FALSE,
                .extendedDynamicState3LineRasterizationMode = VK_FALSE,
                .extendedDynamicState3LineStippleEnable = VK_FALSE,
                .extendedDynamicState3DepthClipNegativeOneToOne = VK_FALSE,
                .extendedDynamicState3ViewportWScalingEnable = VK_FALSE,
                .extendedDynamicState3ViewportSwizzle = VK_FALSE,
                .extendedDynamicState3CoverageToColorEnable = VK_FALSE,
                .extendedDynamicState3CoverageToColorLocation = VK_FALSE,
                .extendedDynamicState3CoverageModulationMode = VK_FALSE,
                .extendedDynamicState3CoverageModulationTableEnable = VK_FALSE,
                .extendedDynamicState3CoverageModulationTable = VK_FALSE,
                .extendedDynamicState3CoverageReductionMode = VK_FALSE,
                .extendedDynamicState3RepresentativeFragmentTestEnable = VK_FALSE,
                .extendedDynamicState3ShadingRateImageEnable = VK_FALSE,
            };

            vkb::PhysicalDeviceSelector selector =
                vkb::PhysicalDeviceSelector(instance)
                    .prefer_gpu_device_type(vkb::PreferredDeviceType::integrated)
                    .defer_surface_initialization()
                    .require_present()
                    .add_required_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME)
                    .set_minimum_version(1, 3)
                    .set_required_features({
#ifdef _DEBUG
                        .robustBufferAccess = VK_TRUE,
#else
                        .robustBufferAccess = VK_FALSE,
#endif
                        .fullDrawIndexUint32 = VK_FALSE,
                        .imageCubeArray = VK_FALSE,
                        .independentBlend = VK_TRUE,
                        .geometryShader = VK_FALSE,
                        .tessellationShader = VK_FALSE,
                        .sampleRateShading = VK_FALSE,
                        .dualSrcBlend = VK_FALSE,
                        .logicOp = VK_TRUE,
                        .multiDrawIndirect = VK_TRUE,
                        .drawIndirectFirstInstance = VK_TRUE,
                        .depthClamp = VK_TRUE,
                        .depthBiasClamp = VK_TRUE,
                        .fillModeNonSolid = VK_TRUE,
                        .depthBounds = VK_TRUE,
                        .wideLines = VK_FALSE,
                        .largePoints = VK_FALSE,
                        .alphaToOne = VK_FALSE,
                        .multiViewport = VK_FALSE,
                        .samplerAnisotropy = VK_TRUE,
                        .textureCompressionETC2 = VK_FALSE,
                        .textureCompressionASTC_LDR = VK_FALSE,
                        .textureCompressionBC = VK_FALSE,
                        .occlusionQueryPrecise = VK_FALSE,
                        .pipelineStatisticsQuery = VK_TRUE,
                        .vertexPipelineStoresAndAtomics = VK_FALSE,
                        .fragmentStoresAndAtomics = VK_FALSE,
                        .shaderTessellationAndGeometryPointSize = VK_FALSE,
                        .shaderImageGatherExtended = VK_FALSE,
                        .shaderStorageImageExtendedFormats = VK_FALSE,
                        .shaderStorageImageMultisample = VK_FALSE,
                        .shaderStorageImageReadWithoutFormat = VK_FALSE,
                        .shaderStorageImageWriteWithoutFormat = VK_FALSE,
                        .shaderUniformBufferArrayDynamicIndexing = VK_TRUE,
                        .shaderSampledImageArrayDynamicIndexing = VK_TRUE,
                        .shaderStorageBufferArrayDynamicIndexing = VK_TRUE,
                        .shaderStorageImageArrayDynamicIndexing = VK_TRUE,
                        .shaderClipDistance = VK_FALSE,
                        .shaderCullDistance = VK_FALSE,
                        .shaderFloat64 = VK_FALSE,
                        .shaderInt64 = VK_FALSE,
                        .shaderInt16 = VK_TRUE,
                        .shaderResourceResidency = VK_FALSE,
                        .shaderResourceMinLod = VK_FALSE,
                        .sparseBinding = VK_FALSE,
                        .sparseResidencyBuffer = VK_FALSE,
                        .sparseResidencyImage2D = VK_FALSE,
                        .sparseResidencyImage3D = VK_FALSE,
                        .sparseResidency2Samples = VK_FALSE,
                        .sparseResidency4Samples = VK_FALSE,
                        .sparseResidency8Samples = VK_FALSE,
                        .sparseResidency16Samples = VK_FALSE,
                        .sparseResidencyAliased = VK_FALSE,
                        .variableMultisampleRate = VK_FALSE,
                        .inheritedQueries = VK_FALSE,
                    })
                    .set_required_features_11({
                        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
                        .pNext = nullptr,
                        .storageBuffer16BitAccess = VK_TRUE,
                        .uniformAndStorageBuffer16BitAccess = VK_TRUE,
                        .storagePushConstant16 = VK_FALSE,
                        .storageInputOutput16 = VK_FALSE,
                        .multiview = VK_FALSE,
                        .multiviewGeometryShader = VK_FALSE,
                        .multiviewTessellationShader = VK_FALSE,
                        .variablePointersStorageBuffer = VK_FALSE,
                        .variablePointers = VK_FALSE,
                        .protectedMemory = VK_FALSE,
                        .samplerYcbcrConversion = VK_FALSE,
                        .shaderDrawParameters = VK_FALSE,
                    })
                    .set_required_features_12({
                        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                        .pNext = nullptr,
                        .samplerMirrorClampToEdge = VK_FALSE,
                        .drawIndirectCount = VK_FALSE,
                        .storageBuffer8BitAccess = VK_TRUE,
                        .uniformAndStorageBuffer8BitAccess = VK_TRUE,
                        .storagePushConstant8 = VK_FALSE,
                        .shaderBufferInt64Atomics = VK_FALSE,
                        .shaderSharedInt64Atomics = VK_FALSE,
                        .shaderFloat16 = VK_TRUE,
                        .shaderInt8 = VK_FALSE,
                        .descriptorIndexing = VK_FALSE,
                        .shaderInputAttachmentArrayDynamicIndexing = VK_FALSE,
                        .shaderUniformTexelBufferArrayDynamicIndexing = VK_FALSE,
                        .shaderStorageTexelBufferArrayDynamicIndexing = VK_FALSE,
                        .shaderUniformBufferArrayNonUniformIndexing = VK_TRUE,
                        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
                        .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
                        .shaderStorageImageArrayNonUniformIndexing = VK_TRUE,
                        .shaderInputAttachmentArrayNonUniformIndexing = VK_FALSE,
                        .shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE,
                        .shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE,
                        .descriptorBindingUniformBufferUpdateAfterBind = VK_FALSE,
                        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
                        .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
                        .descriptorBindingStorageBufferUpdateAfterBind = VK_FALSE,
                        .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE,
                        .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE,
                        .descriptorBindingUpdateUnusedWhilePending = VK_FALSE,
                        .descriptorBindingPartiallyBound = VK_TRUE,
                        .descriptorBindingVariableDescriptorCount = VK_TRUE,
                        .runtimeDescriptorArray = VK_TRUE,
                        .samplerFilterMinmax = VK_FALSE,
                        .scalarBlockLayout = VK_FALSE,
                        .imagelessFramebuffer = VK_TRUE,
                        .uniformBufferStandardLayout = VK_FALSE,
                        .shaderSubgroupExtendedTypes = VK_FALSE,
                        .separateDepthStencilLayouts = VK_TRUE,
                        .hostQueryReset = VK_TRUE,
                        .timelineSemaphore = VK_FALSE,
                        .bufferDeviceAddress = VK_TRUE,
                        .bufferDeviceAddressCaptureReplay = VK_FALSE,
                        .bufferDeviceAddressMultiDevice = VK_FALSE,
                        .vulkanMemoryModel = VK_FALSE,
                        .vulkanMemoryModelDeviceScope = VK_FALSE,
                        .vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE,
                        .shaderOutputViewportIndex = VK_FALSE,
                        .shaderOutputLayer = VK_FALSE,
                        .subgroupBroadcastDynamicId = VK_FALSE,
                    })
                    .set_required_features_13({
                        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                        .pNext = nullptr,
                        .robustImageAccess = VK_FALSE,
                        .inlineUniformBlock = VK_FALSE,
                        .descriptorBindingInlineUniformBlockUpdateAfterBind = VK_FALSE,
                        .pipelineCreationCacheControl = VK_FALSE,
                        .privateData = VK_FALSE,
                        .shaderDemoteToHelperInvocation = VK_FALSE,
                        .shaderTerminateInvocation = VK_FALSE,
                        .subgroupSizeControl = VK_FALSE,
                        .computeFullSubgroups = VK_FALSE,
                        .synchronization2 = VK_TRUE,
                        .textureCompressionASTC_HDR = VK_FALSE,
                        .shaderZeroInitializeWorkgroupMemory = VK_FALSE,
                        .dynamicRendering = VK_TRUE,
                        .shaderIntegerDotProduct = VK_FALSE,
                        .maintenance4 = VK_FALSE,
                    })
                    .add_required_extension_features(extended_dynamic_state);
            return selector;
        }
    } // namespace

    graphics::render_device& render_context::create_device(uint32_t idx)
    {
        auto devices = enumerate_suitable_devices();
        assert(idx < devices.size() && "Device query index out of bounds.");

        if (idx >= _devices.size())
        {
            _devices.resize(idx + 1);
        }

        vkb::PhysicalDeviceSelector selector = select_device(_instance);

        auto selection = selector.select_devices();
        _devices[idx] = make_unique<render_device>(_alloc, _instance, (*selection)[devices[idx].id]);

        return *(_devices[idx]);
    }

    vector<physical_device_context> render_context::enumerate_suitable_devices()
    {
        vkb::PhysicalDeviceSelector selector = select_device(_instance);

        auto selection = selector.select_devices();
        vector<physical_device_context> devices;

        if (selection)
        {
            for (size_t i = 0; i < selection->size(); ++i)
            {
                if ((*selection)[i].properties.limits.maxPerStageDescriptorSampledImages < 512)
                {
                    continue;
                }

                devices.push_back(physical_device_context{
                    .id = static_cast<uint32_t>(i),
                    .name = (*selection)[i].name.c_str(),
                });
            }
        }

        return devices;
    }

    resource_deletion_queue::resource_deletion_queue(size_t frames_in_flight) : _frames_in_flight{frames_in_flight}
    {
    }

    void resource_deletion_queue::add_to_queue(size_t current_frame, function<void()> deleter)
    {
        _queue.push_back(delete_info{
            .frame = current_frame,
            .deleter = deleter,
        });
    }

    void resource_deletion_queue::flush_frame(size_t current_frame)
    {
        std::for_each(std::begin(_queue), std::end(_queue), [this, current_frame](const delete_info& info) {
            if (info.frame + _frames_in_flight >= current_frame)
            {
                info.deleter();
            }
        });

        const auto new_end =
            std::remove_if(std::begin(_queue), std::end(_queue), [this, current_frame](const delete_info& info) {
                return info.frame + _frames_in_flight >= current_frame;
            });

        _queue.erase(new_end, std::end(_queue));
    }

    void resource_deletion_queue::flush_all()
    {
        std::for_each(std::begin(_queue), std::end(_queue), [](const delete_info& info) { info.deleter(); });
        _queue.clear();
    }

    command_list::command_list(VkCommandBuffer buffer, vkb::DispatchTable* dispatch, render_device* device)
        : _cmds{buffer}, _dispatch{dispatch}, _device{device}
    {
    }

    command_list::operator VkCommandBuffer() const noexcept
    {
        return _cmds;
    }

    command_list& command_list::push_constants(uint32_t offset, span<const byte> data,
                                               compute_pipeline_resource_handle handle)
    {
        auto pipeline = _device->access_compute_pipeline(handle);
        _dispatch->cmdPushConstants(_cmds, pipeline->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, offset,
                                    static_cast<uint32_t>(data.size()), data.data());

        return *this;
    }

    command_list& command_list::push_constants(uint32_t offset, span<const byte> data,
                                               graphics_pipeline_resource_handle handle)
    {
        auto pipeline = _device->access_graphics_pipeline(handle);
        _dispatch->cmdPushConstants(_cmds, pipeline->pipeline_layout, VK_SHADER_STAGE_ALL_GRAPHICS, offset,
                                    static_cast<uint32_t>(data.size()), data.data());

        return *this;
    }

    command_list& command_list::set_viewport(float x, float y, float width, float height, float min_depth,
                                             float max_depth, bool flip)
    {
        VkViewport vp = {
            .x = x,
            .y = flip ? height - y : y,
            .width = width,
            .height = flip ? -height : height,
            .minDepth = min_depth,
            .maxDepth = max_depth,
        };

        _dispatch->cmdSetViewportWithCount(_cmds, 1, &vp);

        return *this;
    }

    command_list& command_list::set_scissor_region(int32_t x, int32_t y, uint32_t width, uint32_t height)
    {
        VkRect2D scissor = {
            .offset{
                .x = x,
                .y = y,
            },
            .extent{
                .width = width,
                .height = height,
            },
        };

        _dispatch->cmdSetScissorWithCount(_cmds, 1, &scissor);

        return *this;
    }

    command_list& command_list::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex,
                                     uint32_t first_index)
    {
        _dispatch->cmdDraw(_cmds, vertex_count, instance_count, first_vertex, first_index);

        return *this;
    }

    command_list& command_list::draw(buffer_resource_handle buf, uint32_t offset, uint32_t count, uint32_t stride)
    {
        _dispatch->cmdDrawIndirect(_cmds, _device->access_buffer(buf)->vk_buffer, offset, count, stride);

        return *this;
    }

    command_list& command_list::draw_indexed(buffer_resource_handle buf, uint32_t offset, uint32_t count,
                                             uint32_t stride)
    {
        _dispatch->cmdDrawIndexedIndirect(_cmds, _device->access_buffer(buf)->vk_buffer, offset, count, stride);

        return *this;
    }

    command_list& command_list::use_pipeline(graphics_pipeline_resource_handle pipeline)
    {
        auto vk_pipeline = _device->access_graphics_pipeline(pipeline);
        _dispatch->cmdBindPipeline(_cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline->pipeline);

        return *this;
    }

    command_list& command_list::use_index_buffer(buffer_resource_handle buf, uint32_t offset)
    {
        auto vk_buf = _device->access_buffer(buf);
        _dispatch->cmdBindIndexBuffer(_cmds, vk_buf->vk_buffer, offset, VK_INDEX_TYPE_UINT32);

        return *this;
    }

    command_list& command_list::set_cull_mode(bool front, bool back)
    {
        VkCullModeFlags cull_mode = 0;
        if (front)
        {
            cull_mode |= VK_CULL_MODE_FRONT_BIT;
        }
        
        if (back)
        {
            cull_mode |= VK_CULL_MODE_BACK_BIT;
        }

        _dispatch->cmdSetCullMode(_cmds, cull_mode);

        return *this;
    }

    command_list& command_list::use_pipeline(compute_pipeline_resource_handle pipeline)
    {
        auto vk_pipeline = _device->access_compute_pipeline(pipeline);
        _dispatch->cmdBindPipeline(_cmds, VK_PIPELINE_BIND_POINT_COMPUTE, vk_pipeline->pipeline);

        return *this;
    }

    command_list& command_list::dispatch(uint32_t x, uint32_t y, uint32_t z)
    {
        _dispatch->cmdDispatch(_cmds, x, y, z);
        return *this;
    }

    command_list& command_list::blit(image_resource_handle src, image_resource_handle dst)
    {
        auto src_img = _device->access_image(src);
        auto dst_img = _device->access_image(dst);

        VkImageBlit region = {
            .srcSubresource{
                .aspectMask = src_img->view_info.subresourceRange.aspectMask,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcOffsets{
                {
                    .x = 0,
                    .y = 0,
                    .z = 0,
                },
                {
                    .x = static_cast<int32_t>(src_img->img_info.extent.width),
                    .y = static_cast<int32_t>(src_img->img_info.extent.height),
                    .z = 1,
                },
            },
            .dstSubresource{
                .aspectMask = src_img->view_info.subresourceRange.aspectMask,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .dstOffsets{
                {
                    .x = 0,
                    .y = 0,
                    .z = 0,
                },
                {
                    .x = static_cast<int32_t>(dst_img->img_info.extent.width),
                    .y = static_cast<int32_t>(dst_img->img_info.extent.height),
                    .z = 1,
                },
            },
        };

        _dispatch->cmdBlitImage(_cmds, src_img->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_img->image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);

        return *this;
    }

    command_list& command_list::copy(buffer_resource_handle src, buffer_resource_handle dst, size_t src_offset,
                                     size_t dst_offset, size_t byte_count)
    {
        auto src_buf = _device->access_buffer(src);
        auto dst_buf = _device->access_buffer(dst);

        assert(src_buf->info.size > src_offset &&
               "Buffer copy source size must be larger than the source copy offset.");
        assert(dst_buf->info.size > dst_offset &&
               "Buffer copy source size must be larger than the source copy offset.");

        if (byte_count == std::numeric_limits<size_t>::max())
        {
            size_t src_bytes_available = src_buf->info.size - src_offset;
            size_t dst_bytes_available = dst_buf->info.size - dst_offset;
            size_t bytes_available = std::min(src_bytes_available, dst_bytes_available);

            byte_count = bytes_available;
        }

        assert(src_offset + byte_count <= src_buf->info.size &&
               "src_offset + byte_count must be less than the size of the source buffer.");
        assert(dst_offset + byte_count <= dst_buf->info.size &&
               "src_offset + byte_count must be less than the size of the source buffer.");

        VkBufferCopy copy = {
            .srcOffset = src_offset,
            .dstOffset = dst_offset,
            .size = byte_count,
        };

        _dispatch->cmdCopyBuffer(_cmds, src_buf->vk_buffer, dst_buf->vk_buffer, 1, &copy);

        return *this;
    }

    command_list& command_list::copy(buffer_resource_handle src, image_resource_handle dst, size_t buffer_offset,
                                     uint32_t region_width, uint32_t region_height, uint32_t mip_level,
                                     int32_t offset_x, int32_t offset_y)
    {
        VkBufferImageCopy copy = {
            .bufferOffset = buffer_offset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = mip_level,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset{
                .x = offset_x,
                .y = offset_y,
                .z = 0,
            },
            .imageExtent{
                .width = region_width,
                .height = region_height,
                .depth = 1,
            },
        };

        _dispatch->cmdCopyBufferToImage(_cmds, _device->access_buffer(src)->vk_buffer, _device->access_image(dst)->image,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        return *this;
    }

    command_list& command_list::clear_color(image_resource_handle handle, float r, float g, float b, float a)
    {
        VkClearColorValue color = {
            .float32{r, g, b, a},
        };

        VkImageSubresourceRange range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_ARRAY_LAYERS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        };

        _dispatch->cmdClearColorImage(_cmds, _device->access_image(handle)->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      &color, 1, &range);

        return *this;
    }

    command_list& command_list::transition_image(image_resource_handle img, image_resource_usage old_usage,
                                                 image_resource_usage new_usage)
    {
        auto vk_img = _device->access_image(img);
        return transition_image(img, old_usage, new_usage, 0, vk_img->img_info.mipLevels);
    }

    command_list& command_list::transition_image(image_resource_handle img, image_resource_usage old_usage,
                                                 image_resource_usage new_usage, uint32_t base_mip, uint32_t mip_count)
    {
        if (old_usage == new_usage)
        {
            return *this;
        }

        auto vk_img = _device->access_image(img);

        VkImageMemoryBarrier img_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_NONE,
            .dstAccessMask = VK_ACCESS_NONE,
            .oldLayout = compute_layout(old_usage),
            .newLayout = compute_layout(new_usage),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vk_img->image,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = base_mip,
                .levelCount = mip_count,
                .baseArrayLayer = 0,
                .layerCount = vk_img->img_info.arrayLayers,
            },
        };

        VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        if (old_usage == image_resource_usage::UNDEFINED && new_usage == image_resource_usage::TRANSFER_DESTINATION)
        {
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        else if (old_usage == image_resource_usage::TRANSFER_DESTINATION && new_usage == image_resource_usage::SAMPLED)
        {
            dst_stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        }
        else if (old_usage == image_resource_usage::TRANSFER_DESTINATION && new_usage == image_resource_usage::STORAGE)
        {
            dst_stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        }
        else if (old_usage == image_resource_usage::TRANSFER_DESTINATION &&
                 new_usage == image_resource_usage::TRANSFER_SOURCE)
        {
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        }
        else if (old_usage == image_resource_usage::TRANSFER_SOURCE &&
                 new_usage == image_resource_usage::TRANSFER_DESTINATION)
        {
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        else
        {
            logger->warn("Unexpected transition.");
        }

        _dispatch->cmdPipelineBarrier(_cmds, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);

        return *this;
    }

    command_list& command_list::generate_mip_chain(image_resource_handle img, image_resource_usage usage,
                                                   uint32_t base_mip, uint32_t mip_count)
    {
        auto vk_img = _device->access_image(img);
        auto img_mip_count = vk_img->img_info.mipLevels;
        auto mips_to_generate = std::min(mip_count, img_mip_count) - 1;

        uint32_t src_width = vk_img->img_info.extent.width;
        uint32_t src_height = vk_img->img_info.extent.height;

        for (uint32_t i = base_mip; i < base_mip + mips_to_generate; ++i)
        {
            uint32_t dst_width = src_width / 2;
            uint32_t dst_height = src_height / 2;

            VkImageBlit region = {
                .srcSubresource{
                    .aspectMask = vk_img->view_info.subresourceRange.aspectMask,
                    .mipLevel = i,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .srcOffsets{
                    {
                        .x = 0,
                        .y = 0,
                        .z = 0,
                    },
                    {
                        .x = static_cast<int32_t>(src_width),
                        .y = static_cast<int32_t>(src_height),
                        .z = 1,
                    },
                },
                .dstSubresource{
                    .aspectMask = vk_img->view_info.subresourceRange.aspectMask,
                    .mipLevel = i + 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .dstOffsets{
                    {
                        .x = 0,
                        .y = 0,
                        .z = 0,
                    },
                    {
                        .x = static_cast<int32_t>(dst_width),
                        .y = static_cast<int32_t>(dst_height),
                        .z = 1,
                    },
                },
            };

            if (i == base_mip)
            {
                transition_image(img, usage, image_resource_usage::TRANSFER_SOURCE, i, 1);
            }

            transition_image(img, usage, image_resource_usage::TRANSFER_DESTINATION, i + 1, 1);

            _dispatch->cmdBlitImage(_cmds, vk_img->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk_img->image,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);

            transition_image(img, image_resource_usage::TRANSFER_SOURCE, usage, i, 1);

            if (i == base_mip + mips_to_generate - 1)
            {
                transition_image(img, image_resource_usage::TRANSFER_DESTINATION, usage, i + 1, 1);
            }
            else
            {
                transition_image(img, image_resource_usage::TRANSFER_DESTINATION, image_resource_usage::TRANSFER_SOURCE,
                                 i + 1, 1);
            }

            src_width = dst_width;
            src_height = dst_height;
        }

        return *this;
    }

    void command_buffer_allocator::reset()
    {
        dispatch->resetCommandPool(pool, 0);
        command_buffer_index = 0;
    }

    command_list command_buffer_allocator::allocate()
    {
        if (command_buffer_index >= cached_commands.size())
        {
            VkCommandBuffer buf = VK_NULL_HANDLE;
            VkCommandBufferAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            [[maybe_unused]] VkResult result = dispatch->allocateCommandBuffers(&alloc_info, &buf);
            assert(result == VK_SUCCESS);
            cached_commands.push_back(buf);
        }
        VkCommandBuffer cmds = cached_commands[command_buffer_index++];
        return command_list(cmds, dispatch, device);
    }

    void command_buffer_allocator::release()
    {
        if (!cached_commands.empty())
        {
            dispatch->freeCommandBuffers(pool, static_cast<uint32_t>(cached_commands.size()), cached_commands.data());
        }
        dispatch->destroyCommandPool(pool, nullptr);
    }

    command_buffer_allocator command_buffer_recycler::acquire(vkb::DispatchTable& dispatch, render_device* device)
    {
        if (global_pool.empty())
        {
            VkCommandPoolCreateInfo ci = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueFamilyIndex = queue.queue_family_index,
            };

            VkCommandPool pool;
            [[maybe_unused]] auto result = dispatch.createCommandPool(&ci, nullptr, &pool);

            assert(result == VK_SUCCESS);

            command_buffer_allocator allocator{
                .queue = queue,
                .pool = pool,
                .cached_commands = {},
                .dispatch = &dispatch,
                .device = device,
            };
            global_pool.push_back(allocator);
        }

        auto last = global_pool.back();
        global_pool.pop_back();
        return last;
    }

    void command_buffer_recycler::release(command_buffer_allocator&& allocator, size_t current_frame)
    {
        recycle_pool.push_back(command_buffer_recycle_payload{
            .allocator = std::move(allocator),
            .recycled_frame = current_frame,
        });
    }

    void command_buffer_recycler::recycle(size_t current_frame, [[maybe_unused]] vkb::DispatchTable& dispatch)
    {
        while (!recycle_pool.empty())
        {
            if (recycle_pool.front().recycled_frame + frames_in_flight > current_frame)
            {
                break;
            }

            command_buffer_recycle_payload payload = recycle_pool.front();
            recycle_pool.pop_front();

            payload.allocator.reset();
            global_pool.push_back(std::move(payload.allocator));
        }
    }

    void command_buffer_recycler::release_all([[maybe_unused]] vkb::DispatchTable& dispatch)
    {
        // delete from the global pool
        for (auto& alloc : global_pool)
        {
            alloc.release();
        }

        for (auto& alloc : recycle_pool)
        {
            alloc.allocator.release();
        }
    }

    VkFence sync_primitive_recycler::acquire_fence(vkb::DispatchTable& dispatch)
    {
        if (global_fence_pool.empty())
        {
            VkFenceCreateInfo create = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
            };

            VkFence fen{VK_NULL_HANDLE};
            [[maybe_unused]] auto result = dispatch.createFence(&create, nullptr, &fen);
            assert(result == VK_SUCCESS);
            return fen;
        }
        VkFence fen = global_fence_pool.back();
        global_fence_pool.pop_back();
        return fen;
    }

    VkSemaphore sync_primitive_recycler::acquire_semaphore(vkb::DispatchTable& dispatch)
    {
        if (global_semaphore_pool.empty())
        {
            VkSemaphoreCreateInfo create = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
            };

            VkSemaphore sem{VK_NULL_HANDLE};
            [[maybe_unused]] auto result = dispatch.createSemaphore(&create, nullptr, &sem);
            assert(result == VK_SUCCESS);
            return sem;
        }

        VkSemaphore sem = global_semaphore_pool.back();
        global_semaphore_pool.pop_back();
        return sem;
    }

    void sync_primitive_recycler::release(VkFence&& fen, size_t current_frame)
    {
        fence_recycle_payload payload{
            .fence = std::move(fen),
            .recycled_frame = current_frame,
        };

        recycle_fence_pool.push_back(std::move(payload));
    }

    void sync_primitive_recycler::release(VkSemaphore&& sem, size_t current_frame)
    {
        semaphore_recycle_payload payload{
            .sem = std::move(sem),
            .recycled_frame = current_frame,
        };

        recycle_semaphore_pool.push_back(std::move(payload));
    }

    void sync_primitive_recycler::recycle(size_t current_frame, [[maybe_unused]] vkb::DispatchTable& dispatch)
    {
        while (!recycle_fence_pool.empty())
        {
            if (recycle_fence_pool.front().recycled_frame + frames_in_flight > current_frame)
            {
                break;
            }

            fence_recycle_payload payload = recycle_fence_pool.front();
            recycle_fence_pool.pop_front();

            global_fence_pool.push_back(std::move(payload.fence));
        }

        while (!recycle_semaphore_pool.empty())
        {
            if (recycle_semaphore_pool.front().recycled_frame + frames_in_flight > current_frame)
            {
                break;
            }

            semaphore_recycle_payload payload = recycle_semaphore_pool.front();
            recycle_semaphore_pool.pop_front();

            global_semaphore_pool.push_back(std::move(payload.sem));
        }
    }

    void sync_primitive_recycler::release_all(vkb::DispatchTable& dispatch)
    {
        for (auto sem : global_semaphore_pool)
        {
            dispatch.destroySemaphore(sem, nullptr);
        }

        for (auto sem : recycle_semaphore_pool)
        {
            dispatch.destroySemaphore(sem.sem, nullptr);
        }

        for (auto fence : global_fence_pool)
        {
            dispatch.destroyFence(fence, nullptr);
        }

        for (auto fence : recycle_fence_pool)
        {
            dispatch.destroyFence(fence.fence, nullptr);
        }
    }

    command_execution_service::command_execution_service(vkb::DispatchTable& dispatch, render_device& device)
        : _dispatch{&dispatch}, _device{&device}
    {
        VkCommandPoolCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = device.get_queue().queue_family_index,
        };

        [[maybe_unused]] auto res = _dispatch->createCommandPool(&create_info, nullptr, &_pool);
        assert(res == VK_SUCCESS);

        VkCommandBufferAllocateInfo alloc_ci = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = _pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        VkCommandBuffer buf;
        res = _dispatch->allocateCommandBuffers(&alloc_ci, &buf);
        assert(res == VK_SUCCESS);

        _cmds.emplace(buf, _dispatch, _device);
    }

    command_execution_service::~command_execution_service()
    {
        _dispatch->destroyCommandPool(_pool, nullptr);
    }

    command_list& command_execution_service::get_commands()
    {
        if (!_is_recording)
        {
            VkCommandBufferBeginInfo begin = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = 0,
                .pInheritanceInfo = nullptr,
            };
            VkCommandBuffer buf = _cmds.value();
            [[maybe_unused]] auto res = _dispatch->beginCommandBuffer(buf, &begin);
            assert(res == VK_SUCCESS);
            _is_recording = true;
        }
        return _cmds.value();
    }

    void command_execution_service::submit_and_wait()
    {
        if (!_is_recording) [[unlikely]]
        {
            return;
        }

        VkCommandBuffer cmds = _cmds.value();

        _dispatch->endCommandBuffer(cmds);

        VkSubmitInfo submit = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmds,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = 0,
        };

        VkFence fence = _device->acquire_fence();
        _dispatch->queueSubmit(_device->get_queue().queue, 1, &submit, fence);
        _dispatch->waitForFences(1, &fence, VK_TRUE, UINT64_MAX);

        _device->release_fence(std::move(fence));

        // Reset the command pool
        _dispatch->freeCommandBuffers(_pool, 1, &cmds);
        _dispatch->resetCommandPool(_pool, 0);

        // Fetch a new command buffer
        VkCommandBufferAllocateInfo alloc_ci = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = _pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        [[maybe_unused]] auto res = _dispatch->allocateCommandBuffers(&alloc_ci, &cmds);
        assert(res == VK_SUCCESS);

        _cmds.emplace(cmds, _dispatch, _device);

        _is_recording = false;
    }
} // namespace tempest::graphics::vk