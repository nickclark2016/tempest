#ifndef tempest_graphics_types_hpp
#define tempest_graphics_types_hpp

#include "window.hpp"

#include <tempest/mat4.hpp>
#include <tempest/vec3.hpp>

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
        BGRA8_SRGB,
        RGBA8_UINT,
        RGBA8_UNORM,
        RG32_FLOAT,
        RG32_UINT,
        RGB32_FLOAT,
        RGBA32_FLOAT,
        D32_FLOAT,
    };

    inline std::size_t bytes_per_element(resource_format fmt)
    {
        switch (fmt)
        {
        case resource_format::D32_FLOAT:
            [[fallthrough]];
        case resource_format::RGBA8_SRGB:
            [[fallthrough]];
        case resource_format::RGBA8_UINT:
            [[fallthrough]];
        case resource_format::RGBA8_UNORM:
            [[fallthrough]];
        case resource_format::BGRA8_SRGB:
            return 4;
        case resource_format::RG32_FLOAT:
            [[fallthrough]];
        case resource_format::RG32_UINT:
            return 8;
        case resource_format::RGB32_FLOAT:
            return 12;
        case resource_format::RGBA32_FLOAT:
            return 16;
        }

        std::exit(EXIT_FAILURE);
    }

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
        UNDEFINED,
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
        bool per_frame_memory{false};
    };

    struct buffer_create_info
    {
        bool per_frame;
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
        STRUCTURED_BUFFER_DYNAMIC,
        CONSTANT_BUFFER,
        CONSTANT_BUFFER_DYNAMIC,
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

    struct compute_pipeline_create_info
    {
        pipeline_layout_create_info layout;
        shader_create_info compute_shader;
        std::string name;
    };

    struct swapchain_create_info
    {
        iwindow* win;
        std::uint32_t desired_frame_count;
    };

    struct texture_mip_descriptor
    {
        std::uint32_t width;
        std::uint32_t height;
        std::span<std::byte> bytes;
    };

    struct texture_data_descriptor
    {
        resource_format fmt;
        std::vector<texture_mip_descriptor> mips;
        std::string name;
    };

    enum class filter
    {
        NEAREST,
        LINEAR
    };

    enum class mipmap_mode
    {
        NEAREST,
        LINEAR
    };

    struct sampler_create_info
    {
        filter mag;
        filter min;
        mipmap_mode mipmap;
        float mip_lod_bias;
        float min_lod{0.0f};
        float max_lod{1000.0f};
        std::string name;
    };

    enum class resource_access_type
    {
        READ,
        WRITE,
        READ_WRITE,
    };

    enum class load_op
    {
        LOAD,
        CLEAR,
        DONT_CARE
    };

    enum class store_op
    {
        STORE,
        DONT_CARE
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

    struct compute_pipeline_resource_handle : public gfx_resource_handle
    {
        constexpr compute_pipeline_resource_handle(std::uint32_t id = ~0u, std::uint32_t generation = ~0u)
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

    struct sampler_resource_handle : public gfx_resource_handle
    {
        constexpr sampler_resource_handle(std::uint32_t id = ~0u, std::uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    class command_list
    {
      public:
        virtual ~command_list() = default;

        virtual command_list& set_viewport(float x, float y, float width, float height, float min_depth = 0.0f,
                                           float max_depth = 1.0f, std::uint32_t viewport_id = 0) = 0;
        virtual command_list& set_scissor_region(std::int32_t x, std::int32_t y, std::uint32_t width,
                                                 std::uint32_t height) = 0;
        virtual command_list& draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1,
                                   std::uint32_t first_vertex = 0, std::uint32_t first_index = 0) = 0;
        virtual command_list& use_pipeline(graphics_pipeline_resource_handle pipeline) = 0;

        virtual command_list& blit(image_resource_handle src, image_resource_handle dst) = 0;
        virtual command_list& copy(buffer_resource_handle src, buffer_resource_handle dst, std::size_t src_offset = 0,
                                   std::size_t dst_offset = 0,
                                   std::size_t byte_count = std::numeric_limits<std::size_t>::max()) = 0;
        virtual command_list& copy(buffer_resource_handle src, image_resource_handle dst, std::size_t buffer_offset,
                           std::uint32_t region_width, std::uint32_t region_height, std::uint32_t mip_level, std::int32_t offset_x = 0,
                           std::int32_t offset_y = 0) = 0;

        virtual command_list& transition_image(image_resource_handle img, image_resource_usage old_usage,
                                               image_resource_usage new_usage) = 0;
    };

    class command_execution_service
    {
      public:
        virtual ~command_execution_service() = default;

        virtual command_list& get_commands() = 0;
        virtual void submit_and_wait() = 0;
    };

    struct camera_data
    {
        math::mat4<float> view_matrix;
        math::mat4<float> proj_matrix;
        math::mat4<float> view_proj_matrix;
    };

    struct directional_light
    {
        math::vec3<float> light_direction;
        math::vec4<float> color_illum;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_types_hpp