#include <tempest/vk/rhi.hpp>

#include "window.hpp"

#include <tempest/flat_unordered_map.hpp>
#include <tempest/logger.hpp>
#include <tempest/optional.hpp>
#include <tempest/tuple.hpp>

#include <exception>

namespace tempest::rhi::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::vk::render_device"}});

        [[maybe_unused]] VKAPI_ATTR VkBool32 VKAPI_CALL
        debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                       [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, [[maybe_unused]] void* pUserData)
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

        constexpr VkPresentModeKHR to_vulkan(rhi::present_mode mode)
        {
            switch (mode)
            {
            case rhi::present_mode::IMMEDIATE:
                return VK_PRESENT_MODE_IMMEDIATE_KHR;
            case rhi::present_mode::MAILBOX:
                return VK_PRESENT_MODE_MAILBOX_KHR;
            case rhi::present_mode::FIFO:
                return VK_PRESENT_MODE_FIFO_KHR;
            case rhi::present_mode::FIFO_RELAXED:
                return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            default:
                logger->critical("Invalid present mode: {}", static_cast<uint32_t>(mode));
                std::terminate();
            }
        }

        constexpr VkFormat to_vulkan(rhi::image_format fmt)
        {
            switch (fmt)
            {
            case rhi::image_format::R8_UNORM:
                return VK_FORMAT_R8_UNORM;
            case rhi::image_format::R8_SNORM:
                return VK_FORMAT_R8_SNORM;
            case rhi::image_format::R16_UNORM:
                return VK_FORMAT_R16_UNORM;
            case rhi::image_format::R16_SNORM:
                return VK_FORMAT_R16_SNORM;
            case rhi::image_format::R16_FLOAT:
                return VK_FORMAT_R16_SFLOAT;
            case rhi::image_format::R32_FLOAT:
                return VK_FORMAT_R32_SFLOAT;
            case rhi::image_format::RG8_UNORM:
                return VK_FORMAT_R8G8_UNORM;
            case rhi::image_format::RG8_SNORM:
                return VK_FORMAT_R8G8_SNORM;
            case rhi::image_format::RG16_UNORM:
                return VK_FORMAT_R16G16_UNORM;
            case rhi::image_format::RG16_SNORM:
                return VK_FORMAT_R16G16_SNORM;
            case rhi::image_format::RG16_FLOAT:
                return VK_FORMAT_R16G16_SFLOAT;
            case rhi::image_format::RG32_FLOAT:
                return VK_FORMAT_R32G32_SFLOAT;
            case rhi::image_format::RGBA8_UNORM:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case rhi::image_format::RGBA8_SNORM:
                return VK_FORMAT_R8G8B8A8_SNORM;
            case rhi::image_format::RGBA8_SRGB:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case rhi::image_format::BGRA8_SRGB:
                return VK_FORMAT_B8G8R8A8_SRGB;
            case rhi::image_format::RGBA16_UNORM:
                return VK_FORMAT_R16G16B16A16_UNORM;
            case rhi::image_format::RGBA16_SNORM:
                return VK_FORMAT_R16G16B16A16_SNORM;
            case rhi::image_format::RGBA16_FLOAT:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case rhi::image_format::RGBA32_FLOAT:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case rhi::image_format::S8_UINT:
                return VK_FORMAT_S8_UINT;
            case rhi::image_format::D16_UNORM:
                return VK_FORMAT_D16_UNORM;
            case rhi::image_format::D24_UNORM:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case rhi::image_format::D32_FLOAT:
                return VK_FORMAT_D32_SFLOAT;
            case rhi::image_format::D16_UNORM_S8_UINT:
                return VK_FORMAT_D16_UNORM_S8_UINT;
            case rhi::image_format::D24_UNORM_S8_UINT:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case rhi::image_format::D32_FLOAT_S8_UINT:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            case rhi::image_format::A2BGR10_UNORM_PACK32:
                return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            default:
                logger->critical("Invalid image format: {}", static_cast<uint32_t>(fmt));
                std::terminate();
            }
        }

        constexpr VkColorSpaceKHR to_vulkan(rhi::color_space color_space)
        {
            switch (color_space)
            {
            case rhi::color_space::ADOBE_RGB_LINEAR:
                return VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT;
            case rhi::color_space::ADOBE_RGB_NONLINEAR:
                return VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT;
            case rhi::color_space::BT709_LINEAR:
                return VK_COLOR_SPACE_BT709_LINEAR_EXT;
            case rhi::color_space::BT709_NONLINEAR:
                return VK_COLOR_SPACE_BT709_NONLINEAR_EXT;
            case rhi::color_space::BT2020_LINEAR:
                return VK_COLOR_SPACE_BT2020_LINEAR_EXT;
            case rhi::color_space::DCI_P3_NONLINEAR:
                return VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT;
            case rhi::color_space::DISPLAY_NATIVE_AMD:
                return VK_COLOR_SPACE_DISPLAY_NATIVE_AMD;
            case rhi::color_space::DISPLAY_P3_LINEAR:
                return VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT;
            case rhi::color_space::DISPLAY_P3_NONLINEAR:
                return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
            case rhi::color_space::EXTENDED_SRGB_LINEAR:
                return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
            case rhi::color_space::EXTENDED_SRGB_NONLINEAR:
                return VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT;
            case rhi::color_space::HDR10_HLG:
                return VK_COLOR_SPACE_HDR10_HLG_EXT;
            case rhi::color_space::HDR10_ST2084:
                return VK_COLOR_SPACE_HDR10_ST2084_EXT;
            case rhi::color_space::PASS_THROUGH:
                return VK_COLOR_SPACE_PASS_THROUGH_EXT;
            case rhi::color_space::SRGB_NONLINEAR:
                return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            default:
                logger->critical("Invalid color space: {}", static_cast<uint32_t>(color_space));
                std::terminate();
            }
        }

        constexpr VkImageUsageFlags to_vulkan(enum_mask<rhi::image_usage> usage)
        {
            VkImageUsageFlags flags = 0;

            if ((usage & rhi::image_usage::COLOR_ATTACHMENT) == rhi::image_usage::COLOR_ATTACHMENT)
            {
                flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            }

            if ((usage & rhi::image_usage::DEPTH_ATTACHMENT) == rhi::image_usage::DEPTH_ATTACHMENT)
            {
                flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }

            if ((usage & rhi::image_usage::STENCIL_ATTACHMENT) == rhi::image_usage::STENCIL_ATTACHMENT)
            {
                flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }

            if ((usage & rhi::image_usage::STORAGE) == rhi::image_usage::STORAGE)
            {
                flags |= VK_IMAGE_USAGE_STORAGE_BIT;
            }

            if ((usage & rhi::image_usage::SAMPLED) == rhi::image_usage::SAMPLED)
            {
                flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
            }

            if ((usage & rhi::image_usage::TRANSFER_SRC) == rhi::image_usage::TRANSFER_SRC)
            {
                flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            }

            if ((usage & rhi::image_usage::TRANSFER_DST) == rhi::image_usage::TRANSFER_DST)
            {
                flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            }

            return flags;
        }

        constexpr VkBufferUsageFlags to_vulkan(enum_mask<rhi::buffer_usage> usage)
        {
            VkBufferUsageFlags flags = 0;

            if ((usage & rhi::buffer_usage::VERTEX) == rhi::buffer_usage::VERTEX)
            {
                flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            }

            if ((usage & rhi::buffer_usage::INDEX) == rhi::buffer_usage::INDEX)
            {
                flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            }

            if ((usage & rhi::buffer_usage::INDIRECT) == rhi::buffer_usage::INDIRECT)
            {
                flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            }

            if ((usage & rhi::buffer_usage::CONSTANT) == rhi::buffer_usage::CONSTANT)
            {
                flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            }

            if ((usage & rhi::buffer_usage::STORAGE) == rhi::buffer_usage::STORAGE)
            {
                flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            }

            if ((usage & rhi::buffer_usage::TRANSFER_SRC) == rhi::buffer_usage::TRANSFER_SRC)
            {
                flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            }

            if ((usage & rhi::buffer_usage::TRANSFER_DST) == rhi::buffer_usage::TRANSFER_DST)
            {
                flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            }

            return flags;
        }

        constexpr VkImageType to_vulkan(rhi::image_type type)
        {
            switch (type)
            {
            case rhi::image_type::IMAGE_1D:
                return VK_IMAGE_TYPE_1D;
            case rhi::image_type::IMAGE_2D:
                return VK_IMAGE_TYPE_2D;
            case rhi::image_type::IMAGE_3D:
                return VK_IMAGE_TYPE_3D;
            case rhi::image_type::IMAGE_CUBE:
                return VK_IMAGE_TYPE_2D;
            case rhi::image_type::IMAGE_1D_ARRAY:
                return VK_IMAGE_TYPE_1D;
            case rhi::image_type::IMAGE_2D_ARRAY:
                return VK_IMAGE_TYPE_2D;
            case rhi::image_type::IMAGE_CUBE_ARRAY:
                return VK_IMAGE_TYPE_2D;
            }

            unreachable();
        }

        constexpr VkSampleCountFlagBits to_vulkan(rhi::image_sample_count count)
        {
            switch (count)
            {
            case rhi::image_sample_count::SAMPLE_COUNT_1:
                return VK_SAMPLE_COUNT_1_BIT;
            case rhi::image_sample_count::SAMPLE_COUNT_2:
                return VK_SAMPLE_COUNT_2_BIT;
            case rhi::image_sample_count::SAMPLE_COUNT_4:
                return VK_SAMPLE_COUNT_4_BIT;
            case rhi::image_sample_count::SAMPLE_COUNT_8:
                return VK_SAMPLE_COUNT_8_BIT;
            case rhi::image_sample_count::SAMPLE_COUNT_16:
                return VK_SAMPLE_COUNT_16_BIT;
            case rhi::image_sample_count::SAMPLE_COUNT_32:
                return VK_SAMPLE_COUNT_32_BIT;
            case rhi::image_sample_count::SAMPLE_COUNT_64:
                return VK_SAMPLE_COUNT_64_BIT;
            }

            unreachable();
        }

        constexpr VkImageTiling to_vulkan(rhi::image_tiling_type tiling)
        {
            switch (tiling)
            {
            case rhi::image_tiling_type::OPTIMAL:
                return VK_IMAGE_TILING_OPTIMAL;
            case rhi::image_tiling_type::LINEAR:
                return VK_IMAGE_TILING_LINEAR;
            }

            unreachable();
        }

        constexpr VkSemaphoreType to_vulkan(rhi::semaphore_type type)
        {
            switch (type)
            {
            case rhi::semaphore_type::TIMELINE:
                return VK_SEMAPHORE_TYPE_TIMELINE;
            case rhi::semaphore_type::BINARY:
                return VK_SEMAPHORE_TYPE_BINARY;
            }
            unreachable();
        }

        constexpr VkPipelineStageFlags2 to_vulkan(enum_mask<rhi::pipeline_stage> stages)
        {
            VkPipelineStageFlags2 flags = 0;

            if (stages & rhi::pipeline_stage::TOP)
            {
                flags |= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            }

            if (stages & rhi::pipeline_stage::BOTTOM)
            {
                flags |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            }

            if (stages & rhi::pipeline_stage::INDIRECT_COMMAND)
            {
                flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            }

            if (stages & rhi::pipeline_stage::VERTEX_ATTRIBUTE_INPUT)
            {
                flags |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
            }

            if (stages & rhi::pipeline_stage::VERTEX_SHADER)
            {
                flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::TESSELLATION_CONTROL_SHADER)
            {
                flags |= VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::TESSELLATION_EVALUATION_SHADER)
            {
                flags |= VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::GEOMETRY_SHADER)
            {
                flags |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::FRAGMENT_SHADER)
            {
                flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::EARLY_FRAGMENT_TESTS)
            {
                flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
            }

            if (stages & rhi::pipeline_stage::LATE_FRAGMENT_TESTS)
            {
                flags |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            }

            if (stages & rhi::pipeline_stage::COLOR_ATTACHMENT_OUTPUT)
            {
                flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            }

            if (stages & rhi::pipeline_stage::COMPUTE_SHADER)
            {
                flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::COPY)
            {
                flags |= VK_PIPELINE_STAGE_2_COPY_BIT;
            }

            if (stages & rhi::pipeline_stage::RESOLVE)
            {
                flags |= VK_PIPELINE_STAGE_2_RESOLVE_BIT;
            }

            if (stages & rhi::pipeline_stage::BLIT)
            {
                flags |= VK_PIPELINE_STAGE_2_BLIT_BIT;
            }

            if (stages & rhi::pipeline_stage::CLEAR)
            {
                flags |= VK_PIPELINE_STAGE_2_CLEAR_BIT;
            }

            if (stages & rhi::pipeline_stage::ALL_TRANSFER)
            {
                flags |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            }

            return flags;
        }

        constexpr VkAccessFlags2 to_vulkan(enum_mask<memory_access> access)
        {
            VkAccessFlags2 flags = 0;

            if (access & memory_access::INDIRECT_COMMAND_READ)
            {
                flags |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
            }

            if (access & memory_access::INDEX_READ)
            {
                flags |= VK_ACCESS_2_INDEX_READ_BIT;
            }

            if (access & memory_access::VERTEX_ATTRIBUTE_READ)
            {
                flags |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
            }

            if (access & memory_access::CONSTANT_BUFFER_READ)
            {
                flags |= VK_ACCESS_2_UNIFORM_READ_BIT;
            }

            if (access & memory_access::SHADER_READ)
            {
                flags |= VK_ACCESS_2_SHADER_READ_BIT;
            }

            if (access & memory_access::SHADER_WRITE)
            {
                flags |= VK_ACCESS_2_SHADER_WRITE_BIT;
            }

            if (access & memory_access::COLOR_ATTACHMENT_READ)
            {
                flags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
            }

            if (access & memory_access::COLOR_ATTACHMENT_WRITE)
            {
                flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            }

            if (access & memory_access::DEPTH_STENCIL_ATTACHMENT_READ)
            {
                flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            }

            if (access & memory_access::DEPTH_STENCIL_ATTACHMENT_WRITE)
            {
                flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }

            if (access & memory_access::TRANSFER_READ)
            {
                flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
            }

            if (access & memory_access::TRANSFER_WRITE)
            {
                flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
            }

            if (access & memory_access::HOST_READ)
            {
                flags |= VK_ACCESS_2_HOST_READ_BIT;
            }

            if (access & memory_access::HOST_WRITE)
            {
                flags |= VK_ACCESS_2_HOST_WRITE_BIT;
            }

            if (access & memory_access::MEMORY_READ)
            {
                flags |= VK_ACCESS_2_MEMORY_READ_BIT;
            }

            if (access & memory_access::MEMORY_WRITE)
            {
                flags |= VK_ACCESS_2_MEMORY_WRITE_BIT;
            }

            if (access & memory_access::SHADER_SAMPLED_READ)
            {
                flags |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
            }

            if (access & memory_access::SHADER_STORAGE_READ)
            {
                flags |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }

            if (access & memory_access::SHADER_STORAGE_WRITE)
            {
                flags |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            }

            return flags;
        }

        constexpr VkImageLayout to_vulkan(rhi::image_layout layout)
        {
            switch (layout)
            {
            case rhi::image_layout::UNDEFINED:
                return VK_IMAGE_LAYOUT_UNDEFINED;
            case rhi::image_layout::GENERAL:
                return VK_IMAGE_LAYOUT_GENERAL;
            case rhi::image_layout::COLOR_ATTACHMENT:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case rhi::image_layout::DEPTH_STENCIL_READ_WRITE:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case rhi::image_layout::DEPTH_STENCIL_READ_ONLY:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            case rhi::image_layout::SHADER_READ_ONLY:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case rhi::image_layout::TRANSFER_SRC:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case rhi::image_layout::TRANSFER_DST:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case rhi::image_layout::DEPTH:
                return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            case rhi::image_layout::DEPTH_READ_ONLY:
                return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
            case rhi::image_layout::STENCIL:
                return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            case rhi::image_layout::STENCIL_READ_ONLY:
                return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
            case rhi::image_layout::PRESENT:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }

            unreachable();
        }

        constexpr VkImageViewType get_compatible_view_type(rhi::image_type type)
        {
            switch (type)
            {
            case rhi::image_type::IMAGE_1D:
                return VK_IMAGE_VIEW_TYPE_1D;
            case rhi::image_type::IMAGE_2D:
                return VK_IMAGE_VIEW_TYPE_2D;
            case rhi::image_type::IMAGE_3D:
                return VK_IMAGE_VIEW_TYPE_3D;
            case rhi::image_type::IMAGE_CUBE:
                return VK_IMAGE_VIEW_TYPE_CUBE;
            case rhi::image_type::IMAGE_1D_ARRAY:
                return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            case rhi::image_type::IMAGE_2D_ARRAY:
                return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            case rhi::image_type::IMAGE_CUBE_ARRAY:
                return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            }

            unreachable();
        }

        constexpr VkImageAspectFlags compute_aspect_flags(image_format fmt)
        {
            switch (fmt)
            {
            case rhi::image_format::D16_UNORM:
            case rhi::image_format::D24_UNORM:
            case rhi::image_format::D32_FLOAT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case rhi::image_format::D16_UNORM_S8_UINT:
            case rhi::image_format::D24_UNORM_S8_UINT:
            case rhi::image_format::D32_FLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            case rhi::image_format::S8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;
            case rhi::image_format::R8_UNORM:
            case rhi::image_format::R8_SNORM:
            case rhi::image_format::R16_UNORM:
            case rhi::image_format::R16_SNORM:
            case rhi::image_format::R16_FLOAT:
            case rhi::image_format::R32_FLOAT:
            case rhi::image_format::RG8_UNORM:
            case rhi::image_format::RG8_SNORM:
            case rhi::image_format::RG16_UNORM:
            case rhi::image_format::RG16_SNORM:
            case rhi::image_format::RG16_FLOAT:
            case rhi::image_format::RG32_FLOAT:
            case rhi::image_format::RGBA8_UNORM:
            case rhi::image_format::RGBA8_SNORM:
            case rhi::image_format::RGBA8_SRGB:
            case rhi::image_format::BGRA8_SRGB:
            case rhi::image_format::RGBA16_UNORM:
            case rhi::image_format::RGBA16_SNORM:
            case rhi::image_format::RGBA16_FLOAT:
            case rhi::image_format::RGBA32_FLOAT:
            case rhi::image_format::A2BGR10_UNORM_PACK32:
                return VK_IMAGE_ASPECT_COLOR_BIT;
            }

            logger->critical("Invalid image format: {}", to_underlying(fmt));
            std::terminate();
        }

        constexpr VmaMemoryUsage to_vma(rhi::memory_location location)
        {
            switch (location)
            {
            case rhi::memory_location::DEVICE:
                return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            case rhi::memory_location::HOST:
                return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            case rhi::memory_location::AUTO:
                return VMA_MEMORY_USAGE_AUTO;
            default:
                logger->critical("Invalid memory location: {}", static_cast<uint32_t>(location));
                std::terminate();
            }
        }
    } // namespace

    instance::instance(vkb::Instance instance, vector<vkb::PhysicalDevice> devices) noexcept
        : _vkb_instance(tempest::move(instance)), _vkb_phys_devices(tempest::move(devices))
    {
        _devices.resize(_vkb_phys_devices.size());
    }

    instance::~instance()
    {
        _devices.clear(); // Devices must be released before destroying the instance
        vkb::destroy_instance(_vkb_instance);
    }

    vector<rhi_device_description> instance::get_devices() const noexcept
    {
        vector<rhi_device_description> devices;

        for (size_t i = 0; i < _vkb_phys_devices.size(); ++i)
        {
            devices.push_back({
                .device_index = static_cast<uint32_t>(i),
                .device_name = _vkb_phys_devices[i].name.c_str(),
            });
        }

        return devices;
    }

    rhi::device& instance::acquire_device(uint32_t device_index) noexcept
    {
        if (_devices[device_index] == nullptr)
        {
            vkb::DeviceBuilder bldr(_vkb_phys_devices[device_index]);
            auto result = bldr.build();
            if (!result)
            {
                std::terminate();
            }

            _devices[device_index] = make_unique<rhi::vk::device>(tempest::move(result).value(), &_vkb_instance);
        }

        return *_devices[device_index];
    }

    void delete_queue::enqueue(VkObjectType type, void* handle, uint64_t frame)
    {
        dq.push({
            .last_used_frame = frame,
            .type = type,
            .handle = handle,
        });
    }

    void delete_queue::enqueue(VkObjectType type, void* handle, VmaAllocation allocation, uint64_t frame)
    {
        dq.push({
            .last_used_frame = frame,
            .type = type,
            .handle = handle,
            .allocation = allocation,
        });
    }

    void delete_queue::release_resources(uint64_t frame)
    {
        while (!dq.empty())
        {
            const auto& front = dq.front();
            if (front.last_used_frame >= frame)
            {
                return;
            }

            release_resource(front);

            dq.pop();
        }
    }

    void delete_queue::release_resource(delete_resource res)
    {
        switch (res.type)
        {
        case VK_OBJECT_TYPE_BUFFER:
            vmaDestroyBuffer(allocator, static_cast<VkBuffer>(res.handle), res.allocation);
            break;
        case VK_OBJECT_TYPE_FENCE:
            dispatch->destroyFence(static_cast<VkFence>(res.handle), nullptr);
            break;
        case VK_OBJECT_TYPE_IMAGE:
            vmaDestroyImage(allocator, static_cast<VkImage>(res.handle), res.allocation);
            break;
        case VK_OBJECT_TYPE_IMAGE_VIEW:
            dispatch->destroyImageView(static_cast<VkImageView>(res.handle), nullptr);
            break;
        case VK_OBJECT_TYPE_SEMAPHORE:
            dispatch->destroySemaphore(static_cast<VkSemaphore>(res.handle), nullptr);
            break;
        case VK_OBJECT_TYPE_SURFACE_KHR:
            vkb::destroy_surface(*instance, static_cast<VkSurfaceKHR>(res.handle));
            break;
        case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
            dispatch->destroySwapchainKHR(static_cast<VkSwapchainKHR>(res.handle), nullptr);
            break;
        default:
            break;
        }
    }

    void delete_queue::release_all_immediately()
    {
        dispatch->deviceWaitIdle();
        while (!dq.empty())
        {
            release_resource(dq.front());
            dq.pop();
        }
    }

    device::device(vkb::Device dev, vkb::Instance* instance)
        : _vkb_instance{instance}, _vkb_device{tempest::move(dev)}, _dispatch_table{dev.make_table()}
    {
        VmaVulkanFunctions fns = {
            .vkGetInstanceProcAddr = _vkb_instance->fp_vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = _vkb_device.fp_vkGetDeviceProcAddr,
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
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = _vkb_device.physical_device.physical_device,
            .device = _vkb_device.device,
            .preferredLargeHeapBlockSize = 0,
            .pAllocationCallbacks = nullptr,
            .pDeviceMemoryCallbacks = nullptr,
            .pHeapSizeLimit = nullptr,
            .pVulkanFunctions = &fns,
            .instance = _vkb_instance->instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
            .pTypeExternalMemoryHandleTypes = nullptr,
        };

        const auto result = vmaCreateAllocator(&ci, &_vma_allocator);

        if (result != VK_SUCCESS)
        {
            logger->critical("Failed to create VMA allocator: {}", to_underlying(result));
            std::terminate();
        }

        _delete_queue = delete_queue{
            .allocator = _vma_allocator,
            .dispatch = &_dispatch_table,
            .instance = _vkb_instance,
        };

        auto& queue_families = _vkb_device.queue_families;
        flat_unordered_map<uint32_t, uint32_t> queues_allocated;

        auto family_matcher = [&](VkQueueFlags flags) -> optional<tuple<VkQueueFamilyProperties, uint32_t, uint32_t>> {
            optional<tuple<VkQueueFamilyProperties, uint32_t, uint32_t>> best_match;

            uint32_t family_idx = 0;
            for (const auto& family : queue_families)
            {
                if (family.queueFlags == flags)
                {
                    auto index = queues_allocated[family_idx] < family.queueCount ? queues_allocated[family_idx]++ : 0;

                    return make_tuple(family, family_idx, index);
                }
                else if ((family.queueFlags & flags) == flags)
                {
                    if (best_match)
                    {
                        auto idx = get<1>(*best_match);
                        queues_allocated[idx]--;
                    }

                    auto index = queues_allocated[family_idx] < family.queueCount ? queues_allocated[family_idx]++ : 0;

                    best_match = make_tuple(family, family_idx, index);
                }

                family_idx++;
            }

            return best_match;
        };

        auto default_queue_match = family_matcher(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
        auto compute_queue_match = family_matcher(VK_QUEUE_COMPUTE_BIT);
        auto transfer_queue_match = family_matcher(VK_QUEUE_TRANSFER_BIT);

        if (default_queue_match)
        {
            VkQueue queue;
            _dispatch_table.getDeviceQueue(get<1>(*default_queue_match), get<2>(*default_queue_match), &queue);
            _primary_work_queue.emplace(this, &_dispatch_table, queue, get<1>(*default_queue_match),
                                        frames_in_flight());
        }
        else
        {
            logger->critical("Failed to find a suitable queue family for the device.");
            std::terminate();
        }

        if (compute_queue_match && get<1>(*compute_queue_match) != get<1>(*default_queue_match))
        {
            VkQueue queue;
            _dispatch_table.getDeviceQueue(get<1>(*compute_queue_match), get<2>(*compute_queue_match), &queue);
            _dedicated_compute_queue.emplace(this, &_dispatch_table, queue, get<1>(*compute_queue_match),
                                             frames_in_flight());
        }

        if (transfer_queue_match && get<1>(*transfer_queue_match) != get<1>(*default_queue_match))
        {
            VkQueue queue;
            _dispatch_table.getDeviceQueue(get<1>(*transfer_queue_match), get<2>(*transfer_queue_match), &queue);
            _dedicated_transfer_queue.emplace(this, &_dispatch_table, queue, get<1>(*transfer_queue_match),
                                              frames_in_flight());
        }
    }

    device::~device()
    {
        _dispatch_table.deviceWaitIdle();

        _primary_work_queue = nullopt;
        _dedicated_compute_queue = nullopt;
        _dedicated_transfer_queue = nullopt;

        _delete_queue.release_all_immediately();

        for (auto img : _images)
        {
            if (img.image_view)
            {
                _dispatch_table.destroyImageView(img.image_view, nullptr);
            }
            if (img.image && !img.swapchain_image)
            {
                vmaDestroyImage(_vma_allocator, img.image, img.allocation);
            }
        }
        _images.clear();

        for (auto buf : _buffers)
        {
            if (buf.buffer)
            {
                vmaDestroyBuffer(_vma_allocator, buf.buffer, buf.allocation);
            }
        }
        _buffers.clear();

        for (auto sc : _swapchains)
        {
            vkb::destroy_swapchain(sc.swapchain);
            vkb::destroy_surface(_vkb_instance->instance, sc.surface);
        }
        _swapchains.clear();

        for (auto fence : _fences)
        {
            _dispatch_table.destroyFence(fence.fence, nullptr);
        }
        _fences.clear();

        for (auto sem : _semaphores)
        {
            _dispatch_table.destroySemaphore(sem.semaphore, nullptr);
        }
        _semaphores.clear();

        vmaDestroyAllocator(_vma_allocator);
        vkb::destroy_device(_vkb_device);
    }

    typed_rhi_handle<rhi_handle_type::buffer> device::create_buffer(const buffer_desc& desc) noexcept
    {
        VkBufferCreateInfo buffer_ci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = desc.size,
            .usage = to_vulkan(desc.usage) | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };

        VmaAllocationCreateInfo allocation_ci = {
            .flags = 0,
            .usage = to_vma(desc.location),
            .requiredFlags = 0,
            .preferredFlags = 0,
            .memoryTypeBits = 0,
            .pool = nullptr,
            .pUserData = nullptr,
            .priority = 0,
        };

        switch (desc.access_pattern)
        {
        case rhi::host_access_pattern::RANDOM:
            allocation_ci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case rhi::host_access_pattern::SEQUENTIAL:
            allocation_ci.flags |=
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        default:
            break;
        }

        switch (desc.access_type)
        {
        case rhi::host_access_type::COHERENT:
            allocation_ci.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            break;
        default:
            break;
        }

        VmaAllocation allocation;
        VkBuffer buffer;
        VmaAllocationInfo allocation_info;

        auto result =
            vmaCreateBuffer(_vma_allocator, &buffer_ci, &allocation_ci, &buffer, &allocation, &allocation_info);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create buffer: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::buffer>::null_handle;
        }

        vk::buffer buf = {
            .allocation = allocation,
            .allocation_info = allocation_info,
            .buffer = buffer,
        };

        auto new_key = _buffers.insert(buf);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::buffer>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    typed_rhi_handle<rhi_handle_type::image> device::create_image(const image_desc& desc) noexcept
    {
        VkImageCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = to_vulkan(desc.type),
            .format = to_vulkan(desc.format),
            .extent =
                {
                    .width = desc.width,
                    .height = desc.height,
                    .depth = desc.depth,
                },
            .mipLevels = desc.mip_levels,
            .arrayLayers = desc.array_layers,
            .samples = to_vulkan(desc.sample_count),
            .tiling = to_vulkan(desc.tiling),
            .usage = to_vulkan(desc.usage),
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VmaAllocationCreateInfo allocation_ci = {
            .flags = 0,
            .usage = to_vma(desc.location),
            .requiredFlags = 0,
            .preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .memoryTypeBits = 0,
            .pool = nullptr,
            .pUserData = nullptr,
            .priority = 0,
        };

        // If the image is a render target, we should use a dedicated allocation

        if (desc.usage & rhi::image_usage::COLOR_ATTACHMENT || desc.usage & rhi::image_usage::DEPTH_ATTACHMENT ||
            desc.usage & rhi::image_usage::STENCIL_ATTACHMENT)
        {
            allocation_ci.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VmaAllocation allocation;
        VkImage image;
        VmaAllocationInfo allocation_info;

        auto result = vmaCreateImage(_vma_allocator, &ci, &allocation_ci, &image, &allocation, &allocation_info);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create image: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::image>::null_handle;
        }

        VkImageViewCreateInfo view_ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = image,
            .viewType = get_compatible_view_type(desc.type),
            .format = ci.format,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_R,
                    .g = VK_COMPONENT_SWIZZLE_G,
                    .b = VK_COMPONENT_SWIZZLE_B,
                    .a = VK_COMPONENT_SWIZZLE_A,
                },
            .subresourceRange =
                {
                    .aspectMask = compute_aspect_flags(desc.format),
                    .baseMipLevel = 0,
                    .levelCount = desc.mip_levels,
                    .baseArrayLayer = 0,
                    .layerCount = desc.array_layers,
                },
        };

        VkImageView image_view;
        result = _dispatch_table.createImageView(&view_ci, nullptr, &image_view);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create image view: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::image>::null_handle;
        }

        vk::image img = {.allocation = allocation,
                         .allocation_info = allocation_info,
                         .image = image,
                         .image_view = image_view,
                         .swapchain_image = false,
                         .image_aspect = view_ci.subresourceRange.aspectMask};

        auto new_key = _images.insert(img);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::image>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    typed_rhi_handle<rhi_handle_type::fence> device::create_fence(const fence_info& info) noexcept
    {
        VkFenceCreateInfo fence_ci = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };

        if (info.signaled)
        {
            fence_ci.flags |= VK_FENCE_CREATE_SIGNALED_BIT;
        }

        VkFence fence;
        auto result = _dispatch_table.createFence(&fence_ci, nullptr, &fence);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create fence: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::fence>::null_handle;
        }

        vk::fence new_fence = {
            .fence = fence,
        };

        auto new_key = _fences.insert(new_fence);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::fence>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    typed_rhi_handle<rhi_handle_type::semaphore> device::create_semaphore(const semaphore_info& info) noexcept
    {
        VkSemaphoreTypeCreateInfo sem_type_ci = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = nullptr,
            .semaphoreType = to_vulkan(info.type),
            .initialValue = info.initial_value,
        };

        VkSemaphoreCreateInfo sem_ci = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &sem_type_ci,
            .flags = 0,
        };

        VkSemaphore semaphore;
        auto result = _dispatch_table.createSemaphore(&sem_ci, nullptr, &semaphore);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create semaphore: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::semaphore>::null_handle;
        }

        vk::semaphore new_semaphore = {
            .semaphore = semaphore,
            .type = info.type,
        };

        auto new_key = _semaphores.insert(new_semaphore);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::semaphore>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    typed_rhi_handle<rhi_handle_type::render_surface> device::create_render_surface(
        const render_surface_desc& desc) noexcept
    {
        return create_render_surface(desc, typed_rhi_handle<rhi_handle_type::render_surface>::null_handle);
    }

    void device::destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept
    {
        auto buf_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto buf_it = _buffers.find(buf_key);
        if (buf_it != _buffers.end())
        {
            _delete_queue.enqueue(VK_OBJECT_TYPE_BUFFER, buf_it->buffer, buf_it->allocation,
                                  _current_frame + num_frames_in_flight);
            _buffers.erase(buf_key);
        }
    }

    void device::destroy_image(typed_rhi_handle<rhi_handle_type::image> handle) noexcept
    {
        auto img_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto img_it = _images.find(img_key);
        if (img_it != _images.end())
        {
            // Delete the image view
            if (img_it->image_view)
            {
                _delete_queue.enqueue(VK_OBJECT_TYPE_IMAGE_VIEW, img_it->image_view,
                                      _current_frame + num_frames_in_flight);
            }

            // Delete the image
            if (img_it->image && !img_it->swapchain_image)
            {
                _delete_queue.enqueue(VK_OBJECT_TYPE_IMAGE, img_it->image, img_it->allocation,
                                      _current_frame + num_frames_in_flight);
            }

            _images.erase(img_key);
        }
    }

    void device::destroy_fence(typed_rhi_handle<rhi_handle_type::fence> handle) noexcept
    {
        auto fence_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto fence_it = _fences.find(fence_key);
        if (fence_it != _fences.end())
        {
            _delete_queue.enqueue(VK_OBJECT_TYPE_FENCE, fence_it->fence, _current_frame + num_frames_in_flight);
            _fences.erase(fence_key);
        }
    }

    void device::destroy_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) noexcept
    {
        auto sem_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto sem_it = _semaphores.find(sem_key);
        if (sem_it != _semaphores.end())
        {
            _delete_queue.enqueue(VK_OBJECT_TYPE_SEMAPHORE, sem_it->semaphore, _current_frame + num_frames_in_flight);
            _semaphores.erase(sem_key);
        }
    }

    void device::destroy_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept
    {
        auto swapchain_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto swapchain_it = _swapchains.find(swapchain_key);
        if (swapchain_it != _swapchains.end())
        {
            for (auto img_handle : swapchain_it->images)
            {
                destroy_image(img_handle);
            }

            _delete_queue.enqueue(VK_OBJECT_TYPE_SWAPCHAIN_KHR, swapchain_it->swapchain,
                                  _current_frame + num_frames_in_flight);
            _delete_queue.enqueue(VK_OBJECT_TYPE_SURFACE_KHR, swapchain_it->surface,
                                  _current_frame + num_frames_in_flight);

            _swapchains.erase(swapchain_key);
        }
    }

    void device::recreate_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle,
                                         const render_surface_desc& desc) noexcept
    {
        auto swapchain_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto swapchain_it = _swapchains.find(swapchain_key);
        if (swapchain_it == _swapchains.end())
        {
            logger->error("Failed to recreate render surface: invalid handle");
            return;
        }

        auto old_swapchain = swapchain_it->swapchain;

        // Get a copy of the old swapchain's images
        auto old_images = swapchain_it->images;

        // Create the new swapchain
        create_render_surface(desc, handle);

        // Destroy the old swapchain
        for (auto img_handle : old_images)
        {
            destroy_image(img_handle);
        }
        _delete_queue.enqueue(VK_OBJECT_TYPE_SWAPCHAIN_KHR, old_swapchain, _current_frame + num_frames_in_flight);
    }

    rhi::work_queue& device::get_primary_work_queue() noexcept
    {
        return *_primary_work_queue;
    }

    rhi::work_queue& device::get_dedicated_transfer_queue() noexcept
    {
        if (_dedicated_transfer_queue)
        {
            return *_dedicated_transfer_queue;
        }
        else
        {
            return *_primary_work_queue;
        }
    }

    rhi::work_queue& device::get_dedicated_compute_queue() noexcept
    {
        if (_dedicated_compute_queue)
        {
            return *_dedicated_compute_queue;
        }
        else
        {
            return *_primary_work_queue;
        }
    }

    render_surface_info device::query_render_surface_info([[maybe_unused]] const rhi::window_surface& window) noexcept
    {
        return render_surface_info{};
    }

    span<const typed_rhi_handle<rhi_handle_type::image>> device::get_render_surfaces(
        [[maybe_unused]] typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept
    {
        return span<const typed_rhi_handle<rhi_handle_type::image>>{};
    }

    expected<swapchain_image_acquire_info_result, swapchain_error_code> device::acquire_next_image(
        typed_rhi_handle<rhi_handle_type::render_surface> swapchain,
        typed_rhi_handle<rhi_handle_type::fence> signal_fence) noexcept
    {
        VkFence fence_to_signal = VK_NULL_HANDLE;

        auto swapchain_key = create_slot_map_key<uint64_t>(swapchain.id, swapchain.generation);
        auto swapchain_it = _swapchains.find(swapchain_key);

        if (swapchain_it == _swapchains.end())
        {
            return unexpected{swapchain_error_code::INVALID_SWAPCHAIN_ARGUMENT};
        }

        if (signal_fence)
        {
            auto fence_key = create_slot_map_key<uint64_t>(signal_fence.id, signal_fence.generation);
            auto fence_it = _fences.find(fence_key);
            if (fence_it != _fences.end())
            {
                fence_to_signal = fence_it->fence;
            }
        }

        VkSemaphore semaphore_to_signal =
            get_semaphore(swapchain_it->frames[_current_frame % frames_in_flight()].image_acquired);

        VkAcquireNextImageInfoKHR acquire_info = {
            .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
            .pNext = nullptr,
            .swapchain = swapchain_it->swapchain,
            .timeout = numeric_limits<uint32_t>::max(),
            .semaphore = semaphore_to_signal,
            .fence = fence_to_signal,
            .deviceMask = 1,
        };

        uint32_t image_index;
        auto result = _dispatch_table.acquireNextImage2KHR(&acquire_info, &image_index);

        switch (result)
        {
        case VK_SUBOPTIMAL_KHR:
            [[fallthrough]];
        case VK_SUCCESS: {
            auto image = swapchain_it->images[image_index];
            auto& fif = swapchain_it->frames[_current_frame % frames_in_flight()];

            // We are using this frame, so we need to reset the fence
            auto vk_fence = get_fence(fif.frame_ready);
            if (vk_fence != VK_NULL_HANDLE)
            {
                _dispatch_table.resetFences(1, &vk_fence);
            }

            return swapchain_image_acquire_info_result{
                .frame_complete_fence = fif.frame_ready,
                .acquire_sem = fif.image_acquired,
                .image = image,
                .image_index = image_index,
            };
        }
        case VK_ERROR_OUT_OF_DATE_KHR:
            return unexpected{swapchain_error_code::OUT_OF_DATE};
        default:
            logger->error("Failed to acquire next image: {}", to_underlying(result));
            break;
        }

        return unexpected{swapchain_error_code::FAILURE};
    }

    bool device::is_signaled(typed_rhi_handle<rhi_handle_type::fence> fence) const noexcept
    {
        auto fence_key = create_slot_map_key<uint64_t>(fence.id, fence.generation);
        auto fence_it = _fences.find(fence_key);
        if (fence_it != _fences.end())
        {
            auto result = _dispatch_table.getFenceStatus(fence_it->fence);
            return result == VK_SUCCESS;
        }
        return false;
    }

    bool device::reset(span<const typed_rhi_handle<rhi_handle_type::fence>> fences) const noexcept
    {
        vector<VkFence> vk_fences;

        for (auto fence : fences)
        {
            auto fence_key = create_slot_map_key<uint64_t>(fence.id, fence.generation);
            auto fence_it = _fences.find(fence_key);
            if (fence_it != _fences.end())
            {
                vk_fences.push_back(fence_it->fence);
            }
        }

        auto result = _dispatch_table.resetFences(static_cast<uint32_t>(vk_fences.size()), vk_fences.data());
        return result == VK_SUCCESS;
    }

    bool device::wait(span<const typed_rhi_handle<rhi_handle_type::fence>> fences) const noexcept
    {
        vector<VkFence> vk_fences;
        for (auto fence : fences)
        {
            auto fence_key = create_slot_map_key<uint64_t>(fence.id, fence.generation);
            auto fence_it = _fences.find(fence_key);
            if (fence_it != _fences.end())
            {
                vk_fences.push_back(fence_it->fence);
            }
        }
        auto result = _dispatch_table.waitForFences(static_cast<uint32_t>(vk_fences.size()), vk_fences.data(), VK_TRUE,
                                                    numeric_limits<uint64_t>::max());
        return result == VK_SUCCESS;
    }

    void device::start_frame()
    {
        // Get all of the swapchains frame ready fences
        vector<VkFence> fences_to_wait;
        for (auto& sc : _swapchains)
        {
            auto& fif = sc.frames[_current_frame % num_frames_in_flight];
            auto fence = get_fence(fif.frame_ready);

            if (fence != VK_NULL_HANDLE)
            {
                _dispatch_table.waitForFences(1, &fence, VK_TRUE, numeric_limits<uint64_t>::max());
            }
        }

        _delete_queue.release_resources(_current_frame);

        uint32_t frame_in_flight = _current_frame % num_frames_in_flight;

        if (_primary_work_queue)
        {
            _primary_work_queue->start_frame(frame_in_flight);
        }

        if (_dedicated_compute_queue)
        {
            _dedicated_compute_queue->start_frame(frame_in_flight);
        }

        if (_dedicated_transfer_queue)
        {
            _dedicated_transfer_queue->start_frame(frame_in_flight);
        }
    }

    void device::end_frame()
    {
        _current_frame++;
    }

    uint32_t device::frames_in_flight() const noexcept
    {
        return num_frames_in_flight;
    }

    typed_rhi_handle<rhi_handle_type::image> device::acquire_image(image img) noexcept
    {
        auto new_key = _images.insert(img);

        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::image>(new_key_id, new_key_gen);
    }

    typed_rhi_handle<rhi_handle_type::command_list> device::acquire_command_list(VkCommandBuffer buf) noexcept
    {
        auto new_key = _command_buffers.insert(buf);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return {
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    VkCommandBuffer device::get_command_buffer(typed_rhi_handle<rhi_handle_type::command_list> handle) const noexcept
    {
        auto buf_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto buf_it = _command_buffers.find(buf_key);
        if (buf_it != _command_buffers.end())
        {
            return *buf_it;
        }

        return VK_NULL_HANDLE;
    }

    void device::release_command_list(typed_rhi_handle<rhi_handle_type::command_list> handle) noexcept
    {
        auto buf_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto buf_it = _command_buffers.find(buf_key);
        if (buf_it != _command_buffers.end())
        {
            _command_buffers.erase(buf_key);
        }
    }

    typed_rhi_handle<rhi_handle_type::render_surface> device::create_render_surface(
        const rhi::render_surface_desc& desc, typed_rhi_handle<rhi_handle_type::render_surface> old_swapchain) noexcept
    {
        auto window = static_cast<vk::window_surface*>(desc.window);
        auto surf_res = window->get_surface(_vkb_instance->instance);
        if (!surf_res)
        {
            logger->error("Failed to create render surface for window: {}", desc.window->name().c_str());
            return typed_rhi_handle<rhi_handle_type::render_surface>::null_handle;
        }

        VkSurfaceKHR surface = surf_res.value();

        vkb::SwapchainBuilder swap_bldr =
            vkb::SwapchainBuilder(_vkb_device.physical_device, _vkb_device, surface)
                .add_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .set_required_min_image_count(desc.min_image_count)
                .set_desired_extent(desc.width, desc.height)
                .set_desired_present_mode(to_vulkan(desc.present_mode))
                .set_desired_format({
                    .format = to_vulkan(desc.format.format),
                    .colorSpace = to_vulkan(desc.format.space),
                })
                .set_image_array_layer_count(desc.layers);

        auto old_swapchain_it =
            _swapchains.find(create_slot_map_key<uint64_t>(old_swapchain.id, old_swapchain.generation));
        if (old_swapchain_it != _swapchains.end())
        {
            swap_bldr.set_old_swapchain(old_swapchain_it->swapchain);
        }

        auto result = swap_bldr.build();
        if (!result)
        {
            return typed_rhi_handle<rhi_handle_type::render_surface>::null_handle;
        }

        auto vkb_sc = result.value();

        swapchain sc = {
            .swapchain = vkb_sc,
            .surface = surface,
            .images = {},
            .frames = {},
        };

        auto images_result = sc.swapchain.get_images();
        if (!images_result)
        {
            return typed_rhi_handle<rhi_handle_type::render_surface>::null_handle;
        }

        auto images = images_result.value();

        auto image_views_result = sc.swapchain.get_image_views();
        if (!image_views_result)
        {
            return typed_rhi_handle<rhi_handle_type::render_surface>::null_handle;
        }

        auto image_views = image_views_result.value();

        for (size_t i = 0; i < images.size(); ++i)
        {
            sc.images.push_back(acquire_image({
                .allocation = {},
                .allocation_info = {},
                .image = images[i],
                .image_view = image_views[i],
                .swapchain_image = true,
                .image_aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            }));
        }

        if (old_swapchain_it != _swapchains.end())
        {
            // Copy the old swapchain's sync objects to the new swapchain
            sc.frames = old_swapchain_it->frames;

            // Replace the old swapchain in the map
            *old_swapchain_it = tempest::move(sc);
            return old_swapchain;
        }
        else
        {
            for (size_t i = 0; i < num_frames_in_flight; ++i)
            {
                // Allocate a fence for each frame in flight
                // Allocate a semaphore for each frame in flight

                auto fence = create_fence({
                    .signaled = true,
                });
                auto sem = create_semaphore({
                    .type = rhi::semaphore_type::BINARY,
                    .initial_value = 0,
                });

                fif_data fif = {
                    .frame_ready = fence,
                    .image_acquired = sem,
                };

                sc.frames.push_back(fif);
            }
        }

        auto new_key = _swapchains.insert(sc);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::render_surface>(new_key_id, new_key_gen);
    }

    VkFence device::get_fence(typed_rhi_handle<rhi_handle_type::fence> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _fences.find(key);
        if (it != _fences.end())
        {
            return it->fence;
        }
        return VK_NULL_HANDLE;
    }

    VkSemaphore device::get_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _semaphores.find(key);
        if (it != _semaphores.end())
        {
            return it->semaphore;
        }
        return VK_NULL_HANDLE;
    }

    VkSwapchainKHR device::get_swapchain(typed_rhi_handle<rhi_handle_type::render_surface> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _swapchains.find(key);
        if (it != _swapchains.end())
        {
            return it->swapchain;
        }
        return VK_NULL_HANDLE;
    }

    optional<vk::image> device::get_image(typed_rhi_handle<rhi_handle_type::image> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _images.find(key);
        if (it != _images.end())
        {
            return *it;
        }
        return none();
    }

    work_queue::work_queue(device* parent, vkb::DispatchTable* dispatch, VkQueue queue, uint32_t queue_family_index,
                           uint32_t fif) noexcept
        : _dispatch(dispatch), _queue(queue), _queue_family_index(queue_family_index), _parent{parent}
    {
        _work_groups.resize(fif);

        VkCommandPoolCreateInfo pool_ci = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = _queue_family_index,
        };

        for (auto& wg : _work_groups)
        {
            _dispatch->createCommandPool(&pool_ci, nullptr, &wg.pool);
            wg.dispatch = _dispatch;
            wg.parent = _parent;
        }
    }

    work_queue::~work_queue()
    {
        _dispatch->queueWaitIdle(_queue); // Ensure the queue is done before working on it

        for (auto wg : _work_groups)
        {
            if (!wg.cmd_buffers.empty())
            {
                _dispatch->freeCommandBuffers(wg.pool, static_cast<uint32_t>(wg.cmd_buffers.size()),
                                              wg.cmd_buffers.data());
            }
            _dispatch->destroyCommandPool(wg.pool, nullptr);
        }
    }

    typed_rhi_handle<rhi_handle_type::command_list> work_queue::get_next_command_list(uint32_t frame_in_flight) noexcept
    {
        auto& wg = _work_groups[frame_in_flight];
        return wg.acquire_next_command_buffer();
    }

    bool work_queue::submit(span<const submit_info> infos, typed_rhi_handle<rhi_handle_type::fence> fence) noexcept
    {
        if (infos.empty())
        {
            return false;
        }

        auto submit_infos = _allocator.allocate_typed<VkSubmitInfo2>(infos.size());

        for (size_t i = 0; i < infos.size(); ++i)
        {
            auto wait_sems = _allocator.allocate_typed<VkSemaphoreSubmitInfo>(infos[i].wait_semaphores.size());
            auto signal_sems = _allocator.allocate_typed<VkSemaphoreSubmitInfo>(infos[i].signal_semaphores.size());
            auto cmds = _allocator.allocate_typed<VkCommandBufferSubmitInfo>(infos[i].command_lists.size());

            for (size_t j = 0; j < infos[i].wait_semaphores.size(); ++j)
            {
                wait_sems[j] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .pNext = nullptr,
                    .semaphore = _parent->get_semaphore(infos[i].wait_semaphores[j].semaphore),
                    .value = infos[i].wait_semaphores[j].value,
                    .stageMask = to_vulkan(infos[i].wait_semaphores[j].stages),
                    .deviceIndex = 1,
                };
            }

            for (size_t j = 0; j < infos[i].signal_semaphores.size(); ++j)
            {
                signal_sems[j] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .pNext = nullptr,
                    .semaphore = _parent->get_semaphore(infos[i].signal_semaphores[j].semaphore),
                    .value = infos[i].signal_semaphores[j].value,
                    .stageMask = to_vulkan(infos[i].signal_semaphores[j].stages),
                    .deviceIndex = 1,
                };
            }

            for (size_t j = 0; j < infos[i].command_lists.size(); ++j)
            {
                cmds[j] = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                    .pNext = nullptr,
                    .commandBuffer = _parent->get_command_buffer(infos[i].command_lists[j]),
                    .deviceMask = 1,
                };
            }

            submit_infos[i] = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext = nullptr,
                .flags = 0,
                .waitSemaphoreInfoCount = static_cast<uint32_t>(infos[i].wait_semaphores.size()),
                .pWaitSemaphoreInfos = wait_sems,
                .commandBufferInfoCount = static_cast<uint32_t>(infos[i].command_lists.size()),
                .pCommandBufferInfos = cmds,
                .signalSemaphoreInfoCount = static_cast<uint32_t>(infos[i].signal_semaphores.size()),
                .pSignalSemaphoreInfos = signal_sems,
            };
        }

        VkFence vk_fence = VK_NULL_HANDLE;
        if (fence)
        {
            vk_fence = _parent->get_fence(fence);
        }

        auto result = _dispatch->queueSubmit2(_queue, static_cast<uint32_t>(infos.size()), submit_infos, vk_fence);

        _allocator.reset();

        return result == VK_SUCCESS;
    }

    rhi::work_queue::present_result work_queue::present(const present_info& info) noexcept
    {
        auto swapchains = _allocator.allocate_typed<VkSwapchainKHR>(info.swapchain_images.size());
        auto image_indices = _allocator.allocate_typed<uint32_t>(info.swapchain_images.size());
        auto wait_sems = _allocator.allocate_typed<VkSemaphore>(info.wait_semaphores.size());
        auto results = _allocator.allocate_typed<VkResult>(info.swapchain_images.size());

        for (size_t i = 0; i < info.swapchain_images.size(); ++i)
        {
            swapchains[i] = _parent->get_swapchain(info.swapchain_images[i].render_surface);
            image_indices[i] = info.swapchain_images[i].image_index;
        }

        for (size_t i = 0; i < info.wait_semaphores.size(); ++i)
        {
            wait_sems[i] = _parent->get_semaphore(info.wait_semaphores[i]);
        }

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = static_cast<uint32_t>(info.wait_semaphores.size()),
            .pWaitSemaphores = wait_sems,
            .swapchainCount = static_cast<uint32_t>(info.swapchain_images.size()),
            .pSwapchains = swapchains,
            .pImageIndices = image_indices,
            .pResults = results,
        };

        auto result = _dispatch->queuePresentKHR(_queue, &present_info);

        _allocator.reset();

        switch (result)
        {
        case VK_SUCCESS:
            return rhi::work_queue::present_result::SUCCESS;
        case VK_SUBOPTIMAL_KHR:
            return rhi::work_queue::present_result::SUBOPTIMAL;
        case VK_ERROR_OUT_OF_DATE_KHR:
            return rhi::work_queue::present_result::OUT_OF_DATE;
        default:
            logger->error("Failed to present swapchain: {}", to_underlying(result));
            return rhi::work_queue::present_result::ERROR;
        }
    }

    void work_queue::start_frame(uint32_t frame_in_flight)
    {
        _work_groups[frame_in_flight].reset();
    }

    void work_queue::begin_command_list(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                        bool one_time_submit) noexcept
    {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pInheritanceInfo = nullptr,
        };

        if (one_time_submit)
        {
            begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        }

        _dispatch->beginCommandBuffer(_parent->get_command_buffer(command_list), &begin_info);
    }

    void work_queue::end_command_list(typed_rhi_handle<rhi_handle_type::command_list> command_list) noexcept
    {
        _dispatch->endCommandBuffer(_parent->get_command_buffer(command_list));
    }

    void work_queue::transition_image(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                      span<const image_barrier> image_barriers) noexcept
    {
        VkImageMemoryBarrier2* img_mem_barriers =
            _allocator.allocate_typed<VkImageMemoryBarrier2>(image_barriers.size());

        for (size_t i = 0; i < image_barriers.size(); ++i)
        {
            auto img = _parent->get_image(image_barriers[i].image);

            img_mem_barriers[i] = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = to_vulkan(image_barriers[i].src_stages),
                .srcAccessMask = to_vulkan(image_barriers[i].src_access),
                .dstStageMask = to_vulkan(image_barriers[i].dst_stages),
                .dstAccessMask = to_vulkan(image_barriers[i].dst_access),
                .oldLayout = to_vulkan(image_barriers[i].old_layout),
                .newLayout = to_vulkan(image_barriers[i].new_layout),
                .srcQueueFamilyIndex =
                    image_barriers[i].src_queue
                        ? static_cast<vk::work_queue*>(image_barriers[i].src_queue)->_queue_family_index
                        : VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex =
                    image_barriers[i].dst_queue
                        ? static_cast<vk::work_queue*>(image_barriers[i].dst_queue)->_queue_family_index
                        : VK_QUEUE_FAMILY_IGNORED,
                .image = img->image,
                .subresourceRange =
                    {
                        .aspectMask = img->image_aspect,
                        .baseMipLevel = 0,
                        .levelCount = VK_REMAINING_MIP_LEVELS,
                        .baseArrayLayer = 0,
                        .layerCount = VK_REMAINING_ARRAY_LAYERS,
                    },
            };
        }

        VkDependencyInfo dep_info = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers = nullptr,
            .imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers.size()),
            .pImageMemoryBarriers = img_mem_barriers,
        };

        _dispatch->cmdPipelineBarrier2(_parent->get_command_buffer(command_list), &dep_info);
    }

    void work_queue::clear_color_image(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                       typed_rhi_handle<rhi_handle_type::image> image, image_layout layout, float r,
                                       float g, float b, float a) noexcept
    {
        VkImageSubresourceRange subresource_range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        VkClearColorValue clear_color = {
            .float32 = {r, g, b, a},
        };

        _dispatch->cmdClearColorImage(_parent->get_command_buffer(command_list), _parent->get_image(image)->image,
                                      to_vulkan(layout), &clear_color, 1, &subresource_range);
    }

    void work_group::reset() noexcept
    {
        current_buffer_index = -1;
        dispatch->resetCommandPool(pool, 0);
    }

    typed_rhi_handle<rhi_handle_type::command_list> work_group::acquire_next_command_buffer() noexcept
    {
        ++current_buffer_index;

        // If there are no command buffers available, create a new one
        if (current_buffer_index >= static_cast<ptrdiff_t>(cmd_buffer_handles.size()))
        {
            VkCommandBufferAllocateInfo cmd_buffer_ci = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 4,
            };

            array<VkCommandBuffer, 4> cmds{};
            auto result = dispatch->allocateCommandBuffers(&cmd_buffer_ci, cmds.data());
            if (result != VK_SUCCESS)
            {
                logger->error("Failed to allocate command buffer: {}", to_underlying(result));
                return typed_rhi_handle<rhi_handle_type::command_list>::null_handle;
            }

            for (auto cmd : cmds)
            {
                cmd_buffers.push_back(cmd);
                cmd_buffer_handles.push_back(parent->acquire_command_list(cmd));
            }
        }

        return cmd_buffer_handles[current_buffer_index];
    }

    optional<typed_rhi_handle<rhi_handle_type::command_list>> work_group::current_command_buffer() const noexcept
    {
        if (current_buffer_index < static_cast<ptrdiff_t>(cmd_buffer_handles.size()))
        {
            return some(cmd_buffer_handles[current_buffer_index]);
        }

        return none();
    }

    unique_ptr<rhi::instance> create_instance() noexcept
    {
        vkb::InstanceBuilder bldr;
        bldr.set_app_name("Tempest Application")
            .set_app_version(0, 1, 0)
            .set_engine_name("Tempest Engine")
            .set_engine_version(0, 1, 0)
            .require_api_version(1, 3, 0);

#if defined(_DEBUG)
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
#if defined(TEMPEST_ENABLE_GPU_ASSISTED_VALDATION)
        bldr.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
#endif
#endif
        auto result = bldr.build();
        if (!result)
        {
            return nullptr;
        }

        auto instance = tempest::move(result).value();

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

        VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragment_shader_interlock = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT,
            .pNext = nullptr,
            .fragmentShaderSampleInterlock = VK_TRUE,
            .fragmentShaderPixelInterlock = VK_TRUE,
            .fragmentShaderShadingRateInterlock = VK_FALSE,
        };

        VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = nullptr,
            .bufferDeviceAddress = VK_TRUE,
            .bufferDeviceAddressCaptureReplay = VK_TRUE,
            .bufferDeviceAddressMultiDevice = VK_FALSE,
        };

        vkb::PhysicalDeviceSelector selector =
            vkb::PhysicalDeviceSelector(instance)
                .prefer_gpu_device_type(vkb::PreferredDeviceType::integrated)
                .defer_surface_initialization()
                .require_present()
                .add_required_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME)
                .add_required_extension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME)
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
                    .fragmentStoresAndAtomics = VK_TRUE,
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
                    .shaderDrawParameters = VK_TRUE,
                })
                .set_required_features_12({
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                    .pNext = &buffer_device_address,
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
                    .uniformBufferStandardLayout = VK_TRUE,
                    .shaderSubgroupExtendedTypes = VK_FALSE,
                    .separateDepthStencilLayouts = VK_TRUE,
                    .hostQueryReset = VK_TRUE,
                    .timelineSemaphore = VK_FALSE,
                    .bufferDeviceAddress = VK_TRUE,
                    .bufferDeviceAddressCaptureReplay = VK_FALSE,
                    .bufferDeviceAddressMultiDevice = VK_FALSE,
                    .vulkanMemoryModel = VK_TRUE,
                    .vulkanMemoryModelDeviceScope = VK_TRUE,
                    .vulkanMemoryModelAvailabilityVisibilityChains = VK_TRUE,
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
                    .shaderDemoteToHelperInvocation = VK_TRUE,
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
                .add_required_extension_features(extended_dynamic_state)
                .add_required_extension_features(fragment_shader_interlock);

        auto devices = selector.select_devices();
        if (!devices || devices->empty())
        {
            return nullptr;
        }

        vector<vkb::PhysicalDevice> vkb_devices(devices->begin(), devices->end());

        return make_unique<rhi::vk::instance>(tempest::move(instance), tempest::move(vkb_devices));
    }
} // namespace tempest::rhi::vk
