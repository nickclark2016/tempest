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
            case resource_format::RGBA8_UINT:
                return VK_FORMAT_R8G8B8A8_UINT;
            case resource_format::RGBA8_UNORM:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case resource_format::RGBA8_SRGB:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case resource_format::BGRA8_SRGB:
                return VK_FORMAT_B8G8R8A8_SRGB;
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
            case resource_format::BGRA8_SRGB:
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
            case resource_format::BGRA8_SRGB:
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

        void name_object(const vkb::DispatchTable& dispatch, std::uint64_t object_handle, VkObjectType type,
                         const char* name)
        {
#ifdef _DEBUG
            VkDebugUtilsObjectNameInfoEXT name_info = {
                .sType{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT},
                .pNext{nullptr},
                .objectType{type},
                .objectHandle{object_handle},
                .pObjectName{name},
            };

            dispatch.setDebugUtilsObjectNameEXT(&name_info);
#endif
        }

        static constexpr std::uint32_t IMAGE_POOL_SIZE = 4096;
        static constexpr std::uint32_t BUFFER_POOL_SIZE = 512;
        static constexpr std::uint32_t GRAPHICS_PIPELINE_POOL_SIZE = 256;
        static constexpr std::uint32_t COMPUTE_PIPELINE_POOL_SIZE = 128;
        static constexpr std::uint32_t SWAPCHAIN_POOL_SIZE = 8;
        static constexpr std::uint32_t SAMPLER_POOL_SIZE = 128;
    } // namespace

    render_device::render_device(core::allocator* alloc, vkb::Instance instance, vkb::PhysicalDevice physical)
        : _alloc{alloc}, _instance{instance}, _physical{physical}
    {
        _images.emplace(_alloc, IMAGE_POOL_SIZE, static_cast<std::uint32_t>(sizeof(image)));
        _buffers.emplace(_alloc, BUFFER_POOL_SIZE, static_cast<std::uint32_t>(sizeof(buffer)));
        _graphics_pipelines.emplace(_alloc, GRAPHICS_PIPELINE_POOL_SIZE,
                                    static_cast<std::uint32_t>(sizeof(graphics_pipeline)));
        _compute_pipelines.emplace(_alloc, COMPUTE_PIPELINE_POOL_SIZE,
                                   static_cast<std::uint32_t>(sizeof(compute_pipeline)));
        _swapchains.emplace(_alloc, SWAPCHAIN_POOL_SIZE, static_cast<std::uint32_t>(sizeof(swapchain)));
        _samplers.emplace(_alloc, SAMPLER_POOL_SIZE, static_cast<std::uint32_t>(sizeof(sampler)));
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

        _recycled_cmd_buf_pool = command_buffer_recycler{
            .frames_in_flight{_frames_in_flight},
            .queue{_queue},
        };

        _sync_prim_recycler = sync_primitive_recycler{
            .frames_in_flight{_frames_in_flight},
        };

        _executor.emplace(_dispatch, *this);

        _staging_buffer = create_buffer({
            .per_frame{true},
            .loc{graphics::memory_location::HOST},
            .size{64 * 1024 * 1024 * frames_in_flight()},
            .transfer_source{true},
            .name{"Staging Buffer"},
        });
    }

    render_device::~render_device()
    {
        release_buffer(_staging_buffer);

        _dispatch.deviceWaitIdle();

        _delete_queue->flush_all();
        _recycled_cmd_buf_pool.release_all(_dispatch);
        _sync_prim_recycler.release_all(_dispatch);
        _executor = std::nullopt;

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
            .per_frame_resource{ci.per_frame},
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

        name_object(_dispatch, std::bit_cast<std::uint64_t>(buf.buffer), VK_OBJECT_TYPE_BUFFER, ci.name.c_str());

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

    std::span<std::byte> render_device::map_buffer(buffer_resource_handle handle)
    {
        auto vk_buf = access_buffer(handle);
        void* result;
        auto res = vmaMapMemory(_vk_alloc, vk_buf->allocation, &result);
        assert(res == VK_SUCCESS);
        return std::span(reinterpret_cast<std::byte*>(result), vk_buf->info.size);
    }

    std::span<std::byte> render_device::map_buffer_frame(buffer_resource_handle handle, std::uint64_t frame_offset)
    {
        auto vk_buf = access_buffer(handle);
        void* result;
        auto res = vmaMapMemory(_vk_alloc, vk_buf->allocation, &result);
        assert(res == VK_SUCCESS);

        std::uint64_t frame = (_current_frame + frame_offset) % _frames_in_flight;

        if (vk_buf->per_frame_resource)
        {
            std::uint64_t size_per_frame = vk_buf->alloc_info.size / _frames_in_flight;
            return std::span(reinterpret_cast<std::byte*>(result) + size_per_frame * frame, size_per_frame);
        }

        logger->warn("Performance Note: Buffer is not a per-frame resource. Use map_buffer instead.");

        return std::span(reinterpret_cast<std::byte*>(result), vk_buf->info.size);
    }

    std::size_t render_device::get_buffer_frame_offset(buffer_resource_handle handle, std::uint64_t frame_offset)
    {
        auto vk_buf = access_buffer(handle);

        std::uint64_t frame = (_current_frame + frame_offset) % _frames_in_flight;

        if (vk_buf->per_frame_resource)
        {
            std::uint64_t size_per_frame = vk_buf->alloc_info.size / _frames_in_flight;
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

        name_object(_dispatch, std::bit_cast<std::uint64_t>(img), VK_OBJECT_TYPE_IMAGE, ci.name.c_str());

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

        name_object(_dispatch, std::bit_cast<std::uint64_t>(view), VK_OBJECT_TYPE_IMAGE_VIEW, ci.name.c_str());

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

    sampler* render_device::access_sampler(sampler_resource_handle handle) noexcept
    {
        return reinterpret_cast<sampler*>(_samplers->access({
            .index{handle.id},
            .generation{handle.generation},
        }));
    }

    const sampler* render_device::access_sampler(sampler_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const sampler*>(_samplers->access({
            .index{handle.id},
            .generation{handle.generation},
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
            .sType{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .magFilter{to_vulkan(ci.mag)},
            .minFilter{to_vulkan(ci.min)},
            .mipmapMode{to_vulkan(ci.mipmap)},
            .addressModeU{VK_SAMPLER_ADDRESS_MODE_REPEAT},
            .addressModeV{VK_SAMPLER_ADDRESS_MODE_REPEAT},
            .addressModeW{VK_SAMPLER_ADDRESS_MODE_REPEAT},
            .mipLodBias{ci.mip_lod_bias},
            .anisotropyEnable{VK_FALSE},
            .maxAnisotropy{0.0f},
            .compareEnable{VK_FALSE},
            .compareOp{VK_COMPARE_OP_NEVER},
            .minLod{ci.min_lod},
            .maxLod{ci.max_lod},
            .borderColor{VK_BORDER_COLOR_MAX_ENUM},
            .unnormalizedCoordinates{VK_FALSE},
        };

        VkSampler s;
        auto result = _dispatch.createSampler(&create_info, nullptr, &s);
        if (result != VK_SUCCESS)
        {
            _samplers->release_resource({
                .index{handle.id},
                .generation{handle.generation},
            });

            return sampler_resource_handle();
        }

        sampler smp{
            .vk_sampler{s},
            .info{create_info},
            .name{ci.name},
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
                _samplers->release_resource({.index{handle.id}, .generation{handle.generation}});
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
                    .stageFlags{VK_SHADER_STAGE_ALL_GRAPHICS},
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

        name_object(_dispatch, std::bit_cast<std::uint64_t>(vertex_module), VK_OBJECT_TYPE_SHADER_MODULE,
                    ci.vertex_shader.name.c_str());
        name_object(_dispatch, std::bit_cast<std::uint64_t>(fragment_module), VK_OBJECT_TYPE_SHADER_MODULE,
                    ci.fragment_shader.name.c_str());

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
            .frontFace{VK_FRONT_FACE_CLOCKWISE},
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

        VkPipelineMultisampleStateCreateInfo multisample_state = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .rasterizationSamples{VK_SAMPLE_COUNT_1_BIT},
            .sampleShadingEnable{VK_FALSE},
            .alphaToCoverageEnable{VK_FALSE},
            .alphaToOneEnable{VK_FALSE},
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
            .pMultisampleState{&multisample_state},
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

        name_object(_dispatch, std::bit_cast<std::uint64_t>(pipeline), VK_OBJECT_TYPE_PIPELINE, ci.name.c_str());

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

    compute_pipeline* render_device::access_compute_pipeline(compute_pipeline_resource_handle handle) noexcept
    {
        return reinterpret_cast<compute_pipeline*>(
            _compute_pipelines->access({.index{handle.id}, .generation{handle.generation}}));
    }

    const compute_pipeline* render_device::access_compute_pipeline(
        compute_pipeline_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const compute_pipeline*>(
            _compute_pipelines->access({.index{handle.id}, .generation{handle.generation}}));
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
                    .stageFlags{VK_SHADER_STAGE_ALL_GRAPHICS},
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
                return compute_pipeline_resource_handle();
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
            return compute_pipeline_resource_handle();
        }

        name_object(_dispatch, std::bit_cast<std::uint64_t>(pipeline_layout), VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                    ci.name.c_str());

        VkShaderModuleCreateInfo compute_module_ci = {
            .sType{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .codeSize{ci.compute_shader.bytes.size()},
            .pCode{reinterpret_cast<std::uint32_t*>(ci.compute_shader.bytes.data())},
        };

        VkShaderModule compute_shader_module{VK_NULL_HANDLE};

        auto module_result = _dispatch.createShaderModule(&compute_module_ci, nullptr, &compute_shader_module);
        if (module_result != VK_SUCCESS)
        {
            logger->error("Failed to create VkShaderModule");
            return compute_pipeline_resource_handle();
        }

        name_object(_dispatch, std::bit_cast<std::uint64_t>(compute_shader_module), VK_OBJECT_TYPE_SHADER_MODULE,
                    ci.compute_shader.name.c_str());

        VkPipelineShaderStageCreateInfo compute_stage{
            .sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .stage{VK_SHADER_STAGE_COMPUTE_BIT},
            .module{compute_shader_module},
            .pName{ci.compute_shader.entrypoint.data()},
            .pSpecializationInfo{nullptr},
        };

        VkComputePipelineCreateInfo compute_ci = {
            .sType{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .layout{pipeline_layout},
            .basePipelineHandle{VK_NULL_HANDLE},
            .basePipelineIndex{0},
        };

        compute_pipeline pipeline = {
            .compute_module{compute_shader_module},
            .set_layouts{set_layouts},
            .pipeline_layout{pipeline_layout},
            .name{ci.name},
        };

        auto compute_result =
            _dispatch.createComputePipelines(VK_NULL_HANDLE, 1, &compute_ci, nullptr, &pipeline.pipeline);

        if (compute_result != VK_SUCCESS)
        {
            logger->error("Failed to create compute VkPipeline.");
            return compute_pipeline_resource_handle();
        }

        name_object(_dispatch, std::bit_cast<std::uint64_t>(pipeline.pipeline), VK_OBJECT_TYPE_PIPELINE,
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
                _compute_pipelines->release_resource({.index{handle.id}, .generation{handle.generation}});
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
        glfw::window* win = dynamic_cast<glfw::window*>(info.win);

        std::uint32_t width = info.win->width();
        std::uint32_t height = info.win->height();

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
            width = static_cast<std::uint32_t>(w);
            height = static_cast<std::uint32_t>(h);
        }

        vkb::SwapchainBuilder swap_bldr =
            vkb::SwapchainBuilder(_physical, _device, surface)
                .add_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .set_required_min_image_count(info.desired_frame_count)
                .set_desired_extent(width, height)
                .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
                .set_desired_format({.format{VK_FORMAT_B8G8R8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}});

        auto result = swap_bldr.build();
        if (!result)
        {
            return swapchain_resource_handle();
        }

        swapchain sc = {
            .win{win},
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
                .img_info{
                    .extent{
                        .width{sc.sc.extent.width},
                        .height{sc.sc.extent.height},
                        .depth{1},
                    },
                },
                .view_info{
                    .image{images[i]},
                    .subresourceRange{
                        .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                    },
                },
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
            width = static_cast<std::uint32_t>(w);
            height = static_cast<std::uint32_t>(h);
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
                .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
                .set_desired_format({.format{VK_FORMAT_B8G8R8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}})
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

        _delete_queue->add_to_queue(_current_frame, [this, old_swap]() { vkb::destroy_swapchain(old_swap); });

        sc->sc = *swap_result;

        auto images = sc->sc.get_images().value();
        auto views = sc->sc.get_image_views().value();
        sc->image_handles.reserve(views.size());

        if (views.size() < sc->image_handles.size())
        {
            for (std::size_t i = views.size(); i < sc->image_handles.size(); ++i)
            {
                release_image(sc->image_handles[i]);
            }
            sc->image_handles.erase(sc->image_handles.begin() + views.size(), sc->image_handles.end());
        }

        for (std::uint32_t i = 0; i < sc->sc.image_count; ++i)
        {
            image sc_image = {
                .allocation{nullptr},
                .image{images[i]},
                .view{views[i]},
                .img_info{
                    .extent{
                        .width{sc->sc.extent.width},
                        .height{sc->sc.extent.height},
                        .depth{1},
                    },
                },
                .view_info{
                    .image{images[i]},
                    .subresourceRange{
                        .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                    },
                },
                .name{std::format("swapchain_image_{}", i)},
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

    render_context::render_context(core::allocator* alloc)
        : graphics::render_context(alloc), _instance{build_instance()}
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

    std::uint32_t render_context::device_count() const noexcept
    {
        return static_cast<std::uint32_t>(_devices.size());
    }

    graphics::render_device& render_context::create_device(std::uint32_t idx)
    {
        auto devices = enumerate_suitable_devices();
        assert(idx < devices.size() && "Device query index out of bounds.");

        if (idx >= _devices.size())
        {
            _devices.resize(idx + 1);
        }

        vkb::PhysicalDeviceSelector selector = vkb::PhysicalDeviceSelector(_instance)
                                                   .prefer_gpu_device_type(vkb::PreferredDeviceType::integrated)
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
                                                       .samplerAnisotropy{VK_TRUE},
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
        _devices[idx] = std::make_unique<render_device>(_alloc, _instance, (*selection)[idx]);

        return *(_devices[idx]);
    }

    std::vector<physical_device_context> render_context::enumerate_suitable_devices()
    {
        vkb::PhysicalDeviceSelector selector = vkb::PhysicalDeviceSelector(_instance)
                                                   .prefer_gpu_device_type(vkb::PreferredDeviceType::integrated)
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
                                                       .samplerAnisotropy{VK_TRUE},
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
        std::vector<physical_device_context> devices;

        if (selection)
        {
            for (std::size_t i = 0; i < selection->size(); ++i)
            {
                devices.push_back(physical_device_context{
                    .id{static_cast<std::uint32_t>(i)},
                    .name{(*selection)[i].name},
                });
            }
        }

        return devices;
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

    command_list::command_list(VkCommandBuffer buffer, vkb::DispatchTable* dispatch, render_device* device)
        : _cmds{buffer}, _dispatch{dispatch}, _device{device}
    {
    }

    command_list::operator VkCommandBuffer() const noexcept
    {
        return _cmds;
    }

    command_list& command_list::set_viewport(float x, float y, float width, float height, float min_depth,
                                             float max_depth, std::uint32_t viewport_id)
    {
        VkViewport vp = {
            .x{x},
            .y{height - y},
            .width{width},
            .height{-height},
            .minDepth{min_depth},
            .maxDepth{max_depth},
        };

        _dispatch->cmdSetViewport(_cmds, viewport_id, 1, &vp);

        return *this;
    }

    command_list& command_list::set_scissor_region(std::int32_t x, std::int32_t y, std::uint32_t width,
                                                   std::uint32_t height)
    {
        VkRect2D scissor = {
            .offset{
                .x{x},
                .y{y},
            },
            .extent{
                .width{width},
                .height{height},
            },
        };

        _dispatch->cmdSetScissor(_cmds, 0, 1, &scissor);

        return *this;
    }

    command_list& command_list::draw(std::uint32_t vertex_count, std::uint32_t instance_count,
                                     std::uint32_t first_vertex, std::uint32_t first_index)
    {
        _dispatch->cmdDraw(_cmds, vertex_count, instance_count, first_vertex, first_index);

        return *this;
    }

    command_list& command_list::use_pipeline(graphics_pipeline_resource_handle pipeline)
    {
        auto vk_pipeline = _device->access_graphics_pipeline(pipeline);
        _dispatch->cmdBindPipeline(_cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline->pipeline);

        return *this;
    }

    command_list& command_list::blit(image_resource_handle src, image_resource_handle dst)
    {
        auto src_img = _device->access_image(src);
        auto dst_img = _device->access_image(dst);

        VkImageBlit region = {
            .srcSubresource{
                .aspectMask{src_img->view_info.subresourceRange.aspectMask},
                .mipLevel{0},
                .baseArrayLayer{0},
                .layerCount{1},
            },
            .srcOffsets{
                {
                    .x{0},
                    .y{0},
                    .z{0},
                },
                {
                    .x{static_cast<int32_t>(src_img->img_info.extent.width)},
                    .y{static_cast<int32_t>(src_img->img_info.extent.height)},
                    .z{1},
                },
            },
            .dstSubresource{
                .aspectMask{src_img->view_info.subresourceRange.aspectMask},
                .mipLevel{0},
                .baseArrayLayer{0},
                .layerCount{1},
            },
            .dstOffsets{
                {
                    .x{0},
                    .y{0},
                    .z{0},
                },
                {
                    .x{static_cast<int32_t>(dst_img->img_info.extent.width)},
                    .y{static_cast<int32_t>(dst_img->img_info.extent.height)},
                    .z{1},
                },
            },
        };

        _dispatch->cmdBlitImage(_cmds, src_img->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_img->image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);

        return *this;
    }

    command_list& command_list::copy(buffer_resource_handle src, buffer_resource_handle dst, std::size_t src_offset,
                                     std::size_t dst_offset, std::size_t byte_count)
    {
        auto src_buf = _device->access_buffer(src);
        auto dst_buf = _device->access_buffer(dst);

        assert(src_buf->info.size > src_offset &&
               "Buffer copy source size must be larger than the source copy offset.");
        assert(dst_buf->info.size > dst_offset &&
               "Buffer copy source size must be larger than the source copy offset.");

        if (byte_count == std::numeric_limits<std::size_t>::max())
        {
            std::size_t src_bytes_available = src_buf->info.size - src_offset;
            std::size_t dst_bytes_available = dst_buf->info.size - dst_offset;
            std::size_t bytes_available = std::min(src_bytes_available, dst_bytes_available);

            byte_count = bytes_available;
        }

        assert(src_offset + byte_count <= src_buf->info.size &&
               "src_offset + byte_count must be less than the size of the source buffer.");
        assert(dst_offset + byte_count <= dst_buf->info.size &&
               "src_offset + byte_count must be less than the size of the source buffer.");

        VkBufferCopy copy = {
            .srcOffset{src_offset},
            .dstOffset{dst_offset},
            .size{byte_count},
        };

        _dispatch->cmdCopyBuffer(_cmds, src_buf->buffer, dst_buf->buffer, 1, &copy);

        return *this;
    }

    command_list& command_list::copy(buffer_resource_handle src, image_resource_handle dst, std::size_t buffer_offset,
                                     std::uint32_t region_width, std::uint32_t region_height, std::uint32_t mip_level,
                                     std::int32_t offset_x, std::int32_t offset_y)
    {
        VkBufferImageCopy copy = {
            .bufferOffset{buffer_offset},
            .bufferRowLength{0},
            .bufferImageHeight{0},
            .imageSubresource{
                .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                .mipLevel{mip_level},
                .baseArrayLayer{0},
                .layerCount{1},
            },
            .imageOffset{
                .x{offset_x},
                .y{offset_y},
                .z{0},
            },
            .imageExtent{
                .width{region_width},
                .height{region_height},
                .depth{1},
            },
        };

        _dispatch->cmdCopyBufferToImage(_cmds, _device->access_buffer(src)->buffer, _device->access_image(dst)->image,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        return *this;
    }

    command_list& command_list::transition_image(image_resource_handle img, image_resource_usage old_usage,
                                                 image_resource_usage new_usage)
    {
        auto vk_img = _device->access_image(img);

        VkImageMemoryBarrier img_barrier = {
            .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
            .pNext{nullptr},
            .srcAccessMask{VK_ACCESS_NONE},
            .dstAccessMask{VK_ACCESS_NONE},
            .oldLayout{compute_layout(old_usage)},
            .newLayout{compute_layout(new_usage)},
            .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
            .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
            .image{vk_img->image},
            .subresourceRange{
                .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                .baseMipLevel{0},
                .levelCount{vk_img->img_info.mipLevels},
                .baseArrayLayer{0},
                .layerCount{vk_img->img_info.arrayLayers},
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
        else
        {
            logger->warn("Unexpected transition.");
        }

        _dispatch->cmdPipelineBarrier(_cmds, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);

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
        return command_list(cmds, dispatch, device);
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

    command_buffer_allocator command_buffer_recycler::acquire(vkb::DispatchTable& dispatch, render_device* device)
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
                .device{device},
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
                .flags{},
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
            .sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO},
            .pNext{nullptr},
            .flags{VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT},
            .queueFamilyIndex{device.get_queue().queue_family_index},
        };

        auto res = _dispatch->createCommandPool(&create_info, nullptr, &_pool);
        assert(res == VK_SUCCESS);

        VkCommandBufferAllocateInfo alloc_ci = {
            .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO},
            .pNext{nullptr},
            .commandPool{_pool},
            .level{VK_COMMAND_BUFFER_LEVEL_PRIMARY},
            .commandBufferCount{1},
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
                .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO},
                .pNext{nullptr},
                .flags{},
                .pInheritanceInfo{nullptr},
            };
            VkCommandBuffer buf = _cmds.value();
            auto res = _dispatch->beginCommandBuffer(buf, &begin);
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
            .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO},
            .pNext{nullptr},
            .waitSemaphoreCount{0},
            .pWaitSemaphores{nullptr},
            .pWaitDstStageMask{nullptr},
            .commandBufferCount{1},
            .pCommandBuffers{&cmds},
            .signalSemaphoreCount{0},
            .pSignalSemaphores{0},
        };

        VkFence fence = _device->acquire_fence();
        _dispatch->queueSubmit(_device->get_queue().queue, 1, &submit, fence);
        _dispatch->waitForFences(1, &fence, VK_TRUE, UINT64_MAX);

        _device->release_fence(std::move(fence));

        _is_recording = false;
    }
} // namespace tempest::graphics::vk