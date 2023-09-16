#ifndef tempest_graphics_blit_pass_hpp
#define tempest_graphics_blit_pass_hpp

#include "../command_buffer.hpp"
#include "../device.hpp"

namespace tempest::graphics
{
    struct blit_pass
    {
        bool initialize(gfx_device& device, std::uint16_t width, std::uint16_t height, VkFormat blit_src_format);
        void record(command_buffer& buf, texture_handle blit_dst, VkRect2D viewport);
        void release(gfx_device& device);

        void resize_blit_source(gfx_device& device, std::uint16_t width, std::uint16_t height,
                                VkFormat blit_src_format);

        void transition_to_present(command_buffer& buf, texture_handle blit_dst);

        texture_handle blit_src;

        inline static constexpr resource_state required_input_layout = resource_state::FRAGMENT_SHADER_RESOURCE;
    };
}

#endif // tempest_graphics_blit_pass_hpp