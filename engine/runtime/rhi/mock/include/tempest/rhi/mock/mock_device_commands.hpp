#ifndef tempest_rhi_mock_mock_device_commands_hpp
#define tempest_rhi_mock_mock_device_commands_hpp

#include <tempest/rhi.hpp>
#include <tempest/variant.hpp>

namespace tempest::rhi::mock
{
    struct create_buffer_cmd
    {
        buffer_desc desc;
        typed_rhi_handle<rhi_handle_type::buffer> result;
    };

    struct create_image_cmd
    {
        image_desc desc;
        typed_rhi_handle<rhi_handle_type::image> result;
    };

    struct create_fence_cmd
    {
        fence_info info;
        typed_rhi_handle<rhi_handle_type::fence> result;
    };

    struct create_semaphore_cmd
    {
        semaphore_info info;
        typed_rhi_handle<rhi_handle_type::semaphore> result;
    };

    struct create_render_surface_cmd
    {
        render_surface_desc desc;
        typed_rhi_handle<rhi_handle_type::render_surface> result;
    };

    struct create_descriptor_set_layout_cmd
    {
        vector<descriptor_binding_layout> desc;
        enum_mask<descriptor_set_layout_flags> flags;
        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> result;
    };

    struct create_pipeline_layout_cmd
    {
        pipeline_layout_desc desc;
        typed_rhi_handle<rhi_handle_type::pipeline_layout> result;
    };

    struct create_graphics_pipeline_cmd
    {
        graphics_pipeline_desc desc;
        typed_rhi_handle<rhi_handle_type::graphics_pipeline> result;
    };

    struct create_descriptor_set_cmd
    {
        descriptor_set_desc desc;
        typed_rhi_handle<rhi_handle_type::descriptor_set> result;
    };

    struct create_compute_pipeline_cmd
    {
        compute_pipeline_desc desc;
        typed_rhi_handle<rhi_handle_type::compute_pipeline> result;
    };

    struct create_sampler_cmd
    {
        sampler_desc desc;
        typed_rhi_handle<rhi_handle_type::sampler> result;
    };

    struct destroy_buffer_cmd
    {
        typed_rhi_handle<rhi_handle_type::buffer> handle;
    };

    struct destroy_image_cmd
    {
        typed_rhi_handle<rhi_handle_type::image> handle;
    };

    struct destroy_fence_cmd
    {
        typed_rhi_handle<rhi_handle_type::fence> handle;
    };

    struct destroy_semaphore_cmd
    {
        typed_rhi_handle<rhi_handle_type::semaphore> handle;
    };

    struct destroy_render_surface_cmd
    {
        typed_rhi_handle<rhi_handle_type::render_surface> handle;
    };

    struct destroy_descriptor_set_layout_cmd
    {
        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle;
    };

    struct destroy_pipeline_layout_cmd
    {
        typed_rhi_handle<rhi_handle_type::pipeline_layout> handle;
    };

    struct destroy_graphics_pipeline_cmd
    {
        typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle;
    };

    struct destroy_descriptor_set_cmd
    {
        typed_rhi_handle<rhi_handle_type::descriptor_set> handle;
    };

    struct destroy_compute_pipeline_cmd
    {
        typed_rhi_handle<rhi_handle_type::compute_pipeline> handle;
    };

    struct destroy_sampler_cmd
    {
        typed_rhi_handle<rhi_handle_type::sampler> handle;
    };

    struct get_image_mip_view_cmd
    {
        typed_rhi_handle<rhi_handle_type::image> image;
        uint32_t mip;
        typed_rhi_handle<rhi_handle_type::image> result;
    };

    struct recreate_render_surface_cmd
    {
        typed_rhi_handle<rhi_handle_type::render_surface> handle;
        render_surface_desc desc;
    };

    struct acquire_next_image_cmd
    {
        typed_rhi_handle<rhi_handle_type::render_surface> swapchain;
        typed_rhi_handle<rhi_handle_type::fence> signal_fence;
        expected<swapchain_image_acquire_info_result, swapchain_error_code> result;
    };

    struct map_buffer_cmd
    {
        typed_rhi_handle<rhi_handle_type::buffer> handle;
        byte* result;
    };

    struct unmap_buffer_cmd
    {
        typed_rhi_handle<rhi_handle_type::buffer> handle;
    };

    struct flush_buffers_cmd
    {
        vector<typed_rhi_handle<rhi_handle_type::buffer>> buffers;
    };

    struct release_resources_cmd
    {
    };

    struct finish_frame_cmd
    {
    };

    struct wait_idle_cmd
    {
    };

    struct write_descriptor_buffer_cmd
    {
        descriptor_set_desc desc;
        byte* dest;
        size_t offset;
    };

    using mock_device_command = variant<
        create_buffer_cmd, 
        create_image_cmd, 
        create_fence_cmd, 
        create_semaphore_cmd,
        create_render_surface_cmd, 
        create_descriptor_set_layout_cmd, 
        create_pipeline_layout_cmd, 
        create_graphics_pipeline_cmd, 
        create_descriptor_set_cmd, 
        create_compute_pipeline_cmd, 
        create_sampler_cmd, 
        destroy_buffer_cmd, 
        destroy_image_cmd, 
        destroy_fence_cmd, 
        destroy_semaphore_cmd,
        destroy_render_surface_cmd, 
        destroy_descriptor_set_layout_cmd, 
        destroy_pipeline_layout_cmd, 
        destroy_graphics_pipeline_cmd, 
        destroy_descriptor_set_cmd, 
        destroy_compute_pipeline_cmd, 
        destroy_sampler_cmd, 
        get_image_mip_view_cmd, 
        recreate_render_surface_cmd, 
        acquire_next_image_cmd, 
        map_buffer_cmd, 
        unmap_buffer_cmd, 
        flush_buffers_cmd, 
        release_resources_cmd, 
        finish_frame_cmd, 
        wait_idle_cmd,
        write_descriptor_buffer_cmd>;
} // namespace tempest::rhi::mock

#endif