#ifndef tempest_graphics_resources_hpp__
#define tempest_graphics_resources_hpp__

#include "enums.hpp"
#include "fwd.hpp"

#include <tempest/memory.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace tempest::graphics
{
    struct buffer_handle
    {
        resource_handle index;

        inline constexpr operator bool() const noexcept
        {
            return index != invalid_resource_handle;
        }

        inline constexpr operator resource_handle() const noexcept
        {
            return index;
        }
    };

    struct texture_handle
    {
        resource_handle index;

        inline constexpr operator bool() const noexcept
        {
            return index != invalid_resource_handle;
        }

        inline constexpr operator resource_handle() const noexcept
        {
            return index;
        }
    };

    struct shader_state_handle
    {
        resource_handle index;

        inline constexpr operator bool() const noexcept
        {
            return index != invalid_resource_handle;
        }

        inline constexpr operator resource_handle() const noexcept
        {
            return index;
        }
    };

    struct sampler_handle
    {
        resource_handle index;

        inline constexpr operator bool() const noexcept
        {
            return index != invalid_resource_handle;
        }

        inline constexpr operator resource_handle() const noexcept
        {
            return index;
        }
    };

    struct descriptor_set_layout_handle
    {
        resource_handle index;

        inline constexpr operator bool() const noexcept
        {
            return index != invalid_resource_handle;
        }

        inline constexpr operator resource_handle() const noexcept
        {
            return index;
        }
    };

    struct descriptor_set_handle
    {
        resource_handle index;

        inline constexpr operator bool() const noexcept
        {
            return index != invalid_resource_handle;
        }

        inline constexpr operator resource_handle() const noexcept
        {
            return index;
        }
    };

    struct pipeline_handle
    {
        resource_handle index;

        inline constexpr operator bool() const noexcept
        {
            return index != invalid_resource_handle;
        }

        inline constexpr operator resource_handle() const noexcept
        {
            return index;
        }
    };

    struct render_pass_handle
    {
        resource_handle index;

        inline constexpr operator bool() const noexcept
        {
            return index != invalid_resource_handle;
        }

        inline constexpr operator resource_handle() const noexcept
        {
            return index;
        }
    };

    inline constexpr std::size_t max_framebuffer_attachments = 8;
    inline constexpr std::size_t max_descriptor_set_layouts = 8;
    inline constexpr std::size_t max_shader_stages = 5;
    inline constexpr std::size_t max_descriptors_per_set = 16;
    inline constexpr std::size_t max_vertex_streams = 16;
    inline constexpr std::size_t max_vertex_attributes = 16;
    inline constexpr std::size_t max_barrier_count = 8;

    inline constexpr std::size_t submit_header_sentinel = 0xfefeb7ba;
    inline constexpr std::size_t max_resource_deletions_per_frame = 64;

    template <typename OT, typename ET = OT> struct rect_2d
    {
        OT x{0};
        OT y{0};
        ET width{0};
        ET height{0};
    };

    using rect_2df = rect_2d<float>;
    using rect_2di = rect_2d<std::int16_t, std::uint16_t>;

    struct viewport
    {
        rect_2di rect;
        float min_depth{0.0f};
        float max_depth{0.0f};
    };

    struct viewport_state
    {
        std::span<viewport> viewports;
        std::span<rect_2di> scissors;
    };

    struct stencil_operation_state
    {
        VkStencilOp fail{VK_STENCIL_OP_KEEP};
        VkStencilOp pass{VK_STENCIL_OP_KEEP};
        VkStencilOp depth_fail{VK_STENCIL_OP_KEEP};
        VkCompareOp compare{VK_COMPARE_OP_ALWAYS};

        std::uint32_t compare_mask{0xff};
        std::uint32_t write_mask{0xff};
        std::uint32_t reference{0xff};
    };

    struct depth_stencil_create_info
    {
        stencil_operation_state front_face{};
        stencil_operation_state back_face{};
        VkCompareOp depth_comparison{VK_COMPARE_OP_ALWAYS};

        std::uint8_t depth_test_enable : 1 {0};
        std::uint8_t depth_write_enable : 1 {0};
        std::uint8_t stencil_op_enable : 1 {0};
        std::uint8_t padding : 5 {0}; // 5 bits of padding
    };

    struct component_blend_op
    {
        VkBlendFactor source{VK_BLEND_FACTOR_ONE};
        VkBlendFactor destination{VK_BLEND_FACTOR_ONE};
        VkBlendOp operation{VK_BLEND_OP_ADD};
    };

    struct attachment_blend_state
    {
        component_blend_op rgb;
        component_blend_op alpha;
        VkColorComponentFlags wirte_mask;

        std::uint8_t blend_enabled : 1 {0};
        std::uint8_t separate_blend : 1 {0};
        std::uint8_t padding : 6 {0};
    };

    struct attachment_blend_state_create_info
    {
        std::array<attachment_blend_state, max_framebuffer_attachments> blend_states;
        std::uint32_t attachment_count;
    };

    struct rasterization_create_info
    {
        VkCullModeFlags cull_mode{VK_CULL_MODE_NONE};
        VkFrontFace vertex_winding_order{VK_FRONT_FACE_COUNTER_CLOCKWISE};
        VkPolygonMode fill_mode{VK_POLYGON_MODE_FILL};
    };

    struct buffer_create_info
    {
        VkBufferUsageFlags type{0};
        resource_usage usage{resource_usage::IMMUTABLE};
        std::uint32_t size{0};
        std::span<std::byte> initial_data;

        std::string_view name;
    };

    struct texture_create_info
    {
        std::span<std::byte> initial_payload;
        std::uint16_t width{1};
        std::uint16_t height{1};
        std::uint16_t depth{1}; // or layers, if a layered image
        std::uint8_t mipmap_count{1};
        texture_flags flags{0};

        VkFormat image_format{VK_FORMAT_UNDEFINED};
        texture_type image_type{texture_type::D2};

        std::string_view name;
    };

    struct sampler_create_info
    {
        VkFilter min_filter{VK_FILTER_NEAREST};
        VkFilter mag_filter{VK_FILTER_NEAREST};
        VkSamplerMipmapMode mip_filter{VK_SAMPLER_MIPMAP_MODE_NEAREST};

        VkSamplerAddressMode u_address{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerAddressMode v_address{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerAddressMode w_address{VK_SAMPLER_ADDRESS_MODE_REPEAT};

        std::string_view name;
    };

    struct shader_stage
    {
        std::span<std::byte> byte_code;
        VkShaderStageFlagBits shader_type{VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM};
    };

    struct shader_state_create_info
    {
        std::array<shader_stage, max_shader_stages> stages;
        std::uint32_t stage_count;
        std::string_view name;
    };

    struct descriptor_set_layout_create_info
    {
        struct binding
        {
            VkDescriptorType type{VK_DESCRIPTOR_TYPE_MAX_ENUM};
            std::uint16_t start_binding{0};
            std::uint16_t binding_count{1};
            std::string_view name;
        };

        std::array<binding, max_descriptors_per_set> bindings;
        std::uint32_t binding_count{0};
        std::uint32_t set_index{0};

        std::string name;
    };

    struct descriptor_set_create_info
    {
        std::array<resource_handle, max_descriptors_per_set> resources;
        std::array<sampler_handle, max_descriptors_per_set> samplers;
        std::array<std::uint16_t, max_descriptors_per_set> bindings;

        descriptor_set_layout_handle layout{invalid_resource_handle};
        std::uint32_t resource_count{0};

        std::string_view name;
    };

    struct descriptor_set_update
    {
        descriptor_set_handle desc_set;
        std::uint32_t issuing_frame;
    };

    struct vertex_stream
    {
        std::uint16_t binding{0};
        std::uint16_t stride{0};
        VkVertexInputRate input_rate{VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX};
    };

    struct vertex_attribute
    {
        std::uint16_t location{0};
        std::uint16_t binding{0};          // stream binding to use
        std::uint32_t offset{0};           // offset in the stream
        VkFormat fmt{VK_FORMAT_UNDEFINED}; // TODO: make enum for supported attrib formats
    };

    struct vertex_input_create_info
    {
        std::array<vertex_stream, max_vertex_streams> streams;
        std::array<vertex_attribute, max_vertex_attributes> attributes;
        std::uint32_t stream_count{0};
        std::uint32_t attribute_count{0};
    };

    struct render_pass_attachment_info
    {
        std::array<VkFormat, max_framebuffer_attachments> color_formats;
        VkFormat depth_stencil_format;
        std::uint32_t color_attachment_count;

        render_pass_attachment_operation color_load{render_pass_attachment_operation::DONT_CARE};
        render_pass_attachment_operation depth_load{render_pass_attachment_operation::DONT_CARE};
        render_pass_attachment_operation stencil_load{render_pass_attachment_operation::DONT_CARE};
    };

    struct render_pass_create_info
    {
        std::uint32_t render_targets;
        render_pass_type type{render_pass_type::RASTERIZATION};

        std::array<texture_handle, max_framebuffer_attachments> color_outputs;
        texture_handle depth_stencil_texture{.index{invalid_resource_handle}};

        float scale_x{1.0f};
        float scale_y{1.0f};
        std::uint8_t resize{1};

        render_pass_attachment_operation color_load{render_pass_attachment_operation::DONT_CARE};
        render_pass_attachment_operation depth_load{render_pass_attachment_operation::DONT_CARE};
        render_pass_attachment_operation stencil_load{render_pass_attachment_operation::DONT_CARE};

        std::string_view name;
    };

    struct pipeline_create_info
    {
        rasterization_create_info raster{};
        depth_stencil_create_info ds{};
        attachment_blend_state_create_info blend{};
        vertex_input_create_info vertex_input{};
        shader_state_create_info shaders{};
        render_pass_attachment_info output;
        std::array<descriptor_set_layout_handle, max_descriptor_set_layouts> desc_layouts;
        std::uint32_t active_desc_layouts;

        std::string_view name;
    };

    struct texture_format_utils
    {
        inline static constexpr bool is_depth_stencil(VkFormat fmt) noexcept
        {
            return fmt == VK_FORMAT_D16_UNORM || fmt == VK_FORMAT_D16_UNORM_S8_UINT ||
                   fmt == VK_FORMAT_D24_UNORM_S8_UINT || fmt == VK_FORMAT_D32_SFLOAT ||
                   fmt == VK_FORMAT_D32_SFLOAT_S8_UINT;
        }

        inline static constexpr bool is_depth_only(VkFormat fmt) noexcept
        {
            return fmt == VK_FORMAT_D16_UNORM || fmt == VK_FORMAT_D32_SFLOAT;
        }

        inline static constexpr bool is_stencil_only(VkFormat fmt) noexcept
        {
            return fmt == VK_FORMAT_S8_UINT;
        }

        inline static constexpr bool has_depth(VkFormat fmt) noexcept
        {
            return (fmt >= VK_FORMAT_D16_UNORM && fmt < VK_FORMAT_S8_UINT) ||
                   (fmt >= VK_FORMAT_D16_UNORM_S8_UINT && fmt <= VK_FORMAT_D32_SFLOAT_S8_UINT);
        }

        inline static constexpr bool has_stencil(VkFormat fmt) noexcept
        {
            return fmt >= VK_FORMAT_S8_UINT && fmt <= VK_FORMAT_D32_SFLOAT_S8_UINT;
        }

        inline static constexpr bool has_depth_or_stencil(VkFormat fmt) noexcept
        {
            return fmt >= VK_FORMAT_D16_UNORM && fmt <= VK_FORMAT_D32_SFLOAT_S8_UINT;
        }
    };

    struct resource_data
    {
        std::span<std::byte> data;
    };

    struct resource_binding
    {
        std::uint16_t type;
        std::uint16_t start;
        std::uint16_t count;
        std::uint16_t set;

        std::string_view name;
    };

    struct shader_state_desc
    {
        void* native{nullptr};
        std::string_view name;
    };

    struct buffer_desc
    {
        void* native{nullptr};
        std::string_view name;

        VkBufferUsageFlags type{0};
        resource_usage usage{resource_usage::IMMUTABLE};
        std::uint32_t size{0};
        buffer_handle parent{.index{invalid_resource_handle}};
    };

    struct texture_desc
    {
        void* native{nullptr};
        std::string_view name;

        std::uint16_t width{1};
        std::uint16_t height{1};
        std::uint16_t depth{1};
        std::uint8_t mipmaps{1};
        std::uint8_t render_target{0};
        std::uint8_t compute_access{0};

        VkFormat fmt{VK_FORMAT_UNDEFINED};
        texture_type type{texture_type::D2};
    };

    struct sampler_desc
    {
        std::string_view name;

        VkFilter min_filter{VK_FILTER_NEAREST};
        VkFilter mag_filter{VK_FILTER_NEAREST};
        VkSamplerMipmapMode mip_filter{VK_SAMPLER_MIPMAP_MODE_NEAREST};

        VkSamplerAddressMode u_address{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerAddressMode v_address{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerAddressMode w_address{VK_SAMPLER_ADDRESS_MODE_REPEAT};
    };

    struct descriptor_set_layout_desc
    {
        std::array<resource_binding, max_descriptors_per_set> bindings;
        std::uint32_t binding_count;
    };

    struct descriptor_set_desc
    {
        std::array<resource_data, max_descriptors_per_set> resources;
        std::uint32_t resource_count;
    };

    struct pipeline_desc
    {
        shader_state_handle shader;
    };

    struct map_buffer_desc
    {
        buffer_handle buf{.index{invalid_resource_handle}};
        std::uint32_t offset{0};
        std::uint32_t length{0};
    };

    struct resource_update_desc
    {
        resource_deletion_type type;
        resource_handle handle{invalid_resource_handle};
        std::uint32_t current_frame{0};
    };

    struct device_state_vk;

    struct buffer
    {
        VkBuffer underlying;
        VmaAllocation allocation;
        VkDeviceMemory memory;
        VkDeviceSize vk_size;

        VkBufferUsageFlags buf_type{0};
        resource_usage usage{resource_usage::IMMUTABLE};
        std::uint32_t size{0};
        std::uint32_t global_offset{0};

        buffer_handle handle;
        buffer_handle parent_buffer; // if suballocation
        std::string_view name;
    };

    struct sampler
    {
        VkSampler underlying;

        VkFilter min_filter{VK_FILTER_NEAREST};
        VkFilter mag_filter{VK_FILTER_NEAREST};
        VkSamplerMipmapMode mip_filter{VK_SAMPLER_MIPMAP_MODE_NEAREST};

        VkSamplerAddressMode u_address{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerAddressMode v_address{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerAddressMode w_address{VK_SAMPLER_ADDRESS_MODE_REPEAT};

        std::string_view name;
    };

    struct texture
    {
        VkImage underlying_image;
        VkImageView underlying_view;
        VkFormat image_fmt;
        VkImageLayout image_layout;
        VmaAllocation allocation;

        std::uint16_t width{1};
        std::uint16_t height{1};
        std::uint16_t depth{1};
        std::uint8_t mipmaps{1};
        texture_flags flags{0};

        texture_handle handle;
        texture_type type{texture_type::D2};

        sampler* samp{nullptr};
        std::string_view name;
    };

    struct shader_state
    {
        std::array<VkPipelineShaderStageCreateInfo, max_shader_stages> stage_infos;
        std::uint32_t shader_count{0};
        bool is_graphics{false};

        std::string_view name;
    };

    struct descriptor_binding
    {
        VkDescriptorType type;
        std::uint16_t start{0};
        std::uint16_t count{0};
        std::uint16_t set{0};

        std::string_view name;
    };

    struct descriptor_set_layout
    {
        VkDescriptorSetLayout layout;

        VkDescriptorSetLayoutBinding* vk_binding{nullptr};
        descriptor_binding* bindings{nullptr};
        std::uint16_t num_bindings{0};
        std::uint16_t set_index{0};

        descriptor_set_layout_handle handle;
    };

    struct descriptor_set
    {
        VkDescriptorSet set;

        resource_handle* resources{nullptr};
        sampler_handle* samplers{nullptr};
        std::uint16_t* bindings{nullptr};
        std::uint32_t num_resources{0};

        const descriptor_set_layout* layout{nullptr};
    };

    struct pipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout layout;
        VkPipelineBindPoint kind;

        shader_state_handle state;

        std::array<const descriptor_set_layout*, max_descriptor_set_layouts> desc_set_layouts;
        std::array<descriptor_set_layout_handle, max_descriptor_set_layouts> desc_set_layout_handles;
        std::uint32_t num_active_layouts{0};

        depth_stencil_create_info depth_stencil;
        attachment_blend_state_create_info blend;
        rasterization_create_info raster;

        pipeline_handle handle;
        bool is_graphics_pipeline{true};
    };

    struct render_pass
    {
        VkRenderPass pass;
        VkFramebuffer target;

        render_pass_attachment_info output;
        std::array<texture_handle, max_framebuffer_attachments> output_color_textures;
        texture_handle output_depth_attachment{.index{invalid_resource_handle}};

        render_pass_type type{render_pass_type::RASTERIZATION};

        float scale_x{1.0f};
        float scale_y{1.0f};
        std::uint16_t width{0};
        std::uint16_t height{0};
        std::uint16_t dispatch_x{0};
        std::uint16_t dispatch_y{0};
        std::uint16_t dispatch_z{0};
        std::uint8_t resize{0};
        std::uint8_t num_render_targets{0};

        std::string_view name;
    };

    struct texture_barrier
    {
        texture_handle tex;
    };

    struct memory_barrier
    {
        buffer_handle buf;
    };

    struct execution_barrier
    {
        pipeline_stage source;
        pipeline_stage destination;

        std::uint32_t load_operation{0};

        std::span<texture_barrier> textures;
        std::span<memory_barrier> buffers;
    };

    inline std::string_view get_compiler_extension(VkShaderStageFlagBits stage)
    {
        switch (stage)
        {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return "vert.spv";
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return "frag.spv";
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return "comp.spv";
        default:
            return "";
        }
    }

    inline VkImageType to_vk_image_type(texture_type type)
    {
        switch (type)
        {
        case texture_type::D2: // most common is 2D, so we'll put this first
            [[fallthrough]];
        case texture_type::D2_ARRAY:
            [[likely]] return VK_IMAGE_TYPE_2D;
        case texture_type::D1:
            [[fallthrough]];
        case texture_type::D1_ARRAY:
            [[unlikely]] return VK_IMAGE_TYPE_1D;
        case texture_type::D3:
            [[fallthrough]];
        case texture_type::CUBE_ARRAY:
            [[unlikely]] return VK_IMAGE_TYPE_3D;
        }
        return VK_IMAGE_TYPE_MAX_ENUM;
    }

    inline VkImageViewType to_vk_image_view_type(texture_type type)
    {
        switch (type)
        {
        case texture_type::D2: // most common is 2D, so we'll put this first
            [[likely]] return VK_IMAGE_VIEW_TYPE_2D;
        case texture_type::D2_ARRAY:
            [[unlikely]] return VK_IMAGE_VIEW_TYPE_2D;
        case texture_type::D1:
            [[unlikely]] return VK_IMAGE_VIEW_TYPE_1D;
        case texture_type::D1_ARRAY:
            [[unlikely]] return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case texture_type::D3:
            [[unlikely]] return VK_IMAGE_VIEW_TYPE_3D;
        case texture_type::CUBE_ARRAY:
            [[unlikely]] return VK_IMAGE_VIEW_TYPE_CUBE;
        }
        return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    }

    inline VkPipelineStageFlags to_vk_pipeline_stage(pipeline_stage stage)
    {
        static constexpr VkPipelineStageFlags stages[] = {
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
        };

        return stages[static_cast<std::underlying_type_t<pipeline_stage>>(stage)];
    }

    inline VkPipelineStageFlags fetch_pipeline_stage_flags(VkAccessFlags access, VkQueueFlagBits type)
    {
        VkPipelineStageFlags flags{0};

        if (type & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
        {
            if (access & VK_ACCESS_INDIRECT_COMMAND_READ_BIT)
            {
                flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
            }

            if (access & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT))
            {
                flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
            }

            if (access & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT))
            {
                flags |= VK_PIPELINE_STAGE_HOST_BIT;
            }

            if (flags == 0)
            {
                flags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            }
        }

        switch (type)
        {
        case VK_QUEUE_GRAPHICS_BIT: {
            if (access & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT))
            {
                flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            }

            if (access & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT))
            {
                flags |= (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            }

            if (access & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
            {
                flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }

            if (access & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT))
            {
                flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            }

            if (access & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT))
            {
                flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            }

            break;
        }
        case VK_QUEUE_COMPUTE_BIT: {
            if ((access & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) ||
                (access & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) ||
                (access & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) ||
                (access & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)))
            {
                flags |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            }

            if (access & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT))
            {
                flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }

            break;
        }
        case VK_QUEUE_TRANSFER_BIT: {
            flags |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            break;
        }
        }

        return flags;
    }

    inline VkAccessFlags fetch_access_flags(resource_state state)
    {
        VkAccessFlags flags{0};

        if ((state & resource_state::TRANSFER_SRC) != resource_state::UNDEFINED)
        {
            flags |= VK_ACCESS_TRANSFER_READ_BIT;
        }

        if ((state & resource_state::TRANSFER_DST) != resource_state::UNDEFINED)
        {
            flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
        }

        if ((state & resource_state::VERTEX_AND_UNIFORM_BUFFER) != resource_state::UNDEFINED)
        {
            flags |= (VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT);
        }

        if ((state & resource_state::INDEX_BUFFER) != resource_state::UNDEFINED)
        {
            flags |= VK_ACCESS_INDEX_READ_BIT;
        }

        if ((state & resource_state::UNORDERED_MEMORY_ACCESS) != resource_state::UNDEFINED)
        {
            flags |= (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        }

        if ((state & resource_state::INDIRECT_ARGUMENT_BUFFER) != resource_state::UNDEFINED)
        {
            flags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        }

        if ((state & resource_state::RENDER_TARGET) != resource_state::UNDEFINED)
        {
            flags |= (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        }

        if ((state & resource_state::DEPTH_WRITE) != resource_state::UNDEFINED)
        {
            flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        if ((state & resource_state::DEPTH_READ) != resource_state::UNDEFINED)
        {
            flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }

        if ((state & resource_state::GENERIC_SHADER_RESOURCE) != resource_state::UNDEFINED)
        {
            flags |= VK_ACCESS_SHADER_READ_BIT;
        }

        if ((state & resource_state::PRESENT) != resource_state::UNDEFINED)
        {
            flags |= VK_ACCESS_MEMORY_READ_BIT;
        }

        return flags;
    }
} // namespace tempest::graphics

#endif // tempest_graphics_resources_hpp__
