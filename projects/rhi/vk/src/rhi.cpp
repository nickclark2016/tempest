#include <tempest/math_utils.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/vk/rhi.hpp>

#include "window.hpp"

#include <tempest/flat_unordered_map.hpp>
#include <tempest/logger.hpp>
#include <tempest/optional.hpp>
#include <tempest/tuple.hpp>

#include <exception>
#include <vulkan/vulkan_core.h>

namespace tempest::rhi::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::rhi::vk::rhi"}});

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
            case rhi::present_mode::immediate:
                return VK_PRESENT_MODE_IMMEDIATE_KHR;
            case rhi::present_mode::mailbox:
                return VK_PRESENT_MODE_MAILBOX_KHR;
            case rhi::present_mode::fifo:
                return VK_PRESENT_MODE_FIFO_KHR;
            case rhi::present_mode::fifo_relaxed:
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
            case rhi::image_format::r8_unorm:
                return VK_FORMAT_R8_UNORM;
            case rhi::image_format::r8_snorm:
                return VK_FORMAT_R8_SNORM;
            case rhi::image_format::r16_unorm:
                return VK_FORMAT_R16_UNORM;
            case rhi::image_format::r16_snorm:
                return VK_FORMAT_R16_SNORM;
            case rhi::image_format::r16_float:
                return VK_FORMAT_R16_SFLOAT;
            case rhi::image_format::r32_float:
                return VK_FORMAT_R32_SFLOAT;
            case rhi::image_format::rg8_unorm:
                return VK_FORMAT_R8G8_UNORM;
            case rhi::image_format::rg8_snorm:
                return VK_FORMAT_R8G8_SNORM;
            case rhi::image_format::rg16_unorm:
                return VK_FORMAT_R16G16_UNORM;
            case rhi::image_format::rg16_snorm:
                return VK_FORMAT_R16G16_SNORM;
            case rhi::image_format::rg16_float:
                return VK_FORMAT_R16G16_SFLOAT;
            case rhi::image_format::rg32_float:
                return VK_FORMAT_R32G32_SFLOAT;
            case rhi::image_format::rgba8_unorm:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case rhi::image_format::rgba8_snorm:
                return VK_FORMAT_R8G8B8A8_SNORM;
            case rhi::image_format::rgba8_srgb:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case rhi::image_format::bgra8_srgb:
                return VK_FORMAT_B8G8R8A8_SRGB;
            case rhi::image_format::rgba16_unorm:
                return VK_FORMAT_R16G16B16A16_UNORM;
            case rhi::image_format::rgba16_snorm:
                return VK_FORMAT_R16G16B16A16_SNORM;
            case rhi::image_format::rgba16_float:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case rhi::image_format::rgba32_float:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case rhi::image_format::s8_uint:
                return VK_FORMAT_S8_UINT;
            case rhi::image_format::d16_unorm:
                return VK_FORMAT_D16_UNORM;
            case rhi::image_format::d24_unorm:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case rhi::image_format::d32_float:
                return VK_FORMAT_D32_SFLOAT;
            case rhi::image_format::d16_unorm_s8_uint:
                return VK_FORMAT_D16_UNORM_S8_UINT;
            case rhi::image_format::d24_unorm_s8_uint:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case rhi::image_format::d32_float_s8_uint:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            case rhi::image_format::a2bgr10_unorm_pack32:
                return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            default:
                logger->critical("Invalid image format: {}", static_cast<uint32_t>(fmt));
                std::terminate();
            }
        }

        constexpr VkFormat to_vulkan(rhi::buffer_format fmt)
        {
            switch (fmt)
            {
            case rhi::buffer_format::r8_snorm:
                return VK_FORMAT_R8_SNORM;
            case rhi::buffer_format::r8_unorm:
                return VK_FORMAT_R8_UNORM;
            case rhi::buffer_format::r8_uint:
                return VK_FORMAT_R8_UINT;
            case rhi::buffer_format::r8_sint:
                return VK_FORMAT_R8_SINT;
            case rhi::buffer_format::r16_unorm:
                return VK_FORMAT_R16_UNORM;
            case rhi::buffer_format::r16_snorm:
                return VK_FORMAT_R16_SNORM;
            case rhi::buffer_format::r16_uint:
                return VK_FORMAT_R16_UINT;
            case rhi::buffer_format::r16_sint:
                return VK_FORMAT_R16_SINT;
            case rhi::buffer_format::r16_float:
                return VK_FORMAT_R16_SFLOAT;
            case rhi::buffer_format::r32_float:
                return VK_FORMAT_R32_SFLOAT;
            case rhi::buffer_format::r32_uint:
                return VK_FORMAT_R32_UINT;
            case rhi::buffer_format::r32_sint:
                return VK_FORMAT_R32_SINT;
            case rhi::buffer_format::rg8_snorm:
                return VK_FORMAT_R8G8_SNORM;
            case rhi::buffer_format::rg8_unorm:
                return VK_FORMAT_R8G8_UNORM;
            case rhi::buffer_format::rg8_uint:
                return VK_FORMAT_R8G8_UINT;
            case rhi::buffer_format::rg8_sint:
                return VK_FORMAT_R8G8_SINT;
            case rhi::buffer_format::rg16_unorm:
                return VK_FORMAT_R16G16_UNORM;
            case rhi::buffer_format::rg16_snorm:
                return VK_FORMAT_R16G16_SNORM;
            case rhi::buffer_format::rg16_float:
                return VK_FORMAT_R16G16_SFLOAT;
            case rhi::buffer_format::rg16_uint:
                return VK_FORMAT_R16G16_UINT;
            case rhi::buffer_format::rg16_sint:
                return VK_FORMAT_R16G16_SINT;
            case rhi::buffer_format::rg32_float:
                return VK_FORMAT_R32G32_SFLOAT;
            case rhi::buffer_format::rg32_uint:
                return VK_FORMAT_R32G32_UINT;
            case rhi::buffer_format::rg32_sint:
                return VK_FORMAT_R32G32_SINT;
            case rhi::buffer_format::rgb8_snorm:
                return VK_FORMAT_R8G8B8_SNORM;
            case rhi::buffer_format::rgb8_unorm:
                return VK_FORMAT_R8G8B8_UNORM;
            case rhi::buffer_format::rgb8_uint:
                return VK_FORMAT_R8G8B8_UINT;
            case rhi::buffer_format::rgb8_sint:
                return VK_FORMAT_R8G8B8_SINT;
            case rhi::buffer_format::rgb16_unorm:
                return VK_FORMAT_R16G16B16_UNORM;
            case rhi::buffer_format::rgb16_snorm:
                return VK_FORMAT_R16G16B16_SNORM;
            case rhi::buffer_format::rgb16_float:
                return VK_FORMAT_R16G16B16_SFLOAT;
            case rhi::buffer_format::rgb16_uint:
                return VK_FORMAT_R16G16B16_UINT;
            case rhi::buffer_format::rgb16_sint:
                return VK_FORMAT_R16G16B16_SINT;
            case rhi::buffer_format::rgb32_float:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case rhi::buffer_format::rgb32_uint:
                return VK_FORMAT_R32G32B32_UINT;
            case rhi::buffer_format::rgb32_sint:
                return VK_FORMAT_R32G32B32_SINT;
            case rhi::buffer_format::rgba8_snorm:
                return VK_FORMAT_R8G8B8A8_SNORM;
            case rhi::buffer_format::rgba8_unorm:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case rhi::buffer_format::rgba8_uint:
                return VK_FORMAT_R8G8B8A8_UINT;
            case rhi::buffer_format::rgba8_sint:
                return VK_FORMAT_R8G8B8A8_SINT;
            case rhi::buffer_format::rgba16_unorm:
                return VK_FORMAT_R16G16B16A16_UNORM;
            case rhi::buffer_format::rgba16_snorm:
                return VK_FORMAT_R16G16B16A16_SNORM;
            case rhi::buffer_format::rgba16_float:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case rhi::buffer_format::rgba16_uint:
                return VK_FORMAT_R16G16B16A16_UINT;
            case rhi::buffer_format::rgba16_sint:
                return VK_FORMAT_R16G16B16A16_SINT;
            case rhi::buffer_format::rgba32_float:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case rhi::buffer_format::rgba32_uint:
                return VK_FORMAT_R32G32B32A32_UINT;
            case rhi::buffer_format::rgba32_sint:
                return VK_FORMAT_R32G32B32A32_SINT;
            default:
                logger->critical("Invalid buffer format: {}", static_cast<uint32_t>(fmt));
                std::terminate();
            }
        }

        constexpr VkVertexInputRate to_vulkan(rhi::vertex_input_rate rate)
        {
            switch (rate)
            {
            case rhi::vertex_input_rate::vertex:
                return VK_VERTEX_INPUT_RATE_VERTEX;
            case rhi::vertex_input_rate::instance:
                return VK_VERTEX_INPUT_RATE_INSTANCE;
            default:
                logger->critical("Invalid vertex input rate: {}", static_cast<uint32_t>(rate));
                std::terminate();
            }
        }

        constexpr VkColorSpaceKHR to_vulkan(rhi::color_space color_space)
        {
            switch (color_space)
            {
            case rhi::color_space::adobe_rgb_linear:
                return VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT;
            case rhi::color_space::adobe_rgb_nonlinear:
                return VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT;
            case rhi::color_space::bt709_linear:
                return VK_COLOR_SPACE_BT709_LINEAR_EXT;
            case rhi::color_space::bt709_nonlinear:
                return VK_COLOR_SPACE_BT709_NONLINEAR_EXT;
            case rhi::color_space::bt2020_linear:
                return VK_COLOR_SPACE_BT2020_LINEAR_EXT;
            case rhi::color_space::dci_p3_nonlinear:
                return VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT;
            case rhi::color_space::display_native_amd:
                return VK_COLOR_SPACE_DISPLAY_NATIVE_AMD;
            case rhi::color_space::display_p3_linear:
                return VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT;
            case rhi::color_space::display_p3_nonlinear:
                return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
            case rhi::color_space::extended_srgb_linear:
                return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
            case rhi::color_space::extended_srgb_nonlinear:
                return VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT;
            case rhi::color_space::hdr10_hlg:
                return VK_COLOR_SPACE_HDR10_HLG_EXT;
            case rhi::color_space::hdr10_st2084:
                return VK_COLOR_SPACE_HDR10_ST2084_EXT;
            case rhi::color_space::pass_through:
                return VK_COLOR_SPACE_PASS_THROUGH_EXT;
            case rhi::color_space::srgb_nonlinear:
                return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            default:
                logger->critical("Invalid color space: {}", static_cast<uint32_t>(color_space));
                std::terminate();
            }
        }

        constexpr VkImageUsageFlags to_vulkan(enum_mask<rhi::image_usage> usage)
        {
            VkImageUsageFlags flags = 0;

            if ((usage & rhi::image_usage::color_attachment) == rhi::image_usage::color_attachment)
            {
                flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            }

            if ((usage & rhi::image_usage::depth_attachment) == rhi::image_usage::depth_attachment)
            {
                flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }

            if ((usage & rhi::image_usage::stencil_attachment) == rhi::image_usage::stencil_attachment)
            {
                flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }

            if ((usage & rhi::image_usage::storage) == rhi::image_usage::storage)
            {
                flags |= VK_IMAGE_USAGE_STORAGE_BIT;
            }

            if ((usage & rhi::image_usage::sampled) == rhi::image_usage::sampled)
            {
                flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
            }

            if ((usage & rhi::image_usage::transfer_src) == rhi::image_usage::transfer_src)
            {
                flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            }

            if ((usage & rhi::image_usage::transfer_dst) == rhi::image_usage::transfer_dst)
            {
                flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            }

            return flags;
        }

        constexpr VkBufferUsageFlags to_vulkan(enum_mask<rhi::buffer_usage> usage)
        {
            VkBufferUsageFlags flags = 0;

            if ((usage & rhi::buffer_usage::index) == rhi::buffer_usage::index)
            {
                flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            }

            if ((usage & rhi::buffer_usage::indirect) == rhi::buffer_usage::indirect)
            {
                flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            }

            if ((usage & rhi::buffer_usage::constant) == rhi::buffer_usage::constant)
            {
                flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            }

            if ((usage & rhi::buffer_usage::structured) == rhi::buffer_usage::structured)
            {
                flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            }

            if ((usage & rhi::buffer_usage::transfer_src) == rhi::buffer_usage::transfer_src)
            {
                flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            }

            if ((usage & rhi::buffer_usage::transfer_dst) == rhi::buffer_usage::transfer_dst)
            {
                flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            }

            if ((usage & rhi::buffer_usage::vertex) == rhi::buffer_usage::vertex)
            {
                flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            }

            if ((usage & rhi::buffer_usage::descriptor) == rhi::buffer_usage::descriptor)
            {
                flags |= VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
                         VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
            }

            return flags;
        }

        constexpr VkImageType to_vulkan(rhi::image_type type)
        {
            switch (type)
            {
            case rhi::image_type::image_1d:
                return VK_IMAGE_TYPE_1D;
            case rhi::image_type::image_2d:
                return VK_IMAGE_TYPE_2D;
            case rhi::image_type::image_3d:
                return VK_IMAGE_TYPE_3D;
            case rhi::image_type::image_cube:
                return VK_IMAGE_TYPE_2D;
            case rhi::image_type::image_1d_array:
                return VK_IMAGE_TYPE_1D;
            case rhi::image_type::image_2d_array:
                return VK_IMAGE_TYPE_2D;
            case rhi::image_type::image_cube_array:
                return VK_IMAGE_TYPE_2D;
            }

            unreachable();
        }

        constexpr VkSampleCountFlagBits to_vulkan(rhi::image_sample_count count)
        {
            switch (count)
            {
            case rhi::image_sample_count::sample_count_1:
                return VK_SAMPLE_COUNT_1_BIT;
            case rhi::image_sample_count::sample_count_2:
                return VK_SAMPLE_COUNT_2_BIT;
            case rhi::image_sample_count::sample_count_4:
                return VK_SAMPLE_COUNT_4_BIT;
            case rhi::image_sample_count::sample_count_8:
                return VK_SAMPLE_COUNT_8_BIT;
            case rhi::image_sample_count::sample_count_16:
                return VK_SAMPLE_COUNT_16_BIT;
            case rhi::image_sample_count::sample_count_32:
                return VK_SAMPLE_COUNT_32_BIT;
            case rhi::image_sample_count::sample_count_64:
                return VK_SAMPLE_COUNT_64_BIT;
            }

            unreachable();
        }

        constexpr VkImageTiling to_vulkan(rhi::image_tiling_type tiling)
        {
            switch (tiling)
            {
            case rhi::image_tiling_type::optimal:
                return VK_IMAGE_TILING_OPTIMAL;
            case rhi::image_tiling_type::linear:
                return VK_IMAGE_TILING_LINEAR;
            }

            unreachable();
        }

        constexpr VkSemaphoreType to_vulkan(rhi::semaphore_type type)
        {
            switch (type)
            {
            case rhi::semaphore_type::timeline:
                return VK_SEMAPHORE_TYPE_TIMELINE;
            case rhi::semaphore_type::binary:
                return VK_SEMAPHORE_TYPE_BINARY;
            }
            unreachable();
        }

        constexpr VkPipelineStageFlags2 to_vulkan(enum_mask<rhi::pipeline_stage> stages)
        {
            VkPipelineStageFlags2 flags = 0;

            if (stages & rhi::pipeline_stage::top)
            {
                flags |= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            }

            if (stages & rhi::pipeline_stage::bottom)
            {
                flags |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            }

            if (stages & rhi::pipeline_stage::indirect_command)
            {
                flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            }

            if (stages & rhi::pipeline_stage::vertex_attribute_input)
            {
                flags |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
            }

            if (stages & rhi::pipeline_stage::vertex_shader)
            {
                flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::tessellation_control_shader)
            {
                flags |= VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::tessellation_evaluation_shader)
            {
                flags |= VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::geometry_shader)
            {
                flags |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::fragment_shader)
            {
                flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::early_fragment_tests)
            {
                flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
            }

            if (stages & rhi::pipeline_stage::late_fragment_tests)
            {
                flags |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            }

            if (stages & rhi::pipeline_stage::color_attachment_output)
            {
                flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            }

            if (stages & rhi::pipeline_stage::compute_shader)
            {
                flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            }

            if (stages & rhi::pipeline_stage::copy)
            {
                flags |= VK_PIPELINE_STAGE_2_COPY_BIT;
            }

            if (stages & rhi::pipeline_stage::resolve)
            {
                flags |= VK_PIPELINE_STAGE_2_RESOLVE_BIT;
            }

            if (stages & rhi::pipeline_stage::blit)
            {
                flags |= VK_PIPELINE_STAGE_2_BLIT_BIT;
            }

            if (stages & rhi::pipeline_stage::clear)
            {
                flags |= VK_PIPELINE_STAGE_2_CLEAR_BIT;
            }

            if (stages & rhi::pipeline_stage::all_transfer)
            {
                flags |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            }

            if (stages & rhi::pipeline_stage::host)
            {
                flags |= VK_PIPELINE_STAGE_2_HOST_BIT;
            }

            if (stages & rhi::pipeline_stage::all)
            {
                flags |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            }

            return flags;
        }

        constexpr VkAccessFlags2 to_vulkan(enum_mask<memory_access> access)
        {
            VkAccessFlags2 flags = 0;

            if (access & memory_access::indirect_command_read)
            {
                flags |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
            }

            if (access & memory_access::index_read)
            {
                flags |= VK_ACCESS_2_INDEX_READ_BIT;
            }

            if (access & memory_access::vertex_attribute_read)
            {
                flags |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
            }

            if (access & memory_access::constant_buffer_read)
            {
                flags |= VK_ACCESS_2_UNIFORM_READ_BIT;
            }

            if (access & memory_access::shader_read)
            {
                flags |= VK_ACCESS_2_SHADER_READ_BIT;
            }

            if (access & memory_access::shader_write)
            {
                flags |= VK_ACCESS_2_SHADER_WRITE_BIT;
            }

            if (access & memory_access::color_attachment_read)
            {
                flags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
            }

            if (access & memory_access::color_attachment_write)
            {
                flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            }

            if (access & memory_access::depth_stencil_attachment_read)
            {
                flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            }

            if (access & memory_access::depth_stencil_attachment_write)
            {
                flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }

            if (access & memory_access::transfer_read)
            {
                flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
            }

            if (access & memory_access::transfer_write)
            {
                flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
            }

            if (access & memory_access::host_read)
            {
                flags |= VK_ACCESS_2_HOST_READ_BIT;
            }

            if (access & memory_access::host_write)
            {
                flags |= VK_ACCESS_2_HOST_WRITE_BIT;
            }

            if (access & memory_access::memory_read)
            {
                flags |= VK_ACCESS_2_MEMORY_READ_BIT;
            }

            if (access & memory_access::memory_write)
            {
                flags |= VK_ACCESS_2_MEMORY_WRITE_BIT;
            }

            if (access & memory_access::shader_sampled_read)
            {
                flags |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
            }

            if (access & memory_access::shader_storage_read)
            {
                flags |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }

            if (access & memory_access::shader_storage_write)
            {
                flags |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            }

            if (access & memory_access::descriptor_buffer_read)
            {
                flags |= VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT;
            }

            return flags;
        }

        constexpr VkImageLayout to_vulkan(rhi::image_layout layout)
        {
            switch (layout)
            {
            case rhi::image_layout::undefined:
                return VK_IMAGE_LAYOUT_UNDEFINED;
            case rhi::image_layout::general:
                return VK_IMAGE_LAYOUT_GENERAL;
            case rhi::image_layout::color_attachment:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case rhi::image_layout::depth_stencil_read_write:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case rhi::image_layout::depth_stencil_read_only:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            case rhi::image_layout::shader_read_only:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case rhi::image_layout::transfer_src:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case rhi::image_layout::transfer_dst:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case rhi::image_layout::depth:
                return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            case rhi::image_layout::depth_read_only:
                return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
            case rhi::image_layout::stencil:
                return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            case rhi::image_layout::stencil_read_only:
                return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
            case rhi::image_layout::present:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }

            unreachable();
        }

        constexpr VkAttachmentLoadOp to_vulkan(rhi::work_queue::load_op op)
        {
            switch (op)
            {
            case rhi::work_queue::load_op::load:
                return VK_ATTACHMENT_LOAD_OP_LOAD;
            case rhi::work_queue::load_op::clear:
                return VK_ATTACHMENT_LOAD_OP_CLEAR;
            case rhi::work_queue::load_op::dont_care:
                return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            }
            unreachable();
        }

        constexpr VkAttachmentStoreOp to_vulkan(rhi::work_queue::store_op op)
        {
            switch (op)
            {
            case rhi::work_queue::store_op::store:
                return VK_ATTACHMENT_STORE_OP_STORE;
            case rhi::work_queue::store_op::dont_care:
                return VK_ATTACHMENT_STORE_OP_DONT_CARE;
            case rhi::work_queue::store_op::none:
                return VK_ATTACHMENT_STORE_OP_NONE;
            }
            unreachable();
        }

        constexpr VkDescriptorType to_vulkan(rhi::descriptor_type type)
        {
            switch (type)
            {
            case rhi::descriptor_type::sampler:
                return VK_DESCRIPTOR_TYPE_SAMPLER;
            case rhi::descriptor_type::sampled_image:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case rhi::descriptor_type::storage_image:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case rhi::descriptor_type::constant_buffer:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case rhi::descriptor_type::structured_buffer:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case rhi::descriptor_type::dynamic_constant_buffer:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            case rhi::descriptor_type::dynamic_structured_buffer:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            case rhi::descriptor_type::combined_image_sampler:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            }
            unreachable();
        }

        constexpr VkShaderStageFlags to_vulkan(enum_mask<rhi::shader_stage> stages)
        {
            VkShaderStageFlags flags = 0;

            if (stages & rhi::shader_stage::vertex)
            {
                flags |= VK_SHADER_STAGE_VERTEX_BIT;
            }

            if (stages & rhi::shader_stage::tessellation_control)
            {
                flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            }

            if (stages & rhi::shader_stage::tessellation_evaluation)
            {
                flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            }

            if (stages & rhi::shader_stage::geometry)
            {
                flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
            }

            if (stages & rhi::shader_stage::fragment)
            {
                flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            if (stages & rhi::shader_stage::compute)
            {
                flags |= VK_SHADER_STAGE_COMPUTE_BIT;
            }

            return flags;
        }

        constexpr VkDescriptorBindingFlags to_vulkan(enum_mask<rhi::descriptor_binding_flags> flags)
        {
            VkDescriptorBindingFlags vk_flags = 0;
            if (flags & rhi::descriptor_binding_flags::partially_bound)
            {
                vk_flags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            }
            if (flags & rhi::descriptor_binding_flags::variable_length)
            {
                vk_flags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            }
            return vk_flags;
        }

        constexpr VkPrimitiveTopology to_vulkan(rhi::primitive_topology topo)
        {
            switch (topo)
            {
            case rhi::primitive_topology::point_list:
                return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case rhi::primitive_topology::line_list:
                return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case rhi::primitive_topology::line_strip:
                return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case rhi::primitive_topology::triangle_list:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case rhi::primitive_topology::triangle_strip:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case rhi::primitive_topology::triangle_fan:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
            }
            unreachable();
        }

        constexpr VkFrontFace to_vulkan(rhi::vertex_winding face)
        {
            switch (face)
            {
            case rhi::vertex_winding::counter_clockwise:
                return VK_FRONT_FACE_COUNTER_CLOCKWISE;
            case rhi::vertex_winding::clockwise:
                return VK_FRONT_FACE_CLOCKWISE;
            }
            unreachable();
        }

        constexpr VkCullModeFlags to_vulkan(enum_mask<rhi::cull_mode> mode)
        {
            VkCullModeFlags flags = 0;
            if (mode & rhi::cull_mode::front)
            {
                flags |= VK_CULL_MODE_FRONT_BIT;
            }
            if (mode & rhi::cull_mode::back)
            {
                flags |= VK_CULL_MODE_BACK_BIT;
            }
            return flags;
        }

        constexpr VkPolygonMode to_vulkan(rhi::polygon_mode mode)
        {
            switch (mode)
            {
            case rhi::polygon_mode::fill:
                return VK_POLYGON_MODE_FILL;
            case rhi::polygon_mode::line:
                return VK_POLYGON_MODE_LINE;
            case rhi::polygon_mode::point:
                return VK_POLYGON_MODE_POINT;
            }
            unreachable();
        }

        constexpr VkCompareOp to_vulkan(rhi::compare_op op)
        {
            switch (op)
            {
            case rhi::compare_op::never:
                return VK_COMPARE_OP_NEVER;
            case rhi::compare_op::less:
                return VK_COMPARE_OP_LESS;
            case rhi::compare_op::equal:
                return VK_COMPARE_OP_EQUAL;
            case rhi::compare_op::less_equal:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
            case rhi::compare_op::greater:
                return VK_COMPARE_OP_GREATER;
            case rhi::compare_op::not_equal:
                return VK_COMPARE_OP_NOT_EQUAL;
            case rhi::compare_op::greater_equal:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case rhi::compare_op::always:
                return VK_COMPARE_OP_ALWAYS;
            }
            unreachable();
        }

        constexpr VkStencilOp to_vulkan(rhi::stencil_op op)
        {
            switch (op)
            {
            case rhi::stencil_op::keep:
                return VK_STENCIL_OP_KEEP;
            case rhi::stencil_op::zero:
                return VK_STENCIL_OP_ZERO;
            case rhi::stencil_op::replace:
                return VK_STENCIL_OP_REPLACE;
            case rhi::stencil_op::increment_and_clamp:
                return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
            case rhi::stencil_op::decrement_and_clamp:
                return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
            case rhi::stencil_op::invert:
                return VK_STENCIL_OP_INVERT;
            case rhi::stencil_op::increment_and_wrap:
                return VK_STENCIL_OP_INCREMENT_AND_WRAP;
            case rhi::stencil_op::decrement_and_wrap:
                return VK_STENCIL_OP_DECREMENT_AND_WRAP;
            }
            unreachable();
        }

        constexpr VkBlendFactor to_vulkan(rhi::blend_factor factor)
        {
            switch (factor)
            {
            case rhi::blend_factor::zero:
                return VK_BLEND_FACTOR_ZERO;
            case rhi::blend_factor::one:
                return VK_BLEND_FACTOR_ONE;
            case rhi::blend_factor::src_color:
                return VK_BLEND_FACTOR_SRC_COLOR;
            case rhi::blend_factor::one_minus_src_color:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case rhi::blend_factor::dst_color:
                return VK_BLEND_FACTOR_DST_COLOR;
            case rhi::blend_factor::one_minus_dst_color:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case rhi::blend_factor::src_alpha:
                return VK_BLEND_FACTOR_SRC_ALPHA;
            case rhi::blend_factor::one_minus_src_alpha:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case rhi::blend_factor::dst_alpha:
                return VK_BLEND_FACTOR_DST_ALPHA;
            case rhi::blend_factor::one_minus_dst_alpha:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case rhi::blend_factor::constant_color:
                return VK_BLEND_FACTOR_CONSTANT_COLOR;
            case rhi::blend_factor::one_minus_constant_color:
                return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
            case rhi::blend_factor::constant_alpha:
                return VK_BLEND_FACTOR_CONSTANT_ALPHA;
            case rhi::blend_factor::one_minus_constant_alpha:
                return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
            }
            unreachable();
        }

        constexpr VkBlendOp to_vulkan(rhi::blend_op op)
        {
            switch (op)
            {
            case rhi::blend_op::add:
                return VK_BLEND_OP_ADD;
            case rhi::blend_op::subtract:
                return VK_BLEND_OP_SUBTRACT;
            case rhi::blend_op::reverse_subtract:
                return VK_BLEND_OP_REVERSE_SUBTRACT;
            case rhi::blend_op::min:
                return VK_BLEND_OP_MIN;
            case rhi::blend_op::max:
                return VK_BLEND_OP_MAX;
            }
            unreachable();
        }

        constexpr VkIndexType to_vulkan(rhi::index_format fmt)
        {
            switch (fmt)
            {
            case rhi::index_format::uint8:
                return VK_INDEX_TYPE_UINT8_EXT;
            case rhi::index_format::uint16:
                return VK_INDEX_TYPE_UINT16;
            case rhi::index_format::uint32:
                return VK_INDEX_TYPE_UINT32;
            }
            unreachable();
        }

        constexpr VkFilter to_vulkan(rhi::filter type)
        {
            switch (type)
            {
            case rhi::filter::nearest:
                return VK_FILTER_NEAREST;
            case rhi::filter::linear:
                return VK_FILTER_LINEAR;
            }
            unreachable();
        }

        constexpr VkSamplerMipmapMode to_vulkan(rhi::mipmap_mode mode)
        {
            switch (mode)
            {
            case rhi::mipmap_mode::nearest:
                return VK_SAMPLER_MIPMAP_MODE_NEAREST;
            case rhi::mipmap_mode::linear:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
            }
            unreachable();
        }

        constexpr VkSamplerAddressMode to_vulkan(rhi::address_mode mode)
        {
            switch (mode)
            {
            case rhi::address_mode::repeat:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case rhi::address_mode::mirrored_repeat:
                return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case rhi::address_mode::clamp_to_edge:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case rhi::address_mode::clamp_to_border:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case rhi::address_mode::mirror_clamp_to_edge:
                return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
            }
            unreachable();
        }

        constexpr VkPipelineBindPoint to_vulkan(rhi::bind_point type)
        {
            switch (type)
            {
            case rhi::bind_point::graphics:
                return VK_PIPELINE_BIND_POINT_GRAPHICS;
            case rhi::bind_point::compute:
                return VK_PIPELINE_BIND_POINT_COMPUTE;
            }
            unreachable();
        }

        constexpr VkDescriptorSetLayoutCreateFlags to_vulkan(enum_mask<descriptor_set_layout_flags> flags)
        {
            VkDescriptorSetLayoutCreateFlags vk_flags = 0;

            if ((flags & descriptor_set_layout_flags::push) == descriptor_set_layout_flags::push)
            {
                vk_flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
            }

            if ((flags & descriptor_set_layout_flags::descriptor_buffer) ==
                descriptor_set_layout_flags::descriptor_buffer)
            {
                vk_flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
            }

            return vk_flags;
        }

        constexpr VkImageViewType get_compatible_view_type(rhi::image_type type)
        {
            switch (type)
            {
            case rhi::image_type::image_1d:
                return VK_IMAGE_VIEW_TYPE_1D;
            case rhi::image_type::image_2d:
                return VK_IMAGE_VIEW_TYPE_2D;
            case rhi::image_type::image_3d:
                return VK_IMAGE_VIEW_TYPE_3D;
            case rhi::image_type::image_cube:
                return VK_IMAGE_VIEW_TYPE_CUBE;
            case rhi::image_type::image_1d_array:
                return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            case rhi::image_type::image_2d_array:
                return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            case rhi::image_type::image_cube_array:
                return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            }

            unreachable();
        }

        constexpr VkImageAspectFlags compute_aspect_flags(image_format fmt)
        {
            switch (fmt)
            {
            case rhi::image_format::d16_unorm:
            case rhi::image_format::d24_unorm:
            case rhi::image_format::d32_float:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case rhi::image_format::d16_unorm_s8_uint:
            case rhi::image_format::d24_unorm_s8_uint:
            case rhi::image_format::d32_float_s8_uint:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            case rhi::image_format::s8_uint:
                return VK_IMAGE_ASPECT_STENCIL_BIT;
            case rhi::image_format::r8_unorm:
            case rhi::image_format::r8_snorm:
            case rhi::image_format::r16_unorm:
            case rhi::image_format::r16_snorm:
            case rhi::image_format::r16_float:
            case rhi::image_format::r32_float:
            case rhi::image_format::rg8_unorm:
            case rhi::image_format::rg8_snorm:
            case rhi::image_format::rg16_unorm:
            case rhi::image_format::rg16_snorm:
            case rhi::image_format::rg16_float:
            case rhi::image_format::rg32_float:
            case rhi::image_format::rgba8_unorm:
            case rhi::image_format::rgba8_snorm:
            case rhi::image_format::rgba8_srgb:
            case rhi::image_format::bgra8_srgb:
            case rhi::image_format::rgba16_unorm:
            case rhi::image_format::rgba16_snorm:
            case rhi::image_format::rgba16_float:
            case rhi::image_format::rgba32_float:
            case rhi::image_format::a2bgr10_unorm_pack32:
                return VK_IMAGE_ASPECT_COLOR_BIT;
            }

            logger->critical("Invalid image format: {}", to_underlying(fmt));
            std::terminate();
        }

        constexpr VmaMemoryUsage to_vma(rhi::memory_location location)
        {
            switch (location)
            {
            case rhi::memory_location::device:
                return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            case rhi::memory_location::host:
                return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            case rhi::memory_location::automatic:
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

    void delete_queue::enqueue(VkObjectType type, void* handle, VkDescriptorPool desc_pool, uint64_t frame)
    {
        dq.push({
            .last_used_frame = frame,
            .type = type,
            .handle = handle,
            .desc_pool = desc_pool,
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
        case VK_OBJECT_TYPE_DESCRIPTOR_SET:
            dispatch->freeDescriptorSets(res.desc_pool, 1, reinterpret_cast<VkDescriptorSet*>(&res.handle));
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
        case VK_OBJECT_TYPE_PIPELINE:
            dispatch->destroyPipeline(static_cast<VkPipeline>(res.handle), nullptr);
            break;
        case VK_OBJECT_TYPE_SAMPLER:
            dispatch->destroySampler(static_cast<VkSampler>(res.handle), nullptr);
            break;
        case VK_OBJECT_TYPE_SEMAPHORE:
            dispatch->destroySemaphore(static_cast<VkSemaphore>(res.handle), nullptr);
            break;
        case VK_OBJECT_TYPE_SHADER_MODULE:
            dispatch->destroyShaderModule(static_cast<VkShaderModule>(res.handle), nullptr);
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

    void delete_queue::destroy()
    {
        dispatch->deviceWaitIdle();
        while (!dq.empty())
        {
            release_resource(dq.front());
            dq.pop();
        }
    }

    device::device(vkb::Device dev, vkb::Instance* instance)
        : _vkb_instance{instance}, _vkb_device{tempest::move(dev)}, _dispatch_table{dev.make_table()},
          _instance_dispatch_table{_vkb_instance->make_table()}, _resource_tracker{this, _dispatch_table},
          _descriptor_set_layout_cache{this}, _pipeline_layout_cache{this}
#if TEMPEST_ENABLE_AFTERMATH
          ,
          _crash_tracker{_marker_map}
#endif
    {
#if TEMPEST_ENABLE_AFTERMATH
        _crash_tracker.initialize();
#endif

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

        {
            const auto result = vmaCreateAllocator(&ci, &_vma_allocator);

            if (result != VK_SUCCESS)
            {
                logger->critical("Failed to create VMA allocator: {}", to_underlying(result));
                std::terminate();
            }
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
            _primary_work_queue.emplace(this, &_dispatch_table, queue, get<1>(*default_queue_match), frames_in_flight(),
                                        &_resource_tracker);
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
                                             frames_in_flight(), &_resource_tracker);
        }

        if (transfer_queue_match && get<1>(*transfer_queue_match) != get<1>(*default_queue_match))
        {
            VkQueue queue;
            _dispatch_table.getDeviceQueue(get<1>(*transfer_queue_match), get<2>(*transfer_queue_match), &queue);
            _dedicated_transfer_queue.emplace(this, &_dispatch_table, queue, get<1>(*transfer_queue_match),
                                              frames_in_flight(), &_resource_tracker);
        }

        // Set up the descriptor pool
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 2048},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024 * 1024},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 512},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1024},
        };

        VkDescriptorPoolCreateInfo pool_ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 1024 * 1024,
            .poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes)),
            .pPoolSizes = pool_sizes,
        };

        {
            VkDescriptorPool descriptor_pool;
            auto result = _dispatch_table.createDescriptorPool(&pool_ci, nullptr, &descriptor_pool);
            if (result != VK_SUCCESS)
            {
                logger->critical("Failed to create descriptor pool: {}", to_underlying(result));
                std::terminate();
            }

            _desc_pool = descriptor_pool;
        }

        _is_debug_device = _vkb_instance->debug_messenger != nullptr;
        if (_is_debug_device)
        {
            logger->info("Device in debug mode. Using {}", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

#if defined(TEMPEST_DEBUG_SHADERS)
        _can_name = true;
#else
        _can_name = false;
#endif
        if (_can_name)
        {
            logger->info("Debug names enabled.");
        }

        auto properties = VkPhysicalDeviceProperties2{};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

        properties.pNext = &_descriptor_buffer_properties;
        _descriptor_buffer_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;

        _instance_dispatch_table.getPhysicalDeviceProperties2(_vkb_device.physical_device.physical_device, &properties);
    }

    device::~device()
    {
        _dispatch_table.deviceWaitIdle();

        _primary_work_queue = nullopt;
        _dedicated_compute_queue = nullopt;
        _dedicated_transfer_queue = nullopt;

        _delete_queue.destroy();

        _dispatch_table.destroyDescriptorPool(_desc_pool, nullptr);

        _resource_tracker.destroy();
        _descriptor_set_layout_cache.destroy();
        _pipeline_layout_cache.destroy();

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
        case rhi::host_access_pattern::random:
            allocation_ci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case rhi::host_access_pattern::sequential:
            allocation_ci.flags |=
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        default:
            break;
        }

        switch (desc.access_type)
        {
        case rhi::host_access_type::coherent:
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
            if (desc.name.empty())
            {
                logger->error("Failed to create buffer: {}", to_underlying(result));
            }
            else
            {
                logger->error("Failed to create buffer '{}': {}", desc.name.c_str(), to_underlying(result));
            }
            return typed_rhi_handle<rhi_handle_type::buffer>::null_handle;
        }

        auto buf_dev_address = VkBufferDeviceAddressInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,
            .buffer = buffer,
        };

        auto address = _dispatch_table.getBufferDeviceAddress(&buf_dev_address);

        auto buf = vk::buffer{
            .allocation = allocation,
            .allocation_info = allocation_info,
            .buffer = buffer,
            .address = address,
            .usage = buffer_ci.usage,
        };

        if (!desc.name.empty())
        {
            name_object(VK_OBJECT_TYPE_BUFFER, buf.buffer, desc.name.c_str());
        }

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

        if (desc.usage & rhi::image_usage::color_attachment || desc.usage & rhi::image_usage::depth_attachment ||
            desc.usage & rhi::image_usage::stencil_attachment)
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

        vk::image img = {
            .allocation = allocation,
            .allocation_info = allocation_info,
            .image = image,
            .image_view = image_view,
            .swapchain_image = false,
            .image_aspect = view_ci.subresourceRange.aspectMask,
            .create_info = ci,
            .view_create_info = view_ci,
            .name = desc.name,
        };

        if (!desc.name.empty())
        {
            name_object(VK_OBJECT_TYPE_IMAGE, img.image, desc.name.c_str());
            auto view_name = std::format("{} View", desc.name.c_str());
            name_object(VK_OBJECT_TYPE_IMAGE_VIEW, img.image_view, view_name.c_str());
        }

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

    typed_rhi_handle<rhi_handle_type::descriptor_set_layout> device::create_descriptor_set_layout(
        const vector<descriptor_binding_layout>& desc, enum_mask<descriptor_set_layout_flags> flags) noexcept
    {
        return _descriptor_set_layout_cache.get_or_create_layout(desc, flags);
    }

    typed_rhi_handle<rhi_handle_type::pipeline_layout> device::create_pipeline_layout(
        const pipeline_layout_desc& desc) noexcept
    {
        return _pipeline_layout_cache.get_or_create_layout(desc);
    }

    typed_rhi_handle<rhi_handle_type::graphics_pipeline> device::create_graphics_pipeline(
        const graphics_pipeline_desc& desc) noexcept
    {
        auto pipeline_layout = _pipeline_layout_cache.get_layout(desc.layout);
        if (!pipeline_layout)
        {
            logger->error("Failed to create graphics pipeline: invalid pipeline layout");
            return typed_rhi_handle<rhi_handle_type::graphics_pipeline>::null_handle;
        }

        assert(desc.color_attachment_formats.size() == desc.color_blend.attachments.size());

        // Lambda to create shader modules and stages
        // Returns a pair of shader modules and shader stages

        auto create_shader_stage =
            [&](const vector<byte>& shader_code,
                VkShaderStageFlagBits stage) -> pair<VkShaderModule, VkPipelineShaderStageCreateInfo> {
            VkShaderModuleCreateInfo shader_ci = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .codeSize = shader_code.size(),
                .pCode = reinterpret_cast<const uint32_t*>(shader_code.data()),
            };
            VkShaderModule shader_module;
            auto result = _dispatch_table.createShaderModule(&shader_ci, nullptr, &shader_module);
            if (result != VK_SUCCESS)
            {
                logger->error("Failed to create shader module: {}", to_underlying(result));
                return make_pair(VkShaderModule{}, VkPipelineShaderStageCreateInfo{});
            }
            VkPipelineShaderStageCreateInfo stage_ci = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = stage,
                .module = shader_module,
                .pName = "main",
                .pSpecializationInfo = nullptr,
            };
            return make_pair(shader_module, stage_ci);
        };

        inplace_vector<VkShaderModule, 5> shader_modules;
        inplace_vector<VkPipelineShaderStageCreateInfo, 5> shader_stages;
        if (!desc.vertex_shader.empty())
        {
            auto [mod, stage_ci] = create_shader_stage(desc.vertex_shader, VK_SHADER_STAGE_VERTEX_BIT);
            if (mod == VkShaderModule{})
            {
                return typed_rhi_handle<rhi_handle_type::graphics_pipeline>::null_handle;
            }

            shader_modules.push_back(mod);
            shader_stages.push_back(stage_ci);
        }

        if (!desc.tessellation_control_shader.empty())
        {
            auto [mod, stage_ci] =
                create_shader_stage(desc.tessellation_control_shader, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
            if (mod == VkShaderModule{})
            {
                return typed_rhi_handle<rhi_handle_type::graphics_pipeline>::null_handle;
            }
            shader_modules.push_back(mod);
            shader_stages.push_back(stage_ci);
        }

        if (!desc.tessellation_evaluation_shader.empty())
        {
            auto [mod, stage_ci] =
                create_shader_stage(desc.tessellation_evaluation_shader, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
            if (mod == VkShaderModule{})
            {
                return typed_rhi_handle<rhi_handle_type::graphics_pipeline>::null_handle;
            }
            shader_modules.push_back(mod);
            shader_stages.push_back(stage_ci);
        }

        if (!desc.geometry_shader.empty())
        {
            auto [mod, stage_ci] = create_shader_stage(desc.geometry_shader, VK_SHADER_STAGE_GEOMETRY_BIT);
            if (mod == VkShaderModule{})
            {
                return typed_rhi_handle<rhi_handle_type::graphics_pipeline>::null_handle;
            }
            shader_modules.push_back(mod);
            shader_stages.push_back(stage_ci);
        }

        if (!desc.fragment_shader.empty())
        {
            auto [mod, stage_ci] = create_shader_stage(desc.fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT);
            if (mod == VkShaderModule{})
            {
                return typed_rhi_handle<rhi_handle_type::graphics_pipeline>::null_handle;
            }
            shader_modules.push_back(mod);
            shader_stages.push_back(stage_ci);
        }

        auto vertex_bindings = vector<VkVertexInputBindingDescription>();
        auto vertex_attributes = vector<VkVertexInputAttributeDescription>();

        if (desc.vertex_input)
        {
            for (const auto& binding : desc.vertex_input->bindings)
            {
                VkVertexInputBindingDescription vk_binding = {
                    .binding = binding.binding_index,
                    .stride = binding.stride,
                    .inputRate = to_vulkan(binding.input_rate),
                };

                vertex_bindings.push_back(tempest::move(vk_binding));
            }

            for (const auto& attr : desc.vertex_input->attributes)
            {
                VkVertexInputAttributeDescription vk_attr = {
                    .location = attr.location_index,
                    .binding = attr.binding_index,
                    .format = to_vulkan(attr.format),
                    .offset = attr.offset,
                };

                vertex_attributes.push_back(tempest::move(vk_attr));
            }
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_bindings.size()),
            .pVertexBindingDescriptions = vertex_bindings.empty() ? nullptr : vertex_bindings.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes.size()),
            .pVertexAttributeDescriptions = vertex_attributes.empty() ? nullptr : vertex_attributes.data(),
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = to_vulkan(desc.input_assembly.topology),
            .primitiveRestartEnable = VK_FALSE,
        };

        VkPipelineTessellationStateCreateInfo tessellation_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .patchControlPoints = desc.tessellation ? desc.tessellation->patch_control_points : 0,
        };

        VkPipelineRasterizationStateCreateInfo rasterization_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthClampEnable = desc.rasterization.depth_clamp_enable ? VK_TRUE : VK_FALSE,
            .rasterizerDiscardEnable = desc.rasterization.rasterizer_discard_enable ? VK_TRUE : VK_FALSE,
            .polygonMode = to_vulkan(desc.rasterization.polygon_mode),
            .cullMode = to_vulkan(desc.rasterization.cull_mode),
            .frontFace = to_vulkan(desc.rasterization.vertex_winding),
            .depthBiasEnable = desc.rasterization.depth_bias ? VK_TRUE : VK_FALSE,
            .depthBiasConstantFactor =
                desc.rasterization.depth_bias ? desc.rasterization.depth_bias->constant_factor : 0.0f,
            .depthBiasClamp = desc.rasterization.depth_bias ? desc.rasterization.depth_bias->clamp : 0.0f,
            .depthBiasSlopeFactor = desc.rasterization.depth_bias ? desc.rasterization.depth_bias->slope_factor : 0.0f,
            .lineWidth = desc.rasterization.line_width,
        };

        VkPipelineMultisampleStateCreateInfo multisample_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = to_vulkan(desc.multisample.sample_count),
            .sampleShadingEnable = desc.multisample.sample_shading ? VK_TRUE : VK_FALSE,
            .minSampleShading =
                desc.multisample.sample_shading ? desc.multisample.sample_shading->min_sample_shading : 0.0f,
            .pSampleMask =
                desc.multisample.sample_shading ? desc.multisample.sample_shading->sample_mask.data() : nullptr,
            .alphaToCoverageEnable = desc.multisample.alpha_to_coverage ? VK_TRUE : VK_FALSE,
            .alphaToOneEnable = desc.multisample.alpha_to_one ? VK_TRUE : VK_FALSE,
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthTestEnable = desc.depth_stencil.depth ? VK_TRUE : VK_FALSE,
            .depthWriteEnable =
                desc.depth_stencil.depth ? desc.depth_stencil.depth->write_enable ? VK_TRUE : VK_FALSE : VK_FALSE,
            .depthCompareOp =
                desc.depth_stencil.depth ? to_vulkan(desc.depth_stencil.depth->compare_op) : VK_COMPARE_OP_ALWAYS,
            .depthBoundsTestEnable = desc.depth_stencil.depth
                                         ? desc.depth_stencil.depth->depth_bounds_test_enable ? VK_TRUE : VK_FALSE
                                         : VK_FALSE,
            .stencilTestEnable = desc.depth_stencil.stencil ? VK_TRUE : VK_FALSE,
            .front =
                {
                    .failOp = desc.depth_stencil.stencil ? to_vulkan(desc.depth_stencil.stencil->front.fail_op)
                                                         : VK_STENCIL_OP_KEEP,
                    .passOp = desc.depth_stencil.stencil ? to_vulkan(desc.depth_stencil.stencil->front.pass_op)
                                                         : VK_STENCIL_OP_KEEP,
                    .depthFailOp = desc.depth_stencil.stencil
                                       ? to_vulkan(desc.depth_stencil.stencil->front.depth_fail_op)
                                       : VK_STENCIL_OP_KEEP,
                    .compareOp = desc.depth_stencil.stencil ? to_vulkan(desc.depth_stencil.stencil->front.compare_op)
                                                            : VK_COMPARE_OP_ALWAYS,
                    .compareMask = desc.depth_stencil.stencil ? desc.depth_stencil.stencil->front.compare_mask : 0,
                    .writeMask = desc.depth_stencil.stencil ? desc.depth_stencil.stencil->front.write_mask : 0,
                    .reference = desc.depth_stencil.stencil ? desc.depth_stencil.stencil->front.reference : 0,
                },
            .back =
                {
                    .failOp = desc.depth_stencil.stencil ? to_vulkan(desc.depth_stencil.stencil->back.fail_op)
                                                         : VK_STENCIL_OP_KEEP,
                    .passOp = desc.depth_stencil.stencil ? to_vulkan(desc.depth_stencil.stencil->back.pass_op)
                                                         : VK_STENCIL_OP_KEEP,
                    .depthFailOp = desc.depth_stencil.stencil
                                       ? to_vulkan(desc.depth_stencil.stencil->back.depth_fail_op)
                                       : VK_STENCIL_OP_KEEP,
                    .compareOp = desc.depth_stencil.stencil ? to_vulkan(desc.depth_stencil.stencil->back.compare_op)
                                                            : VK_COMPARE_OP_ALWAYS,
                    .compareMask = desc.depth_stencil.stencil ? desc.depth_stencil.stencil->back.compare_mask : 0,
                    .writeMask = desc.depth_stencil.stencil ? desc.depth_stencil.stencil->back.write_mask : 0,
                    .reference = desc.depth_stencil.stencil ? desc.depth_stencil.stencil->back.reference : 0,
                },
            .minDepthBounds = desc.depth_stencil.depth ? desc.depth_stencil.depth->min_depth_bounds : 0.0f,
            .maxDepthBounds = desc.depth_stencil.depth ? desc.depth_stencil.depth->max_depth_bounds : 0.0f,
        };

        auto color_blend_attachments = vector<VkPipelineColorBlendAttachmentState>(desc.color_blend.attachments.size());
        for (size_t i = 0; i < desc.color_blend.attachments.size(); ++i)
        {
            color_blend_attachments[i] = {
                .blendEnable = desc.color_blend.attachments[i].blend_enable ? VK_TRUE : VK_FALSE,
                .srcColorBlendFactor = to_vulkan(desc.color_blend.attachments[i].src_color_blend_factor),
                .dstColorBlendFactor = to_vulkan(desc.color_blend.attachments[i].dst_color_blend_factor),
                .colorBlendOp = to_vulkan(desc.color_blend.attachments[i].color_blend_op),
                .srcAlphaBlendFactor = to_vulkan(desc.color_blend.attachments[i].src_alpha_blend_factor),
                .dstAlphaBlendFactor = to_vulkan(desc.color_blend.attachments[i].dst_alpha_blend_factor),
                .alphaBlendOp = to_vulkan(desc.color_blend.attachments[i].alpha_blend_op),
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                  VK_COLOR_COMPONENT_A_BIT,
            };
        }

        VkPipelineColorBlendStateCreateInfo color_blend_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_NO_OP,
            .attachmentCount = static_cast<uint32_t>(color_blend_attachments.size()),
            .pAttachments = color_blend_attachments.empty() ? nullptr : color_blend_attachments.data(),
            .blendConstants =
                {
                    desc.color_blend.blend_constants[0],
                    desc.color_blend.blend_constants[1],
                    desc.color_blend.blend_constants[2],
                    desc.color_blend.blend_constants[3],
                },
        };

        array dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_CULL_MODE,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data(),
        };

        auto color_attachments = tempest::vector<VkFormat>(desc.color_attachment_formats.size());

        for (size_t i = 0; i < desc.color_attachment_formats.size(); ++i)
        {
            color_attachments[i] = to_vulkan(desc.color_attachment_formats[i]);
        }

        VkPipelineRenderingCreateInfo pipeline_rendering_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext = nullptr,
            .viewMask = 0,
            .colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
            .pColorAttachmentFormats = color_attachments.empty() ? nullptr : color_attachments.data(),
            .depthAttachmentFormat =
                desc.depth_attachment_format ? to_vulkan(*desc.depth_attachment_format) : VK_FORMAT_UNDEFINED,
            .stencilAttachmentFormat =
                desc.stencil_attachment_format ? to_vulkan(*desc.stencil_attachment_format) : VK_FORMAT_UNDEFINED,
        };

        VkViewport default_vp = {
            .x = 0,
            .y = 0,
            .width = 1.0f,
            .height = 1.0f,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D default_scissor = {
            .offset = {0, 0},
            .extent = {1, 1},
        };

        VkPipelineViewportStateCreateInfo viewport_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = &default_vp,
            .scissorCount = 1,
            .pScissors = &default_scissor,
        };

        VkGraphicsPipelineCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &pipeline_rendering_ci,
            .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
            .stageCount = static_cast<uint32_t>(shader_stages.size()),
            .pStages = shader_stages.data(),
            .pVertexInputState = &vertex_input_ci,
            .pInputAssemblyState = &input_assembly_ci,
            .pTessellationState = desc.tessellation ? &tessellation_ci : nullptr,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterization_ci,
            .pMultisampleState = &multisample_state,
            .pDepthStencilState = &depth_stencil_state,
            .pColorBlendState = &color_blend_ci,
            .pDynamicState = &dynamic_state,
            .layout = pipeline_layout,
            .renderPass = nullptr,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0,
        };

        VkPipeline pipeline;
        auto result = _dispatch_table.createGraphicsPipelines(VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create graphics pipeline: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::graphics_pipeline>::null_handle;
        }

        graphics_pipeline gp = {
            .shader_modules = tempest::move(shader_modules),
            .pipeline = pipeline,
            .layout = pipeline_layout,
            .desc = tempest::move(desc),
        };

        if (!desc.name.empty())
        {
            name_object(VK_OBJECT_TYPE_PIPELINE, gp.pipeline, desc.name.c_str());
        }

        auto new_key = _graphics_pipelines.insert(gp);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::graphics_pipeline>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    typed_rhi_handle<rhi_handle_type::descriptor_set> device::create_descriptor_set(
        const descriptor_set_desc& desc) noexcept
    {
        // Set 1: Allocating descriptor set
        auto desc_set_layout = _descriptor_set_layout_cache.get_layout(desc.layout);
        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = _desc_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &desc_set_layout,
        };

        VkDescriptorSet desc_set;
        auto result = _dispatch_table.allocateDescriptorSets(&alloc_info, &desc_set);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to allocate descriptor set: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::descriptor_set>::null_handle;
        }

        descriptor_set desc_result = {
            .set = desc_set,
            .pool = _desc_pool,
            .layout = desc_set_layout,
            .bound_buffers = {},
            .bound_images = {},
            .bound_samplers = {},
        };

        // Step 2: Write to the descriptor set
        auto buffer_write_count = desc.buffers.size();
        auto image_write_count = desc.images.size();
        auto sampler_write_count = desc.samplers.size();

        auto total_write_count = buffer_write_count + image_write_count + sampler_write_count;

        auto writes = _desc_pool_allocator.allocate_typed<VkWriteDescriptorSet>(total_write_count);

        uint32_t write_index = 0;

        for (const auto& buffer_desc : desc.buffers)
        {
            auto buf_info = _desc_pool_allocator.allocate_typed<VkDescriptorBufferInfo>(1);
            *buf_info = {
                .buffer = get_buffer(buffer_desc.buffer)->buffer,
                .offset = buffer_desc.offset,
                .range = buffer_desc.size,
            };

            writes[write_index++] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = desc_set,
                .dstBinding = buffer_desc.index,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = to_vulkan(buffer_desc.type),
                .pImageInfo = nullptr,
                .pBufferInfo = buf_info,
                .pTexelBufferView = nullptr,
            };

            // Add the buffer to the descriptor set's bound resources
            desc_result.bound_buffers.push_back(buffer_desc.buffer);
        }

        for (const auto& image_desc : desc.images)
        {
            auto img_infos = _desc_pool_allocator.allocate_typed<VkDescriptorImageInfo>(image_desc.images.size());
            for (size_t i = 0; i < image_desc.images.size(); ++i)
            {
                img_infos[i] = {
                    .sampler = image_desc.images[i].sampler ? get_sampler(image_desc.images[i].sampler)->sampler
                                                            : VK_NULL_HANDLE,
                    .imageView = get_image(image_desc.images[i].image)->image_view,
                    .imageLayout = to_vulkan(image_desc.images[i].layout),
                };

                // Add the image to the descriptor set's bound resources
                desc_result.bound_images.push_back(image_desc.images[i].image);
            }

            writes[write_index++] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = desc_set,
                .dstBinding = image_desc.index,
                .dstArrayElement = image_desc.array_offset,
                .descriptorCount = static_cast<uint32_t>(image_desc.images.size()),
                .descriptorType = to_vulkan(image_desc.type),
                .pImageInfo = img_infos,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr,
            };
        }

        for (const auto& sampler_desc : desc.samplers)
        {
            auto sampler_infos =
                _desc_pool_allocator.allocate_typed<VkDescriptorImageInfo>(sampler_desc.samplers.size());
            for (size_t i = 0; i < sampler_desc.samplers.size(); ++i)
            {
                const auto sampler = get_sampler(sampler_desc.samplers[i]);
                assert(sampler.has_value());

                sampler_infos[i] = {
                    .sampler = sampler->sampler,
                    .imageView = VK_NULL_HANDLE,
                    .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                };

                // Add the sampler to the descriptor set's bound resources
                desc_result.bound_samplers.push_back(sampler_desc.samplers[i]);
            }

            writes[write_index++] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = desc_set,
                .dstBinding = sampler_desc.index,
                .dstArrayElement = 0,
                .descriptorCount = static_cast<uint32_t>(sampler_desc.samplers.size()),
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .pImageInfo = sampler_infos,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr,
            };
        }

        _dispatch_table.updateDescriptorSets(static_cast<uint32_t>(total_write_count), writes, 0, nullptr);
        _desc_pool_allocator.reset();

        auto new_key = _descriptor_sets.insert(desc_result);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);
        return typed_rhi_handle<rhi_handle_type::descriptor_set>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    typed_rhi_handle<rhi_handle_type::compute_pipeline> device::create_compute_pipeline(
        const compute_pipeline_desc& desc) noexcept
    {
        VkShaderModuleCreateInfo shader_ci = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = desc.compute_shader.size(),
            .pCode = reinterpret_cast<const uint32_t*>(desc.compute_shader.data()),
        };

        VkShaderModule shader_module;
        auto result = _dispatch_table.createShaderModule(&shader_ci, nullptr, &shader_module);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create shader module: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::compute_pipeline>::null_handle;
        }

        VkPipelineShaderStageCreateInfo stage_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader_module,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        };

        VkComputePipelineCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
            .stage = stage_ci,
            .layout = _pipeline_layout_cache.get_layout(desc.layout),
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0,
        };

        VkPipeline pipeline;
        result = _dispatch_table.createComputePipelines(VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create compute pipeline: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::compute_pipeline>::null_handle;
        }

        compute_pipeline cp = {
            .shader_module = shader_module,
            .pipeline = pipeline,
            .layout = _pipeline_layout_cache.get_layout(desc.layout),
            .desc = desc,
        };

        if (!desc.name.empty())
        {
            name_object(VK_OBJECT_TYPE_PIPELINE, cp.pipeline, desc.name.c_str());
        }

        auto new_key = _compute_pipelines.insert(cp);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::compute_pipeline>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    typed_rhi_handle<rhi_handle_type::sampler> device::create_sampler(const sampler_desc& desc) noexcept
    {
        const auto sampler_ci = VkSamplerCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = to_vulkan(desc.mag),
            .minFilter = to_vulkan(desc.min),
            .mipmapMode = to_vulkan(desc.mipmap),
            .addressModeU = to_vulkan(desc.address_u),
            .addressModeV = to_vulkan(desc.address_v),
            .addressModeW = to_vulkan(desc.address_w),
            .mipLodBias = desc.mip_lod_bias,
            .anisotropyEnable = desc.max_anisotropy ? VK_TRUE : VK_FALSE,
            .maxAnisotropy = desc.max_anisotropy.value_or(0.0f),
            .compareEnable = desc.compare ? VK_TRUE : VK_FALSE,
            .compareOp = to_vulkan(desc.compare.value_or(compare_op::never)),
            .minLod = desc.min_lod,
            .maxLod = desc.max_lod,
            .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };

        VkSampler sampler;
        auto result = _dispatch_table.createSampler(&sampler_ci, nullptr, &sampler);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create sampler: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::sampler>::null_handle;
        }

        vk::sampler new_sampler = {
            .sampler = sampler,
            .create_info = sampler_ci,
        };

        if (!desc.name.empty())
        {
            name_object(VK_OBJECT_TYPE_SAMPLER, new_sampler.sampler, desc.name.c_str());
        }

        const auto new_key = _samplers.insert(new_sampler);
        const auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        const auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::sampler>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    void device::destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept
    {
        // If the buffer is tracked, handle it through the resource tracker
        if (_resource_tracker.is_tracked(handle))
        {
            _resource_tracker.request_release(handle);
        }
        else
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
    }

    void device::destroy_image(typed_rhi_handle<rhi_handle_type::image> handle) noexcept
    {
        // If the image is tracked, handle it through the resource tracker
        if (_resource_tracker.is_tracked(handle))
        {
            _resource_tracker.request_release(handle);
        }
        else
        {
            auto img_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
            auto img_it = _images.find(img_key);
            if (img_it != _images.end())
            {
                if (img_it->image_view)
                {
                    _dispatch_table.destroyImageView(img_it->image_view, nullptr);
                }
                if (img_it->image && !img_it->swapchain_image)
                {
                    _delete_queue.enqueue(VK_OBJECT_TYPE_IMAGE, img_it->image, img_it->allocation,
                                          _current_frame + num_frames_in_flight);
                }
                _images.erase(img_key);
            }
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

    void device::destroy_descriptor_set_layout(typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept
    {
        _descriptor_set_layout_cache.release_layout(handle);
    }

    void device::destroy_pipeline_layout(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept
    {
        _pipeline_layout_cache.release_layout(handle);
    }

    void device::destroy_graphics_pipeline(typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle) noexcept
    {
        if (_resource_tracker.is_tracked(handle))
        {
            _resource_tracker.request_release(handle);
        }
        else
        {
            auto pipeline_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
            auto pipeline_it = this->_graphics_pipelines.find(pipeline_key);
            if (pipeline_it != this->_graphics_pipelines.end())
            {
                for (auto shader_module : pipeline_it->shader_modules)
                {
                    _delete_queue.enqueue(VK_OBJECT_TYPE_SHADER_MODULE, shader_module,
                                          _current_frame + num_frames_in_flight);
                }

                _pipeline_layout_cache.release_layout(pipeline_it->desc.layout);

                _delete_queue.enqueue(VK_OBJECT_TYPE_PIPELINE, pipeline_it->pipeline,
                                      _current_frame + num_frames_in_flight);
                this->_graphics_pipelines.erase(pipeline_key);
            }
        }
    }

    void device::destroy_descriptor_set(typed_rhi_handle<rhi_handle_type::descriptor_set> handle) noexcept
    {
        if (_resource_tracker.is_tracked(handle))
        {
            _resource_tracker.request_release(handle);
        }
        else
        {
            auto desc_set_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
            auto desc_set_it = _descriptor_sets.find(desc_set_key);
            if (desc_set_it != _descriptor_sets.end())
            {
                _delete_queue.enqueue(VK_OBJECT_TYPE_DESCRIPTOR_SET, desc_set_it->set, desc_set_it->pool,
                                      _current_frame + num_frames_in_flight);
                _descriptor_sets.erase(desc_set_key);
            }
        }
    }

    void device::destroy_compute_pipeline(typed_rhi_handle<rhi_handle_type::compute_pipeline> handle) noexcept
    {
        if (_resource_tracker.is_tracked(handle))
        {
            _resource_tracker.request_release(handle);
        }
        else
        {
            auto pipeline_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
            auto pipeline_it = this->_compute_pipelines.find(pipeline_key);
            if (pipeline_it != this->_compute_pipelines.end())
            {
                _delete_queue.enqueue(VK_OBJECT_TYPE_SHADER_MODULE, pipeline_it->shader_module,
                                      _current_frame + num_frames_in_flight);
                _pipeline_layout_cache.release_layout(pipeline_it->desc.layout);
                _delete_queue.enqueue(VK_OBJECT_TYPE_PIPELINE, pipeline_it->pipeline,
                                      _current_frame + num_frames_in_flight);
                this->_compute_pipelines.erase(pipeline_key);
            }
        }
    }

    void device::destroy_sampler(typed_rhi_handle<rhi_handle_type::sampler> handle) noexcept
    {
        if (_resource_tracker.is_tracked(handle))
        {
            _resource_tracker.request_release(handle);
        }
        else
        {
            auto sampler_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
            auto sampler_it = _samplers.find(sampler_key);
            if (sampler_it != _samplers.end())
            {
                _delete_queue.enqueue(VK_OBJECT_TYPE_SAMPLER, sampler_it->sampler,
                                      _current_frame + num_frames_in_flight);
                _samplers.erase(sampler_key);
            }
        }
    }

    void device::recreate_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle,
                                         const render_surface_desc& desc) noexcept
    {
        _dispatch_table.deviceWaitIdle();

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
            _resource_tracker.untrack(img_handle, &*_primary_work_queue);
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
            return unexpected{swapchain_error_code::invalid_swapchain_argument};
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
            auto render_complete = swapchain_it->render_complete[image_index];
            auto& fif = swapchain_it->frames[_current_frame % frames_in_flight()];

            return swapchain_image_acquire_info_result{
                .acquire_sem = fif.image_acquired,
                .render_complete_sem = render_complete,
                .image = image,
                .image_index = image_index,
            };
        }
        case VK_ERROR_OUT_OF_DATE_KHR:
            return unexpected{swapchain_error_code::out_of_date};
        default:
            logger->error("Failed to acquire next image: {}", to_underlying(result));
            break;
        }

        return unexpected{swapchain_error_code::failure};
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

    byte* device::map_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept
    {
        auto buf_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto buf_it = _buffers.find(buf_key);
        if (buf_it != _buffers.end())
        {
            // Check if the buffer is already mapped
            if (buf_it->allocation_info.pMappedData)
            {
                return reinterpret_cast<byte*>(buf_it->allocation_info.pMappedData);
            }

            void* mapped_data;
            auto result = vmaMapMemory(_vma_allocator, buf_it->allocation, &mapped_data);
            if (result != VK_SUCCESS)
            {
                logger->error("Failed to map buffer: {}", to_underlying(result));
                return nullptr;
            }
            return reinterpret_cast<byte*>(mapped_data);
        }
        return nullptr;
    }

    void device::unmap_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept
    {
        auto buf_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto buf_it = _buffers.find(buf_key);
        if (buf_it != _buffers.end())
        {
            // If persistent mapping is enabled, no op
            if (buf_it->allocation_info.pMappedData)
            {
                return;
            }

            vmaUnmapMemory(_vma_allocator, buf_it->allocation);
        }
    }

    void device::flush_buffers(span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers) noexcept
    {
        vector<VmaAllocation> allocations;
        for (auto buf : buffers)
        {
            auto buf_key = create_slot_map_key<uint64_t>(buf.id, buf.generation);
            auto buf_it = _buffers.find(buf_key);
            if (buf_it != _buffers.end())
            {
                allocations.push_back(buf_it->allocation);
            }
        }

        if (!allocations.empty())
        {
            vmaFlushAllocations(_vma_allocator, static_cast<uint32_t>(allocations.size()), allocations.data(), nullptr,
                                nullptr);
        }
    }

    size_t device::get_buffer_size(typed_rhi_handle<rhi_handle_type::buffer> handle) const noexcept
    {
        auto buf = get_buffer(handle);
        return buf->allocation_info.size;
    }

    uint32_t device::get_render_surface_width(typed_rhi_handle<rhi_handle_type::render_surface> handle) const noexcept
    {
        auto swapchain_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto swapchain_it = _swapchains.find(swapchain_key);
        if (swapchain_it != _swapchains.end())
        {
            return swapchain_it->swapchain.extent.width;
        }
        return 0;
    }

    uint32_t device::get_render_surface_height(typed_rhi_handle<rhi_handle_type::render_surface> handle) const noexcept
    {
        auto swapchain_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto swapchain_it = _swapchains.find(swapchain_key);
        if (swapchain_it != _swapchains.end())
        {
            return swapchain_it->swapchain.extent.height;
        }
        return 0;
    }

    const rhi::window_surface* device::get_window_surface(
        typed_rhi_handle<rhi_handle_type::render_surface> handle) const noexcept
    {
        auto swapchain_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto swapchain_it = _swapchains.find(swapchain_key);
        if (swapchain_it != _swapchains.end())
        {
            return swapchain_it->window;
        }
        return nullptr;
    }

    bool device::supports_descriptor_buffers() const noexcept
    {
        return true;
    }

    size_t device::get_descriptor_buffer_alignment() const noexcept
    {
        return _descriptor_buffer_properties.descriptorBufferOffsetAlignment;
    }

    size_t device::get_descriptor_set_layout_size(
        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> layout) const noexcept
    {
        auto desc_set_layout = _descriptor_set_layout_cache.get_layout(layout);
        if (desc_set_layout != VK_NULL_HANDLE)
        {
            // Query the size of the descriptor set layout using vkGetDescriptorSetLayoutSizeEXT
            auto size = VkDeviceSize{};
            _dispatch_table.getDescriptorSetLayoutSizeEXT(desc_set_layout, &size);
            return static_cast<size_t>(size);
        }

        return 0;
    }

    size_t device::write_descriptor_buffer(const descriptor_set_desc& writes, byte* dest, size_t offset) const noexcept
    {
        const auto layout = _descriptor_set_layout_cache.get_layout(writes.layout);
        const auto get_desc_binding_size = [&](descriptor_type type) -> size_t {
            switch (type)
            {
            case descriptor_type::sampler:
                return _descriptor_buffer_properties.samplerDescriptorSize;
            case descriptor_type::combined_image_sampler:
                return _descriptor_buffer_properties.combinedImageSamplerDescriptorSize;
            case descriptor_type::sampled_image:
                return _descriptor_buffer_properties.sampledImageDescriptorSize;
            case descriptor_type::storage_image:
                return _descriptor_buffer_properties.storageImageDescriptorSize;
            case descriptor_type::constant_buffer:
                return _descriptor_buffer_properties.uniformBufferDescriptorSize;
            case descriptor_type::structured_buffer:
                return _descriptor_buffer_properties.storageBufferDescriptorSize;
            default:
                logger->error("Unsupported descriptor type for descriptor buffer writing - {}", to_underlying(type));
                std::terminate();
            }
        };

        const auto aligned_offset = math::round_to_next_multiple(offset, get_descriptor_buffer_alignment());

        for (const auto& write : writes.buffers)
        {
            auto buf = get_buffer(write.buffer);

            const auto buffer_desc = VkDescriptorAddressInfoEXT{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
                .pNext = nullptr,
                .address = buf->address + write.offset,
                .range = write.size,
                .format = VK_FORMAT_UNDEFINED,
            };

            const auto desc_data = [&]() {
                switch (write.type)
                {
                case rhi::descriptor_type::constant_buffer:
                    return VkDescriptorDataEXT{
                        .pUniformBuffer = &buffer_desc,
                    };
                case rhi::descriptor_type::structured_buffer:
                    return VkDescriptorDataEXT{
                        .pStorageBuffer = &buffer_desc,
                    };
                default:
                    logger->error("Unsupported buffer descriptor type for descriptor buffer writing - {}",
                                  to_underlying(write.type));
                    std::terminate();
                };
            }();

            const auto desc = VkDescriptorGetInfoEXT{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .pNext = nullptr,
                .type = to_vulkan(write.type),
                .data = desc_data,
            };

            // Query the descriptor offset
            // TODO: Cache offsets for better performance
            VkDeviceSize descriptor_offset = 0;
            _dispatch_table.getDescriptorSetLayoutBindingOffsetEXT(layout, write.index, &descriptor_offset);

            // Write the descriptor data to the destination buffer
            _dispatch_table.getDescriptorEXT(&desc, get_desc_binding_size(write.type),
                                             dest + aligned_offset + descriptor_offset);
        }

        for (const auto& write : writes.images)
        {
            // Query the descriptor offset
            VkDeviceSize descriptor_offset = 0;
            _dispatch_table.getDescriptorSetLayoutBindingOffsetEXT(layout, write.index, &descriptor_offset);

            for (size_t i = 0; i < write.images.size(); ++i)
            {
                auto img = get_image(write.images[i].image);
                VkSampler sampler = VK_NULL_HANDLE;
                if (write.images[i].sampler)
                {
                    sampler = get_sampler(write.images[i].sampler)->sampler;
                }

                const auto image_info = VkDescriptorImageInfo{
                    .sampler = sampler,
                    .imageView = img->image_view,
                    .imageLayout = to_vulkan(write.images[i].layout),
                };

                const auto desc_data = [&]() {
                    switch (write.type)
                    {
                    case rhi::descriptor_type::sampled_image:
                        return VkDescriptorDataEXT{
                            .pSampledImage = &image_info,
                        };
                    case rhi::descriptor_type::storage_image:
                        return VkDescriptorDataEXT{
                            .pStorageImage = &image_info,
                        };
                    case rhi::descriptor_type::combined_image_sampler:
                        return VkDescriptorDataEXT{
                            .pCombinedImageSampler = &image_info,
                        };
                    default:
                        logger->error("Unsupported image descriptor type for descriptor buffer writing - {}",
                                      to_underlying(write.type));
                        std::terminate();
                    }
                }();

                const auto desc = VkDescriptorGetInfoEXT{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                    .pNext = nullptr,
                    .type = to_vulkan(write.type),
                    .data = desc_data,
                };

                // Write the descriptor data to the destination buffer
                _dispatch_table.getDescriptorEXT(&desc, get_desc_binding_size(write.type),
                                                 dest + aligned_offset + descriptor_offset +
                                                     i * get_desc_binding_size(write.type));
            }
        }

        for (const auto& sampler : writes.samplers)
        {
            // Query the descriptor offset
            VkDeviceSize descriptor_offset = 0;
            _dispatch_table.getDescriptorSetLayoutBindingOffsetEXT(layout, sampler.index, &descriptor_offset);

            for (size_t i = 0; i < sampler.samplers.size(); ++i)
            {
                const auto samp = get_sampler(sampler.samplers[i]);

                const auto desc_data = VkDescriptorDataEXT{
                    .pSampler = &samp->sampler,
                };

                const auto desc = VkDescriptorGetInfoEXT{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                    .pNext = nullptr,
                    .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .data = desc_data,
                };

                // Write the descriptor data to the destination buffer
                _dispatch_table.getDescriptorEXT(&desc, _descriptor_buffer_properties.sampledImageDescriptorSize,
                                                 dest + aligned_offset + descriptor_offset +
                                                     i * _descriptor_buffer_properties.samplerDescriptorSize);
            }
        }

        return aligned_offset + get_descriptor_set_layout_size(writes.layout);
    }

    void device::release_resources()
    {
        _delete_queue.release_resources(_current_frame);
        _resource_tracker.try_release();
    }

    void device::finish_frame()
    {
        _current_frame++;
    }

    uint32_t device::frame_in_flight() const noexcept
    {
        return _current_frame % frames_in_flight();
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
        auto window = static_cast<const vk::window_surface*>(desc.window);
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
                .set_required_min_image_count(3)
                .set_desired_min_image_count(desc.min_image_count)
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
            .render_complete = {},
            .frames = {},
            .window = desc.window,
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
                .create_info =
                    {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                        .pNext = nullptr,
                        .flags = 0,
                        .imageType = VK_IMAGE_TYPE_2D,
                        .format = to_vulkan(desc.format.format),
                        .extent =
                            {
                                .width = desc.width,
                                .height = desc.height,
                                .depth = 1,
                            },
                        .mipLevels = 1,
                        .arrayLayers = desc.layers,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .tiling = VK_IMAGE_TILING_OPTIMAL,
                        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                        .queueFamilyIndexCount = 0,
                        .pQueueFamilyIndices = nullptr,
                        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    },
                .view_create_info = {},
                .name = "Swapchain Image",
            }));
        }

        for (size_t i = 0; i < num_frames_in_flight; ++i)
        {
            // Allocate a fence for each frame in flight
            // Allocate two semaphores for each frame in flight

            auto image_acquired = create_semaphore({
                .type = rhi::semaphore_type::binary,
                .initial_value = 0,
            });

            fif_data fif = {
                .image_acquired = image_acquired,
            };

            sc.frames.push_back(fif);
        }

        for (size_t i = 0; i < images.size(); ++i)
        {
            auto render_complete = create_semaphore({
                .type = rhi::semaphore_type::binary,
                .initial_value = 0,
            });

            sc.render_complete.push_back(render_complete);
        }

        if (old_swapchain_it != _swapchains.end())
        {
            for (auto fif : old_swapchain_it->frames)
            {
                destroy_semaphore(fif.image_acquired);
            }

            for (auto sem : old_swapchain_it->render_complete)
            {
                destroy_semaphore(sem);
            }

            // Replace the old swapchain in the map
            *old_swapchain_it = tempest::move(sc);
            return old_swapchain;
        }

        const auto new_key = _swapchains.insert(sc);
        const auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        const auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::render_surface>(new_key_id, new_key_gen);
    }

    VkFence device::get_fence(typed_rhi_handle<rhi_handle_type::fence> handle) const noexcept
    {
        const auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        const auto it = _fences.find(key);
        if (it != _fences.end())
        {
            return it->fence;
        }
        return VK_NULL_HANDLE;
    }

    VkSemaphore device::get_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) const noexcept
    {
        const auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        const auto it = _semaphores.find(key);
        if (it != _semaphores.end())
        {
            return it->semaphore;
        }
        return VK_NULL_HANDLE;
    }

    VkSwapchainKHR device::get_swapchain(typed_rhi_handle<rhi_handle_type::render_surface> handle) const noexcept
    {
        const auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        const auto it = _swapchains.find(key);
        if (it != _swapchains.end())
        {
            return it->swapchain;
        }
        return VK_NULL_HANDLE;
    }

    VkDescriptorSetLayout device::get_descriptor_set_layout(
        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) const noexcept
    {
        return _descriptor_set_layout_cache.get_layout(handle);
    }

    VkPipelineLayout device::get_pipeline_layout(
        typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) const noexcept
    {
        return _pipeline_layout_cache.get_layout(handle);
    }

    optional<const vk::buffer&> device::get_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) const noexcept
    {
        const auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        const auto it = _buffers.find(key);
        if (it != _buffers.end())
        {
            return *it;
        }
        return none();
    }

    optional<const vk::image&> device::get_image(typed_rhi_handle<rhi_handle_type::image> handle) const noexcept
    {
        const auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        const auto it = _images.find(key);
        if (it != _images.end())
        {
            return *it;
        }
        return none();
    }

    optional<const vk::graphics_pipeline&> device::get_graphics_pipeline(
        typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle) const noexcept
    {
        const auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        const auto it = _graphics_pipelines.find(key);
        if (it != _graphics_pipelines.end())
        {
            return *it;
        }

        return none();
    }

    optional<const vk::descriptor_set&> device::get_descriptor_set(
        typed_rhi_handle<rhi_handle_type::descriptor_set> handle) const noexcept
    {
        const auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        const auto it = _descriptor_sets.find(key);
        if (it != _descriptor_sets.end())
        {
            return *it;
        }
        return none();
    }

    optional<const vk::sampler&> device::get_sampler(typed_rhi_handle<rhi_handle_type::sampler> handle) const noexcept
    {
        const auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        const auto it = _samplers.find(key);
        if (it != _samplers.end())
        {
            return *it;
        }
        return none();
    }

    optional<const vk::compute_pipeline&> device::get_compute_pipeline(
        typed_rhi_handle<rhi_handle_type::compute_pipeline> handle) const noexcept
    {
        const auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        const auto it = _compute_pipelines.find(key);
        if (it != _compute_pipelines.end())
        {
            return *it;
        }
        return none();
    }

    void device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _buffers.find(key);
        if (it != _buffers.end())
        {
            vmaDestroyBuffer(_vma_allocator, it->buffer, it->allocation);
            _buffers.erase(key);
        }
    }

    void device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::image> handle) noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _images.find(key);
        if (it != _images.end())
        {
            if (it->image_view)
            {
                _dispatch_table.destroyImageView(it->image_view, nullptr);
            }
            if (it->image && !it->swapchain_image)
            {
                vmaDestroyImage(_vma_allocator, it->image, it->allocation);
            }
            _images.erase(key);
        }
    }

    bool device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept
    {
        return _descriptor_set_layout_cache.release_layout(handle);
    }

    bool device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept
    {
        return _pipeline_layout_cache.release_layout(handle);
    }

    void device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle) noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _graphics_pipelines.find(key);
        if (it != _graphics_pipelines.end())
        {
            _dispatch_table.destroyPipeline(it->pipeline, nullptr);

            release_resource_immediate(it->desc.layout);

            for (auto& shader : it->shader_modules)
            {
                _dispatch_table.destroyShaderModule(shader, nullptr);
            }

            _graphics_pipelines.erase(key);
        }
    }

    void device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::descriptor_set> handle) noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _descriptor_sets.find(key);
        if (it != _descriptor_sets.end())
        {
            _dispatch_table.freeDescriptorSets(it->pool, 1, &it->set);
            _descriptor_sets.erase(key);
        }
    }

    void device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::compute_pipeline> handle) noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _compute_pipelines.find(key);
        if (it != _compute_pipelines.end())
        {
            _dispatch_table.destroyPipeline(it->pipeline, nullptr);
            release_resource_immediate(it->desc.layout);

            _dispatch_table.destroyShaderModule(it->shader_module, nullptr);

            _compute_pipelines.erase(key);
        }
    }

    void device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::sampler> handle) noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _samplers.find(key);
        if (it != _samplers.end())
        {
            _dispatch_table.destroySampler(it->sampler, nullptr);
            _samplers.erase(key);
        }
    }

    vector<pair<const work_queue*, uint64_t>> device::compute_current_work_queue_timeline_values() const noexcept
    {
        vector<pair<const work_queue*, uint64_t>> timeline_values;

        if (_primary_work_queue)
        {
            timeline_values.emplace_back(&*_primary_work_queue, _primary_work_queue->query_completed_timeline_value());
        }

        if (_dedicated_compute_queue)
        {
            timeline_values.emplace_back(&*_dedicated_compute_queue,
                                         _dedicated_compute_queue->query_completed_timeline_value());
        }

        if (_dedicated_transfer_queue)
        {
            timeline_values.emplace_back(&*_dedicated_transfer_queue,
                                         _dedicated_transfer_queue->query_completed_timeline_value());
        }

        return timeline_values;
    }

    void device::name_object(VkObjectType type, void* handle, const char* name) noexcept
    {
        if (_can_name)
        {
            VkDebugUtilsObjectNameInfoEXT name_info = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = nullptr,
                .objectType = type,
                .objectHandle = bit_cast<uint64_t>(handle),
                .pObjectName = name,
            };

            _dispatch_table.setDebugUtilsObjectNameEXT(&name_info);

            logger->debug("Named object {} - {}", handle, name);
        }
    }

    work_queue::work_queue(device* parent, vkb::DispatchTable* dispatch, VkQueue queue, uint32_t queue_family_index,
                           uint32_t fif, resource_tracker* res_tracker) noexcept
        : _dispatch(dispatch), _queue(queue), _queue_family_index(queue_family_index), _parent{parent},
          _res_tracker{res_tracker}
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

        VkSemaphoreTypeCreateInfo timeline_sem_type_ci = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = 0,
        };

        VkSemaphoreCreateInfo sem_ci = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &timeline_sem_type_ci,
            .flags = 0,
        };

        _dispatch->createSemaphore(&sem_ci, nullptr, &_resource_tracking_sem);
    }

    work_queue::~work_queue()
    {
        _dispatch->queueWaitIdle(_queue); // Ensure the queue is done before working on it

        _dispatch->destroySemaphore(_resource_tracking_sem, nullptr);

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

    typed_rhi_handle<rhi_handle_type::command_list> work_queue::get_next_command_list() noexcept
    {
        auto& wg = _work_groups[_parent->frame_in_flight()];
        return wg.acquire_next_command_buffer();
    }

    bool work_queue::submit(span<const submit_info> infos, typed_rhi_handle<rhi_handle_type::fence> fence) noexcept
    {
        if (infos.empty())
        {
            return false;
        }

        auto timestamp = _next_timeline_value++;
        auto timeline_sem = get_timeline_semaphore();

        auto submit_infos = _allocator.allocate_typed<VkSubmitInfo2>(infos.size());

        for (size_t i = 0; i < infos.size(); ++i)
        {
            auto wait_count = static_cast<uint32_t>(infos[i].wait_semaphores.size());
            auto signal_count = static_cast<uint32_t>(infos[i].signal_semaphores.size());

            // If this is the last submit in the frame, use this to signal the timeline semaphore
            if (i == infos.size() - 1)
            {
                signal_count++;
            }

            auto wait_sems = _allocator.allocate_typed<VkSemaphoreSubmitInfo>(wait_count);
            auto signal_sems = _allocator.allocate_typed<VkSemaphoreSubmitInfo>(signal_count);
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

            // If this is the last submit in the frame, signal the timeline semaphore
            if (i == infos.size() - 1)
            {
                signal_sems[signal_count - 1] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .pNext = nullptr,
                    .semaphore = timeline_sem,
                    .value = timestamp,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .deviceIndex = 1,
                };

                _last_submitted_value = timestamp;
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
                .waitSemaphoreInfoCount = wait_count,
                .pWaitSemaphoreInfos = wait_sems,
                .commandBufferInfoCount = static_cast<uint32_t>(infos[i].command_lists.size()),
                .pCommandBufferInfos = cmds,
                .signalSemaphoreInfoCount = signal_count,
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

        // Track all the resources used in this submit
        for (const auto& info : infos)
        {
            for (const auto& cmd_list : info.command_lists)
            {
                for (const auto& buf : used_buffers[cmd_list])
                {
                    _res_tracker->track(buf, timestamp, this);
                }
                used_buffers.erase(cmd_list);

                for (const auto& img : used_images[cmd_list])
                {
                    _res_tracker->track(img, timestamp, this);
                }
                used_images.erase(cmd_list);

                for (const auto& smp : used_samplers[cmd_list])
                {
                    _res_tracker->track(smp, timestamp, this);
                }
                used_samplers.erase(cmd_list);

                for (const auto& pipe : used_gfx_pipelines[cmd_list])
                {
                    _res_tracker->track(pipe, timestamp, this);
                }
                used_gfx_pipelines.erase(cmd_list);
            }
        }

        return result == VK_SUCCESS;
    }

    vector<rhi::work_queue::present_result> work_queue::present(const present_info& info) noexcept
    {
        auto swapchains = _allocator.allocate_typed<VkSwapchainKHR>(info.swapchain_images.size());
        auto image_indices = _allocator.allocate_typed<uint32_t>(info.swapchain_images.size());
        auto wait_sems = _allocator.allocate_typed<VkSemaphore>(info.wait_semaphores.size());
        auto vk_results = _allocator.allocate_typed<VkResult>(info.swapchain_images.size());

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
            .pResults = vk_results,
        };

        _dispatch->queuePresentKHR(_queue, &present_info);

        auto results = vector<rhi::work_queue::present_result>(info.swapchain_images.size());

        for (size_t i = 0; i < info.swapchain_images.size(); ++i)
        {
            switch (vk_results[i])
            {
            case VK_SUCCESS:
                results[i] = rhi::work_queue::present_result::success;
                break;
            case VK_ERROR_OUT_OF_DATE_KHR:
                results[i] = rhi::work_queue::present_result::out_of_date;
                break;
            case VK_SUBOPTIMAL_KHR:
                results[i] = rhi::work_queue::present_result::suboptimal;
                break;
            default:
                results[i] = rhi::work_queue::present_result::error;
                break;
            }
        }

        _allocator.reset();

        return results;
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

            // Track the image barrier for the resource tracker
            used_images[command_list].push_back(image_barriers[i].image);
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

        // Track the image for the resource tracker
        used_images[command_list].push_back(image);
    }

    void work_queue::blit(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::image> src, image_layout src_layout, uint32_t src_mip,
                          typed_rhi_handle<rhi_handle_type::image> dst, image_layout dst_layout,
                          uint32_t dst_mip) noexcept
    {
        VkImageBlit blit_region = {
            .srcSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = src_mip,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .srcOffsets =
                {
                    {
                        .x = 0,
                        .y = 0,
                        .z = 0,
                    },
                    {
                        .x = static_cast<int32_t>(_parent->get_image(src)->create_info.extent.width),
                        .y = static_cast<int32_t>(_parent->get_image(src)->create_info.extent.height),
                        .z = static_cast<int32_t>(_parent->get_image(src)->create_info.extent.depth),
                    },
                },
            .dstSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = dst_mip,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .dstOffsets =
                {
                    {
                        .x = 0,
                        .y = 0,
                        .z = 0,
                    },
                    {
                        .x = static_cast<int32_t>(_parent->get_image(dst)->create_info.extent.width),
                        .y = static_cast<int32_t>(_parent->get_image(dst)->create_info.extent.height),
                        .z = static_cast<int32_t>(_parent->get_image(dst)->create_info.extent.depth),
                    },
                },
        };
        _dispatch->cmdBlitImage(_parent->get_command_buffer(command_list), _parent->get_image(src)->image,
                                to_vulkan(src_layout), _parent->get_image(dst)->image, to_vulkan(dst_layout), 1,
                                &blit_region, VK_FILTER_LINEAR);

        // Track the images for the resource tracker
        used_images[command_list].push_back(src);
        used_images[command_list].push_back(dst);
    }

    void work_queue::generate_mip_chain(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                        typed_rhi_handle<rhi_handle_type::image> img, image_layout current_layout,
                                        uint32_t base_mip, uint32_t mip_count) noexcept
    {
        // Transition image to general layout
        // Source should wait for all previous operations to complete

        auto image = _parent->get_image(img);

        VkImageMemoryBarrier2 pre_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = to_vulkan(current_layout),
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = _queue_family_index,
            .dstQueueFamilyIndex = _queue_family_index,
            .image = image->image,
            .subresourceRange =
                {
                    .aspectMask = _parent->get_image(img)->image_aspect,
                    .baseMipLevel = base_mip,
                    .levelCount = mip_count,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        auto width = image->create_info.extent.width;
        auto height = image->create_info.extent.height;

        auto mip_width = width;
        auto mip_height = height;

        auto blits = _allocator.allocate_typed<VkImageBlit>(mip_count - 1);

        // From mips 1 to mip_count - 1, blit from mip 0 to mip i
        for (size_t i = 1; i < mip_count; ++i)
        {
            // Get mip width and height
            mip_width = std::max(1u, width >> i);
            mip_height = std::max(1u, height >> i);

            blits[i - 1] = {
                .srcSubresource =
                    {
                        .aspectMask = image->image_aspect,
                        .mipLevel = base_mip,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .srcOffsets =
                    {
                        {
                            .x = 0,
                            .y = 0,
                            .z = 0,
                        },
                        {
                            .x = static_cast<int32_t>(width),
                            .y = static_cast<int32_t>(height),
                            .z = 1,
                        },
                    },
                .dstSubresource =
                    {
                        .aspectMask = image->image_aspect,
                        .mipLevel = base_mip + static_cast<uint32_t>(i),
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .dstOffsets =
                    {
                        {
                            .x = 0,
                            .y = 0,
                            .z = 0,
                        },
                        {
                            .x = static_cast<int32_t>(mip_width),
                            .y = static_cast<int32_t>(mip_height),
                            .z = 1,
                        },
                    },
            };
        }

        // Transition image to original layout
        VkImageMemoryBarrier2 post_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = to_vulkan(current_layout),
            .srcQueueFamilyIndex = _queue_family_index,
            .dstQueueFamilyIndex = _queue_family_index,
            .image = image->image,
            .subresourceRange =
                {
                    .aspectMask = image->image_aspect,
                    .baseMipLevel = base_mip,
                    .levelCount = mip_count,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        VkDependencyInfo dep_info_pre = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &pre_barrier,
        };

        VkDependencyInfo dep_info_post = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &post_barrier,
        };

        // Record the commands
        _dispatch->cmdPipelineBarrier2(_parent->get_command_buffer(command_list), &dep_info_pre);
        _dispatch->cmdBlitImage(_parent->get_command_buffer(command_list), image->image, VK_IMAGE_LAYOUT_GENERAL,
                                image->image, VK_IMAGE_LAYOUT_GENERAL, mip_count - 1, blits, VK_FILTER_LINEAR);
        _dispatch->cmdPipelineBarrier2(_parent->get_command_buffer(command_list), &dep_info_post);
        _allocator.reset();

        used_images[command_list].push_back(img);
    }

    void work_queue::copy(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::buffer> src, typed_rhi_handle<rhi_handle_type::buffer> dst,
                          size_t src_offset, size_t dst_offset, size_t byte_count) noexcept
    {
        if (byte_count == 0)
        {
            return;
        }

        VkBufferCopy copy_region = {
            .srcOffset = src_offset,
            .dstOffset = dst_offset,
            .size = byte_count,
        };

        _dispatch->cmdCopyBuffer(_parent->get_command_buffer(command_list), _parent->get_buffer(src)->buffer,
                                 _parent->get_buffer(dst)->buffer, 1, &copy_region);

        // Track the buffers for the resource tracker
        used_buffers[command_list].push_back(src);
        used_buffers[command_list].push_back(dst);
    }

    void work_queue::fill(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::buffer> handle, size_t offset, size_t size,
                          uint32_t data) noexcept
    {
        _dispatch->cmdFillBuffer(_parent->get_command_buffer(command_list), _parent->get_buffer(handle)->buffer, offset,
                                 size, data);

        // Track the buffer for the resource tracker
        used_buffers[command_list].push_back(handle);
    }

    void work_queue::copy(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::buffer> src, typed_rhi_handle<rhi_handle_type::image> dst,
                          image_layout layout, size_t src_offset, uint32_t dst_mip) noexcept
    {
        const auto& img = *_parent->get_image(dst);

        const VkBufferImageCopy copy_region = {
            .bufferOffset = src_offset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask = img.view_create_info.subresourceRange.aspectMask,
                    .mipLevel = dst_mip,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageOffset =
                {
                    .x = 0,
                    .y = 0,
                    .z = 0,
                },
            .imageExtent =
                {
                    .width = img.create_info.extent.width,
                    .height = img.create_info.extent.height,
                    .depth = img.create_info.extent.depth,
                },
        };
        _dispatch->cmdCopyBufferToImage(_parent->get_command_buffer(command_list), _parent->get_buffer(src)->buffer,
                                        img.image, to_vulkan(layout), 1, &copy_region);

        // Track the buffer and image for the resource tracker
        used_buffers[command_list].push_back(src);
        used_images[command_list].push_back(dst);
    }

    void work_queue::pipeline_barriers(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                       span<const image_barrier> image_barriers,
                                       span<const buffer_barrier> buffer_barriers) noexcept
    {
        auto img_barriers = _allocator.allocate_typed<VkImageMemoryBarrier2>(image_barriers.size());
        auto buf_barriers = _allocator.allocate_typed<VkBufferMemoryBarrier2>(buffer_barriers.size());

        for (size_t i = 0; i < image_barriers.size(); ++i)
        {
            auto img = _parent->get_image(image_barriers[i].image);
            img_barriers[i] = {
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

            // Track the image barrier for the resource tracker
            used_images[command_list].push_back(image_barriers[i].image);
        }

        for (size_t i = 0; i < buffer_barriers.size(); ++i)
        {
            auto buf = _parent->get_buffer(buffer_barriers[i].buffer);
            buf_barriers[i] = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = to_vulkan(buffer_barriers[i].src_stages),
                .srcAccessMask = to_vulkan(buffer_barriers[i].src_access),
                .dstStageMask = to_vulkan(buffer_barriers[i].dst_stages),
                .dstAccessMask = to_vulkan(buffer_barriers[i].dst_access),
                .srcQueueFamilyIndex =
                    buffer_barriers[i].src_queue
                        ? static_cast<vk::work_queue*>(buffer_barriers[i].src_queue)->_queue_family_index
                        : VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex =
                    buffer_barriers[i].dst_queue
                        ? static_cast<vk::work_queue*>(buffer_barriers[i].dst_queue)->_queue_family_index
                        : VK_QUEUE_FAMILY_IGNORED,
                .buffer = buf->buffer,
                .offset = buffer_barriers[i].offset,
                .size = buffer_barriers[i].size,
            };

            // Track the buffer barrier for the resource tracker
            used_buffers[command_list].push_back(buffer_barriers[i].buffer);
        }

        VkDependencyInfo dep_info = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers.size()),
            .pBufferMemoryBarriers = buf_barriers,
            .imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers.size()),
            .pImageMemoryBarriers = img_barriers,
        };

        _dispatch->cmdPipelineBarrier2(_parent->get_command_buffer(command_list), &dep_info);

        _allocator.reset();
    }

    void work_queue::begin_rendering(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                     const render_pass_info& render_pass_info) noexcept
    {
        auto color_attachments =
            render_pass_info.color_attachments.empty()
                ? nullptr
                : _allocator.allocate_typed<VkRenderingAttachmentInfo>(render_pass_info.color_attachments.size());
        auto depth_attachment =
            render_pass_info.depth_attachment ? _allocator.allocate_typed<VkRenderingAttachmentInfo>(1) : nullptr;
        auto stencil_attachment =
            render_pass_info.stencil_attachment ? _allocator.allocate_typed<VkRenderingAttachmentInfo>(1) : nullptr;

        for (size_t i = 0; i < render_pass_info.color_attachments.size(); ++i)
        {
            color_attachments[i] = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = _parent->get_image(render_pass_info.color_attachments[i].image)->image_view,
                .imageLayout = to_vulkan(render_pass_info.color_attachments[i].layout),
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = to_vulkan(render_pass_info.color_attachments[i].load_op),
                .storeOp = to_vulkan(render_pass_info.color_attachments[i].store_op),
                .clearValue =
                    {
                        .color =
                            {
                                .float32 =
                                    {
                                        render_pass_info.color_attachments[i].clear_color[0],
                                        render_pass_info.color_attachments[i].clear_color[1],
                                        render_pass_info.color_attachments[i].clear_color[2],
                                        render_pass_info.color_attachments[i].clear_color[3],
                                    },
                            },
                    },
            };
            // Track the image for the resource tracker
            used_images[command_list].push_back(render_pass_info.color_attachments[i].image);
        }

        if (render_pass_info.depth_attachment)
        {
            *depth_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = _parent->get_image(render_pass_info.depth_attachment->image)->image_view,
                .imageLayout = to_vulkan(render_pass_info.depth_attachment->layout),
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = to_vulkan(render_pass_info.depth_attachment->load_op),
                .storeOp = to_vulkan(render_pass_info.depth_attachment->store_op),
                .clearValue =
                    {
                        .depthStencil =
                            {
                                .depth = render_pass_info.depth_attachment->clear_depth,
                                .stencil = 0,
                            },
                    },
            };
            // Track the image for the resource tracker
            used_images[command_list].push_back(render_pass_info.depth_attachment->image);
        }

        if (render_pass_info.stencil_attachment)
        {
            *stencil_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = _parent->get_image(render_pass_info.stencil_attachment->image)->image_view,
                .imageLayout = to_vulkan(render_pass_info.stencil_attachment->layout),
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = to_vulkan(render_pass_info.stencil_attachment->load_op),
                .storeOp = to_vulkan(render_pass_info.stencil_attachment->store_op),
                .clearValue =
                    {
                        .depthStencil =
                            {
                                .depth = 0.0f,
                                .stencil = render_pass_info.stencil_attachment->clear_stencil,
                            },
                    },
            };
            // Track the image for the resource tracker
            used_images[command_list].push_back(render_pass_info.stencil_attachment->image);
        }

        VkRenderingInfo render_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea =
                {
                    .offset =
                        {
                            .x = render_pass_info.x,
                            .y = render_pass_info.y,
                        },
                    .extent =
                        {
                            .width = render_pass_info.width,
                            .height = render_pass_info.height,
                        },
                },
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = static_cast<uint32_t>(render_pass_info.color_attachments.size()),
            .pColorAttachments = color_attachments,
            .pDepthAttachment = depth_attachment ? depth_attachment : nullptr,
            .pStencilAttachment = stencil_attachment ? stencil_attachment : nullptr,
        };

        _dispatch->cmdBeginRendering(_parent->get_command_buffer(command_list), &render_info);
        _allocator.reset();
    }

    void work_queue::end_rendering(typed_rhi_handle<rhi_handle_type::command_list> command_list) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        _dispatch->cmdEndRendering(cmds);
    }

    void work_queue::bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::graphics_pipeline> pipeline) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        auto pipe = _parent->get_graphics_pipeline(pipeline);
        _dispatch->cmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->pipeline);

        // Track the pipeline for the resource tracker
        used_gfx_pipelines[command_list].push_back(pipeline);
    }

    void work_queue::draw(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::buffer> indirect_buffer, uint32_t offset,
                          uint32_t draw_count, uint32_t stride) noexcept
    {
        if (draw_count == 0)
        {
            return; // No draws to perform
        }

        auto cmds = _parent->get_command_buffer(command_list);
        auto buf = _parent->get_buffer(indirect_buffer);
        _dispatch->cmdDrawIndexedIndirect(cmds, buf->buffer, offset, draw_count, stride);

        // Track the buffer for the resource tracker
        used_buffers[command_list].push_back(indirect_buffer);
    }

    void work_queue::draw(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t vertex_count,
                          uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        _dispatch->cmdDraw(cmds, vertex_count, instance_count, first_vertex, first_instance);
    }

    void work_queue::draw(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t index_count,
                          uint32_t instance_count, uint32_t first_index, int32_t vertex_offset,
                          uint32_t first_instance) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        _dispatch->cmdDrawIndexed(cmds, index_count, instance_count, first_index, vertex_offset, first_instance);
    }

    void work_queue::bind_index_buffer(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                       typed_rhi_handle<rhi_handle_type::buffer> buffer, uint32_t offset,
                                       rhi::index_format index_type) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        auto buf = _parent->get_buffer(buffer);
        _dispatch->cmdBindIndexBuffer(cmds, buf->buffer, offset, to_vulkan(index_type));
        // Track the buffer for the resource tracker
        used_buffers[command_list].push_back(buffer);
    }

    void work_queue::bind_vertex_buffers(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                         uint32_t first_binding,
                                         span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers,
                                         span<const size_t> offsets) noexcept
    {
        auto vk_buffers = _allocator.allocate_typed<VkBuffer>(buffers.size());
        auto vk_offsets = _allocator.allocate_typed<VkDeviceSize>(offsets.size());

        for (size_t i = 0; i < buffers.size(); ++i)
        {
            auto buf = _parent->get_buffer(buffers[i]);
            vk_buffers[i] = buf->buffer;
            vk_offsets[i] = static_cast<VkDeviceSize>(offsets[i]);
        }

        _dispatch->cmdBindVertexBuffers(_parent->get_command_buffer(command_list), first_binding,
                                        static_cast<uint32_t>(buffers.size()), vk_buffers, vk_offsets);

        for (auto&& buffer : buffers)
        {
            used_buffers[command_list].push_back(buffer);
        }
    }

    void work_queue::set_scissor_region(typed_rhi_handle<rhi_handle_type::command_list> command_list, int32_t x,
                                        int32_t y, uint32_t width, uint32_t height, uint32_t region_index) noexcept
    {
        const auto cmds = _parent->get_command_buffer(command_list);
        const auto scissor = VkRect2D{
            .offset =
                {
                    .x = x,
                    .y = y,
                },
            .extent =
                {
                    .width = width,
                    .height = height,
                },
        };
        _dispatch->cmdSetScissor(cmds, region_index, 1, &scissor);
    }

    void work_queue::set_viewport(typed_rhi_handle<rhi_handle_type::command_list> command_list, float x, float y,
                                  float width, float height, float min_depth, float max_depth, uint32_t region_index,
                                  bool flipped) noexcept
    {
        const auto cmds = _parent->get_command_buffer(command_list);
        const auto viewport = VkViewport{
            .x = x,
            .y = flipped ? height - y : y,
            .width = width,
            .height = flipped ? -height : height,
            .minDepth = min_depth,
            .maxDepth = max_depth,
        };
        _dispatch->cmdSetViewport(cmds, region_index, 1, &viewport);
    }

    void work_queue::set_cull_mode(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                   enum_mask<cull_mode> cull) noexcept
    {
        const auto cmds = _parent->get_command_buffer(command_list);
        const auto cull_mode = to_vulkan(cull);
        _dispatch->cmdSetCullMode(cmds, cull_mode);
    }

    void work_queue::bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::compute_pipeline> pipeline) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        auto pipe = _parent->get_compute_pipeline(pipeline);
        _dispatch->cmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->pipeline);
        // Track the pipeline for the resource tracker
        used_compute_pipelines[command_list].push_back(pipeline);
    }

    void work_queue::dispatch(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t x, uint32_t y,
                              uint32_t z) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        _dispatch->cmdDispatch(cmds, x, y, z);
    }

    void work_queue::bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout, bind_point point,
                          uint32_t first_set_index, span<const typed_rhi_handle<rhi_handle_type::descriptor_set>> sets,
                          span<const uint32_t> dynamic_offsets) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        auto vk_sets = _allocator.allocate_typed<VkDescriptorSet>(sets.size());
        for (size_t i = 0; i < sets.size(); ++i)
        {
            auto set_payload = _parent->get_descriptor_set(sets[i]);
            vk_sets[i] = set_payload->set;

            // Track the descriptor set for the resource tracker
            for (const auto& buf : set_payload->bound_buffers)
            {
                used_buffers[command_list].push_back(buf);
            }

            for (const auto& img : set_payload->bound_images)
            {
                used_images[command_list].push_back(img);
            }

            for (const auto& smp : set_payload->bound_samplers)
            {
                used_samplers[command_list].push_back(smp);
            }
        }

        _dispatch->cmdBindDescriptorSets(cmds, to_vulkan(point), _parent->get_pipeline_layout(pipeline_layout),
                                         first_set_index, static_cast<uint32_t>(sets.size()), vk_sets,
                                         static_cast<uint32_t>(dynamic_offsets.size()), dynamic_offsets.data());
        _allocator.reset();
    }

    void work_queue::push_descriptors(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                      typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout,
                                      bind_point point, uint32_t set_index,
                                      span<const buffer_binding_descriptor> buffers,
                                      span<const image_binding_descriptor> images,
                                      span<const sampler_binding_descriptor> samplers) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        auto layout = _parent->get_pipeline_layout(pipeline_layout);

        const auto write_count = static_cast<uint32_t>(buffers.size() + images.size() + samplers.size());
        if (write_count == 0)
        {
            return; // No descriptors to push
        }

        auto writes = _allocator.allocate_typed<VkWriteDescriptorSet>(write_count);
        auto write_index = 0u;

        for (const auto& buffer : buffers)
        {
            auto buffer_write_info = _allocator.allocate_typed<VkDescriptorBufferInfo>(1);
            buffer_write_info[0] = {
                .buffer = _parent->get_buffer(buffer.buffer)->buffer,
                .offset = buffer.offset,
                .range = buffer.size,
            };

            writes[write_index++] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = VK_NULL_HANDLE, // Will be set later
                .dstBinding = buffer.index,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = to_vulkan(buffer.type),
                .pImageInfo = nullptr,
                .pBufferInfo = buffer_write_info,
                .pTexelBufferView = nullptr,
            };
            // Track the buffer for the resource tracker
            used_buffers[command_list].push_back(buffer.buffer);
        }

        for (const auto& image : images)
        {
            auto image_write_info = _allocator.allocate_typed<VkDescriptorImageInfo>(image.images.size());
            for (size_t i = 0; i < image.images.size(); ++i)
            {
                const auto& img_info = image.images[i];
                auto img = _parent->get_image(img_info.image);
                auto smp = img_info.sampler ? _parent->get_sampler(img_info.sampler)->sampler : VK_NULL_HANDLE;
                auto img_layout = to_vulkan(img_info.layout);

                image_write_info[i] = {
                    .sampler = smp,
                    .imageView = img->image_view,
                    .imageLayout = img_layout,
                };
            }

            writes[write_index++] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = VK_NULL_HANDLE, // Will be set later
                .dstBinding = image.index,
                .dstArrayElement = 0,
                .descriptorCount = static_cast<uint32_t>(image.images.size()),
                .descriptorType = to_vulkan(image.type),
                .pImageInfo = image_write_info,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr,
            };

            // Track the images and samplers for the resource tracker
            for (const auto& img_info : image.images)
            {
                used_images[command_list].push_back(img_info.image);
                if (img_info.sampler)
                {
                    used_samplers[command_list].push_back(img_info.sampler);
                }
            }
        }

        for (const auto& sampler : samplers)
        {
            auto sampler_write_info = _allocator.allocate_typed<VkDescriptorImageInfo>(sampler.samplers.size());
            for (size_t i = 0; i < sampler.samplers.size(); ++i)
            {
                auto smp = _parent->get_sampler(sampler.samplers[i])->sampler;
                sampler_write_info[i] = {
                    .sampler = smp,
                    .imageView = VK_NULL_HANDLE,
                    .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                };
            }
            writes[write_index++] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = VK_NULL_HANDLE, // Will be set later
                .dstBinding = sampler.index,
                .dstArrayElement = 0,
                .descriptorCount = static_cast<uint32_t>(sampler.samplers.size()),
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .pImageInfo = sampler_write_info,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr,
            };
            // Track the samplers for the resource tracker
            used_samplers[command_list].insert(used_samplers[command_list].end(), sampler.samplers.begin(),
                                               sampler.samplers.end());
        }

        _dispatch->cmdPushDescriptorSetKHR(cmds, to_vulkan(point), layout, set_index, write_count, writes);
    }

    void work_queue::push_constants(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                    typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout,
                                    enum_mask<rhi::shader_stage> stages, uint32_t offset,
                                    span<const byte> values) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        auto layout = _parent->get_pipeline_layout(pipeline_layout);
        _dispatch->cmdPushConstants(cmds, layout, to_vulkan(stages), offset, static_cast<uint32_t>(values.size()),
                                    values.data());
    }

    void work_queue::bind_descriptor_buffers(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                             typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout,
                                             bind_point point, uint32_t first_set_index,
                                             span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers,
                                             span<const uint64_t> offsets) noexcept
    {
        TEMPEST_ASSERT(offsets.size() == buffers.size() && "Number of offsets must match number of descriptor buffers");

        auto cmds = _parent->get_command_buffer(command_list);

        // First, gather the used VkBuffer handles into a vector of unique handles
        auto buffer_binding_infos = _allocator.allocate_typed<VkDescriptorBufferBindingInfoEXT>(buffers.size());
        auto unique_buffer_count = 0u;
        auto buffer_binding_indices = _allocator.allocate_typed<uint32_t>(buffers.size());
        auto buffer_bindings_written = 0;

        for (auto buf : buffers)
        {
            auto buffer_payload = _parent->get_buffer(buf);
            auto it = tempest::find_if(buffer_binding_infos, buffer_binding_infos + unique_buffer_count,
                                       [&](const auto& binding_info) {
                                           return binding_info.address == buffer_payload->address &&
                                                  binding_info.usage == buffer_payload->usage;
                                       });

            auto buffer_binding_index = static_cast<uint32_t>(tempest::distance(buffer_binding_infos, it));

            if (it == buffer_binding_infos + unique_buffer_count)
            {
                buffer_binding_infos[unique_buffer_count] = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                    .pNext = nullptr,
                    .address = buffer_payload->address,
                    .usage = buffer_payload->usage,
                };

                unique_buffer_count++;
            }

            buffer_binding_indices[buffer_bindings_written++] = buffer_binding_index;
        }

        _dispatch->cmdBindDescriptorBuffersEXT(cmds, unique_buffer_count, buffer_binding_infos);
        _dispatch->cmdSetDescriptorBufferOffsetsEXT(cmds, to_vulkan(point),
                                                    _parent->get_pipeline_layout(pipeline_layout), first_set_index,
                                                    static_cast<uint32_t>(buffers.size()), buffer_binding_indices, offsets.data());

        _allocator.reset();
    }

    void work_queue::reset(uint64_t frame_in_flight)
    {
        start_frame(static_cast<uint32_t>(frame_in_flight));
    }

    void work_queue::begin_debug_region(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                         string_view name)
    {
        if (!_parent->can_name_objects())
        {
            return;
        }

        const auto label = VkDebugUtilsLabelEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext = nullptr,
            .pLabelName = name.data(),
            .color = {0.0f, 0.0f, 0.0f, 1.0f},
        };

        _dispatch->cmdBeginDebugUtilsLabelEXT(_parent->get_command_buffer(command_list), &label);
    }

    void work_queue::end_debug_region(typed_rhi_handle<rhi_handle_type::command_list> command_list)
    {
        if (!_parent->can_name_objects())
        {
            return;
        }

        _dispatch->cmdEndDebugUtilsLabelEXT(_parent->get_command_buffer(command_list));
    }

    void work_queue::set_debug_marker(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                       string_view name)
    {
        if (!_parent->can_name_objects())
        {
            return;
        }

        const auto label = VkDebugUtilsLabelEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext = nullptr,
            .pLabelName = name.data(),
            .color = {0.0f, 0.0f, 0.0f, 1.0f},
        };

        _dispatch->cmdInsertDebugUtilsLabelEXT(_parent->get_command_buffer(command_list), &label);
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

