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

    command_buffer& command_buffer::begin_rendering(VkRect2D viewport, std::span<render_attachment_descriptor> colors,
                                                    const std::optional<render_attachment_descriptor>& depth,
                                                    const std::optional<render_attachment_descriptor>& stencil)
    {
        std::array<VkRenderingAttachmentInfo, max_framebuffer_attachments> vk_colors;
        std::optional<VkRenderingAttachmentInfo> vk_depth;
        std::optional<VkRenderingAttachmentInfo> vk_stencil;

        std::uint32_t color_count = 0;

        for (const auto& color : colors)
        {
            vk_colors[color_count++] = {
                .sType{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},
                .pNext{nullptr},
                .imageView{_device->access_texture(color.tex)->underlying_view},
                .imageLayout{color.layout},
                .resolveMode{color.resolve_mode},
                .resolveImageView{color.resolve_target ? _device->access_texture(color.resolve_target)->underlying_view
                                                       : nullptr},
                .resolveImageLayout{color.resolve_layout},
                .loadOp{color.load},
                .storeOp{color.store},
                .clearValue{color.clear},
            };
        }

        if (depth)
        {
            vk_depth = VkRenderingAttachmentInfo{
                .sType{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},
                .pNext{nullptr},
                .imageView{_device->access_texture(depth->tex)->underlying_view},
                .imageLayout{depth->layout},
                .resolveMode{depth->resolve_mode},
                .resolveImageView{
                    depth->resolve_target ? _device->access_texture(depth->resolve_target)->underlying_view : nullptr},
                .resolveImageLayout{depth->resolve_layout},
                .loadOp{depth->load},
                .storeOp{depth->store},
                .clearValue{depth->clear},
            };
        }

        if (stencil)
        {
            vk_stencil = VkRenderingAttachmentInfo{
                .sType{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},
                .pNext{nullptr},
                .imageView{_device->access_texture(stencil->tex)->underlying_view},
                .imageLayout{stencil->layout},
                .resolveMode{stencil->resolve_mode},
                .resolveImageView{stencil->resolve_target
                                      ? _device->access_texture(stencil->resolve_target)->underlying_view
                                      : nullptr},
                .resolveImageLayout{stencil->resolve_layout},
                .loadOp{stencil->load},
                .storeOp{stencil->store},
                .clearValue{stencil->clear},
            };
        }

        VkRenderingInfo render_info{
            .sType{VK_STRUCTURE_TYPE_RENDERING_INFO},
            .pNext{nullptr},
            .flags{0},
            .renderArea{viewport},
            .layerCount{1},
            .colorAttachmentCount{color_count},
            .pColorAttachments{color_count ? vk_colors.data() : nullptr},
            .pDepthAttachment{vk_depth ? &*vk_depth : nullptr},
            .pStencilAttachment{vk_stencil ? &*vk_stencil : nullptr},
        };

        _device->_dispatch.cmdBeginRendering(_buf, &render_info);

        // TODO: insert return statement here
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
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                    [[fallthrough]];
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    [[fallthrough]];
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
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

        std::uint32_t image_cnt{0};
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

    command_buffer& command_buffer::end_rendering()
    {
        _device->_dispatch.cmdEndRendering(_buf);
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

    command_buffer& command_buffer::blit_image(texture_handle src, texture_handle dst)
    {
        auto src_tex = _device->access_texture(src);
        auto dst_tex = _device->access_texture(dst);

        VkImageBlit blit = {
            .srcSubresource{
                .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                .mipLevel{0},
                .baseArrayLayer{0},
                .layerCount{1},
            },
            .srcOffsets{
                {
                    .x{0},
                    .y{0},
                    .z{0},
                },
                {
                    .x{src_tex->width},
                    .y{src_tex->height},
                    .z{src_tex->depth},
                },
            },
            .dstSubresource{
                .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                .mipLevel{0},
                .baseArrayLayer{0},
                .layerCount{1},
            },
            .dstOffsets{
                {
                    .x{0},
                    .y{0},
                    .z{0},
                },
                {
                    .x{dst_tex->width},
                    .y{dst_tex->height},
                    .z{dst_tex->depth},
                },
            },
        };

        VkImageLayout old_src_layout = src_tex->image_layout;
        VkImageLayout old_dst_layout = dst_tex->image_layout;

        std::uint32_t barrier_count{0};
        VkImageMemoryBarrier barriers[2];

        if (old_src_layout != VK_IMAGE_LAYOUT_GENERAL && old_src_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            barriers[barrier_count] = {
                .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                .pNext{nullptr},
                .srcAccessMask{VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
                .dstAccessMask{VK_ACCESS_TRANSFER_READ_BIT},
                .oldLayout{old_src_layout},
                .newLayout{VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
                .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .image{src_tex->underlying_image},
                .subresourceRange{
                    .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                    .baseMipLevel{0},
                    .levelCount{1},
                    .baseArrayLayer{0},
                    .layerCount{1},
                },
            };

            barrier_count++;
        }

        if (old_dst_layout != VK_IMAGE_LAYOUT_GENERAL && old_dst_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barriers[barrier_count] = {
                .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                .pNext{nullptr},
                .srcAccessMask{VK_ACCESS_TRANSFER_WRITE_BIT},
                .dstAccessMask{VK_ACCESS_MEMORY_READ_BIT},
                .oldLayout{old_dst_layout},
                .newLayout{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
                .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .image{dst_tex->underlying_image},
                .subresourceRange{
                    .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                    .baseMipLevel{0},
                    .levelCount{1},
                    .baseArrayLayer{0},
                    .layerCount{1},
                },
            };

            barrier_count++;
        }

        if (barrier_count)
        {
            _device->_dispatch.cmdPipelineBarrier(_buf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                  VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                                                  barrier_count, barriers);
        }

        _device->_dispatch.cmdBlitImage(_buf, src_tex->underlying_image, src_tex->image_layout,
                                        dst_tex->underlying_image, dst_tex->image_layout, 1, &blit, VK_FILTER_LINEAR);

        if (barrier_count)
        {
            barrier_count = 0;
            if (old_src_layout != VK_IMAGE_LAYOUT_GENERAL && old_src_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            {
                barriers[barrier_count++] = {
                    .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                    .pNext{nullptr},
                    .srcAccessMask{VK_ACCESS_TRANSFER_READ_BIT},
                    .dstAccessMask{VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
                    .oldLayout{VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
                    .newLayout{old_src_layout},
                    .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .image{src_tex->underlying_image},
                    .subresourceRange{
                        .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                        .baseMipLevel{0},
                        .levelCount{1},
                        .baseArrayLayer{0},
                        .layerCount{1},
                    },
                };
            }

            if (old_dst_layout != VK_IMAGE_LAYOUT_GENERAL && old_dst_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                barriers[barrier_count++] = {
                    .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                    .pNext{nullptr},
                    .srcAccessMask{VK_ACCESS_TRANSFER_WRITE_BIT},
                    .dstAccessMask{VK_ACCESS_MEMORY_READ_BIT},
                    .oldLayout{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
                    .newLayout{old_dst_layout},
                    .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .image{dst_tex->underlying_image},
                    .subresourceRange{
                        .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                        .baseMipLevel{0},
                        .levelCount{1},
                        .baseArrayLayer{0},
                        .layerCount{1},
                    },
                };
            }

            _device->_dispatch.cmdPipelineBarrier(_buf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                  VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                                                  barrier_count, barriers);
        }

        return *this;
    }

    namespace
    {
        VkImageAspectFlags get_aspect(resource_state state)
        {
            switch (state)
            {
            case resource_state::DEPTH_READ:
            case resource_state::DEPTH_WRITE:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case resource_state::GENERIC_SHADER_RESOURCE:
            case resource_state::RENDER_TARGET:
            case resource_state::PRESENT:
                return VK_IMAGE_ASPECT_COLOR_BIT;
            default:
                return 0;
            }
        }
    } // namespace

    command_buffer& command_buffer::transition_resource(std::span<state_transition_descriptor> descs,
                                                        pipeline_stage src, pipeline_stage dst)
    {
        static constexpr std::size_t MAX_BARRIER_COUNT = 16;

        assert(descs.size() <= MAX_BARRIER_COUNT);

        VkImageMemoryBarrier images[MAX_BARRIER_COUNT];
        VkBufferMemoryBarrier buffers[MAX_BARRIER_COUNT];

        std::uint32_t image_barrier_count{0};
        std::uint32_t buffer_barrier_count{0};

        for (const state_transition_descriptor& desc : descs)
        {
            if (desc.texture)
            {
                auto texture = _device->access_texture(desc.texture);

                VkImageMemoryBarrier& barrier = images[image_barrier_count++];
                barrier = VkImageMemoryBarrier{
                    .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                    .pNext{nullptr},
                    .srcAccessMask{to_vk_access_flags(desc.src_state)},
                    .dstAccessMask{to_vk_access_flags(desc.dst_state)},
                    .oldLayout{to_vk_image_layout(desc.src_state)},
                    .newLayout{to_vk_image_layout(desc.dst_state)},
                    .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .image{texture->underlying_image},
                    .subresourceRange{
                        .aspectMask{get_aspect(desc.src_state) | get_aspect(desc.dst_state)},
                        .baseMipLevel{desc.first_mip},
                        .levelCount{desc.mip_count},
                        .baseArrayLayer{desc.base_layer},
                        .layerCount{desc.layer_count},
                    },
                };
            }
            else if (desc.buffer)
            {
                auto buffer = _device->access_buffer(desc.buffer);

                VkBufferMemoryBarrier& barrier = buffers[buffer_barrier_count++];
                barrier = VkBufferMemoryBarrier{
                    .sType{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER},
                    .pNext{nullptr},
                    .srcAccessMask{to_vk_access_flags(desc.src_state)},
                    .dstAccessMask{to_vk_access_flags(desc.dst_state)},
                    .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                    .buffer{buffer->underlying},
                    .offset{desc.offset},
                    .size{desc.range},
                };
            }
        }

        _device->_dispatch.cmdPipelineBarrier(_buf, to_vk_pipeline_stage(src), to_vk_pipeline_stage(dst), 0, 0, nullptr,
                                              buffer_barrier_count, buffer_barrier_count ? buffers : nullptr,
                                              image_barrier_count, image_barrier_count ? images : nullptr);

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