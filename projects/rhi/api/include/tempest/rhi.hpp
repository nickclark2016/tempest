#ifndef tempest_rhi_rhi_hpp
#define tempest_rhi_rhi_hpp

#include <tempest/rhi_types.hpp>

#include <tempest/enum.hpp>
#include <tempest/expected.hpp>
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
            const vector<descriptor_binding_layout>& desc) noexcept = 0;

        virtual void destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept = 0;
        virtual void destroy_image(typed_rhi_handle<rhi_handle_type::image> handle) noexcept = 0;
        virtual void destroy_fence(typed_rhi_handle<rhi_handle_type::fence> handle) noexcept = 0;
        virtual void destroy_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) noexcept = 0;
        virtual void destroy_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept = 0;
        virtual void destroy_descriptor_set_layout(
            typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept = 0;

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

        virtual typed_rhi_handle<rhi_handle_type::command_list> get_next_command_list() noexcept = 0;

        virtual bool submit(span<const submit_info> infos,
                            typed_rhi_handle<rhi_handle_type::fence> fence =
                                typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept = 0;
        virtual present_result present(const present_info& info) noexcept = 0;

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
                          typed_rhi_handle<rhi_handle_type::image> src,
                          typed_rhi_handle<rhi_handle_type::image> dst) noexcept = 0;
        virtual void generate_mip_chain(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                        typed_rhi_handle<rhi_handle_type::image> img, image_layout current_layout,
                                        uint32_t base_mip = 0,
                                        uint32_t mip_count = numeric_limits<uint32_t>::max()) = 0;

        // Buffer Commands
        virtual void copy(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::buffer> src, typed_rhi_handle<rhi_handle_type::buffer> dst,
                          size_t src_offset = 0, size_t dst_offset = 0,
                          size_t byte_count = numeric_limits<size_t>::max()) noexcept = 0;
        virtual void fill(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                          typed_rhi_handle<rhi_handle_type::buffer> handle, size_t offset, size_t size,
                          uint32_t data) noexcept = 0;

        // Barrier commands
        virtual void pipeline_barriers(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                       span<const image_barrier> img_barriers,
                                       span<const buffer_barrier> buf_barriers) noexcept = 0;

        // Rendering commands
        virtual void begin_rendering(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                     const render_pass_info& render_pass_info) noexcept = 0;
        virtual void end_rendering(typed_rhi_handle<rhi_handle_type::command_list> command_list) noexcept = 0;

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

        virtual void commit(uint32_t set_index, const descriptor_resource_binding& binding,
                            typed_rhi_handle<rhi_handle_type::descriptor_set_layout> layout) noexcept = 0;
        virtual typed_rhi_handle<rhi_handle_type::descriptor_set> get_active_descriptor_set(
            uint32_t index) const noexcept = 0;

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
