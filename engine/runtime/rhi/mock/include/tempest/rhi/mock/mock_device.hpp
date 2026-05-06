#ifndef tempest_rhi_mock_mock_device_hpp
#define tempest_rhi_mock_mock_device_hpp

#include <tempest/api.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi/mock/mock_device_commands.hpp>
#include <tempest/rhi/mock/mock_work_queue.hpp>

namespace tempest::rhi::mock
{
    class TEMPEST_API mock_device final : public rhi::device
    {
      public:
        mock_device() = default;
        mock_device(const mock_device&) = delete;
        mock_device(mock_device&&) = delete;

        ~mock_device() override = default;

        mock_device& operator=(const mock_device&) = delete;
        mock_device& operator=(mock_device&&) = delete;

        auto create_buffer(const buffer_desc& desc) noexcept -> typed_rhi_handle<rhi_handle_type::buffer> override;
        auto create_image(const image_desc& desc) noexcept -> typed_rhi_handle<rhi_handle_type::image> override;
        auto create_fence(const fence_info& info) noexcept -> typed_rhi_handle<rhi_handle_type::fence> override;
        auto create_semaphore(const semaphore_info& info) noexcept
            -> typed_rhi_handle<rhi_handle_type::semaphore> override;
        auto create_render_surface(const render_surface_desc& desc) noexcept
            -> typed_rhi_handle<rhi_handle_type::render_surface> override;
        auto create_descriptor_set_layout(
            const vector<descriptor_binding_layout>& desc,
            enum_mask<descriptor_set_layout_flags> flags = make_enum_mask(descriptor_set_layout_flags::none)) noexcept
            -> typed_rhi_handle<rhi_handle_type::descriptor_set_layout> override;
        auto create_pipeline_layout(const pipeline_layout_desc& desc) noexcept
            -> typed_rhi_handle<rhi_handle_type::pipeline_layout> override;
        auto create_graphics_pipeline(const graphics_pipeline_desc& desc) noexcept
            -> typed_rhi_handle<rhi_handle_type::graphics_pipeline> override;
        auto create_descriptor_set(const descriptor_set_desc& desc) noexcept
            -> typed_rhi_handle<rhi_handle_type::descriptor_set> override;
        auto create_compute_pipeline(const compute_pipeline_desc& desc) noexcept
            -> typed_rhi_handle<rhi_handle_type::compute_pipeline> override;
        auto create_sampler(const sampler_desc& desc) noexcept -> typed_rhi_handle<rhi_handle_type::sampler> override;

        void destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept override;
        void destroy_image(typed_rhi_handle<rhi_handle_type::image> handle) noexcept override;
        void destroy_fence(typed_rhi_handle<rhi_handle_type::fence> handle) noexcept override;
        void destroy_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) noexcept override;
        void destroy_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept override;
        void destroy_descriptor_set_layout(
            typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept override;
        void destroy_pipeline_layout(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept override;
        void destroy_graphics_pipeline(typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle) noexcept override;
        void destroy_descriptor_set(typed_rhi_handle<rhi_handle_type::descriptor_set> handle) noexcept override;
        void destroy_compute_pipeline(typed_rhi_handle<rhi_handle_type::compute_pipeline> handle) noexcept override;
        void destroy_sampler(typed_rhi_handle<rhi_handle_type::sampler> handle) noexcept override;

        auto get_image_mip_view(typed_rhi_handle<rhi_handle_type::image> image, uint32_t mip) noexcept
            -> typed_rhi_handle<rhi_handle_type::image> override;

        auto get_primary_work_queue() noexcept -> work_queue& override;
        auto get_dedicated_transfer_queue() noexcept -> work_queue& override;
        auto get_dedicated_compute_queue() noexcept -> work_queue& override;

        void recreate_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle,
                                     const render_surface_desc& desc) noexcept override;

        [[nodiscard]] auto query_render_surface_info(const window_surface& /*window*/) noexcept
            -> render_surface_info override;

        [[nodiscard]] auto get_render_surfaces(typed_rhi_handle<rhi_handle_type::render_surface> /*handle*/) noexcept
            -> span<const typed_rhi_handle<rhi_handle_type::image>> override;

        [[nodiscard]] auto acquire_next_image(typed_rhi_handle<rhi_handle_type::render_surface> swapchain,
                                              typed_rhi_handle<rhi_handle_type::fence> signal_fence =
                                                  typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept
            -> expected<swapchain_image_acquire_info_result, swapchain_error_code> override;

        [[nodiscard]] auto is_signaled(typed_rhi_handle<rhi_handle_type::fence> /*fence*/) const noexcept
            -> bool override;

        [[nodiscard]] auto reset(span<const typed_rhi_handle<rhi_handle_type::fence>> /*fences*/) const noexcept
            -> bool override;

        [[nodiscard]] auto wait(span<const typed_rhi_handle<rhi_handle_type::fence>> /*fences*/) const noexcept
            -> bool override;

        // Image Management
        [[nodiscard]] auto get_image_width(typed_rhi_handle<rhi_handle_type::image> /*handle*/) const noexcept
            -> size_t override;
        [[nodiscard]] auto get_image_height(typed_rhi_handle<rhi_handle_type::image> /*handle*/) const noexcept
            -> size_t override;

        // Swapchain info
        [[nodiscard]] auto get_render_surface_width(
            typed_rhi_handle<rhi_handle_type::render_surface> /*surface*/) const noexcept -> uint32_t override;
        [[nodiscard]] auto get_render_surface_height(
            typed_rhi_handle<rhi_handle_type::render_surface> /*surface*/) const noexcept -> uint32_t override;
        [[nodiscard]] auto get_window_surface(typed_rhi_handle<rhi_handle_type::render_surface> /*surface*/)
            const noexcept -> const window_surface* override;

        // Descriptor buffers
        [[nodiscard]] auto supports_descriptor_buffers() const noexcept -> bool override;
        [[nodiscard]] auto get_descriptor_buffer_alignment() const noexcept -> size_t override;
        [[nodiscard]] auto get_descriptor_set_layout_size(
            typed_rhi_handle<rhi_handle_type::descriptor_set_layout> /*layout*/) const noexcept -> size_t override;
        auto write_descriptor_buffer(const descriptor_set_desc& desc, byte* dest, size_t offset) const noexcept
            -> size_t override;

        // Buffer Management
        auto map_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept -> byte* override;
        void unmap_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept override;
        void flush_buffers(span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers) noexcept override;
        [[nodiscard]] auto get_buffer_size(typed_rhi_handle<rhi_handle_type::buffer> /*handle*/) const noexcept
            -> size_t override;

        // Miscellaneous
        void release_resources() override;
        void finish_frame() override;
        [[nodiscard]] auto frames_in_flight() const noexcept -> uint32_t override;
        void wait_idle() override;
        [[nodiscard]] auto get_history() const noexcept -> span<const mock_device_command>;
        [[nodiscard]] auto get_history_count() const noexcept -> size_t;
        [[nodiscard]] auto get_history(size_t index) const noexcept -> const mock_device_command&;

      private:
        mutable vector<mock_device_command> _history;
        uint32_t _next_handle = 1;
        mock_work_queue _primary_queue;
        mock_work_queue _transfer_queue;
        mock_work_queue _compute_queue;
    };
} // namespace tempest::rhi::mock

#endif