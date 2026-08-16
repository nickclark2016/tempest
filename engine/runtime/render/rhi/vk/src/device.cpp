#include <tempest/exception.hpp>
#if defined(TEMPEST_PLATFORM_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#elif defined(TEMPEST_PLATFORM_LINUX)
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include <tempest/vk/device.hpp>

#include <tempest/vk/execution_port.hpp>
#include <tempest/vk/render_surface.hpp>

#include <vulkan/vulkan.hpp>

namespace tempest::rhi::vk
{
    namespace
    {
        auto as_tempest_color_space(VkColorSpaceKHR color_space) -> optional<surface_color_space>
        {
            switch (color_space)
            {
            case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
                return surface_color_space::srgb_nonlinear;
            case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
                return surface_color_space::extended_srgb_linear;
            case VK_COLOR_SPACE_HDR10_ST2084_EXT:
                return surface_color_space::hdr10_st2084;
            default:
                return nullopt;
            }
        }

        auto as_vulkan(surface_color_space color_space) -> VkColorSpaceKHR
        {
            switch (color_space)
            {
            case surface_color_space::srgb_nonlinear:
                return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            case surface_color_space::extended_srgb_linear:
                return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
            case surface_color_space::hdr10_st2084:
                return VK_COLOR_SPACE_HDR10_ST2084_EXT;
            default:
                tempest::unreachable();
            }
        }

        auto as_tempest_render_surface_format(VkFormat format) -> optional<render_surface_format>
        {
            switch (format)
            {
            case VK_FORMAT_B8G8R8A8_UNORM:
                return render_surface_format::bgra8_unorm;
            case VK_FORMAT_R8G8B8A8_UNORM:
                return render_surface_format::rgba8_unorm;
            case VK_FORMAT_B8G8R8A8_SRGB:
                return render_surface_format::bgra8_srgb;
            case VK_FORMAT_R8G8B8A8_SRGB:
                return render_surface_format::rgba8_srgb;
            default:
                return nullopt;
            }
        }

        auto as_vulkan(render_surface_format format) -> VkFormat
        {
            switch (format)
            {
            case render_surface_format::bgra8_unorm:
                return VK_FORMAT_B8G8R8A8_UNORM;
            case render_surface_format::rgba8_unorm:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case render_surface_format::bgra8_srgb:
                return VK_FORMAT_B8G8R8A8_SRGB;
            case render_surface_format::rgba8_srgb:
                return VK_FORMAT_R8G8B8A8_SRGB;
            default:
                tempest::unreachable();
            }
        }

        auto as_tempest_present_mode(VkPresentModeKHR mode) -> optional<present_mode>
        {
            switch (mode)
            {
            case VK_PRESENT_MODE_FIFO_KHR:
                return present_mode::vsync;
            case VK_PRESENT_MODE_IMMEDIATE_KHR:
                return present_mode::immediate;
            case VK_PRESENT_MODE_MAILBOX_KHR:
                return present_mode::mailbox;
            default:
                return nullopt;
            }
        }

        auto as_vulkan(present_mode mode) -> VkPresentModeKHR
        {
            switch (mode)
            {
            case present_mode::vsync:
                return VK_PRESENT_MODE_FIFO_KHR;
            case present_mode::immediate:
                return VK_PRESENT_MODE_IMMEDIATE_KHR;
            case present_mode::mailbox:
                return VK_PRESENT_MODE_MAILBOX_KHR;
            default:
                tempest::unreachable();
            }
        }

        auto as_vulkan(data_format format) -> VkFormat
        {
            switch (format)
            {
            case data_format::r8_unorm:
                return VK_FORMAT_R8_UNORM;
            case data_format::r16_float:
                return VK_FORMAT_R16_SFLOAT;
            case data_format::r32_float:
                return VK_FORMAT_R32_SFLOAT;
            case data_format::rg8_unorm:
                return VK_FORMAT_R8G8_UNORM;
            case data_format::rg16_float:
                return VK_FORMAT_R16G16_SFLOAT;
            case data_format::rg32_float:
                return VK_FORMAT_R32G32_SFLOAT;
            case data_format::rgba8_unorm:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case data_format::rgba8_srgb:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case data_format::rgba16_float:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case data_format::rgba32_float:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case data_format::depth16_unorm:
                return VK_FORMAT_D16_UNORM;
            case data_format::depth24_unorm_stencil8:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case data_format::depth32_float:
                return VK_FORMAT_D32_SFLOAT;
            case data_format::depth32_float_stencil8:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            case data_format::unknown:
            default:
                return VK_FORMAT_UNDEFINED;
            }
        }

        auto infer_aspect_flags(VkFormat format) -> VkImageAspectFlags
        {
            switch (format)
            {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case VK_FORMAT_S8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
            }
        }

