#ifndef tempest_graphics_enums_hpp
#define tempest_graphics_enums_hpp

#include <vulkan/vulkan.h>

#include <cstdint>
#include <type_traits>

namespace tempest::graphics
{
    enum class render_pass_attachment_operation
    {
        LOAD = VK_ATTACHMENT_LOAD_OP_LOAD,
        CLEAR = VK_ATTACHMENT_LOAD_OP_CLEAR,
        DONT_CARE = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    };

    enum class render_pass_type
    {
        RASTERIZATION = VK_PIPELINE_BIND_POINT_GRAPHICS,
        COMPUTE = VK_PIPELINE_BIND_POINT_COMPUTE,
        SWAPCHAIN = 10,
    };

    enum class resource_usage
    {
        IMMUTABLE,
        DYNAMIC,
        STREAM
    };

    enum class texture_type
    {
        D1,
        D2,
        D3,
        D1_ARRAY,
        D2_ARRAY,
        CUBE_ARRAY
    };

    enum class texture_flags : std::uint8_t
    {
        DEFAULT = 1 << 0,
        RENDER_TARGET = 1 << 1,
        COMPUTE_TARGET = 1 << 2
    };

    enum class pipeline_stage
    {
        DRAW_INDIRECT,
        VERTEX_INPUT,
        VERTEX_SHADER,
        FRAGMENT_SHADER,
        FRAMEBUFFER_OUTPUT,
        COMPUTE_SHADER,
        TRANSFER
    };

    enum class resource_type
    {
        BUFFER,
        TEXTURE,
        PIPELINE,
        SAMPLER,
        DESCRIPTOR_SET_LAYOUT,
        DESCRIPTOR_SET,
        RENDER_PASS,
        SHADER_STATE,
    };

    enum class queue_type
    {
        GRAPHICS = VK_QUEUE_GRAPHICS_BIT,
        TRANSFER = VK_QUEUE_TRANSFER_BIT,
        COMPUTE = VK_QUEUE_COMPUTE_BIT
    };

    enum class resource_state
    {
        UNDEFINED = 0x0,
        VERTEX_AND_UNIFORM_BUFFER = 0x01,
        INDEX_BUFFER = 0x02,
        RENDER_TARGET = 0x04,
        UNORDERED_MEMORY_ACCESS = 0x08,
        DEPTH_WRITE = 0x10,
        DEPTH_READ = 0x20,
        NON_FRAGMENT_SHADER_RESOURCE = 0x40,
        FRAGMENT_SHADER_RESOURCE = 0x80,
        GENERIC_SHADER_RESOURCE = 0x40 | 0x80,
        OUTPUT_STREAM = 0x100,
        INDIRECT_ARGUMENT_BUFFER = 0x200,
        TRANSFER_SRC = 0x400,
        TRANSFER_DST = 0x800,
        READ_OP = 0x1 | 0x2 | 0x40 | 0x80 | 0x200 | 0x400,
        PRESENT = 0x1000,
        COMMON = 0x2000
        // TODO: RT, Variable Shading
    };

    inline resource_state operator&(resource_state lhs, resource_state rhs) noexcept
    {
        return static_cast<resource_state>(static_cast<std::underlying_type_t<resource_state>>(lhs) &
                                           static_cast<std::underlying_type_t<resource_state>>(rhs));
    }

    inline resource_state operator|(resource_state lhs, resource_state rhs) noexcept
    {
        return static_cast<resource_state>(static_cast<std::underlying_type_t<resource_state>>(lhs) |
                                           static_cast<std::underlying_type_t<resource_state>>(rhs));
    }

    inline resource_state operator^(resource_state lhs, resource_state rhs) noexcept
    {
        return static_cast<resource_state>(static_cast<std::underlying_type_t<resource_state>>(lhs) ^
                                           static_cast<std::underlying_type_t<resource_state>>(rhs));
    }
} // namespace tempest::graphics

#endif // tempest_graphics_enums_hpp
