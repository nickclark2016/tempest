#ifndef tempest_rhi_rhi_hpp
#define tempest_rhi_rhi_hpp

#include <tempest/rhi_types.hpp>

#include <tempest/enum.hpp>
#include <tempest/expected.hpp>
#include <tempest/input.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

namespace tempest::rhi
{
    class instance
    {
      public:
        instance(const instance&) = delete;
        instance(instance&&) noexcept = delete;
        virtual ~instance() = default;

        instance& operator=(const instance&) = delete;
        instance& operator=(instance&&) noexcept = delete;

        virtual vector<rhi_device_description> get_devices() const noexcept = 0;
        virtual device& acquire_device(uint32_t device_index) noexcept = 0;

      protected:
        instance() = default;
    };

    class device
    {
      public:
        device(const device&) = delete;
        device(device&&) noexcept = delete;
        virtual ~device() = default;

        device& operator=(const device&) = delete;
        device& operator=(device&&) noexcept = delete;

        virtual typed_rhi_handle<rhi_handle_type::buffer> create_buffer(const buffer_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::image> create_image(const image_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::fence> create_fence(const fence_info& info) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::semaphore> create_semaphore(const semaphore_info& info) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::render_surface> create_render_surface(
            const render_surface_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::descriptor_set_layout> create_descriptor_set_layout(
            const vector<descriptor_binding_layout>& desc,
            enum_mask<descriptor_set_layout_flags> flags =
                make_enum_mask(descriptor_set_layout_flags::none)) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::pipeline_layout> create_pipeline_layout(
            const pipeline_layout_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::graphics_pipeline> create_graphics_pipeline(
            const graphics_pipeline_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::descriptor_set> create_descriptor_set(
            const descriptor_set_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::compute_pipeline> create_compute_pipeline(
            const compute_pipeline_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::sampler> create_sampler(const sampler_desc& desc) noexcept = 0;

        virtual void destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept = 0;
        virtual void destroy_image(typed_rhi_handle<rhi_handle_type::image> handle) noexcept = 0;
        virtual void destroy_fence(typed_rhi_handle<rhi_handle_type::fence> handle) noexcept = 0;
        virtual void destroy_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) noexcept = 0;
        virtual void destroy_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept = 0;
        virtual void destroy_descriptor_set_layout(
            typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept = 0;
        virtual void destroy_pipeline_layout(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept = 0;
        virtual void destroy_graphics_pipeline(
            typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle) noexcept = 0;
        virtual void destroy_descriptor_set(typed_rhi_handle<rhi_handle_type::descriptor_set> handle) noexcept = 0;
        virtual void destroy_compute_pipeline(typed_rhi_handle<rhi_handle_type::compute_pipeline> handle) noexcept = 0;
        virtual void destroy_sampler(typed_rhi_handle<rhi_handle_type::sampler> handle) noexcept = 0;

        virtual work_queue& get_primary_work_queue() noexcept = 0;
        virtual work_queue& get_dedicated_transfer_queue() noexcept = 0;
        virtual work_queue& get_dedicated_compute_queue() noexcept = 0;

        virtual void recreate_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle,
                                             const render_surface_desc& desc) noexcept = 0;

        virtual render_surface_info query_render_surface_info(const window_surface& window) noexcept = 0;
        virtual span<const typed_rhi_handle<rhi_handle_type::image>> get_render_surfaces(
            typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept = 0;
        virtual expected<swapchain_image_acquire_info_result, swapchain_error_code> acquire_next_image(
            typed_rhi_handle<rhi_handle_type::render_surface> swapchain,
            typed_rhi_handle<rhi_handle_type::fence> signal_fence =
                typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept = 0;

        virtual bool is_signaled(typed_rhi_handle<rhi_handle_type::fence> fence) const noexcept = 0;
        virtual bool reset(span<const typed_rhi_handle<rhi_handle_type::fence>> fences) const noexcept = 0;
        virtual bool wait(span<const typed_rhi_handle<rhi_handle_type::fence>> fences) const noexcept = 0;

        // Buffer Management
        virtual byte* map_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept = 0;
        virtual void unmap_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept = 0;
        virtual void flush_buffers(span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers) noexcept = 0;

        // Swapchain info
        virtual uint32_t get_render_surface_width(
            typed_rhi_handle<rhi_handle_type::render_surface> surface) const noexcept = 0;
        virtual uint32_t get_render_surface_height(
            typed_rhi_handle<rhi_handle_type::render_surface> surface) const noexcept = 0;
        virtual const window_surface* get_window_surface(
            typed_rhi_handle<rhi_handle_type::render_surface> surface) const noexcept = 0;

        // Descriptor buffer support
        virtual bool supports_descriptor_buffers() const noexcept = 0;
        virtual size_t get_descriptor_buffer_alignment() const noexcept = 0;
        virtual size_t get_descriptor_set_layout_size(
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> layout) const noexcept = 0;
        virtual size_t write_descriptor_buffer(const descriptor_set_desc& desc, byte* dest,
                                               size_t offset) const noexcept = 0;

        // Miscellaneous
        virtual void release_resources() = 0;
        virtual void finish_frame() = 0;

        virtual uint32_t frames_in_flight() const noexcept = 0;

      protected:
        device() = default;
    };

    class work_queue
    {
      public:
        struct semaphore_submit_info
        {
            typed_rhi_handle<rhi_handle_type::semaphore> semaphore;
            uint64_t value;
            enum_mask<pipeline_stage> stages;
        };

        struct submit_info
        {
            vector<typed_rhi_handle<rhi_handle_type::command_list>> command_lists;
            vector<semaphore_submit_info> wait_semaphores;
            vector<semaphore_submit_info> signal_semaphores;
        };

        struct swapchain_image_present_info
        {
            typed_rhi_handle<rhi_handle_type::render_surface> render_surface;
            uint32_t image_index;
        };

        struct present_info
        {
            vector<swapchain_image_present_info> swapchain_images;
            vector<typed_rhi_handle<rhi_handle_type::semaphore>> wait_semaphores;
        };

        enum class present_result
        {
            success,
            suboptimal,
            out_of_date,
            error,
        };

        work_queue(const work_queue&) = delete;
        work_queue(work_queue&&) noexcept = delete;
        virtual ~work_queue() = default;

        work_queue& operator=(const work_queue&) = delete;
        work_queue& operator=(work_queue&&) noexcept = delete;

        virtual typed_rhi_handle<rhi_handle_type::command_list> get_next_command_list() noexcept = 0;

        virtual bool submit(span<const submit_info> infos,
                            typed_rhi_handle<rhi_handle_type::fence> fence =
                                typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept = 0;
        virtual vector<present_result> present(const present_info& info) noexcept = 0;

        // Commands
        struct image_barrier
        {
            typed_rhi_handle<rhi_handle_type::image> image;
            image_layout old_layout;
            image_layout new_layout;
            enum_mask<pipeline_stage> src_stages;
            enum_mask<memory_access> src_access;
            enum_mask<pipeline_stage> dst_stages;
            enum_mask<memory_access> dst_access;
            work_queue* src_queue = nullptr;
            work_queue* dst_queue = nullptr;
        };

        struct buffer_barrier
        {
            typed_rhi_handle<rhi_handle_type::buffer> buffer;
            enum_mask<pipeline_stage> src_stages;
            enum_mask<memory_access> src_access;
            enum_mask<pipeline_stage> dst_stages;
            enum_mask<memory_access> dst_access;
            work_queue* src_queue = nullptr;
            work_queue* dst_queue = nullptr;
            size_t offset = 0;
            size_t size = numeric_limits<size_t>::max();
        };

        enum class load_op
        {
            load,
            clear,
            dont_care,
        };

        enum class store_op
        {
            store,
            dont_care,
        };

        struct color_attachment_info
        {
            typed_rhi_handle<rhi_handle_type::image> image;
            image_layout layout;
            float clear_color[4];
            load_op load_op;
            store_op store_op;
        };

        struct depth_attachment_info
        {
            typed_rhi_handle<rhi_handle_type::image> image;
            image_layout layout;
            float clear_depth;
            load_op load_op;
            store_op store_op;
        };

        struct stencil_attachment_info
        {
            typed_rhi_handle<rhi_handle_type::image> image;
            image_layout layout;
            uint32_t clear_stencil;
            load_op load_op;
            store_op store_op;
        };

        struct render_pass_info
        {
            vector<color_attachment_info> color_attachments;
            optional<depth_attachment_info> depth_attachment;
            optional<stencil_attachment_info> stencil_attachment;
            int32_t x;
            int32_t y;
            uint32_t width;
            uint32_t height;
            uint32_t layers;
            string name;
        };

        virtual void begin_command_list(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                        bool one_time_submit) noexcept = 0;
        virtual void end_command_list(typed_rhi_handle<rhi_handle_type::command_list> command_list) noexcept = 0;

        // Image Commands
        virtual void transition_image(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                      span<const image_barrier> image_barriers) noexcept = 0;
        virtual void clear_color_image(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                       typed_rhi_handle<rhi_handle_type::image> image, image_layout layout, float r,
                                       float g, float b, float a) noexcept = 0;
        virtual void blit(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::image> src, image_layout src_layout, uint32_t src_mip,
                          typed_rhi_handle<rhi_handle_type::image> dst, image_layout dst_layout,
                          uint32_t dst_mip) noexcept = 0;
        virtual void generate_mip_chain(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                        typed_rhi_handle<rhi_handle_type::image> img, image_layout current_layout,
                                        uint32_t base_mip = 0,
                                        uint32_t mip_count = numeric_limits<uint32_t>::max()) = 0;

        // Buffer and Image Commands
        virtual void copy(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::buffer> src, typed_rhi_handle<rhi_handle_type::buffer> dst,
                          size_t src_offset = 0, size_t dst_offset = 0,
                          size_t byte_count = numeric_limits<size_t>::max()) noexcept = 0;
        virtual void fill(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::buffer> handle, size_t offset, size_t size,
                          uint32_t data) noexcept = 0;
        virtual void copy(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::buffer> src, typed_rhi_handle<rhi_handle_type::image> dst,
                          image_layout layout, size_t src_offset = 0, uint32_t dst_mip = 0) noexcept = 0;

        // Barrier commands
        virtual void pipeline_barriers(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                       span<const image_barrier> img_barriers,
                                       span<const buffer_barrier> buf_barriers) noexcept = 0;

        // Rendering commands
        virtual void begin_rendering(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                     const render_pass_info& render_pass_info) noexcept = 0;
        virtual void end_rendering(typed_rhi_handle<rhi_handle_type::command_list> command_list) noexcept = 0;
        virtual void bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::graphics_pipeline> pipeline) noexcept = 0;
        virtual void draw(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::buffer> indirect_buffer, uint32_t offset,
                          uint32_t draw_count, uint32_t stride) noexcept = 0;
        virtual void draw(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t vertex_count,
                          uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) noexcept = 0;
        virtual void draw(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t index_count,
                          uint32_t instance_count, uint32_t first_index, int32_t vertex_offset,
                          uint32_t first_instance) noexcept = 0;
        virtual void bind_index_buffer(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                       typed_rhi_handle<rhi_handle_type::buffer> buffer, uint32_t offset,
                                       rhi::index_format index_type) noexcept = 0;
        virtual void bind_vertex_buffers(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                         uint32_t first_binding,
                                         span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers,
                                         span<const size_t> offsets) noexcept = 0;
        virtual void set_scissor_region(typed_rhi_handle<rhi_handle_type::command_list> command_list, int32_t x,
                                        int32_t y, uint32_t width, uint32_t height,
                                        uint32_t region_index = 0) noexcept = 0;
        virtual void set_viewport(typed_rhi_handle<rhi_handle_type::command_list> command_list, float x, float y,
                                  float width, float height, float min_depth, float max_depth,
                                  uint32_t viewport_index = 0, bool flipped = false) noexcept = 0;
        virtual void set_cull_mode(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                   enum_mask<cull_mode> cull) noexcept = 0;

        // Compute commands
        virtual void bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::compute_pipeline> pipeline) noexcept = 0;
        virtual void dispatch(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t x, uint32_t y,
                              uint32_t z) noexcept = 0;

        // Descriptor commands
        virtual void bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout, bind_point point,
                          uint32_t first_set_index, span<const typed_rhi_handle<rhi_handle_type::descriptor_set>> sets,
                          span<const uint32_t> dynamic_offsets = {}) noexcept = 0;

        virtual void push_constants(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                    typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout,
                                    enum_mask<rhi::shader_stage> stages, uint32_t offset,
                                    span<const byte> values) noexcept = 0;

        virtual void push_descriptors(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                      typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout,
                                      bind_point point, uint32_t set_index,
                                      span<const buffer_binding_descriptor> buffers,
                                      span<const image_binding_descriptor> images,
                                      span<const sampler_binding_descriptor> samplers) noexcept = 0;

        template <typename T>
            requires is_trivially_copyable_v<T> && is_standard_layout_v<T>
        void typed_push_constants(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                  typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout,
                                  enum_mask<rhi::shader_stage> stages, uint32_t offset, const T& value) noexcept
        {
            static_assert(sizeof(T) % 4 == 0, "Push constant size must be a multiple of 4 bytes");
            push_constants(command_list, pipeline_layout, stages, offset,
                           span<const byte>{reinterpret_cast<const byte*>(&value), sizeof(T)});
        }

        virtual void reset(uint64_t frame_in_flight) = 0;

      protected:
        work_queue() = default;
    };

    class window_surface
    {
      public:
        enum class cursor_shape
        {
            arrow,
            ibeam,
            crosshair,
            hand,
            resize_horizontal,
            resize_vertical,
        };

        struct video_mode
        {
            uint32_t width;
            uint32_t height;
            uint32_t refresh_rate;
            uint8_t red_bits;
            uint8_t green_bits;
            uint8_t blue_bits;
        };

        struct monitor
        {
            int32_t work_x;
            int32_t work_y;
            uint32_t work_width;
            uint32_t work_height;

            int32_t x;
            int32_t y;

            float content_scale_x;
            float content_scale_y;

            string_view name;

            video_mode current_video_mode;
        };

        window_surface(const window_surface&) = delete;
        window_surface(window_surface&&) noexcept = delete;
        virtual ~window_surface() = default;

        window_surface& operator=(const window_surface&) = delete;
        window_surface& operator=(window_surface&&) noexcept = delete;

        virtual uint32_t width() const noexcept = 0;
        virtual uint32_t height() const noexcept = 0;
        virtual uint32_t framebuffer_width() const noexcept = 0;
        virtual uint32_t framebuffer_height() const noexcept = 0;
        virtual string name() const noexcept = 0;
        virtual bool should_close() const noexcept = 0;
        virtual bool minimized() const noexcept = 0;
        virtual bool is_cursor_disabled() const noexcept = 0;
        virtual void hide_cursor() noexcept = 0;
        virtual void disable_cursor() noexcept = 0;
        virtual void show_cursor() noexcept = 0;
        virtual bool is_focused() const noexcept = 0;

        virtual void close() = 0;

        // Set up input callbacks
        virtual void register_keyboard_callback(function<void(const core::key_state&)>&& cb) noexcept = 0;
        virtual void register_mouse_callback(function<void(const core::mouse_button_state&)>&& cb) noexcept = 0;
        virtual void register_cursor_callback(function<void(float, float)>&& cb) noexcept = 0;
        virtual void register_scroll_callback(function<void(float, float)>&& cb) noexcept = 0;
        virtual void register_character_input_callback(function<void(uint32_t)>&& cb) noexcept = 0;

        // Set up other miscellaneous callbacks
        virtual void register_close_callback(function<void()>&& cb) noexcept = 0;
        virtual void register_resize_callback(function<void(uint32_t, uint32_t)>&& cb) noexcept = 0;
        virtual void register_content_resize_callback(function<void(uint32_t, uint32_t)>&& cb) noexcept = 0;
        virtual void register_focus_callback(function<void(bool)>&& cb) noexcept = 0;
        virtual void register_minimize_callback(function<void(bool)>&& cb) noexcept = 0;
        virtual void register_cursor_enter_callback(function<void(bool)>&& cb) noexcept = 0;

        // Set up clipboard operations
        virtual void set_clipboard_text(const char* text) noexcept = 0;
        virtual const char* get_clipboard_text() noexcept = 0;

        // Cursor management
        virtual void set_cursor_shape(cursor_shape shape) noexcept = 0;

        // Monitor management
        virtual vector<monitor> get_monitors() const noexcept = 0;

      protected:
        window_surface() = default;
    };
} // namespace tempest::rhi

namespace tempest::rhi::vk
{
    extern unique_ptr<rhi::instance> create_instance() noexcept;
    extern unique_ptr<rhi::window_surface> create_window_surface(const rhi::window_surface_desc& desc) noexcept;
} // namespace tempest::rhi::vk

#endif // tempest_rhi_rhi_hpp
