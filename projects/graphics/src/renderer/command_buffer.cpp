#include "command_buffer.hpp"

#include "device.hpp"
#include "resources.hpp"

#include <tempest/logger.hpp>

namespace tempest::graphics
{
    namespace
    {
        auto logger = tempest::logger::logger_factory::create({.prefix{"tempest::graphics::command_buffer"}});
    }

    command_buffer::command_buffer(VkCommandBuffer buf, gfx_device* device, queue_type type, std::uint32_t buffer_size,
                                   std::uint32_t submit_size, bool is_baked)
        : _buf{buf}, _device{device}, _type{type}, _buffer_size{buffer_size}, _is_baked{is_baked}
    {
    }

    command_buffer::operator VkCommandBuffer() const noexcept
    {
        return _buf;
    }

    command_buffer& command_buffer::reset()
    {
        _is_recording = false;
        _active_pass = nullptr;
        _active_pipeline = nullptr;
        _current_command = 0;

        return *this;
    }

    command_buffer& command_buffer::set_clear_color(float r, float g, float b, float a)
    {
        _clear_values[0].color = VkClearColorValue{
            .float32{r, g, b, a},
        };
        return *this;
    }

    command_buffer& command_buffer::set_clear_depth_stencil(float depth, std::uint32_t stencil)
    {
        _clear_values[1].depthStencil = VkClearDepthStencilValue{
            .depth{depth},
            .stencil{stencil},
        };
        return *this;
    }

    command_buffer& command_buffer::set_scissor_region(VkRect2D scissor)
    {
        _device->_dispatch.cmdSetScissor(_buf, 0, 1, &scissor);

        return *this;
    }

    command_buffer& command_buffer::use_default_scissor()
    {
        VkRect2D scissor{
            .offset{
                .x{0},
                .y{0},
            },
            .extent{_device->_winfo.swapchain.extent},
        };

        return set_scissor_region(scissor);
    }

    command_buffer& command_buffer::set_viewport(VkViewport viewport)
    {
        // invert y
        viewport.y = viewport.height;
        viewport.height *= -1.0f;

        _device->_dispatch.cmdSetViewport(_buf, 0, 1, &viewport);

        return *this;
    }

    command_buffer& command_buffer::use_default_viewport()
    {
        VkViewport viewport = {
            .x{0},
            .y{0},
            .width{static_cast<float>(_device->_winfo.swapchain.extent.width)},
            .height{static_cast<float>(_device->_winfo.swapchain.extent.height)},
            .minDepth{0.0f},
            .maxDepth{1.0f},
        };

        return set_viewport(viewport);
    }

    command_buffer& command_buffer::bind_render_pass(render_pass_handle pass)
    {
        _is_recording = true;
        render_pass* p = _device->access_render_pass(pass);

        if (_active_pass != nullptr && _active_pass->type != render_pass_type::COMPUTE && p != _active_pass)
        {
            _device->_dispatch.cmdEndRenderPass(_buf);
        }

        if (p != _active_pass && p->type != render_pass_type::COMPUTE)
        {
            VkRenderPassBeginInfo begin = {
                .sType{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO},
                .pNext{nullptr},
                .renderPass{p->pass},
                .framebuffer{p->type == render_pass_type::SWAPCHAIN
                                 ? _device->_winfo.swapchain_targets[_device->_winfo.image_index]
                                 : p->target},
                .renderArea{
                    .offset{
                        .x{0},
                        .y{0},
                    },
                    .extent{
                        .width{p->width},
                        .height{p->height},
                    },
                },
                .clearValueCount{2},
                .pClearValues{_clear_values.data()},
            };

            _device->_dispatch.cmdBeginRenderPass(_buf, &begin, VK_SUBPASS_CONTENTS_INLINE);
        }

        _active_pass = p;

        return *this;
    }

    command_buffer& command_buffer::bind_pipeline(pipeline_handle pipeline)
    {
        auto pipe = _device->access_pipeline(pipeline);
        _device->_dispatch.cmdBindPipeline(_buf, pipe->kind, pipe->pipeline);
        _active_pipeline = pipe;

        return *this;
    }

    command_buffer& command_buffer::draw(const std::uint32_t vertex_count, std::uint32_t instance_count,
                                         std::uint32_t first_vertex, std::uint32_t first_instance)
    {
        _device->_dispatch.cmdDraw(_buf, vertex_count, instance_count, first_vertex, first_instance);

        return *this;
    }

    void command_buffer::begin()
    {
        reset();

        VkCommandBufferBeginInfo info = {
            .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO},
            .flags{VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT},
        };
        _device->_dispatch.beginCommandBuffer(_buf, &info);
    }

    void command_buffer::end()
    {
        _device->_dispatch.endCommandBuffer(_buf);
    }

    command_buffer_ring::command_buffer_ring(gfx_device* dev) : _dev{dev}
    {
        for (std::size_t i = 0; i < max_pools; ++i)
        {
            VkCommandPoolCreateInfo cmd_pool_info = {
                .sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO},
                .pNext{nullptr},
                .flags{VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT},
                .queueFamilyIndex{dev->_graphics_queue_family},
            };

            auto result = dev->_dispatch.createCommandPool(&cmd_pool_info, dev->_alloc_callbacks, &_cmd_pools[i]);
            if (result != VK_SUCCESS)
            {
                logger->error("Failed to create VkCommandPool.");
            }
        }

        for (std::size_t i = 0; i < max_buffers; ++i)
        {
            auto pool = get_command_pool(i);

            VkCommandBufferAllocateInfo alloc_info = {
                .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO},
                .pNext{nullptr},
                .commandPool{pool},
                .level{VK_COMMAND_BUFFER_LEVEL_PRIMARY},
                .commandBufferCount{1},
            };

            VkCommandBuffer cmd;
            auto cmd_alloc_result = _dev->_dispatch.allocateCommandBuffers(&alloc_info, &cmd);
            if (cmd_alloc_result != VK_SUCCESS)
            {
                logger->error("Failed to allocate VkCommandBuffer {0}", i);
            }

            _command_buffers[i] = command_buffer(cmd, _dev, queue_type::GRAPHICS, 0, 0, false);
            _command_buffers[i]._handle = static_cast<std::uint32_t>(i);
            _command_buffers[i].reset();
        }
    }

    command_buffer_ring::~command_buffer_ring()
    {
        for (std::size_t i = 0; i < max_pools; ++i)
        {
            _dev->_dispatch.destroyCommandPool(_cmd_pools[i], _dev->_alloc_callbacks);
        }
    }

    VkCommandPool command_buffer_ring::get_command_pool(std::size_t index) const noexcept
    {
        return _cmd_pools[index / buffers_per_pool];
    }

    void command_buffer_ring::reset_pools(std::uint32_t frame)
    {
        for (std::size_t i = 0; i < max_threads; ++i)
        {
            _dev->_dispatch.resetCommandPool(_cmd_pools[frame * max_threads + i], 0);
        }
    }

    command_buffer& command_buffer_ring::fetch_buffer(std::uint32_t frame)
    {
        auto& buf = _command_buffers[frame * buffers_per_pool];
        return buf;
    }

    command_buffer& command_buffer_ring::fetch_buffer_instant(std::uint32_t frame)
    {
        auto& buf = _command_buffers[frame * buffers_per_pool + 1];
        return buf;
    }
} // namespace tempest::graphics