        auto as_vulkan(filter_mode mode) -> VkFilter
        {
            switch (mode)
            {
            case filter_mode::nearest:
                return VK_FILTER_NEAREST;
            case filter_mode::linear:
                return VK_FILTER_LINEAR;
            default:
                tempest::unreachable();
            }
        }

        auto as_vulkan(mipmap_mode mode) -> VkSamplerMipmapMode
        {
            switch (mode)
            {
            case mipmap_mode::nearest:
                return VK_SAMPLER_MIPMAP_MODE_NEAREST;
            case mipmap_mode::linear:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
            default:
                tempest::unreachable();
            }
        }

        auto as_vulkan(address_mode mode) -> VkSamplerAddressMode
        {
            switch (mode)
            {
            case address_mode::repeat:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case address_mode::mirrored_repeat:
                return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case address_mode::clamp_to_edge:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case address_mode::clamp_to_border:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            default:
                tempest::unreachable();
            }
        }

        auto as_vulkan(compare_op op) -> VkCompareOp
        {
            switch (op)
            {
            case compare_op::never:
                return VK_COMPARE_OP_NEVER;
            case compare_op::less:
                return VK_COMPARE_OP_LESS;
            case compare_op::equal:
                return VK_COMPARE_OP_EQUAL;
            case compare_op::less_or_equal:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
            case compare_op::greater:
                return VK_COMPARE_OP_GREATER;
            case compare_op::not_equal:
                return VK_COMPARE_OP_NOT_EQUAL;
            case compare_op::greater_or_equal:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case compare_op::always:
                return VK_COMPARE_OP_ALWAYS;
            default:
                tempest::unreachable();
            }
        }

        auto as_vulkan(enum_mask<buffer_usage> usage) -> VkBufferUsageFlags
        {
            auto flags = VkBufferUsageFlags{0};
            if (usage & buffer_usage::transfer_src)
            {
                flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            }
            if (usage & buffer_usage::transfer_dst)
            {
                flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            }
            if (usage & buffer_usage::uniform_buffer)
            {
                flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            }
            if (usage & buffer_usage::storage_buffer)
            {
                flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            }
            if (usage & buffer_usage::index_buffer)
            {
                flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            }
            if (usage & buffer_usage::vertex_buffer)
            {
                flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            }
            if (usage & buffer_usage::indirect_buffer)
            {
                flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            }
            if (usage & buffer_usage::device_address)
            {
                flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            }
            return flags;
        }

        auto as_vulkan(enum_mask<texture_usage> usage) -> VkImageUsageFlags
        {
            auto flags = VkImageUsageFlags{0};
            if (usage & texture_usage::transfer_src)
            {
                flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            }
            if (usage & texture_usage::transfer_dst)
            {
                flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            }
            if (usage & texture_usage::sampled)
            {
                flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
            }
            if (usage & texture_usage::storage)
            {
                flags |= VK_IMAGE_USAGE_STORAGE_BIT;
            }
            if (usage & texture_usage::color_attachment)
            {
                flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            }
            if (usage & texture_usage::depth_stencil_attachment)
            {
                flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
            return flags;
        }
    } // namespace

    auto device::create(vkb::Instance instance, vkb::PhysicalDevice physical_device, vkb::Device dev, device_desc desc)
        -> unique_ptr<rhi::device>
    {
        return unique_ptr<rhi::device>{new device{tempest::move(instance), tempest::move(physical_device),
                                                  tempest::move(dev), tempest::move(desc)}};
    }

    device::~device()
    {
        wait_idle();

        _graphics_execution_port.reset();
        _async_compute_execution_port.reset();
        _async_transfer_execution_port.reset();

        for (const auto& raw_surface : _raw_surfaces)
        {
            _instance_dispatch_table.destroySurfaceKHR(raw_surface.handle, nullptr);
        }
        _raw_surfaces.clear();

        for (const auto& view : _texture_views)
        {
            _dispatch_table.destroyImageView(view.handle, nullptr);
        }
        _texture_views.clear();

        for (const auto& samp : _samplers)
        {
            _dispatch_table.destroySampler(samp.handle, nullptr);
        }
        _samplers.clear();

        for (const auto& tex : _textures)
        {
            if (tex.allocation != VK_NULL_HANDLE)
            {
                vmaDestroyImage(_allocator, tex.handle, tex.allocation);
            }
        }
        _textures.clear();

        for (const auto& buf : _buffers)
        {
            if (buf.allocation != VK_NULL_HANDLE)
            {
                vmaDestroyBuffer(_allocator, buf.handle, buf.allocation);
            }
        }
        _buffers.clear();

        for (const auto& sem : _semaphores)
        {
            _dispatch_table.destroySemaphore(sem, nullptr);
        }
        _semaphores.clear();

        for (const auto& event : _events)
        {
            _dispatch_table.destroyEvent(event, nullptr);
        }
        _events.clear();

        if (_default_pipeline_layout != VK_NULL_HANDLE)
        {
            _dispatch_table.destroyPipelineLayout(_default_pipeline_layout, nullptr);
        }
        if (_sampler_set_layout != VK_NULL_HANDLE)
        {
            _dispatch_table.destroyDescriptorSetLayout(_sampler_set_layout, nullptr);
        }
        if (_sampled_image_set_layout != VK_NULL_HANDLE)
        {
            _dispatch_table.destroyDescriptorSetLayout(_sampled_image_set_layout, nullptr);
        }
        if (_storage_image_set_layout != VK_NULL_HANDLE)
        {
            _dispatch_table.destroyDescriptorSetLayout(_storage_image_set_layout, nullptr);
        }

        if (_allocator != VK_NULL_HANDLE)
        {
            vmaDestroyAllocator(_allocator);
            _allocator = VK_NULL_HANDLE;
        }

        vkb::destroy_device(_device);
    }

