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

        constexpr VkAttachmentLoadOp to_vulkan(rhi::work_queue::load_op op)
        {
            switch (op)
            {
            case rhi::work_queue::load_op::LOAD:
                return VK_ATTACHMENT_LOAD_OP_LOAD;
            case rhi::work_queue::load_op::CLEAR:
                return VK_ATTACHMENT_LOAD_OP_CLEAR;
            case rhi::work_queue::load_op::DONT_CARE:
                return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            }
            unreachable();
        }

        constexpr VkAttachmentStoreOp to_vulkan(rhi::work_queue::store_op op)
        {
            switch (op)
            {
            case rhi::work_queue::store_op::STORE:
                return VK_ATTACHMENT_STORE_OP_STORE;
            case rhi::work_queue::store_op::DONT_CARE:
                return VK_ATTACHMENT_STORE_OP_DONT_CARE;
            }
            unreachable();
        }

        constexpr VkDescriptorType to_vulkan(rhi::descriptor_type type)
        {
            switch (type)
            {
            case rhi::descriptor_type::SAMPLER:
                return VK_DESCRIPTOR_TYPE_SAMPLER;
            case rhi::descriptor_type::COMBINED_IMAGE_SAMPLER:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case rhi::descriptor_type::SAMPLED_IMAGE:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case rhi::descriptor_type::STORAGE_IMAGE:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case rhi::descriptor_type::UNIFORM_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case rhi::descriptor_type::STORAGE_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case rhi::descriptor_type::UNIFORM_TEXEL_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            case rhi::descriptor_type::STORAGE_TEXEL_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            case rhi::descriptor_type::INPUT_ATTACHMENT:
                return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            }
            unreachable();
        }

        constexpr VkShaderStageFlags to_vulkan(enum_mask<rhi::shader_stage> stages)
        {
            VkShaderStageFlags flags = 0;

            if (stages & rhi::shader_stage::VERTEX)
            {
                flags |= VK_SHADER_STAGE_VERTEX_BIT;
            }

            if (stages & rhi::shader_stage::TESSELLATION_CONTROL)
            {
                flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            }

            if (stages & rhi::shader_stage::TESSELLATION_EVALUATION)
            {
                flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            }

            if (stages & rhi::shader_stage::GEOMETRY)
            {
                flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
            }

            if (stages & rhi::shader_stage::FRAGMENT)
            {
                flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            if (stages & rhi::shader_stage::COMPUTE)
            {
                flags |= VK_SHADER_STAGE_COMPUTE_BIT;
            }

            return flags;
        }

        constexpr VkDescriptorBindingFlags to_vulkan(enum_mask<rhi::descriptor_binding_flags> flags)
        {
            VkDescriptorBindingFlags vk_flags = 0;
            if (flags & rhi::descriptor_binding_flags::PARTIALLY_BOUND)
            {
                vk_flags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            }
            if (flags & rhi::descriptor_binding_flags::VARIABLE_LENGTH)
            {
                vk_flags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            }
            return vk_flags;
        }

        constexpr VkPrimitiveTopology to_vulkan(rhi::primitive_topology topo)
        {
            switch (topo)
            {
            case rhi::primitive_topology::POINT_LIST:
                return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case rhi::primitive_topology::LINE_LIST:
                return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case rhi::primitive_topology::LINE_STRIP:
                return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case rhi::primitive_topology::TRIANGLE_LIST:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case rhi::primitive_topology::TRIANGLE_STRIP:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case rhi::primitive_topology::TRIANGLE_FAN:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
            }
            unreachable();
        }

        constexpr VkFrontFace to_vulkan(rhi::vertex_winding face)
        {
            switch (face)
            {
            case rhi::vertex_winding::COUNTER_CLOCKWISE:
                return VK_FRONT_FACE_COUNTER_CLOCKWISE;
            case rhi::vertex_winding::CLOCKWISE:
                return VK_FRONT_FACE_CLOCKWISE;
            }
            unreachable();
        }

        constexpr VkCullModeFlags to_vulkan(enum_mask<rhi::cull_mode> mode)
        {
            VkCullModeFlags flags = 0;
            if (mode & rhi::cull_mode::FRONT)
            {
                flags |= VK_CULL_MODE_FRONT_BIT;
            }
            if (mode & rhi::cull_mode::BACK)
            {
                flags |= VK_CULL_MODE_BACK_BIT;
            }
            return flags;
        }

        constexpr VkPolygonMode to_vulkan(rhi::polygon_mode mode)
        {
            switch (mode)
            {
            case rhi::polygon_mode::FILL:
                return VK_POLYGON_MODE_FILL;
            case rhi::polygon_mode::LINE:
                return VK_POLYGON_MODE_LINE;
            case rhi::polygon_mode::POINT:
                return VK_POLYGON_MODE_POINT;
            }
            unreachable();
        }

        constexpr VkCompareOp to_vulkan(rhi::compare_op op)
        {
            switch (op)
            {
            case rhi::compare_op::NEVER:
                return VK_COMPARE_OP_NEVER;
            case rhi::compare_op::LESS:
                return VK_COMPARE_OP_LESS;
            case rhi::compare_op::EQUAL:
                return VK_COMPARE_OP_EQUAL;
            case rhi::compare_op::LESS_EQUAL:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
            case rhi::compare_op::GREATER:
                return VK_COMPARE_OP_GREATER;
            case rhi::compare_op::NOT_EQUAL:
                return VK_COMPARE_OP_NOT_EQUAL;
            case rhi::compare_op::GREATER_EQUAL:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case rhi::compare_op::ALWAYS:
                return VK_COMPARE_OP_ALWAYS;
            }
            unreachable();
        }

        constexpr VkStencilOp to_vulkan(rhi::stencil_op op)
        {
            switch (op)
            {
            case rhi::stencil_op::KEEP:
                return VK_STENCIL_OP_KEEP;
            case rhi::stencil_op::ZERO:
                return VK_STENCIL_OP_ZERO;
            case rhi::stencil_op::REPLACE:
                return VK_STENCIL_OP_REPLACE;
            case rhi::stencil_op::INCREMENT_AND_CLAMP:
                return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
            case rhi::stencil_op::DECREMENT_AND_CLAMP:
                return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
            case rhi::stencil_op::INVERT:
                return VK_STENCIL_OP_INVERT;
            case rhi::stencil_op::INCREMENT_AND_WRAP:
                return VK_STENCIL_OP_INCREMENT_AND_WRAP;
            case rhi::stencil_op::DECREMENT_AND_WRAP:
                return VK_STENCIL_OP_DECREMENT_AND_WRAP;
            }
            unreachable();
        }

        constexpr VkBlendFactor to_vulkan(rhi::blend_factor factor)
        {
            switch (factor)
            {
            case rhi::blend_factor::ZERO:
                return VK_BLEND_FACTOR_ZERO;
            case rhi::blend_factor::ONE:
                return VK_BLEND_FACTOR_ONE;
            case rhi::blend_factor::SRC_COLOR:
                return VK_BLEND_FACTOR_SRC_COLOR;
            case rhi::blend_factor::ONE_MINUS_SRC_COLOR:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case rhi::blend_factor::DST_COLOR:
                return VK_BLEND_FACTOR_DST_COLOR;
            case rhi::blend_factor::ONE_MINUS_DST_COLOR:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case rhi::blend_factor::SRC_ALPHA:
                return VK_BLEND_FACTOR_SRC_ALPHA;
            case rhi::blend_factor::ONE_MINUS_SRC_ALPHA:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case rhi::blend_factor::DST_ALPHA:
                return VK_BLEND_FACTOR_DST_ALPHA;
            case rhi::blend_factor::ONE_MINUS_DST_ALPHA:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case rhi::blend_factor::CONSTANT_COLOR:
                return VK_BLEND_FACTOR_CONSTANT_COLOR;
            case rhi::blend_factor::ONE_MINUS_CONSTANT_COLOR:
                return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
            case rhi::blend_factor::CONSTANT_ALPHA:
                return VK_BLEND_FACTOR_CONSTANT_ALPHA;
            case rhi::blend_factor::ONE_MINUS_CONSTANT_ALPHA:
                return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
            }
            unreachable();
        }

        constexpr VkBlendOp to_vulkan(rhi::blend_op op)
        {
            switch (op)
            {
            case rhi::blend_op::ADD:
                return VK_BLEND_OP_ADD;
            case rhi::blend_op::SUBTRACT:
                return VK_BLEND_OP_SUBTRACT;
            case rhi::blend_op::REVERSE_SUBTRACT:
                return VK_BLEND_OP_REVERSE_SUBTRACT;
            case rhi::blend_op::MIN:
                return VK_BLEND_OP_MIN;
            case rhi::blend_op::MAX:
                return VK_BLEND_OP_MAX;
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
        case VK_OBJECT_TYPE_PIPELINE:
            dispatch->destroyPipeline(static_cast<VkPipeline>(res.handle), nullptr);
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
          _resource_tracker{this, _dispatch_table}, _descriptor_set_layout_cache{this}, _pipeline_layout_cache{this}
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
    }

    device::~device()
    {
        _dispatch_table.deviceWaitIdle();

        _primary_work_queue = nullopt;
        _dedicated_compute_queue = nullopt;
        _dedicated_transfer_queue = nullopt;

        _delete_queue.destroy();

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

    typed_rhi_handle<rhi_handle_type::BUFFER> device::create_buffer(const buffer_desc& desc) noexcept
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
            return typed_rhi_handle<rhi_handle_type::BUFFER>::null_handle;
        }

        vk::buffer buf = {
            .allocation = allocation,
            .allocation_info = allocation_info,
            .buffer = buffer,
        };

        auto new_key = _buffers.insert(buf);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::BUFFER>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    typed_rhi_handle<rhi_handle_type::IMAGE> device::create_image(const image_desc& desc) noexcept
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
            return typed_rhi_handle<rhi_handle_type::IMAGE>::null_handle;
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
            return typed_rhi_handle<rhi_handle_type::IMAGE>::null_handle;
        }

        vk::image img = {
            .allocation = allocation,
            .allocation_info = allocation_info,
            .image = image,
            .image_view = image_view,
            .swapchain_image = false,
            .image_aspect = view_ci.subresourceRange.aspectMask,
            .create_info = ci,
        };

        auto new_key = _images.insert(img);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::IMAGE>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    typed_rhi_handle<rhi_handle_type::FENCE> device::create_fence(const fence_info& info) noexcept
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
            return typed_rhi_handle<rhi_handle_type::FENCE>::null_handle;
        }

        vk::fence new_fence = {
            .fence = fence,
        };

        auto new_key = _fences.insert(new_fence);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::FENCE>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    typed_rhi_handle<rhi_handle_type::SEMAPHORE> device::create_semaphore(const semaphore_info& info) noexcept
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
            return typed_rhi_handle<rhi_handle_type::SEMAPHORE>::null_handle;
        }

        vk::semaphore new_semaphore = {
            .semaphore = semaphore,
            .type = info.type,
        };

        auto new_key = _semaphores.insert(new_semaphore);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::SEMAPHORE>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> device::create_render_surface(
        const render_surface_desc& desc) noexcept
    {
        return create_render_surface(desc, typed_rhi_handle<rhi_handle_type::RENDER_SURFACE>::null_handle);
    }

    typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT> device::create_descriptor_set_layout(
        const vector<descriptor_binding_layout>& desc) noexcept
    {
        return _descriptor_set_layout_cache.get_or_create_layout(desc);
    }

    typed_rhi_handle<rhi_handle_type::PIPELINE_LAYOUT> device::create_pipeline_layout(
        const pipeline_layout_desc& desc) noexcept
    {
        return _pipeline_layout_cache.get_or_create_layout(desc);
    }

    typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE> device::create_graphics_pipeline(
        const graphics_pipeline_desc& desc) noexcept
    {
        auto pipeline_layout = _pipeline_layout_cache.get_layout(desc.layout);
        if (!pipeline_layout)
        {
            logger->error("Failed to create graphics pipeline: invalid pipeline layout");
            return typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE>::null_handle;
        }

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
                return typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE>::null_handle;
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
                return typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE>::null_handle;
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
                return typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE>::null_handle;
            }
            shader_modules.push_back(mod);
            shader_stages.push_back(stage_ci);
        }

        if (!desc.geometry_shader.empty())
        {
            auto [mod, stage_ci] = create_shader_stage(desc.geometry_shader, VK_SHADER_STAGE_GEOMETRY_BIT);
            if (mod == VkShaderModule{})
            {
                return typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE>::null_handle;
            }
            shader_modules.push_back(mod);
            shader_stages.push_back(stage_ci);
        }

        if (!desc.fragment_shader.empty())
        {
            auto [mod, stage_ci] = create_shader_stage(desc.fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT);
            if (mod == VkShaderModule{})
            {
                return typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE>::null_handle;
            }
            shader_modules.push_back(mod);
            shader_stages.push_back(stage_ci);
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr,
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
            VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
            VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
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

        VkGraphicsPipelineCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &pipeline_rendering_ci,
            .flags = 0,
            .stageCount = static_cast<uint32_t>(shader_stages.size()),
            .pStages = shader_stages.data(),
            .pVertexInputState = &vertex_input_ci,
            .pInputAssemblyState = &input_assembly_ci,
            .pTessellationState = desc.tessellation ? &tessellation_ci : nullptr,
            .pViewportState = nullptr,
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
            return typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE>::null_handle;
        }

        graphics_pipeline gp = {
            .shader_modules = tempest::move(shader_modules),
            .pipeline = pipeline,
            .layout = pipeline_layout,
            .desc = tempest::move(desc),
        };

        auto new_key = _graphics_pipelines.insert(gp);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE>{
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    void device::destroy_buffer(typed_rhi_handle<rhi_handle_type::BUFFER> handle) noexcept
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

    void device::destroy_image(typed_rhi_handle<rhi_handle_type::IMAGE> handle) noexcept
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

    void device::destroy_fence(typed_rhi_handle<rhi_handle_type::FENCE> handle) noexcept
    {
        auto fence_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto fence_it = _fences.find(fence_key);
        if (fence_it != _fences.end())
        {
            _delete_queue.enqueue(VK_OBJECT_TYPE_FENCE, fence_it->fence, _current_frame + num_frames_in_flight);
            _fences.erase(fence_key);
        }
    }

    void device::destroy_semaphore(typed_rhi_handle<rhi_handle_type::SEMAPHORE> handle) noexcept
    {
        auto sem_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto sem_it = _semaphores.find(sem_key);
        if (sem_it != _semaphores.end())
        {
            _delete_queue.enqueue(VK_OBJECT_TYPE_SEMAPHORE, sem_it->semaphore, _current_frame + num_frames_in_flight);
            _semaphores.erase(sem_key);
        }
    }

    void device::destroy_render_surface(typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> handle) noexcept
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

    void device::destroy_descriptor_set_layout(typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT> handle) noexcept
    {
        _descriptor_set_layout_cache.release_layout(handle);
    }

    void device::destroy_pipeline_layout(typed_rhi_handle<rhi_handle_type::PIPELINE_LAYOUT> handle) noexcept
    {
        _pipeline_layout_cache.release_layout(handle);
    }

    void device::destroy_graphics_pipeline(typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE> handle) noexcept
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

    void device::recreate_render_surface(typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> handle,
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

    span<const typed_rhi_handle<rhi_handle_type::IMAGE>> device::get_render_surfaces(
        [[maybe_unused]] typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> handle) noexcept
    {
        return span<const typed_rhi_handle<rhi_handle_type::IMAGE>>{};
    }

    expected<swapchain_image_acquire_info_result, swapchain_error_code> device::acquire_next_image(
        typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> swapchain,
        typed_rhi_handle<rhi_handle_type::FENCE> signal_fence) noexcept
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
                .render_complete_sem = fif.render_complete,
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

    bool device::is_signaled(typed_rhi_handle<rhi_handle_type::FENCE> fence) const noexcept
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

    bool device::reset(span<const typed_rhi_handle<rhi_handle_type::FENCE>> fences) const noexcept
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

    bool device::wait(span<const typed_rhi_handle<rhi_handle_type::FENCE>> fences) const noexcept
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

        _resource_tracker.try_release();

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

    uint32_t device::frame_in_flight() const noexcept
    {
        return _current_frame % frames_in_flight();
    }

    uint32_t device::frames_in_flight() const noexcept
    {
        return num_frames_in_flight;
    }

    typed_rhi_handle<rhi_handle_type::IMAGE> device::acquire_image(image img) noexcept
    {
        auto new_key = _images.insert(img);

        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::IMAGE>(new_key_id, new_key_gen);
    }

    typed_rhi_handle<rhi_handle_type::COMMAND_LIST> device::acquire_command_list(VkCommandBuffer buf) noexcept
    {
        auto new_key = _command_buffers.insert(buf);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return {
            .id = new_key_id,
            .generation = new_key_gen,
        };
    }

    VkCommandBuffer device::get_command_buffer(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> handle) const noexcept
    {
        auto buf_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto buf_it = _command_buffers.find(buf_key);
        if (buf_it != _command_buffers.end())
        {
            return *buf_it;
        }

        return VK_NULL_HANDLE;
    }

    void device::release_command_list(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> handle) noexcept
    {
        auto buf_key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto buf_it = _command_buffers.find(buf_key);
        if (buf_it != _command_buffers.end())
        {
            _command_buffers.erase(buf_key);
        }
    }

    typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> device::create_render_surface(
        const rhi::render_surface_desc& desc, typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> old_swapchain) noexcept
    {
        auto window = static_cast<vk::window_surface*>(desc.window);
        auto surf_res = window->get_surface(_vkb_instance->instance);
        if (!surf_res)
        {
            logger->error("Failed to create render surface for window: {}", desc.window->name().c_str());
            return typed_rhi_handle<rhi_handle_type::RENDER_SURFACE>::null_handle;
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
            return typed_rhi_handle<rhi_handle_type::RENDER_SURFACE>::null_handle;
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
            return typed_rhi_handle<rhi_handle_type::RENDER_SURFACE>::null_handle;
        }

        auto images = images_result.value();

        auto image_views_result = sc.swapchain.get_image_views();
        if (!image_views_result)
        {
            return typed_rhi_handle<rhi_handle_type::RENDER_SURFACE>::null_handle;
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
                // Allocate two semaphores for each frame in flight

                auto fence = create_fence({
                    .signaled = true,
                });

                auto image_acquired = create_semaphore({
                    .type = rhi::semaphore_type::BINARY,
                    .initial_value = 0,
                });

                auto render_complete = create_semaphore({
                    .type = rhi::semaphore_type::BINARY,
                    .initial_value = 0,
                });

                fif_data fif = {
                    .frame_ready = fence,
                    .image_acquired = image_acquired,
                    .render_complete = render_complete,
                };

                sc.frames.push_back(fif);
            }
        }

        auto new_key = _swapchains.insert(sc);
        auto new_key_id = get_slot_map_key_id<uint64_t>(new_key);
        auto new_key_gen = get_slot_map_key_generation<uint64_t>(new_key);

        return typed_rhi_handle<rhi_handle_type::RENDER_SURFACE>(new_key_id, new_key_gen);
    }

    VkFence device::get_fence(typed_rhi_handle<rhi_handle_type::FENCE> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _fences.find(key);
        if (it != _fences.end())
        {
            return it->fence;
        }
        return VK_NULL_HANDLE;
    }

    VkSemaphore device::get_semaphore(typed_rhi_handle<rhi_handle_type::SEMAPHORE> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _semaphores.find(key);
        if (it != _semaphores.end())
        {
            return it->semaphore;
        }
        return VK_NULL_HANDLE;
    }

    VkSwapchainKHR device::get_swapchain(typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _swapchains.find(key);
        if (it != _swapchains.end())
        {
            return it->swapchain;
        }
        return VK_NULL_HANDLE;
    }

    VkDescriptorSetLayout device::get_descriptor_set_layout(
        typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT> handle) const noexcept
    {
        return _descriptor_set_layout_cache.get_layout(handle);
    }

    optional<const vk::buffer&> device::get_buffer(typed_rhi_handle<rhi_handle_type::BUFFER> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _buffers.find(key);
        if (it != _buffers.end())
        {
            return *it;
        }
        return none();
    }

    optional<const vk::image&> device::get_image(typed_rhi_handle<rhi_handle_type::IMAGE> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _images.find(key);
        if (it != _images.end())
        {
            return *it;
        }
        return none();
    }

    optional<const vk::graphics_pipeline&> device::get_graphics_pipeline(
        typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE> handle) const noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _graphics_pipelines.find(key);
        if (it != _graphics_pipelines.end())
        {
            return *it;
        }

        return none();
    }

    void device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::BUFFER> handle) noexcept
    {
        auto key = create_slot_map_key<uint64_t>(handle.id, handle.generation);
        auto it = _buffers.find(key);
        if (it != _buffers.end())
        {
            vmaDestroyBuffer(_vma_allocator, it->buffer, it->allocation);
            _buffers.erase(key);
        }
    }

    void device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::IMAGE> handle) noexcept
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

    bool device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT> handle) noexcept
    {
        return _descriptor_set_layout_cache.release_layout(handle);
    }

    bool device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::PIPELINE_LAYOUT> handle) noexcept
    {
        return _pipeline_layout_cache.release_layout(handle);
    }

    void device::release_resource_immediate(typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE> handle) noexcept
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

    flat_unordered_map<const work_queue*, uint64_t> device::compute_current_work_queue_timeline_values() const noexcept
    {
        flat_unordered_map<const work_queue*, uint64_t> timeline_values;

        if (_primary_work_queue)
        {
            timeline_values[&*_primary_work_queue] = _primary_work_queue->query_completed_timeline_value();
        }

        if (_dedicated_compute_queue)
        {
            timeline_values[&*_dedicated_compute_queue] = _dedicated_compute_queue->query_completed_timeline_value();
        }

        if (_dedicated_transfer_queue)
        {
            timeline_values[&*_dedicated_transfer_queue] = _dedicated_transfer_queue->query_completed_timeline_value();
        }

        return timeline_values;
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

    typed_rhi_handle<rhi_handle_type::COMMAND_LIST> work_queue::get_next_command_list() noexcept
    {
        auto& wg = _work_groups[_parent->frame_in_flight()];
        return wg.acquire_next_command_buffer();
    }

    bool work_queue::submit(span<const submit_info> infos, typed_rhi_handle<rhi_handle_type::FENCE> fence) noexcept
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

                for (const auto& pipe : used_gfx_pipelines[cmd_list])
                {
                    _res_tracker->track(pipe, timestamp, this);
                }
                used_gfx_pipelines.erase(cmd_list);
            }
        }

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

    void work_queue::begin_command_list(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
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

    void work_queue::end_command_list(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list) noexcept
    {
        _dispatch->endCommandBuffer(_parent->get_command_buffer(command_list));
    }

    void work_queue::transition_image(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
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

    void work_queue::clear_color_image(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                                       typed_rhi_handle<rhi_handle_type::IMAGE> image, image_layout layout, float r,
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

    void work_queue::blit(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                          typed_rhi_handle<rhi_handle_type::IMAGE> src,
                          typed_rhi_handle<rhi_handle_type::IMAGE> dst) noexcept
    {
        VkImageBlit blit_region = {
            .srcSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
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
                    .mipLevel = 0,
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
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _parent->get_image(dst)->image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit_region, VK_FILTER_LINEAR);

        // Track the images for the resource tracker
        used_images[command_list].push_back(src);
        used_images[command_list].push_back(dst);
    }

    void work_queue::generate_mip_chain(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                                        typed_rhi_handle<rhi_handle_type::IMAGE> img, image_layout current_layout,
                                        uint32_t base_mip, uint32_t mip_count) noexcept
    {
        // Transition image to general layout
        // Source should wait for all previous operations to complete

        auto image = _parent->get_image(img);

        VkImageMemoryBarrier2 pre_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = 0,
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
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
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
    }

    void work_queue::copy(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                          typed_rhi_handle<rhi_handle_type::BUFFER> src, typed_rhi_handle<rhi_handle_type::BUFFER> dst,
                          size_t src_offset, size_t dst_offset, size_t byte_count) noexcept
    {
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

    void work_queue::fill(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                          typed_rhi_handle<rhi_handle_type::BUFFER> handle, size_t offset, size_t size,
                          uint32_t data) noexcept
    {
        _dispatch->cmdFillBuffer(_parent->get_command_buffer(command_list), _parent->get_buffer(handle)->buffer, offset,
                                 size, data);

        // Track the buffer for the resource tracker
        used_buffers[command_list].push_back(handle);
    }

    void work_queue::pipeline_barriers(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
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

    void work_queue::begin_rendering(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
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

    void work_queue::end_rendering(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        _dispatch->cmdEndRendering(cmds);
    }

    void work_queue::bind(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                          typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE> pipeline) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        auto pipe = _parent->get_graphics_pipeline(pipeline);
        _dispatch->cmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->pipeline);

        // Track the pipeline for the resource tracker
        used_gfx_pipelines[command_list].push_back(pipeline);
    }

    void work_queue::draw(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                          typed_rhi_handle<rhi_handle_type::BUFFER> indirect_buffer, uint32_t draw_count,
                          uint32_t stride) noexcept
    {
        auto cmds = _parent->get_command_buffer(command_list);
        auto buf = _parent->get_buffer(indirect_buffer);
        _dispatch->cmdDrawIndirectCount(cmds, buf->buffer, 0, buf->buffer, 0, draw_count, stride);

        // Track the buffer for the resource tracker
        used_buffers[command_list].push_back(indirect_buffer);
    }

    void work_group::reset() noexcept
    {
        current_buffer_index = -1;
        dispatch->resetCommandPool(pool, 0);
    }

    typed_rhi_handle<rhi_handle_type::COMMAND_LIST> work_group::acquire_next_command_buffer() noexcept
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
                return typed_rhi_handle<rhi_handle_type::COMMAND_LIST>::null_handle;
            }

            for (auto cmd : cmds)
            {
                cmd_buffers.push_back(cmd);
                cmd_buffer_handles.push_back(parent->acquire_command_list(cmd));
            }
        }

        return cmd_buffer_handles[current_buffer_index];
    }

    optional<typed_rhi_handle<rhi_handle_type::COMMAND_LIST>> work_group::current_command_buffer() const noexcept
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

        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
            .pNext = nullptr,
            .extendedDynamicState = VK_TRUE,
        };

        VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extended_dynamic_state3 = {
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
                .add_required_extension_features(extended_dynamic_state)
                .add_required_extension_features(extended_dynamic_state3)
                .add_required_extension_features(fragment_shader_interlock);

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

    typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT> descriptor_set_layout_cache::get_or_create_layout(
        const vector<descriptor_binding_layout>& desc) noexcept
    {
        // Check if the layout already exists in the cache
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
            .flags = 0,
            .bindingCount = static_cast<uint32_t>(desc.size()),
            .pBindings = bindings,
        };

        VkDescriptorSetLayout layout;
        auto result = _dev->get_dispatch_table().createDescriptorSetLayout(&layout_info, nullptr, &layout);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create descriptor set layout: {}", to_underlying(result));
            return typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT>::null_handle;
        }

        // Create a new cache entry
        slot_entry entry = {
            .key = tempest::move(key),
            .layout = layout,
            .ref_count = 1,
        };

        auto slot = _cache_slots.insert(entry);

        auto typed_key = typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT>{
            .id = get_slot_map_key_id(slot),
            .generation = get_slot_map_key_generation(slot),
        };

        _cache[key] = typed_key;

        return typed_key;
    }

    bool descriptor_set_layout_cache::release_layout(
        typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT> layout) noexcept
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
        typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT> handle) const noexcept
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
        typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT> handle) noexcept
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

    size_t descriptor_set_layout_cache::_compute_cache_hash(
        const vector<descriptor_binding_layout>& desc) const noexcept
    {
        size_t hash = 0;
        for (const auto& binding : desc)
        {
            hash = hash_combine(hash, binding.binding_index, to_underlying(binding.type), binding.count,
                                to_underlying(binding.stages.value()), to_underlying(binding.flags.value()));
        }
        return hash;
    }

    pipeline_layout_cache::pipeline_layout_cache(device* dev) noexcept : _dev{dev}
    {
    }

    typed_rhi_handle<rhi_handle_type::PIPELINE_LAYOUT> pipeline_layout_cache::get_or_create_layout(
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
        auto set_layouts = desc.descriptor_set_layouts.empty()
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
            return typed_rhi_handle<rhi_handle_type::PIPELINE_LAYOUT>::null_handle;
        }

        // Create a new cache entry
        slot_entry entry = {
            .key = tempest::move(key),
            .layout = layout,
            .ref_count = 1,
        };

        auto slot = _cache_slots.insert(entry);
        auto typed_key = typed_rhi_handle<rhi_handle_type::PIPELINE_LAYOUT>{
            .id = get_slot_map_key_id(slot),
            .generation = get_slot_map_key_generation(slot),
        };

        _cache[key] = typed_key;
        return typed_key;
    }

    VkPipelineLayout pipeline_layout_cache::get_layout(
        typed_rhi_handle<rhi_handle_type::PIPELINE_LAYOUT> handle) const noexcept
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

    bool pipeline_layout_cache::release_layout(typed_rhi_handle<rhi_handle_type::PIPELINE_LAYOUT> handle) noexcept
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

    bool pipeline_layout_cache::add_usage(typed_rhi_handle<rhi_handle_type::PIPELINE_LAYOUT> handle) noexcept
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
