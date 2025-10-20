#ifndef tempest_rhi_rhi_types_hpp
#define tempest_rhi_rhi_types_hpp

#include <tempest/enum.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>

namespace tempest::rhi
{
    class device;
    class instance;
    class work_queue;
    class window_surface;
    class rhi_resource_tracker;
    class descriptor_context;

    enum class rhi_handle_type
    {
        buffer,
        image,
        sampler,
        graphics_pipeline,
        compute_pipeline,
        command_list,
        fence,
        semaphore,
        render_surface,
        descriptor_set_layout,
        pipeline_layout,
        descriptor_set,
    };

    enum class bind_point
    {
        graphics,
        compute,
    };

    template <rhi_handle_type T>
    struct typed_rhi_handle
    {
        static constexpr rhi_handle_type type = T;

        uint32_t id;
        uint32_t generation;

        static const typed_rhi_handle<T> null_handle;

        constexpr bool operator==(const typed_rhi_handle& other) const noexcept;
        constexpr bool operator!=(const typed_rhi_handle& other) const noexcept;

        constexpr operator bool() const noexcept;
        constexpr bool is_valid() const noexcept;
    };

    template <rhi_handle_type T>
    inline const typed_rhi_handle<T> typed_rhi_handle<T>::null_handle = {
        .id = numeric_limits<uint32_t>::max(),
        .generation = numeric_limits<uint32_t>::max(),
    };

    template <rhi_handle_type T>
    inline constexpr bool typed_rhi_handle<T>::operator==(const typed_rhi_handle& other) const noexcept
    {
        return id == other.id && generation == other.generation;
    }

    template <rhi_handle_type T>
    inline constexpr bool typed_rhi_handle<T>::operator!=(const typed_rhi_handle& other) const noexcept
    {
        return !(*this == other);
    }

    template <rhi_handle_type T>
    inline constexpr typed_rhi_handle<T>::operator bool() const noexcept
    {
        return is_valid();
    }

    template <rhi_handle_type T>
    inline constexpr bool typed_rhi_handle<T>::is_valid() const noexcept
    {
        return id != numeric_limits<uint32_t>::max() && generation != numeric_limits<uint32_t>::max();
    }

    struct rhi_device_description
    {
        uint32_t device_index;
        string device_name;
    };

    enum class memory_location
    {
        device,
        host,
        automatic,
    };

    enum class image_format
    {
        // Single channel, color format
        r8_unorm,
        r8_snorm,
        r16_unorm,
        r16_snorm,
        r16_float,
        r32_float,
        // Two channels, color format
        rg8_unorm,
        rg8_snorm,
        rg16_unorm,
        rg16_snorm,
        rg16_float,
        rg32_float,
        // Four channels, color format
        rgba8_unorm,
        rgba8_snorm,
        rgba8_srgb,
        bgra8_srgb,
        rgba16_unorm,
        rgba16_snorm,
        rgba16_float,
        rgba32_float,
        // Depth-Stencil formats
        s8_uint,
        d16_unorm,
        d24_unorm,
        d32_float,
        d16_unorm_s8_uint,
        d24_unorm_s8_uint,
        d32_float_s8_uint,
        // HDR Formats
        a2bgr10_unorm_pack32,
    };

    enum class buffer_format
    {
        r8_unorm,
        r8_snorm,
        r8_uint,
        r8_sint,
        r16_unorm,
        r16_snorm,
        r16_uint,
        r16_sint,
        r16_float,
        r32_float,
        r32_uint,
        r32_sint,
        rg8_unorm,
        rg8_snorm,
        rg8_uint,
        rg8_sint,
        rg16_unorm,
        rg16_snorm,
        rg16_float,
        rg16_uint,
        rg16_sint,
        rg32_float,
        rg32_uint,
        rg32_sint,
        rgb8_unorm,
        rgb8_snorm,
        rgb8_uint,
        rgb8_sint,
        rgb16_unorm,
        rgb16_snorm,
        rgb16_float,
        rgb16_uint,
        rgb16_sint,
        rgb32_float,
        rgb32_uint,
        rgb32_sint,
        rgba8_unorm,
        rgba8_snorm,
        rgba8_uint,
        rgba8_sint,
        rgba16_unorm,
        rgba16_snorm,
        rgba16_float,
        rgba16_uint,
        rgba16_sint,
        rgba32_float,
        rgba32_uint,
        rgba32_sint,
    };