    auto device::wait_idle() -> void
    {
        _dispatch_table.deviceWaitIdle();
    }

    auto device::wait_for_sync(host_sync_point sync_point) -> void
    {
        auto sem = get_semaphore(sync_point.semaphore);
        if (sem != VK_NULL_HANDLE)
        {
            auto timeline_info = VkSemaphoreWaitInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                .pNext = nullptr,
                .flags = 0,
                .semaphoreCount = 1,
                .pSemaphores = &sem,
                .pValues = &sync_point.value,
            };
            _dispatch_table.waitSemaphores(&timeline_info, UINT64_MAX);
        }
    }

    auto device::is_ray_tracing_supported() const -> bool
    {
        return _desc.features.ray_tracing;
    }

    auto device::is_mesh_shading_supported() const -> bool
    {
        return _desc.features.mesh_shading;
    }

    auto device::is_ray_query_supported() const -> bool
    {
        return _desc.features.ray_query;
    }

    auto device::create_raw_surface(native_wsi_handle native_window_handle)
        -> expected<raw_surface_handle, raw_surface_creation_error>
    {
#ifdef TEMPEST_PLATFORM_WINDOWS
        auto surface_create_info = VkWin32SurfaceCreateInfoKHR{
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .hinstance = static_cast<HINSTANCE>(native_window_handle.display),
            .hwnd = static_cast<HWND>(native_window_handle.window),
        };

        auto* surface = VkSurfaceKHR{VK_NULL_HANDLE};

        // Get the pointer to the vkCreateWin32SurfaceKHR function from the Vulkan dispatch table
        const auto create_ptr =
            reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(_instance_dispatch_table.fp_vkCreateWin32SurfaceKHR);

        [[maybe_unused]] const auto result =
            create_ptr(_instance_dispatch_table.instance, &surface_create_info, nullptr, &surface);
        TEMPEST_ASSERT(result == VK_SUCCESS);

        const auto raw_surface_desc = raw_surface{
            .handle = surface,
            .native_surface_handles = native_window_handle,
        };

        auto handle = _raw_surfaces.insert(tempest::move(raw_surface_desc));

        return raw_surface_handle{
            .handle = handle,
        };
#else
        return unexpected(raw_surface_creation_error::unknown);
#endif
    }

    auto device::get_surface_capabilities(raw_surface_handle surface) -> surface_capabilities
    {
        auto* const vk_surface = get_raw_surface(surface)->handle;
        auto vk_surface_caps = VkSurfaceCapabilitiesKHR{};
        [[maybe_unused]] auto result = _instance_dispatch_table.getPhysicalDeviceSurfaceCapabilitiesKHR(
            _physical_device.physical_device, vk_surface, &vk_surface_caps);
        TEMPEST_ASSERT(result == VK_SUCCESS);

        // Get the supported surface formats
        uint32_t format_count = 0;
        result = _instance_dispatch_table.getPhysicalDeviceSurfaceFormatsKHR(_physical_device.physical_device,
                                                                             vk_surface, &format_count, nullptr);
        TEMPEST_ASSERT(result == VK_SUCCESS);
        auto vk_supported_formats = vector<VkSurfaceFormatKHR>(format_count);
        result = _instance_dispatch_table.getPhysicalDeviceSurfaceFormatsKHR(
            _physical_device.physical_device, vk_surface, &format_count, vk_supported_formats.data());
        TEMPEST_ASSERT(result == VK_SUCCESS);

        auto supported_formats = vector<surface_format>{};
        supported_formats.reserve(vk_supported_formats.size());

        for (const auto& vk_format : vk_supported_formats)
        {
            auto tempest_format = as_tempest_render_surface_format(vk_format.format);
            auto tempest_color_space = as_tempest_color_space(vk_format.colorSpace);

            if (tempest_format && tempest_color_space)
            {
                supported_formats.emplace_back(surface_format{
                    .format = *tempest_format,
                    .color_space = *tempest_color_space,
                });
            }
        }

        auto vk_supported_present_modes = vector<VkPresentModeKHR>{};
        result = _instance_dispatch_table.getPhysicalDeviceSurfacePresentModesKHR(_physical_device.physical_device,
                                                                                  vk_surface, &format_count, nullptr);
        TEMPEST_ASSERT(result == VK_SUCCESS);
        vk_supported_present_modes.resize(format_count);
        result = _instance_dispatch_table.getPhysicalDeviceSurfacePresentModesKHR(
            _physical_device.physical_device, vk_surface, &format_count, vk_supported_present_modes.data());
        TEMPEST_ASSERT(result == VK_SUCCESS);

        auto supported_present_modes = vector<present_mode>{};
        supported_present_modes.reserve(vk_supported_present_modes.size());

        for (const auto& vk_present_mode : vk_supported_present_modes)
        {
            auto tempest_present_mode = as_tempest_present_mode(vk_present_mode);
            if (tempest_present_mode)
            {
                supported_present_modes.emplace_back(*tempest_present_mode);
            }
        }

        return surface_capabilities{
            .min_image_count = vk_surface_caps.minImageCount,
            .max_image_count = vk_surface_caps.maxImageCount,
            .max_image_array_layers = vk_surface_caps.maxImageArrayLayers,
            .supported_formats = tempest::move(supported_formats),
            .supported_present_modes = tempest::move(supported_present_modes),
        };
    }

