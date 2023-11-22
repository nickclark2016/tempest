#include "vk_render_device.hpp"

#include "../../windowing/glfw_window.hpp"

#include <tempest/logger.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

namespace tempest::graphics::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::vk::render_device"}});

        VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                      VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                      void* pUserData)
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
                .add_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
                .add_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
                .add_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
                .add_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
                .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
                .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT)
                .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
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

        constexpr VkImageUsageFlagBits to_vulkan(image_resource_usage usage)
        {
            switch (usage)
            {
            case image_resource_usage::COLOR_ATTACHMENT:
                return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            case image_resource_usage::DEPTH_ATTACHMENT:
                return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            case image_resource_usage::SAMPLED:
                return VK_IMAGE_USAGE_SAMPLED_BIT;
            case image_resource_usage::STORAGE:
                return VK_IMAGE_USAGE_STORAGE_BIT;
            case image_resource_usage::TRANSFER_SOURCE:
                return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            case image_resource_usage::TRANSFER_DESTINATION:
                return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            }

            logger->critical("Logic Error: Failed to determine proper VkImageUsageFlagBits. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr VkImageAspectFlags to_vulkan_aspect(image_resource_usage usage)
        {
            switch (usage)
            {
            case image_resource_usage::COLOR_ATTACHMENT:
                return VK_IMAGE_ASPECT_COLOR_BIT;
            case image_resource_usage::DEPTH_ATTACHMENT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            }

            return VK_IMAGE_ASPECT_NONE;
        }

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
            case resource_format::RGBA8_SRGB:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case resource_format::RG32_FLOAT:
                return VK_FORMAT_R32G32_SFLOAT;
            case resource_format::RG32_UINT:
                return VK_FORMAT_R32G32_UINT;
            case resource_format::RGB32_FLOAT:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case resource_format::RGBA32_FLOAT:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case resource_format::D32_FLOAT:
                return VK_FORMAT_D32_SFLOAT;
            case resource_format::UNKNOWN:
                return VK_FORMAT_UNDEFINED;
            }

            logger->critical("Logic Error: Failed to determine proper VkFormat. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr std::size_t get_format_size(resource_format fmt)
        {
            switch (fmt)
            {
            case resource_format::RGBA8_SRGB:
                return 4;
            case resource_format::RG32_FLOAT:
                return 2 * sizeof(float);
            case resource_format::RG32_UINT:
                return 2 * sizeof(std::uint32_t);
            case resource_format::RGB32_FLOAT:
                return 3 * sizeof(float);
            case resource_format::RGBA32_FLOAT:
                return 4 * sizeof(float);
            case resource_format::D32_FLOAT:
                return sizeof(float);
            case resource_format::UNKNOWN:
                return 0;
            }

            logger->critical("Logic Error: Failed to determine proper VkFormat. Forcing exit.");
            std::exit(EXIT_FAILURE);
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
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            case descriptor_binding_type::CONSTANT_BUFFER:
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

        constexpr VkBlendFactor to_vulkan(blend_factor factor, bool is_color)
        {
            switch (factor)
            {
            case blend_factor::ZERO:
                return VK_BLEND_FACTOR_ZERO;
            case blend_factor::ONE:
                return VK_BLEND_FACTOR_ONE;
            case blend_factor::SRC:
                return is_color ? VK_BLEND_FACTOR_SRC_COLOR : VK_BLEND_FACTOR_SRC_ALPHA;
            case blend_factor::ONE_MINUS_SRC:
                return is_color ? VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case blend_factor::DST:
                return is_color ? VK_BLEND_FACTOR_DST_COLOR : VK_BLEND_FACTOR_DST_ALPHA;
            case blend_factor::ONE_MINUS_DST:
                return is_color ? VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR : VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            }

            logger->critical("Logic Error: Failed to determine proper VkBlendFactor. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        constexpr VkColorComponentFlags compute_blend_write_mask(resource_format fmt)
        {
            switch (fmt)
            {
            case resource_format::RG32_FLOAT:
                [[fallthrough]];
            case resource_format::RG32_UINT:
                return VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
            case resource_format::RGB32_FLOAT:
                return VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
            case resource_format::RGBA8_SRGB:
                [[fallthrough]];
            case resource_format::RGBA32_FLOAT:
                return VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                       VK_COLOR_COMPONENT_A_BIT;
            case resource_format::D32_FLOAT: {
                logger->critical("Logic Error: Cannot compute color component mask of depth format.");
                break;
            }
            }

            logger->critical("Logic Error: Failed to determine proper VkColorComponentFlags. Forcing exit.");
            std::exit(EXIT_FAILURE);
        }

        static constexpr std::uint32_t IMAGE_POOL_SIZE = 4096;
        static constexpr std::uint32_t BUFFER_POOL_SIZE = 512;
        static constexpr std::uint32_t GRAPHICS_PIPELINE_POOL_SIZE = 256;
        static constexpr std::uint32_t SWAPCHAIN_POOL_SIZE = 8;
        static constexpr std::uint32_t SEMAPHORE_POOL_SIZE = 512;
        static constexpr std::uint32_t FENCE_POOL_SIZE = 256;
    } // namespace

    render_device::render_device(core::allocator* alloc, vkb::Instance instance, vkb::PhysicalDevice physical)
        : _alloc{alloc}, _instance{instance}, _physical{physical}
    {
        _images.emplace(_alloc, IMAGE_POOL_SIZE, static_cast<std::uint32_t>(sizeof(image)));
        _buffers.emplace(_alloc, BUFFER_POOL_SIZE, static_cast<std::uint32_t>(sizeof(buffer)));
        _graphics_pipelines.emplace(_alloc, GRAPHICS_PIPELINE_POOL_SIZE,
                                    static_cast<std::uint32_t>(sizeof(graphics_pipeline)));
        _swapchains.emplace(_alloc, SWAPCHAIN_POOL_SIZE, static_cast<std::uint32_t>(sizeof(swapchain)));
        _delete_queue.emplace(_frames_in_flight);

        auto queue_families = _physical.get_queue_families();
        std::unordered_map<std::uint32_t, std::uint32_t> queues_allocated;

        auto family_matcher = [&](VkQueueFlags flags)
            -> std::optional<std::tuple<VkQueueFamilyProperties, std::uint32_t, std::uint32_t>> {
            std::optional<std::tuple<VkQueueFamilyProperties, std::uint32_t, std::uint32_t>> best_match;

            std::uint32_t family_idx = 0;
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
            .queue_family_index{std::get<1>(*queue_family_info)},
            .queue_index{std::get<2>(*queue_family_info)},
            .flags{std::get<0>(*queue_family_info).queueFlags},
        };
        _dispatch.getDeviceQueue(_queue.queue_family_index, _queue.queue_index, &_queue.queue);

        VmaVulkanFunctions fns = {
            .vkGetInstanceProcAddr{_instance.fp_vkGetInstanceProcAddr},
            .vkGetDeviceProcAddr{_device.fp_vkGetDeviceProcAddr},
        };

        VmaAllocatorCreateInfo ci = {
            .physicalDevice{physical.physical_device},
            .device{_device.device},
            .pAllocationCallbacks{nullptr},
            .pVulkanFunctions{&fns},
            .instance{_instance.instance},
        };

        VmaAllocator allocator;
        const auto result = vmaCreateAllocator(&ci, &allocator);
        _vk_alloc = allocator;

        _per_frame.reserve(_frames_in_flight);

        for (std::size_t i = 0; i < _frames_in_flight; ++i)
        {
            per_frame_data data = {

            };

            VkSemaphoreCreateInfo sem_ci = {
                .sType{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO},
                .flags{},
            };

            VkFenceCreateInfo fence_ci = {
                .sType{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO},
                .flags{VK_FENCE_CREATE_SIGNALED_BIT},
            };

            _dispatch.createSemaphore(&sem_ci, nullptr, &data.present_ready);
            _dispatch.createSemaphore(&sem_ci, nullptr, &data.render_ready);
            _dispatch.createFence(&fence_ci, nullptr, &data.render_fence);

            _per_frame.push_back(data);
        }

        _recycled_cmd_buf_pool = command_buffer_recycler{
            .frames_in_flight{_frames_in_flight},
            .queue{_queue},
        };

        _sync_prim_recycler = sync_primitive_recycler{
            .frames_in_flight{_frames_in_flight},
        };
    }

    render_device::~render_device()
    {
        _dispatch.deviceWaitIdle();

        _delete_queue->flush_all();
        _recycled_cmd_buf_pool.release_all(_dispatch);
        _sync_prim_recycler.release_all(_dispatch);

        for (const auto& frame : _per_frame)
        {
            _dispatch.destroySemaphore(frame.present_ready, nullptr);
            _dispatch.destroySemaphore(frame.render_ready, nullptr);
            _dispatch.destroyFence(frame.render_fence, nullptr);
        }

        _per_frame.clear();

        vmaDestroyAllocator(_vk_alloc);
        vkb::destroy_device(_device);
    }

    void render_device::start_frame() noexcept
    {
    }

    void render_device::end_frame() noexcept
    {
        _recycled_cmd_buf_pool.recycle(_current_frame, _dispatch);
        _delete_queue->flush_frame(_current_frame);
        _current_frame++;
    }

    buffer* render_device::access_buffer(buffer_resource_handle handle) noexcept
    {
        return reinterpret_cast<buffer*>(_buffers->access({
            .index{handle.id},
            .generation{handle.generation},
        }));
    }

    const buffer* render_device::access_buffer(buffer_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const buffer*>(_buffers->access({
            .index{handle.id},
            .generation{handle.generation},
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
        usage |= ci.storage_buffer ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 0;
        usage |= ci.transfer_destination ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0;
        usage |= ci.transfer_source ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0;
        usage |= ci.uniform_buffer ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0;
        usage |= ci.vertex_buffer ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 0;

        VkBufferCreateInfo buf_ci = {
            .sType{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .size{ci.size},
            .usage{usage},
            .sharingMode{VK_SHARING_MODE_EXCLUSIVE},
            .queueFamilyIndexCount{0},
            .pQueueFamilyIndices{nullptr},
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
            .flags{alloc_flags},
            .usage{mem_usage},
            .requiredFlags{required},
            .preferredFlags{preferred},
        };

        buffer buf{
            .info{buf_ci},
            .name{std::string{ci.name}},
        };

        auto result = vmaCreateBuffer(_vk_alloc, &buf_ci, &alloc_ci, &buf.buffer, &buf.allocation, &buf.alloc_info);
        if (result != VK_SUCCESS)
        {
            _buffers->release_resource({
                .index{handle.id},
                .generation{handle.generation},
            });

            return buffer_resource_handle();
        }

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
                vmaDestroyBuffer(_vk_alloc, buf->buffer, buf->allocation);
                std::destroy_at(buf);
                _buffers->release_resource({.index{handle.id}, .generation{handle.generation}});
            });
        }
    }

    image* render_device::access_image(image_resource_handle handle) noexcept
    {
        return reinterpret_cast<image*>(_images->access({
            .index{handle.id},
            .generation{handle.generation},
        }));
    }

    const image* render_device::access_image(image_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const image*>(_images->access({
            .index{handle.id},
            .generation{handle.generation},
        }));
    }

    image_resource_handle render_device::create_image(const image_create_info& ci)
    {
        return create_image(ci, allocate_image());
    }

    image_resource_handle render_device::allocate_image()
    {
        auto key = _images->acquire_resource();
        return image_resource_handle(key.index, key.generation);
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
            .sType{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .imageType{to_vulkan(ci.type)},
            .format{to_vulkan(ci.format)},
            .extent{
                .width{ci.width},
                .height{ci.height},
                .depth{ci.depth},
            },
            .mipLevels{ci.mip_count},
            .arrayLayers{ci.layers},
            .samples{to_vulkan(ci.samples)},
            .tiling{VK_IMAGE_TILING_OPTIMAL},
            .usage{imgUsage},
            .sharingMode{VK_SHARING_MODE_EXCLUSIVE},
            .queueFamilyIndexCount{0},
            .pQueueFamilyIndices{nullptr},
            .initialLayout{VK_IMAGE_LAYOUT_UNDEFINED},
        };

        VmaAllocationCreateFlags alloc_flags = 0;
        if (ci.color_attachment || ci.depth_attachment)
        {
            alloc_flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VmaAllocationCreateInfo alloc_create_info = {
            .flags{alloc_flags},
            .usage{VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE},
            .requiredFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
        };

        VkImage img;
        VmaAllocation alloc;
        VmaAllocationInfo alloc_info;

        auto img_result = vmaCreateImage(_vk_alloc, &image_ci, &alloc_create_info, &img, &alloc, &alloc_info);
        if (img_result != VK_SUCCESS)
        {
            _images->release_resource({
                .index{handle.id},
                .generation{handle.generation},
            });
            return image_resource_handle();
        }

        VkImageAspectFlags aspect = 0;
        aspect |= (ci.color_attachment || ci.sampled) ? VK_IMAGE_ASPECT_COLOR_BIT : 0;
        aspect |= ci.depth_attachment ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;

        VkImageViewCreateInfo view_ci = {
            .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .image{img},
            .viewType{to_vulkan_view(ci.type)},
            .format{to_vulkan(ci.format)},
            .components{
                .r{VK_COMPONENT_SWIZZLE_IDENTITY},
                .g{VK_COMPONENT_SWIZZLE_IDENTITY},
                .b{VK_COMPONENT_SWIZZLE_IDENTITY},
                .a{VK_COMPONENT_SWIZZLE_IDENTITY},
            },
            .subresourceRange{
                .aspectMask{aspect},
                .baseMipLevel{0},
                .levelCount{ci.mip_count},
                .baseArrayLayer{0},
                .layerCount{ci.layers},
            },
        };

        VkImageView view;
        auto view_result = _dispatch.createImageView(&view_ci, nullptr, &view);
        if (view_result != VK_SUCCESS)
        {
            vmaDestroyImage(_vk_alloc, img, alloc);

            _images->release_resource({
                .index{handle.id},
                .generation{handle.generation},
            });

            return image_resource_handle();
        }

        image img_info = {
            .allocation{alloc},
            .alloc_info{alloc_info},
            .image{img},
            .view{view},
            .img_info{image_ci},
            .view_info{view_ci},
            .name{std::string(ci.name)},
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
                _images->release_resource({.index{handle.id}, .generation{handle.generation}});
            });
        }
    }

    graphics_pipeline* render_device::access_graphics_pipeline(graphics_pipeline_resource_handle handle) noexcept
    {
        return reinterpret_cast<graphics_pipeline*>(_graphics_pipelines->access({
            .index{handle.id},
            .generation{handle.generation},
        }));
    }

    const graphics_pipeline* render_device::access_graphics_pipeline(
        graphics_pipeline_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const graphics_pipeline*>(_graphics_pipelines->access({
            .index{handle.id},
            .generation{handle.generation},
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
        std::vector<VkDescriptorSetLayout> set_layouts;
        std::vector<VkPushConstantRange> ranges;

        for (const auto& info : ci.layout.set_layouts)
        {
            std::vector<VkDescriptorSetLayoutBinding> bindings;

            for (const auto& binding : info.bindings)
            {
                bindings.push_back(VkDescriptorSetLayoutBinding{
                    .binding{binding.binding_index},
                    .descriptorType{to_vulkan(binding.type)},
                    .descriptorCount{binding.binding_count},
                    .stageFlags{VK_SHADER_STAGE_ALL},
                    .pImmutableSamplers{nullptr},
                });
            }

            VkDescriptorSetLayoutCreateInfo set_layout_ci = {
                .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO},
                .pNext{nullptr},
                .bindingCount{static_cast<std::uint32_t>(bindings.size())},
                .pBindings{bindings.data()},
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
                .stageFlags{VK_SHADER_STAGE_ALL},
                .offset{range.offset},
                .size{range.range},
            });
        }

        VkPipelineLayoutCreateInfo pipeline_layout_ci = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .setLayoutCount{static_cast<std::uint32_t>(set_layouts.size())},
            .pSetLayouts{set_layouts.empty() ? nullptr : set_layouts.data()},
            .pushConstantRangeCount{static_cast<std::uint32_t>(ranges.size())},
            .pPushConstantRanges{ranges.empty() ? nullptr : ranges.data()},
        };

        VkPipelineLayout pipeline_layout;
        auto pipeline_layout_result = _dispatch.createPipelineLayout(&pipeline_layout_ci, nullptr, &pipeline_layout);
        if (pipeline_layout_result != VK_SUCCESS)
        {
            logger->error("Failed to create VkPipelineLayout.");
            return graphics_pipeline_resource_handle();
        }

        std::vector<VkFormat> color_formats;
        for (auto fmt : ci.target.color_attachment_formats)
        {
            color_formats.push_back(to_vulkan(fmt));
        }

        VkPipelineRenderingCreateInfo dynamic_render = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO},
            .pNext{nullptr},
            .viewMask{0},
            .colorAttachmentCount{static_cast<std::uint32_t>(color_formats.size())},
            .pColorAttachmentFormats{color_formats.empty() ? nullptr : color_formats.data()},
            .depthAttachmentFormat{to_vulkan(ci.target.depth_attachment_format)},
            .stencilAttachmentFormat{VK_FORMAT_UNDEFINED},
        };

        VkShaderModuleCreateInfo vertex_ci = {
            .sType{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .codeSize{ci.vertex_shader.bytes.size()},
            .pCode{reinterpret_cast<std::uint32_t*>(ci.vertex_shader.bytes.data())},
        };

        VkShaderModuleCreateInfo fragment_ci = {
            .sType{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .codeSize{ci.fragment_shader.bytes.size()},
            .pCode{reinterpret_cast<std::uint32_t*>(ci.fragment_shader.bytes.data())},
        };

        VkShaderModule vertex_module, fragment_module;
        auto vertex_module_result = _dispatch.createShaderModule(&vertex_ci, nullptr, &vertex_module);
        auto fragment_module_result = _dispatch.createShaderModule(&fragment_ci, nullptr, &fragment_module);

        if (vertex_module_result != VK_SUCCESS || fragment_module_result != VK_SUCCESS)
        {
            logger->error("Failed to create VkShaderModules for pipeline.");
            return graphics_pipeline_resource_handle();
        }

        VkPipelineShaderStageCreateInfo vertex_stage_ci = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
            .pNext{nullptr},
            .flags{},
            .stage{VK_SHADER_STAGE_VERTEX_BIT},
            .module{vertex_module},
            .pName{ci.vertex_shader.entrypoint.data()},
            .pSpecializationInfo{nullptr},
        };

        VkPipelineShaderStageCreateInfo fragment_stage_ci = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
            .pNext{nullptr},
            .flags{},
            .stage{VK_SHADER_STAGE_FRAGMENT_BIT},
            .module{fragment_module},
            .pName{ci.fragment_shader.entrypoint.data()},
            .pSpecializationInfo{nullptr},
        };

        VkPipelineShaderStageCreateInfo stages_ci[] = {
            vertex_stage_ci,
            fragment_stage_ci,
        };

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
            VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
        };

        std::vector<VkVertexInputBindingDescription> vertex_bindings;
        std::vector<VkVertexInputAttributeDescription> vertex_attributes;

        std::unordered_map<std::size_t, std::uint32_t> binding_sizes;

        for (const auto& element : ci.vertex_layout.elements)
        {
            binding_sizes[element.binding] += static_cast<std::uint32_t>(get_format_size(element.format));
        }

        for (const auto& element : ci.vertex_layout.elements)
        {
            auto it = std::find_if(std::begin(vertex_bindings), std::end(vertex_bindings),
                                   [element](const auto& binding) { return binding.binding == element.binding; });
            if (it != std::end(vertex_bindings))
            {
                VkVertexInputBindingDescription binding = {
                    .binding{element.binding},
                    .stride{binding_sizes[element.binding]},
                    .inputRate{VK_VERTEX_INPUT_RATE_VERTEX},
                };

                vertex_bindings.push_back(binding);
            }

            VkVertexInputAttributeDescription attrib = {
                .location{element.location},
                .binding{element.binding},
                .format{to_vulkan(element.format)},
                .offset{element.offset},
            };

            vertex_attributes.push_back(attrib);
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_state = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .vertexBindingDescriptionCount{static_cast<std::uint32_t>(vertex_bindings.size())},
            .pVertexBindingDescriptions{vertex_bindings.empty() ? nullptr : vertex_bindings.data()},
            .vertexAttributeDescriptionCount{static_cast<std::uint32_t>(vertex_attributes.size())},
            .pVertexAttributeDescriptions{vertex_attributes.empty() ? nullptr : vertex_attributes.data()},
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
            .primitiveRestartEnable{VK_FALSE},
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .depthTestEnable{ci.depth_testing.enable_test ? VK_TRUE : VK_FALSE},
            .depthWriteEnable{ci.depth_testing.enable_write ? VK_TRUE : VK_FALSE},
            .depthCompareOp{to_vulkan(ci.depth_testing.depth_test_op)},
            .stencilTestEnable{VK_FALSE},
            .minDepthBounds{ci.depth_testing.min_depth_bounds},
            .maxDepthBounds{ci.depth_testing.max_depth_bounds},
        };

        VkPipelineRasterizationStateCreateInfo raster_state = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .depthClampEnable{ci.depth_testing.clamp_depth ? VK_TRUE : VK_FALSE},
            .rasterizerDiscardEnable{VK_FALSE},
            .polygonMode{VK_POLYGON_MODE_FILL},
            .cullMode{VK_CULL_MODE_BACK_BIT},
            .frontFace{VK_FRONT_FACE_COUNTER_CLOCKWISE},
            .depthBiasEnable{ci.depth_testing.enable_depth_bias ? VK_TRUE : VK_FALSE},
            .depthBiasConstantFactor{ci.depth_testing.depth_bias_constant_factor},
            .depthBiasClamp{ci.depth_testing.depth_bias_clamp},
            .depthBiasSlopeFactor{ci.depth_testing.depth_bias_slope_factor},
            .lineWidth{1.0f},
        };

        std::vector<VkPipelineColorBlendAttachmentState> attachment_blends;
        for (const auto& blend_info : ci.blending.attachment_blend_ops)
        {
            VkPipelineColorBlendAttachmentState state = {
                .blendEnable{blend_info.enabled ? VK_TRUE : VK_FALSE},
                .srcColorBlendFactor{to_vulkan(blend_info.color.src, true)},
                .dstColorBlendFactor{to_vulkan(blend_info.color.dst, true)},
                .colorBlendOp{to_vulkan(blend_info.color.op)},
                .srcAlphaBlendFactor{to_vulkan(blend_info.alpha.src, false)},
                .dstAlphaBlendFactor{to_vulkan(blend_info.alpha.dst, false)},
                .alphaBlendOp{to_vulkan(blend_info.alpha.op)},
                .colorWriteMask{compute_blend_write_mask(ci.target.color_attachment_formats[attachment_blends.size()])},
            };

            attachment_blends.push_back(state);
        };

        VkPipelineColorBlendStateCreateInfo color_blend_state = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .logicOpEnable{VK_FALSE},
            .logicOp{VK_LOGIC_OP_NO_OP},
            .attachmentCount{static_cast<std::uint32_t>(attachment_blends.size())},
            .pAttachments{attachment_blends.empty() ? nullptr : attachment_blends.data()},
        };

        VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .dynamicStateCount{static_cast<std::uint32_t>(sizeof(dynamic_states) / sizeof(VkDynamicState))},
            .pDynamicStates{sizeof(dynamic_states) == 0 ? nullptr : dynamic_states},
        };

        VkPipelineViewportStateCreateInfo viewport = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .viewportCount{0},
            .pViewports{nullptr},
            .scissorCount{0},
            .pScissors{nullptr},
        };

        VkGraphicsPipelineCreateInfo pipeline_ci = {
            .sType{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO},
            .pNext{&dynamic_render},
            .flags{0},
            .stageCount{2},
            .pStages{stages_ci},
            .pVertexInputState{&vertex_input_state},
            .pInputAssemblyState{&input_assembly_state},
            .pTessellationState{nullptr},
            .pViewportState{&viewport},
            .pRasterizationState{&raster_state},
            .pMultisampleState{nullptr},
            .pDepthStencilState{&depth_stencil_state},
            .pColorBlendState{&color_blend_state},
            .pDynamicState{&dynamic_state},
            .layout{pipeline_layout},
            .renderPass{VK_NULL_HANDLE},
            .subpass{0},
            .basePipelineHandle{VK_NULL_HANDLE},
            .basePipelineIndex{0},
        };

        VkPipeline pipeline;
        auto pipeline_result = _dispatch.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &pipeline);
        if (pipeline_result != VK_SUCCESS)
        {
            return graphics_pipeline_resource_handle();
        }

        graphics_pipeline gfx_pipeline = {
            .vertex_module{vertex_module},
            .fragment_module{fragment_module},
            .set_layouts{std::move(set_layouts)},
            .pipeline{pipeline},
            .pipeline_layout{pipeline_layout},
            .name{ci.name},
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
                _graphics_pipelines->release_resource({.index{handle.id}, .generation{handle.generation}});
            });
        }
    }

    swapchain* render_device::access_swapchain(swapchain_resource_handle handle) noexcept
    {
        return reinterpret_cast<swapchain*>(_swapchains->access({.index{handle.id}, .generation{handle.generation}}));
    }

    const swapchain* render_device::access_swapchain(swapchain_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const swapchain*>(
            _swapchains->access({.index{handle.id}, .generation{handle.generation}}));
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

        if (glfw::window* win = dynamic_cast<glfw::window*>(info.win))
        {
            auto window = win->raw();
            auto result = glfwCreateWindowSurface(_instance.instance, window, nullptr, &surface);
            if (result != VK_SUCCESS)
            {
                return swapchain_resource_handle();
            }
        }

        vkb::SwapchainBuilder swap_bldr =
            vkb::SwapchainBuilder(_physical, _device, surface)
                .add_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .set_required_min_image_count(info.desired_frame_count)
                .set_desired_extent(info.win->width(), info.win->height())
                .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
                .set_desired_format({.format{VK_FORMAT_B8G8R8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}});

        auto result = swap_bldr.build();
        if (!result)
        {
            return swapchain_resource_handle();
        }

        swapchain sc = {
            .sc{result.value()},
            .surface{surface},
        };

        auto images = sc.sc.get_images().value();
        auto views = sc.sc.get_image_views().value();
        sc.image_handles.reserve(views.size());

        for (std::uint32_t i = 0; i < sc.sc.image_count; ++i)
        {
            image sc_image = {
                .allocation{nullptr},
                .image{images[i]},
                .view{views[i]},
                .name{std::format("swapchain_image_{}", i)},
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
            for (image_resource_handle handle : sc->image_handles)
            {
                release_image(handle);
            }

            _delete_queue->add_to_queue(_current_frame, [this, sc, handle]() {
                vkb::destroy_swapchain(sc->sc);
                vkb::destroy_surface(_instance, sc->surface);
                std::destroy_at(sc);
                _swapchains->release_resource({.index{handle.id}, .generation{handle.generation}});
            });
        }
    }

    void render_device::recreate_swapchain(swapchain_resource_handle handle, std::uint32_t width, std::uint32_t height)
    {
    }

    VkResult render_device::acquire_next_image(swapchain_resource_handle handle, VkSemaphore sem, VkFence fen)
    {
        auto swap = access_swapchain(handle);
        return _dispatch.acquireNextImageKHR(swap->sc, UINT_MAX, sem, fen, &swap->image_index);
    }

    per_frame_data& render_device::get_current_frame() noexcept
    {
        return _per_frame[_current_frame % _frames_in_flight];
    }

    const per_frame_data& render_device::get_current_frame() const noexcept
    {
        return _per_frame[_current_frame % _frames_in_flight];
    }

    command_buffer_allocator render_device::acquire_frame_local_command_buffer_allocator()
    {
        return _recycled_cmd_buf_pool.acquire(_dispatch);
    }

    void render_device::release_frame_local_command_buffer_allocator(command_buffer_allocator&& allocator)
    {
        _recycled_cmd_buf_pool.release(std::move(allocator), _current_frame);
    }

    render_context::render_context(core::allocator* alloc)
        : graphics::render_context(alloc), _instance{build_instance()}
    {
        vkb::PhysicalDeviceSelector selector = vkb::PhysicalDeviceSelector(_instance)
                                                   .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                                                   .defer_surface_initialization()
                                                   .require_present()
                                                   .set_minimum_version(1, 3)
                                                   .set_required_features({
                                                       .robustBufferAccess{VK_TRUE},
                                                       .independentBlend{VK_TRUE},
                                                       .logicOp{VK_TRUE},
                                                       .depthClamp{VK_TRUE},
                                                       .depthBiasClamp{VK_TRUE},
                                                       .fillModeNonSolid{VK_TRUE},
                                                       .depthBounds{VK_TRUE},
                                                       .alphaToOne{VK_TRUE},
                                                       .shaderUniformBufferArrayDynamicIndexing{VK_TRUE},
                                                       .shaderSampledImageArrayDynamicIndexing{VK_TRUE},
                                                       .shaderStorageBufferArrayDynamicIndexing{VK_TRUE},
                                                       .shaderStorageImageArrayDynamicIndexing{VK_TRUE},
                                                       .shaderInt64{VK_TRUE},
                                                   })
                                                   .set_required_features_12({
                                                       .drawIndirectCount{VK_TRUE},
                                                       .shaderUniformBufferArrayNonUniformIndexing{VK_TRUE},
                                                       .shaderSampledImageArrayNonUniformIndexing{VK_TRUE},
                                                       .shaderStorageBufferArrayNonUniformIndexing{VK_TRUE},
                                                       .shaderStorageImageArrayNonUniformIndexing{VK_TRUE},
                                                       .shaderUniformTexelBufferArrayNonUniformIndexing{VK_TRUE},
                                                       .shaderStorageTexelBufferArrayNonUniformIndexing{VK_TRUE},
                                                       .descriptorBindingSampledImageUpdateAfterBind{VK_TRUE},
                                                       .descriptorBindingStorageImageUpdateAfterBind{VK_TRUE},
                                                       .descriptorBindingPartiallyBound{VK_TRUE},
                                                       .descriptorBindingVariableDescriptorCount{VK_TRUE},
                                                       .imagelessFramebuffer{VK_TRUE},
                                                       .separateDepthStencilLayouts{VK_TRUE},
                                                       .bufferDeviceAddress{VK_TRUE},
                                                   })
                                                   .set_required_features_13({
                                                       .dynamicRendering{VK_TRUE},
                                                   });

        auto selection = selector.select_devices();
        if (selection)
        {
            for (const vkb::PhysicalDevice& phys_dev : selection.value())
            {
                _devices.emplace_back(std::make_unique<render_device>(alloc, _instance, phys_dev));
            }
        }

        if (_devices.empty())
        {
            logger->critical("Failed to find suitable device for rendering.");
        }
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

    std::uint32_t render_context::device_count() const noexcept
    {
        return static_cast<std::uint32_t>(_devices.size());
    }

    graphics::render_device& render_context::get_device(std::uint32_t idx)
    {
        assert(idx < _devices.size() && "Device query index out of bounds.");
        return *(_devices[idx]);
    }

    resource_deletion_queue::resource_deletion_queue(std::size_t frames_in_flight) : _frames_in_flight{frames_in_flight}
    {
    }

    void resource_deletion_queue::add_to_queue(std::size_t current_frame, std::function<void()> deleter)
    {
        _queue.push_back(delete_info{
            .frame{current_frame},
            .deleter{deleter},
        });
    }

    void resource_deletion_queue::flush_frame(std::size_t current_frame)
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

    command_list::command_list(VkCommandBuffer buffer, vkb::DispatchTable* dispatch)
        : _cmds{buffer}, _dispatch{dispatch}
    {
    }

    command_list::operator VkCommandBuffer() const noexcept
    {
        return _cmds;
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
                .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO},
                .pNext{nullptr},
                .commandPool{pool},
                .level{VK_COMMAND_BUFFER_LEVEL_PRIMARY},
                .commandBufferCount{1},
            };
            VkResult result = dispatch->allocateCommandBuffers(&alloc_info, &buf);
            assert(result == VK_SUCCESS);
            cached_commands.push_back(buf);
        }
        VkCommandBuffer cmds = cached_commands[command_buffer_index++];
        return command_list(cmds, dispatch);
    }

    void command_buffer_allocator::release()
    {
        if (!cached_commands.empty())
        {
            dispatch->freeCommandBuffers(pool, static_cast<std::uint32_t>(cached_commands.size()),
                                         cached_commands.data());
        }
        dispatch->destroyCommandPool(pool, nullptr);
    }

    command_buffer_allocator command_buffer_recycler::acquire(vkb::DispatchTable& dispatch)
    {
        if (global_pool.empty())
        {
            VkCommandPoolCreateInfo ci = {
                .sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .queueFamilyIndex{queue.queue_family_index},
            };

            VkCommandPool pool;
            auto result = dispatch.createCommandPool(&ci, nullptr, &pool);

            assert(result == VK_SUCCESS);

            command_buffer_allocator allocator{
                .queue{queue},
                .pool{pool},
                .dispatch{&dispatch},
            };
            global_pool.push_back(allocator);
        }

        auto last = global_pool.back();
        global_pool.pop_back();
        return last;
    }

    void command_buffer_recycler::release(command_buffer_allocator&& allocator, std::size_t current_frame)
    {
        recycle_pool.push_back(command_buffer_recycle_payload{
            .allocator{std::move(allocator)},
            .recycled_frame{current_frame},
        });
    }

    void command_buffer_recycler::recycle(std::size_t current_frame, vkb::DispatchTable& dispatch)
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

    void command_buffer_recycler::release_all(vkb::DispatchTable& dispatch)
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
                .sType{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO},
                .pNext{nullptr},
                .flags{VK_FENCE_CREATE_SIGNALED_BIT},
            };
            VkFence fen{VK_NULL_HANDLE};
            auto result = dispatch.createFence(&create, nullptr, &fen);
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
                .sType{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO},
                .pNext{nullptr},
            };

            VkSemaphore sem{VK_NULL_HANDLE};
            auto result = dispatch.createSemaphore(&create, nullptr, &sem);
            assert(result == VK_SUCCESS);
            return sem;
        }

        VkSemaphore sem = global_semaphore_pool.back();
        global_semaphore_pool.pop_back();
        return sem;
    }

    void sync_primitive_recycler::release(VkFence&& fen, std::size_t current_frame)
    {
        fence_recycle_payload payload{
            .fence{std::move(fen)},
            .recycled_frame{current_frame},
        };

        recycle_fence_pool.push_back(std::move(payload));
    }

    void sync_primitive_recycler::release(VkSemaphore&& sem, std::size_t current_frame)
    {
        semaphore_recycle_payload payload{
            .sem{std::move(sem)},
            .recycled_frame{current_frame},
        };

        recycle_semaphore_pool.push_back(std::move(payload));
    }

    void sync_primitive_recycler::recycle(std::size_t current_frame, vkb::DispatchTable& dispatch)
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
            if (recycle_fence_pool.front().recycled_frame + frames_in_flight > current_frame)
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
} // namespace tempest::graphics::vk