    enum class image_layout
    {
        undefined,
        general,
        color_attachment,
        depth_stencil_read_write,
        depth_stencil_read_only,
        shader_read_only,
        transfer_src,
        transfer_dst,
        depth,
        depth_read_only,
        stencil,
        stencil_read_only,
        present,
    };

    enum class buffer_usage
    {
        index = 0x00000001,
        indirect = 0x00000002,
        constant = 0x00000004,
        structured = 0x00000008,
        transfer_src = 0x00000010,
        transfer_dst = 0x00000020,
        vertex = 0x00000040,
        descriptor = 0x00000080,
    };

    enum class image_usage
    {
        color_attachment = 0x00000001,
        depth_attachment = 0x00000002,
        stencil_attachment = 0x00000004,
        storage = 0x00000008,
        sampled = 0x00000010,
        transfer_src = 0x00000020,
        transfer_dst = 0x00000040,
    };

    enum class image_type
    {
        image_1d,
        image_2d,
        image_3d,
        image_cube,
        image_1d_array,
        image_2d_array,
        image_cube_array,
    };

    enum class image_tiling_type
    {
        optimal,
        linear,
    };

    enum class image_sample_count
    {
        sample_count_1 = 0x00000001,
        sample_count_2 = 0x00000002,
        sample_count_4 = 0x00000004,
        sample_count_8 = 0x00000008,
        sample_count_16 = 0x00000010,
        sample_count_32 = 0x00000020,
        sample_count_64 = 0x00000040,
    };

    enum class host_access_type
    {
        none,
        coherent,
        incoherent,
    };

    enum class host_access_pattern
    {
        none,
        random,
        sequential,
    };

    struct buffer_desc
    {
        size_t size;
        memory_location location;
        enum_mask<buffer_usage> usage;
        host_access_type access_type;
        host_access_pattern access_pattern;
        string name;
    };

    struct image_desc
    {
        image_format format;
        image_type type;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t array_layers;
        uint32_t mip_levels;
        image_sample_count sample_count;
        image_tiling_type tiling;
        memory_location location;
        enum_mask<image_usage> usage;
        string name;
    };

    enum class filter
    {
        nearest,
        linear,
    };

    enum class mipmap_mode
    {
        nearest,
        linear,
    };

    enum class address_mode
    {
        repeat,
        mirrored_repeat,
        clamp_to_edge,
        clamp_to_border,
        mirror_clamp_to_edge,
    };

    enum class compare_op
    {
        never,
        less,
        equal,
        less_equal,
        greater,
        not_equal,
        greater_equal,
        always,
    };

    struct sampler_desc
    {
        filter mag;
        filter min;
        mipmap_mode mipmap;
        address_mode address_u;
        address_mode address_v;
        address_mode address_w;
        float mip_lod_bias;
        float min_lod;
        float max_lod;
        optional<float> max_anisotropy;
        optional<compare_op> compare;
        string name;
    };

    struct fence_info
    {
        bool signaled;
    };

    enum class semaphore_type
    {
        binary,
        timeline,
    };

    struct semaphore_info
    {
        semaphore_type type;
        uint64_t initial_value;
    };

    enum class operation_type
    {
        compute,
        graphics,
        transfer,
    };

    enum class color_space
    {
        adobe_rgb_linear,
        adobe_rgb_nonlinear,
        bt709_linear,
        bt709_nonlinear,
        bt2020_linear,
        dci_p3_nonlinear,
        display_native_amd,
        display_p3_linear,
        display_p3_nonlinear,
        extended_srgb_linear,
        extended_srgb_nonlinear,
        hdr10_hlg,
        hdr10_st2084,
        pass_through,
        srgb_nonlinear,
    };

