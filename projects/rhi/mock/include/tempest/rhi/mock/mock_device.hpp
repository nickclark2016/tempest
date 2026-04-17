#ifndef tempest_rhi_mock_mock_device_hpp
#define tempest_rhi_mock_mock_device_hpp

#include <tempest/rhi.hpp>
#include <tempest/rhi/mock/mock_work_queue.hpp>

namespace tempest::rhi::mock
{
    class mock_device : public rhi::device
    {
      public:
        mock_device() = default;
        ~mock_device() override = default;

        typed_rhi_handle<rhi_handle_type::buffer> create_buffer(const buffer_desc& desc) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::buffer>{_next_handle++, 0};
        }

        typed_rhi_handle<rhi_handle_type::image> create_image(const image_desc& desc) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::image>{_next_handle++, 0};
        }

        typed_rhi_handle<rhi_handle_type::fence> create_fence(const fence_info& info) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::fence>{_next_handle++, 0};
        }

        typed_rhi_handle<rhi_handle_type::semaphore> create_semaphore(const semaphore_info& info) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::semaphore>{_next_handle++, 0};
        }

        typed_rhi_handle<rhi_handle_type::render_surface> create_render_surface(
            const render_surface_desc& desc) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::render_surface>{_next_handle++, 0};
        }

        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> create_descriptor_set_layout(
            const vector<descriptor_binding_layout>& desc,
            enum_mask<descriptor_set_layout_flags> flags =
                make_enum_mask(descriptor_set_layout_flags::none)) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::descriptor_set_layout>{_next_handle++, 0};
        }

        typed_rhi_handle<rhi_handle_type::pipeline_layout> create_pipeline_layout(
            const pipeline_layout_desc& desc) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::pipeline_layout>{_next_handle++, 0};
        }

        typed_rhi_handle<rhi_handle_type::graphics_pipeline> create_graphics_pipeline(
            const graphics_pipeline_desc& desc) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::graphics_pipeline>{_next_handle++, 0};
        }

        typed_rhi_handle<rhi_handle_type::descriptor_set> create_descriptor_set(
            const descriptor_set_desc& desc) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::descriptor_set>{_next_handle++, 0};
        }

        typed_rhi_handle<rhi_handle_type::compute_pipeline> create_compute_pipeline(
            const compute_pipeline_desc& desc) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::compute_pipeline>{_next_handle++, 0};
        }

        typed_rhi_handle<rhi_handle_type::sampler> create_sampler(const sampler_desc& desc) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::sampler>{_next_handle++, 0};
        }

        void destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept override {}
        void destroy_image(typed_rhi_handle<rhi_handle_type::image> /*handle*/) noexcept override {}
        void destroy_fence(typed_rhi_handle<rhi_handle_type::fence> handle) noexcept override {}
        void destroy_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) noexcept override {}
        void destroy_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept override {}
        void destroy_descriptor_set_layout(
            typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept override {}
        void destroy_pipeline_layout(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept override {}
        void destroy_graphics_pipeline(
            typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle) noexcept override {}
        void destroy_descriptor_set(typed_rhi_handle<rhi_handle_type::descriptor_set> handle) noexcept override {}
        void destroy_compute_pipeline(typed_rhi_handle<rhi_handle_type::compute_pipeline> handle) noexcept override {}
        void destroy_sampler(typed_rhi_handle<rhi_handle_type::sampler> handle) noexcept override {}

        typed_rhi_handle<rhi::rhi_handle_type::image> get_image_mip_view(
            typed_rhi_handle<rhi::rhi_handle_type::image> image, uint32_t mip) noexcept override
        {
            return typed_rhi_handle<rhi_handle_type::image>{_next_handle++, 0};
        }

        work_queue& get_primary_work_queue() noexcept override
        {
            return _primary_queue;
        }
        
        work_queue& get_dedicated_transfer_queue() noexcept override
        {
            return _transfer_queue;
        }
        
        work_queue& get_dedicated_compute_queue() noexcept override
        {
            return _compute_queue;
        }

        void recreate_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle,
                                     const render_surface_desc& desc) noexcept override {}

        render_surface_info query_render_surface_info(const window_surface& window) noexcept override
        {
            return {};
        }

        span<const typed_rhi_handle<rhi_handle_type::image>> get_render_surfaces(
            typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept override
        {
            return {};
        }

        expected<swapchain_image_acquire_info_result, swapchain_error_code> acquire_next_image(
            typed_rhi_handle<rhi_handle_type::render_surface> swapchain,
            typed_rhi_handle<rhi_handle_type::fence> signal_fence =
                typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept override
        {
            return swapchain_image_acquire_info_result{0, false};
        }

        bool is_signaled(typed_rhi_handle<rhi_handle_type::fence> fence) const noexcept override
        {
            return true;
        }
        
        bool reset(span<const typed_rhi_handle<rhi_handle_type::fence>> fences) const noexcept override
        {
            return true;
        }
        
        bool wait(span<const typed_rhi_handle<rhi_handle_type::fence>> fences) const noexcept override
        {
            return true;
        }

        // Buffer Management
        byte* map_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept override
        {
            return nullptr;
        }
        
        void unmap_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept override {}
        
        void flush_buffers(span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers) noexcept override {}
        
        size_t get_buffer_size(typed_rhi_handle<rhi_handle_type::buffer> handle) const noexcept override
        {
            return 0;
        }

        // Image Management
        size_t get_image_width(typed_rhi_handle<rhi_handle_type::image> /*handle*/) const noexcept override
        {
            return 0;
        }
        
        size_t get_image_height(typed_rhi_handle<rhi_handle_type::image> /*handle*/) const noexcept override
        {
            return 0;
        }

        // Swapchain info
        uint32_t get_render_surface_width(
            typed_rhi_handle<rhi_handle_type::render_surface> surface) const noexcept override
        {
            return 0;
        }
        
        uint32_t get_render_surface_height(
            typed_rhi_handle<rhi_handle_type::render_surface> surface) const noexcept override
        {
            return 0;
        }
        
        const window_surface* get_window_surface(
            typed_rhi_handle<rhi_handle_type::render_surface> surface) const noexcept override
        {
            return nullptr;
        }

        // Descriptor buffer support
        bool supports_descriptor_buffers() const noexcept override
        {
            return false;
        }
        
        size_t get_descriptor_buffer_alignment() const noexcept override
        {
            return 256;
        }
        
        size_t get_descriptor_set_layout_size(
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> layout) const noexcept override
        {
            return 0;
        }
        
        size_t write_descriptor_buffer(const descriptor_set_desc& desc, byte* dest,
                                       size_t offset) const noexcept override
        {
            return 0;
        }

        // Miscellaneous
        void release_resources() override {}
        void finish_frame() override {}

        uint32_t frames_in_flight() const noexcept override
        {
            return 2;
        }

        void wait_idle() override {}

        // Resource Tracking mock logic
        image_layout get_image_state(typed_rhi_handle<rhi_handle_type::image> /*handle*/) 
        {
            return image_layout::undefined;
        }

      private:
        uint32_t _next_handle = 1;
        mock_work_queue _primary_queue;
        mock_work_queue _transfer_queue;
        mock_work_queue _compute_queue;
    };
} // namespace tempest::rhi::mock

#endif