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

        auto create_buffer(const buffer_desc& desc) noexcept -> typed_rhi_handle<rhi_handle_type::buffer> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::buffer>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(create_buffer_cmd{
                .desc = desc,
                .result = result,
            });

            return result;
        }

        auto create_image(const image_desc& desc) noexcept -> typed_rhi_handle<rhi_handle_type::image> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::image>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(create_image_cmd{
                .desc = desc,
                .result = result,
            });

            return result;
        }

        auto create_fence(const fence_info& info) noexcept -> typed_rhi_handle<rhi_handle_type::fence> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::fence>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(create_fence_cmd{
                .info = info,
                .result = result,
            });

            return result;
        }

        auto create_semaphore(const semaphore_info& info) noexcept
            -> typed_rhi_handle<rhi_handle_type::semaphore> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::semaphore>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(create_semaphore_cmd{
                .info = info,
                .result = result,
            });

            return result;
        }

        auto create_render_surface(const render_surface_desc& desc) noexcept
            -> typed_rhi_handle<rhi_handle_type::render_surface> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::render_surface>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(create_render_surface_cmd{
                .desc = desc,
                .result = result,
            });

            return result;
        }

        auto create_descriptor_set_layout(
            const vector<descriptor_binding_layout>& desc,
            enum_mask<descriptor_set_layout_flags> flags = make_enum_mask(descriptor_set_layout_flags::none)) noexcept
            -> typed_rhi_handle<rhi_handle_type::descriptor_set_layout> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::descriptor_set_layout>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(create_descriptor_set_layout_cmd{
                .desc = desc,
                .flags = flags,
                .result = result,
            });

            return result;
        }

        auto create_pipeline_layout(const pipeline_layout_desc& desc) noexcept
            -> typed_rhi_handle<rhi_handle_type::pipeline_layout> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::pipeline_layout>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(create_pipeline_layout_cmd{
                .desc = desc,
                .result = result,
            });

            return result;
        }

        auto create_graphics_pipeline(const graphics_pipeline_desc& desc) noexcept
            -> typed_rhi_handle<rhi_handle_type::graphics_pipeline> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::graphics_pipeline>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(create_graphics_pipeline_cmd{
                .desc = desc,
                .result = result,
            });

            return result;
        }

        auto create_descriptor_set(const descriptor_set_desc& desc) noexcept
            -> typed_rhi_handle<rhi_handle_type::descriptor_set> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::descriptor_set>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(create_descriptor_set_cmd{
                .desc = desc,
                .result = result,
            });

            return result;
        }

        auto create_compute_pipeline(const compute_pipeline_desc& desc) noexcept
            -> typed_rhi_handle<rhi_handle_type::compute_pipeline> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::compute_pipeline>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(create_compute_pipeline_cmd{
                .desc = desc,
                .result = result,
            });

            return result;
        }

        auto create_sampler(const sampler_desc& desc) noexcept -> typed_rhi_handle<rhi_handle_type::sampler> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::sampler>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(create_sampler_cmd{
                .desc = desc,
                .result = result,
            });

            return result;
        }

        void destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept override
        {
            _history.push_back(destroy_buffer_cmd{handle});
        }

        void destroy_image(typed_rhi_handle<rhi_handle_type::image> handle) noexcept override
        {
            _history.push_back(destroy_image_cmd{handle});
        }

        void destroy_fence(typed_rhi_handle<rhi_handle_type::fence> handle) noexcept override
        {
            _history.push_back(destroy_fence_cmd{handle});
        }

        void destroy_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) noexcept override
        {
            _history.push_back(destroy_semaphore_cmd{handle});
        }

        void destroy_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept override
        {
            _history.push_back(destroy_render_surface_cmd{handle});
        }

        void destroy_descriptor_set_layout(
            typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept override
        {
            _history.push_back(destroy_descriptor_set_layout_cmd{handle});
        }

        void destroy_pipeline_layout(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept override
        {
            _history.push_back(destroy_pipeline_layout_cmd{handle});
        }

        void destroy_graphics_pipeline(typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle) noexcept override
        {
            _history.push_back(destroy_graphics_pipeline_cmd{handle});
        }

        void destroy_descriptor_set(typed_rhi_handle<rhi_handle_type::descriptor_set> handle) noexcept override
        {
            _history.push_back(destroy_descriptor_set_cmd{handle});
        }

        void destroy_compute_pipeline(typed_rhi_handle<rhi_handle_type::compute_pipeline> handle) noexcept override
        {
            _history.push_back(destroy_compute_pipeline_cmd{handle});
        }

        void destroy_sampler(typed_rhi_handle<rhi_handle_type::sampler> handle) noexcept override
        {
            _history.push_back(destroy_sampler_cmd{handle});
        }

        auto get_image_mip_view(typed_rhi_handle<rhi_handle_type::image> image, uint32_t mip) noexcept
            -> typed_rhi_handle<rhi_handle_type::image> override
        {
            auto result = typed_rhi_handle<rhi_handle_type::image>{
                .id = _next_handle++,
                .generation = 0,
            };

            _history.push_back(get_image_mip_view_cmd{
                .image = image,
                .mip = mip,
                .result = result,
            });

            return result;
        }

        auto get_primary_work_queue() noexcept -> work_queue& override
        {
            return _primary_queue;
        }

        auto get_dedicated_transfer_queue() noexcept -> work_queue& override
        {
            return _transfer_queue;
        }

        auto get_dedicated_compute_queue() noexcept -> work_queue& override
        {
            return _compute_queue;
        }

        void recreate_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle,
                                     const render_surface_desc& desc) noexcept override
        {
            _history.push_back(recreate_render_surface_cmd{
                .handle = handle,
                .desc = desc,
            });
        }

        [[nodiscard]] auto query_render_surface_info(const window_surface& /*window*/) noexcept
            -> render_surface_info override
        {
            return {};
        }

        [[nodiscard]] auto get_render_surfaces(typed_rhi_handle<rhi_handle_type::render_surface> /*handle*/) noexcept
            -> span<const typed_rhi_handle<rhi_handle_type::image>> override
        {
            return {};
        }

        [[nodiscard]] auto acquire_next_image(typed_rhi_handle<rhi_handle_type::render_surface> swapchain,
                                              typed_rhi_handle<rhi_handle_type::fence> signal_fence =
                                                  typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept
            -> expected<swapchain_image_acquire_info_result, swapchain_error_code> override
        {
            auto result = swapchain_image_acquire_info_result{};
            _history.push_back(acquire_next_image_cmd{
                .swapchain = swapchain,
                .signal_fence = signal_fence,
                .result = result,
            });
            return result;
        }

        [[nodiscard]] auto is_signaled(typed_rhi_handle<rhi_handle_type::fence> /*fence*/) const noexcept
            -> bool override
        {
            return true;
        }

        [[nodiscard]] auto reset(span<const typed_rhi_handle<rhi_handle_type::fence>> /*fences*/) const noexcept
            -> bool override
        {
            return true;
        }

        [[nodiscard]] auto wait(span<const typed_rhi_handle<rhi_handle_type::fence>> /*fences*/) const noexcept
            -> bool override
        {
            return true;
        }

        void unmap_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept override
        {
            _history.push_back(unmap_buffer_cmd{handle});
        }

        void flush_buffers(span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers) noexcept override
        {
            _history.push_back(
                flush_buffers_cmd{vector<typed_rhi_handle<rhi_handle_type::buffer>>(buffers.begin(), buffers.end())});
        }

        [[nodiscard]] auto get_buffer_size(typed_rhi_handle<rhi_handle_type::buffer> /*handle*/) const noexcept
            -> size_t override
        {
            return 0;
        }

        // Image Management
        [[nodiscard]] auto get_image_width(typed_rhi_handle<rhi_handle_type::image> /*handle*/) const noexcept
            -> size_t override
        {
            return 0;
        }

        [[nodiscard]] auto get_image_height(typed_rhi_handle<rhi_handle_type::image> /*handle*/) const noexcept
            -> size_t override
        {
            return 0;
        }

        // Swapchain info
        [[nodiscard]] auto get_render_surface_width(
            typed_rhi_handle<rhi_handle_type::render_surface> /*surface*/) const noexcept -> uint32_t override
        {
            return 0;
        }

        [[nodiscard]] auto get_render_surface_height(
            typed_rhi_handle<rhi_handle_type::render_surface> /*surface*/) const noexcept -> uint32_t override
        {
            return 0;
        }

        [[nodiscard]] auto get_window_surface(typed_rhi_handle<rhi_handle_type::render_surface> /*surface*/)
            const noexcept -> const window_surface* override
        {
            return nullptr;
        }

        [[nodiscard]] auto supports_descriptor_buffers() const noexcept -> bool override
        {
            return false;
        }

        [[nodiscard]] auto get_descriptor_buffer_alignment() const noexcept -> size_t override
        {
            return 0;
        }

        [[nodiscard]] auto get_descriptor_set_layout_size(
            typed_rhi_handle<rhi_handle_type::descriptor_set_layout> /*layout*/) const noexcept -> size_t override
        {
            return 0;
        }

        auto write_descriptor_buffer(const descriptor_set_desc& desc, byte* dest,
                                       size_t offset) const noexcept -> size_t override
        {
            _history.push_back(write_descriptor_buffer_cmd{
                .desc = desc,
                .dest = dest,
                .offset = offset,
             });

            return 0;
        }

        // Buffer Management
        auto map_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept -> byte* override
        {
            auto* result = static_cast<byte*>(nullptr);
            _history.push_back(map_buffer_cmd{
                .handle = handle,
                .result = result,
            });
            return result;
        }

        // Miscellaneous
        void release_resources() override
        {
            _history.push_back(release_resources_cmd{});
        }

        void finish_frame() override
        {
            _history.push_back(finish_frame_cmd{});
        }

        auto frames_in_flight() const noexcept -> uint32_t override
        {
            return 2;
        }

        void wait_idle() override
        {
            _history.push_back(wait_idle_cmd{});
        }

        [[nodiscard]] auto get_history() const noexcept -> span<const mock_device_command>
        {
            return _history;
        }

        [[nodiscard]] auto get_history_count() const noexcept -> size_t
        {
            return _history.size();
        }

        [[nodiscard]] auto get_history(size_t index) const noexcept -> const mock_device_command&
        {
            return _history[index];
        }

      private:
        mutable vector<mock_device_command> _history;
        uint32_t _next_handle = 1;
        mock_work_queue _primary_queue;
        mock_work_queue _transfer_queue;
        mock_work_queue _compute_queue;
    };
} // namespace tempest::rhi::mock

#endif