    enum class present_mode
    {
        immediate,
        mailbox,
        fifo,
        fifo_relaxed,
    };

    struct render_surface_format
    {
        color_space space;
        image_format format;
    };

    struct render_surface_info
    {
        vector<present_mode> present_modes;
        vector<render_surface_format> formats;
        uint32_t min_image_count;
        uint32_t max_image_count;
        uint32_t min_image_width;
        uint32_t min_image_height;
        uint32_t max_image_width;
        uint32_t max_image_height;
        uint32_t max_image_layers;
        enum_mask<image_usage> supported_usages;
    };

    struct render_surface_desc
    {
        const window_surface* window;
        uint32_t min_image_count;
        render_surface_format format;
        present_mode present_mode;
        uint32_t width;
        uint32_t height;
        uint32_t layers;
    };

    struct window_surface_desc
    {
        uint32_t width;
        uint32_t height;
        string name;
        bool fullscreen;
    };

    enum class swapchain_error_code
    {
        out_of_date,
        suboptimal,
        failure,
        invalid_swapchain_argument,
    };

    struct swapchain_image_acquire_info_result
    {
        typed_rhi_handle<rhi_handle_type::semaphore> acquire_sem;
        typed_rhi_handle<rhi_handle_type::semaphore> render_complete_sem;
        typed_rhi_handle<rhi_handle_type::image> image;
        uint32_t image_index;
    };

    enum class pipeline_stage
    {
        none = 0x00000,
        top = 0x00001,
        bottom = 0x00002,
        indirect_command = 0x00004,
        // Graphics commmands
        vertex_attribute_input = 0x00008,
        index_input = 0x00010,
        vertex_shader = 0x00020,
        tessellation_control_shader = 0x00040,
        tessellation_evaluation_shader = 0x00080,
        geometry_shader = 0x00100,
        fragment_shader = 0x00200,
        early_fragment_tests = 0x00400,
        late_fragment_tests = 0x00800,
        all_fragment_tests = (early_fragment_tests | late_fragment_tests),
        color_attachment_output = 0x01000,
        // Compute commands
        compute_shader = 0x02000,
        // Transfer commands
        copy = 0x04000,
        resolve = 0x08000,
        blit = 0x10000,
        clear = 0x20000,
        all_transfer = 0x40000,
        // Host commands
        host = 0x80000,
        // All commands
        all = 0x100000,
    };

    enum class memory_access
    {
        none = 0x00000,
        indirect_command_read = 0x00001,
        index_read = 0x00002,
        vertex_attribute_read = 0x00004,
        constant_buffer_read = 0x00008,
        shader_read = 0x00010,
        shader_write = 0x00020,
        color_attachment_read = 0x00040,
        color_attachment_write = 0x00080,
        depth_stencil_attachment_read = 0x00100,
        depth_stencil_attachment_write = 0x00200,
        transfer_read = 0x00400,
        transfer_write = 0x00800,
        host_read = 0x01000,
        host_write = 0x02000,
        memory_read = 0x04000,
        memory_write = 0x08000,
        shader_sampled_read = 0x10000,
        shader_storage_read = 0x20000,
        shader_storage_write = 0x40000,
        descriptor_buffer_read = 0x80000,
    };

    enum class shader_stage
    {
        none = 0x00000,
        vertex = 0x00001,
        tessellation_control = 0x00002,
        tessellation_evaluation = 0x00004,
        geometry = 0x00008,
        fragment = 0x00010,
        compute = 0x00020,
    };

    enum class descriptor_type
    {
        sampler,
        sampled_image,
        storage_image,
        constant_buffer,
        structured_buffer,
        dynamic_constant_buffer,
        dynamic_structured_buffer,
        combined_image_sampler,
    };

