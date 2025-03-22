#ifndef tempest_graphics_types_hpp
#define tempest_graphics_types_hpp

#include "window.hpp"

#include <tempest/mat4.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vector.hpp>

#include <compare>
#include <cstddef>
#include <numeric>
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
        R8_UNORM,
        R16_FLOAT,
        R32_FLOAT,
        R32_UINT,
        RGBA8_SRGB,
        BGRA8_SRGB,
        RGBA8_UINT,
        RGBA8_UNORM,
        RGBA16_FLOAT,
        RGBA16_UNORM,
        RG16_FLOAT,
        RG32_FLOAT,
        RG32_UINT,
        RGB32_FLOAT,
        RGBA32_FLOAT,
        D32_FLOAT,
        D24_FLOAT,
        D24_S8_FLOAT,
    };

    inline constexpr size_t bytes_per_element(resource_format fmt)
    {
        switch (fmt)
        {
        case resource_format::R8_UNORM:
            return 1;
        case resource_format::R16_FLOAT:
            return 2;
        case resource_format::R32_FLOAT:
            [[fallthrough]];
        case resource_format::R32_UINT:
            [[fallthrough]];
        case resource_format::D32_FLOAT:
            [[fallthrough]];
        case resource_format::D24_FLOAT:
            [[fallthrough]];
        case resource_format::D24_S8_FLOAT:
            [[fallthrough]];
        case resource_format::RGBA8_SRGB:
            [[fallthrough]];
        case resource_format::RGBA8_UINT:
            [[fallthrough]];
        case resource_format::RGBA8_UNORM:
            [[fallthrough]];
        case resource_format::BGRA8_SRGB:
            [[fallthrough]];
        case resource_format::RG16_FLOAT:
            return 4;
        case resource_format::RG32_FLOAT:
            [[fallthrough]];
        case resource_format::RG32_UINT:
            [[fallthrough]];
        case resource_format::RGBA16_FLOAT:
            return 8;
        case resource_format::RGB32_FLOAT:
            return 12;
        case resource_format::RGBA32_FLOAT:
            return 16;
        default:
            break;
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
        HOST_WRITE,
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
        uint32_t width;
        uint32_t height;
        uint32_t depth{1};
        uint32_t layers{1};
        uint32_t mips{1};
        resource_format fmt;
        image_type type;
        bool persistent = false;
        string_view name;
    };

    struct buffer_desc
    {
        size_t size;
        memory_location location{memory_location::AUTO};
        string_view name;
        bool per_frame_memory{false};
    };

    struct buffer_create_info
    {
        bool per_frame;
        memory_location loc;
        size_t size;
        bool transfer_source : 1  = false;
        bool transfer_destination : 1 = false;
        bool uniform_buffer : 1 = false;
        bool storage_buffer : 1 = false;
        bool index_buffer : 1 = false;
        bool vertex_buffer : 1 = false;
        bool indirect_buffer : 1 = false;
        string name{};
    };

    struct image_create_info
    {
        image_type type;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t layers;
        uint32_t mip_count;
        resource_format format;
        sample_count samples;
        bool transfer_source : 1 = false;
        bool transfer_destination : 1 = false;
        bool sampled : 1 = false;
        bool storage : 1 = false;
        bool color_attachment : 1 = false;
        bool depth_attachment : 1 = false;
        bool persistent = false;
        string name{};
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
        uint32_t binding_index;
        uint32_t binding_count;
    };

    struct descriptor_set_layout_create_info
    {
        uint32_t set;
        span<descriptor_binding_info> bindings;
    };

    struct push_constant_layout
    {
        uint32_t offset;
        uint32_t range;
    };

    struct pipeline_layout_create_info
    {
        span<descriptor_set_layout_create_info> set_layouts{};
        span<push_constant_layout> push_constants{};
    };

    enum class blend_factor
    {
        ZERO,
        ONE,
        SRC_COLOR,
        ONE_MINUS_SRC_COLOR,
        SRC_ALPHA,
        ONE_MINUS_SRC_ALPHA,
        DST_COLOR,
        ONE_MINUS_DST_COLOR,
        DST_ALPHA,
        ONE_MINUS_DST_ALPHA,
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
        NOT_EQUALS,
        NEVER,
        ALWAYS,
    };

    enum class vertex_winding_order
    {
        CLOCKWISE,
        COUNTER_CLOCKWISE,
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
        attachment_blend_info color{};
        attachment_blend_info alpha{};
    };

    struct color_blend_state
    {
        span<color_blend_attachment_state> attachment_blend_ops;
    };

    struct vertex_input_element
    {
        uint32_t binding;
        uint32_t location;
        uint32_t offset;
        resource_format format;
    };

    struct vertex_input_layout
    {
        span<vertex_input_element> elements;
    };

    struct render_target_layout
    {
        span<resource_format> color_attachment_formats{};
        resource_format depth_attachment_format{resource_format::UNKNOWN};
    };

    struct depth_state
    {
        bool enable_test;
        bool enable_write;
        bool enable_bounds_test{false};
        bool clamp_depth{false};
        compare_operation depth_test_op;
        float min_depth_bounds{0.0f};
        float max_depth_bounds{1.0f};
        bool enable_depth_bias{false};
        float depth_bias_constant_factor{0.0f};
        float depth_bias_clamp{0.0f};
        float depth_bias_slope_factor{0.0f};
    };

    struct shader_create_info
    {
        span<byte> bytes;
        string_view entrypoint;
        string name;
    };

    struct graphics_pipeline_create_info
    {
        pipeline_layout_create_info layout;
        render_target_layout target;
        shader_create_info vertex_shader;
        shader_create_info fragment_shader;
        vertex_input_layout vertex_layout{};
        depth_state depth_testing;
        color_blend_state blending;
        string name;
    };

    struct compute_pipeline_create_info
    {
        pipeline_layout_create_info layout;
        shader_create_info compute_shader;
        string name;
    };

    struct swapchain_create_info
    {
        iwindow* win;
        uint32_t desired_frame_count;
        bool use_vsync;
    };

    struct texture_mip_descriptor
    {
        uint32_t width;
        uint32_t height;
        span<byte> bytes;
    };

    struct texture_data_descriptor
    {
        resource_format fmt;
        vector<texture_mip_descriptor> mips;
        string name;
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
        float mip_lod_bias{0.0f};
        float min_lod{0.0f};
        float max_lod{1000.0f};
        bool enable_aniso{false};
        float max_anisotropy{};
        string name{};
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
        uint32_t id;
        uint32_t generation;

        constexpr gfx_resource_handle(uint32_t id, uint32_t generation) : id{id}, generation{generation}
        {
        }

        inline constexpr operator bool() const noexcept
        {
            return generation != ~0u;
        }

        inline constexpr uint64_t as_uint64() const noexcept
        {
            return (static_cast<uint64_t>(id) << 32) | generation;
        }

        constexpr auto operator<=>(const gfx_resource_handle& rhs) const noexcept = default;
    };

    struct image_resource_handle : public gfx_resource_handle
    {
        constexpr image_resource_handle(uint32_t id = ~0u, uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    struct buffer_resource_handle : public gfx_resource_handle
    {
        constexpr buffer_resource_handle(uint32_t id = ~0u, uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    struct graph_pass_handle : public gfx_resource_handle
    {
        constexpr graph_pass_handle(uint32_t id = ~0u, uint32_t generation = ~0u) : gfx_resource_handle(id, generation)
        {
        }
    };

    struct graphics_pipeline_resource_handle : public gfx_resource_handle
    {
        constexpr graphics_pipeline_resource_handle(uint32_t id = ~0u, uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    struct compute_pipeline_resource_handle : public gfx_resource_handle
    {
        constexpr compute_pipeline_resource_handle(uint32_t id = ~0u, uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    struct swapchain_resource_handle : public gfx_resource_handle
    {
        constexpr swapchain_resource_handle(uint32_t id = ~0u, uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    struct sampler_resource_handle : public gfx_resource_handle
    {
        constexpr sampler_resource_handle(uint32_t id = ~0u, uint32_t generation = ~0u)
            : gfx_resource_handle(id, generation)
        {
        }
    };

    class command_list
    {
      public:
        virtual ~command_list() = default;

        template <typename T>
            requires(!is_same_v<T, span<const byte>>)
        command_list& push_constants(uint32_t offset, const T& data, compute_pipeline_resource_handle handle)
        {
            return push_constants(offset, span{reinterpret_cast<const byte*>(&data), sizeof(T)}, handle);
        }

        template <typename T>
            requires(!is_same_v<T, span<const byte>>)
        command_list& push_constants(uint32_t offset, const T& data, graphics_pipeline_resource_handle handle)
        {
            return push_constants(offset, span{reinterpret_cast<const byte*>(&data), sizeof(T)}, handle);
        }

        virtual command_list& push_constants(uint32_t offset, span<const byte> data,
                                             compute_pipeline_resource_handle handle) = 0;
        virtual command_list& push_constants(uint32_t offset, span<const byte> data,
                                             graphics_pipeline_resource_handle handle) = 0;

        virtual command_list& set_viewport(float x, float y, float width, float height, float min_depth = 0.0f,
                                           float max_depth = 1.0f, bool flip = true) = 0;
        virtual command_list& set_scissor_region(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;
        virtual command_list& draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0,
                                   uint32_t first_index = 0) = 0;
        virtual command_list& draw(buffer_resource_handle buf, uint32_t offset, uint32_t count, uint32_t stride) = 0;
        virtual command_list& draw_indexed(buffer_resource_handle buf, uint32_t offset, uint32_t count,
                                           uint32_t stride) = 0;
        virtual command_list& use_pipeline(graphics_pipeline_resource_handle pipeline) = 0;
        virtual command_list& use_index_buffer(buffer_resource_handle buf, uint32_t offset) = 0;
        virtual command_list& set_cull_mode(bool front, bool back) = 0;

        virtual command_list& blit(image_resource_handle src, image_resource_handle dst) = 0;
        virtual command_list& copy(buffer_resource_handle src, buffer_resource_handle dst, size_t src_offset = 0,
                                   size_t dst_offset = 0, size_t byte_count = std::numeric_limits<size_t>::max()) = 0;
        virtual command_list& copy(buffer_resource_handle src, image_resource_handle dst, size_t buffer_offset,
                                   uint32_t region_width, uint32_t region_height, uint32_t mip_level,
                                   int32_t offset_x = 0, int32_t offset_y = 0) = 0;
        virtual command_list& clear_color(image_resource_handle handle, float r, float g, float b, float a) = 0;

        virtual command_list& transition_image(image_resource_handle img, image_resource_usage old_usage,
                                               image_resource_usage new_usage) = 0;
        virtual command_list& generate_mip_chain(image_resource_handle img, image_resource_usage usage,
                                                 uint32_t base_mip = 0,
                                                 uint32_t mip_count = std::numeric_limits<uint32_t>::max()) = 0;

        virtual command_list& use_pipeline(compute_pipeline_resource_handle pipeline) = 0;
        virtual command_list& dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;
    };

    class command_execution_service
    {
      public:
        virtual ~command_execution_service() = default;

        virtual command_list& get_commands() = 0;
        virtual void submit_and_wait() = 0;
    };

    struct indirect_command
    {
        uint32_t vertex_count;
        uint32_t instance_count;
        uint32_t first_vertex;
        uint32_t first_instance;
    };

    struct indexed_indirect_command
    {
        uint32_t index_count;
        uint32_t instance_count;
        uint32_t first_index;
        int32_t vertex_offset;
        uint32_t first_instance;
    };

    struct camera_data
    {
        math::mat4<float> view_matrix;
        math::mat4<float> proj_matrix;
        math::mat4<float> view_proj_matrix;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_types_hpp