    auto device::create_render_surface(const render_surface_desc& desc) -> unique_ptr<rhi::render_surface>
    {
        auto* const raw_surface = get_raw_surface(desc.raw_surface)->handle;
        const auto vk_swapchain_ci = VkSwapchainCreateInfoKHR{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = raw_surface,
            .minImageCount = desc.preferred_image_count,
            .imageFormat = as_vulkan(desc.format.format),
            .imageColorSpace = as_vulkan(desc.format.color_space),
            .imageExtent =
                VkExtent2D{
                    .width = desc.width,
                    .height = desc.height,
                },
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = as_vulkan(desc.present_mode),
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE, // TODO: Handle swapchain recreation with old surface hint
        };

        auto vk_swapchain = VkSwapchainKHR{VK_NULL_HANDLE};
        const auto result = _dispatch_table.createSwapchainKHR(&vk_swapchain_ci, nullptr, &vk_swapchain);
        if (result != VK_SUCCESS)
        {
            return nullptr;
        }

        auto image_count = uint32_t{0};
        _dispatch_table.getSwapchainImagesKHR(vk_swapchain, &image_count, nullptr);
        auto swapchain_images = vector<VkImage>{};
        swapchain_images.resize(image_count);
        _dispatch_table.getSwapchainImagesKHR(vk_swapchain, &image_count, swapchain_images.data());

        auto attachments = vector<swapchain_attachment>{};
        attachments.reserve(image_count);

        for (auto vk_image : swapchain_images)
        {
            auto tex = texture{
                .handle = vk_image,
                .allocation = VK_NULL_HANDLE,
                .format = as_vulkan(desc.format.format),
                .width = desc.width,
                .height = desc.height,
                .depth = 1,
                .mip_levels = 1,
                .array_layers = 1,
            };
            auto tex_slot = _textures.insert(tex);
            auto tex_h = texture_handle{.handle = tex_slot};

            auto view_desc = texture_view_desc{
                .base_mip_level = 0,
                .mip_level_count = 1,
                .base_array_layer = 0,
                .array_layer_count = 1,
            };
            auto view_h = create_texture_view(tex_h, view_desc);
            attachments.emplace_back(swapchain_attachment{
                .image = tex_h,
                .view = view_h,
            });
        }

        return make_unique<vk::render_surface>(swapchain_create_desc{
            .rhi_device = this,
            .surface = raw_surface,
            .swapchain = vk_swapchain,
            .attachments = tempest::move(attachments),
            .width = desc.width,
            .height = desc.height,
            .format = desc.format.format,
            .present = desc.present_mode,
        });
    }

    auto device::destroy_render_surface(unique_ptr<rhi::render_surface> surface) -> void
    {
        surface.reset();
    }

    auto device::destroy_raw_surface(raw_surface_handle surface) -> void
    {
        const auto raw_surface_iter = _raw_surfaces.find(surface.handle);
        if (raw_surface_iter != _raw_surfaces.end())
        {
            _instance_dispatch_table.destroySurfaceKHR(raw_surface_iter->handle, nullptr);
            _raw_surfaces.erase(surface.handle);
        }
    }

    auto device::get_graphics_execution_port() -> rhi::execution_port&
    {
        return *_graphics_execution_port;
    }

    auto device::get_async_compute_execution_port() -> rhi::execution_port&
    {
        return *_async_compute_execution_port;
    }

    auto device::get_async_transfer_execution_port() -> rhi::execution_port&
    {
        return *_async_transfer_execution_port;
    }

