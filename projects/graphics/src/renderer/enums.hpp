#ifndef tempest_graphics_enums_hpp__
#define tempest_graphics_enums_hpp__

#include <vulkan/vulkan.h>

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

    enum class texture_flags
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

    enum class resource_deletion_type
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
} // namespace tempest::graphics

#endif // tempest_graphics_enums_hpp__
