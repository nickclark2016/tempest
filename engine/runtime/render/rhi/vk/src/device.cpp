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
            case data_format::bgra8_unorm:
                return VK_FORMAT_B8G8R8A8_UNORM;
            case data_format::bgra8_srgb:
                return VK_FORMAT_B8G8R8A8_SRGB;
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

        auto as_vulkan(shader_stage stage) noexcept -> VkShaderStageFlagBits
        {
            switch (stage)
            {
            case shader_stage::vertex:
                return VK_SHADER_STAGE_VERTEX_BIT;
            case shader_stage::tessellation_control:
                return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            case shader_stage::tessellation_evaluation:
                return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            case shader_stage::geometry:
                return VK_SHADER_STAGE_GEOMETRY_BIT;
            case shader_stage::fragment:
                return VK_SHADER_STAGE_FRAGMENT_BIT;
            case shader_stage::compute:
                return VK_SHADER_STAGE_COMPUTE_BIT;
            case shader_stage::raygen:
                return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            case shader_stage::any_hit:
                return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
            case shader_stage::closest_hit:
                return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            case shader_stage::miss:
                return VK_SHADER_STAGE_MISS_BIT_KHR;
            case shader_stage::intersection:
                return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
            case shader_stage::callable:
                return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
            }
            return VK_SHADER_STAGE_ALL;
        }

        auto as_vulkan(primitive_topology topology) noexcept -> VkPrimitiveTopology
        {
            switch (topology)
            {
            case primitive_topology::point_list:
                return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case primitive_topology::line_list:
                return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case primitive_topology::line_strip:
                return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case primitive_topology::triangle_list:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case primitive_topology::triangle_strip:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            }
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }

        auto as_vulkan(polygon_mode mode) noexcept -> VkPolygonMode
        {
            switch (mode)
            {
            case polygon_mode::fill:
                return VK_POLYGON_MODE_FILL;
            case polygon_mode::line:
                return VK_POLYGON_MODE_LINE;
            case polygon_mode::point:
                return VK_POLYGON_MODE_POINT;
            }
            return VK_POLYGON_MODE_FILL;
        }

        auto as_vulkan(cull_mode mode) noexcept -> VkCullModeFlags
        {
            switch (mode)
            {
            case cull_mode::none:
                return VK_CULL_MODE_NONE;
            case cull_mode::front:
                return VK_CULL_MODE_FRONT_BIT;
            case cull_mode::back:
                return VK_CULL_MODE_BACK_BIT;
            }
            return VK_CULL_MODE_BACK_BIT;
        }

        auto as_vulkan(vertex_winding_order order) noexcept -> VkFrontFace
        {
            switch (order)
            {
            case vertex_winding_order::clockwise:
                return VK_FRONT_FACE_CLOCKWISE;
            case vertex_winding_order::counter_clockwise:
                return VK_FRONT_FACE_COUNTER_CLOCKWISE;
            }
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        }

        auto as_vulkan(blend_factor factor) noexcept -> VkBlendFactor
        {
            switch (factor)
            {
            case blend_factor::zero:
                return VK_BLEND_FACTOR_ZERO;
            case blend_factor::one:
                return VK_BLEND_FACTOR_ONE;
            case blend_factor::src_color:
                return VK_BLEND_FACTOR_SRC_COLOR;
            case blend_factor::one_minus_src_color:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case blend_factor::dst_color:
                return VK_BLEND_FACTOR_DST_COLOR;
            case blend_factor::one_minus_dst_color:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case blend_factor::src_alpha:
                return VK_BLEND_FACTOR_SRC_ALPHA;
            case blend_factor::one_minus_src_alpha:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case blend_factor::dst_alpha:
                return VK_BLEND_FACTOR_DST_ALPHA;
            case blend_factor::one_minus_dst_alpha:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            }
            return VK_BLEND_FACTOR_ZERO;
        }

        auto as_vulkan(stencil_op op) noexcept -> VkStencilOp
        {
            switch (op)
            {
            case stencil_op::keep:
                return VK_STENCIL_OP_KEEP;
            case stencil_op::zero:
                return VK_STENCIL_OP_ZERO;
            case stencil_op::replace:
                return VK_STENCIL_OP_REPLACE;
            case stencil_op::increment_and_clamp:
                return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
            case stencil_op::decrement_and_clamp:
                return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
            case stencil_op::invert:
                return VK_STENCIL_OP_INVERT;
            case stencil_op::increment_and_wrap:
                return VK_STENCIL_OP_INCREMENT_AND_WRAP;
            case stencil_op::decrement_and_wrap:
                return VK_STENCIL_OP_DECREMENT_AND_WRAP;
            }
            return VK_STENCIL_OP_KEEP;
        }

        auto as_vulkan(image_layout layout) noexcept -> VkImageLayout
        {
            switch (layout)
            {
            case image_layout::undefined:
                return VK_IMAGE_LAYOUT_UNDEFINED;
            case image_layout::general:
                return VK_IMAGE_LAYOUT_GENERAL;
            case image_layout::present:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }
            return VK_IMAGE_LAYOUT_GENERAL;
        }

        auto create_shader_module(const vkb::DispatchTable& dispatch_table, span<const byte> ir_code) -> VkShaderModule
        {
            if (ir_code.empty() || (ir_code.size() % sizeof(uint32_t) != 0))
            {
                return VK_NULL_HANDLE;
            }

            auto module_ci = VkShaderModuleCreateInfo{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .codeSize = ir_code.size(),
                .pCode = reinterpret_cast<const uint32_t*>(ir_code.data()),
            };

            auto shader_module = VkShaderModule{VK_NULL_HANDLE};
            auto result = dispatch_table.createShaderModule(&module_ci, nullptr, &shader_module);
            if (result != VK_SUCCESS)
            {
                return VK_NULL_HANDLE;
            }
            return shader_module;
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

        for (const auto& pipeline : _graphics_pipelines)
        {
            _dispatch_table.destroyPipeline(pipeline.handle, nullptr);
        }
        _graphics_pipelines.clear();

        for (const auto& pipeline : _compute_pipelines)
        {
            _dispatch_table.destroyPipeline(pipeline.handle, nullptr);
        }
        _compute_pipelines.clear();

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

        if (_sampler_descriptor_buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(_allocator, _sampler_descriptor_buffer, _sampler_descriptor_allocation);
            _sampler_descriptor_buffer = VK_NULL_HANDLE;
            _sampler_descriptor_allocation = VK_NULL_HANDLE;
            _sampler_descriptor_buffer_ptr = nullptr;
        }

        if (_resource_descriptor_buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(_allocator, _resource_descriptor_buffer, _resource_descriptor_allocation);
            _resource_descriptor_buffer = VK_NULL_HANDLE;
            _resource_descriptor_allocation = VK_NULL_HANDLE;
            _resource_descriptor_buffer_ptr = nullptr;
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
        auto vk_raw_surface = get_raw_surface(desc.raw_surface)->handle;

        auto old_vk_swapchain = VkSwapchainKHR{VK_NULL_HANDLE};
        if (desc.old_surface != nullptr)
        {
            const auto* const old_vk_surface = static_cast<const vk::render_surface*>(desc.old_surface);
            old_vk_swapchain = old_vk_surface->get_swapchain();
        }

        const auto vk_swapchain_ci = VkSwapchainCreateInfoKHR{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = vk_raw_surface,
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
            .oldSwapchain = old_vk_swapchain,
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
            .surface = vk_raw_surface,
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
        auto shader_modules = vector<VkShaderModule>{};
        shader_modules.reserve(desc.shader_modules.size());
        auto stages = vector<VkPipelineShaderStageCreateInfo>{};
        stages.reserve(desc.shader_modules.size());

        for (const auto& sm_desc : desc.shader_modules)
        {
            auto sm = create_shader_module(_dispatch_table, sm_desc.ir_code);
            if (sm == VK_NULL_HANDLE)
            {
                for (auto mod : shader_modules)
                {
                    _dispatch_table.destroyShaderModule(mod, nullptr);
                }
                return {};
            }
            shader_modules.push_back(sm);

            const auto entry_name = sm_desc.entry_point.empty() ? "main" : sm_desc.entry_point.data();
            stages.push_back(VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = as_vulkan(sm_desc.stage),
                .module = sm,
                .pName = entry_name,
                .pSpecializationInfo = nullptr,
            });
        }

        // Dynamic rendering color and depth/stencil format configuration
        auto color_formats = vector<VkFormat>{};
        color_formats.reserve(desc.color_attachment_formats.size());
        for (auto fmt : desc.color_attachment_formats)
        {
            color_formats.push_back(as_vulkan(fmt));
        }

        auto depth_format = VK_FORMAT_UNDEFINED;
        auto stencil_format = VK_FORMAT_UNDEFINED;
        if (desc.depth_stencil_attachment_format.has_value())
        {
            depth_format = as_vulkan(*desc.depth_stencil_attachment_format);
            const auto fmt = *desc.depth_stencil_attachment_format;
            if (fmt == data_format::depth24_unorm_stencil8 || fmt == data_format::depth32_float_stencil8)
            {
                stencil_format = depth_format;
            }
        }

        auto rendering_ci = VkPipelineRenderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext = nullptr,
            .viewMask = 0,
            .colorAttachmentCount = static_cast<uint32_t>(color_formats.size()),
            .pColorAttachmentFormats = color_formats.data(),
            .depthAttachmentFormat = depth_format,
            .stencilAttachmentFormat = stencil_format,
        };

        // Programmable vertex pulling (empty vertex input state)
        auto vertex_input_ci = VkPipelineVertexInputStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr,
        };

        // Input assembly
        auto input_assembly_ci = VkPipelineInputAssemblyStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = as_vulkan(desc.primitive_topology),
            .primitiveRestartEnable = VK_FALSE,
        };

        // Viewport state (dynamic viewport and scissor)
        auto viewport_state_ci = VkPipelineViewportStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = nullptr,
            .scissorCount = 1,
            .pScissors = nullptr,
        };

        // Rasterization state
        auto rasterization_ci = VkPipelineRasterizationStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = as_vulkan(desc.rasterization_state.polygon_mode),
            .cullMode = as_vulkan(desc.rasterization_state.cull_mode),
            .frontFace = as_vulkan(desc.rasterization_state.front_face),
            .depthBiasEnable = desc.rasterization_state.depth_bias.has_value() ? VK_TRUE : VK_FALSE,
            .depthBiasConstantFactor =
                desc.rasterization_state.depth_bias.has_value() ? desc.rasterization_state.depth_bias->constant_factor : 0.0f,
            .depthBiasClamp =
                desc.rasterization_state.depth_bias.has_value() ? desc.rasterization_state.depth_bias->clamp : 0.0f,
            .depthBiasSlopeFactor =
                desc.rasterization_state.depth_bias.has_value() ? desc.rasterization_state.depth_bias->slope_factor : 0.0f,
            .lineWidth = 1.0f,
        };

        // Multisample state
        auto multisample_ci = VkPipelineMultisampleStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };

        // Depth Stencil State
        auto depth_stencil_ci = VkPipelineDepthStencilStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthTestEnable = desc.depth_stencil_state.depth_test_enable ? VK_TRUE : VK_FALSE,
            .depthWriteEnable = desc.depth_stencil_state.depth_write_enable ? VK_TRUE : VK_FALSE,
            .depthCompareOp = as_vulkan(desc.depth_stencil_state.depth_compare_op),
            .depthBoundsTestEnable = desc.depth_stencil_state.depth_bounds.has_value() ? VK_TRUE : VK_FALSE,
            .stencilTestEnable = desc.depth_stencil_state.stencil.has_value() ? VK_TRUE : VK_FALSE,
            .front =
                desc.depth_stencil_state.stencil.has_value()
                    ? VkStencilOpState{
                          .failOp = as_vulkan(desc.depth_stencil_state.stencil->front.fail_op),
                          .passOp = as_vulkan(desc.depth_stencil_state.stencil->front.pass_op),
                          .depthFailOp = as_vulkan(desc.depth_stencil_state.stencil->front.depth_fail_op),
                          .compareOp = as_vulkan(desc.depth_stencil_state.stencil->front.compare_op),
                          .compareMask = desc.depth_stencil_state.stencil->front.compare_mask,
                          .writeMask = desc.depth_stencil_state.stencil->front.write_mask,
                          .reference = desc.depth_stencil_state.stencil->front.reference,
                      }
                    : VkStencilOpState{},
            .back =
                desc.depth_stencil_state.stencil.has_value()
                    ? VkStencilOpState{
                          .failOp = as_vulkan(desc.depth_stencil_state.stencil->back.fail_op),
                          .passOp = as_vulkan(desc.depth_stencil_state.stencil->back.pass_op),
                          .depthFailOp = as_vulkan(desc.depth_stencil_state.stencil->back.depth_fail_op),
                          .compareOp = as_vulkan(desc.depth_stencil_state.stencil->back.compare_op),
                          .compareMask = desc.depth_stencil_state.stencil->back.compare_mask,
                          .writeMask = desc.depth_stencil_state.stencil->back.write_mask,
                          .reference = desc.depth_stencil_state.stencil->back.reference,
                      }
                    : VkStencilOpState{},
            .minDepthBounds =
                desc.depth_stencil_state.depth_bounds.has_value() ? desc.depth_stencil_state.depth_bounds->min_depth : 0.0f,
            .maxDepthBounds =
                desc.depth_stencil_state.depth_bounds.has_value() ? desc.depth_stencil_state.depth_bounds->max_depth : 1.0f,
        };

        // Blend State
        auto blend_attachments = vector<VkPipelineColorBlendAttachmentState>{};
        blend_attachments.reserve(desc.color_attachment_blend_states.size());
        for (const auto& blend : desc.color_attachment_blend_states)
        {
            blend_attachments.push_back(VkPipelineColorBlendAttachmentState{
                .blendEnable = blend.blend_enable ? VK_TRUE : VK_FALSE,
                .srcColorBlendFactor = as_vulkan(blend.src_color_blend_factor),
                .dstColorBlendFactor = as_vulkan(blend.dst_color_blend_factor),
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = as_vulkan(blend.src_alpha_blend_factor),
                .dstAlphaBlendFactor = as_vulkan(blend.dst_alpha_blend_factor),
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                  VK_COLOR_COMPONENT_A_BIT,
            });
        }

        // Default blend state for color attachments without explicit blend states
        while (blend_attachments.size() < color_formats.size())
        {
            blend_attachments.push_back(VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                  VK_COLOR_COMPONENT_A_BIT,
            });
        }

        auto color_blend_ci = VkPipelineColorBlendStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = static_cast<uint32_t>(blend_attachments.size()),
            .pAttachments = blend_attachments.data(),
            .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
        };

        // Dynamic states
        auto dynamic_states = array{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_LINE_WIDTH,
            VK_DYNAMIC_STATE_DEPTH_BIAS,
            VK_DYNAMIC_STATE_BLEND_CONSTANTS,
            VK_DYNAMIC_STATE_DEPTH_BOUNDS,
            VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
            VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
            VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        };

        auto dynamic_state_ci = VkPipelineDynamicStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data(),
        };

        auto pipeline_ci = VkGraphicsPipelineCreateInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &rendering_ci,
            .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
            .stageCount = static_cast<uint32_t>(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &vertex_input_ci,
            .pInputAssemblyState = &input_assembly_ci,
            .pTessellationState = nullptr,
            .pViewportState = &viewport_state_ci,
            .pRasterizationState = &rasterization_ci,
            .pMultisampleState = &multisample_ci,
            .pDepthStencilState = &depth_stencil_ci,
            .pColorBlendState = &color_blend_ci,
            .pDynamicState = &dynamic_state_ci,
            .layout = _default_pipeline_layout,
            .renderPass = VK_NULL_HANDLE,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };

        auto vk_pipeline = VkPipeline{VK_NULL_HANDLE};
        auto result = _dispatch_table.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &vk_pipeline);

        for (auto mod : shader_modules)
        {
            _dispatch_table.destroyShaderModule(mod, nullptr);
        }

        if (result != VK_SUCCESS)
        {
            return {};
        }

        auto slot_handle = _graphics_pipelines.insert(graphics_pipeline{
            .handle = vk_pipeline,
        });
        return graphics_pipeline_handle{
            .handle = slot_handle,
        };
    }

    auto device::create_compute_pipeline(const compute_pipeline_desc& desc) -> compute_pipeline_handle
    {
        auto shader_module = create_shader_module(_dispatch_table, desc.shader_module.ir_code);
        if (shader_module == VK_NULL_HANDLE)
        {
            return {};
        }

        const auto entry_point = desc.shader_module.entry_point.empty() ? "main" : desc.shader_module.entry_point.data();

        auto stage_ci = VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader_module,
            .pName = entry_point,
            .pSpecializationInfo = nullptr,
        };

        auto pipeline_ci = VkComputePipelineCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
            .stage = stage_ci,
            .layout = _default_pipeline_layout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };

        auto vk_pipeline = VkPipeline{VK_NULL_HANDLE};
        auto result = _dispatch_table.createComputePipelines(VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &vk_pipeline);

        _dispatch_table.destroyShaderModule(shader_module, nullptr);

        if (result != VK_SUCCESS)
        {
            return {};
        }

        auto slot_handle = _compute_pipelines.insert(compute_pipeline{
            .handle = vk_pipeline,
        });
        return compute_pipeline_handle{
            .handle = slot_handle,
        };
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
        auto pipe_opt = get_graphics_pipeline(pipeline);
        if (pipe_opt.has_value())
        {
            _dispatch_table.destroyPipeline(pipe_opt->handle, nullptr);
            _graphics_pipelines.erase(pipeline.handle);
        }
    }

    auto device::destroy_compute_pipeline(compute_pipeline_handle pipeline) -> void
    {
        auto pipe_opt = get_compute_pipeline(pipeline);
        if (pipe_opt.has_value())
        {
            _dispatch_table.destroyPipeline(pipe_opt->handle, nullptr);
            _compute_pipelines.erase(pipeline.handle);
        }
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
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
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
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
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
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
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

            auto push_constant_range = VkPushConstantRange{
                .stageFlags = VK_SHADER_STAGE_ALL,
                .offset = 0,
                .size = 128,
            };

            auto pipeline_layout_create_info = VkPipelineLayoutCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
                .pSetLayouts = set_layouts.data(),
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push_constant_range,
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

        // Query descriptor buffer properties
        auto descriptor_buffer_properties = VkPhysicalDeviceDescriptorBufferPropertiesEXT{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
            .pNext = nullptr,
        };
        auto device_properties2 = VkPhysicalDeviceProperties2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &descriptor_buffer_properties,
        };
        _instance_dispatch_table.getPhysicalDeviceProperties2(_physical_device.physical_device, &device_properties2);

        _sampler_descriptor_size = descriptor_buffer_properties.samplerDescriptorSize;
        _sampled_image_descriptor_size = descriptor_buffer_properties.sampledImageDescriptorSize;
        _storage_image_descriptor_size = descriptor_buffer_properties.storageImageDescriptorSize;
        _descriptor_buffer_offset_alignment = descriptor_buffer_properties.descriptorBufferOffsetAlignment;

        // Calculate buffer sizes
        auto sampler_buffer_size = max_active_samplers * _sampler_descriptor_size;
        auto sampled_images_size = max_active_textures * _sampled_image_descriptor_size;
        _storage_image_buffer_offset = ((sampled_images_size + _descriptor_buffer_offset_alignment - 1) /
                                        _descriptor_buffer_offset_alignment) *
                                       _descriptor_buffer_offset_alignment;
        auto resource_buffer_size =
            _storage_image_buffer_offset + (max_active_storage_images * _storage_image_descriptor_size);

        // Allocate Sampler Descriptor Buffer
        auto sampler_buf_ci = VkBufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = sampler_buffer_size,
            .usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        auto sampler_alloc_ci = VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        auto sampler_alloc_info = VmaAllocationInfo{};
        [[maybe_unused]] auto s_res = vmaCreateBuffer(_allocator, &sampler_buf_ci, &sampler_alloc_ci,
                                                      &_sampler_descriptor_buffer, &_sampler_descriptor_allocation,
                                                      &sampler_alloc_info);
        TEMPEST_ASSERT(s_res == VK_SUCCESS);
        _sampler_descriptor_buffer_ptr = static_cast<byte*>(sampler_alloc_info.pMappedData);

        auto sampler_bda_info = VkBufferDeviceAddressInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,
            .buffer = _sampler_descriptor_buffer,
        };
        _sampler_descriptor_buffer_address = _dispatch_table.getBufferDeviceAddress(&sampler_bda_info);

        // Allocate Resource Descriptor Buffer
        auto resource_buf_ci = VkBufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = resource_buffer_size,
            .usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        auto resource_alloc_ci = VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        auto resource_alloc_info = VmaAllocationInfo{};
        [[maybe_unused]] auto r_res = vmaCreateBuffer(_allocator, &resource_buf_ci, &resource_alloc_ci,
                                                      &_resource_descriptor_buffer, &_resource_descriptor_allocation,
                                                      &resource_alloc_info);
        TEMPEST_ASSERT(r_res == VK_SUCCESS);
        _resource_descriptor_buffer_ptr = static_cast<byte*>(resource_alloc_info.pMappedData);

        auto resource_bda_info = VkBufferDeviceAddressInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,
            .buffer = _resource_descriptor_buffer,
        };
        _resource_descriptor_buffer_address = _dispatch_table.getBufferDeviceAddress(&resource_bda_info);

        // Initialize slot tracking and free lists
        _sampler_slots.resize(max_active_samplers);
        _sampler_free_list.reserve(max_active_samplers);
        for (uint32_t i = max_active_samplers; i > 0; --i)
        {
            _sampler_free_list.push_back(i - 1);
        }

        _sampled_image_slots.resize(max_active_textures);
        _sampled_image_free_list.reserve(max_active_textures);
        for (uint32_t i = max_active_textures; i > 0; --i)
        {
            _sampled_image_free_list.push_back(i - 1);
        }

        _storage_image_slots.resize(max_active_storage_images);
        _storage_image_free_list.reserve(max_active_storage_images);
        for (uint32_t i = max_active_storage_images; i > 0; --i)
        {
            _storage_image_free_list.push_back(i - 1);
        }
    }

    auto device::allocate_descriptor(descriptor_type type) -> descriptor_handle
    {
        switch (type)
        {
        case descriptor_type::sampler: {
            if (_sampler_free_list.empty())
            {
                return {};
            }
            auto idx = _sampler_free_list.back();
            _sampler_free_list.pop_back();
            _sampler_slots[idx].allocated = true;
            return descriptor_handle{.index = idx, .generation = _sampler_slots[idx].generation};
        }
        case descriptor_type::sampled_image: {
            if (_sampled_image_free_list.empty())
            {
                return {};
            }
            auto idx = _sampled_image_free_list.back();
            _sampled_image_free_list.pop_back();
            _sampled_image_slots[idx].allocated = true;
            return descriptor_handle{.index = idx, .generation = _sampled_image_slots[idx].generation};
        }
        case descriptor_type::storage_image: {
            if (_storage_image_free_list.empty())
            {
                return {};
            }
            auto idx = _storage_image_free_list.back();
            _storage_image_free_list.pop_back();
            _storage_image_slots[idx].allocated = true;
            return descriptor_handle{.index = idx, .generation = _storage_image_slots[idx].generation};
        }
        }
        return {};
    }

    auto device::free_descriptor(descriptor_type type, descriptor_handle descriptor) -> void
    {
        switch (type)
        {
        case descriptor_type::sampler:
            if (descriptor.index < _sampler_slots.size() && _sampler_slots[descriptor.index].allocated &&
                _sampler_slots[descriptor.index].generation == descriptor.generation)
            {
                _sampler_slots[descriptor.index].allocated = false;
                _sampler_slots[descriptor.index].generation++;
                _sampler_free_list.push_back(descriptor.index);
                std::memset(_sampler_descriptor_buffer_ptr + descriptor.index * _sampler_descriptor_size, 0,
                            _sampler_descriptor_size);
            }
            break;
        case descriptor_type::sampled_image:
            if (descriptor.index < _sampled_image_slots.size() && _sampled_image_slots[descriptor.index].allocated &&
                _sampled_image_slots[descriptor.index].generation == descriptor.generation)
            {
                _sampled_image_slots[descriptor.index].allocated = false;
                _sampled_image_slots[descriptor.index].generation++;
                _sampled_image_free_list.push_back(descriptor.index);
                std::memset(_resource_descriptor_buffer_ptr + descriptor.index * _sampled_image_descriptor_size, 0,
                            _sampled_image_descriptor_size);
            }
            break;
        case descriptor_type::storage_image:
            if (descriptor.index < _storage_image_slots.size() && _storage_image_slots[descriptor.index].allocated &&
                _storage_image_slots[descriptor.index].generation == descriptor.generation)
            {
                _storage_image_slots[descriptor.index].allocated = false;
                _storage_image_slots[descriptor.index].generation++;
                _storage_image_free_list.push_back(descriptor.index);
                std::memset(_resource_descriptor_buffer_ptr + _storage_image_buffer_offset +
                                descriptor.index * _storage_image_descriptor_size,
                            0, _storage_image_descriptor_size);
            }
            break;
        }
    }

    auto device::write_sampler_descriptor(descriptor_handle slot, sampler_handle sampler) -> void
    {
        auto samp_opt = get_sampler(sampler);
        if (!samp_opt.has_value() || slot.index >= max_active_samplers)
        {
            return;
        }

        auto get_info = VkDescriptorGetInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .pNext = nullptr,
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .data =
                {
                    .pSampler = &samp_opt->handle,
                },
        };

        auto* dest_ptr = _sampler_descriptor_buffer_ptr + slot.index * _sampler_descriptor_size;
        _dispatch_table.getDescriptorEXT(&get_info, _sampler_descriptor_size, dest_ptr);
    }

    auto device::write_sampled_image_descriptor(descriptor_handle slot, texture_view_handle view, image_layout layout)
        -> void
    {
        auto view_opt = get_texture_view(view);
        if (!view_opt.has_value() || slot.index >= max_active_textures)
        {
            return;
        }

        auto image_info = VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = view_opt->handle,
            .imageLayout = as_vulkan(layout),
        };

        auto get_info = VkDescriptorGetInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .pNext = nullptr,
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .data =
                {
                    .pSampledImage = &image_info,
                },
        };

        auto* dest_ptr = _resource_descriptor_buffer_ptr + slot.index * _sampled_image_descriptor_size;
        _dispatch_table.getDescriptorEXT(&get_info, _sampled_image_descriptor_size, dest_ptr);
    }

    auto device::write_storage_image_descriptor(descriptor_handle slot, texture_view_handle view, image_layout layout)
        -> void
    {
        auto view_opt = get_texture_view(view);
        if (!view_opt.has_value() || slot.index >= max_active_storage_images)
        {
            return;
        }

        auto image_info = VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = view_opt->handle,
            .imageLayout = as_vulkan(layout),
        };

        auto get_info = VkDescriptorGetInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .pNext = nullptr,
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .data =
                {
                    .pStorageImage = &image_info,
                },
        };

        auto* dest_ptr =
            _resource_descriptor_buffer_ptr + _storage_image_buffer_offset + slot.index * _storage_image_descriptor_size;
        _dispatch_table.getDescriptorEXT(&get_info, _storage_image_descriptor_size, dest_ptr);
    }
} // namespace tempest::rhi::vk