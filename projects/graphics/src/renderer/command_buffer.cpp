#include "command_buffer.hpp"

#include "device.hpp"
#include "resources.hpp"

#include <tempest/logger.hpp>

namespace tempest::graphics
{
    namespace
    {
        auto logger = tempest::logger::logger_factory::create({.prefix{"tempest::graphics::command_buffer"}});

        resource_state as_resource_state(pipeline_stage stage)
        {
            auto index = static_cast<std::underlying_type_t<pipeline_stage>>(stage);
            static constexpr resource_state states[] = {
                resource_state::INDIRECT_ARGUMENT_BUFFER,
                resource_state::VERTEX_AND_UNIFORM_BUFFER,
                resource_state::NON_FRAGMENT_SHADER_RESOURCE,
                resource_state::FRAGMENT_SHADER_RESOURCE,
                resource_state::RENDER_TARGET,
                resource_state::UNORDERED_MEMORY_ACCESS,
                resource_state::TRANSFER_DST,
            };

            return states[index];
        }
    } // namespace

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

        _device->_dispatch.resetCommandBuffer(_buf, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

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

    command_buffer& command_buffer::set_viewport(VkViewport viewport, bool flip)
    {
        // invert y
        if (flip)
        {
            viewport.y = viewport.height;
            viewport.height *= -1.0f;
        }

        _device->_dispatch.cmdSetViewport(_buf, 0, 1, &viewport);

        return *this;
    }

    command_buffer& command_buffer::use_default_viewport(bool flip)
    {
        VkViewport viewport = {
            .x{0},
            .y{0},
            .width{static_cast<float>(_device->_winfo.swapchain.extent.width)},
            .height{static_cast<float>(_device->_winfo.swapchain.extent.height)},
            .minDepth{0.0f},
            .maxDepth{1.0f},
        };

        return set_viewport(viewport, flip);
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
                .clearValueCount{p->num_render_targets + (p->output_depth_attachment ? 1u : 0u)},
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

    command_buffer& command_buffer::bind_descriptor_set(std::span<descriptor_set_handle> sets,
                                                        std::span<std::uint32_t> offsets, std::uint32_t first_set)
    {
        std::array<VkDescriptorSet, max_descriptor_set_layouts> vk_desc_sets;
        std::array<std::uint32_t, max_descriptor_set_layouts> desc_offsets;
        std::uint32_t num_offsets{0};

        for (std::size_t i = 0; i < sets.size(); ++i)
        {
            descriptor_set* set = _device->access_descriptor_set(sets[i]);
            vk_desc_sets[i] = set->set;

            auto set_layout = set->layout;
            for (std::uint32_t layout = 0; layout < set_layout->num_bindings; ++layout)
            {
                const auto& binding = set_layout->bindings[layout];
                switch (binding.type)
                {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    [[fallthrough]];
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
                    const auto resource_index = set->bindings[layout];
                    resource_handle buf_handle = set->resources[resource_index];
                    buffer* buf = _device->access_buffer(buffer_handle{.index{buf_handle}});
                    desc_offsets[num_offsets++] = buf->global_offset;
                    break;
                }
                default:
                    break;
                }
            }
        }

        _device->_dispatch.cmdBindDescriptorSets(_buf, _active_pipeline->kind, _active_pipeline->layout, first_set,
                                                 static_cast<std::uint32_t>(sets.size()), vk_desc_sets.data(),
                                                 num_offsets, num_offsets ? desc_offsets.data() : nullptr);

        return *this;
    }

    command_buffer& command_buffer::draw(const std::uint32_t vertex_count, std::uint32_t instance_count,
                                         std::uint32_t first_vertex, std::uint32_t first_instance)
    {
        _device->_dispatch.cmdDraw(_buf, vertex_count, instance_count, first_vertex, first_instance);

        return *this;
    }

    command_buffer& command_buffer::barrier(const execution_barrier& barrier)
    {
        if (_active_pass && _active_pass->type != render_pass_type::COMPUTE)
        {
            _device->_dispatch.cmdEndRenderPass(_buf);
            _active_pass = nullptr;
        }

        std::uint32_t buffer_count = static_cast<std::uint32_t>(barrier.buffers.size());
        std::uint32_t image_count = static_cast<std::uint32_t>(barrier.textures.size());

        assert(buffer_count <= 8 && "Received more memory barriers than supported (8).");
        assert(image_count <= 8 && "Received more image barriers than supported (8).");

        std::array<VkBufferMemoryBarrier, 8> memory_barriers;
        std::array<VkImageMemoryBarrier, 8> image_barriers;

        VkImageLayout new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkImageLayout new_depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        VkAccessFlags source_access_mask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        VkAccessFlags source_buffer_access_mask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        VkAccessFlags source_depth_access_mask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        VkAccessFlags destination_access_mask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        VkAccessFlags destination_buffer_access_mask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        VkAccessFlags destination_depth_access_mask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        switch (barrier.destination)
        {
        case pipeline_stage::COMPUTE_SHADER: {
            new_layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        }
        case pipeline_stage::FRAMEBUFFER_OUTPUT: {
            new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            new_depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            destination_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                      VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT;
            destination_depth_access_mask =
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            break;
        }
        case pipeline_stage::DRAW_INDIRECT: {
            destination_buffer_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            break;
        }
        default:
            break;
        }

        switch (barrier.source)
        {
        case pipeline_stage::FRAMEBUFFER_OUTPUT: {
            source_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            source_depth_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        }
        case pipeline_stage::DRAW_INDIRECT: {
            source_buffer_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            break;
        }
        default:
            break;
        }

        if (barrier.source == pipeline_stage::FRAMEBUFFER_OUTPUT &&
            barrier.destination == pipeline_stage::FRAGMENT_SHADER)
        {
            source_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            destination_access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        if (barrier.source == pipeline_stage::FRAGMENT_SHADER &&
            barrier.destination == pipeline_stage::FRAMEBUFFER_OUTPUT)
        {
            source_access_mask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
            destination_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }

        bool has_depth_buffer = false;

        // populate barriers
        for (std::size_t i = 0; i < buffer_count; ++i)
        {
            auto& buf_barrier = barrier.buffers[i];
            buffer* buf = _device->access_buffer(buf_barrier.buf);

            memory_barriers[i] = {
                .sType{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER},
                .pNext{nullptr},
                .srcAccessMask{source_access_mask},
                .dstAccessMask{destination_access_mask},
                .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .buffer{buf->underlying},
                .offset{0},
                .size{buf->size},
            };
        }

        std::size_t image_cnt{0};
        for (std::size_t i = 0; i < image_count; ++i)
        {
            auto& img_barrier = barrier.textures[i];
            texture* tex = _device->access_texture(img_barrier.tex);

            const auto is_color = !texture_format_utils::has_depth_or_stencil(tex->image_fmt);
            has_depth_buffer |= !is_color;

            image_barriers[image_cnt] = {
                .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                .pNext{nullptr},
                .srcAccessMask{is_color ? source_access_mask : source_depth_access_mask},
                .dstAccessMask{is_color ? destination_access_mask : destination_depth_access_mask},
                .oldLayout{tex->image_layout},
                .newLayout{is_color ? new_layout : new_depth_layout},
                .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .image{tex->underlying_image},
                .subresourceRange{
                    .aspectMask{is_color ? VK_IMAGE_ASPECT_COLOR_BIT
                                         : static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT |
                                                                           VK_IMAGE_ASPECT_STENCIL_BIT)},
                    .baseMipLevel{0},
                    .levelCount{1},
                    .baseArrayLayer{0},
                    .layerCount{1},
                },
            };

            tex->image_layout = image_barriers[i].newLayout;

            if (image_barriers[image_cnt].oldLayout != image_barriers[image_cnt].newLayout)
            {
                ++image_cnt;
            }
        }

        auto src_stage_mask = to_vk_pipeline_stage(barrier.source);
        auto dst_stage_mask = to_vk_pipeline_stage(barrier.destination);

        _device->_dispatch.cmdPipelineBarrier(_buf, to_vk_pipeline_stage(barrier.source),
                                              to_vk_pipeline_stage(barrier.destination), 0, 0, nullptr, buffer_count,
                                              buffer_count ? memory_barriers.data() : nullptr, image_cnt,
                                              image_cnt ? image_barriers.data() : nullptr);

        return *this;
    }

    command_buffer& command_buffer::transition_to_depth_image(texture_handle depth_tex)
    {
        auto tex = _device->access_texture(depth_tex);
        VkImageMemoryBarrier barrier = {
            .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
            .pNext{nullptr},
            .srcAccessMask{0},
            .dstAccessMask{0},
            .oldLayout{VK_IMAGE_LAYOUT_UNDEFINED},
            .newLayout{VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL},
            .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
            .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
            .image{tex->underlying_image},
            .subresourceRange{
                .aspectMask{VK_IMAGE_ASPECT_DEPTH_BIT},
                .baseMipLevel{0},
                .levelCount{1},
                .baseArrayLayer{0},
                .layerCount{1},
            },
        };

        _device->_dispatch.cmdPipelineBarrier(_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                              &barrier);

        tex->image_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

        return *this;
    }

    command_buffer& command_buffer::transition_to_color_image(texture_handle color_tex)
    {
        auto tex = _device->access_texture(color_tex);
        VkImageMemoryBarrier barrier = {
            .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
            .pNext{nullptr},
            .srcAccessMask{0},
            .dstAccessMask{0},
            .oldLayout{VK_IMAGE_LAYOUT_UNDEFINED},
            .newLayout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
            .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
            .image{tex->underlying_image},
            .subresourceRange{
                .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                .baseMipLevel{0},
                .levelCount{1},
                .baseArrayLayer{0},
                .layerCount{1},
            },
        };

        _device->_dispatch.cmdPipelineBarrier(_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                              &barrier);

        tex->image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
        if (_active_pass && _active_pass->type != render_pass_type::COMPUTE)
        {
            _device->_dispatch.cmdEndRenderPass(_buf);
            _active_pass = nullptr;
        }
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