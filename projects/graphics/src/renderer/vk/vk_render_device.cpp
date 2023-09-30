#include "vk_render_device.hpp"

#include <tempest/logger.hpp>

#include <algorithm>
#include <cassert>

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

        static constexpr std::uint32_t IMAGE_POOL_SIZE = 4096;
        static constexpr std::uint32_t BUFFER_POOL_SIZE = 512;
    } // namespace

    render_device::render_device(core::allocator* alloc, vkb::Instance instance, vkb::PhysicalDevice physical,
                                 vkb::Device device)
        : _alloc{alloc}, _instance{instance}, _physical{physical}, _device{device}, _dispatch{_device.make_table()}
    {
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

        _images.emplace(_alloc, IMAGE_POOL_SIZE, static_cast<std::uint32_t>(sizeof(image)));
        _buffers.emplace(_alloc, BUFFER_POOL_SIZE, static_cast<std::uint32_t>(sizeof(buffer)));
        _delete_queue.emplace(_frames_in_flight);
    }

    render_device::~render_device()
    {
        _dispatch.deviceWaitIdle();

        _delete_queue->flush_all();

        vmaDestroyAllocator(_vk_alloc);
        vkb::destroy_device(_device);
    }

    void render_device::start_frame() noexcept
    {
    }

    void render_device::end_frame() noexcept
    {
        _delete_queue->flush_frame(_current_frame);
        _current_frame++;
    }

    buffer_resource_handle render_device::create_buffer(const buffer_create_info& ci)
    {
        return buffer_resource_handle();
    }

    image* render_device::access_image(image_resource_handle handle) noexcept
    {
        return reinterpret_cast<image*>(_images->access({.index{handle.id}, .generation{handle.generation}}));
    }

    const image* render_device::access_image(image_resource_handle handle) const noexcept
    {
        return reinterpret_cast<const image*>(_images->access({.index{handle.id}, .generation{handle.generation}}));
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

        VmaAllocationCreateInfo alloc_create_info = {
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
        _delete_queue->add_to_queue(_current_frame, [this, handle]() {
            auto img = access_image(handle);
            if (img)
            {
                if (img->image)
                {
                    vmaDestroyImage(_vk_alloc, img->image, img->allocation);
                }
                _dispatch.destroyImageView(img->view, nullptr);
            }
        });
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
                auto device_result = vkb::DeviceBuilder(phys_dev).build();
                if (device_result)
                {
                    auto device = *device_result;
                    _devices.emplace_back(std::make_unique<render_device>(alloc, _instance, phys_dev, device));
                }
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
} // namespace tempest::graphics::vk