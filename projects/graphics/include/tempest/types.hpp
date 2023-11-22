#ifndef tempest_graphics_types_hpp
#define tempest_graphics_types_hpp

#include "window.hpp"

#include <compare>
#include <cstddef>
#include <numeric>
#include <span>
#include <string>

namespace tempest::graphics
{
    enum class queue_operation_type
    {
        GRAPHICS,
        TRANSFER,
        COMPUTE,
        COMPUTE_AND_TRANSFER,
        GRAPHICS_AND_TRANSFER,
    };

    enum class sample_count
    {
        COUNT_1 = 0b00001,
        COUNT_2 = 0b00010,
        COUNT_4 = 0b00100,
        COUNT_8 = 0b01000,
        COUNT_16 = 0b10000
    };

    enum class resource_format
    {
        UNKNOWN,
        RGBA8_SRGB,
        RG32_FLOAT,
        RG32_UINT,
        RGB32_FLOAT,
        RGBA32_FLOAT,
        D32_FLOAT,
    };

    enum class image_type
    {
        IMAGE_1D,
        IMAGE_2D,
        IMAGE_3D,
        IMAGE_CUBE_MAP,
        IMAGE_1D_ARRAY,
        IMAGE_2D_ARRAY,
        IMAGE_CUBE_MAP_ARRAY,
    };

    enum class shader_stage
    {
        NONE,
        VERTEX,
        FRAGMENT,
        COMPUTE
    };

    enum class buffer_resource_usage
    {
        STRUCTURED,
        CONSTANT,
        VERTEX,
        INDEX,
        INDIRECT_ARGUMENT,
        TRANSFER_SOURCE,
        TRANSFER_DESTINATION,
    };

    enum class image_resource_usage
    {
        COLOR_ATTACHMENT,
        DEPTH_ATTACHMENT,
        SAMPLED,
        STORAGE,
        TRANSFER_SOURCE,
        TRANSFER_DESTINATION,
        PRESENT,
    };

    enum class pipeline_stage
    {
        INFER,
        BEGIN,
        DRAW_INDIRECT,
        VERTEX,
        FRAGMENT,
        COLOR_OUTPUT,
        COMPUTE,
        TRANSFER,
        END,
    };

    enum class memory_location
    {
        DEVICE,
        HOST,
        AUTO,
    };

    struct image_desc
    {
        sample_count samples{sample_count::COUNT_1};
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t depth{1};
        std::uint32_t layers{1};
        resource_format fmt;
        image_type type;
        std::string_view name;
    };

    struct buffer_desc
    {
        std::size_t size;
        memory_location location{memory_location::AUTO};
        std::string_view name;
    };

    struct buffer_create_info
    {
        memory_location loc;
        std::size_t size;
        bool transfer_source : 1;
        bool transfer_destination : 1;
        bool uniform_buffer : 1;
        bool storage_buffer : 1;
        bool index_buffer : 1;
        bool vertex_buffer : 1;
        bool indirect_buffer : 1;
        std::string name;
    };

    struct image_create_info
    {
        image_type type;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t depth;
        std::uint32_t layers;
        std::uint32_t mip_count;
        resource_format format;
        sample_count samples;
        bool transfer_source : 1;
        bool transfer_destination : 1;
        bool sampled : 1;
        bool storage : 1;
        bool color_attachment : 1;
        bool depth_attachment : 1;
        std::string name;
    };

    enum class descriptor_binding_type
    {
        STRUCTURED_BUFFER,
        CONSTANT_BUFFER,
        STORAGE_IMAGE,
        SAMPLED_IMAGE,
        SAMPLER,
    };

    struct descriptor_binding_info
    {
        descriptor_binding_type type;
        std::uint32_t binding_index;
        std::uint32_t binding_count;
    };

    struct descriptor_set_layout_create_info
    {
        std::uint32_t set;
        std::span<descriptor_binding_info> bindings;
    };

    struct push_constant_layout
    {
        std::uint32_t offset;
        std::uint32_t range;
    };

    struct pipeline_layout_create_info
    {
        std::span<descriptor_set_layout_create_info> set_layouts;
        std::span<push_constant_layout> push_constants;
    };

