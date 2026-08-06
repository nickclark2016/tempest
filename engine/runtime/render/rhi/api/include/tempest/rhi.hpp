#ifndef tempest_rhi_api_rhi_hpp
#define tempest_rhi_api_rhi_hpp

#include <tempest/api.hpp>
#include <tempest/bit.hpp>
#include <tempest/cstring_view.hpp>
#include <tempest/enum.hpp>
#include <tempest/expected.hpp>
#include <tempest/guid.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest::rhi
{
    class context;
    class device;
    class execution_port;
    class command_list;
    class render_surface;

    struct semaphore_handle
    {
        uint64_t handle;
    };

    struct event_handle
    {
        uint64_t handle;
    };

    struct buffer_handle
    {
        uint64_t handle;
        uint64_t gpu_address;
        void* cpu_address;
    };

    struct texture_handle
    {
        uint64_t handle;
    };

    struct texture_view_handle
    {
        uint64_t handle;
    };

    struct texture_view_descriptor
    {
        uint32_t heap_index;
        uint32_t heap_generation;
    };

    struct sampler_handle
    {
        uint64_t handle;
    };

    struct sampler_descriptor
    {
        uint32_t heap_index;
        uint32_t heap_generation;
    };

    struct native_wsi_handle
    {
        void* display;
        void* window;
    };

    enum class present_mode : uint8_t
    {
        vsync,
        immediate,
        mailbox,
    };

    enum class render_surface_format : uint8_t
    {
        unknown,
        rgba8_unorm,
        rgba8_srgb,
        bgra8_unorm,
        bgra8_srgb,
    };

    enum class memory_usage : int8_t
    {
        device_only,
        upload,
        readback,
    };

    enum class data_format : uint8_t
    {
        unknown,
        // 1 channel formats
        r8_unorm,
        r16_float,
        r32_float,
        // 2 channel formats
        rg8_unorm,
        rg16_float,
        rg32_float,
        // 4 channel formats
        rgba8_unorm,
        rgba8_srgb,
        rgba16_float,
        rgba32_float,
        // depth stencil formats
        depth16_unorm,
        depth24_unorm_stencil8,
        depth32_float,
        depth32_float_stencil8,
    };

    struct buffer_desc
    {
        uint64_t size;
        memory_usage usage;
        cstring_view name;
    };

    struct texture_desc
    {
        uint32_t width;
        uint32_t height;
        uint32_t depth = 1;
        uint32_t mip_levels = 1;
        uint32_t array_layers = 1;
        data_format format;
        memory_usage usage;
        cstring_view name;
    };

    struct texture_view_desc
    {
        optional<data_format> override_format = nullopt;
        uint32_t base_mip_level = 0;
        uint32_t mip_level_count = ~0U;
        uint32_t base_array_layer = 0;
        uint32_t array_layer_count = ~0U;
    };

    enum class filter_mode : uint8_t
    {
        nearest,
        linear,
    };

    enum class address_mode : uint8_t
    {
        repeat,
        mirrored_repeat,
        clamp_to_edge,
        clamp_to_border,
    };

    enum class mipmap_mode : uint8_t
    {
        nearest,
        linear,
    };

    enum class compare_op : uint8_t
    {
        never,
        less,
        equal,
        less_or_equal,
        greater,
        not_equal,
        greater_or_equal,
        always,
    };

    struct sampler_desc
    {
        filter_mode min_filter;
        filter_mode mag_filter;
        mipmap_mode mipmap_mode;
        address_mode address_u;
        address_mode address_v;
        address_mode address_w;
        float mip_lod_bias = 0.0F;
        float min_lod = 0.0F;
        float max_lod = 1000.0F; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) -- Arbitrary
                                 // large value to allow all mip levels by default.
        optional<float> max_anisotropy = nullopt;
        optional<compare_op> compare_op = nullopt;
        cstring_view name;
    };

    enum class pipeline_stage : uint32_t
    {
        top_of_pipe = 1 << 0,
        copy = 1 << 1,
        blit = 1 << 2,
        resolve = 1 << 3,
        clear = 1 << 4,
        all_transfer = copy | blit | resolve | clear,
        vertex = 1 << 5,
        tessellation_control = 1 << 6,
        tessellation_evaluation = 1 << 7,
        geometry = 1 << 8,
        fragment = 1 << 9,
        early_fragment_tests = 1 << 10,
        late_fragment_tests = 1 << 11,
        attachment_output = 1 << 12,
        all_graphics = vertex | tessellation_control | tessellation_evaluation | geometry | fragment |
                       early_fragment_tests | late_fragment_tests | attachment_output,
        compute = 1 << 13,
        build_acceleration_structure = 1 << 14,
        copy_acceleration_structure = 1 << 15,
        ray_tracing = 1 << 16,
        all_compute = compute | build_acceleration_structure | copy_acceleration_structure | ray_tracing,
        bottom_of_pipe = 1 << 17,
        all_commands = top_of_pipe | all_transfer | all_graphics | all_compute | bottom_of_pipe,
    };

    enum class resource_access : uint8_t
    {
        none = 0,
        read = 1 << 0,
        write = 1 << 1,
        read_write = read | write,
    };

    enum class image_layout : uint8_t
    {
        undefined,
        general,
        present,
    };

    enum class shader_stage : uint16_t
    {
        vertex = 1 << 0,
        tessellation_control = 1 << 1,
        tessellation_evaluation = 1 << 2,
        geometry = 1 << 3,
        fragment = 1 << 4,
        compute = 1 << 5,
        raygen = 1 << 6,
        any_hit = 1 << 7,
        closest_hit = 1 << 8,
        miss = 1 << 9,
        intersection = 1 << 10,
        callable = 1 << 11,
    };

    enum class polygon_mode : uint8_t
    {
        fill,
        line,
        point,
    };

    enum class cull_mode : uint8_t
    {
        none,
        front,
        back,
    };

    enum class vertex_winding_order : uint8_t
    {
        clockwise,
        counter_clockwise,
    };

    enum class blend_factor : uint8_t
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
    };

    enum class primitive_topology : uint8_t
    {
        point_list,
        line_list,
        line_strip,
        triangle_list,
        triangle_strip,
    };

    enum class stencil_op : uint8_t
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

    enum class stencil_face : uint8_t
    {
        front = 1 << 0,
        back = 1 << 1,
        front_and_back = front | back,
    };

    struct specialization_constant
    {
        uint32_t constant_id;
        span<const byte> data;
    };

    struct shader_module_desc
    {
        shader_stage stage;
        span<const byte> ir_code;
        cstring_view entry_point;
        span<const specialization_constant> specialization_constants;
    };

    struct depth_bias_state
    {
        float constant_factor = 0.0F;
        float clamp = 0.0F;
        float slope_factor = 0.0F;
    };

    struct depth_bounds_state
    {
        float min_depth = 0.0F;
        float max_depth = 1.0F;
    };

    struct stencil_op_state
    {
        stencil_op fail_op = stencil_op::keep;
        stencil_op pass_op = stencil_op::keep;
        stencil_op depth_fail_op = stencil_op::keep;
        compare_op compare_op = compare_op::always;
        uint32_t compare_mask = 0xFFFFFFFFU; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                                             // -- Mask for all bits of the stencil buffer by default.
        uint32_t write_mask = 0xFFFFFFFFU; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) --
                                           // Mask for all bits of the stencil buffer by default.
        uint32_t reference = 0;
    };

    struct stencil_state
    {
        stencil_op_state front;
        stencil_op_state back;
    };

    struct rasterization_state
    {
        polygon_mode polygon_mode = polygon_mode::fill;
        cull_mode cull_mode = cull_mode::back;
        vertex_winding_order front_face = vertex_winding_order::counter_clockwise;
        optional<depth_bias_state> depth_bias;
    };

    struct depth_stencil_state
    {
        bool depth_test_enable = true;
        bool depth_write_enable = true;
        compare_op depth_compare_op = compare_op::less;
        optional<depth_bounds_state> depth_bounds;
        optional<stencil_state> stencil;
    };

    struct attachment_blend_state
    {
        bool blend_enable = false;
        blend_factor src_color_blend_factor = blend_factor::one;
        blend_factor dst_color_blend_factor = blend_factor::zero;
        blend_factor src_alpha_blend_factor = blend_factor::one;
        blend_factor dst_alpha_blend_factor = blend_factor::zero;
    };

    struct graphics_pipeline_desc
    {
        span<const shader_module_desc> shader_modules;
        span<const data_format> color_attachment_formats;
        optional<data_format> depth_stencil_attachment_format;
        primitive_topology primitive_topology = primitive_topology::triangle_list;
        rasterization_state rasterization_state;
        depth_stencil_state depth_stencil_state;
        span<const attachment_blend_state> color_attachment_blend_states;
    };

    struct graphics_pipeline_handle
    {
        uint64_t handle;
    };

    struct compute_pipeline_desc
    {
        shader_module_desc shader_module;
    };

    struct compute_pipeline_handle
    {
        uint64_t handle;
    };

    struct texture_barrier
    {
        texture_handle texture;
        pipeline_stage src_stage;
        resource_access src_access;
        pipeline_stage dst_stage;
        resource_access dst_access;
        image_layout old_layout;
        image_layout new_layout;

        execution_port* src_queue = nullptr;
        execution_port* dst_queue = nullptr;
    };

    struct buffer_barrier
    {
        buffer_handle buffer;
        pipeline_stage src_stage;
        resource_access src_access;
        pipeline_stage dst_stage;
        resource_access dst_access;

        execution_port* src_queue = nullptr;
        execution_port* dst_queue = nullptr;
    };

    enum class load_op : uint8_t
    {
        load,
        clear,
        dont_care,
    };

    enum class store_op : uint8_t
    {
        store,
        dont_care,
    };

    enum class index_type : uint8_t
    {
        uint16,
        uint32,
    };

    struct clear_color_value
    {
        float r;
        float g;
        float b;
        float a;
    };

    struct clear_depth_stencil_value
    {
        float depth;
        uint32_t stencil;
    };

    struct color_attachment
    {
        texture_handle view;
        load_op load_op;
        store_op store_op;
        clear_color_value clear_value;
    };

    struct depth_stencil_attachment
    {
        texture_handle view;
        load_op depth_load_op;
        store_op depth_store_op;
        load_op stencil_load_op;
        store_op stencil_store_op;
        clear_depth_stencil_value clear_value;
    };

    struct copy_buffer_desc
    {
        buffer_handle src_buffer;
        uint64_t src_offset;
        buffer_handle dst_buffer;
        uint64_t dst_offset;
        uint64_t size;
    };

    struct copy_buffer_image_desc
    {
        buffer_handle src_buffer;
        uint64_t src_offset;
        texture_handle dst_texture;
        uint32_t dst_mip_level;
        uint32_t dst_array_layer;
        uint32_t dst_x;
        uint32_t dst_y;
        uint32_t dst_z;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    };

    struct copy_image_buffer_desc
    {
        texture_handle src_texture;
        uint32_t src_mip_level;
        uint32_t src_array_layer;
        uint32_t src_x;
        uint32_t src_y;
        uint32_t src_z;
        buffer_handle dst_buffer;
        uint64_t dst_offset;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    };

    struct device_sync_point
    {
        semaphore_handle semaphore;
        uint64_t value;
        enum_mask<pipeline_stage> stages;
    };

    struct host_sync_point
    {
        semaphore_handle semaphore;
        uint64_t value;
    };

    class TEMPEST_API command_list
    {
      public:
        command_list(const command_list&) = delete;
        command_list(command_list&&) noexcept = delete;
        virtual ~command_list() = default;

        command_list& operator=(const command_list&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        command_list& operator=(command_list&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        virtual auto begin() -> void = 0;
        virtual auto end() -> void = 0;

        // Synchronization
        virtual auto pipeline_barrier(span<const texture_barrier> texture_barriers,
                                      span<const buffer_barrier> buffer_barriers) -> void = 0;
        virtual auto signal_event(event_handle event, enum_mask<pipeline_stage> stages) -> void = 0;
        virtual auto wait_event(event_handle event, enum_mask<pipeline_stage> stages) -> void = 0;
        virtual auto reset_event(event_handle event) -> void = 0;

        // General commands
        virtual auto push_constants(enum_mask<shader_stage> stages, uint32_t offset, span<const byte> data) -> void = 0;

        // Rendering
        virtual auto begin_render_pass(span<const color_attachment> color_attachments,
                                       optional<depth_stencil_attachment> depth_stencil_attachment) -> void = 0;
        virtual auto end_render_pass() -> void = 0;
        virtual auto bind_pipeline(graphics_pipeline_handle pipeline) -> void = 0;
        virtual auto set_viewport(float x, float y, float width, float height, // NOLINT(readability-identifier-length)
                                  float min_depth, float max_depth) -> void = 0;
        virtual auto set_scissor(int32_t x, int32_t y, uint32_t width, // NOLINT(readability-identifier-length)
                                 uint32_t height) -> void = 0;
        virtual auto set_depth_bias(float constant_factor, float clamp, float slope_factor) -> void = 0;
        virtual auto set_stencil_reference(uint32_t reference) -> void = 0;
        virtual auto set_stencil_compare_mask(uint32_t compare_mask) -> void = 0;
        virtual auto set_stencil_write_mask(uint32_t write_mask) -> void = 0;
        virtual auto bind_index_buffer(buffer_handle buffer, index_type type, uint64_t offset) -> void = 0;
        virtual auto draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex,
                          uint32_t first_instance) -> void = 0;
        virtual auto draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index,
                                  int32_t vertex_offset, uint32_t first_instance) -> void = 0;
        virtual auto draw_indirect(buffer_handle buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
            -> void = 0;
        virtual auto draw_indexed_indirect(buffer_handle buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
            -> void = 0;
        virtual auto draw_indirect_count(buffer_handle buffer, uint64_t offset, buffer_handle count_buffer,
                                         uint64_t count_buffer_offset, uint32_t max_draw_count, uint32_t stride)
            -> void = 0;
        virtual auto draw_indexed_indirect_count(buffer_handle buffer, uint64_t offset, buffer_handle count_buffer,
                                                 uint64_t count_buffer_offset, uint32_t max_draw_count, uint32_t stride)
            -> void = 0;

        // Compute
        virtual auto bind_pipeline(compute_pipeline_handle pipeline) -> void = 0;
        virtual auto dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) -> void = 0;

        // Transfer
        virtual auto copy_buffers(span<const copy_buffer_desc> copy_descriptions) -> void = 0;
        virtual auto copy_buffers_to_images(span<const copy_buffer_image_desc> copy_descriptions) -> void = 0;
        virtual auto copy_images_to_buffers(span<const copy_image_buffer_desc> copy_descriptions) -> void = 0;

      protected:
        command_list() = default;
    };

    struct device_features
    {
        bool ray_tracing;
        bool mesh_shading;
        bool ray_query;
    };

    enum class device_type : uint8_t
    {
        discrete_gpu,
        integrated_gpu,
        virtual_gpu,
        cpu,
        unknown,
    };

    enum class device_vendor : uint8_t
    {
        amd,
        apple,
        arm,
        intel,
        khronos,
        nvidia,
        qualcomm,
        unknown,
    };

    struct device_desc
    {
        guid device_uuid;
        device_features features;
        cstring_view name;
        device_vendor vendor;
    };

    enum class graphics_api : uint8_t
    {
        vulkan
    };

    struct context_desc
    {
        cstring_view application_name;
        uint32_t version_major;
        uint32_t version_minor;
        uint32_t version_patch;
        bool enable_api_validation;
        graphics_api api;
    };

    enum class context_creation_error : uint8_t
    {
        unsupported_api,
        context_creation_failed,
        no_valid_devices_found,
        unknown,
    };

    class TEMPEST_API context
    {
      public:
        context(const context&) = delete;
        context(context&&) noexcept = delete;
        virtual ~context() = default;

        context& operator=(const context&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        context& operator=(context&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        [[nodiscard]] virtual auto enumerate_devices() -> span<const device_desc> = 0;
        [[nodiscard]] virtual auto create_device(guid device_uuid) -> unique_ptr<device> = 0;

      protected:
        context() = default;
    };

    TEMPEST_API auto create_context(const context_desc& desc) -> expected<unique_ptr<context>, context_creation_error>;

    enum class surface_color_space : uint8_t
    {
        srgb_nonlinear,
        extended_srgb_linear,
        hdr10_st2084,
    };

    struct surface_format
    {
        render_surface_format format;
        surface_color_space color_space;
    };

    struct surface_capabilities
    {
        uint32_t min_image_count;
        uint32_t max_image_count;
        uint32_t max_image_array_layers;
        vector<surface_format> supported_formats;
    };

    struct render_surface_desc
    {
        native_wsi_handle native_window_handle;
        surface_format format;
        uint32_t width;
        uint32_t height;
        uint32_t min_image_count;
        uint32_t preferred_image_count;
    };

    class TEMPEST_API device
    {
      public:
        device(const device&) = delete;
        device(device&&) noexcept = delete;
        virtual ~device() = default;

        device& operator=(const device&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        device& operator=(device&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        virtual auto wait_idle() -> void = 0;
        virtual auto wait_for_sync(host_sync_point sync_point) -> void = 0;

        // Capabilities
        [[nodiscard]] virtual auto is_ray_tracing_supported() const -> bool = 0;
        [[nodiscard]] virtual auto is_mesh_shading_supported() const -> bool = 0;
        [[nodiscard]] virtual auto is_ray_query_supported() const -> bool = 0;

        // Surface management
        [[nodiscard]] virtual auto get_surface_capabilities(render_surface& surface) -> surface_capabilities = 0;
        [[nodiscard]] virtual auto create_render_surface(const render_surface_desc& desc)
            -> unique_ptr<render_surface> = 0;

        // Execution ports
        [[nodiscard]] virtual auto get_graphics_execution_port() -> execution_port& = 0;
        [[nodiscard]] virtual auto get_async_compute_execution_port() -> execution_port& = 0;
        [[nodiscard]] virtual auto get_async_transfer_execution_port() -> execution_port& = 0;

        // Resource creation
        [[nodiscard]] virtual auto create_buffer(const buffer_desc& desc) -> buffer_handle = 0;
        [[nodiscard]] virtual auto create_texture(const texture_desc& desc) -> texture_handle = 0;
        [[nodiscard]] virtual auto create_texture_view(texture_handle texture, const texture_view_desc& desc)
            -> texture_view_handle = 0;
        [[nodiscard]] virtual auto create_sampler(const sampler_desc& desc) -> sampler_handle = 0;
        [[nodiscard]] virtual auto create_graphics_pipeline(const graphics_pipeline_desc& desc)
            -> graphics_pipeline_handle = 0;
        [[nodiscard]] virtual auto create_compute_pipeline(const compute_pipeline_desc& desc)
            -> compute_pipeline_handle = 0;
        [[nodiscard]] virtual auto create_event() -> event_handle = 0;
        [[nodiscard]] virtual auto create_timeline_semaphore() -> semaphore_handle = 0;
        [[nodiscard]] virtual auto create_binary_semaphore() -> semaphore_handle = 0;

        // Resource destruction
        virtual auto destroy_buffer(buffer_handle buffer) -> void = 0;
        virtual auto destroy_texture(texture_handle texture) -> void = 0;
        virtual auto destroy_texture_view(texture_view_handle view) -> void = 0;
        virtual auto destroy_sampler(sampler_handle sampler) -> void = 0;
        virtual auto destroy_graphics_pipeline(graphics_pipeline_handle pipeline) -> void = 0;
        virtual auto destroy_compute_pipeline(compute_pipeline_handle pipeline) -> void = 0;
        virtual auto destroy_event(event_handle event) -> void = 0;
        virtual auto destroy_semaphore(semaphore_handle semaphore) -> void = 0;

      protected:
        device() = default;
    };

    enum class submit_error : uint8_t
    {
        out_of_device_memory,
        out_of_host_memory,
        device_lost,
        unspecified,
    };

    enum class command_list_lifetime : uint8_t
    {
        /**
         * Commands recorded on this command list are short-lived and should finish execution in the same frame they are
         * submitted.
         */
        transient,

        /**
         * Commands recorded on this command list are long-lived and may execute across multiple frames.
         */
        long_lived,
    };

    class TEMPEST_API execution_port
    {
      public:
        execution_port(const execution_port&) = delete;
        execution_port(execution_port&&) noexcept = delete;
        virtual ~execution_port() = default;

        execution_port& operator=(const execution_port&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        execution_port& operator=(execution_port&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        virtual auto wait_idle() -> void = 0;

        // Command list management
        [[nodiscard]] virtual auto acquire_command_list(
            uint32_t thread_id = 0, command_list_lifetime lifetime = command_list_lifetime::transient)
            -> command_list& = 0;
        [[nodiscard]] virtual auto submit(span<const command_list*> commands,
                                          span<const device_sync_point> wait_semaphores,
                                          span<const device_sync_point> signal_semaphores)
            -> expected<void, submit_error> = 0;

      protected:
        execution_port() = default;
    };

    struct swapchain_image
    {
        texture_handle texture;
        texture_view_handle view;
        uint32_t swapchain_image_index;
    };

    enum class swapchain_error : uint8_t
    {
        out_of_date,
        suboptimal,
        unspecified,
    };

    class TEMPEST_API render_surface
    {
      public:
        render_surface(const render_surface&) = delete;
        render_surface(render_surface&&) noexcept = delete;
        virtual ~render_surface() = default;

        render_surface& operator=(const render_surface&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        render_surface& operator=(render_surface&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        [[nodiscard]] virtual auto get_format() const -> render_surface_format = 0;
        [[nodiscard]] virtual auto get_present_mode() const -> present_mode = 0;
        [[nodiscard]] virtual auto get_width() const -> uint32_t = 0;
        [[nodiscard]] virtual auto get_height() const -> uint32_t = 0;
        [[nodiscard]] virtual auto acquire_next_image(device_sync_point signal_values, uint64_t timeout_ns)
            -> expected<swapchain_image, swapchain_error> = 0;
        [[nodiscard]] virtual auto present(device_sync_point wait_values) -> expected<void, swapchain_error> = 0;

      protected:
        render_surface() = default;
    };
} // namespace tempest::rhi

#endif
