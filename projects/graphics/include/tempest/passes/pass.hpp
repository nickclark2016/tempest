#ifndef tempest_graphics_pass_hpp
#define tempest_graphics_pass_hpp

#include <tempest/graphics_components.hpp>
#include <tempest/types.hpp>

namespace tempest::graphics::passes
{
    struct draw_command_state
    {
        buffer_resource_handle indirect_command_buffer;
        size_t first_indirect_command;
        size_t indirect_command_count;
        bool double_sided;
    };
}

#endif // tempest_graphics_pass_hpp