#if defined(TEMPEST_ENABLE_VALIDATION_LAYERS)
        bldr.request_validation_layers(true)
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

#if defined(TEMPEST_DEBUG_SHADERS)
        bldr.enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        auto result = bldr.build();
        if (!result)
        {
            return nullptr;
        }

        auto instance = tempest::move(result).value();

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

        VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
            .pNext = nullptr,
            .descriptorBuffer = VK_TRUE,
            .descriptorBufferCaptureReplay = VK_TRUE,
            .descriptorBufferImageLayoutIgnored = VK_TRUE,
            .descriptorBufferPushDescriptors = VK_TRUE,
        };

        vkb::PhysicalDeviceSelector selector =
            vkb::PhysicalDeviceSelector(instance)
                .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                .defer_surface_initialization()
                .require_present()
                .add_required_extension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME)
                .add_required_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)
                .add_required_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)
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
                    .uniformAndStorageBuffer16BitAccess = VK_TRUE, // Switch to false when Slang fixes #8760
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
                    .uniformAndStorageBuffer8BitAccess = VK_FALSE,
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
                    .timelineSemaphore = VK_TRUE,
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
                .add_required_extension_features(fragment_shader_interlock)
                .add_required_extension_features(descriptor_buffer_features);

        auto devices = selector.select_devices();
        if (!devices || devices->empty())
        {
            return nullptr;
        }

        vector<vkb::PhysicalDevice> vkb_devices(devices->begin(), devices->end());

        return make_unique<rhi::vk::instance>(tempest::move(instance), tempest::move(vkb_devices));
    }

    descriptor_set_layout_cache::descriptor_set_layout_cache(device* dev) noexcept : _dev{dev}
    {
    }

    typed_rhi_handle<rhi_handle_type::descriptor_set_layout> descriptor_set_layout_cache::get_or_create_layout(
        const vector<descriptor_binding_layout>& desc, enum_mask<descriptor_set_layout_flags> flags) noexcept
    {
        // Check if the layout already exists in the cache
        size_t hash = _compute_cache_hash(desc, flags);

        cache_key key = {
            .desc = desc,
            .flags = flags,
            .hash = hash,
        };

        auto cache_entry_it = _cache.find(key);
        if (cache_entry_it != _cache.end())
        {
            auto sk = cache_entry_it->second;
            auto& cache_value = *_cache_slots.find(create_slot_map_key<uint64_t>(sk.id, sk.generation));
            cache_value.ref_count++;
            return sk;
        }

        // Build the descriptor set layout
        auto bindings = _allocator.allocate_typed<VkDescriptorSetLayoutBinding>(desc.size());
        auto binding_flags = _allocator.allocate_typed<VkDescriptorBindingFlags>(desc.size());

        for (size_t i = 0; i < desc.size(); ++i)
        {
            bindings[i] = {
                .binding = desc[i].binding_index,
                .descriptorType = to_vulkan(desc[i].type),
                .descriptorCount = desc[i].count,
                .stageFlags = to_vulkan(desc[i].stages),
                .pImmutableSamplers = nullptr,
            };

            binding_flags[i] = to_vulkan(desc[i].flags);
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = static_cast<uint32_t>(desc.size()),
            .pBindingFlags = binding_flags,
        };

        VkDescriptorSetLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &binding_flags_info,
            .flags = to_vulkan(flags),
            .bindingCount = static_cast<uint32_t>(desc.size()),
            .pBindings = bindings,
        };

        VkDescriptorSetLayout layout;
        auto result = _dev->get_dispatch_table().createDescriptorSetLayout(&layout_info, nullptr, &layout);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create descriptor set layout: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::descriptor_set_layout>::null_handle;
        }

        // Create a new cache entry
        slot_entry entry = {
            .key = tempest::move(key),
            .layout = layout,
            .ref_count = 1,
        };

        auto slot = _cache_slots.insert(entry);

        auto typed_key = typed_rhi_handle<rhi_handle_type::descriptor_set_layout>{
            .id = get_slot_map_key_id(slot),
            .generation = get_slot_map_key_generation(slot),
        };

        _cache[key] = typed_key;

        return typed_key;
    }

    bool descriptor_set_layout_cache::release_layout(
        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> layout) noexcept
    {
        auto key = create_slot_map_key<uint64_t>(layout.id, layout.generation);

        auto cache_entry_it = _cache_slots.find(key);
        if (cache_entry_it == _cache_slots.end())
        {
            logger->error("Failed to release descriptor set layout: layout not found in cache");
            return false;
        }

        slot_entry& cache_value = *cache_entry_it;
        cache_value.ref_count--;

        // If the ref count is zero, add it to the deletion queue
        if (cache_value.ref_count == 0)
        {
            _dev->get_dispatch_table().destroyDescriptorSetLayout(cache_value.layout, nullptr);

            _cache.erase(cache_value.key);
            _cache_slots.erase(key);

            return true;
        }

        return false;
    }

    VkDescriptorSetLayout descriptor_set_layout_cache::get_layout(
        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto cache_entry_it = _cache_slots.find(key);
        if (cache_entry_it == _cache_slots.end())
        {
            logger->error("Failed to get descriptor set layout: layout not found in cache");
            return VK_NULL_HANDLE;
        }
        return cache_entry_it->layout;
    }

    void descriptor_set_layout_cache::destroy() noexcept
    {
        for (auto& [key, layout, ref_count] : _cache_slots)
        {
            _dev->get_dispatch_table().destroyDescriptorSetLayout(layout, nullptr);
        }
        _cache_slots.clear();
        _cache.clear();
    }

    bool descriptor_set_layout_cache::add_usage(
        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto cache_entry_it = _cache_slots.find(key);
        if (cache_entry_it == _cache_slots.end())
        {
            logger->error("Failed to add usage: layout not found in cache");
            return false;
        }
        slot_entry& cache_value = *cache_entry_it;
        cache_value.ref_count++;
        return true;
    }

    size_t descriptor_set_layout_cache::_compute_cache_hash(const vector<descriptor_binding_layout>& desc,
                                                            enum_mask<descriptor_set_layout_flags> flags) const noexcept
    {
        size_t hash = 0;
        for (const auto& binding : desc)
        {
            hash = hash_combine(hash, binding.binding_index, to_underlying(binding.type), binding.count,
                                to_underlying(binding.stages.value()), to_underlying(binding.flags.value()));
        }
        hash = hash_combine(hash, to_underlying(flags.value()));
        return hash;
    }

    pipeline_layout_cache::pipeline_layout_cache(device* dev) noexcept : _dev{dev}
    {
    }

    typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout_cache::get_or_create_layout(
        const pipeline_layout_desc& desc) noexcept
    {
        size_t hash = _compute_cache_hash(desc);
        cache_key key = {
            .desc = desc,
            .hash = hash,
        };

        auto cache_entry_it = _cache.find(key);
        if (cache_entry_it != _cache.end())
        {
            auto sk = cache_entry_it->second;
            auto& cache_value = *_cache_slots.find(create_slot_map_key<uint64_t>(sk.id, sk.generation));
            cache_value.ref_count++;
            return sk;
        }

        // Build the pipeline layout
        auto set_layouts = desc.descriptor_set_layouts.size() == 0 // not using empty because intellisense is dumb
                               ? nullptr
                               : _allocator.allocate_typed<VkDescriptorSetLayout>(desc.descriptor_set_layouts.size());
        for (size_t i = 0; i < desc.descriptor_set_layouts.size(); ++i)
        {
            set_layouts[i] = _dev->get_descriptor_set_layout(desc.descriptor_set_layouts[i]);
        }

        auto push_constant_ranges = desc.push_constants.empty()
                                        ? nullptr
                                        : _allocator.allocate_typed<VkPushConstantRange>(desc.push_constants.size());
        for (size_t i = 0; i < desc.push_constants.size(); ++i)
        {
            push_constant_ranges[i] = {
                .stageFlags = to_vulkan(desc.push_constants[i].stages),
                .offset = desc.push_constants[i].offset,
                .size = desc.push_constants[i].range,
            };
        }

        VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = static_cast<uint32_t>(desc.descriptor_set_layouts.size()),
            .pSetLayouts = set_layouts,
            .pushConstantRangeCount = static_cast<uint32_t>(desc.push_constants.size()),
            .pPushConstantRanges = push_constant_ranges,
        };

        VkPipelineLayout layout;
        auto result = _dev->get_dispatch_table().createPipelineLayout(&layout_info, nullptr, &layout);

        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create pipeline layout: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::pipeline_layout>::null_handle;
        }

        // Create a new cache entry
        slot_entry entry = {
            .key = tempest::move(key),
            .layout = layout,
            .ref_count = 1,
        };

        auto slot = _cache_slots.insert(entry);
        auto typed_key = typed_rhi_handle<rhi_handle_type::pipeline_layout>{
            .id = get_slot_map_key_id(slot),
            .generation = get_slot_map_key_generation(slot),
        };

        _cache[key] = typed_key;
        return typed_key;
    }

    VkPipelineLayout pipeline_layout_cache::get_layout(
        typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto cache_entry_it = _cache_slots.find(key);
        if (cache_entry_it == _cache_slots.end())
        {
            logger->error("Failed to get pipeline layout: layout not found in cache");
            return VK_NULL_HANDLE;
        }
        return cache_entry_it->layout;
    }

    void pipeline_layout_cache::destroy() noexcept
    {
        for (auto& [key, layout, ref_count] : _cache_slots)
        {
            _dev->get_dispatch_table().destroyPipelineLayout(layout, nullptr);
        }
        _cache_slots.clear();
        _cache.clear();
    }

    size_t pipeline_layout_cache::_compute_cache_hash(const pipeline_layout_desc& desc) const noexcept
    {
        size_t hc = 0;

        for (const auto& set_layout : desc.descriptor_set_layouts)
        {
            hc = hash_combine(hc, set_layout);
        }

        for (const auto& push_constant : desc.push_constants)
        {
            hc = hash_combine(hc, to_underlying(push_constant.stages.value()), push_constant.offset,
                              push_constant.range);
        }

        return hc;
    }

    bool pipeline_layout_cache::release_layout(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto cache_entry_it = _cache_slots.find(key);
        if (cache_entry_it == _cache_slots.end())
        {
            logger->error("Failed to release pipeline layout: layout not found in cache");
            return false;
        }
        slot_entry& cache_value = *cache_entry_it;
        cache_value.ref_count--;
        // If the ref count is zero, add it to the deletion queue
        if (cache_value.ref_count == 0)
        {
            _dev->get_dispatch_table().destroyPipelineLayout(cache_value.layout, nullptr);

            // Release the descriptor set layouts
            for (auto& set_layout : cache_value.key.desc.descriptor_set_layouts)
            {
                _dev->release_resource_immediate(set_layout);
            }

            _cache.erase(cache_value.key);
            _cache_slots.erase(key);
            return true;
        }
        return false;
    }

    bool pipeline_layout_cache::add_usage(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto cache_entry_it = _cache_slots.find(key);
        if (cache_entry_it == _cache_slots.end())
        {
            logger->error("Failed to add usage: layout not found in cache");
            return false;
        }
        slot_entry& cache_value = *cache_entry_it;
        cache_value.ref_count++;
        return true;
    }
} // namespace tempest::rhi::vk