    auto device::create_buffer(const buffer_desc& desc) -> buffer_handle
    {
        auto buffer_usage_flags = as_vulkan(desc.usage);
        auto vma_usage = VMA_MEMORY_USAGE_AUTO;
        auto vma_flags = VmaAllocationCreateFlags{0};

        switch (desc.memory_usage)
        {
        case memory_usage::device_only:
            vma_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            break;
        case memory_usage::upload:
            vma_usage = VMA_MEMORY_USAGE_AUTO;
            vma_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case memory_usage::readback:
            vma_usage = VMA_MEMORY_USAGE_AUTO;
            vma_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        }

        auto buffer_ci = VkBufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = desc.size,
            .usage = buffer_usage_flags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };

        auto alloc_ci = VmaAllocationCreateInfo{
            .flags = vma_flags,
            .usage = vma_usage,
            .requiredFlags = 0,
            .preferredFlags = 0,
            .memoryTypeBits = 0,
            .pool = VK_NULL_HANDLE,
            .pUserData = nullptr,
            .priority = 0.0F,
        };

        auto vk_buffer = VkBuffer{VK_NULL_HANDLE};
        auto allocation = VmaAllocation{VK_NULL_HANDLE};
        auto alloc_info = VmaAllocationInfo{};

        auto result = vmaCreateBuffer(_allocator, &buffer_ci, &alloc_ci, &vk_buffer, &allocation, &alloc_info);
        if (result != VK_SUCCESS)
        {
            return {};
        }

        auto gpu_address = uint64_t{0};
        if (buffer_usage_flags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            auto bda_info = VkBufferDeviceAddressInfo{
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .pNext = nullptr,
                .buffer = vk_buffer,
            };
            gpu_address = _dispatch_table.getBufferDeviceAddress(&bda_info);
        }

        auto buf = buffer{
            .handle = vk_buffer,
            .allocation = allocation,
            .allocation_info = alloc_info,
        };

        auto slot_handle = _buffers.insert(buf);

