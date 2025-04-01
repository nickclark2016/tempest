#ifndef tempest_graphics_pass_hpp
#define tempest_graphics_pass_hpp

#include <tempest/graphics_components.hpp>
#include <tempest/int.hpp>
#include <tempest/types.hpp>

namespace tempest::graphics::passes
{
    struct descriptor_bind_point
    {
        descriptor_binding_type type;
        uint32_t binding;
        uint32_t set;
        uint32_t count;

        inline constexpr descriptor_binding_info to_binding_info() const noexcept
        {
            return {
                .type = type,
                .binding_index = binding,
                .binding_count = count,
            };
        }
    };

    struct draw_command_state
    {
        buffer_resource_handle indirect_command_buffer;
        size_t first_indirect_command;
        size_t indirect_command_count;
        bool double_sided;
    };

    struct compute_command_state
    {
        buffer_resource_handle indirect_command_buffer;
        size_t offset;
        uint32_t x;
        uint32_t y;
        uint32_t z;
    };
} // namespace tempest::graphics::passes

#endif // tempest_graphics_pass_hpp