    enum class descriptor_binding_flags
    {
        none = 0x00,
        partially_bound = 0x01,
        variable_length = 0x02,
    };

    struct descriptor_binding_layout
    {
        uint32_t binding_index;
        descriptor_type type;
        uint32_t count;
        enum_mask<shader_stage> stages;
        enum_mask<descriptor_binding_flags> flags = make_enum_mask(descriptor_binding_flags::none);

        bool operator==(const descriptor_binding_layout& other) const noexcept;
        bool operator!=(const descriptor_binding_layout& other) const noexcept;
    };

    inline bool descriptor_binding_layout::operator==(const descriptor_binding_layout& other) const noexcept
    {
        return binding_index == other.binding_index && type == other.type && count == other.count &&
               stages == other.stages && flags == other.flags;
    }

    inline bool descriptor_binding_layout::operator!=(const descriptor_binding_layout& other) const noexcept
    {
        return !(*this == other);
    }

    enum class descriptor_set_layout_flags
    {
        none = 0x0,
        push = 0x1,
        descriptor_buffer = 0x2,
    };

    struct push_constant_range
    {
        uint32_t offset;
        uint32_t range;
        enum_mask<shader_stage> stages;
    };

    struct pipeline_layout_desc
    {
        vector<typed_rhi_handle<rhi_handle_type::descriptor_set_layout>> descriptor_set_layouts;
        vector<push_constant_range> push_constants;
    };

    inline bool operator==(const push_constant_range& lhs, const push_constant_range& rhs) noexcept
    {
        return lhs.offset == rhs.offset && lhs.range == rhs.range && lhs.stages == rhs.stages;
    }

