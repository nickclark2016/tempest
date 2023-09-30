#ifndef tempest_graphics_visbuffer_pass_hpp
#define tempest_graphics_visbuffer_pass_hpp

#include "../command_buffer.hpp"
#include "../device.hpp"

namespace tempest::graphics
{
    struct visibility_buffer_pass
    {
        bool initialize(gfx_device& device, std::uint32_t width, std::uint32_t height);
        void record(command_buffer& buf, descriptor_set_handle world_data, std::span<std::uint32_t> world_set_offset);
        void release(gfx_device& device);

        texture_handle visibility_buffer;
        texture_handle resolve_texture;
        buffer_handle material_count_buffer;
        buffer_handle material_start_buffer;
        buffer_handle pixel_xy_buffer;

        pipeline_handle visbuffer_populate_gfx;
        pipeline_handle material_count_cs;
        pipeline_handle material_start_cs;
        pipeline_handle pixel_sort_cs;

        descriptor_set_layout_handle world_desc_layout;
        descriptor_set_layout_handle vis_buffer_layout;
        descriptor_set_handle vis_buffer_desc_set;

        VkFormat visbuffer_fmt = VK_FORMAT_R32G32_UINT;
        VkFormat depth_fmt = VK_FORMAT_D32_SFLOAT;
    };
}

#endif // tempest_graphics_visbuffer_pass_hpp