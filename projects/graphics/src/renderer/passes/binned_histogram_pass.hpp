#ifndef tempest_graphics_binned_histogram_pass_hpp
#define tempest_graphics_binned_histogram_pass_hpp

#include "../device.hpp"

namespace tempest::graphics
{
    struct binned_histogram_pass
    {
        bool initialize(gfx_device& device);
        void record(command_buffer& buf);
        void release(gfx_device& device);

        descriptor_set_layout_handle compute_ios_layout;
        descriptor_set_handle compute_ios;
        pipeline_handle compute_shader;
        buffer_handle input;
        buffer_handle output;

        inline static constexpr resource_state required_input_layout = resource_state::FRAGMENT_SHADER_RESOURCE;
    };
}

#endif // tempest_graphics_binned_histogram_pass_hpp