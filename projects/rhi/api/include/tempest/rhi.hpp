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

        virtual typed_rhi_handle<rhi_handle_type::BUFFER> create_buffer(const buffer_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::IMAGE> create_image(const image_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::FENCE> create_fence(const fence_info& info) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::SEMAPHORE> create_semaphore(const semaphore_info& info) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> create_render_surface(
            const render_surface_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT> create_descriptor_set_layout(
            const vector<descriptor_binding_layout>& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::PIPELINE_LAYOUT> create_pipeline_layout(
            const pipeline_layout_desc& desc) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE> create_graphics_pipeline(
            const graphics_pipeline_desc& desc) noexcept = 0;

        virtual void destroy_buffer(typed_rhi_handle<rhi_handle_type::BUFFER> handle) noexcept = 0;
        virtual void destroy_image(typed_rhi_handle<rhi_handle_type::IMAGE> handle) noexcept = 0;
        virtual void destroy_fence(typed_rhi_handle<rhi_handle_type::FENCE> handle) noexcept = 0;
        virtual void destroy_semaphore(typed_rhi_handle<rhi_handle_type::SEMAPHORE> handle) noexcept = 0;
        virtual void destroy_render_surface(typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> handle) noexcept = 0;
        virtual void destroy_descriptor_set_layout(
            typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT> handle) noexcept = 0;
        virtual void destroy_pipeline_layout(typed_rhi_handle<rhi_handle_type::PIPELINE_LAYOUT> handle) noexcept = 0;
        virtual void destroy_graphics_pipeline(
            typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE> handle) noexcept = 0;

        virtual work_queue& get_primary_work_queue() noexcept = 0;
        virtual work_queue& get_dedicated_transfer_queue() noexcept = 0;
        virtual work_queue& get_dedicated_compute_queue() noexcept = 0;

        virtual void recreate_render_surface(typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> handle,
                                             const render_surface_desc& desc) noexcept = 0;

        virtual render_surface_info query_render_surface_info(const window_surface& window) noexcept = 0;
        virtual span<const typed_rhi_handle<rhi_handle_type::IMAGE>> get_render_surfaces(
            typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> handle) noexcept = 0;
        virtual expected<swapchain_image_acquire_info_result, swapchain_error_code> acquire_next_image(
            typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> swapchain,
            typed_rhi_handle<rhi_handle_type::FENCE> signal_fence =
                typed_rhi_handle<rhi_handle_type::FENCE>::null_handle) noexcept = 0;

        virtual bool is_signaled(typed_rhi_handle<rhi_handle_type::FENCE> fence) const noexcept = 0;
        virtual bool reset(span<const typed_rhi_handle<rhi_handle_type::FENCE>> fences) const noexcept = 0;
        virtual bool wait(span<const typed_rhi_handle<rhi_handle_type::FENCE>> fences) const noexcept = 0;

        virtual void start_frame() = 0;
        virtual void end_frame() = 0;

        virtual uint32_t frames_in_flight() const noexcept = 0;

      protected:
        device() = default;
    };

    class work_queue
    {
      public:
        struct semaphore_submit_info
        {
            typed_rhi_handle<rhi_handle_type::SEMAPHORE> semaphore;
            uint64_t value;
            enum_mask<pipeline_stage> stages;
        };

        struct submit_info
        {
            vector<typed_rhi_handle<rhi_handle_type::COMMAND_LIST>> command_lists;
            vector<semaphore_submit_info> wait_semaphores;
            vector<semaphore_submit_info> signal_semaphores;
        };

        struct swapchain_image_present_info
        {
            typed_rhi_handle<rhi_handle_type::RENDER_SURFACE> render_surface;
            uint32_t image_index;
        };

        struct present_info
        {
            vector<swapchain_image_present_info> swapchain_images;
            vector<typed_rhi_handle<rhi_handle_type::SEMAPHORE>> wait_semaphores;
        };

        enum class present_result
        {
            SUCCESS,
            SUBOPTIMAL,
            OUT_OF_DATE,
            ERROR,
        };

        work_queue(const work_queue&) = delete;
        work_queue(work_queue&&) noexcept = delete;
        virtual ~work_queue() = default;

        work_queue& operator=(const work_queue&) = delete;
        work_queue& operator=(work_queue&&) noexcept = delete;

        virtual typed_rhi_handle<rhi_handle_type::COMMAND_LIST> get_next_command_list() noexcept = 0;

        virtual bool submit(span<const submit_info> infos,
                            typed_rhi_handle<rhi_handle_type::FENCE> fence =
                                typed_rhi_handle<rhi_handle_type::FENCE>::null_handle) noexcept = 0;
        virtual present_result present(const present_info& info) noexcept = 0;

        // Commands
        struct image_barrier
        {
            typed_rhi_handle<rhi_handle_type::IMAGE> image;
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
            typed_rhi_handle<rhi_handle_type::BUFFER> buffer;
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
            LOAD,
            CLEAR,
            DONT_CARE,
        };

        enum class store_op
        {
            STORE,
            DONT_CARE,
        };

        struct color_attachment_info
        {
            typed_rhi_handle<rhi_handle_type::IMAGE> image;
            image_layout layout;
            float clear_color[4];
            load_op load_op;
            store_op store_op;
        };

        struct depth_attachment_info
        {
            typed_rhi_handle<rhi_handle_type::IMAGE> image;
            image_layout layout;
            float clear_depth;
            load_op load_op;
            store_op store_op;
        };

        struct stencil_attachment_info
        {
            typed_rhi_handle<rhi_handle_type::IMAGE> image;
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
        };

        virtual void begin_command_list(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                                        bool one_time_submit) noexcept = 0;
        virtual void end_command_list(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list) noexcept = 0;

        // Image Commands
        virtual void transition_image(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                                      span<const image_barrier> image_barriers) noexcept = 0;
        virtual void clear_color_image(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                                       typed_rhi_handle<rhi_handle_type::IMAGE> image, image_layout layout, float r,
                                       float g, float b, float a) noexcept = 0;
        virtual void blit(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                          typed_rhi_handle<rhi_handle_type::IMAGE> src,
                          typed_rhi_handle<rhi_handle_type::IMAGE> dst) noexcept = 0;
        virtual void generate_mip_chain(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                                        typed_rhi_handle<rhi_handle_type::IMAGE> img, image_layout current_layout,
                                        uint32_t base_mip = 0,
                                        uint32_t mip_count = numeric_limits<uint32_t>::max()) = 0;

        // Buffer Commands
        virtual void copy(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                          typed_rhi_handle<rhi_handle_type::BUFFER> src, typed_rhi_handle<rhi_handle_type::BUFFER> dst,
                          size_t src_offset = 0, size_t dst_offset = 0,
                          size_t byte_count = numeric_limits<size_t>::max()) noexcept = 0;
        virtual void fill(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                          typed_rhi_handle<rhi_handle_type::BUFFER> handle, size_t offset, size_t size,
                          uint32_t data) noexcept = 0;

        // Barrier commands
        virtual void pipeline_barriers(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                                       span<const image_barrier> img_barriers,
                                       span<const buffer_barrier> buf_barriers) noexcept = 0;

        // Rendering commands
        virtual void begin_rendering(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                                     const render_pass_info& render_pass_info) noexcept = 0;
        virtual void end_rendering(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list) noexcept = 0;
        virtual void bind(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                          typed_rhi_handle<rhi_handle_type::GRAPHICS_PIPELINE> pipeline) noexcept = 0;
        virtual void draw(typed_rhi_handle<rhi_handle_type::COMMAND_LIST> command_list,
                          typed_rhi_handle<rhi_handle_type::BUFFER> indirect_buffer, uint32_t draw_count,
                          uint32_t stride) noexcept = 0;

      protected:
        work_queue() = default;
    };

    class window_surface
    {
      public:
        window_surface(const window_surface&) = delete;
        window_surface(window_surface&&) noexcept = delete;
        virtual ~window_surface() = default;

        window_surface& operator=(const window_surface&) = delete;
        window_surface& operator=(window_surface&&) noexcept = delete;

        virtual uint32_t width() const noexcept = 0;
        virtual uint32_t height() const noexcept = 0;
        virtual string name() const noexcept = 0;
        virtual bool should_close() const noexcept = 0;
        virtual bool minimized() const noexcept = 0;

        virtual void close() = 0;

        // Set up input callbacks
        virtual void register_keyboard_callback(function<void(const core::key_state&)>&& cb) noexcept = 0;
        virtual void register_mouse_callback(function<void(const core::mouse_button_state&)>&& cb) noexcept = 0;
        virtual void register_cursor_callback(function<void(float, float)>&& cb) noexcept = 0;
        virtual void register_scroll_callback(function<void(float, float)>&& cb) noexcept = 0;

        // Set up other miscellaneous callbacks
        virtual void register_close_callback(function<void()>&& cb) noexcept = 0;
        virtual void register_resize_callback(function<void(uint32_t, uint32_t)>&& cb) noexcept = 0;
        virtual void register_focus_callback(function<void(bool)>&& cb) noexcept = 0;
        virtual void register_minimize_callback(function<void(bool)>&& cb) noexcept = 0;

      protected:
        window_surface() = default;
    };

    class descriptor_context
    {
      public:
        descriptor_context(const descriptor_context&) = delete;
        descriptor_context(descriptor_context&&) noexcept = delete;
        virtual ~descriptor_context() = default;

        descriptor_context& operator=(const descriptor_context&) = delete;
        descriptor_context& operator=(descriptor_context&&) noexcept = delete;

        virtual typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET> allocate_descriptor_set(
            typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET_LAYOUT> layout) noexcept = 0;

        virtual void write_combined_image_sampler(typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET> set,
                                                  uint32_t binding, uint32_t array_element,
                                                  typed_rhi_handle<rhi_handle_type::SAMPLER> sampler,
                                                  typed_rhi_handle<rhi_handle_type::IMAGE> image,
                                                  image_layout layout) noexcept = 0;

        virtual void write_sampled_image(typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET> set, uint32_t binding,
                                         uint32_t array_element, typed_rhi_handle<rhi_handle_type::IMAGE> image,
                                         image_layout layout) noexcept = 0;

        virtual void write_sampled_images(typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET> set, uint32_t binding,
                                          uint32_t first_array_element,
                                          span<const typed_rhi_handle<rhi_handle_type::IMAGE>> images,
                                          image_layout layout) noexcept = 0;

        virtual void write_storage_image(typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET> set, uint32_t binding,
                                         uint32_t array_element, typed_rhi_handle<rhi_handle_type::IMAGE> image,
                                         image_layout layout) noexcept = 0;

        virtual void write_uniform_buffer(typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET> set, uint32_t binding,
                                          uint32_t array_element, typed_rhi_handle<rhi_handle_type::BUFFER> buffer,
                                          uint64_t offset, uint64_t range) noexcept = 0;

        virtual void write_storage_buffer(typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET> set, uint32_t binding,
                                          uint32_t array_element, typed_rhi_handle<rhi_handle_type::BUFFER> buffer,
                                          uint64_t offset, uint64_t range) noexcept = 0;

        virtual void write_input_attachment(typed_rhi_handle<rhi_handle_type::DESCRIPTOR_SET> set, uint32_t binding,
                                            uint32_t array_element, typed_rhi_handle<rhi_handle_type::IMAGE> image,
                                            image_layout layout) noexcept = 0;

      protected:
        descriptor_context() = default;
    };
} // namespace tempest::rhi

namespace tempest::rhi::vk
{
    extern unique_ptr<rhi::instance> create_instance() noexcept;
    extern unique_ptr<rhi::window_surface> create_window_surface(const rhi::window_surface_desc& desc) noexcept;
} // namespace tempest::rhi::vk

#endif // tempest_rhi_rhi_hpp
