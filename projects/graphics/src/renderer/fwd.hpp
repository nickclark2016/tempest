#ifndef tempest_graphics_fwd_hpp__
#define tempest_graphics_fwd_hpp__

#include <cstdint>
#include <limits>

namespace tempest::graphics
{
    struct device_render_frame;
    struct gfx_timestamp;
    struct pipeline;
    struct render_pass;

    class command_buffer;
    class command_buffer_ring;
    class descriptor_pool;
    class gfx_device;
    class gfx_timestamp_manager;

    using resource_handle = std::uint32_t;
    inline constexpr resource_handle invalid_resource_handle = std::numeric_limits<resource_handle>::max();
}

#endif // tempest_graphics_fwd_hpp__
