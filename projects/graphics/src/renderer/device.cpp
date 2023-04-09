#include "device.hpp"

#include <tempest/logger.hpp>

#include <wyhash/wyhash.h>

#include <algorithm>

namespace tempest::graphics
{
    namespace
    {
        auto logger = tempest::logger::logger_factory::create({.prefix{"tempest::graphics::device"}});

        VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                      VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                      void* pUserData)
        {

            if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            {
                logger->error("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            {
                logger->warn("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            {
                logger->info("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            {
                logger->debug("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }
            else
            {
                logger->debug("Vulkan Validation Message: {}", pCallbackData->pMessage);
            }

            return VK_FALSE;
        }

        vkb::Instance create_instance(const gfx_device_create_info& info, VkAllocationCallbacks* alloc_callbacks)
        {
            vkb::InstanceBuilder bldr = vkb::InstanceBuilder{}
                                            .set_app_name("Tempest Engine Application")
                                            .set_app_version(0, 0, 1)
                                            .set_engine_name("Tempest Engine")
                                            .set_engine_version(0, 0, 1)
                                            .require_api_version(1, 2, 0)
                                            .set_allocation_callbacks(alloc_callbacks);

            if (info.enable_debug)
            {
                bldr.set_debug_callback(debug_callback)
                    .set_debug_messenger_severity(
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                    .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
                    .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
                    .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT)
                    .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT)
                    .enable_validation_layers();
                //.enable_layer("VK_LAYER_LUNARG_api_dump");
            }

            auto result = bldr.build();
            if (!result)
            {
                logger->error("Failed to create VkInstance.");
            }

            return *result;
        }

        vkb::PhysicalDevice select_physical_device(vkb::Instance& instance)
        {
            vkb::PhysicalDeviceSelector selector = vkb::PhysicalDeviceSelector{instance}
                                                       .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                                                       .defer_surface_initialization()
                                                       .add_desired_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
                                                       .require_present()
                                                       .set_minimum_version(1, 2)
                                                       .set_required_features({
                                                           .independentBlend{VK_TRUE},
                                                           .logicOp{VK_TRUE},
                                                           .depthClamp{VK_TRUE},
                                                           .depthBiasClamp{VK_TRUE},
                                                           .fillModeNonSolid{VK_TRUE},
                                                           .depthBounds{VK_TRUE},
                                                           .alphaToOne{VK_TRUE},
                                                           .shaderUniformBufferArrayDynamicIndexing{VK_TRUE},
                                                           .shaderSampledImageArrayDynamicIndexing{VK_TRUE},
                                                           .shaderStorageBufferArrayDynamicIndexing{VK_TRUE},
                                                           .shaderStorageImageArrayDynamicIndexing{VK_TRUE},
                                                       })
                                                       .set_required_features_12({
                                                           .drawIndirectCount{VK_TRUE},
                                                           .shaderUniformBufferArrayNonUniformIndexing{VK_TRUE},
                                                           .shaderSampledImageArrayNonUniformIndexing{VK_TRUE},
                                                           .shaderStorageBufferArrayNonUniformIndexing{VK_TRUE},
                                                           .shaderStorageImageArrayNonUniformIndexing{VK_TRUE},
                                                           .shaderUniformTexelBufferArrayNonUniformIndexing{VK_TRUE},
                                                           .shaderStorageTexelBufferArrayNonUniformIndexing{VK_TRUE},
                                                           .descriptorBindingSampledImageUpdateAfterBind{VK_TRUE},
                                                           .descriptorBindingStorageImageUpdateAfterBind{VK_TRUE},
                                                           .descriptorBindingPartiallyBound{VK_TRUE},
                                                           .descriptorBindingVariableDescriptorCount{VK_TRUE},
                                                           .imagelessFramebuffer{VK_TRUE},
                                                           .separateDepthStencilLayouts{VK_TRUE},
                                                           .bufferDeviceAddress{VK_TRUE},
                                                       });

            auto result = selector.select();
            if (!result)
            {
                logger->error("Failed to select suitable VkPhysicalDevice.");
            }

            return *result;
        }

        vkb::Device create_device(vkb::PhysicalDevice physical, VkAllocationCallbacks* alloc_callbacks)
        {
            vkb::DeviceBuilder bldr = vkb::DeviceBuilder{physical}.set_allocation_callbacks(alloc_callbacks);

            auto result = bldr.build();
            if (!result)
            {
                logger->error("Failed to create VkDevice.");
            }

            return *result;
        }

        window_info build_surface(vkb::Instance instance, vkb::Device device, glfw::window& win,
                                  VkAllocationCallbacks* alloc_calllbacks)
        {
            auto handle = win.raw();

            VkSurfaceKHR surface;
            auto surface_result =
                glfwCreateWindowSurface(instance.instance, handle, instance.allocation_callbacks, &surface);
            if (surface_result != VK_SUCCESS)
            {
                logger->error("Failed to create VkSurfaceKHR for window.");
                return {};
            }

            vkb::SwapchainBuilder bldr = vkb::SwapchainBuilder{device, surface}
                                             .set_allocation_callbacks(alloc_calllbacks)
                                             .set_required_min_image_count(2)
                                             .set_desired_format({.format{VK_FORMAT_R8G8B8A8_SRGB},
                                                                  .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}})
                                             .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR);
            auto swap_result = bldr.build();
            if (!swap_result)
            {
                logger->error("Failed to create VkSwapchainKHR for window.");
                return {};
            }

            VkImageViewUsageCreateInfo usage = {
                .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO},
                .usage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
            };

            const auto swapchain_images_result = swap_result->get_images();
            const auto swapchain_views_result = swap_result->get_image_views(&usage);

            if (!swapchain_views_result || !swapchain_images_result)
            {
                logger->error("Failed to create VkImageViews for VkSwapchainKHR attachments.");
                return {};
            }

            return window_info{
                .win{&win},
                .surface{surface},
                .swapchain{*swap_result},
                .images{*swapchain_images_result},
                .views{*swapchain_views_result},
            };
        }

        std::tuple<VkQueue, std::uint32_t> fetch_queue(vkb::Device dev, vkb::QueueType type)
        {
            auto queue_result = dev.get_queue(type);
            auto index_result = dev.get_queue_index(type);

            if (!queue_result || !index_result)
            {
                logger->error("Failed to fetch queue of type {0}", static_cast<std::uint32_t>(type));
            }

            return std::make_tuple(*queue_result, *index_result);
        }

        VmaAllocator create_allocator(vkb::Instance inst, vkb::PhysicalDevice physical, vkb::Device dev,
                                      VkAllocationCallbacks* alloc_callbacks)
        {
            VmaVulkanFunctions fns = {
                .vkGetInstanceProcAddr{inst.fp_vkGetInstanceProcAddr},
                .vkGetDeviceProcAddr{dev.fp_vkGetDeviceProcAddr},
            };

            VmaAllocatorCreateInfo ci = {
                .physicalDevice{physical.physical_device},
                .device{dev.device},
                .pAllocationCallbacks{alloc_callbacks},
                .pVulkanFunctions{&fns},
                .instance{inst.instance},
            };

            VmaAllocator allocator;
            const auto result = vmaCreateAllocator(&ci, &allocator);
            if (result != VK_SUCCESS)
            {
                logger->error("Failed to create VmaAllocator.");
                return nullptr;
            }

            return allocator;
        }

        void transition_image_layout(vkb::DispatchTable& dispatch, VkCommandBuffer buf, VkImage image, VkFormat fmt,
                                     VkImageLayout old_layout, VkImageLayout new_layout, VkImageAspectFlags aspect)
        {
            VkImageMemoryBarrier img_barrier = {
                .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                .pNext{nullptr},
                .oldLayout{old_layout},
                .newLayout{new_layout},
                .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
                .image{image},
                .subresourceRange{
                    .aspectMask{aspect},
                    .baseMipLevel{0},
                    .levelCount{1},
                    .baseArrayLayer{0},
                    .layerCount{1},
                },
            };

            VkPipelineStageFlags src_stage{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
            VkPipelineStageFlags dst_stage{VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

            if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                img_barrier.srcAccessMask = 0;
                img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                     new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            {
                img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else
            {
                logger->warn("Unexpected VkImageLayout transition from {0} to {1}",
                             static_cast<std::uint32_t>(old_layout), static_cast<std::uint32_t>(new_layout));
            }

            dispatch.cmdPipelineBarrier(buf, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);
        }
    } // namespace

    gfx_timestamp_manager::gfx_timestamp_manager(core::allocator* alloc, std::uint16_t query_per_frame,
                                                 std::uint16_t max_frames)
        : _alloc{alloc}, _queries_per_frame{query_per_frame}
    {
        const std::uint32_t data_per_query = 2; // start, end, 2x 64 bit integers
        const std::size_t allocated_size = sizeof(gfx_timestamp) * query_per_frame * max_frames +
                                           sizeof(std::uint64_t) * query_per_frame * max_frames * data_per_query;

        auto allocation = _alloc->allocate(allocated_size, 1);
        _timestamps = reinterpret_cast<gfx_timestamp*>(allocation);
        _timestamp_data = reinterpret_cast<std::uint64_t*>(reinterpret_cast<std::byte*>(allocation) +
                                                           sizeof(gfx_timestamp) * query_per_frame * max_frames);
        reset();
    }

    gfx_timestamp_manager::gfx_timestamp_manager(gfx_timestamp_manager&& other) noexcept
        : _alloc{other._alloc}, _timestamps{other._timestamps}, _timestamp_data{other._timestamp_data},
          _queries_per_frame{other._queries_per_frame}, _current_query{other._current_query},
          _parent_index{other._parent_index}, _depth{other._depth}
    {
        other._alloc = nullptr;
        other._timestamps = nullptr;
        other._timestamp_data = nullptr;
    }

    gfx_timestamp_manager::~gfx_timestamp_manager()
    {
        _release();
    }

    gfx_timestamp_manager& gfx_timestamp_manager::operator=(gfx_timestamp_manager&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release();

        std::swap(_alloc, rhs._alloc);
        std::swap(_timestamps, rhs._timestamps);
        std::swap(_timestamp_data, rhs._timestamp_data);

        return *this;
    }

    bool gfx_timestamp_manager::has_valid_queries() const noexcept
    {
        return _current_query > 0 && _depth == 0;
    }

    void gfx_timestamp_manager::reset()
    {
        _current_query = 0;
        _parent_index = 0;
        _is_current_frame_resolved = false;
        _depth = 0;
    }

    std::uint32_t gfx_timestamp_manager::resolve(std::uint32_t current_frame, gfx_timestamp* timestamps_to_fill)
    {
        memcpy_s(timestamps_to_fill, sizeof(gfx_timestamp) * _current_query,
                 &_timestamps[current_frame * _queries_per_frame], sizeof(gfx_timestamp) * _current_query);
        return _current_query;
    }

    std::uint32_t gfx_timestamp_manager::push(std::uint32_t current_frame, std::string_view name)
    {
        auto query_index = current_frame * _queries_per_frame + _current_query;
        gfx_timestamp& timestamp = _timestamps[query_index];
        timestamp.parent_index = static_cast<std::uint16_t>(_parent_index);
        timestamp.start = query_index * 2;
        timestamp.end = timestamp.start + 1;
        timestamp.name = name;
        timestamp.depth = static_cast<std::uint16_t>(_depth++);
        _parent_index = _current_query;
        ++_current_query;

        return query_index * 2;
    }

    std::uint32_t gfx_timestamp_manager::pop(std::uint32_t current_frame)
    {
        auto query_index = current_frame * _queries_per_frame + _current_query;
        gfx_timestamp& timestamp = _timestamps[query_index];
        _parent_index = timestamp.parent_index;
        --_depth;
        return query_index * 2 + 1;
    }

    std::uint32_t gfx_timestamp_manager::queries_per_frame() const noexcept
    {
        return _queries_per_frame;
    }

    void gfx_timestamp_manager::_release()
    {
        if (_timestamps)
        {
            _alloc->deallocate(_timestamps);
            _timestamps = nullptr; // sanitize allocations
            _timestamp_data = nullptr;
        }
    }

    gfx_device::gfx_device(const gfx_device_create_info& info)
        : _global_allocator{info.global_allocator}, _temporary_allocator{info.temp_allocator},
          _buffer_pool{_global_allocator, 512, sizeof(buffer)}, _texture_pool{_global_allocator, 512, sizeof(texture)},
          _shader_state_pool{_global_allocator, 128, sizeof(shader_state)},
          _pipeline_pool{_global_allocator, 128, sizeof(pipeline)}, _render_pass_pool{_global_allocator, 128,
                                                                                      sizeof(render_pass)},
          _descriptor_set_layout_pool{_global_allocator, 128, sizeof(descriptor_set_layout)}, _sampler_pool{
                                                                                                  _global_allocator, 32,
                                                                                                  sizeof(sampler)}
    {
        logger->debug("gfx_device creation started");

        if (_temporary_allocator == nullptr)
        {
            _temporary_allocator = new core::stack_allocator(64 * 1024);
        }

        _instance = create_instance(info, _alloc_callbacks);
        _physical_device = select_physical_device(_instance);
        _logical_device = create_device(_physical_device, _alloc_callbacks);
        _winfo = build_surface(_instance, _logical_device, *info.win, _alloc_callbacks);
        _dispatch = _logical_device.make_table();
        _vma_alloc = create_allocator(_instance, _physical_device, _logical_device, _alloc_callbacks);

        auto supported_extensions = _physical_device.get_extensions();
        _has_debug_utils_extension = std::find(supported_extensions.begin(), supported_extensions.end(),
                                               VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supported_extensions.end();

        _physical_device_properties = _physical_device.properties;

        auto [graphics_queue, graphics_queue_family] = fetch_queue(_logical_device, vkb::QueueType::graphics);
        auto [transfer_queue, transfer_queue_family] = fetch_queue(_logical_device, vkb::QueueType::transfer);
        auto [compute_queue, compute_queue_family] = fetch_queue(_logical_device, vkb::QueueType::compute);

        _graphics_queue = graphics_queue;
        _graphics_queue_family = graphics_queue_family;
        _transfer_queue = transfer_queue;
        _transfer_queue_family = transfer_queue_family;
        _compute_queue = compute_queue;
        _compute_queue_family = compute_queue_family;

        VkSemaphoreCreateInfo sem_ci = {
            .sType{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
        };

        VkFenceCreateInfo fence_ci = {
            .sType{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO},
            .pNext{nullptr},
            .flags{VK_FENCE_CREATE_SIGNALED_BIT},
        };

        for (std::size_t i = 0; i < frames_in_flight; ++i)
        {
            const auto present_ready_result = _dispatch.createSemaphore(&sem_ci, _alloc_callbacks, &_present_ready[i]);
            const auto render_complete_result =
                _dispatch.createSemaphore(&sem_ci, _alloc_callbacks, &_render_complete[i]);
            const auto cmd_buf_complete_result =
                _dispatch.createFence(&fence_ci, _alloc_callbacks, &_command_buffer_complete[i]);

            if (present_ready_result != VK_SUCCESS || render_complete_result != VK_SUCCESS ||
                cmd_buf_complete_result != VK_SUCCESS)
            {
                logger->error("Failed to create frame synchronization primitives.");
            }
        }

        {
            _dynamic_buffer_storage_per_frame = 1024 * 1024 * 10; // 10mb per frame

            buffer_create_info bci = {
                .type{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT},
                .usage{resource_usage::IMMUTABLE},
                .size{_dynamic_buffer_storage_per_frame * frames_in_flight},
                .name{"Persistent Device Dynamic Buffer"},
            };

            _global_dynamic_buffer = create_buffer(bci);
        }

        {
            _cmd_ring.emplace(this);
        }

        {
            _timestamps.emplace(_global_allocator, info.gpu_time_queries_per_frame,
                                static_cast<std::uint16_t>(_winfo.swapchain.image_count));
        }

        {
            VkQueryPoolCreateInfo ci = {
                .sType{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .queryType{VK_QUERY_TYPE_TIMESTAMP},
                .queryCount{info.gpu_time_queries_per_frame * 2 * frames_in_flight},
                .pipelineStatistics{0},
            };
            _dispatch.createQueryPool(&ci, _alloc_callbacks, &_timestamp_query_pool);
        }

        // Set up default depth buffer
        {
            std::fill_n(std::begin(_swapchain_attachment_info.color_formats),
                        _swapchain_attachment_info.color_formats.size(), VK_FORMAT_UNDEFINED);

            _swapchain_attachment_info.color_formats[0] = _winfo.swapchain.image_format;
            _swapchain_attachment_info.depth_stencil_format = VK_FORMAT_UNDEFINED;
            _swapchain_attachment_info.color_attachment_count = 1;
            _swapchain_attachment_info.color_load = _swapchain_attachment_info.depth_load =
                _swapchain_attachment_info.stencil_load = render_pass_attachment_operation::DONT_CARE;
        }

        // Set up swapchain render pass
        {
            render_pass_create_info ci = {
                .render_targets{1},
                .type{render_pass_type::SWAPCHAIN},
                .color_load{render_pass_attachment_operation::CLEAR},
                .depth_load{render_pass_attachment_operation::CLEAR},
                .stencil_load{render_pass_attachment_operation::CLEAR},
                .name{"Swapchain Resolve Pass"},
            };

            _swapchain_render_pass = create_render_pass(ci);
        }

        {
            _desc_pool.emplace(this);
        }

        {
            sampler_create_info sci = {
                .min_filter{VK_FILTER_LINEAR},
                .mag_filter{VK_FILTER_LINEAR},
                .mip_filter{VK_SAMPLER_MIPMAP_MODE_LINEAR},
                .u_address{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
                .v_address{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
                .w_address{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
                .name{"Default Sampler"},
            };

            _default_sampler = create_sampler(sci);
        }

        logger->debug("gfx_device creation completed");
    }

    gfx_device::~gfx_device()
    {
        logger->debug("gfx_device destruction started");

        _dispatch.deviceWaitIdle();

        release_sampler(_default_sampler);
        release_buffer(_global_dynamic_buffer);
        release_render_pass(_swapchain_render_pass);

        _cmd_ring = std::nullopt;

        _release_resources_imm();

        _desc_pool = std::nullopt;

        for (std::size_t i = 0; i < frames_in_flight; ++i)
        {
            _dispatch.destroySemaphore(_present_ready[i], _alloc_callbacks);
            _dispatch.destroySemaphore(_render_complete[i], _alloc_callbacks);
            _dispatch.destroyFence(_command_buffer_complete[i], _alloc_callbacks);
        }

        _winfo.swapchain.destroy_image_views(_winfo.views);

        vmaDestroyAllocator(_vma_alloc);
        vkb::destroy_swapchain(_winfo.swapchain);
        vkb::destroy_surface(_instance, _winfo.surface);
        vkb::destroy_device(_logical_device);
        vkb::destroy_instance(_instance);

        delete _temporary_allocator;

        logger->debug("gfx_device destruction completed");
    }

    void gfx_device::start_frame()
    {
        auto render_complete = _command_buffer_complete[_current_frame];
        auto wait_result = _dispatch.getFenceStatus(render_complete);
        if (wait_result != VK_SUCCESS)
        {
            _dispatch.waitForFences(1, &render_complete, VK_TRUE, UINT64_MAX);
        }

        _dispatch.resetFences(1, &render_complete);
        _cmd_ring->reset_pools(static_cast<std::uint32_t>(_current_frame));
    }

    void gfx_device::end_frame()
    {
        VkResult acquire_result =
            _dispatch.acquireNextImageKHR(_winfo.swapchain.swapchain, UINT64_MAX, _present_ready[_current_frame],
                                          VK_NULL_HANDLE, &_winfo.image_index);
        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            _recreate_swapchain();
            _advance_frame_counter();

            for (std::size_t i = 0; i < _queued_command_buffer_count; ++i)
            {
                _queued_commands_buffers[i].reset();
            }
            _queued_command_buffer_count = 0;

            return;
        }

        auto render_complete_fence = _command_buffer_complete[_current_frame];
        auto render_complete_sem = _render_complete[_current_frame];
        auto image_acquired_sem = _present_ready[_current_frame];
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        std::array<VkCommandBuffer, 8> cmds;
        for (std::size_t i = 0; i < _queued_command_buffer_count; ++i)
        {
            cmds[i] = _queued_commands_buffers[i];
        }

        VkSubmitInfo submit = {
            .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO},
            .pNext{nullptr},
            .waitSemaphoreCount{1},
            .pWaitSemaphores{&image_acquired_sem},
            .pWaitDstStageMask{wait_stages},
            .commandBufferCount{_queued_command_buffer_count},
            .pCommandBuffers{cmds.data()},
            .signalSemaphoreCount{1},
            .pSignalSemaphores{&render_complete_sem},
        };

        _dispatch.queueSubmit(_graphics_queue, 1, &submit, render_complete_fence);

        VkPresentInfoKHR present = {
            .sType{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR},
            .pNext{nullptr},
            .waitSemaphoreCount{1},
            .pWaitSemaphores{&render_complete_sem},
            .swapchainCount{1},
            .pSwapchains{&_winfo.swapchain.swapchain},
            .pImageIndices{&_winfo.image_index},
            .pResults{nullptr},
        };

        auto result = _dispatch.queuePresentKHR(_graphics_queue, &present);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            _recreate_swapchain();
            _advance_frame_counter();

            for (std::size_t i = 0; i < _queued_command_buffer_count; ++i)
            {
                _queued_commands_buffers[i].reset();
            }
            _queued_command_buffer_count = 0;

            return;
        }

        _queued_command_buffer_count = 0;

        _advance_frame_counter();

        _write_bindless_images();
    }

    buffer* gfx_device::access_buffer(buffer_handle handle)
    {
        return reinterpret_cast<buffer*>(_buffer_pool.access(handle.index));
    }

    const buffer* gfx_device::access_buffer(buffer_handle handle) const
    {
        return reinterpret_cast<const buffer*>(_buffer_pool.access(handle.index));
    }

    buffer_handle gfx_device::create_buffer(const buffer_create_info& ci)
    {
        buffer_handle handle{.index{_buffer_pool.acquire_resource()}};

        // early out check for failure to allocate handle
        if (handle.index == invalid_resource_handle) [[unlikely]]
        {
            return handle;
        }

        auto buf = access_buffer(handle);
        buf->name = ci.name;
        buf->size = ci.size;
        buf->buf_type = ci.type;
        buf->usage = ci.usage;
        buf->handle = handle;
        buf->global_offset = 0;
        buf->parent_buffer.index = invalid_resource_handle;

        static constexpr VkBufferUsageFlags dynamic_buffer_mask =
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        const bool uses_global_buf = (ci.type & dynamic_buffer_mask) != 0;

        // if we are dynamic and can use the global buffer, use it
        if (ci.usage == resource_usage::DYNAMIC && uses_global_buf)
        {
            buf->parent_buffer = _global_dynamic_buffer;
            return handle;
        }

        VkBufferCreateInfo vk_ci = {
            .sType{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO},
            .pNext{nullptr},
            .flags{},
            .size{std::max(ci.size, 1u)},
            .usage{VK_BUFFER_USAGE_TRANSFER_DST_BIT | ci.type},
        };

        VmaAllocationCreateInfo vma_ci = {
            .flags{VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT},
            .usage{VMA_MEMORY_USAGE_CPU_TO_GPU},
        };

        VmaAllocationInfo alloc_info = {};
        auto alloc_result =
            vmaCreateBuffer(_vma_alloc, &vk_ci, &vma_ci, &buf->underlying, &buf->allocation, &alloc_info);
        if (alloc_result != VK_SUCCESS)
        {
            logger->error("Failed to allocate VkBuffer and underlying memory.");
        }

        // should probably use a bit cast, but i like to live dangerously
        _set_resource_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<std::uint64_t>(buf->underlying), buf->name);
        buf->memory = alloc_info.deviceMemory;

        if (ci.initial_data.size() > 0)
        {
            std::byte* data;
            vmaMapMemory(_vma_alloc, buf->allocation, reinterpret_cast<void**>(&data));
            std::copy_n(ci.initial_data.data(), ci.initial_data.size(), data);
            vmaUnmapMemory(_vma_alloc, buf->allocation);
        }

        // TODO: handle persistence
        return handle;
    }

    void gfx_device::release_buffer(buffer_handle handle)
    {
        if (handle.index < _buffer_pool.size())
        {
            _deletion_queue.push_back(resource_update_desc{
                .type{resource_type::BUFFER},
                .handle{handle.index},
                .current_frame{static_cast<std::uint32_t>(_current_frame)},
            });
        }
    }

    shader_state* gfx_device::access_shader_state(shader_state_handle handle)
    {
        return reinterpret_cast<shader_state*>(_shader_state_pool.access(handle.index));
    }

    const shader_state* gfx_device::access_shader_state(shader_state_handle handle) const
    {
        return reinterpret_cast<const shader_state*>(_shader_state_pool.access(handle.index));
    }

    shader_state_handle gfx_device::create_shader_state(const shader_state_create_info& ci)
    {
        shader_state_handle handle{.index{invalid_resource_handle}};
        if (ci.stage_count == 0)
        {
            logger->warn("No provided shader stages.");
            return handle;
        }

        handle.index = _shader_state_pool.acquire_resource();
        if (handle.index == invalid_resource_handle)
        {
            logger->warn("Failed to allocate shader state handle.");
            return handle;
        }

        auto state = access_shader_state(handle);
        state->is_graphics = true;
        state->shader_count = ci.stage_count;

        auto tmp_marker = _temporary_allocator->get_marker();

        bool module_creation_failed = false;

        for (std::size_t i = 0; i < ci.stage_count; ++i)
        {
            auto stage = ci.stages[i];

            if (stage.shader_type == VK_SHADER_STAGE_COMPUTE_BIT)
            {
                state->is_graphics = false;
            }

            VkShaderModuleCreateInfo vk_module_ci = {
                .sType{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .codeSize{stage.byte_code.size()},
                .pCode{reinterpret_cast<std::uint32_t*>(stage.byte_code.data())},
            };

            VkPipelineShaderStageCreateInfo vk_stage_ci = {
                .sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .stage{stage.shader_type},
                .pName{"main"},
            };

            auto result = _dispatch.createShaderModule(&vk_module_ci, _alloc_callbacks, &vk_stage_ci.module);

            if (result != VK_SUCCESS)
            {
                logger->error("Failed to create shader module for stage {0} of shader {1}.",
                              static_cast<int>(stage.shader_type), ci.name);
                module_creation_failed = true;
                state->shader_count = static_cast<std::uint32_t>(i);
                break;
            }

            _set_resource_name(VK_OBJECT_TYPE_SHADER_MODULE, reinterpret_cast<std::uint64_t>(vk_stage_ci.module),
                               ci.name);
            state->stage_infos[i] = vk_stage_ci;
        }

        _temporary_allocator->free_marker(tmp_marker);

        if (!module_creation_failed)
        {
            state->name = ci.name;
        }
        else
        {
            release_shader_state(handle);
            handle.index = invalid_resource_handle;
        }

        return handle;
    }

    void gfx_device::release_shader_state(shader_state_handle handle)
    {
        if (handle.index < _shader_state_pool.size())
        {
            _deletion_queue.push_back(resource_update_desc{
                .type{resource_type::SHADER_STATE},
                .handle{handle.index},
                .current_frame{static_cast<std::uint32_t>(_current_frame)},
            });
        }
    }

    pipeline* gfx_device::access_pipeline(pipeline_handle handle)
    {
        return reinterpret_cast<pipeline*>(_pipeline_pool.access(handle.index));
    }

    const pipeline* gfx_device::access_pipeline(pipeline_handle handle) const
    {
        return reinterpret_cast<const pipeline*>(_pipeline_pool.access(handle.index));
    }

    pipeline_handle gfx_device::create_pipeline(const pipeline_create_info& ci)
    {
        pipeline_handle handle{.index{_pipeline_pool.acquire_resource()}};
        if (handle.index == invalid_resource_handle) [[unlikely]]
        {
            return handle;
        }

        shader_state_handle shader_data_handle = create_shader_state(ci.shaders);

        if (shader_data_handle.index == invalid_resource_handle)
        {
            _pipeline_pool.release_resource(handle);
            handle.index = invalid_resource_handle;
            return handle;
        }

        pipeline* pipeline_data = access_pipeline(handle);
        shader_state* shader_data = access_shader_state(shader_data_handle);

        pipeline_data->state = shader_data_handle; // use the correct shaders for the state

        std::array<VkDescriptorSetLayout, max_descriptors_per_set> vk_layouts;
        for (std::uint32_t i = 0; i < ci.active_desc_layouts; ++i)
        {
            pipeline_data->desc_set_layouts[i] = access_descriptor_set_layout(ci.desc_layouts[i]);
            pipeline_data->desc_set_layout_handles[i] = ci.desc_layouts[i];
            vk_layouts[i] = pipeline_data->desc_set_layouts[i]->layout;
        }

        VkPipelineLayoutCreateInfo pipeline_layout_ci = {
            .sType{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .setLayoutCount{ci.active_desc_layouts},
            .pSetLayouts{vk_layouts.data()},
            .pushConstantRangeCount{0},
            .pPushConstantRanges{nullptr},
        };

        VkPipelineLayout pipeline_layout;
        auto pipeline_layout_result =
            _dispatch.createPipelineLayout(&pipeline_layout_ci, _alloc_callbacks, &pipeline_layout);
        if (pipeline_layout_result != VK_SUCCESS)
        {
            logger->error("Failed to create VkPipelineLayout.");

            _pipeline_pool.release_resource(handle.index);
            release_shader_state(shader_data_handle);

            handle.index = invalid_resource_handle;

            return handle;
        }

        pipeline_data->layout = pipeline_layout;
        pipeline_data->num_active_layouts = ci.active_desc_layouts;

        if (shader_data->is_graphics)
        {
            std::array<VkVertexInputAttributeDescription, max_vertex_attributes> vertex_attribs;
            std::array<VkVertexInputBindingDescription, max_vertex_streams> vertex_bindings;

            for (std::size_t i = 0; i < ci.vertex_input.attribute_count; ++i)
            {
                auto& attr = ci.vertex_input.attributes[i];

                vertex_attribs[i] = {
                    .location{attr.location},
                    .binding{attr.binding},
                    .format{attr.fmt},
                    .offset{attr.offset},
                };
            }

            for (std::size_t i = 0; i < ci.vertex_input.stream_count; ++i)
            {
                auto& binding = ci.vertex_input.streams[i];

                vertex_bindings[i] = {
                    .binding{binding.binding},
                    .stride{binding.stride},
                    .inputRate{binding.input_rate},
                };
            }

            VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
                .sType{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .vertexBindingDescriptionCount{ci.vertex_input.stream_count},
                .pVertexBindingDescriptions{ci.vertex_input.stream_count == 0 ? nullptr : vertex_bindings.data()},
                .vertexAttributeDescriptionCount{ci.vertex_input.attribute_count},
                .pVertexAttributeDescriptions{ci.vertex_input.attribute_count == 0 ? nullptr : vertex_attribs.data()},
            };

            VkPipelineInputAssemblyStateCreateInfo assembly_ci = {
                .sType{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
                .primitiveRestartEnable{VK_FALSE},
            };

            std::array<VkPipelineColorBlendAttachmentState, max_framebuffer_attachments> color_blend_attachments;

            for (std::size_t i = 0; i < ci.blend.attachment_count; ++i)
            {
                auto& blend_state = ci.blend.blend_states[i];
                auto& attachment = color_blend_attachments[i];

                auto alpha_blend = blend_state.separate_blend ? blend_state.alpha : blend_state.rgb;

                attachment = {
                    .blendEnable{blend_state.blend_enabled ? VK_TRUE : VK_FALSE},
                    .srcColorBlendFactor{blend_state.rgb.source},
                    .dstColorBlendFactor{blend_state.rgb.destination},
                    .colorBlendOp{blend_state.rgb.operation},
                    .srcAlphaBlendFactor{alpha_blend.source},
                    .dstAlphaBlendFactor{alpha_blend.destination},
                    .alphaBlendOp{alpha_blend.operation},
                    .colorWriteMask{VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                    VK_COLOR_COMPONENT_A_BIT},
                };
            }

            VkPipelineColorBlendStateCreateInfo color_blend_ci = {
                .sType{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .logicOpEnable{VK_FALSE},
                .logicOp{VK_LOGIC_OP_COPY},
                .attachmentCount{ci.blend.attachment_count},
                .pAttachments{color_blend_attachments.data()},
                .blendConstants{0.0f, 0.0f, 0.0f, 0.0f},
            };

            VkPipelineDepthStencilStateCreateInfo depth_stencil_ci = {
                .sType{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .depthTestEnable{ci.ds.depth_test_enable},
                .depthWriteEnable{ci.ds.depth_write_enable ? VK_TRUE : VK_FALSE},
                .depthCompareOp{ci.ds.depth_comparison},
                .stencilTestEnable{ci.ds.stencil_op_enable ? VK_TRUE : VK_FALSE}, // TODO: Stencil operation
            };

            VkPipelineMultisampleStateCreateInfo mutlisample_ci = {
                .sType{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .rasterizationSamples{VK_SAMPLE_COUNT_1_BIT},
                .sampleShadingEnable{VK_FALSE},
                .minSampleShading{1.0f},
                .pSampleMask{nullptr},
                .alphaToCoverageEnable{VK_FALSE},
                .alphaToOneEnable{VK_FALSE},
            };

            VkPipelineRasterizationStateCreateInfo raster_ci = {
                .sType{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .depthClampEnable{VK_FALSE},
                .rasterizerDiscardEnable{VK_FALSE},
                .polygonMode{ci.raster.fill_mode},
                .cullMode{ci.raster.cull_mode},
                .frontFace{ci.raster.vertex_winding_order},
                .depthBiasConstantFactor{0.0f},
                .depthBiasClamp{0.0f},
                .depthBiasSlopeFactor{0.0f},
                .lineWidth{1.0f},
            };

            VkViewport viewport = {
                .x{0.0f},
                .y{0.0f},
                .width{static_cast<float>(_winfo.swapchain.extent.width)},
                .height{static_cast<float>(_winfo.swapchain.extent.height)},
                .minDepth{0.0f},
                .maxDepth{1.0f},
            };

            VkRect2D scissor = {
                .offset{
                    .x{0},
                    .y{0},
                },
                .extent{
                    .width{_winfo.swapchain.extent.width},
                    .height{_winfo.swapchain.extent.height},
                },
            };

            VkPipelineViewportStateCreateInfo viewport_ci = {
                .sType{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .viewportCount{1},
                .pViewports{&viewport},
                .scissorCount{1},
                .pScissors{&scissor},
            };

            VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

            VkPipelineDynamicStateCreateInfo dynamic_state_ci = {
                .sType{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .dynamicStateCount{sizeof(dyn_states) / sizeof(VkDynamicState)},
                .pDynamicStates{dyn_states},
            };

            VkGraphicsPipelineCreateInfo graphics_pipeline_ci = {
                .sType{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .stageCount{shader_data->shader_count},
                .pStages{shader_data->stage_infos.data()},
                .pVertexInputState{&vertex_input_ci},
                .pInputAssemblyState{&assembly_ci},
                .pTessellationState{nullptr},
                .pViewportState{&viewport_ci},
                .pRasterizationState{&raster_ci},
                .pMultisampleState{&mutlisample_ci},
                .pDepthStencilState{&depth_stencil_ci},
                .pColorBlendState{&color_blend_ci},
                .pDynamicState{&dynamic_state_ci},
                .layout{pipeline_layout},
                .renderPass{_fetch_vk_render_pass(ci.output, ci.name)},
                .subpass{0},
            };

            auto result = _dispatch.createGraphicsPipelines(nullptr, 1, &graphics_pipeline_ci, _alloc_callbacks,
                                                            &pipeline_data->pipeline);
            if (result != VK_SUCCESS)
            {
                logger->error("Failed to create VkPipeline: {0}", ci.name);
            }

            pipeline_data->kind = VK_PIPELINE_BIND_POINT_GRAPHICS;
        }
        else
        {
            logger->error("TODO: Implement compute pipeline.");
        }

        return handle;
    }

    void gfx_device::release_pipeline(pipeline_handle handle)
    {
        if (handle.index < _pipeline_pool.size())
        {
            _deletion_queue.push_back(resource_update_desc{
                .type{resource_type::PIPELINE},
                .handle{handle.index},
                .current_frame{static_cast<std::uint32_t>(_current_frame)},
            });
        }
    }

    texture* gfx_device::access_texture(texture_handle handle)
    {
        return reinterpret_cast<texture*>(_texture_pool.access(handle.index));
    }

    const texture* gfx_device::access_texture(texture_handle handle) const
    {
        return reinterpret_cast<const texture*>(_texture_pool.access(handle.index));
    }

    texture_handle gfx_device::create_texture(const texture_create_info& ci)
    {
        texture_handle handle{.index{_texture_pool.acquire_resource()}};
        if (handle.index == invalid_resource_handle)
        {
            return handle;
        }

        texture* tex = access_texture(handle);
        *tex = {
            .image_fmt{ci.image_format},
            .width{ci.width},
            .height{ci.height},
            .depth{ci.depth},
            .mipmaps{ci.mipmap_count},
            .flags{ci.flags},
            .handle{handle},
            .type{ci.image_type},
            .samp{nullptr},
            .name{ci.name},
        };

        VkImageCreateInfo img_ci = {
            .sType{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .imageType{to_vk_image_type(ci.image_type)},
            .format{tex->image_fmt},
            .extent{
                .width{tex->width},
                .height{tex->height},
                .depth{1},
            },
            .mipLevels{tex->mipmaps},
            .arrayLayers{1},
            .samples{VK_SAMPLE_COUNT_1_BIT},
            .tiling{VK_IMAGE_TILING_OPTIMAL},
            .initialLayout{VK_IMAGE_LAYOUT_UNDEFINED},
        };

        const bool is_render_target =
            static_cast<std::uint8_t>(ci.flags) & static_cast<std::uint8_t>(texture_flags::RENDER_TARGET);
        const bool is_compute_target =
            static_cast<std::uint8_t>(ci.flags) & static_cast<std::uint8_t>(texture_flags::COMPUTE_TARGET);

        img_ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        img_ci.usage |= is_compute_target ? VK_IMAGE_USAGE_STORAGE_BIT : 0;

        if (texture_format_utils::has_depth_or_stencil(ci.image_format))
        {
            img_ci.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        else
        {
            img_ci.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            img_ci.usage |= is_render_target ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
        }

        img_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_ci = {
            .usage{VMA_MEMORY_USAGE_GPU_ONLY},
        };

        auto image_result =
            vmaCreateImage(_vma_alloc, &img_ci, &alloc_ci, &tex->underlying_image, &tex->allocation, nullptr);
        if (image_result != VK_SUCCESS)
        {
            logger->error("Failed to create VkImage {0}", ci.name);
        }

        _set_resource_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(tex->underlying_image), ci.name);

        VkImageViewCreateInfo img_view_ci = {
            .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .image{tex->underlying_image},
            .viewType{to_vk_image_view_type(ci.image_type)},
            .format{ci.image_format},
            .subresourceRange{
                .baseMipLevel{0},
                .levelCount{1},
                .baseArrayLayer{0},
                .layerCount{1},
            },
        };

        if (!texture_format_utils::has_depth_or_stencil(ci.image_format))
        {
            img_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        else
        {
            if (texture_format_utils::has_depth(ci.image_format))
            {
                img_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            }
        }

        auto image_view_result = _dispatch.createImageView(&img_view_ci, _alloc_callbacks, &tex->underlying_view);
        if (image_view_result != VK_SUCCESS)
        {
            logger->error("Failed to create VkImageView {0}", ci.name);
        }

        // upload
        if (!ci.initial_payload.empty())
        {
            // create staging buffer
            VkBufferCreateInfo staging_buf_ci = {
                .sType{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO},
                .size{static_cast<std::uint64_t>(ci.width * ci.height) * 4}, // TODO: Compute BPP
                .usage{VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
            };

            VmaAllocationCreateInfo staging_buf_alloc_ci = {
                .flags{VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT},
                .usage{VMA_MEMORY_USAGE_CPU_TO_GPU},
            };

            VmaAllocationInfo alloc_info{};
            VkBuffer staging_buffer{};
            VmaAllocation staging_allocation;

            auto create_staging_result = vmaCreateBuffer(_vma_alloc, &staging_buf_ci, &staging_buf_alloc_ci,
                                                         &staging_buffer, &staging_allocation, &alloc_info);
            if (create_staging_result != VK_SUCCESS)
            {
                logger->error("Failed to create VkBuffer for staging operations on VkImage {0}", ci.name);
            }

            // copy over data
            void* dst_map_addr{nullptr};
            vmaMapMemory(_vma_alloc, staging_allocation, &dst_map_addr);
            memcpy_s(dst_map_addr, staging_buf_ci.size, ci.initial_payload.data(), ci.initial_payload.size_bytes());
            vmaUnmapMemory(_vma_alloc, staging_allocation);

            // copy commands
            auto& cmd_buffer = get_instant_command_buffer();
            cmd_buffer.begin();

            VkBufferImageCopy copy_region = {
                .bufferOffset{0},
                .bufferRowLength{0},
                .bufferImageHeight{0},
                .imageSubresource{
                    .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                    .mipLevel{0},
                    .baseArrayLayer{0},
                    .layerCount{1},
                },
                .imageOffset{
                    .x{0},
                    .y{0},
                    .z{0},
                },
                .imageExtent{
                    .width{tex->width},
                    .height{tex->height},
                    .depth{tex->depth},
                },
            };

            transition_image_layout(_dispatch, cmd_buffer, tex->underlying_image, tex->image_fmt,
                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT);
            _dispatch.cmdCopyBufferToImage(cmd_buffer, staging_buffer, tex->underlying_image,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
            transition_image_layout(_dispatch, cmd_buffer, tex->underlying_image, tex->image_fmt,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT);

            cmd_buffer.end();

            VkCommandBuffer vk_cmd = cmd_buffer;

            VkSubmitInfo submit = {
                .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO},
                .pNext{nullptr},
                .commandBufferCount{1},
                .pCommandBuffers{&vk_cmd},
            };

            _dispatch.queueSubmit(_graphics_queue, 1, &submit, VK_NULL_HANDLE);
            _dispatch.queueWaitIdle(_graphics_queue);

            vmaDestroyBuffer(_vma_alloc, staging_buffer, staging_allocation);
            _dispatch.resetCommandBuffer(vk_cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
            tex->image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        if (ci.bindless)
        {
            _texture_bindless_update_queue.push_back(resource_update_desc{
                .type{resource_type::TEXTURE},
                .handle{handle.index},
                .current_frame{static_cast<std::uint32_t>(_current_frame)},
            });
        }

        return handle;
    }

    void gfx_device::release_texture(texture_handle handle)
    {
        if (handle.index < _texture_pool.size())
        {
            _deletion_queue.push_back(resource_update_desc{
                .type{resource_type::TEXTURE},
                .handle{handle.index},
                .current_frame{static_cast<std::uint32_t>(_current_frame)},
            });
        }
    }

    sampler* gfx_device::access_sampler(sampler_handle handle)
    {
        return reinterpret_cast<sampler*>(_sampler_pool.access(handle.index));
    }

    const sampler* gfx_device::access_sampler(sampler_handle handle) const
    {
        return reinterpret_cast<const sampler*>(_sampler_pool.access(handle.index));
    }

    sampler_handle gfx_device::create_sampler(const sampler_create_info& ci)
    {
        sampler_handle handle{.index{_sampler_pool.acquire_resource()}};
        if (handle.index == invalid_resource_handle)
        {
            return handle;
        }

        sampler* smp = access_sampler(handle);
        *smp = {
            .min_filter{ci.min_filter},
            .mag_filter{ci.mag_filter},
            .mip_filter{ci.mip_filter},
            .u_address{ci.u_address},
            .v_address{ci.v_address},
            .w_address{ci.w_address},
            .name{ci.name},
        };

        VkSamplerCreateInfo vk_ci = {
            .sType{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .magFilter{ci.mag_filter},
            .minFilter{ci.min_filter},
            .mipmapMode{ci.mip_filter},
            .addressModeU{ci.u_address},
            .addressModeV{ci.v_address},
            .addressModeW{ci.w_address},
            .anisotropyEnable{VK_FALSE},
            .compareEnable{VK_FALSE},
            .borderColor{VK_BORDER_COLOR_INT_OPAQUE_WHITE},
            .unnormalizedCoordinates{VK_FALSE},
        };

        // TODO: Hnalde comparison, anisotropy, and lod bias
        auto result = _dispatch.createSampler(&vk_ci, _alloc_callbacks, &smp->underlying);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create VkSampler {}", ci.name);
        }
        _set_resource_name(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<std::uint64_t>(smp->underlying), ci.name);

        return handle;
    }

    void gfx_device::release_sampler(sampler_handle handle)
    {
        _deletion_queue.push_back(resource_update_desc{
            .type{resource_type::SAMPLER},
            .handle{handle.index},
            .current_frame{static_cast<std::uint32_t>(_current_frame)},
        });
    }

    descriptor_set_layout* gfx_device::access_descriptor_set_layout(descriptor_set_layout_handle handle)
    {
        return reinterpret_cast<descriptor_set_layout*>(_descriptor_set_layout_pool.access(handle.index));
    }

    const descriptor_set_layout* gfx_device::access_descriptor_set_layout(descriptor_set_layout_handle handle) const
    {
        return reinterpret_cast<const descriptor_set_layout*>(_descriptor_set_layout_pool.access(handle.index));
    }

    descriptor_set_layout_handle gfx_device::create_descriptor_set_layout(const descriptor_set_layout_create_info& ci)
    {
        descriptor_set_layout_handle handle{.index{_descriptor_set_layout_pool.acquire_resource()}};

        if (handle.index == invalid_resource_handle) [[unlikely]]
        {
            return handle;
        }

        descriptor_set_layout* layout = access_descriptor_set_layout(handle);
        std::size_t alloc_size = (sizeof(VkDescriptorSetLayoutBinding) + sizeof(descriptor_binding)) * ci.binding_count;
        std::byte* memory = reinterpret_cast<std::byte*>(_global_allocator->allocate(alloc_size, 1));
        layout->bindings = reinterpret_cast<descriptor_binding*>(memory);
        layout->vk_binding =
            reinterpret_cast<VkDescriptorSetLayoutBinding*>(memory + sizeof(descriptor_binding) * ci.binding_count);
        layout->handle = handle;
        layout->set_index = static_cast<std::uint16_t>(ci.set_index);

        std::uint32_t used_binding_count{0};
        for (std::uint32_t r = 0; r < ci.binding_count; ++r)
        {
            descriptor_binding& binding = layout->bindings[r];
            const descriptor_set_layout_create_info::binding& input = ci.bindings[r];
            binding.start = input.start_binding == std::numeric_limits<std::uint16_t>::max()
                                ? static_cast<std::uint16_t>(r)
                                : input.start_binding;
            binding.count = 1;
            binding.type = input.type;
            binding.name = input.name;

            VkDescriptorSetLayoutBinding& vk_binding = layout->vk_binding[used_binding_count];
            ++used_binding_count;

            vk_binding.binding = binding.start;
            vk_binding.descriptorType = binding.type;
            vk_binding.descriptorCount = binding.count;
            vk_binding.stageFlags = VK_SHADER_STAGE_ALL;
            vk_binding.pImmutableSamplers = nullptr;
        }

        layout->num_bindings = used_binding_count;

        VkDescriptorSetLayoutCreateInfo vk_ci = {
            .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .bindingCount{used_binding_count},
            .pBindings{layout->vk_binding},
        };

        auto result = _dispatch.createDescriptorSetLayout(&vk_ci, _alloc_callbacks, &layout->layout);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create VkDescriptorSetLayout {0}", ci.name);
        }
        _set_resource_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<std::uint64_t>(layout->layout),
                           ci.name);

        return handle;
    }

    void gfx_device::release_descriptor_set_layout(descriptor_set_layout_handle handle)
    {
        if (handle.index < _descriptor_set_layout_pool.size())
        {
            _deletion_queue.push_back(resource_update_desc{
                .type{resource_type::DESCRIPTOR_SET_LAYOUT},
                .handle{handle.index},
                .current_frame{static_cast<std::uint32_t>(_current_frame)},
            });
        }
    }

    descriptor_set* gfx_device::access_descriptor_set(descriptor_set_handle handle)
    {
        return _desc_pool->access(handle);
    }

    const descriptor_set* gfx_device::access_descriptor_set(descriptor_set_handle handle) const
    {
        return _desc_pool->access(handle);
    }

    descriptor_set_handle gfx_device::create_descriptor_set(const descriptor_set_create_info& ci)
    {
        return _desc_pool->create(ci);
    }

    descriptor_set_handle gfx_device::create_descriptor_set(const descriptor_set_builder& bldr)
    {
        return bldr.build(*_desc_pool);
    }

    void gfx_device::release_descriptor_set(descriptor_set_handle handle)
    {
        _deletion_queue.push_back(resource_update_desc{
            .type{resource_type::DESCRIPTOR_SET},
            .handle{handle.index},
            .current_frame{static_cast<std::uint32_t>(_current_frame)},
        });
    }

    render_pass* gfx_device::access_render_pass(render_pass_handle handle)
    {
        return reinterpret_cast<render_pass*>(_render_pass_pool.access(handle.index));
    }

    const render_pass* gfx_device::access_render_pass(render_pass_handle handle) const
    {
        return reinterpret_cast<const render_pass*>(_render_pass_pool.access(handle.index));
    }

    render_pass_handle gfx_device::create_render_pass(const render_pass_create_info& ci)
    {
        render_pass_handle handle{.index{_render_pass_pool.acquire_resource()}};
        if (handle.index == invalid_resource_handle)
        {
            return handle;
        }

        render_pass* pass = access_render_pass(handle);
        *pass = {
            .pass{nullptr},
            .target{nullptr},
            .type{ci.type},
            .scale_x{ci.scale_x},
            .scale_y{ci.scale_y},
            .dispatch_x{0},
            .dispatch_y{0},
            .dispatch_z{0},
            .resize{ci.resize},
            .num_render_targets{static_cast<std::uint8_t>(ci.render_targets)},
            .name{ci.name},
        };

        // fetch and cache the texture target handles to build the framebuffer
        std::uint32_t color_target_count{0};
        for (; color_target_count < ci.render_targets; ++color_target_count)
        {
            texture* tex = access_texture(ci.color_outputs[color_target_count]);
            pass->width = tex->width;
            pass->height = tex->height;
            pass->output_color_textures[color_target_count] = ci.color_outputs[color_target_count];
        }

        pass->output_depth_attachment = ci.depth_stencil_texture;

        switch (ci.type)
        {
        case render_pass_type::RASTERIZATION: {
            pass->output = _fill_render_pass_attachment_info(ci);
            pass->pass = _fetch_vk_render_pass(pass->output, ci.name);
            _create_framebuffer(pass,
                                std::span<texture_handle>(pass->output_color_textures.data(), pass->num_render_targets),
                                ci.depth_stencil_texture);
            break;
        }
        case render_pass_type::SWAPCHAIN: {
            _create_swapchain_pass(ci, pass);
            break;
        }
        case render_pass_type::COMPUTE: {
            logger->error("TODO: Implement compute pass construction.");
            break;
        }
        default:
            break;
        }

        return handle;
    }

    void gfx_device::release_render_pass(render_pass_handle handle)
    {
        if (handle.index < _render_pass_pool.size())
        {
            _deletion_queue.push_back(resource_update_desc{
                .type{resource_type::RENDER_PASS},
                .handle{handle.index},
                .current_frame{static_cast<std::uint32_t>(_current_frame)},
            });
        }
    }

    command_buffer& gfx_device::get_command_buffer(queue_type type, bool begin)
    {
        auto& cb = _cmd_ring->fetch_buffer(static_cast<std::uint32_t>(_current_frame));

        if (begin)
        {
            cb.begin();
        }

        if (_gpu_timestamp_reset && begin)
        {
            // VkCommandBuffer buf = cb;
            //_dispatch.cmdResetQueryPool(buf, _timestamp_query_pool,
            //                            static_cast<std::uint32_t>(_current_frame) * _timestamps->queries_per_frame()
            //                            *
            //                                 2,
            //                             _timestamps->queries_per_frame());
            //_gpu_timestamp_reset = false;
        }

        return cb;
    }

    command_buffer& gfx_device::get_instant_command_buffer()
    {
        return _cmd_ring->fetch_buffer(static_cast<std::uint32_t>(_current_frame));
    }

    void gfx_device::queue_command_buffer(const command_buffer& buffer)
    {
        _queued_commands_buffers[_queued_command_buffer_count++] = buffer;
    }

    void gfx_device::execute_immediate(const command_buffer& buffer)
    {
        VkCommandBuffer vk_buf = buffer;

        VkSubmitInfo submit = {
            .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO},
            .pNext{nullptr},
            .commandBufferCount{1},
            .pCommandBuffers{&vk_buf},
        };

        _dispatch.queueSubmit(_graphics_queue, 1, &submit, VK_NULL_HANDLE);
        _dispatch.queueWaitIdle(_graphics_queue);
    }

    void gfx_device::_advance_frame_counter() noexcept
    {
        _previous_frame = _current_frame;
        _current_frame = (_current_frame + 1) % frames_in_flight;
        ++_absolute_frame;
    }

    void gfx_device::_set_resource_name(VkObjectType type, std::uint64_t handle, std::string_view name)
    {
        if (!_has_debug_utils_extension)
        {
            return;
        }

        VkDebugUtilsObjectNameInfoEXT name_info = {
            .sType{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT},
            .pNext{nullptr},
            .objectType{type},
            .objectHandle{handle},
            .pObjectName{name.data()},
        };
        _dispatch.setDebugUtilsObjectNameEXT(&name_info);
    }

    void gfx_device::_release_resources_imm()
    {
        for (auto desc : _deletion_queue)
        {
            if (desc.current_frame == -1)
            {
                continue;
            }

            switch (desc.type)
            {
            case resource_type::BUFFER: {
                _destroy_buffer_imm(desc.handle);
                break;
            }
            case resource_type::DESCRIPTOR_SET: {
                _destroy_desc_set_imm(desc.handle);
                break;
            }
            case resource_type::DESCRIPTOR_SET_LAYOUT: {
                _destroy_desc_set_layout_imm(desc.handle);
                break;
            }
            case resource_type::PIPELINE: {
                _destroy_pipeline_imm(desc.handle);
                break;
            }
            case resource_type::RENDER_PASS: {
                _destroy_render_pass_imm(desc.handle);
                break;
            }
            case resource_type::SAMPLER: {
                _destroy_sampler_imm(desc.handle);
                break;
            }
            case resource_type::SHADER_STATE: {
                _destroy_shader_state_imm(desc.handle);
                break;
            }
            case resource_type::TEXTURE: {
                _destroy_texture_imm(desc.handle);
                break;
            }
            default: {
                logger->warn("Deletion not implemented for provided resource type: {0}", static_cast<int>(desc.type));
                break;
            }
            }
        }

        for (auto& [hash, pass] : _render_pass_cache)
        {
            _dispatch.destroyRenderPass(pass, _alloc_callbacks);
        }

        _dispatch.destroyRenderPass(access_render_pass(_swapchain_render_pass)->pass, _alloc_callbacks);
        for (auto& img : _winfo.swapchain_targets)
        {
            _dispatch.destroyFramebuffer(img, _alloc_callbacks);
        }

        _dispatch.destroyQueryPool(_timestamp_query_pool, _alloc_callbacks);
    }

    void gfx_device::_destroy_buffer_imm(resource_handle hnd)
    {
        auto buffer = access_buffer(buffer_handle{hnd});
        if (buffer)
        {
            vmaDestroyBuffer(_vma_alloc, buffer->underlying, buffer->allocation);
        }
        _buffer_pool.release_resource(hnd);
    }

    void gfx_device::_destroy_desc_set_layout_imm(resource_handle hnd)
    {
        auto layout = access_descriptor_set_layout(descriptor_set_layout_handle{hnd});
        if (layout)
        {
            _global_allocator->deallocate(layout->bindings);
            _dispatch.destroyDescriptorSetLayout(layout->layout, _alloc_callbacks);
        }
        _descriptor_set_layout_pool.release_resource(hnd);
    }

    void gfx_device::_destroy_texture_imm(resource_handle hnd)
    {
        auto texture = access_texture(texture_handle{hnd});
        if (texture)
        {
            _dispatch.destroyImageView(texture->underlying_view, _alloc_callbacks);
            vmaDestroyImage(_vma_alloc, texture->underlying_image, texture->allocation);
        }
        _texture_pool.release_resource(hnd);
    }

    void gfx_device::_destroy_shader_state_imm(resource_handle hnd)
    {
        shader_state* state = access_shader_state(shader_state_handle{hnd});
        if (state)
        {
            for (std::size_t i = 0; i < state->shader_count; ++i)
            {
                _dispatch.destroyShaderModule(state->stage_infos[i].module, _alloc_callbacks);
            }
        }
        _shader_state_pool.release_resource(hnd);
    }

    void gfx_device::_destroy_pipeline_imm(resource_handle hnd)
    {
        pipeline* pipe = access_pipeline(pipeline_handle{hnd});
        if (pipe)
        {
            _destroy_shader_state_imm(pipe->state);
            _dispatch.destroyPipeline(pipe->pipeline, _alloc_callbacks);
            _dispatch.destroyPipelineLayout(pipe->layout, _alloc_callbacks);
        }
        _pipeline_pool.release_resource(hnd);
    }

    void gfx_device::_destroy_render_pass_imm(resource_handle hnd)
    {
        render_pass* pass = access_render_pass(render_pass_handle{hnd});
        if (pass)
        {
            if (pass->num_render_targets > 0)
            {
                _dispatch.destroyFramebuffer(pass->target, _alloc_callbacks);
            }
        }
        _render_pass_pool.release_resource(hnd);
    }

    void gfx_device::_destroy_desc_set_imm(resource_handle hnd)
    {
        _desc_pool->release({.index{hnd}});
    }

    void gfx_device::_destroy_sampler_imm(resource_handle hnd)
    {
        sampler* smp = access_sampler(sampler_handle{.index{hnd}});
        if (smp)
        {
            _dispatch.destroySampler(smp->underlying, _alloc_callbacks);
        }

        _sampler_pool.release_resource(hnd);
    }

    VkRenderPass gfx_device::_fetch_vk_render_pass(const render_pass_attachment_info& out, std::string_view name)
    {
        auto hashed = wyhash(&out, sizeof(render_pass_attachment_info), 0, _wyp);
        auto render_pass_it = _render_pass_cache.find(hashed);
        if (render_pass_it != _render_pass_cache.end())
        {
            return render_pass_it->second;
        }

        auto pass = _create_vk_render_pass(out, name);
        _render_pass_cache[hashed] = pass;
        return pass;
    }

    VkRenderPass gfx_device::_create_vk_render_pass(const render_pass_attachment_info& out, std::string_view name)
    {
        std::array<VkAttachmentDescription, max_framebuffer_attachments> color_attachments;
        std::array<VkAttachmentReference, max_framebuffer_attachments> color_attachment_refs;
        VkAttachmentLoadOp color_op, depth_op{}, stencil_op{};
        VkImageLayout color_initial, depth_initial{};

        switch (out.color_load)
        {
        case render_pass_attachment_operation::LOAD: {
            color_op = VK_ATTACHMENT_LOAD_OP_LOAD;
            color_initial = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        }
        case render_pass_attachment_operation::CLEAR: {
            color_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_initial = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        }
        case render_pass_attachment_operation::DONT_CARE: {
            color_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color_initial = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        }
        default:
            break;
        }

        switch (out.depth_load)
        {
        case render_pass_attachment_operation::LOAD: {
            depth_op = VK_ATTACHMENT_LOAD_OP_LOAD;
            depth_initial = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        }
        case render_pass_attachment_operation::CLEAR: {
            depth_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_initial = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        }
        case render_pass_attachment_operation::DONT_CARE: {
            depth_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depth_initial = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        }
        default:
            break;
        }

        switch (out.stencil_load)
        {
        case render_pass_attachment_operation::LOAD: {
            stencil_op = VK_ATTACHMENT_LOAD_OP_LOAD;
            break;
        }
        case render_pass_attachment_operation::CLEAR: {
            stencil_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        }
        case render_pass_attachment_operation::DONT_CARE: {
            stencil_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;
        }
        default:
            break;
        }

        std::uint32_t attachment_index = 0;

        for (; attachment_index < out.color_attachment_count; ++attachment_index)
        {
            color_attachments[attachment_index] = {
                .format{out.color_formats[attachment_index]},
                .samples{VK_SAMPLE_COUNT_1_BIT},
                .loadOp{color_op},
                .storeOp{VK_ATTACHMENT_STORE_OP_STORE},
                .stencilLoadOp{stencil_op},
                .stencilStoreOp{VK_ATTACHMENT_STORE_OP_DONT_CARE},
                .initialLayout{VK_IMAGE_LAYOUT_UNDEFINED},
                .finalLayout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            };

            color_attachment_refs[attachment_index] = {
                .attachment{attachment_index},
                .layout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            };
        }

        VkAttachmentDescription depth_attachment{};
        VkAttachmentReference depth_reference{};
        // check if we have a depth stencil attachment
        if (out.depth_stencil_format != VK_FORMAT_UNDEFINED)
        {
            depth_attachment = {
                .format{out.depth_stencil_format},
                .samples{VK_SAMPLE_COUNT_1_BIT},
                .loadOp{depth_op},
                .storeOp{VK_ATTACHMENT_STORE_OP_STORE},
                .stencilLoadOp{stencil_op},
                .stencilStoreOp{VK_ATTACHMENT_STORE_OP_DONT_CARE},
                .initialLayout{depth_initial},
                .finalLayout{VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
            };

            depth_reference = {
                .attachment{attachment_index},
                .layout{VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
            };
        }

        // TODO: multisubpass render passes
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        std::array<VkAttachmentDescription, max_framebuffer_attachments + 1> image_attachments;
        std::copy_n(std::begin(color_attachments), out.color_attachment_count, std::begin(image_attachments));

        subpass.colorAttachmentCount = out.color_attachment_count ? out.color_attachment_count : 0;
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = nullptr;

        std::uint32_t depth_stencil_count = 0;
        if (out.depth_stencil_format != VK_FORMAT_UNDEFINED)
        {
            image_attachments[subpass.colorAttachmentCount] = depth_attachment;
            subpass.pDepthStencilAttachment = &depth_reference;
            depth_stencil_count = 1;
        }

        VkRenderPassCreateInfo render_pass_info = {
            .sType{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .attachmentCount{out.color_attachment_count + depth_stencil_count},
            .pAttachments{image_attachments.data()},
            .subpassCount{1},
            .pSubpasses{&subpass},
        };

        VkRenderPass rp{VK_NULL_HANDLE};
        auto result = _dispatch.createRenderPass(&render_pass_info, _alloc_callbacks, &rp);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create VkRenderPass.");
            return rp;
        }

        _set_resource_name(VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<std::uint64_t>(rp), name);
        return rp;
    }

    void gfx_device::_create_swapchain_pass(const render_pass_create_info& ci, render_pass* pass)
    {
        VkAttachmentDescription swapchain_attachment = {
            .format{_winfo.swapchain.image_format},
            .samples{VK_SAMPLE_COUNT_1_BIT},
            .loadOp{VK_ATTACHMENT_LOAD_OP_CLEAR},
            .storeOp{VK_ATTACHMENT_STORE_OP_STORE},
            .stencilLoadOp{VK_ATTACHMENT_LOAD_OP_DONT_CARE},
            .stencilStoreOp{VK_ATTACHMENT_STORE_OP_DONT_CARE},
            .initialLayout{VK_IMAGE_LAYOUT_UNDEFINED},
            .finalLayout{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
        };

        VkAttachmentReference swapchain_image_ref = {
            .attachment{0},
            .layout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        };

        VkSubpassDescription blit_pass = {
            .flags{0},
            .pipelineBindPoint{VK_PIPELINE_BIND_POINT_GRAPHICS},
            .inputAttachmentCount{0},
            .pInputAttachments{nullptr},
            .colorAttachmentCount{1},
            .pColorAttachments{&swapchain_image_ref},
            .pResolveAttachments{nullptr},
            .pDepthStencilAttachment{nullptr},
            .preserveAttachmentCount{0},
            .pPreserveAttachments{nullptr},
        };

        VkAttachmentDescription attachments[] = {swapchain_attachment};

        VkRenderPassCreateInfo vk_rp_ci = {
            .sType{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .attachmentCount{1},
            .pAttachments{attachments},
            .subpassCount{1},
            .pSubpasses{&blit_pass},
            .dependencyCount{0},
            .pDependencies{nullptr},
        };

        auto rp_result = _dispatch.createRenderPass(&vk_rp_ci, _alloc_callbacks, &pass->pass);
        if (rp_result != VK_SUCCESS)
        {
            logger->error("Failed to create VkRenderPass.");
            return;
        }
        _set_resource_name(VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<std::uint64_t>(pass->pass), ci.name);

        VkFramebufferCreateInfo vk_fb_ci = {
            .sType{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .renderPass{pass->pass},
            .attachmentCount{1},
            .width{_winfo.swapchain.extent.width},
            .height{_winfo.swapchain.extent.height},
            .layers{1},
        };

        // 2 attachments, 1 swapchain image, 1 depth buffer image
        VkImageView fb_attachments[1];
        _winfo.swapchain_targets.resize(_winfo.swapchain.image_count);

        for (std::size_t i = 0; i < _winfo.swapchain.image_count; ++i)
        {
            fb_attachments[0] = _winfo.views[i];
            vk_fb_ci.pAttachments = fb_attachments;

            auto fb_result = _dispatch.createFramebuffer(&vk_fb_ci, _alloc_callbacks, &_winfo.swapchain_targets[i]);
            if (fb_result != VK_SUCCESS)
            {
                logger->error("Failed to create VkFramebuffer for swapchain pass.");
                return;
            }
        }

        pass->width = static_cast<std::uint16_t>(_winfo.swapchain.extent.width);
        pass->height = static_cast<std::uint16_t>(_winfo.swapchain.extent.height);

        // record and submit
        {
            auto& cmd_buffer = get_instant_command_buffer();
            cmd_buffer.begin();

            VkBufferImageCopy region = {
                .bufferOffset{0},
                .bufferRowLength{0},
                .bufferImageHeight{0},
                .imageSubresource{
                    .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                    .mipLevel{0},
                    .baseArrayLayer{0},
                    .layerCount{1},
                },
                .imageOffset{
                    .x{0},
                    .y{0},
                    .z{0},
                },
                .imageExtent{
                    .width{_winfo.swapchain.extent.width},
                    .height{_winfo.swapchain.extent.height},
                    .depth{1},
                },
            };

            for (std::size_t i = 0; i < _winfo.swapchain.image_count; ++i)
            {
                transition_image_layout(_dispatch, cmd_buffer, _winfo.images[i], _winfo.swapchain.image_format,
                                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                        VK_IMAGE_ASPECT_COLOR_BIT);
            }

            cmd_buffer.end();

            VkCommandBuffer vk_buf = cmd_buffer;

            VkSubmitInfo submit = {
                .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO},
                .pNext{nullptr},
                .commandBufferCount{1},
                .pCommandBuffers{&vk_buf},
            };

            _dispatch.queueSubmit(_graphics_queue, 1, &submit, VK_NULL_HANDLE);
            _dispatch.queueWaitIdle(_graphics_queue);
        }
    }

    void gfx_device::_create_framebuffer(render_pass* pass, std::span<texture_handle> colors,
                                         texture_handle depth_stencil)
    {
        std::array<VkImageView, max_framebuffer_attachments + 1> attachments;
        std::uint32_t attachment_count{0};

        assert(colors.size() + 1 <= max_framebuffer_attachments);

        for (auto& handle : colors)
        {
            texture* tex = access_texture(handle);
            attachments[attachment_count++] = tex->underlying_view;
        }

        if (depth_stencil.index != invalid_resource_handle)
        {
            texture* tex = access_texture(depth_stencil);
            assert(attachment_count < attachments.size());
            attachments[attachment_count++] = tex->underlying_view;
        }

        VkFramebufferCreateInfo vk_fb_ci = {
            .sType{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .renderPass{pass->pass},
            .attachmentCount{attachment_count},
            .pAttachments{attachments.data()},
            .width{pass->width},
            .height{pass->height},
            .layers{1},
        };

        auto result = _dispatch.createFramebuffer(&vk_fb_ci, _alloc_callbacks, &pass->target);
        if (result != VK_SUCCESS)
        {
            logger->error("Failed to create VkFramebuffer for pass {0}", pass->name);
        }
        _set_resource_name(VK_OBJECT_TYPE_FRAMEBUFFER, reinterpret_cast<std::uint64_t>(pass->target), pass->name);
    }

    render_pass_attachment_info gfx_device::_fill_render_pass_attachment_info(const render_pass_create_info& ci)
    {
        render_pass_attachment_info attachment_info{
            .color_attachment_count{ci.render_targets},
            .color_load{ci.color_load},
            .depth_load{ci.depth_load},
            .stencil_load{ci.stencil_load},
        };

        for (std::size_t i = 0; i < ci.render_targets; ++i)
        {
            texture* tex = access_texture(ci.color_outputs[i]);
            attachment_info.color_formats[i] = tex->image_fmt;
        }

        if (ci.depth_stencil_texture.index != invalid_resource_handle)
        {
            texture* tex = access_texture(ci.depth_stencil_texture);
            attachment_info.depth_stencil_format = tex->image_fmt;
        }

        return attachment_info;
    }

    void gfx_device::_recreate_swapchain()
    {
        logger->info("Swapchain no longer optimal. Reconstructing the swapchain.");

        while (_winfo.win->minimized())
        {
            glfwWaitEvents();
        }

        _dispatch.deviceWaitIdle();
        auto width = _winfo.win->width();
        auto height = _winfo.win->height();

        if (width == 0 || height == 0)
        {
            logger->warn("Cannot resize swapchain with 0 sized dimension. Requested dimensions: {0}x{1}", width,
                         height);
            return;
        }

        render_pass* swap_pass = access_render_pass(_swapchain_render_pass);
        _dispatch.destroyRenderPass(swap_pass->pass, _alloc_callbacks);
        _destroy_swapchain_resources();

        vkb::SwapchainBuilder bldr =
            vkb::SwapchainBuilder{_logical_device, _winfo.surface}
                .set_allocation_callbacks(_alloc_callbacks)
                .set_required_min_image_count(2)
                .set_desired_format({.format{VK_FORMAT_R8G8B8A8_SRGB}, .colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}})
                .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR);
        auto swap_result = bldr.build();
        if (!swap_result)
        {
            logger->error("Failed to create VkSwapchainKHR for window.");
            return;
        }

        VkImageViewUsageCreateInfo usage = {
            .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO},
            .usage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
        };

        const auto swapchain_images_result = swap_result->get_images();
        const auto swapchain_views_result = swap_result->get_image_views(&usage);

        if (!swapchain_views_result || !swapchain_images_result)
        {
            logger->error("Failed to create VkImageViews for VkSwapchainKHR attachments.");
            return;
        }

        _winfo.swapchain = *swap_result;
        _winfo.images = *swapchain_images_result;
        _winfo.views = *swapchain_views_result;

        std::fill_n(std::begin(_swapchain_attachment_info.color_formats),
                    _swapchain_attachment_info.color_formats.size(), VK_FORMAT_UNDEFINED);

        _swapchain_attachment_info.color_formats[0] = _winfo.swapchain.image_format;
        _swapchain_attachment_info.depth_stencil_format = VK_FORMAT_UNDEFINED;
        _swapchain_attachment_info.color_attachment_count = 1;
        _swapchain_attachment_info.color_load = _swapchain_attachment_info.depth_load =
            _swapchain_attachment_info.stencil_load = render_pass_attachment_operation::DONT_CARE;

        render_pass_create_info ci = {
            .type{render_pass_type::SWAPCHAIN},
            .color_load{render_pass_attachment_operation::CLEAR},
            .depth_load{render_pass_attachment_operation::CLEAR},
            .stencil_load{render_pass_attachment_operation::CLEAR},
            .name{"Swapchain Resolve Pass"},
        };

        _create_swapchain_pass(ci, swap_pass);
    }

    void gfx_device::_destroy_swapchain_resources()
    {
        for (std::size_t i = 0; i < _winfo.swapchain.image_count; ++i)
        {
            _dispatch.destroyFramebuffer(_winfo.swapchain_targets[i], _alloc_callbacks);
            _dispatch.destroyImageView(_winfo.views[i], _alloc_callbacks);
        }
        vkb::destroy_swapchain(_winfo.swapchain);
    }

    void gfx_device::_write_bindless_images()
    {
        constexpr std::size_t writes_per_frame = 32;
        std::array<VkWriteDescriptorSet, writes_per_frame> writes;
        std::array<VkDescriptorImageInfo, writes_per_frame> desc_image_infos;

        std::size_t expected_write_count = std::min(_texture_bindless_update_queue.size(), writes_per_frame);

        // no work to be done, early return
        if (expected_write_count == 0)
        {
            return;
        }

        for (std::size_t i = 0; i < expected_write_count; ++i)
        {
            auto desc_index = expected_write_count - i - 1;
            auto& res_update_desc = _texture_bindless_update_queue[desc_index];

            auto& desc_write = writes[i];
            desc_write = {
                .sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                .pNext{nullptr},
                .dstSet{_desc_pool->get_bindless_texture_descriptors()},
                .dstBinding{_desc_pool->get_bindless_texture_index()},
                .descriptorCount{1},
                .descriptorType{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                .pImageInfo{&desc_image_infos[i]},
            };

            auto& image_desc = desc_image_infos[i];
            auto tex = access_texture(texture_handle{res_update_desc.handle});

            image_desc = {
                .sampler{tex->samp->underlying},
                .imageView{tex->underlying_view},
                .imageLayout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            };

            // swap and pop
            if (desc_index != _texture_bindless_update_queue.size())
            {
                // move end to current index
                _texture_bindless_update_queue[desc_index] = _texture_bindless_update_queue.back();
            }
            _texture_bindless_update_queue.pop_back();
        }

        _dispatch.updateDescriptorSets(static_cast<std::uint32_t>(expected_write_count), writes.data(), 0, nullptr);
    }

    void gfx_device::_fill_write_descriptor_sets(
        const descriptor_set_layout* desc_set_layout, VkDescriptorSet vk_desc_set,
        std::span<VkWriteDescriptorSet> desc_write, std::span<VkDescriptorBufferInfo> buf_info,
        std::span<VkDescriptorImageInfo> img_info, std::uint32_t& resource_count,
        std::span<const resource_handle> resources, std::span<const sampler_handle> samplers,
        std::span<const std::uint16_t> bindings)
    {
        std::uint32_t used_resource_count{0};

        for (std::uint32_t res{0}; res < resource_count; ++res)
        {
            std::uint32_t binding_index = bindings[res];

            const auto& binding = desc_set_layout->bindings[res];
            std::uint32_t i = used_resource_count;
            ++used_resource_count;

            const std::uint32_t binding_point = binding.start;

            desc_write[i] = {
                .sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                .pNext{nullptr},
                .dstSet{vk_desc_set},
                .dstBinding{binding_point},
                .dstArrayElement{0},
                .descriptorCount{1},
                .descriptorType{binding.type},
            };

            switch (binding.type)
            {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                texture_handle handle = {.index{resources[res]}};
                texture* tex = access_texture(handle);
                img_info[i].sampler = access_sampler(_default_sampler)->underlying;
                if (tex->samp)
                {
                    img_info[i].sampler = tex->samp->underlying;
                }

                if (samplers[res].index != invalid_resource_handle)
                {
                    sampler* samp = access_sampler(samplers[res]);
                    img_info[i].sampler = samp->underlying;
                }

                img_info[i].imageLayout = texture_format_utils::has_depth_or_stencil(tex->image_fmt)
                                              ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                              : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                img_info[i].imageView = tex->underlying_view;
                desc_write[i].pImageInfo = &img_info[i];

                break;
            }
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
                texture_handle handle = {.index{resources[res]}};
                texture* tex = access_texture(handle);
                img_info[i].sampler = tex->samp->underlying;
                img_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                img_info[i].imageView = tex->underlying_view;
                desc_write[i].pImageInfo = &img_info[i];

                break;
            }
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
                texture_handle handle = {.index{resources[res]}};
                texture* tex = access_texture(handle);
                img_info[i].sampler = VK_NULL_HANDLE;
                img_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                img_info[i].imageView = tex->underlying_view;
                desc_write[i].pImageInfo = &img_info[i];
                break;
            }
            case VK_DESCRIPTOR_TYPE_SAMPLER: {
                sampler_handle handle = {.index{samplers[res]}};
                sampler* smp = access_sampler(handle);
                img_info[i].sampler = smp->underlying;
                img_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                img_info[i].imageView = VK_NULL_HANDLE;
                desc_write[i].pImageInfo = &img_info[i];
                break;
            }
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                [[fallthrough]];
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
                buffer_handle handle = {.index{resources[res]}};
                buffer* buf = access_buffer(handle);

                if (buf->parent_buffer.index != invalid_resource_handle)
                {
                    auto parent = access_buffer(buf->parent_buffer);
                    buf_info[i].buffer = parent->underlying;
                }
                else
                {
                    buf_info[i].buffer = buf->underlying;
                }

                buf_info[i].offset = 0;
                buf_info[i].range = buf->vk_size;
                break;
            }
            default:
                logger->warn("Unexpected descriptor type for VkDescriptorWrite fill");
            }
        }

        resource_count = used_resource_count;
    }

} // namespace tempest::graphics
