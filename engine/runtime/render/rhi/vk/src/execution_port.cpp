#include <tempest/vk/execution_port.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/vk/device.hpp>
#include <vulkan/vulkan_core.h>

namespace tempest::rhi::vk
{
    namespace
    {
        auto as_vulkan(filter_mode mode) -> VkFilter
        {
            switch (mode)
            {
            case filter_mode::nearest:
                return VK_FILTER_NEAREST;
            case filter_mode::linear:
                return VK_FILTER_LINEAR;
            default:
                return VK_FILTER_LINEAR;
            }
        }

        auto infer_aspect_flags(VkFormat format) -> VkImageAspectFlags
        {
            switch (format)
            {
            case VK_FORMAT_D16_UNORM:
                [[fallthrough]];
            case VK_FORMAT_X8_D24_UNORM_PACK32:
                [[fallthrough]];
            case VK_FORMAT_D32_SFLOAT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case VK_FORMAT_S8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
            }
        }

        auto as_vulkan(enum_mask<pipeline_stage> stages) -> VkPipelineStageFlags2
        {
            auto flags = VkPipelineStageFlags2{0};

            if (stages & pipeline_stage::top_of_pipe)
            {
                flags |= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            }

            if ((stages & pipeline_stage::all_transfer) == pipeline_stage::all_transfer)
            {
                flags |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            }
            else
            {
                if (stages & pipeline_stage::copy)
                {
                    flags |= VK_PIPELINE_STAGE_2_COPY_BIT;
                }

                if (stages & pipeline_stage::blit)
                {
                    flags |= VK_PIPELINE_STAGE_2_BLIT_BIT;
                }

                if (stages & pipeline_stage::resolve)
                {
                    flags |= VK_PIPELINE_STAGE_2_RESOLVE_BIT;
                }

                if (stages & pipeline_stage::clear)
                {
                    flags |= VK_PIPELINE_STAGE_2_CLEAR_BIT;
                }
            }

            if ((stages & pipeline_stage::all_graphics) == pipeline_stage::all_graphics)
            {
                flags |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
            }
            else
            {
                if (stages & pipeline_stage::indirect_commands)
                {
                    flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
                }

                if (stages & pipeline_stage::index_input)
                {
                    flags |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
                }

                if (stages & pipeline_stage::vertex)
                {
                    flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
                }

                if (stages & pipeline_stage::tessellation_control)
                {
                    flags |= VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;
                }

                if (stages & pipeline_stage::tessellation_evaluation)
                {
                    flags |= VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
                }

                if (stages & pipeline_stage::geometry)
                {
                    flags |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
                }

                if (stages & pipeline_stage::fragment)
                {
                    flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                }

                if (stages & pipeline_stage::early_fragment_tests)
                {
                    flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
                }

                if (stages & pipeline_stage::late_fragment_tests)
                {
                    flags |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                }

                if (stages & pipeline_stage::attachment_output)
                {
                    flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                }
            }

            if (stages & pipeline_stage::compute)
            {
                flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            }

            if (stages & pipeline_stage::build_acceleration_structure)
            {
                flags |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
            }

            if (stages & pipeline_stage::copy_acceleration_structure)
            {
                flags |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR;
            }

            if (stages & pipeline_stage::ray_tracing)
            {
                flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            }

            if (stages & pipeline_stage::bottom_of_pipe)
            {
                flags |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            }

            return flags;
        }

        auto as_vulkan_map_reads(enum_mask<pipeline_stage> stages) -> VkAccessFlags2
        {
            auto flags = VK_ACCESS_2_NONE;
            flags |= (stages & pipeline_stage::copy) ? VK_ACCESS_2_TRANSFER_READ_BIT : 0;
            flags |= (stages & pipeline_stage::blit) ? VK_ACCESS_2_TRANSFER_READ_BIT : 0;
            flags |= (stages & pipeline_stage::resolve) ? VK_ACCESS_2_TRANSFER_READ_BIT : 0;
            flags |= (stages & pipeline_stage::clear) ? VK_ACCESS_2_TRANSFER_READ_BIT : 0;
            flags |= (stages & pipeline_stage::indirect_commands) ? VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT : 0;
            flags |= (stages & pipeline_stage::index_input) ? VK_ACCESS_2_INDEX_READ_BIT : 0;
            flags |= (stages & pipeline_stage::vertex) ? VK_ACCESS_2_SHADER_READ_BIT : 0;
            flags |= (stages & pipeline_stage::tessellation_control) ? VK_ACCESS_2_SHADER_READ_BIT : 0;
            flags |= (stages & pipeline_stage::tessellation_evaluation) ? VK_ACCESS_2_SHADER_READ_BIT : 0;
            flags |= (stages & pipeline_stage::geometry) ? VK_ACCESS_2_SHADER_READ_BIT : 0;
            flags |= (stages & pipeline_stage::fragment) ? VK_ACCESS_2_SHADER_READ_BIT : 0;
            flags |=
                (stages & pipeline_stage::early_fragment_tests) ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT : 0;
            flags |= (stages & pipeline_stage::late_fragment_tests) ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT : 0;
            flags |= (stages & pipeline_stage::attachment_output) ? VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT : 0;
            flags |= (stages & pipeline_stage::compute) ? VK_ACCESS_2_SHADER_READ_BIT : 0;
            flags |= (stages & pipeline_stage::build_acceleration_structure)
                         ? VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
                         : 0;
            flags |= (stages & pipeline_stage::copy_acceleration_structure)
                         ? VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
                         : 0;
            flags |= (stages & pipeline_stage::ray_tracing) ? VK_ACCESS_2_SHADER_READ_BIT : 0;
            return flags;
        }