    enum class blend_factor
    {
        ZERO,
        ONE,
        SRC,
        ONE_MINUS_SRC,
        DST,
        ONE_MINUS_DST
    };

    enum class blend_operation
    {
        ADD,
        SUB,
        MIN,
        MAX,
    };

    enum class compare_operation
    {
        LESS,
        LESS_OR_EQUALS,
        EQUALS,
        GREATER_OR_EQUALS,
        GREATER,
        NOT_EQUALS
    };

    struct attachment_blend_info
    {
        blend_factor src;
        blend_factor dst;
        blend_operation op;
    };

    struct color_blend_attachment_state
    {
        bool enabled;
        attachment_blend_info color;
        attachment_blend_info alpha;
    };

    struct color_blend_state
    {
        std::span<color_blend_attachment_state> attachment_blend_ops;
    };

    struct vertex_input_element
    {
        std::uint32_t binding;
        std::uint32_t location;
        std::uint32_t offset;
        resource_format format;
    };

    struct vertex_input_layout
    {
        std::span<vertex_input_element> elements;
    };

    struct render_target_layout
    {
        std::span<resource_format> color_attachment_formats;
        resource_format depth_attachment_format{resource_format::UNKNOWN};
    };

    struct depth_state
    {
        bool enable_test;
        bool enable_write;
        bool enable_bounds_test;
        bool clamp_depth;
        compare_operation depth_test_op;
        float min_depth_bounds{0.0f};
        float max_depth_bounds{1.0f};
        bool enable_depth_bias;
        float depth_bias_constant_factor{0.0f};
        float depth_bias_clamp{0.0f};
        float depth_bias_slope_factor{0.0f};
    };

    struct shader_create_info
    {
        std::span<std::byte> bytes;
        std::string_view entrypoint;
        std::string name;
    };

    struct graphics_pipeline_create_info
    {
        pipeline_layout_create_info layout;
        render_target_layout target;
        shader_create_info vertex_shader;
        shader_create_info fragment_shader;
        vertex_input_layout vertex_layout;
        depth_state depth_testing;
        color_blend_state blending;
        std::string name;
    };

    struct swapchain_create_info
    {
        iwindow* win;
        std::uint32_t desired_frame_count;
    };

    struct gfx_resource_handle
    {
        std::uint32_t id;
        std::uint32_t generation;

        constexpr gfx_resource_handle(std::uint32_t id, std::uint32_t generation) : id{id}, generation{generation}
        {
        }

        inline constexpr operator bool() const noexcept
        {
            return generation != ~0u;
        }
        
        inline constexpr std::uint64_t as_uint64() const noexcept
        {
            return (static_cast<std::uint64_t>(id) << 32) | generation;
        }

        constexpr auto operator<=>(const gfx_resource_handle& rhs) const noexcept = default;
    };

    struct image_resource_handle : public gfx_resource_handle
    {
        constexpr image_resource_handle(std::uint32_t id = ~0u, std::uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    struct buffer_resource_handle : public gfx_resource_handle
    {
        constexpr buffer_resource_handle(std::uint32_t id = ~0u, std::uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    struct graph_pass_handle : public gfx_resource_handle
    {
        constexpr graph_pass_handle(std::uint32_t id = ~0u, std::uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    struct graphics_pipeline_resource_handle : public gfx_resource_handle
    {
        constexpr graphics_pipeline_resource_handle(std::uint32_t id = ~0u, std::uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    struct swapchain_resource_handle : public gfx_resource_handle
    {
        constexpr swapchain_resource_handle(std::uint32_t id = ~0u, std::uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    enum class resource_access_type
    {
        READ,
        WRITE,
        READ_WRITE,
    };

    class command_list
    {
      public:
        virtual ~command_list() = default;

        // virtual command_list& set_viewport(float x, float y, float width, float height, float min_depth = 0.0f,
        //                                    float max_depth = 1.0f, std::uint32_t viewport_id = 0) = 0;
        // virtual command_list& set_scissor_region(std::int32_t x, std::int32_t y, std::uint32_t width,
        //                                          std::uint32_t height) = 0;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_types_hpp