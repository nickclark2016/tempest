#include <tempest/rhi/mock/mock_device.hpp>

namespace tempest::rhi::mock
{
    auto mock_device::create_buffer(const buffer_desc& desc) noexcept -> typed_rhi_handle<rhi_handle_type::buffer>
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

    auto mock_device::create_image(const image_desc& desc) noexcept -> typed_rhi_handle<rhi_handle_type::image>
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

    auto mock_device::create_fence(const fence_info& info) noexcept -> typed_rhi_handle<rhi_handle_type::fence>
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

    auto mock_device::create_semaphore(const semaphore_info& info) noexcept
        -> typed_rhi_handle<rhi_handle_type::semaphore>
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

    auto mock_device::create_render_surface(const render_surface_desc& desc) noexcept
        -> typed_rhi_handle<rhi_handle_type::render_surface>
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

    auto mock_device::create_descriptor_set_layout(const vector<descriptor_binding_layout>& desc,
                                                   enum_mask<descriptor_set_layout_flags> flags) noexcept
        -> typed_rhi_handle<rhi_handle_type::descriptor_set_layout>
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

    auto mock_device::create_pipeline_layout(const pipeline_layout_desc& desc) noexcept
        -> typed_rhi_handle<rhi_handle_type::pipeline_layout>
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

    auto mock_device::create_graphics_pipeline(const graphics_pipeline_desc& desc) noexcept
        -> typed_rhi_handle<rhi_handle_type::graphics_pipeline>
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

    auto mock_device::create_descriptor_set(const descriptor_set_desc& desc) noexcept
        -> typed_rhi_handle<rhi_handle_type::descriptor_set>
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

    auto mock_device::create_compute_pipeline(const compute_pipeline_desc& desc) noexcept
        -> typed_rhi_handle<rhi_handle_type::compute_pipeline>
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

    auto mock_device::create_sampler(const sampler_desc& desc) noexcept -> typed_rhi_handle<rhi_handle_type::sampler>
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

    void mock_device::destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept
    {
        _history.push_back(destroy_buffer_cmd{handle});
    }

    void mock_device::destroy_image(typed_rhi_handle<rhi_handle_type::image> handle) noexcept
    {
        _history.push_back(destroy_image_cmd{handle});
    }

    void mock_device::destroy_fence(typed_rhi_handle<rhi_handle_type::fence> handle) noexcept
    {
        _history.push_back(destroy_fence_cmd{handle});
    }