    inline bool operator!=(const push_constant_range& lhs, const push_constant_range& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    inline bool operator==(const pipeline_layout_desc& lhs, const pipeline_layout_desc& rhs) noexcept
    {
        return lhs.descriptor_set_layouts == rhs.descriptor_set_layouts && lhs.push_constants == rhs.push_constants;
    }

    enum class primitive_topology
    {
        point_list,
        line_list,
        line_strip,
        triangle_list,
        triangle_strip,
        triangle_fan
    };

    enum class index_format
    {
        uint8,
        uint16,
        uint32,
    };

    struct input_assembly_desc
    {
        primitive_topology topology;
    };

    struct tessellation_desc
    {
        uint32_t patch_control_points;
    };

    enum class polygon_mode
    {
        fill,
        line,
        point,
    };

    enum class cull_mode
    {
        none = 0x00,
        front = 0x01,
        back = 0x02,
    };

    enum class vertex_winding
    {
        clockwise,
        counter_clockwise,
    };

    struct depth_bias
    {
        float constant_factor;
        float clamp;
        float slope_factor;
    };

    struct rasterization_state
    {
        bool depth_clamp_enable;
        bool rasterizer_discard_enable;
        polygon_mode polygon_mode;
        enum_mask<cull_mode> cull_mode;
        vertex_winding vertex_winding;
        optional<depth_bias> depth_bias;
        float line_width;
    };

    struct sample_shading
    {
        float min_sample_shading;
        vector<uint32_t> sample_mask;
    };

    struct multisample_state
    {
        image_sample_count sample_count;
        optional<sample_shading> sample_shading;
        bool alpha_to_coverage;
        bool alpha_to_one;
    };

    enum class stencil_op
    {
        keep,
        zero,
        replace,
        increment_and_clamp,
        decrement_and_clamp,
        invert,
        increment_and_wrap,
        decrement_and_wrap,
    };

    struct stencil_op_state
    {
        stencil_op fail_op;
        stencil_op pass_op;
        stencil_op depth_fail_op;
        compare_op compare_op;
        uint32_t compare_mask;
        uint32_t write_mask;
        uint32_t reference;
    };

    struct depth_test
    {
        bool write_enable;
        compare_op compare_op;
        bool depth_bounds_test_enable;
        float min_depth_bounds;
        float max_depth_bounds;
    };

    struct stencil_test
    {
        stencil_op_state front;
        stencil_op_state back;
    };

    struct depth_stencil_state
    {
        optional<depth_test> depth;
        optional<stencil_test> stencil;
    };

    enum class blend_factor
    {
        zero,
        one,
        src_color,
        one_minus_src_color,
        dst_color,
        one_minus_dst_color,
        src_alpha,
        one_minus_src_alpha,
        dst_alpha,
        one_minus_dst_alpha,
        constant_color,
        one_minus_constant_color,
        constant_alpha,
        one_minus_constant_alpha,
    };

    enum class blend_op
    {
        add,
        subtract,
        reverse_subtract,
        min,
        max,
    };

    struct color_blend_attachment
    {
        bool blend_enable;
        blend_factor src_color_blend_factor;
        blend_factor dst_color_blend_factor;
        blend_op color_blend_op;
        blend_factor src_alpha_blend_factor;
        blend_factor dst_alpha_blend_factor;
        blend_op alpha_blend_op;
    };

    struct color_blend_state
    {
        vector<color_blend_attachment> attachments;
        array<float, 4> blend_constants;
    };

    enum class vertex_input_rate
    {
        vertex,
        instance,
    };

    struct vertex_binding_desc
    {
        uint32_t binding_index;
        uint32_t stride;
        vertex_input_rate input_rate;
    };

    struct vertex_attribute_desc
    {
        uint32_t binding_index;
        uint32_t location_index;
        buffer_format format;
        uint32_t offset;
    };

    struct vertex_input_desc
    {
        vector<vertex_binding_desc> bindings;
        vector<vertex_attribute_desc> attributes;
    };

    struct graphics_pipeline_desc
    {
        vector<image_format> color_attachment_formats;
        optional<image_format> depth_attachment_format;
        optional<image_format> stencil_attachment_format;

        vector<byte> vertex_shader;
        vector<byte> tessellation_control_shader;
        vector<byte> tessellation_evaluation_shader;
        vector<byte> geometry_shader;
        vector<byte> fragment_shader;

        input_assembly_desc input_assembly;
        optional<vertex_input_desc> vertex_input;
        optional<tessellation_desc> tessellation;
        multisample_state multisample;
        rasterization_state rasterization;
        depth_stencil_state depth_stencil;
        color_blend_state color_blend;

        typed_rhi_handle<rhi_handle_type::pipeline_layout> layout;

        string name;
    };

    struct compute_pipeline_desc
    {
        vector<byte> compute_shader;
        typed_rhi_handle<rhi_handle_type::pipeline_layout> layout;
        string name;
    };

    struct buffer_binding_descriptor
    {
        uint32_t index;
        descriptor_type type;
        uint32_t offset;
        uint32_t size;
        typed_rhi_handle<rhi_handle_type::buffer> buffer;
    };

    struct image_binding_info
    {
        typed_rhi_handle<rhi_handle_type::image> image;
        typed_rhi_handle<rhi_handle_type::sampler> sampler;
        image_layout layout;
    };

    struct image_binding_descriptor
    {
        uint32_t index;
        descriptor_type type;
        uint32_t array_offset;
        vector<image_binding_info> images;
    };

    struct sampler_binding_descriptor
    {
        uint32_t index;
        vector<typed_rhi_handle<rhi_handle_type::sampler>> samplers;
    };

    struct descriptor_set_desc
    {
        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> layout;
        vector<buffer_binding_descriptor> buffers;
        vector<image_binding_descriptor> images;
        vector<sampler_binding_descriptor> samplers;
    };
} // namespace tempest::rhi

namespace tempest
{
    template <rhi::rhi_handle_type T>
    struct hash<rhi::typed_rhi_handle<T>>
    {
        size_t operator()(const rhi::typed_rhi_handle<T>& handle) const noexcept
        {
            return ::tempest::hash_combine(handle.id, handle.generation, to_underlying(T));
        }
    };
} // namespace tempest

#endif // tempest_rhi_rhi_types_hpp