        auto as_vulkan_map_writes(enum_mask<pipeline_stage> stages) -> VkAccessFlags2
        {
            auto flags = VK_ACCESS_2_NONE;
            flags |= (stages & pipeline_stage::copy) ? VK_ACCESS_2_TRANSFER_WRITE_BIT : 0;
            flags |= (stages & pipeline_stage::blit) ? VK_ACCESS_2_TRANSFER_WRITE_BIT : 0;
            flags |= (stages & pipeline_stage::resolve) ? VK_ACCESS_2_TRANSFER_WRITE_BIT : 0;
            flags |= (stages & pipeline_stage::clear) ? VK_ACCESS_2_TRANSFER_WRITE_BIT : 0;
            flags |= (stages & pipeline_stage::indirect_commands) ? VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT : 0;
            flags |= (stages & pipeline_stage::index_input) ? VK_ACCESS_2_INDEX_READ_BIT : 0;
            flags |= (stages & pipeline_stage::vertex) ? VK_ACCESS_2_SHADER_WRITE_BIT : 0;
            flags |= (stages & pipeline_stage::tessellation_control) ? VK_ACCESS_2_SHADER_WRITE_BIT : 0;
            flags |= (stages & pipeline_stage::tessellation_evaluation) ? VK_ACCESS_2_SHADER_WRITE_BIT : 0;
            flags |= (stages & pipeline_stage::geometry) ? VK_ACCESS_2_SHADER_WRITE_BIT : 0;
            flags |= (stages & pipeline_stage::fragment) ? VK_ACCESS_2_SHADER_WRITE_BIT : 0;
            flags |=
                (stages & pipeline_stage::early_fragment_tests) ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0;
            flags |=
                (stages & pipeline_stage::late_fragment_tests) ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0;
            flags |= (stages & pipeline_stage::attachment_output) ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT : 0;
            flags |= (stages & pipeline_stage::compute) ? VK_ACCESS_2_SHADER_WRITE_BIT : 0;
            flags |= (stages & pipeline_stage::build_acceleration_structure)
                         ? VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
                         : 0;
            flags |= (stages & pipeline_stage::copy_acceleration_structure)
                         ? VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
                         : 0;
            flags |= (stages & pipeline_stage::ray_tracing) ? VK_ACCESS_2_SHADER_WRITE_BIT : 0;
            return flags;
        }

        auto as_vulkan(enum_mask<resource_access> access, enum_mask<pipeline_stage> stages) -> VkAccessFlags2
        {
            auto flags = VK_ACCESS_2_NONE;

            if (access & resource_access::read)
            {
                flags |= as_vulkan_map_reads(stages);
            }

            if (access & resource_access::write)
            {
                flags |= as_vulkan_map_writes(stages);
            }

            return flags;
        }

        auto as_vulkan(enum_mask<shader_stage> stages) -> VkShaderStageFlags
        {
            auto flags = VkShaderStageFlags{0};

            if (stages & shader_stage::vertex)
            {
                flags |= VK_SHADER_STAGE_VERTEX_BIT;
            }

            if (stages & shader_stage::tessellation_control)
            {
                flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            }

            if (stages & shader_stage::tessellation_evaluation)
            {
                flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            }

            if (stages & shader_stage::geometry)
            {
                flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
            }

            if (stages & shader_stage::fragment)
            {
                flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            if (stages & shader_stage::compute)
            {
                flags |= VK_SHADER_STAGE_COMPUTE_BIT;
            }

            if (stages & shader_stage::raygen)
            {
                flags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            }

            if (stages & shader_stage::any_hit)
            {
                flags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
            }

            if (stages & shader_stage::closest_hit)
            {
                flags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            }

            if (stages & shader_stage::miss)
            {
                flags |= VK_SHADER_STAGE_MISS_BIT_KHR;
            }

            if (stages & shader_stage::intersection)
            {
                flags |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
            }

            if (stages & shader_stage::callable)
            {
                flags |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;
            }

            return flags;
        }
    } // namespace

    command_list::command_list(VkCommandBuffer command_buffer, const vkb::DispatchTable& dispatch_table,
                               const vk::device& device, vkb::QueueType queue_type) noexcept
        : _command_buffer{command_buffer}, _dispatch_table{&dispatch_table}, _parent_device{&device},
          _queue_type{queue_type}
    {
    }

    auto command_list::begin() const -> void
    {
        auto begin_info = VkCommandBufferBeginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };

        [[maybe_unused]] auto result = _dispatch_table->beginCommandBuffer(_command_buffer, &begin_info);
        TEMPEST_ASSERT(result == VK_SUCCESS);

        if (_queue_type == vkb::QueueType::graphics || _queue_type == vkb::QueueType::compute)
        {
            auto binding_infos = array<VkDescriptorBufferBindingInfoEXT, 2>{
                VkDescriptorBufferBindingInfoEXT{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                    .pNext = nullptr,
                    .address = _parent_device->get_sampler_descriptor_buffer_address(),
                    .usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
                },
                VkDescriptorBufferBindingInfoEXT{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                    .pNext = nullptr,
                    .address = _parent_device->get_resource_descriptor_buffer_address(),
                    .usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
                },
            };

            _dispatch_table->cmdBindDescriptorBuffersEXT(_command_buffer, static_cast<uint32_t>(binding_infos.size()),
                                                         binding_infos.data());

            // Buffer index 0 is Samplers (Set 0 at offset 0)
            auto sampler_buffer_index = uint32_t{0};
            auto sampler_offset = VkDeviceSize{0};
            if (_queue_type == vkb::QueueType::graphics)
            {
                _dispatch_table->cmdSetDescriptorBufferOffsetsEXT(_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                                  _parent_device->get_global_pipeline_layout(), 0, 1,
                                                                  &sampler_buffer_index, &sampler_offset);
            }
            _dispatch_table->cmdSetDescriptorBufferOffsetsEXT(_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                              _parent_device->get_global_pipeline_layout(), 0, 1,
                                                              &sampler_buffer_index, &sampler_offset);

            // Buffer index 1 is Sampled Images (Set 1 at offset 0) and Storage Images (Set 2 at offset storage_offset)
            auto resource_buffer_indices = array<uint32_t, 2>{1, 1};
            auto resource_offsets =
                array<VkDeviceSize, 2>{0, _parent_device->get_storage_image_descriptor_buffer_offset()};
            if (_queue_type == vkb::QueueType::graphics)
            {
                _dispatch_table->cmdSetDescriptorBufferOffsetsEXT(
                    _command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _parent_device->get_global_pipeline_layout(), 1,
                    2, resource_buffer_indices.data(), resource_offsets.data());
            }
            _dispatch_table->cmdSetDescriptorBufferOffsetsEXT(_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                              _parent_device->get_global_pipeline_layout(), 1, 2,
                                                              resource_buffer_indices.data(), resource_offsets.data());
        }
    }

