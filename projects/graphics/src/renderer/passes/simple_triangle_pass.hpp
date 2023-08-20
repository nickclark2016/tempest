#ifndef tempest_graphics_simple_triangle_pass_hpp
#define tempest_graphics_simple_triangle_pass_hpp

#include "../command_buffer.hpp"
#include "../device.hpp"

namespace tempest::graphics
{
    struct simple_triangle_pass
    {
        bool initialize(gfx_device& device, VkFormat _color_format, VkFormat _depth_format, descriptor_set_layout_handle meshes);
        void record(command_buffer& buf, texture_handle color_target, texture_handle depth_target, VkRect2D viewport,
                    descriptor_set_handle mesh_desc);
        void release(gfx_device& device);

        pipeline_handle triangle_pipeline{};
    };
} // namespace tempest::graphics

#endif // tempest_graphics_simple_triangle_pass_hpp