    void mock_device::destroy_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) noexcept
    {
        _history.push_back(destroy_semaphore_cmd{handle});
    }

    void mock_device::destroy_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept
    {
        _history.push_back(destroy_render_surface_cmd{handle});
    }

    void mock_device::destroy_descriptor_set_layout(
        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept
    {
        _history.push_back(destroy_descriptor_set_layout_cmd{handle});
    }

    void mock_device::destroy_pipeline_layout(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept
    {
        _history.push_back(destroy_pipeline_layout_cmd{handle});
    }

    void mock_device::destroy_graphics_pipeline(typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle) noexcept
    {
        _history.push_back(destroy_graphics_pipeline_cmd{handle});
    }

    void mock_device::destroy_descriptor_set(typed_rhi_handle<rhi_handle_type::descriptor_set> handle) noexcept
    {
        _history.push_back(destroy_descriptor_set_cmd{handle});
    }

    void mock_device::destroy_compute_pipeline(typed_rhi_handle<rhi_handle_type::compute_pipeline> handle) noexcept
    {
        _history.push_back(destroy_compute_pipeline_cmd{handle});
    }

    void mock_device::destroy_sampler(typed_rhi_handle<rhi_handle_type::sampler> handle) noexcept
    {
        _history.push_back(destroy_sampler_cmd{handle});
    }

    auto mock_device::get_image_mip_view(typed_rhi_handle<rhi_handle_type::image> image, uint32_t mip) noexcept
        -> typed_rhi_handle<rhi_handle_type::image>
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

    auto mock_device::get_primary_work_queue() noexcept -> work_queue&
    {
        return _primary_queue;
    }

    auto mock_device::get_dedicated_transfer_queue() noexcept -> work_queue&
    {
        return _transfer_queue;
    }

    auto mock_device::get_dedicated_compute_queue() noexcept -> work_queue&
    {
        return _compute_queue;
    }

    void mock_device::recreate_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle,
                                              const render_surface_desc& desc) noexcept
    {
        _history.push_back(recreate_render_surface_cmd{
            .handle = handle,
            .desc = desc,
        });
    }

    auto mock_device::query_render_surface_info(const window_surface&) noexcept -> render_surface_info
    {
        return {};
    }

    auto mock_device::get_render_surfaces(typed_rhi_handle<rhi_handle_type::render_surface>) noexcept
        -> span<const typed_rhi_handle<rhi_handle_type::image>>
    {
        return {};
    }

    auto mock_device::acquire_next_image(typed_rhi_handle<rhi_handle_type::render_surface> swapchain,
                                         typed_rhi_handle<rhi_handle_type::fence> signal_fence) noexcept
        -> expected<swapchain_image_acquire_info_result, swapchain_error_code>
    {
        auto result = swapchain_image_acquire_info_result{};
        _history.push_back(acquire_next_image_cmd{
            .swapchain = swapchain,
            .signal_fence = signal_fence,
            .result = result,
        });
        return result;
    }

    auto mock_device::is_signaled(typed_rhi_handle<rhi_handle_type::fence>) const noexcept -> bool
    {
        return true;
    }

    auto mock_device::reset(span<const typed_rhi_handle<rhi_handle_type::fence>>) const noexcept -> bool
    {
        return true;
    }

    auto mock_device::wait(span<const typed_rhi_handle<rhi_handle_type::fence>>) const noexcept -> bool
    {
        return true;
    }

    auto mock_device::get_image_width(typed_rhi_handle<rhi_handle_type::image>) const noexcept -> size_t
    {
        return 0;
    }

    auto mock_device::get_image_height(typed_rhi_handle<rhi_handle_type::image>) const noexcept -> size_t
    {
        return 0;
    }

    auto mock_device::get_render_surface_width(typed_rhi_handle<rhi_handle_type::render_surface>) const noexcept
        -> uint32_t
    {
        return 0;
    }

    auto mock_device::get_render_surface_height(typed_rhi_handle<rhi_handle_type::render_surface>) const noexcept
        -> uint32_t
    {
        return 0;
    }

    auto mock_device::get_window_surface(typed_rhi_handle<rhi_handle_type::render_surface>) const noexcept
        -> const window_surface*
    {
        return nullptr;
    }

    auto mock_device::supports_descriptor_buffers() const noexcept -> bool
    {
        return false;
    }

    auto mock_device::get_descriptor_buffer_alignment() const noexcept -> size_t
    {
        return 0;
    }

    auto mock_device::get_descriptor_set_layout_size(
        typed_rhi_handle<rhi_handle_type::descriptor_set_layout>) const noexcept -> size_t
    {
        return 0;
    }

    auto mock_device::write_descriptor_buffer(const descriptor_set_desc& desc, byte* dest, size_t offset) const noexcept
        -> size_t
    {
        _history.push_back(write_descriptor_buffer_cmd{
            .desc = desc,
            .dest = dest,
            .offset = offset,
        });

        return 0;
    }

    auto mock_device::map_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept -> byte*
    {
        auto* result = static_cast<byte*>(nullptr);
        _history.push_back(map_buffer_cmd{
            .handle = handle,
            .result = result,
        });
        return result;
    }

    void mock_device::unmap_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept
    {
        _history.push_back(unmap_buffer_cmd{handle});
    }

    void mock_device::flush_buffers(span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers) noexcept
    {
        _history.push_back(
            flush_buffers_cmd{vector<typed_rhi_handle<rhi_handle_type::buffer>>(buffers.begin(), buffers.end())});
    }

    auto mock_device::get_buffer_size(typed_rhi_handle<rhi_handle_type::buffer> /*handle*/) const noexcept -> size_t
    {
        return 0;
    }

    auto mock_device::get_required_alignment(enum_mask<rhi::buffer_usage> /*usage*/) const noexcept -> size_t
    {
        return 0;
    }

    void mock_device::release_resources()
    {
        _history.push_back(release_resources_cmd{});
    }

    void mock_device::finish_frame()
    {
        _history.push_back(finish_frame_cmd{});
    }

    auto mock_device::frames_in_flight() const noexcept -> uint32_t
    {
        return 2;
    }

    void mock_device::wait_idle()
    {
        _history.push_back(wait_idle_cmd{});
    }
    
    auto mock_device::get_history() const noexcept -> span<const mock_device_command>
    {
        return _history;
    }
    
    auto mock_device::get_history_count() const noexcept -> size_t
    {
        return _history.size();
    }
    
    auto mock_device::get_history(size_t index) const noexcept -> const mock_device_command&
    {
        return _history[index];
    }
} // namespace tempest::rhi::mock