    auto command_list::end() const -> void
    {
        [[maybe_unused]] auto result = _dispatch_table->endCommandBuffer(_command_buffer);
        TEMPEST_ASSERT(result == VK_SUCCESS);
    }

    namespace
    {
        auto as_vulkan(const device& dev, buffer_barrier barrier) -> VkBufferMemoryBarrier2
        {
            return VkBufferMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = as_vulkan(barrier.src.stages),
                .srcAccessMask = as_vulkan(barrier.src.access, barrier.src.stages),
                .dstStageMask = as_vulkan(barrier.dst.stages),
                .dstAccessMask = as_vulkan(barrier.dst.access, barrier.dst.stages),
                .srcQueueFamilyIndex =
                    barrier.src_queue != nullptr
                        ? static_cast<vk::execution_port*>(barrier.src_queue)->get_queue_family_index()
                        : VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex =
                    barrier.dst_queue != nullptr
                        ? static_cast<vk::execution_port*>(barrier.dst_queue)->get_queue_family_index()
                        : VK_QUEUE_FAMILY_IGNORED,
                .buffer = dev.get_buffer(barrier.buffer)->handle,
                .offset = barrier.offset,
                .size = barrier.size,
            };
        }

        auto as_vulkan(image_layout layout) noexcept -> VkImageLayout
        {
            switch (layout)
            {
            case image_layout::undefined:
                return VK_IMAGE_LAYOUT_UNDEFINED;
            case image_layout::general:
                return VK_IMAGE_LAYOUT_GENERAL;
            case image_layout::present:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }
            return VK_IMAGE_LAYOUT_GENERAL;
        }