        return buffer_handle{
            .handle = slot_handle,
            .gpu_address = gpu_address,
            .cpu_address = alloc_info.pMappedData,
        };
    }

    auto device::create_texture(const texture_desc& desc) -> texture_handle
    {
        auto vk_format = as_vulkan(desc.format);

        auto image_type = VK_IMAGE_TYPE_2D;
        if (desc.depth > 1)
        {
            image_type = VK_IMAGE_TYPE_3D;
        }
        else if (desc.height == 1 && desc.width > 1)
        {
            image_type = VK_IMAGE_TYPE_1D;
        }

        auto usage_flags = as_vulkan(desc.usage);
        auto vma_usage = VMA_MEMORY_USAGE_AUTO;
        auto vma_flags = VmaAllocationCreateFlags{0};

        switch (desc.memory_usage)
        {
        case memory_usage::device_only:
            vma_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            break;
        case memory_usage::upload:
            vma_usage = VMA_MEMORY_USAGE_AUTO;
            vma_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case memory_usage::readback:
            vma_usage = VMA_MEMORY_USAGE_AUTO;
            vma_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        }

        auto image_ci = VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = image_type,
            .format = vk_format,
            .extent =
                {
                    .width = desc.width,
                    .height = desc.height,
                    .depth = desc.depth,
                },
            .mipLevels = desc.mip_levels,
            .arrayLayers = desc.array_layers,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage_flags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        auto alloc_ci = VmaAllocationCreateInfo{
            .flags = vma_flags,
            .usage = vma_usage,
            .requiredFlags = 0,
            .preferredFlags = 0,
            .memoryTypeBits = 0,
            .pool = VK_NULL_HANDLE,
            .pUserData = nullptr,
            .priority = 0.0F,
        };

        auto vk_image = VkImage{VK_NULL_HANDLE};
        auto allocation = VmaAllocation{VK_NULL_HANDLE};
        auto alloc_info = VmaAllocationInfo{};

        auto result = vmaCreateImage(_allocator, &image_ci, &alloc_ci, &vk_image, &allocation, &alloc_info);
        if (result != VK_SUCCESS)
        {
            return {};
        }

        auto tex = texture{
            .handle = vk_image,
            .allocation = allocation,
            .allocation_info = alloc_info,
            .format = vk_format,
            .usage_flags = usage_flags,
            .width = desc.width,
            .height = desc.height,
            .depth = desc.depth,
            .mip_levels = desc.mip_levels,
            .array_layers = desc.array_layers,
        };

        auto slot_handle = _textures.insert(tex);
        return texture_handle{
            .handle = slot_handle,
        };
    }

    auto device::create_texture_view(texture_handle texture, const texture_view_desc& desc) -> texture_view_handle
    {
        auto tex_opt = get_texture(texture);
        if (!tex_opt.has_value())
        {
            return {};
        }

        const auto& tex = *tex_opt;
        auto view_format = desc.override_format.has_value() ? as_vulkan(*desc.override_format) : tex.format;
        auto aspect_flags = infer_aspect_flags(view_format);

        auto view_type = VK_IMAGE_VIEW_TYPE_2D;
        if (tex.depth > 1)
        {
            view_type = VK_IMAGE_VIEW_TYPE_3D;
        }
        else if (tex.array_layers > 1)
        {
            view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
        else if (tex.height == 1 && tex.width > 1)
        {
            view_type = VK_IMAGE_VIEW_TYPE_1D;
        }

        auto mip_count = desc.mip_level_count == ~0U ? (tex.mip_levels - desc.base_mip_level) : desc.mip_level_count;
        auto layer_count =
            desc.array_layer_count == ~0U ? (tex.array_layers - desc.base_array_layer) : desc.array_layer_count;

        auto subresource_range = VkImageSubresourceRange{
            .aspectMask = aspect_flags,
            .baseMipLevel = desc.base_mip_level,
            .levelCount = mip_count,
            .baseArrayLayer = desc.base_array_layer,
            .layerCount = layer_count,
        };

        auto view_ci = VkImageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = tex.handle,
            .viewType = view_type,
            .format = view_format,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange = subresource_range,
        };

        auto vk_view = VkImageView{VK_NULL_HANDLE};
        auto result = _dispatch_table.createImageView(&view_ci, nullptr, &vk_view);
        if (result != VK_SUCCESS)
        {
            return {};
        }

        auto tex_view = texture_view{
            .handle = vk_view,
            .view_type = view_type,
            .format = view_format,
            .subresource_range = subresource_range,
        };

        auto slot_handle = _texture_views.insert(tex_view);
        return texture_view_handle{
            .handle = slot_handle,
        };
    }

    auto device::create_sampler(const sampler_desc& desc) -> sampler_handle
    {
        auto sampler_ci = VkSamplerCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = as_vulkan(desc.mag_filter),
            .minFilter = as_vulkan(desc.min_filter),
            .mipmapMode = as_vulkan(desc.mipmap_mode),
            .addressModeU = as_vulkan(desc.address_u),
            .addressModeV = as_vulkan(desc.address_v),
            .addressModeW = as_vulkan(desc.address_w),
            .mipLodBias = desc.mip_lod_bias,
            .anisotropyEnable = desc.max_anisotropy.has_value() ? VK_TRUE : VK_FALSE,
            .maxAnisotropy = desc.max_anisotropy.value_or(1.0F),
            .compareEnable = desc.compare_op.has_value() ? VK_TRUE : VK_FALSE,
            .compareOp = desc.compare_op.has_value() ? as_vulkan(*desc.compare_op) : VK_COMPARE_OP_NEVER,
            .minLod = desc.min_lod,
            .maxLod = desc.max_lod,
            .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };

        auto vk_sampler = VkSampler{VK_NULL_HANDLE};
        auto result = _dispatch_table.createSampler(&sampler_ci, nullptr, &vk_sampler);
        if (result != VK_SUCCESS)
        {
            return {};
        }

        auto samp = sampler{
            .handle = vk_sampler,
        };

        auto slot_handle = _samplers.insert(samp);
        return sampler_handle{
            .handle = slot_handle,
        };
    }

    auto device::create_graphics_pipeline(const graphics_pipeline_desc& desc) -> graphics_pipeline_handle
    {
        return {};
    }

    auto device::create_compute_pipeline(const compute_pipeline_desc& desc) -> compute_pipeline_handle
    {
        return {};
    }

    auto device::create_event() -> event_handle
    {
        auto event_ci = VkEventCreateInfo{
            .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_EVENT_CREATE_DEVICE_ONLY_BIT,
        };

        auto vk_event = VkEvent{VK_NULL_HANDLE};
        auto result = _dispatch_table.createEvent(&event_ci, nullptr, &vk_event);
        if (result != VK_SUCCESS)
        {
            return {};
        }

        auto slot_handle = _events.insert(vk_event);
        return event_handle{
            .handle = slot_handle,
        };
    }

    auto device::create_timeline_semaphore() -> semaphore_handle
    {
        auto type_ci = VkSemaphoreTypeCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = 0,
        };

        auto sem_ci = VkSemaphoreCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &type_ci,
            .flags = 0,
        };

        auto vk_semaphore = VkSemaphore{VK_NULL_HANDLE};
        auto result = _dispatch_table.createSemaphore(&sem_ci, nullptr, &vk_semaphore);
        if (result != VK_SUCCESS)
        {
            return {};
        }

        auto slot_handle = _semaphores.insert(vk_semaphore);
        return semaphore_handle{
            .handle = slot_handle,
        };
    }

    auto device::create_binary_semaphore() -> semaphore_handle
    {
        auto sem_ci = VkSemaphoreCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };

        auto vk_semaphore = VkSemaphore{VK_NULL_HANDLE};
        auto result = _dispatch_table.createSemaphore(&sem_ci, nullptr, &vk_semaphore);
        if (result != VK_SUCCESS)
        {
            return {};
        }

        auto slot_handle = _semaphores.insert(vk_semaphore);
        return semaphore_handle{
            .handle = slot_handle,
        };
    }

    auto device::destroy_buffer(buffer_handle buffer) -> void
    {
        auto buf_opt = get_buffer(buffer);
        if (buf_opt.has_value())
        {
            vmaDestroyBuffer(_allocator, buf_opt->handle, buf_opt->allocation);
            _buffers.erase(buffer.handle);
        }
    }

    auto device::destroy_texture(texture_handle texture) -> void
    {
        auto tex_opt = get_texture(texture);
        if (tex_opt.has_value())
        {
            if (tex_opt->allocation != VK_NULL_HANDLE)
            {
                vmaDestroyImage(_allocator, tex_opt->handle, tex_opt->allocation);
            }
            _textures.erase(texture.handle);
        }
    }

    auto device::destroy_texture_view(texture_view_handle view) -> void
    {
        auto view_opt = get_texture_view(view);
        if (view_opt.has_value())
        {
            _dispatch_table.destroyImageView(view_opt->handle, nullptr);
            _texture_views.erase(view.handle);
        }
    }

    auto device::destroy_sampler(sampler_handle sampler) -> void
    {
        auto samp_opt = get_sampler(sampler);
        if (samp_opt.has_value())
        {
            _dispatch_table.destroySampler(samp_opt->handle, nullptr);
            _samplers.erase(sampler.handle);
        }
    }

    auto device::destroy_graphics_pipeline(graphics_pipeline_handle pipeline) -> void
    {
    }

    auto device::destroy_compute_pipeline(compute_pipeline_handle pipeline) -> void
    {
    }

    auto device::destroy_event(event_handle event) -> void
    {
        auto vk_event = get_event(event);
        if (vk_event != VK_NULL_HANDLE)
        {
            _dispatch_table.destroyEvent(vk_event, nullptr);
            _events.erase(event.handle);
        }
    }

    auto device::destroy_semaphore(semaphore_handle semaphore) -> void
    {
        auto vk_sem = get_semaphore(semaphore);
        if (vk_sem != VK_NULL_HANDLE)
        {
            _dispatch_table.destroySemaphore(vk_sem, nullptr);
            _semaphores.erase(semaphore.handle);
        }
    }

    namespace
    {
        auto create_sampler_set_layout(vkb::Device& device, uint32_t implementation_max_samplers)
            -> VkDescriptorSetLayout
        {
            const auto max_samplers = tempest::max(
                device.physical_device.properties.limits.maxPerStageDescriptorSamplers, implementation_max_samplers);

            // Variable descriptor count for samplers
            auto sampler_binding_layout = VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = max_samplers,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr,
            };

            auto binding_flags = VkDescriptorBindingFlags{VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                          VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                                          VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

            auto binding_flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = nullptr,
                .bindingCount = 1,
                .pBindingFlags = &binding_flags,
            };

            auto sampler_set_layout = VkDescriptorSetLayoutCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &binding_flags_create_info,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingCount = 1,
                .pBindings = &sampler_binding_layout,
            };

            auto* layout = VkDescriptorSetLayout{VK_NULL_HANDLE};
            [[maybe_unused]] auto result =
                device.make_table().createDescriptorSetLayout(&sampler_set_layout, nullptr, &layout);
            TEMPEST_ASSERT(result == VK_SUCCESS);
            return layout;
        }

        auto create_sampled_image_set_layout(vkb::Device& device, uint32_t implementation_max_textures)
            -> VkDescriptorSetLayout
        {
            const auto max_textures =
                tempest::max(device.physical_device.properties.limits.maxPerStageDescriptorSampledImages,
                             implementation_max_textures);

            // Variable descriptor count for images
            auto image_binding_layout = VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = max_textures,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr,
            };

            auto binding_flags = VkDescriptorBindingFlags{VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                          VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                                          VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

            auto binding_flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = nullptr,
                .bindingCount = 1,
                .pBindingFlags = &binding_flags,
            };

            auto image_set_layout = VkDescriptorSetLayoutCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &binding_flags_create_info,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingCount = 1,
                .pBindings = &image_binding_layout,
            };

            auto* layout = VkDescriptorSetLayout{VK_NULL_HANDLE};
            [[maybe_unused]] auto result =
                device.make_table().createDescriptorSetLayout(&image_set_layout, nullptr, &layout);
            TEMPEST_ASSERT(result == VK_SUCCESS);
            return layout;
        }

        auto create_storage_image_set_layout(vkb::Device& device, uint32_t implementation_max_storage_images)
            -> VkDescriptorSetLayout
        {
            const auto max_storage_images =
                tempest::max(device.physical_device.properties.limits.maxPerStageDescriptorStorageImages,
                             implementation_max_storage_images);

            // Variable descriptor count for images
            auto image_binding_layout = VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = max_storage_images,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr,
            };

            auto binding_flags = VkDescriptorBindingFlags{VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                          VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                                          VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

            auto binding_flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = nullptr,
                .bindingCount = 1,
                .pBindingFlags = &binding_flags,
            };

            auto image_set_layout = VkDescriptorSetLayoutCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &binding_flags_create_info,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingCount = 1,
                .pBindings = &image_binding_layout,
            };

            auto* layout = VkDescriptorSetLayout{VK_NULL_HANDLE};
            [[maybe_unused]] auto result =
                device.make_table().createDescriptorSetLayout(&image_set_layout, nullptr, &layout);
            TEMPEST_ASSERT(result == VK_SUCCESS);
            return layout;
        }

        auto create_default_pipeline_layout(vkb::Device& device, VkDescriptorSetLayout sampler_set_layout,
                                            VkDescriptorSetLayout sampled_image_set_layout,
                                            VkDescriptorSetLayout storage_image_set_layout) -> VkPipelineLayout
        {
            auto set_layouts = array<VkDescriptorSetLayout, 3>{
                sampler_set_layout,
                sampled_image_set_layout,
                storage_image_set_layout,
            };

            auto pipeline_layout_create_info = VkPipelineLayoutCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
                .pSetLayouts = set_layouts.data(),
                .pushConstantRangeCount = 0,
                .pPushConstantRanges = nullptr,
            };

            auto* layout = VkPipelineLayout{VK_NULL_HANDLE};
            [[maybe_unused]] auto result =
                device.make_table().createPipelineLayout(&pipeline_layout_create_info, nullptr, &layout);
            TEMPEST_ASSERT(result == VK_SUCCESS);
            return layout;
        }
    } // namespace

    device::device(vkb::Instance instance, vkb::PhysicalDevice physical_device, vkb::Device dev, device_desc desc)
        : _physical_device{tempest::move(physical_device)}, _device{tempest::move(dev)}, _desc{tempest::move(desc)},
          _instance_dispatch_table{instance.make_table()}, _dispatch_table{_device.make_table()},
          _storage_image_set_layout{create_storage_image_set_layout(_device, max_active_storage_images)},
          _sampled_image_set_layout{create_sampled_image_set_layout(_device, max_active_textures)},
          _sampler_set_layout{create_sampler_set_layout(_device, max_active_samplers)},
          _default_pipeline_layout{create_default_pipeline_layout(_device, _sampler_set_layout,
                                                                  _sampled_image_set_layout, _storage_image_set_layout)}
    {
#ifdef TEMPEST_PLATFORM_WINDOWS
        _instance_dispatch_table.fp_vkCreateWin32SurfaceKHR =
            reinterpret_cast<void*>(_instance_dispatch_table.getInstanceProcAddr("vkCreateWin32SurfaceKHR"));
        if (_instance_dispatch_table.fp_vkCreateWin32SurfaceKHR == nullptr)
        {
            tempest::terminate();
        }
#endif

        auto vulkan_functions = VmaVulkanFunctions{
            .vkGetInstanceProcAddr =
                reinterpret_cast<PFN_vkGetInstanceProcAddr>(_instance_dispatch_table.fp_vkGetInstanceProcAddr),
            .vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(_device.fp_vkGetDeviceProcAddr),
        };

        auto allocator_create_info = VmaAllocatorCreateInfo{
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = _physical_device.physical_device,
            .device = _device.device,
            .pVulkanFunctions = &vulkan_functions,
            .instance = _instance_dispatch_table.instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
        };

        [[maybe_unused]] auto vma_result = vmaCreateAllocator(&allocator_create_info, &_allocator);
        TEMPEST_ASSERT(vma_result == VK_SUCCESS);

        auto graphics_queue = _device.get_queue(vkb::QueueType::graphics);
        auto graphics_queue_index = _device.get_queue_index(vkb::QueueType::graphics);
        if (graphics_queue && graphics_queue_index)
        {
            _graphics_execution_port = make_unique<execution_port>(
                *this, graphics_queue_index.value(), vkb::QueueType::graphics, graphics_queue.value(), _dispatch_table);
        }

        auto compute_queue = _device.get_queue(vkb::QueueType::compute);
        auto compute_queue_index = _device.get_queue_index(vkb::QueueType::compute);
        if (compute_queue && compute_queue_index)
        {
            _async_compute_execution_port = make_unique<execution_port>(
                *this, compute_queue_index.value(), vkb::QueueType::compute, compute_queue.value(), _dispatch_table);
        }

        auto transfer_queue = _device.get_queue(vkb::QueueType::transfer);
        auto transfer_queue_index = _device.get_queue_index(vkb::QueueType::transfer);
        if (transfer_queue && transfer_queue_index)
        {
            _async_transfer_execution_port = make_unique<execution_port>(
                *this, transfer_queue_index.value(), vkb::QueueType::transfer, transfer_queue.value(), _dispatch_table);
        }
    }
} // namespace tempest::rhi::vk