        auto as_vulkan(const device& dev, texture_barrier barrier) -> VkImageMemoryBarrier2
        {
            return VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = as_vulkan(barrier.src.stages),
                .srcAccessMask = as_vulkan(barrier.src.access, barrier.src.stages),
                .dstStageMask = as_vulkan(barrier.dst.stages),
                .dstAccessMask = as_vulkan(barrier.dst.access, barrier.dst.stages),
                .oldLayout = as_vulkan(barrier.src.layout),
                .newLayout = as_vulkan(barrier.dst.layout),
                .srcQueueFamilyIndex =
                    barrier.src_queue != nullptr
                        ? static_cast<vk::execution_port*>(barrier.src_queue)->get_queue_family_index()
                        : VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex =
                    barrier.dst_queue != nullptr
                        ? static_cast<vk::execution_port*>(barrier.dst_queue)->get_queue_family_index()
                        : VK_QUEUE_FAMILY_IGNORED,
                .image = dev.get_texture(barrier.texture)->handle,
                .subresourceRange =
                    {
                        .aspectMask = infer_aspect_flags(dev.get_texture(barrier.texture)->format),
                        .baseMipLevel = barrier.base_mip_level,
                        .levelCount = barrier.mip_level_count,
                        .baseArrayLayer = barrier.base_array_layer,
                        .layerCount = barrier.array_layer_count,
                    },
            };
        }
    } // namespace

    auto command_list::pipeline_barrier(span<const texture_barrier> texture_barriers,
                                        span<const buffer_barrier> buffer_barriers) const -> void
    {
        // TODO: Investigate stack allocators instead of relying on the default heap allocator for these temporary
        // vectors. The number of barriers is usually small, so stack allocation should be sufficient.
        auto vk_texture_barriers = vector<VkImageMemoryBarrier2>{};
        auto vk_buffer_barriers = vector<VkBufferMemoryBarrier2>{};
        vk_texture_barriers.reserve(texture_barriers.size());
        vk_buffer_barriers.reserve(buffer_barriers.size());

        for (const auto& barrier : texture_barriers)
        {
            vk_texture_barriers.emplace_back(as_vulkan(*_parent_device, barrier));
        }

        for (const auto& barrier : buffer_barriers)
        {
            vk_buffer_barriers.emplace_back(as_vulkan(*_parent_device, barrier));
        }

        auto dependency_info = VkDependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = static_cast<uint32_t>(vk_buffer_barriers.size()),
            .pBufferMemoryBarriers = vk_buffer_barriers.data(),
            .imageMemoryBarrierCount = static_cast<uint32_t>(vk_texture_barriers.size()),
            .pImageMemoryBarriers = vk_texture_barriers.data(),
        };

        _dispatch_table->cmdPipelineBarrier2(_command_buffer, &dependency_info);
    }

    auto command_list::signal_event(event_handle event, span<const texture_barrier> texture_sources,
                                    span<const buffer_barrier> buffer_sources) const -> void
    {
        auto vk_texture_barriers = vector<VkImageMemoryBarrier2>{};
        auto vk_buffer_barriers = vector<VkBufferMemoryBarrier2>{};
        vk_texture_barriers.reserve(texture_sources.size());
        vk_buffer_barriers.reserve(buffer_sources.size());

        for (const auto& barrier : texture_sources)
        {
            vk_texture_barriers.emplace_back(as_vulkan(*_parent_device, barrier));
        }

        for (const auto& barrier : buffer_sources)
        {
            vk_buffer_barriers.emplace_back(as_vulkan(*_parent_device, barrier));
        }

        auto* const vk_event = _parent_device->get_event(event);
        auto dependency_info = VkDependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = static_cast<uint32_t>(vk_buffer_barriers.size()),
            .pBufferMemoryBarriers = vk_buffer_barriers.data(),
            .imageMemoryBarrierCount = static_cast<uint32_t>(vk_texture_barriers.size()),
            .pImageMemoryBarriers = vk_texture_barriers.data(),
        };

        _dispatch_table->cmdSetEvent2(_command_buffer, vk_event, &dependency_info);
    }

    auto command_list::wait_event(event_handle event, span<const texture_barrier> texture_destinations,
                                  span<const buffer_barrier> buffer_destinations) const -> void
    {
        auto vk_texture_barriers = vector<VkImageMemoryBarrier2>{};
        auto vk_buffer_barriers = vector<VkBufferMemoryBarrier2>{};
        vk_texture_barriers.reserve(texture_destinations.size());
        vk_buffer_barriers.reserve(buffer_destinations.size());

        for (const auto& barrier : texture_destinations)
        {
            vk_texture_barriers.emplace_back(as_vulkan(*_parent_device, barrier));
        }

        for (const auto& barrier : buffer_destinations)
        {
            vk_buffer_barriers.emplace_back(as_vulkan(*_parent_device, barrier));
        }

        auto* const vk_event = _parent_device->get_event(event);
        auto dependency_info = VkDependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = static_cast<uint32_t>(vk_buffer_barriers.size()),
            .pBufferMemoryBarriers = vk_buffer_barriers.data(),
            .imageMemoryBarrierCount = static_cast<uint32_t>(vk_texture_barriers.size()),
            .pImageMemoryBarriers = vk_texture_barriers.data(),
        };

        _dispatch_table->cmdWaitEvents2(_command_buffer, 1, &vk_event, &dependency_info);
    }

    auto command_list::reset_event(event_handle event, enum_mask<pipeline_stage> stages) const -> void
    {
        auto* const vk_event = _parent_device->get_event(event);
        _dispatch_table->cmdResetEvent2(_command_buffer, vk_event, as_vulkan(stages));
    }

    auto command_list::push_constants(enum_mask<shader_stage> stages, uint32_t offset, span<const byte> data) -> void
    {
        _dispatch_table->cmdPushConstants(_command_buffer, _parent_device->get_global_pipeline_layout(),
                                          VK_SHADER_STAGE_ALL, offset, static_cast<uint32_t>(data.size()), data.data());
    }

    auto command_list::begin_render_pass(span<const color_attachment> color_attachments,
                                         optional<depth_stencil_attachment> depth_stencil_attachment, uint32_t width,
                                         uint32_t height) -> void
    {
        auto color_attachment_descriptions = vector<VkRenderingAttachmentInfo>{};
        color_attachment_descriptions.reserve(color_attachments.size());

        for (const auto& attachment : color_attachments)
        {
            color_attachment_descriptions.emplace_back(VkRenderingAttachmentInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = _parent_device->get_texture_view(attachment.view)->handle,
                .imageLayout = static_cast<VkImageLayout>(VK_IMAGE_LAYOUT_GENERAL),
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = static_cast<VkAttachmentLoadOp>(attachment.load_op),
                .storeOp = static_cast<VkAttachmentStoreOp>(attachment.store_op),
                .clearValue = VkClearValue{.color =
                                               {
                                                   attachment.clear_value.r,
                                                   attachment.clear_value.g,
                                                   attachment.clear_value.b,
                                                   attachment.clear_value.a,
                                               }},
            });
        }

        auto depth_attachment_desc = optional<VkRenderingAttachmentInfo>{};
        auto stencil_attachment_desc = optional<VkRenderingAttachmentInfo>{};
        if (depth_stencil_attachment.has_value())
        {
            const auto& attachment = depth_stencil_attachment.value();
            const auto view_opt = _parent_device->get_texture_view(attachment.view);
            if (view_opt.has_value())
            {
                const auto& view = *view_opt;
                const auto fmt = view.format;
                const auto is_depth =
                    (fmt == VK_FORMAT_D16_UNORM || fmt == VK_FORMAT_D32_SFLOAT || fmt == VK_FORMAT_D24_UNORM_S8_UINT ||
                     fmt == VK_FORMAT_D32_SFLOAT_S8_UINT || fmt == VK_FORMAT_X8_D24_UNORM_PACK32);
                const auto is_stencil = (fmt == VK_FORMAT_D24_UNORM_S8_UINT || fmt == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                                         fmt == VK_FORMAT_S8_UINT || fmt == VK_FORMAT_D16_UNORM_S8_UINT);

                auto attach_info = VkRenderingAttachmentInfo{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext = nullptr,
                    .imageView = view.handle,
                    .imageLayout = static_cast<VkImageLayout>(VK_IMAGE_LAYOUT_GENERAL),
                    .resolveMode = VK_RESOLVE_MODE_NONE,
                    .resolveImageView = VK_NULL_HANDLE,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp = static_cast<VkAttachmentLoadOp>(attachment.depth_load_op),
                    .storeOp = static_cast<VkAttachmentStoreOp>(attachment.depth_store_op),
                    .clearValue = VkClearValue{.depthStencil =
                                                   {
                                                       .depth = attachment.clear_value.depth,
                                                       .stencil = attachment.clear_value.stencil,
                                                   }},
                };

                if (is_depth)
                {
                    depth_attachment_desc = attach_info;
                }
                if (is_stencil)
                {
                    stencil_attachment_desc = attach_info;
                }
            }
        }

        const auto rendering_info = VkRenderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea =
                {
                    .offset =
                        {
                            .x = 0,
                            .y = 0,
                        },
                    .extent =
                        {
                            .width = width,
                            .height = height,
                        },
                },
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = static_cast<uint32_t>(color_attachment_descriptions.size()),
            .pColorAttachments = color_attachment_descriptions.data(),
            .pDepthAttachment = depth_attachment_desc.has_value() ? &depth_attachment_desc.value() : nullptr,
            .pStencilAttachment = stencil_attachment_desc.has_value() ? &stencil_attachment_desc.value() : nullptr,
        };

        _dispatch_table->cmdBeginRendering(_command_buffer, &rendering_info);
    }

    auto command_list::end_render_pass() -> void
    {
        _dispatch_table->cmdEndRendering(_command_buffer);
    }

    auto command_list::bind_pipeline(graphics_pipeline_handle pipeline) -> void
    {
        _dispatch_table->cmdBindPipeline(_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                         _parent_device->get_graphics_pipeline(pipeline)->handle);
    }

    auto command_list::set_viewport(float min_x, float min_y, float width, float height, float min_depth,
                                    float max_depth) -> void
    {
        const auto viewport = VkViewport{
            .x = min_x,
            .y = min_y,
            .width = width,
            .height = height,
            .minDepth = min_depth,
            .maxDepth = max_depth,
        };

        _dispatch_table->cmdSetViewport(_command_buffer, 0, 1, &viewport);
    }

    auto command_list::set_scissor(int32_t min_x, int32_t min_y, uint32_t width, uint32_t height) -> void
    {
        const auto scissor = VkRect2D{
            .offset =
                {
                    .x = min_x,
                    .y = min_y,
                },
            .extent =
                {
                    .width = width,
                    .height = height,
                },
        };

        _dispatch_table->cmdSetScissor(_command_buffer, 0, 1, &scissor);
    }

    auto command_list::set_depth_bias(float constant_factor, float clamp, float slope_factor) -> void
    {
        _dispatch_table->cmdSetDepthBias(_command_buffer, constant_factor, clamp, slope_factor);
    }

    auto command_list::set_stencil_reference(uint32_t reference) -> void
    {
        _dispatch_table->cmdSetStencilReference(_command_buffer, VK_STENCIL_FRONT_AND_BACK, reference);
    }

    auto command_list::set_stencil_compare_mask(uint32_t compare_mask) -> void
    {
        _dispatch_table->cmdSetStencilCompareMask(_command_buffer, VK_STENCIL_FRONT_AND_BACK, compare_mask);
    }

    auto command_list::set_stencil_write_mask(uint32_t write_mask) -> void
    {
        _dispatch_table->cmdSetStencilWriteMask(_command_buffer, VK_STENCIL_FRONT_AND_BACK, write_mask);
    }

    auto command_list::bind_index_buffer(buffer_handle buffer, index_type type, uint64_t offset) -> void
    {
        _dispatch_table->cmdBindIndexBuffer(_command_buffer, _parent_device->get_buffer(buffer)->handle, offset,
                                            static_cast<VkIndexType>(type));
    }

    auto command_list::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex,
                            uint32_t first_instance) -> void
    {
        _dispatch_table->cmdDraw(_command_buffer, vertex_count, instance_count, first_vertex, first_instance);
    }

    auto command_list::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index,
                                    int32_t vertex_offset, uint32_t first_instance) -> void
    {
        _dispatch_table->cmdDrawIndexed(_command_buffer, index_count, instance_count, first_index, vertex_offset,
                                        first_instance);
    }

    auto command_list::draw_indirect(buffer_handle buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
        -> void
    {
        _dispatch_table->cmdDrawIndirect(_command_buffer, _parent_device->get_buffer(buffer)->handle, offset,
                                         draw_count, stride);
    }

    auto command_list::draw_indexed_indirect(buffer_handle buffer, uint64_t offset, uint32_t draw_count,
                                             uint32_t stride) -> void
    {
        _dispatch_table->cmdDrawIndexedIndirect(_command_buffer, _parent_device->get_buffer(buffer)->handle, offset,
                                                draw_count, stride);
    }

    auto command_list::draw_indirect_count(buffer_handle buffer, uint64_t offset, buffer_handle count_buffer,
                                           uint64_t count_buffer_offset, uint32_t max_draw_count, uint32_t stride)
        -> void
    {
        _dispatch_table->cmdDrawIndirectCount(_command_buffer, _parent_device->get_buffer(buffer)->handle, offset,
                                              _parent_device->get_buffer(count_buffer)->handle, count_buffer_offset,
                                              max_draw_count, stride);
    }

    auto command_list::draw_indexed_indirect_count(buffer_handle buffer, uint64_t offset, buffer_handle count_buffer,
                                                   uint64_t count_buffer_offset, uint32_t max_draw_count,
                                                   uint32_t stride) -> void
    {
        _dispatch_table->cmdDrawIndexedIndirectCount(_command_buffer, _parent_device->get_buffer(buffer)->handle,
                                                     offset, _parent_device->get_buffer(count_buffer)->handle,
                                                     count_buffer_offset, max_draw_count, stride);
    }

    auto command_list::bind_pipeline(compute_pipeline_handle pipeline) -> void
    {
        _dispatch_table->cmdBindPipeline(_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                         _parent_device->get_compute_pipeline(pipeline)->handle);
    }

    auto command_list::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) -> void
    {
        _dispatch_table->cmdDispatch(_command_buffer, group_count_x, group_count_y, group_count_z);
    }

    auto command_list::dispatch_indirect(buffer_handle buffer, uint64_t offset) -> void
    {
        _dispatch_table->cmdDispatchIndirect(_command_buffer, _parent_device->get_buffer(buffer)->handle, offset);
    }

    auto command_list::copy_buffer(buffer_handle src_buffer, buffer_handle dst_buffer,
                                   span<const buffer_copy_region> regions) -> void
    {
        auto vk_regions = vector<VkBufferCopy2>{};
        vk_regions.reserve(regions.size());

        for (const auto& region : regions)
        {
            vk_regions.emplace_back(VkBufferCopy2{
                .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext = nullptr,
                .srcOffset = region.src_offset,
                .dstOffset = region.dst_offset,
                .size = region.size,
            });
        }

        const auto copy_info = VkCopyBufferInfo2{
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext = nullptr,
            .srcBuffer = _parent_device->get_buffer(src_buffer)->handle,
            .dstBuffer = _parent_device->get_buffer(dst_buffer)->handle,
            .regionCount = static_cast<uint32_t>(vk_regions.size()),
            .pRegions = vk_regions.data(),
        };

        _dispatch_table->cmdCopyBuffer2(_command_buffer, &copy_info);
    }

    auto command_list::copy_buffer_to_texture(buffer_handle src_buffer, texture_handle dst_texture,
                                              span<const buffer_texture_copy_region> regions) -> void
    {
        auto vk_regions = vector<VkBufferImageCopy2>{};
        vk_regions.reserve(regions.size());

        for (const auto& region : regions)
        {
            vk_regions.emplace_back(VkBufferImageCopy2{
                .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .pNext = nullptr,
                .bufferOffset = region.buffer_offset,
                .bufferRowLength = region.buffer_row_length,
                .bufferImageHeight = region.buffer_image_height,
                .imageSubresource =
                    {
                        .aspectMask = infer_aspect_flags(_parent_device->get_texture(dst_texture)->format),
                        .mipLevel = region.mip_level,
                        .baseArrayLayer = region.base_array_layer,
                        .layerCount = region.array_layer_count,
                    },
                .imageOffset =
                    {
                        .x = region.image_offset_x,
                        .y = region.image_offset_y,
                        .z = region.image_offset_z,
                    },
                .imageExtent =
                    {
                        .width = region.image_extent_width,
                        .height = region.image_extent_height,
                        .depth = region.image_extent_depth,
                    },
            });
        }

        const auto copy_info = VkCopyBufferToImageInfo2{
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
            .pNext = nullptr,
            .srcBuffer = _parent_device->get_buffer(src_buffer)->handle,
            .dstImage = _parent_device->get_texture(dst_texture)->handle,
            .dstImageLayout = as_vulkan(image_layout::general),
            .regionCount = static_cast<uint32_t>(vk_regions.size()),
            .pRegions = vk_regions.data(),
        };

        _dispatch_table->cmdCopyBufferToImage2(_command_buffer, &copy_info);
    }

    auto command_list::copy_texture_to_buffer(texture_handle src_texture, buffer_handle dst_buffer,
                                              span<const buffer_texture_copy_region> regions) -> void
    {
        auto vk_regions = vector<VkBufferImageCopy2>{};
        vk_regions.reserve(regions.size());

        for (const auto& region : regions)
        {
            vk_regions.emplace_back(VkBufferImageCopy2{
                .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .pNext = nullptr,
                .bufferOffset = region.buffer_offset,
                .bufferRowLength = region.buffer_row_length,
                .bufferImageHeight = region.buffer_image_height,
                .imageSubresource =
                    {
                        .aspectMask = infer_aspect_flags(_parent_device->get_texture(src_texture)->format),
                        .mipLevel = region.mip_level,
                        .baseArrayLayer = region.base_array_layer,
                        .layerCount = region.array_layer_count,
                    },
                .imageOffset =
                    {
                        .x = region.image_offset_x,
                        .y = region.image_offset_y,
                        .z = region.image_offset_z,
                    },
                .imageExtent =
                    {
                        .width = region.image_extent_width,
                        .height = region.image_extent_height,
                        .depth = region.image_extent_depth,
                    },
            });
        }

        const auto copy_info = VkCopyImageToBufferInfo2{
            .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
            .pNext = nullptr,
            .srcImage = _parent_device->get_texture(src_texture)->handle,
            .srcImageLayout = as_vulkan(image_layout::general),
            .dstBuffer = _parent_device->get_buffer(dst_buffer)->handle,
            .regionCount = static_cast<uint32_t>(vk_regions.size()),
            .pRegions = vk_regions.data(),
        };

        _dispatch_table->cmdCopyImageToBuffer2(_command_buffer, &copy_info);
    }

    auto command_list::blit_texture(texture_handle src_texture, texture_handle dst_texture,
                                    span<const texture_blit_region> regions, filter_mode filter) -> void
    {
        const auto src_tex = _parent_device->get_texture(src_texture);
        const auto dst_tex = _parent_device->get_texture(dst_texture);
        if (!src_tex.has_value() || !dst_tex.has_value())
        {
            return;
        }

        auto vk_regions = vector<VkImageBlit2>{};
        vk_regions.reserve(regions.size());

        for (const auto& region : regions)
        {
            vk_regions.emplace_back(VkImageBlit2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                .pNext = nullptr,
                .srcSubresource =
                    {
                        .aspectMask = infer_aspect_flags(src_tex->format),
                        .mipLevel = region.src_subresource.mip_level,
                        .baseArrayLayer = region.src_subresource.base_array_layer,
                        .layerCount = region.src_subresource.array_layer_count,
                    },
                .srcOffsets =
                    {
                        VkOffset3D{
                            .x = region.src_offsets[0].x,
                            .y = region.src_offsets[0].y,
                            .z = region.src_offsets[0].z,
                        },
                        VkOffset3D{
                            .x = region.src_offsets[1].x,
                            .y = region.src_offsets[1].y,
                            .z = region.src_offsets[1].z,
                        },
                    },
                .dstSubresource =
                    {
                        .aspectMask = infer_aspect_flags(dst_tex->format),
                        .mipLevel = region.dst_subresource.mip_level,
                        .baseArrayLayer = region.dst_subresource.base_array_layer,
                        .layerCount = region.dst_subresource.array_layer_count,
                    },
                .dstOffsets =
                    {
                        VkOffset3D{
                            .x = region.dst_offsets[0].x,
                            .y = region.dst_offsets[0].y,
                            .z = region.dst_offsets[0].z,
                        },
                        VkOffset3D{
                            .x = region.dst_offsets[1].x,
                            .y = region.dst_offsets[1].y,
                            .z = region.dst_offsets[1].z,
                        },
                    },
            });
        }

        const auto blit_info = VkBlitImageInfo2{
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext = nullptr,
            .srcImage = src_tex->handle,
            .srcImageLayout = as_vulkan(image_layout::general),
            .dstImage = dst_tex->handle,
            .dstImageLayout = as_vulkan(image_layout::general),
            .regionCount = static_cast<uint32_t>(vk_regions.size()),
            .pRegions = vk_regions.data(),
            .filter = as_vulkan(filter),
        };

        _dispatch_table->cmdBlitImage2(_command_buffer, &blit_info);
    }

    auto command_list::write_timestamp(query_pool_handle pool, uint32_t query_index, pipeline_stage stage) -> void
    {
        const auto qp_opt = _parent_device->get_query_pool(pool);
        if (!qp_opt.has_value() || qp_opt->handle == VK_NULL_HANDLE)
        {
            return;
        }

        if (_dispatch_table->fp_vkCmdWriteTimestamp2 != nullptr)
        {
            const auto stage_flags2 = as_vulkan(stage);
            _dispatch_table->cmdWriteTimestamp2(_command_buffer, stage_flags2, qp_opt->handle, query_index);
        }
        else if (_dispatch_table->fp_vkCmdWriteTimestamp != nullptr)
        {
            auto stage_flag1 = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            if (stage == pipeline_stage::top_of_pipe)
            {
                stage_flag1 = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            }
            else if (stage == pipeline_stage::vertex)
            {
                stage_flag1 = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            }
            else if (stage == pipeline_stage::fragment)
            {
                stage_flag1 = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else if (stage == pipeline_stage::compute)
            {
                stage_flag1 = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }
            else if ((static_cast<uint32_t>(stage) & static_cast<uint32_t>(pipeline_stage::all_transfer)) != 0)
            {
                stage_flag1 = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            _dispatch_table->cmdWriteTimestamp(_command_buffer, stage_flag1, qp_opt->handle, query_index);
        }
    }

    auto command_list::begin_query(query_pool_handle pool, uint32_t query_index) -> void
    {
        const auto qp_opt = _parent_device->get_query_pool(pool);
        if (qp_opt.has_value() && qp_opt->handle != VK_NULL_HANDLE)
        {
            _dispatch_table->cmdBeginQuery(_command_buffer, qp_opt->handle, query_index, 0);
        }
    }

    auto command_list::end_query(query_pool_handle pool, uint32_t query_index) -> void
    {
        const auto qp_opt = _parent_device->get_query_pool(pool);
        if (qp_opt.has_value() && qp_opt->handle != VK_NULL_HANDLE)
        {
            _dispatch_table->cmdEndQuery(_command_buffer, qp_opt->handle, query_index);
        }
    }

    auto command_list::reset_query_pool(query_pool_handle pool, uint32_t first_query, uint32_t query_count) -> void
    {
        const auto qp_opt = _parent_device->get_query_pool(pool);
        if (qp_opt.has_value() && qp_opt->handle != VK_NULL_HANDLE)
        {
            _dispatch_table->cmdResetQueryPool(_command_buffer, qp_opt->handle, first_query, query_count);
        }
    }

    auto command_list::begin_debug_region([[maybe_unused]] const debug_label& label) -> void
    {
#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
        if (_dispatch_table->fp_vkCmdBeginDebugUtilsLabelEXT == nullptr || label.name.empty())
        {
            return;
        }

        const auto label_info = VkDebugUtilsLabelEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext = nullptr,
            .pLabelName = label.name.data(),
            .color = {label.color[0], label.color[1], label.color[2], label.color[3]},
        };
        _dispatch_table->cmdBeginDebugUtilsLabelEXT(_command_buffer, &label_info);
#endif
    }

    auto command_list::end_debug_region() -> void
    {
#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
        if (_dispatch_table->fp_vkCmdEndDebugUtilsLabelEXT == nullptr)
        {
            return;
        }

        _dispatch_table->cmdEndDebugUtilsLabelEXT(_command_buffer);
#endif
    }

    auto command_list::insert_debug_marker([[maybe_unused]] const debug_label& label) -> void
    {
#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
        if (_dispatch_table->fp_vkCmdInsertDebugUtilsLabelEXT == nullptr || label.name.empty())
        {
            return;
        }

        const auto label_info = VkDebugUtilsLabelEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext = nullptr,
            .pLabelName = label.name.data(),
            .color = {label.color[0], label.color[1], label.color[2], label.color[3]},
        };
        _dispatch_table->cmdInsertDebugUtilsLabelEXT(_command_buffer, &label_info);
#endif
    }

    execution_port::execution_port(vk::device& parent_device, uint32_t queue_family_index, vkb::QueueType queue_type,
                                   VkQueue queue, vkb::DispatchTable dispatch_table)
        : _parent_device{&parent_device}, _queue_family_index{queue_family_index}, _queue_type{queue_type},
          _queue{queue}, _dispatch_table{dispatch_table}
    {
        _timeline_semaphore = _parent_device->create_timeline_semaphore();

        if (_queue != VK_NULL_HANDLE)
        {
            if (queue_type == vkb::QueueType::graphics)
            {
                _parent_device->set_object_name(reinterpret_cast<uint64_t>(_queue), VK_OBJECT_TYPE_QUEUE,
                                                "Graphics Queue");
            }
            else if (queue_type == vkb::QueueType::compute)
            {
                _parent_device->set_object_name(reinterpret_cast<uint64_t>(_queue), VK_OBJECT_TYPE_QUEUE,
                                                "Async Compute Queue");
            }
            else if (queue_type == vkb::QueueType::transfer)
            {
                _parent_device->set_object_name(reinterpret_cast<uint64_t>(_queue), VK_OBJECT_TYPE_QUEUE,
                                                "Async Transfer Queue");
            }
        }
    }

    execution_port::~execution_port()
    {
        wait_idle();
        if (_timeline_semaphore.handle != 0)
        {
            _parent_device->destroy_semaphore(_timeline_semaphore);
            _timeline_semaphore = {};
        }
    }

    auto execution_port::wait_idle() -> void
    {
        auto lock = lock_guard{_submit_mutex};
        if (_queue != VK_NULL_HANDLE)
        {
            _dispatch_table.queueWaitIdle(_queue);
        }
    }

    auto execution_port::acquire_command_list(uint32_t thread_id, command_list_lifetime lifetime) -> rhi::command_list&
    {
        {
            auto lock = lock_guard{_slab_allocator_mutex};
            if (thread_id >= _slab_allocators.size())
            {
                while (_slab_allocators.size() <= thread_id)
                {
                    _slab_allocators.push_back(make_unique<combined_command_list_slab_allocator>(
                        _dispatch_table, *_parent_device, _queue_family_index, _queue_type));
                }
            }
        }

        auto* slab_alloc = _slab_allocators[thread_id].get();
        auto current_timeline_val = uint64_t{0};
        auto vk_sem = _parent_device->get_semaphore(_timeline_semaphore);
        if (vk_sem != VK_NULL_HANDLE)
        {
            _dispatch_table.getSemaphoreCounterValue(vk_sem, &current_timeline_val);
        }

        if (lifetime == command_list_lifetime::transient)
        {
            return slab_alloc->transient_allocator.acquire_command_list(current_timeline_val, vk_sem);
        }

        return slab_alloc->persistent_allocator.acquire_command_list(current_timeline_val, vk_sem);
    }

    auto execution_port::submit(span<const rhi::command_list*> commands, span<const device_sync_point> wait_semaphores,
                                span<const device_sync_point> signal_semaphores) -> expected<void, submit_error>
    {
        auto lock = lock_guard{_submit_mutex};

        ++_timeline_value;

        auto vk_cmd_infos = vector<VkCommandBufferSubmitInfo>{};
        vk_cmd_infos.reserve(commands.size());
        for (const auto* cmd : commands)
        {
            const auto* vk_cmd = static_cast<const command_list*>(cmd);
            vk_cmd_infos.emplace_back(VkCommandBufferSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext = nullptr,
                .commandBuffer = vk_cmd->get_handle(),
                .deviceMask = 0,
            });
        }

        auto vk_wait_infos = vector<VkSemaphoreSubmitInfo>{};
        vk_wait_infos.reserve(wait_semaphores.size());
        for (const auto& wait_sp : wait_semaphores)
        {
            auto vk_sem = _parent_device->get_semaphore(wait_sp.semaphore);
            vk_wait_infos.emplace_back(VkSemaphoreSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = vk_sem,
                .value = wait_sp.value,
                .stageMask = as_vulkan(wait_sp.stages),
                .deviceIndex = 0,
            });
        }

        auto vk_signal_infos = vector<VkSemaphoreSubmitInfo>{};
        vk_signal_infos.reserve(signal_semaphores.size() + 1);
        for (const auto& sig_sp : signal_semaphores)
        {
            auto vk_sem = _parent_device->get_semaphore(sig_sp.semaphore);
            vk_signal_infos.emplace_back(VkSemaphoreSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = vk_sem,
                .value = sig_sp.value,
                .stageMask = as_vulkan(sig_sp.stages),
                .deviceIndex = 0,
            });
        }

        // Always signal our internal timeline semaphore for slab tracking
        auto internal_vk_sem = _parent_device->get_semaphore(_timeline_semaphore);
        if (internal_vk_sem != VK_NULL_HANDLE)
        {
            vk_signal_infos.emplace_back(VkSemaphoreSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = internal_vk_sem,
                .value = _timeline_value,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .deviceIndex = 0,
            });
        }

        auto submit_info = VkSubmitInfo2{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = static_cast<uint32_t>(vk_wait_infos.size()),
            .pWaitSemaphoreInfos = vk_wait_infos.data(),
            .commandBufferInfoCount = static_cast<uint32_t>(vk_cmd_infos.size()),
            .pCommandBufferInfos = vk_cmd_infos.data(),
            .signalSemaphoreInfoCount = static_cast<uint32_t>(vk_signal_infos.size()),
            .pSignalSemaphoreInfos = vk_signal_infos.data(),
        };

        auto result = _dispatch_table.queueSubmit2(_queue, 1, &submit_info, VK_NULL_HANDLE);
        if (result != VK_SUCCESS)
        {
            switch (result)
            {
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                return unexpected{.value = submit_error::out_of_device_memory};
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                return unexpected{.value = submit_error::out_of_host_memory};
            case VK_ERROR_DEVICE_LOST:
                return unexpected{.value = submit_error::device_lost};
            default:
                return unexpected{.value = submit_error::unspecified};
            }
        }

        // Mark active slabs with this submission timeline value
        {
            auto slab_lock = lock_guard{_slab_allocator_mutex};
            for (auto& slab_alloc : _slab_allocators)
            {
                slab_alloc->transient_allocator.mark_current_slab_submitted(_timeline_value);
                slab_alloc->persistent_allocator.mark_current_slab_submitted(_timeline_value);
            }
        }

        return {};
    }

    auto execution_port::begin_debug_region([[maybe_unused]] const debug_label& label) -> void
    {
#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
        if (_dispatch_table.fp_vkQueueBeginDebugUtilsLabelEXT == nullptr || _queue == VK_NULL_HANDLE ||
            label.name.empty())
        {
            return;
        }

        const auto label_info = VkDebugUtilsLabelEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext = nullptr,
            .pLabelName = label.name.data(),
            .color = {label.color[0], label.color[1], label.color[2], label.color[3]},
        };
        _dispatch_table.queueBeginDebugUtilsLabelEXT(_queue, &label_info);
#endif
    }

    auto execution_port::end_debug_region() -> void
    {
#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
        if (_dispatch_table.fp_vkQueueEndDebugUtilsLabelEXT == nullptr || _queue == VK_NULL_HANDLE)
        {
            return;
        }

        _dispatch_table.queueEndDebugUtilsLabelEXT(_queue);
#endif
    }

    auto execution_port::insert_debug_marker([[maybe_unused]] const debug_label& label) -> void
    {
#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
        if (_dispatch_table.fp_vkQueueInsertDebugUtilsLabelEXT == nullptr || _queue == VK_NULL_HANDLE ||
            label.name.empty())
        {
            return;
        }

        const auto label_info = VkDebugUtilsLabelEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext = nullptr,
            .pLabelName = label.name.data(),
            .color = {label.color[0], label.color[1], label.color[2], label.color[3]},
        };
        _dispatch_table.queueInsertDebugUtilsLabelEXT(_queue, &label_info);
#endif
    }
} // namespace tempest::rhi::vk