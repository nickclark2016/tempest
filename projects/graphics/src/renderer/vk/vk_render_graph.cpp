#include "vk_render_graph.hpp"

#include <tempest/imgui_context.hpp>
#include <tempest/logger.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>

#include <algorithm>
#include <cassert>
#include <map>
#include <ranges>
#include <unordered_set>

#include <backends/imgui_impl_vulkan.h>

namespace tempest::graphics::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::vk::render_graph"}});

        VkImageLayout compute_layout(image_resource_usage usage)
        {
            switch (usage)
            {
            case image_resource_usage::COLOR_ATTACHMENT:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case image_resource_usage::DEPTH_ATTACHMENT:
                return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            case image_resource_usage::PRESENT:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            case image_resource_usage::SAMPLED:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case image_resource_usage::STORAGE:
                return VK_IMAGE_LAYOUT_GENERAL;
            case image_resource_usage::TRANSFER_DESTINATION:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case image_resource_usage::TRANSFER_SOURCE:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case image_resource_usage::UNDEFINED:
                break;
            }
            logger->critical("Failed to compute expected image layout.");
            std::exit(EXIT_FAILURE);
        }

        VkPipelineStageFlags2 compute_image_stage_access(resource_access_type type, image_resource_usage usage,
                                                         [[maybe_unused]] pipeline_stage stage)
        {
            switch (usage)
            {
            case image_resource_usage::COLOR_ATTACHMENT: {
                switch (type)
                {
                case resource_access_type::READ_WRITE:
                    [[fallthrough]];
                case resource_access_type::READ:
                    return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                case resource_access_type::WRITE:
                    return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                }
                break;
            }
            case image_resource_usage::DEPTH_ATTACHMENT:
                return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            case image_resource_usage::SAMPLED:
                return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            case image_resource_usage::STORAGE: {
                return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            }
            case image_resource_usage::TRANSFER_SOURCE:
                [[fallthrough]];
            case image_resource_usage::TRANSFER_DESTINATION:
                return VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            case image_resource_usage::PRESENT:
                return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            case image_resource_usage::UNDEFINED:
                break;
            }

            logger->critical("Failed to determine VkPipelineStageFlags for image access.");
            std::exit(EXIT_FAILURE);
        }

        VkPipelineStageFlags2 compute_buffer_stage_access([[maybe_unused]] resource_access_type type,
                                                          buffer_resource_usage usage, queue_operation_type ops)
        {
            switch (usage)
            {
            case buffer_resource_usage::CONSTANT:
            case buffer_resource_usage::STRUCTURED: {
                switch (ops)
                {
                case queue_operation_type::GRAPHICS:
                    [[fallthrough]];
                case queue_operation_type::GRAPHICS_AND_TRANSFER:
                    return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                case queue_operation_type::COMPUTE:
                    [[fallthrough]];
                case queue_operation_type::COMPUTE_AND_TRANSFER:
                    return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                default:
                    break;
                }
                break;
            }
            case buffer_resource_usage::VERTEX:
                [[fallthrough]];
            case buffer_resource_usage::INDEX:
                return VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
            case buffer_resource_usage::INDIRECT_ARGUMENT:
                return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            case buffer_resource_usage::TRANSFER_DESTINATION:
                [[fallthrough]];
            case buffer_resource_usage::TRANSFER_SOURCE:
                return VK_PIPELINE_STAGE_2_COPY_BIT;
            case buffer_resource_usage::HOST_WRITE:
                return VK_PIPELINE_STAGE_2_HOST_BIT;
            }

            logger->critical("Failed to determine VkPipelineStageFlags for buffer access.");
            std::exit(EXIT_FAILURE);
        }

        VkAccessFlags2 compute_image_access_mask(resource_access_type type, image_resource_usage usage,
                                                 [[maybe_unused]] queue_operation_type ops)
        {
            switch (usage)
            {
            case image_resource_usage::COLOR_ATTACHMENT: {
                switch (type)
                {
                case resource_access_type::READ:
                    return VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
                case resource_access_type::WRITE:
                    return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                case resource_access_type::READ_WRITE:
                    return VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                }
                break;
            }
            case image_resource_usage::DEPTH_ATTACHMENT: {
                switch (type)
                {
                case resource_access_type::READ:
                    return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                case resource_access_type::WRITE:
                    return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                case resource_access_type::READ_WRITE:
                    return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }
                break;
            }
            case image_resource_usage::SAMPLED:
                return VK_ACCESS_2_SHADER_READ_BIT;
            case image_resource_usage::STORAGE: {
                switch (type)
                {
                case resource_access_type::READ:
                    return VK_ACCESS_2_SHADER_READ_BIT;
                case resource_access_type::WRITE:
                    return VK_ACCESS_2_SHADER_WRITE_BIT;
                case resource_access_type::READ_WRITE:
                    return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
                }
                break;
            }
            case image_resource_usage::TRANSFER_DESTINATION:
                return VK_ACCESS_2_TRANSFER_WRITE_BIT;
            case image_resource_usage::TRANSFER_SOURCE:
                return VK_ACCESS_2_TRANSFER_READ_BIT;
            case image_resource_usage::PRESENT:
                return VK_ACCESS_2_NONE;
            default:
                break;
            }

            logger->critical("Failed to determine VkAccessFlags for image access.");
            std::exit(EXIT_FAILURE);
        }

        VkAccessFlags2 compute_buffer_access_mask(resource_access_type type, buffer_resource_usage usage)
        {
            switch (usage)
            {
            case buffer_resource_usage::STRUCTURED: {
                switch (type)
                {
                case resource_access_type::READ:
                    return VK_ACCESS_2_SHADER_READ_BIT;
                case resource_access_type::WRITE:
                    return VK_ACCESS_2_SHADER_WRITE_BIT;
                case resource_access_type::READ_WRITE:
                    return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
                }
                break;
            }
            case buffer_resource_usage::CONSTANT:
                return VK_ACCESS_2_UNIFORM_READ_BIT;
            case buffer_resource_usage::VERTEX:
                return VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
            case buffer_resource_usage::INDEX:
                return VK_ACCESS_2_INDEX_READ_BIT;
            case buffer_resource_usage::INDIRECT_ARGUMENT:
                return VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
            case buffer_resource_usage::TRANSFER_DESTINATION:
                return VK_ACCESS_2_TRANSFER_WRITE_BIT;
            case buffer_resource_usage::TRANSFER_SOURCE:
                return VK_ACCESS_2_TRANSFER_READ_BIT;
            case buffer_resource_usage::HOST_WRITE:
                return VK_ACCESS_2_HOST_WRITE_BIT;
            }

            logger->critical("Failed to determine VkAccessFlags for buffer access.");
            std::exit(EXIT_FAILURE);
        }

        bool has_write_mask(VkAccessFlags2 access)
        {
            static constexpr VkAccessFlags2 write_access_mask =
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT |
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT |
                VK_ACCESS_2_TRANSFER_WRITE_BIT;
            return (access & write_access_mask) != 0;
        }

        VkAttachmentLoadOp compute_load_op(load_op load)
        {
            return static_cast<VkAttachmentLoadOp>(load);
        }

        VkAttachmentStoreOp compute_store_op(store_op store)
        {
            return static_cast<VkAttachmentStoreOp>(store);
        }

        VkShaderStageFlags compute_accessible_stages(queue_operation_type op)
        {
            switch (op)
            {
            case queue_operation_type::COMPUTE_AND_TRANSFER:
                [[fallthrough]];
            case queue_operation_type::COMPUTE:
                return VK_SHADER_STAGE_COMPUTE_BIT;
            case queue_operation_type::GRAPHICS_AND_TRANSFER:
                [[fallthrough]];
            case queue_operation_type::GRAPHICS:
                return VK_SHADER_STAGE_ALL_GRAPHICS;
            default:
                break;
            }

            logger->critical("Failed to determine VkPipelineStageFlags for resource access.");
            std::exit(EXIT_FAILURE);
        }

        VkDescriptorType get_descriptor_type(buffer_resource_usage usage, bool per_frame)
        {
            switch (usage)
            {
            case buffer_resource_usage::STRUCTURED: {
                return per_frame ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            }
            case buffer_resource_usage::CONSTANT:
                return per_frame ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            default:
                break;
            }

            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }

        VkDescriptorType get_descriptor_type(image_resource_usage usage)
        {
            switch (usage)
            {
            case image_resource_usage::SAMPLED: {
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            }
            case image_resource_usage::STORAGE: {
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            }
            default:
                break;
            }

            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }

        void begin_marked_region([[maybe_unused]] const vkb::DispatchTable& dispatch,
                                 [[maybe_unused]] VkCommandBuffer buf, [[maybe_unused]] string_view name)
        {
#ifdef _DEBUG
            VkDebugUtilsLabelEXT label = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                .pNext = nullptr,
                .pLabelName = name.data(),
                .color = {},
            };
            dispatch.cmdBeginDebugUtilsLabelEXT(buf, &label);
#endif
        }

        void end_marked_region([[maybe_unused]] const vkb::DispatchTable& dispatch,
                               [[maybe_unused]] VkCommandBuffer buf)
        {
#ifdef _DEBUG
            dispatch.cmdEndDebugUtilsLabelEXT(buf);
#endif
        }
    } // namespace

    render_graph_resource_library::render_graph_resource_library([[maybe_unused]] abstract_allocator* alloc,
                                                                 render_device* device)
        : _device{device}
    {
    }

    render_graph_resource_library::~render_graph_resource_library()
    {
        for (auto& img : _compiled_images)
        {
            _device->release_image(img);
        }

        for (auto& buf : _compiled_buffers)
        {
            _device->release_buffer(buf);
        }
    }

    image_resource_handle render_graph_resource_library::find_texture([[maybe_unused]] string_view name)
    {
        // TODO: Implement
        return image_resource_handle();
    }

    image_resource_handle render_graph_resource_library::load(const image_desc& desc)
    {
        auto handle = _device->allocate_image();

        image_create_info ci = {
            .type = desc.type,
            .width = desc.width,
            .height = desc.height,
            .depth = desc.depth,
            .layers = desc.layers,
            .mip_count = desc.mips,
            .format = desc.fmt,
            .samples = desc.samples,
            .persistent = desc.persistent,
            .name = string{desc.name},
        };

        _images_to_compile.push_back(deferred_image_create_info{
            .info{ci},
            .allocation{handle},
        });

        return handle;
    }

    void render_graph_resource_library::add_image_usage(image_resource_handle handle, image_resource_usage usage)
    {
        auto image_it = std::find_if(std::begin(_images_to_compile), std::end(_images_to_compile),
                                     [handle](const auto& def) { return def.allocation == handle; });
        if (image_it != std::end(_images_to_compile))
        {
            switch (usage)
            {
            case image_resource_usage::COLOR_ATTACHMENT: {
                image_it->info.color_attachment = true;
                break;
            }
            case image_resource_usage::DEPTH_ATTACHMENT: {
                image_it->info.depth_attachment = true;
                break;
            }
            case image_resource_usage::SAMPLED: {
                image_it->info.sampled = true;
                break;
            }
            case image_resource_usage::STORAGE: {
                image_it->info.storage = true;
                break;
            }
            case image_resource_usage::TRANSFER_SOURCE: {
                image_it->info.transfer_source = true;
                break;
            }
            case image_resource_usage::TRANSFER_DESTINATION: {
                image_it->info.transfer_destination = true;
                break;
            }
            default:
                break;
            }
        }
    }

    buffer_resource_handle render_graph_resource_library::find_buffer([[maybe_unused]] string_view name)
    {
        // TODO: Implement
        return buffer_resource_handle();
    }

    buffer_resource_handle render_graph_resource_library::load(const buffer_desc& desc)
    {
        auto handle = _device->allocate_buffer();

        auto aligned_size = (desc.size + 64 - 1) & -64;

        _buffers_to_compile.push_back(deferred_buffer_create_info{
            .info{
                .per_frame = desc.per_frame_memory,
                .loc = desc.location,
                .size = aligned_size * (desc.per_frame_memory ? _device->frames_in_flight() : 1),
                .name = string{desc.name},
            },
            .allocation{handle},
        });

        return handle;
    }

    void render_graph_resource_library::add_buffer_usage(buffer_resource_handle handle, buffer_resource_usage usage)
    {
        auto buffer_it = std::find_if(std::begin(_buffers_to_compile), std::end(_buffers_to_compile),
                                      [handle](const auto& def) { return def.allocation == handle; });

        if (buffer_it != std::end(_buffers_to_compile))
        {
            switch (usage)
            {
            case buffer_resource_usage::STRUCTURED: {
                buffer_it->info.storage_buffer = true;
                break;
            }
            case buffer_resource_usage::CONSTANT: {
                buffer_it->info.uniform_buffer = true;
                break;
            }
            case buffer_resource_usage::INDEX: {
                buffer_it->info.index_buffer = true;
                break;
            }
            case buffer_resource_usage::VERTEX: {
                buffer_it->info.vertex_buffer = true;
                break;
            }
            case buffer_resource_usage::INDIRECT_ARGUMENT: {
                buffer_it->info.indirect_buffer = true;
                break;
            }
            case buffer_resource_usage::TRANSFER_SOURCE: {
                buffer_it->info.transfer_source = true;
                break;
            }
            case buffer_resource_usage::TRANSFER_DESTINATION: {
                buffer_it->info.transfer_destination = true;
                break;
            }
            case buffer_resource_usage::HOST_WRITE: {
                break;
            }
            }
        }
    }

    bool render_graph_resource_library::compile()
    {
        for (auto& image_info : _images_to_compile)
        {
            auto compiled = _device->create_image(image_info.info, image_info.allocation);
            if (!compiled)
            {
                return false;
            }
            _compiled_images.push_back(compiled);
        }

        for (auto& buffer_info : _buffers_to_compile)
        {
            auto compiled = _device->create_buffer(buffer_info.info, buffer_info.allocation);
            if (!compiled)
            {
                return false;
            }
            _compiled_buffers.push_back(compiled);
        }

        return true;
    }

    render_graph::render_graph(abstract_allocator* alloc, render_device* device,
                               span<graphics::graph_pass_builder> pass_builders,
                               unique_ptr<render_graph_resource_library>&& resources, bool imgui_enabled,
                               bool gpu_profile_enabled)
        : _resource_lib{std::move(resources)}, _alloc{alloc}, _device{device}
    {
        graphics::dependency_graph pass_graph;

        for (auto& bldr : pass_builders)
        {
            _pass_index_map[bldr.handle().as_uint64()] = _all_passes.size();
            _all_passes.push_back(bldr);
        }

        _per_frame.resize(_device->frames_in_flight());
        for (auto& frame : _per_frame)
        {
            frame.commands_complete = VK_NULL_HANDLE;
        }

        _descriptor_set_states.resize(_all_passes.size());
        for (auto& state : _descriptor_set_states)
        {
            state.per_frame_descriptors.resize(_device->frames_in_flight());
        }

        build_descriptor_sets();

        if (imgui_enabled)
        {
            VkDescriptorPoolSize pool_sizes[] = {
                {
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                },
            };

            VkDescriptorPoolCreateInfo pool_ci = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = pool_sizes,
            };

            imgui_render_graph_context ctx = {
                .instance = _device->instance().instance,
                .dev = _device->logical_device().device,
                .instance_proc_addr = _device->instance().fp_vkGetInstanceProcAddr,
                .dev_proc_addr = _device->logical_device().fp_vkGetDeviceProcAddr,
                .imgui_desc_pool = VK_NULL_HANDLE,
                .init_info = {},
                .initialized = false,
            };

            auto res = _device->dispatch().createDescriptorPool(&pool_ci, nullptr, &ctx.imgui_desc_pool);
            if (res != VK_SUCCESS)
            {
                logger->error("Failed to create VkDescriptorPool for ImGUI context.");
                return;
            }

            ImGui_ImplVulkan_InitInfo init_info = {
                .Instance = _device->instance().instance,
                .PhysicalDevice = _device->physical_device().physical_device,
                .Device = _device->logical_device().device,
                .QueueFamily = _device->get_queue().queue_family_index,
                .Queue = _device->get_queue().queue,
                .PipelineCache = VK_NULL_HANDLE,
                .DescriptorPool = ctx.imgui_desc_pool,
                .Subpass = 0,
                .MinImageCount = static_cast<uint32_t>(_device->frames_in_flight()),
                .ImageCount = static_cast<uint32_t>(_device->frames_in_flight()),
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                .MemoryAllocator = _device->vma_allocator(),
                .UseDynamicRendering = true,
                .ColorAttachmentFormat = VK_FORMAT_R8G8B8A8_SRGB,
                .Allocator = nullptr,
                .CheckVkResultFn{[](VkResult res) {
                    if (res != VK_SUCCESS)
                    {
                        logger->error("ImGUI Vulkan returned non-success result: {}",
                                      static_cast<std::underlying_type_t<VkResult>>(res));
                    }
                }},
            };

            ctx.init_info = init_info;
            _imgui_ctx = ctx;

            ImGui_ImplVulkan_LoadFunctions(
                [](const char* fn_name, void* user_data) {
                    imgui_render_graph_context* ctx = reinterpret_cast<imgui_render_graph_context*>(user_data);
                    PFN_vkVoidFunction instance_addr = ctx->instance_proc_addr(ctx->instance, fn_name);
                    PFN_vkVoidFunction device_addr = ctx->dev_proc_addr(ctx->dev, fn_name);
                    return device_addr ? device_addr : instance_addr;
                },
                &_imgui_ctx.value());

            ImGui_ImplVulkan_Init(&_imgui_ctx->init_info, VK_NULL_HANDLE);
            ImGui_ImplVulkan_CreateFontsTexture();
        }

        if (gpu_profile_enabled)
        {
            _gpu_profile_state.emplace();
            _gpu_profile_state->timestamp_period = _device->physical_device().properties.limits.timestampPeriod;

            for (const auto& pass : _all_passes)
            {
                VkQueryPoolCreateInfo timing_pool_ci = {
                    .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = {},
                    .queryType = VK_QUERY_TYPE_TIMESTAMP,
                    .queryCount = 2 * static_cast<uint32_t>(
                                          _device->frames_in_flight()), // start and end timestamps for each frame
                    .pipelineStatistics = 0,
                };

                VkQueryPoolCreateInfo statistics_pool_ci = {
                    .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = {},
                    .queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS,
                    .queryCount = 0,
                    .pipelineStatistics = 0,
                };

                statistics_pool_ci.pipelineStatistics =
                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
                statistics_pool_ci.queryCount = static_cast<uint32_t>(
                    pipeline_statistic_results::statistic_query_count * _device->frames_in_flight());

                // TODO: Check for query pool support

                gpu_profile_pool_state pools = {
                    .pass = pass.handle(),
                    .pipeline_stats = tempest::nullopt,
                    .timestamp = {},
                    .cpu_timestamp = {},
                };

                if (timing_pool_ci.queryCount > 0)
                {
                    _device->dispatch().createQueryPool(&timing_pool_ci, nullptr, &pools.timestamp_queries);
                    _device->dispatch().resetQueryPool(pools.timestamp_queries, 0, timing_pool_ci.queryCount);
                }

                if (statistics_pool_ci.queryCount > 0)
                {
                    _device->dispatch().createQueryPool(&statistics_pool_ci, nullptr, &pools.pipeline_stat_queries);
                    _device->dispatch().resetQueryPool(pools.pipeline_stat_queries, 0, statistics_pool_ci.queryCount);
                }

                _gpu_profile_state->recording_state.pools.emplace_back(pools);

                _gpu_profile_state->results.pass_results.push_back(gpu_profile_pass_results{
                    .pass = pass.handle(),
                    .pipeline_stats = tempest::nullopt,
                    .timestamp = {},
                    .cpu_timestamp = {},
                });
            }
        }
    }

    render_graph::~render_graph()
    {
        for (auto& state : _descriptor_set_states)
        {
            for (auto& write : state.writes)
            {
                if (write.descriptorCount == 1)
                {
                    delete write.pBufferInfo;
                    delete write.pImageInfo;
                }
                else
                {
                    delete[] write.pBufferInfo;
                    delete[] write.pImageInfo;
                }
            }
        }

        _device->idle();

        if (_imgui_ctx)
        {
            ImGui_ImplVulkan_DestroyFontsTexture();
            ImGui_ImplVulkan_Shutdown();
            _device->dispatch().destroyDescriptorPool(_imgui_ctx->imgui_desc_pool, nullptr);
        }

        for (auto& frame : _per_frame)
        {
            if (frame.commands_complete)
            {
                _device->release_fence(std::move(frame.commands_complete));
            }

            _device->dispatch().destroyDescriptorPool(frame.desc_pool, nullptr);
        }

        for (auto& desc_set_state : _descriptor_set_states)
        {
            for (VkDescriptorSetLayout layout : desc_set_state.set_layouts)
            {
                _device->dispatch().destroyDescriptorSetLayout(layout, nullptr);
            }

            if (desc_set_state.layout != VK_NULL_HANDLE)
            {
                _device->dispatch().destroyPipelineLayout(desc_set_state.layout, nullptr);
            }
        }

        for (auto& pools : _gpu_profile_state->recording_state.pools)
        {
            if (pools.timestamp_queries != VK_NULL_HANDLE)
            {
                _device->dispatch().destroyQueryPool(pools.timestamp_queries, nullptr);
            }

            if (pools.pipeline_stat_queries != VK_NULL_HANDLE)
            {
                _device->dispatch().destroyQueryPool(pools.pipeline_stat_queries, nullptr);
            }
        }
    }

    void render_graph::update_external_sampled_images(graph_pass_handle pass, span<image_resource_handle> images,
                                                      uint32_t set, uint32_t binding, pipeline_stage stage)
    {
        auto pass_idx = _pass_index_map[pass.as_uint64()];
        _all_passes[pass_idx].add_external_sampled_images(images, set, binding, stage);

        auto image_count = images.size();

        auto image_writes = new VkDescriptorImageInfo[image_count];

        uint32_t images_written = 0;

        for (; images_written < image_count; ++images_written)
        {
            if (images[images_written])
            {
                auto img = _device->access_image(images[images_written]);
                image_writes[images_written] = {
                    .sampler = VK_NULL_HANDLE,
                    .imageView = img->view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
            }
        }

        auto vk_set = _descriptor_set_states[pass_idx].per_frame_descriptors[0].descriptor_sets[set];

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = vk_set,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = images_written,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = image_writes,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        auto& write_info = *std::find_if(
            begin(_descriptor_set_states[pass_idx].writes), end(_descriptor_set_states[pass_idx].writes),
            [&](const auto& write) {
                return write.dstBinding == binding &&
                       std::any_of(tempest::begin(_descriptor_set_states[pass_idx].vk_set_to_set_index),
                                   tempest::end(_descriptor_set_states[pass_idx].vk_set_to_set_index),
                                   [&](const auto& vk_index_pair) { return vk_index_pair.second == set; });
            });

        delete[] write_info.pImageInfo;

        write_info = write;

        _descriptor_set_states[pass_idx].last_update_frame = _device->current_frame();
    }

    void render_graph::execute()
    {
        bool gpu_profile_enabled = _gpu_profile_state.has_value();

        if (gpu_profile_enabled)
        {
            auto frame_start_time = std::chrono::high_resolution_clock::now();
            auto frame_start_time_ns =
                std::chrono::time_point_cast<std::chrono::nanoseconds>(frame_start_time).time_since_epoch().count();

            _gpu_profile_state->recording_state.full_frame_cpu_timestamp.begin_timestamp = frame_start_time_ns;
        }

        _device->start_frame();

        // first, check to see if the pass states are the same
        bool active_change_detected = false;
        for (size_t i = 0; i < _all_passes.size(); ++i)
        {
            auto& pass = _all_passes[i];
            auto should_exec = pass.should_execute();
            active_change_detected |= (_active_passes.test(i) != should_exec);
            _active_passes.set(i, should_exec);
        }

        // if the passes are no longer active, we need to re-sort the passes
        if (active_change_detected)
        {
            dependency_graph pass_graph;

            for (std::size_t i = 0; i < _all_passes.size(); ++i)
            {
                auto& pass = _all_passes[i];
                if (!_active_passes.test(i))
                {
                    continue;
                }

                _descriptor_set_states[i].last_update_frame = _device->current_frame();

                pass_graph.add_graph_pass(pass.handle().as_uint64());

                for (auto& dep : pass.depends_on())
                {
                    auto dep_bldr_it = std::find_if(std::begin(_all_passes), std::end(_all_passes),
                                                    [dep](const auto& node) { return node.handle() == dep; });
                    if (dep_bldr_it != std::end(_all_passes)) [[likely]]
                    {
                        graph_pass_builder& bldr = *dep_bldr_it;
                        if (bldr.should_execute())
                        {
                            pass_graph.add_graph_dependency(dep.as_uint64(), pass.handle().as_uint64());
                        }
                    }
                }
            }

            auto sorted_pass_handles = pass_graph.toposort();
            _active_pass_set.clear();

            std::transform(std::begin(sorted_pass_handles), std::end(sorted_pass_handles),
                           std::back_inserter(_active_pass_set), [this](auto handle) {
                               return ref(*std::find_if(
                                   std::begin(_all_passes), std::end(_all_passes),
                                   [handle](const auto& node) { return node.handle().as_uint64() == handle; }));
                           });

            _active_swapchain_set.clear();
            for (auto& pass : _active_pass_set)
            {
                for (auto& swapchain_usage : pass.get().external_swapchain_usage())
                {
                    auto search_it = std::find(std::begin(_active_swapchain_set), std::end(_active_swapchain_set),
                                               swapchain_usage.swap);
                    if (search_it == std::end(_active_swapchain_set))
                    {
                        _active_swapchain_set.push_back(swapchain_usage.swap);
                    }
                }
            }
        }

        // write barriers
        auto cmd_buffer_alloc = _device->acquire_frame_local_command_buffer_allocator();
        auto cmds = cmd_buffer_alloc.allocate();
        auto dispatch = cmd_buffer_alloc.dispatch;

        // first, wait for commands to complete
        auto& frame_data = _per_frame[_device->frame_in_flight() % _device->frames_in_flight()];
        VkFence& commands_complete = frame_data.commands_complete;
        if (commands_complete == VK_NULL_HANDLE) [[unlikely]]
        {
            commands_complete = _device->acquire_fence();
        }
        else if (dispatch->getFenceStatus(commands_complete) != VK_SUCCESS && !_recreated_sc_last_frame) [[likely]]
        {
            dispatch->waitForFences(1, &commands_complete, VK_TRUE, UINT_MAX);
            _recreated_sc_last_frame = false;
        }

        dispatch->resetFences(1, &commands_complete);

        vector<VkSemaphore> image_acquired_sems;
        vector<VkSemaphore> render_complete_sems;
        vector<VkPipelineStageFlags> wait_stages;
        vector<VkSwapchainKHR> swapchains;
        vector<uint32_t> image_indices;

        if (gpu_profile_enabled)
        {
            auto pre_acquire_time = std::chrono::high_resolution_clock::now();
            auto pre_acquire_time_ns =
                std::chrono::time_point_cast<std::chrono::nanoseconds>(pre_acquire_time).time_since_epoch().count();
            _gpu_profile_state->recording_state.image_acquire_cpu_timestamp.begin_timestamp = pre_acquire_time_ns;
        }

        // next, acquire the swapchain image
        for (auto& swapchain : _active_swapchain_set)
        {
            auto signal_sem = _device->acquire_semaphore();
            auto render_complete_sem = _device->acquire_semaphore();
            auto swap = _device->access_swapchain(swapchain);
            auto acquire_result = dispatch->acquireNextImageKHR(swap->sc.swapchain, UINT_MAX, signal_sem,
                                                                VK_NULL_HANDLE, &swap->image_index);

            if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                _device->release_frame_local_command_buffer_allocator(std::move(cmd_buffer_alloc));
                for (auto sem : image_acquired_sems)
                {
                    _device->release_semaphore(std::move(sem));
                }

                for (auto sem : render_complete_sems)
                {
                    _device->release_semaphore(std::move(sem));
                }

                _device->release_semaphore(std::move(signal_sem));
                _device->release_semaphore(std::move(render_complete_sem));

                _device->recreate_swapchain(swapchain);
                _device->end_frame();
                _recreated_sc_last_frame = true;

                return;
            }

            image_acquired_sems.push_back(signal_sem);
            wait_stages.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
            render_complete_sems.push_back(render_complete_sem);
            swapchains.push_back(swap->sc.swapchain);
            image_indices.push_back(swap->image_index);
        }

        if (gpu_profile_enabled)
        {
            auto post_acquire_time = std::chrono::high_resolution_clock::now();
            auto post_acquire_time_ns =
                std::chrono::time_point_cast<std::chrono::nanoseconds>(post_acquire_time).time_since_epoch().count();
            _gpu_profile_state->recording_state.image_acquire_cpu_timestamp.end_timestamp = post_acquire_time_ns;
        }

        auto queue = _device->get_queue();

        VkCommandBufferBeginInfo begin = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pInheritanceInfo = nullptr,
        };
        cmd_buffer_alloc.dispatch->beginCommandBuffer(cmds, &begin);

        if (gpu_profile_enabled)
        {
            _gpu_profile_state->results.frame_index = _device->current_frame() - _device->frames_in_flight();
        }

        for (auto pass_ref_wrapper : _active_pass_set)
        {
            auto start_time = std::chrono::high_resolution_clock::now();

            if (gpu_profile_enabled)
            {
                // Get pass index
                auto& pass_ref = pass_ref_wrapper.get();
                auto pass_idx = _pass_index_map[pass_ref.handle().as_uint64()];

                // Get the pool for this pass
                auto& pools = _gpu_profile_state->recording_state.pools[pass_idx];

                // Get the timestamp query index
                auto begin_timestamp_query_index = static_cast<uint32_t>(_device->frame_in_flight() * 2);

                // Queries are only ready after the FRAMES_IN_FLIGHT frames have been submitted
                if (_device->current_frame() >= _device->frames_in_flight()) [[likely]]
                {
                    // Check if the queries are ready
                    uint64_t timestamps[2] = {0, 0};

                    VkResult result = dispatch->getQueryPoolResults(
                        pools.timestamp_queries, begin_timestamp_query_index, 2, sizeof(uint64_t) * 2, timestamps,
                        sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
                    // If the queries are ready, log the results
                    if (result == VK_SUCCESS)
                    {
                        pools.timestamp.begin_timestamp = timestamps[0];
                        pools.timestamp.end_timestamp = timestamps[1];

                        // Reset the query
                        dispatch->cmdResetQueryPool(cmds, pools.timestamp_queries, begin_timestamp_query_index, 2);

                        logger->debug("Successfully queried timestamps for pass {} and frame {}.  Query Results: Begin "
                                      "- {} End - {}",
                                      pass_ref.name().data(), _device->current_frame() - _device->frames_in_flight(),
                                      timestamps[0], timestamps[1]);
                    }
                    else
                    {
                        pools.timestamp.begin_timestamp = 0;
                        pools.timestamp.end_timestamp = 0;

                        logger->warn("Failed to get timestamp query results for pass {} and frame {}.",
                                     pass_ref.name().data(), _device->current_frame() - _device->frames_in_flight());
                    }
                }

                // std::array<uint64_t, pipeline_statistic_results::statistic_query_count> pipeline_statistics =
                // {}; result = dispatch->getQueryPoolResults(
                //     pools.pipeline_stat_queries,
                //     _device->frame_in_flight() * pipeline_statistic_results::statistic_query_count,
                //     pipeline_statistic_results::statistic_query_count,
                //     sizeof(uint64_t) * pipeline_statistic_results::statistic_query_count, pipeline_statistics.data(),
                //     sizeof(uint64_t) * pipeline_statistic_results::statistic_query_count, VK_QUERY_RESULT_64_BIT);

                // if (result == VK_SUCCESS)
                // {
                //     pools.pipeline_stats.emplace(pipeline_statistic_results{
                //         .input_assembly_vertices = pipeline_statistics[0],
                //         .input_assembly_primitives = pipeline_statistics[1],
                //         .vertex_shader_invocations = pipeline_statistics[2],
                //         .tess_control_shader_invocations = pipeline_statistics[8],
                //         .tess_evaluation_shader_invocations = pipeline_statistics[9],
                //         .geometry_shader_invocations = pipeline_statistics[3],
                //         .geometry_shader_primitives = pipeline_statistics[4],
                //         .fragment_shader_invocations = pipeline_statistics[7],
                //         .clipping_invocations = pipeline_statistics[5],
                //         .clipping_primitives = pipeline_statistics[6],
                //         .compute_shader_invocations = pipeline_statistics[10],
                //     });
                // }
                // else
                // {
                //     pools.pipeline_stats = std::nullopt;
                // }

                // Begin timestamp query
                cmd_buffer_alloc.dispatch->cmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                             pools.timestamp_queries, begin_timestamp_query_index);
            }

            auto& pass_ref = pass_ref_wrapper.get();
            begin_marked_region(_device->dispatch(), cmds, pass_ref.name());

            // for the updated passes, update the descriptor sets
            auto desc_set_state = _descriptor_set_states[_pass_index_map[pass_ref.handle().as_uint64()]];
            if (desc_set_state.last_update_frame + _device->frames_in_flight() > _device->current_frame())
            {
                auto& per_frame_desc = desc_set_state.per_frame_descriptors[_device->frame_in_flight()];
                auto& writes = desc_set_state.writes;

                for (auto& write : writes)
                {
                    auto set_index = desc_set_state.vk_set_to_set_index[write.dstSet];
                    auto set = per_frame_desc.descriptor_sets[set_index];

                    write.dstSet = set;
                }

                auto write_copy = writes;
                tempest::erase_if(write_copy, [](const auto& write) { return write.descriptorCount == 0; });

                dispatch->updateDescriptorSets(static_cast<uint32_t>(write_copy.size()), write_copy.data(), 0, nullptr);
            }

            // apply barriers
            std::vector<VkImageMemoryBarrier2> image_barriers_2;
            std::vector<VkBufferMemoryBarrier2> buffer_barriers_2;

            for (const auto& swap : pass_ref.external_swapchain_usage())
            {
                auto swapchain_state_it = _last_known_state.swapchain.find(swap.swap.as_uint64());
                auto swapchain = _device->access_swapchain(swap.swap);
                auto vk_img = _device->access_image(swapchain->image_handles[swapchain->image_index]);

                swapchain_resource_state next_state = {
                    .swapchain = swap.swap,
                    .image_layout = compute_layout(swap.usage),
                    .stage_mask = compute_image_stage_access(swap.type, swap.usage, swap.first_access),
                    .access_mask = compute_image_access_mask(swap.type, swap.usage, queue_operation_type::GRAPHICS),
                };

                VkImageMemoryBarrier2 img_barrier_2 = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .pNext = nullptr,
                    .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    .srcAccessMask = 0,
                    .dstStageMask = next_state.stage_mask,
                    .dstAccessMask = next_state.access_mask,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = next_state.image_layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = vk_img->image,
                    .subresourceRange{
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                };

                if (swapchain_state_it != _last_known_state.swapchain.end())
                {
                    swapchain_resource_state last_state = swapchain_state_it->second;

                    img_barrier_2.srcAccessMask = last_state.access_mask;

                    img_barrier_2.srcStageMask = last_state.stage_mask;
                    img_barrier_2.dstStageMask = next_state.stage_mask;
                }
                else
                {
                    img_barrier_2.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                    img_barrier_2.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
                }

                if (img_barrier_2.oldLayout != img_barrier_2.newLayout || has_write_mask(img_barrier_2.srcAccessMask) ||
                    has_write_mask(img_barrier_2.dstAccessMask))
                {
                    image_barriers_2.push_back(img_barrier_2);
                }

                _last_known_state.swapchain[swap.swap.as_uint64()] = next_state;
            }

            for (const auto& img : pass_ref.image_usage())
            {
                for (const auto& img_handle : img.handles)
                {
                    auto image_state_it = _last_known_state.images.find(img_handle.as_uint64());
                    auto vk_img = _device->access_image(img_handle);

                    render_graph_image_state next_state = {
                        .persistent = vk_img->persistent,
                        .stage_mask = compute_image_stage_access(img.type, img.usage, img.first_access),
                        .access_mask = compute_image_access_mask(img.type, img.usage, pass_ref.operation_type()),
                        .image_layout = compute_layout(img.usage),
                        .image = vk_img->image,
                        .aspect = vk_img->view_info.subresourceRange.aspectMask,
                        .base_mip = vk_img->view_info.subresourceRange.baseMipLevel,
                        .mip_count = vk_img->view_info.subresourceRange.levelCount,
                        .base_array_layer = vk_img->view_info.subresourceRange.baseArrayLayer,
                        .layer_count = vk_img->view_info.subresourceRange.layerCount,
                        .queue_family = queue.queue_family_index,
                    };

                    VkImageMemoryBarrier2 img_barrier_2 = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .pNext = nullptr,
                        .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        .srcAccessMask = 0,
                        .dstStageMask = next_state.stage_mask,
                        .dstAccessMask = next_state.access_mask,
                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout = next_state.image_layout,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = vk_img->image,
                        .subresourceRange = vk_img->view_info.subresourceRange,
                    };

                    if (image_state_it != _last_known_state.images.end()) [[likely]]
                    {
                        render_graph_image_state last_state = image_state_it->second;

                        img_barrier_2.oldLayout = last_state.image_layout;
                        img_barrier_2.srcAccessMask = last_state.access_mask;
                        img_barrier_2.srcStageMask = last_state.stage_mask;

                        if (pass_ref.operation_type() == queue_operation_type::COMPUTE &&
                            (vk_img->img_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ==
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                        {
                            img_barrier_2.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                        }
                    }

                    if (img_barrier_2.oldLayout != img_barrier_2.newLayout ||
                        img_barrier_2.srcQueueFamilyIndex != img_barrier_2.dstQueueFamilyIndex ||
                        has_write_mask(img_barrier_2.srcAccessMask) || has_write_mask(img_barrier_2.dstAccessMask))
                    {
                        img_barrier_2.dstStageMask |= next_state.stage_mask;
                        image_barriers_2.push_back(img_barrier_2);
                    }

                    // write new state for the end of the frame
                    _last_known_state.images[img_handle.as_uint64()] = next_state;
                }
            }

            for (const auto& buf : pass_ref.buffer_usage())
            {
                const auto buffer_state_it = _last_known_state.buffers.find(buf.buf.as_uint64());
                auto vk_buf = _device->access_buffer(buf.buf);

                auto size_per_frame = vk_buf->per_frame_resource ? vk_buf->alloc_info.size / _device->frames_in_flight()
                                                                 : vk_buf->alloc_info.size;
                auto offset = vk_buf->per_frame_resource ? size_per_frame * _device->frame_in_flight() : 0;

                render_graph_buffer_state next_state = {
                    .stage_mask = compute_buffer_stage_access(buf.type, buf.usage, pass_ref.operation_type()),
                    .access_mask = compute_buffer_access_mask(buf.type, buf.usage),
                    .buffer = vk_buf->vk_buffer,
                    .offset = offset,
                    .size = size_per_frame,
                    .queue_family = queue.queue_family_index,
                };

                VkBufferMemoryBarrier2 buf_barrier_2 = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                    .pNext = nullptr,
                    .srcStageMask = 0,
                    .srcAccessMask = VK_ACCESS_NONE,
                    .dstStageMask = 0,
                    .dstAccessMask = next_state.access_mask,
                    .srcQueueFamilyIndex = queue.queue_family_index,
                    .dstQueueFamilyIndex = next_state.queue_family,
                    .buffer = vk_buf->vk_buffer,
                    .offset = next_state.offset,
                    .size = next_state.size,
                };

                if ((next_state.access_mask & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
                {
                    buf_barrier_2.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                }

                if (buffer_state_it != _last_known_state.buffers.end()) [[likely]]
                {
                    render_graph_buffer_state last_state = buffer_state_it->second;

                    buf_barrier_2.srcAccessMask = last_state.access_mask;
                    buf_barrier_2.srcQueueFamilyIndex = last_state.queue_family;
                    buf_barrier_2.srcStageMask = last_state.stage_mask;
                }

                // if we've got a queue ownership transformation or a write access, force a barrier
                if (buf_barrier_2.srcQueueFamilyIndex != buf_barrier_2.dstQueueFamilyIndex ||
                    has_write_mask(buf_barrier_2.srcAccessMask) || has_write_mask(buf_barrier_2.dstAccessMask))
                {
                    buf_barrier_2.dstStageMask |= next_state.stage_mask;
                    buffer_barriers_2.push_back(buf_barrier_2);
                }

                _last_known_state.buffers[buf.buf.as_uint64()] = next_state;
            }

            if (!image_barriers_2.empty() || !buffer_barriers_2.empty())
            {
                VkDependencyInfo dep_info = {
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .pNext = nullptr,
                    .dependencyFlags = 0,
                    .memoryBarrierCount = 0,
                    .pMemoryBarriers = nullptr,
                    .bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers_2.size()),
                    .pBufferMemoryBarriers = buffer_barriers_2.empty() ? nullptr : buffer_barriers_2.data(),
                    .imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers_2.size()),
                    .pImageMemoryBarriers = image_barriers_2.empty() ? nullptr : image_barriers_2.data(),
                };
                cmd_buffer_alloc.dispatch->cmdPipelineBarrier2(cmds, &dep_info);
            }

            if (pass_ref.operation_type() == queue_operation_type::GRAPHICS)
            {
                VkRect2D area{};
                std::vector<VkRenderingAttachmentInfo> color_attachments;
                VkRenderingAttachmentInfo depth_attachment;
                bool has_depth = false;
                VkFormat first_color_fmt{VK_FORMAT_UNDEFINED};

                for (const auto& sc : pass_ref.external_swapchain_usage())
                {
                    auto swap = _device->access_swapchain(sc.swap);
                    auto vk_img = _device->access_image(swap->image_handles[swap->image_index]);

                    if (sc.usage == image_resource_usage::COLOR_ATTACHMENT)
                    {
                        VkRenderingAttachmentInfo info = {
                            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                            .pNext = nullptr,
                            .imageView = vk_img->view,
                            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            .resolveMode = VK_RESOLVE_MODE_NONE,
                            .resolveImageView = VK_NULL_HANDLE,
                            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                            .loadOp = compute_load_op(sc.load),
                            .storeOp = compute_store_op(sc.store),
                            .clearValue = {},
                        };

                        area.offset = {
                            .x = 0,
                            .y = 0,
                        };

                        area.extent = {
                            .width = vk_img->img_info.extent.width,
                            .height = vk_img->img_info.extent.height,
                        };

                        color_attachments.push_back(info);

                        if (first_color_fmt == VK_FORMAT_UNDEFINED)
                        {
                            first_color_fmt = vk_img->img_info.format;
                        }
                    }
                }

                std::vector<VkImageMemoryBarrier2> resolve_barriers;

                VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

                for (const auto& img : pass_ref.image_usage())
                {
                    auto vk_img = _device->access_image(img.handles[0]);

                    samples = vk_img->img_info.samples;

                    // find the resolve attachment if it exists
                    auto resolve_target =
                        std::find_if(std::begin(pass_ref.resolve_images()), std::end(pass_ref.resolve_images()),
                                     [img](const auto& resolve) { return resolve.src == img.handles[0]; });

                    if (img.usage == image_resource_usage::COLOR_ATTACHMENT)
                    {
                        VkRenderingAttachmentInfo info = {
                            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                            .pNext = nullptr,
                            .imageView = vk_img->view,
                            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            .resolveMode = resolve_target == std::end(pass_ref.resolve_images())
                                               ? VK_RESOLVE_MODE_NONE
                                               : VK_RESOLVE_MODE_AVERAGE_BIT,
                            .resolveImageView = resolve_target == std::end(pass_ref.resolve_images())
                                                    ? VK_NULL_HANDLE
                                                    : _device->access_image(resolve_target->dst)->view,
                            .resolveImageLayout = resolve_target == std::end(pass_ref.resolve_images())
                                                      ? VK_IMAGE_LAYOUT_UNDEFINED
                                                      : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            .loadOp = compute_load_op(img.load),
                            .storeOp = compute_store_op(img.store),
                            .clearValue{
                                .color{
                                    .float32{
                                        img.clear_color.x,
                                        img.clear_color.y,
                                        img.clear_color.z,
                                        img.clear_color.w,
                                    },
                                },
                            },
                        };

                        area.offset = {
                            .x = 0,
                            .y = 0,
                        };

                        area.extent = {
                            .width = vk_img->img_info.extent.width,
                            .height = vk_img->img_info.extent.height,
                        };

                        color_attachments.push_back(info);

                        if (first_color_fmt == VK_FORMAT_UNDEFINED)
                        {
                            first_color_fmt = vk_img->img_info.format;
                        }
                    }
                    else if (img.usage == image_resource_usage::DEPTH_ATTACHMENT)
                    {
                        depth_attachment = {
                            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                            .pNext = nullptr,
                            .imageView = vk_img->view,
                            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                            .resolveMode = resolve_target == std::end(pass_ref.resolve_images())
                                               ? VK_RESOLVE_MODE_NONE
                                               : VK_RESOLVE_MODE_MIN_BIT,
                            .resolveImageView = resolve_target == std::end(pass_ref.resolve_images())
                                                    ? VK_NULL_HANDLE
                                                    : _device->access_image(resolve_target->dst)->view,
                            .resolveImageLayout = resolve_target == std::end(pass_ref.resolve_images())
                                                      ? VK_IMAGE_LAYOUT_UNDEFINED
                                                      : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                            .loadOp = compute_load_op(img.load),
                            .storeOp = compute_store_op(img.store),
                            .clearValue{
                                .depthStencil{
                                    .depth = img.clear_depth,
                                    .stencil = 0,
                                },
                            },
                        };

                        area.offset = {
                            .x = 0,
                            .y = 0,
                        };

                        area.extent = {
                            .width = vk_img->img_info.extent.width,
                            .height = vk_img->img_info.extent.height,
                        };

                        has_depth = true;
                    }

                    // check if resolve target requires a barrier
                    if (resolve_target != std::end(pass_ref.resolve_images()))
                    {
                        auto resolve_img = _device->access_image(resolve_target->dst);

                        // fetch prior usages
                        auto prior_usage = _last_known_state.images.find(resolve_target->dst.as_uint64());

                        VkImageAspectFlagBits aspect_mask = img.usage == image_resource_usage::COLOR_ATTACHMENT
                                                                ? VK_IMAGE_ASPECT_COLOR_BIT
                                                                : VK_IMAGE_ASPECT_DEPTH_BIT;

                        auto src_stage_mask = prior_usage == _last_known_state.images.end()
                                                  ? VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
                                                  : prior_usage->second.stage_mask;

                        auto dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

                        VkImageMemoryBarrier2 resolve_barrier = {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                            .pNext = nullptr,
                            .srcStageMask = static_cast<VkPipelineStageFlags2>(src_stage_mask),
                            .srcAccessMask = prior_usage == _last_known_state.images.end()
                                                 ? VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
                                                 : prior_usage->second.access_mask,
                            .dstStageMask = static_cast<VkPipelineStageFlags2>(dst_stage_mask),
                            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                            .newLayout = img.usage == image_resource_usage::COLOR_ATTACHMENT
                                             ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                             : VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .image = resolve_img->image,
                            .subresourceRange{
                                .aspectMask = static_cast<VkImageAspectFlags>(aspect_mask),
                                .baseMipLevel = 0,
                                .levelCount = 1,
                                .baseArrayLayer = 0,
                                .layerCount = 1,
                            },
                        };

                        resolve_barriers.push_back(resolve_barrier);

                        // write new state for the next pass
                        _last_known_state.images[resolve_target->dst.as_uint64()] = {
                            .persistent = resolve_img->persistent,
                            .stage_mask = resolve_barrier.dstStageMask,
                            .access_mask = resolve_barrier.dstAccessMask,
                            .image_layout = resolve_barrier.newLayout,
                            .image = resolve_img->image,
                            .aspect = static_cast<VkImageAspectFlags>(aspect_mask),
                            .base_mip = 0,
                            .mip_count = 1,
                            .base_array_layer = 0,
                            .layer_count = 1,
                            .queue_family = resolve_barrier.dstQueueFamilyIndex,
                        };
                    }
                }

                if (!resolve_barriers.empty())
                {
                    VkDependencyInfo dep_info = {
                        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                        .pNext = nullptr,
                        .dependencyFlags = 0,
                        .memoryBarrierCount = 0,
                        .pMemoryBarriers = nullptr,
                        .bufferMemoryBarrierCount = 0,
                        .pBufferMemoryBarriers = nullptr,
                        .imageMemoryBarrierCount = static_cast<uint32_t>(resolve_barriers.size()),
                        .pImageMemoryBarriers = resolve_barriers.empty() ? nullptr : resolve_barriers.data(),
                    };
                    cmd_buffer_alloc.dispatch->cmdPipelineBarrier2(cmds, &dep_info);
                }

                VkRenderingInfo render_info = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .renderArea = area,
                    .layerCount = 1,
                    .viewMask = 0,
                    .colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
                    .pColorAttachments = color_attachments.empty() ? nullptr : color_attachments.data(),
                    .pDepthAttachment = has_depth ? &depth_attachment : nullptr,
                    .pStencilAttachment = nullptr,
                };

                dispatch->cmdBeginRendering(cmds, &render_info);

                if (pass_ref.should_draw_imgui())
                {
                    _imgui_ctx->init_info.ColorAttachmentFormat = first_color_fmt;
                    if (!_imgui_ctx->initialized)
                    {
                        ImGui_ImplVulkan_GetBackendData()->VulkanInitInfo.ColorAttachmentFormat = first_color_fmt;
                        ImGui_ImplVulkan_CreateDeviceObjects();
                        _imgui_ctx->initialized = true;
                    }

                    ImGui_ImplVulkan_NewFrame();
                    ImGui::Render();
                    ImDrawData* data = ImGui::GetDrawData();
                    ImGui_ImplVulkan_RenderDrawData(data, cmds);
                }

                dispatch->cmdSetRasterizationSamplesEXT(cmds, samples);
            }

            std::size_t pass_idx = _pass_index_map[pass_ref.handle().as_uint64()];
            auto& set_frame_state =
                _descriptor_set_states[pass_idx]
                    .per_frame_descriptors[_device->frame_in_flight() % _device->frames_in_flight()];

            if (!_descriptor_set_states[pass_idx].set_layouts.empty())
            {
                VkPipelineBindPoint bind_point = pass_ref.operation_type() == queue_operation_type::GRAPHICS
                                                     ? VK_PIPELINE_BIND_POINT_GRAPHICS
                                                     : VK_PIPELINE_BIND_POINT_COMPUTE;

                dispatch->cmdBindDescriptorSets(
                    cmds, bind_point, _descriptor_set_states[pass_idx].layout, 0,
                    static_cast<uint32_t>(_descriptor_set_states[pass_idx].set_layouts.size()),
                    set_frame_state.descriptor_sets.data(),
                    static_cast<uint32_t>(set_frame_state.dynamic_offsets.size()),
                    set_frame_state.dynamic_offsets.empty() ? nullptr : set_frame_state.dynamic_offsets.data());
            }

            pass_ref.execute(cmds);

            if (pass_ref.operation_type() == queue_operation_type::GRAPHICS)
            {
                dispatch->cmdEndRendering(cmds);
            }

            end_marked_region(_device->dispatch(), cmds);

            if (gpu_profile_enabled)
            {
                auto& pools = _gpu_profile_state->recording_state.pools[pass_idx];
                auto end_timestamp_query_index = _device->frame_in_flight() * 2 + 1;

                // End timestamp query
                cmd_buffer_alloc.dispatch->cmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                             pools.timestamp_queries,
                                                             static_cast<uint32_t>(end_timestamp_query_index));
            }

            auto end_time = std::chrono::high_resolution_clock::now();

            if (gpu_profile_enabled)
            {
                // Convert start and end time to nanoseconds
                auto start_ns =
                    std::chrono::time_point_cast<std::chrono::nanoseconds>(start_time).time_since_epoch().count();
                auto end_ns =
                    std::chrono::time_point_cast<std::chrono::nanoseconds>(end_time).time_since_epoch().count();

                auto& pools = _gpu_profile_state->recording_state.pools[pass_idx];
                pools.cpu_timestamp = {
                    .begin_timestamp = static_cast<uint64_t>(start_ns),
                    .end_timestamp = static_cast<uint64_t>(end_ns),
                };
            }
        }

        std::vector<VkImageMemoryBarrier2> transition_to_present;
        transition_to_present.reserve(_last_known_state.swapchain.size());

        for (auto [_, state] : _last_known_state.swapchain)
        {
            auto swapchain = _device->access_swapchain(state.swapchain);

            VkImageMemoryBarrier2 barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = state.stage_mask,
                .srcAccessMask = state.access_mask,
                .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .dstAccessMask = VK_ACCESS_NONE,
                .oldLayout = state.image_layout,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex = queue.queue_family_index,
                .dstQueueFamilyIndex = queue.queue_family_index,
                .image = _device->access_image(swapchain->image_handles[swapchain->image_index])->image,
                .subresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };

            transition_to_present.push_back(std::move(barrier));

            _last_known_state.swapchain[state.swapchain.as_uint64()] = {
                .swapchain = state.swapchain,
                .image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .access_mask = VK_ACCESS_NONE,
            };
        }

        if (!transition_to_present.empty())
        {
            VkDependencyInfo dep_info = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext = nullptr,
                .dependencyFlags = 0,
                .memoryBarrierCount = 0,
                .pMemoryBarriers = nullptr,
                .bufferMemoryBarrierCount = 0,
                .pBufferMemoryBarriers = nullptr,
                .imageMemoryBarrierCount = static_cast<uint32_t>(transition_to_present.size()),
                .pImageMemoryBarriers = transition_to_present.empty() ? nullptr : transition_to_present.data(),
            };

            cmd_buffer_alloc.dispatch->cmdPipelineBarrier2(cmds, &dep_info);
        }

        cmd_buffer_alloc.dispatch->endCommandBuffer(cmds);

        VkCommandBuffer to_submit = cmds;

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = static_cast<uint32_t>(image_acquired_sems.size()),
            .pWaitSemaphores = image_acquired_sems.data(),
            .pWaitDstStageMask = wait_stages.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &to_submit,
            .signalSemaphoreCount = static_cast<uint32_t>(render_complete_sems.size()),
            .pSignalSemaphores = render_complete_sems.data(),
        };

        if (gpu_profile_enabled)
        {
            auto time_before_submit = std::chrono::high_resolution_clock::now();
            auto time_before_submit_ns =
                std::chrono::time_point_cast<std::chrono::nanoseconds>(time_before_submit).time_since_epoch().count();
            _gpu_profile_state->recording_state.submit_cpu_timestamp.begin_timestamp = time_before_submit_ns;

            dispatch->queueSubmit(queue.queue, 1, &submit_info, commands_complete);

            auto time_after_submit = std::chrono::high_resolution_clock::now();
            auto time_after_submit_ns =
                std::chrono::time_point_cast<std::chrono::nanoseconds>(time_after_submit).time_since_epoch().count();
            _gpu_profile_state->recording_state.submit_cpu_timestamp.end_timestamp = time_after_submit_ns;
        }
        else
        {
            dispatch->queueSubmit(queue.queue, 1, &submit_info, commands_complete);
        }

        std::vector<VkResult> results(swapchains.size(), VK_SUCCESS);
        VkPresentInfoKHR present = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = submit_info.signalSemaphoreCount,
            .pWaitSemaphores = submit_info.pSignalSemaphores,
            .swapchainCount = static_cast<uint32_t>(swapchains.size()),
            .pSwapchains = swapchains.data(),
            .pImageIndices = image_indices.data(),
            .pResults = results.data(),
        };

        if (gpu_profile_enabled)
        {
            auto time_before_present = std::chrono::high_resolution_clock::now();
            auto time_before_present_ns =
                std::chrono::time_point_cast<std::chrono::nanoseconds>(time_before_present).time_since_epoch().count();
            _gpu_profile_state->recording_state.present_cpu_timestamp.begin_timestamp = time_before_present_ns;

            dispatch->queuePresentKHR(queue.queue, &present);

            auto time_after_present = std::chrono::high_resolution_clock::now();
            auto time_after_present_ns =
                std::chrono::time_point_cast<std::chrono::nanoseconds>(time_after_present).time_since_epoch().count();
            _gpu_profile_state->recording_state.present_cpu_timestamp.end_timestamp = time_after_present_ns;
        }
        else
        {
            dispatch->queuePresentKHR(queue.queue, &present);
        }

        for (std::size_t i = 0; i < results.size(); ++i)
        {
            VkResult present_result = results[i];
            if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR)
            {
                auto sc = _active_swapchain_set[i];
                _device->recreate_swapchain(sc);
            }
        }

        for (auto sem : image_acquired_sems)
        {
            _device->release_semaphore(std::move(sem));
        }

        for (auto sem : render_complete_sems)
        {
            _device->release_semaphore(std::move(sem));
        }

        // return allocator
        _device->release_frame_local_command_buffer_allocator(std::move(cmd_buffer_alloc));

        tempest::erase_if(_last_known_state.images, [](const auto& state) {
            auto [key, info] = state;
            return !info.persistent;
        });

        _last_known_state.swapchain.clear();

        _device->end_frame();

        // Copy the profiling results
        if (gpu_profile_enabled)
        {
            _gpu_profile_state->results.frame_index = _device->current_frame() - _device->frames_in_flight();
            _gpu_profile_state->results.pass_results.clear();

            auto frame_end_time = std::chrono::high_resolution_clock::now();
            auto frame_end_time_ns =
                std::chrono::time_point_cast<std::chrono::nanoseconds>(frame_end_time).time_since_epoch().count();
            _gpu_profile_state->recording_state.full_frame_cpu_timestamp.end_timestamp = frame_end_time_ns;

            for (auto& pools : _gpu_profile_state->recording_state.pools)
            {
                gpu_profile_pass_results pass_results = {
                    .pass = pools.pass,
                    .pipeline_stats = pools.pipeline_stats,
                    .timestamp =
                        {
                            .begin_timestamp = pools.timestamp.begin_timestamp,
                            .end_timestamp = pools.timestamp.end_timestamp,
                        },
                    .cpu_timestamp =
                        {
                            .begin_timestamp = pools.cpu_timestamp.begin_timestamp,
                            .end_timestamp = pools.cpu_timestamp.end_timestamp,
                        },
                };

                _gpu_profile_state->results.pass_results.push_back(pass_results);
            }

            // Copy over the CPU timestamps
            _gpu_profile_state->results.submit_cpu_timestamp = _gpu_profile_state->recording_state.submit_cpu_timestamp;
            _gpu_profile_state->results.present_cpu_timestamp =
                _gpu_profile_state->recording_state.present_cpu_timestamp;
            _gpu_profile_state->results.full_frame_cpu_timestamp =
                _gpu_profile_state->recording_state.full_frame_cpu_timestamp;
            _gpu_profile_state->results.image_acquire_cpu_timestamp =
                _gpu_profile_state->recording_state.image_acquire_cpu_timestamp;
        }
    }

    void render_graph::show_gpu_profiling() const
    {
        if (_gpu_profile_state)
        {
            imgui_context::create_window("Render Graph Profile", [this]() {
                auto frame = _gpu_profile_state->results.frame_index;

                ImGui::Text("Frame: %zu", frame);
                ImGui::Text("Time to Record: %.2f ms",
                            (_gpu_profile_state->results.full_frame_cpu_timestamp.end_timestamp -
                             _gpu_profile_state->results.full_frame_cpu_timestamp.begin_timestamp) /
                                1000000.0f);

                for (auto pass_result : _gpu_profile_state->results.pass_results)
                {
                    auto pass_index = _pass_index_map.find(pass_result.pass.as_uint64())->second;
                    auto& pass = _all_passes[pass_index];
                    auto did_exec = _active_passes[pass_index];
                    if (!did_exec)
                    {
                        continue;
                    }
                    auto pass_name = pass.name();

                    auto gpu_begin_timestamp = pass_result.timestamp.begin_timestamp;
                    auto gpu_end_timestamp = pass_result.timestamp.end_timestamp;

                    if (gpu_begin_timestamp == 0 || gpu_end_timestamp == 0)
                    {
                        ImGui::Text("Pass timings not available for pass.");
                        continue;
                    }

                    auto gpu_pass_duration_ns =
                        (gpu_end_timestamp - gpu_begin_timestamp) * _gpu_profile_state->timestamp_period;
                    auto gpu_pass_duration_ms = gpu_pass_duration_ns / 1000000.0f;

                    auto cpu_pass_duration_ns =
                        pass_result.cpu_timestamp.end_timestamp - pass_result.cpu_timestamp.begin_timestamp;
                    auto cpu_pass_duration_ms = cpu_pass_duration_ns / 1000000.0f;

                    if (ImGui::TreeNode(pass_name.data()))
                    {
                        // Print Pass Type
                        ImGui::Text("Pass Type: %s",
                                    pass.operation_type() == queue_operation_type::GRAPHICS  ? "Graphics"
                                    : pass.operation_type() == queue_operation_type::COMPUTE ? "Compute"
                                                                                             : "Transfer");

                        ImGui::Text("CPU Duration: %.2f ms", cpu_pass_duration_ms);
                        ImGui::Text("GPU Duration: %.2f ms", gpu_pass_duration_ms);
                        ImGui::TreePop();
                    }
                }

                if (ImGui::TreeNode("Miscellaneous Timings"))
                {
                    auto submit_begin_timestamp = _gpu_profile_state->results.submit_cpu_timestamp.begin_timestamp;
                    auto submit_end_timestamp = _gpu_profile_state->results.submit_cpu_timestamp.end_timestamp;

                    auto submit_duration_ns = submit_end_timestamp - submit_begin_timestamp;
                    auto submit_duration_ms = submit_duration_ns / 1000000.0f;

                    auto present_begin_timestamp = _gpu_profile_state->results.present_cpu_timestamp.begin_timestamp;
                    auto present_end_timestamp = _gpu_profile_state->results.present_cpu_timestamp.end_timestamp;

                    auto present_duration_ns = present_end_timestamp - present_begin_timestamp;
                    auto present_duration_ms = present_duration_ns / 1000000.0f;

                    auto acquire_begin_timestamp =
                        _gpu_profile_state->results.image_acquire_cpu_timestamp.begin_timestamp;
                    auto acquire_end_timestamp = _gpu_profile_state->results.image_acquire_cpu_timestamp.end_timestamp;

                    auto acquire_duration_ns = acquire_end_timestamp - acquire_begin_timestamp;
                    auto acquire_duration_ms = acquire_duration_ns / 1000000.0f;

                    ImGui::Text("Swapchain Image Acquire Duration: %.2f ms (Count - %zu)", acquire_duration_ms,
                                _active_swapchain_set.size());
                    ImGui::Text("Submit Duration: %.2f ms", submit_duration_ms);
                    ImGui::Text("Present Duration: %.2f ms", present_duration_ms);

                    ImGui::TreePop();
                }
            });
        }
    }

    void render_graph::build_descriptor_sets()
    {
        std::size_t set_count{0};
        VkDescriptorPoolSize sizes[11] = {}; // VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1
        for (std::size_t i = 0; i < 11; ++i)
        {
            sizes[i] = {
                .type = static_cast<VkDescriptorType>(i),
                .descriptorCount = 0,
            };
        }

        for (const auto& pass : _all_passes)
        {
            std::unordered_set<uint32_t> sets;

            for (const auto& buffer : pass.buffer_usage())
            {
                auto vk_buf = _device->access_buffer(buffer.buf);

                switch (buffer.usage)
                {
                case buffer_resource_usage::CONSTANT: {
                    if (vk_buf->per_frame_resource)
                    {
                        sizes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC].descriptorCount++;
                    }
                    else
                    {
                        sizes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER].descriptorCount++;
                    }
                    break;
                }
                case buffer_resource_usage::STRUCTURED: {
                    if (vk_buf->per_frame_resource)
                    {
                        sizes[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC].descriptorCount++;
                    }
                    else
                    {
                        sizes[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER].descriptorCount++;
                    }
                    break;
                }
                default:
                    continue;
                }

                sets.insert(buffer.set);
            }

            for (const auto& img : pass.image_usage())
            {
                switch (img.usage)
                {
                case image_resource_usage::SAMPLED: {
                    sizes[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE].descriptorCount++;
                    break;
                }
                case image_resource_usage::STORAGE: {
                    sizes[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE].descriptorCount++;
                    break;
                }
                default:
                    continue;
                }

                sets.insert(img.set);
            }

            for (const auto& external_img : pass.external_images())
            {
                if (external_img.usage == image_resource_usage::SAMPLED)
                {
                    sizes[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE].descriptorCount += external_img.count;
                }
                else if (external_img.usage == image_resource_usage::STORAGE)
                {
                    sizes[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE].descriptorCount += external_img.count;
                }

                sets.insert(external_img.set);
            }

            for (const auto& external_smp : pass.external_samplers())
            {
                sizes[VK_DESCRIPTOR_TYPE_SAMPLER].descriptorCount +=
                    static_cast<uint32_t>(external_smp.samplers.size());
            }

            set_count += sets.size();
        }

        auto first_not_sized = std::partition(std::begin(sizes), std::end(sizes),
                                              [](VkDescriptorPoolSize sz) { return sz.descriptorCount > 0; });
        auto pool_size_count = std::distance(std::begin(sizes), first_not_sized);

        VkDescriptorPoolCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .maxSets = static_cast<uint32_t>(set_count),
            .poolSizeCount = static_cast<uint32_t>(pool_size_count),
            .pPoolSizes = sizes,
        };

        if (ci.maxSets == 0)
        {
            return;
        }

        for (auto& frame : _per_frame)
        {
            _device->dispatch().createDescriptorPool(&ci, nullptr, &frame.desc_pool);
        }

        std::size_t pass_index{0};

        for (auto& pass : _all_passes)
        {
            std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> bindings;
            std::map<uint32_t, std::vector<VkDescriptorBindingFlags>> binding_flags;
            std::map<uint32_t, std::vector<VkWriteDescriptorSet>> binding_writes;

            for (auto& buffer : pass.buffer_usage())
            {
                auto vk_buf = _device->access_buffer(buffer.buf);
                auto type = get_descriptor_type(buffer.usage, vk_buf->per_frame_resource);
                if (type == VK_DESCRIPTOR_TYPE_MAX_ENUM)
                {
                    continue;
                }

                bindings[buffer.set].push_back(VkDescriptorSetLayoutBinding{
                    .binding = buffer.binding,
                    .descriptorType = type,
                    .descriptorCount = 1,
                    .stageFlags = compute_accessible_stages(pass.operation_type()),
                    .pImmutableSamplers = nullptr,
                });

                binding_flags[buffer.set].push_back(0);

                auto buf = _device->access_buffer(buffer.buf);
                auto buffer_size = vk_buf->per_frame_resource ? buf->alloc_info.size / _device->frames_in_flight()
                                                              : buf->alloc_info.size;

                binding_writes[buffer.set].push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = VK_NULL_HANDLE,
                    .dstBinding = buffer.binding,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = type,
                    .pImageInfo = nullptr,
                    .pBufferInfo =
                        new VkDescriptorBufferInfo{
                            .buffer = buf->vk_buffer,
                            .offset = 0,
                            .range = buffer_size,
                        },
                    .pTexelBufferView = nullptr,
                });
            }

            for (auto& img : pass.image_usage())
            {
                auto type = get_descriptor_type(img.usage);
                if (type == VK_DESCRIPTOR_TYPE_MAX_ENUM)
                {
                    continue;
                }

                bindings[img.set].push_back(VkDescriptorSetLayoutBinding{
                    .binding = img.binding,
                    .descriptorType = type,
                    .descriptorCount = static_cast<uint32_t>(img.handles.size()),
                    .stageFlags = compute_accessible_stages(pass.operation_type()),
                    .pImmutableSamplers = nullptr,
                });

                auto img_count = static_cast<uint32_t>(img.handles.size());
                auto images = new VkDescriptorImageInfo[img_count];

                for (uint32_t i = 0; i < img_count; ++i)
                {
                    auto vk_img = _device->access_image(img.handles[i]);
                    images[i] = {
                        .sampler = VK_NULL_HANDLE,
                        .imageView = vk_img->view,
                        .imageLayout = compute_layout(img.usage),
                    };
                }

                binding_writes[img.set].push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = VK_NULL_HANDLE,
                    .dstBinding = img.binding,
                    .dstArrayElement = 0,
                    .descriptorCount = img_count,
                    .descriptorType = type,
                    .pImageInfo = images,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr,
                });

                binding_flags[img.set].push_back(0);
            }

            for (auto& img : pass.external_images())
            {
                auto type = get_descriptor_type(img.usage);
                if (type == VK_DESCRIPTOR_TYPE_MAX_ENUM)
                {
                    continue;
                }

                auto img_count = static_cast<uint32_t>(img.images.size());

                bindings[img.set].push_back(VkDescriptorSetLayoutBinding{
                    .binding = img.binding,
                    .descriptorType = type,
                    .descriptorCount = img.count,
                    .stageFlags = compute_accessible_stages(pass.operation_type()),
                    .pImmutableSamplers = nullptr,
                });

                auto images = new VkDescriptorImageInfo[img_count];

                for (uint32_t i = 0; i < static_cast<uint32_t>(img.images.size()); ++i)
                {
                    VkImageView view = VK_NULL_HANDLE;
                    if (img.images[i])
                    {
                        view = _device->access_image(img.images[i])->view;
                        images[i] = {
                            .sampler = VK_NULL_HANDLE,
                            .imageView = view,
                            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        };
                    }
                }

                binding_writes[img.set].push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = VK_NULL_HANDLE,
                    .dstBinding = img.binding,
                    .dstArrayElement = 0,
                    .descriptorCount = static_cast<uint32_t>(img.images.size()),
                    .descriptorType = type,
                    .pImageInfo = images,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr,
                });

                binding_flags[img.set].push_back(img.count > 1 ? VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT : 0);
            }

            for (auto& smp : pass.external_samplers())
            {
                bindings[smp.set].push_back(VkDescriptorSetLayoutBinding{
                    .binding = smp.binding,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = compute_accessible_stages(pass.operation_type()),
                    .pImmutableSamplers = nullptr,
                });

                auto sampler_count = smp.samplers.size();
                auto samplers = new VkDescriptorImageInfo[sampler_count];

                binding_writes[smp.set].push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = VK_NULL_HANDLE,
                    .dstBinding = smp.binding,
                    .dstArrayElement = 0,
                    .descriptorCount = static_cast<uint32_t>(sampler_count),
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .pImageInfo = samplers,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr,
                });

                for (std::size_t i = 0; i < sampler_count; ++i)
                {
                    samplers[i] = {
                        .sampler = _device->access_sampler(smp.samplers[i])->vk_sampler,
                        .imageView = VK_NULL_HANDLE,
                        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    };
                }

                binding_flags[smp.set].push_back(sampler_count > 1 ? VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT : 0);
            }

            if (bindings.empty())
            {
                ++pass_index;
                continue;
            }

            vector<VkDescriptorSetLayout> set_layouts;
            for (auto& [id, binding_arr] : bindings)
            {
                auto& bind_flags = binding_flags[id];

                VkDescriptorSetLayoutBindingFlagsCreateInfo binding_ci = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                    .pNext = nullptr,
                    .bindingCount = static_cast<uint32_t>(bind_flags.size()),
                    .pBindingFlags = bind_flags.empty() ? nullptr : bind_flags.data(),
                };

                VkDescriptorSetLayoutCreateInfo layout_ci = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                    .pNext = &binding_ci,
                    .flags = 0,
                    .bindingCount = static_cast<uint32_t>(binding_arr.size()),
                    .pBindings = binding_arr.data(),
                };

                VkDescriptorSetLayout layout;
                [[maybe_unused]] auto result =
                    _device->dispatch().createDescriptorSetLayout(&layout_ci, nullptr, &layout);
                assert(result == VK_SUCCESS);

                set_layouts.push_back(layout);
            }

            VkPushConstantRange push_constant_range = {
                .stageFlags = compute_accessible_stages(pass.operation_type()),
                .offset = 0,
                .size = static_cast<uint32_t>(pass.push_constant_range_size()),
            };

            VkPipelineLayoutCreateInfo pipeline_layout_ci = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
                .pSetLayouts = set_layouts.data(),
                .pushConstantRangeCount = push_constant_range.size > 0 ? 1u : 0u,
                .pPushConstantRanges = push_constant_range.size > 0 ? &push_constant_range : nullptr,
            };

            VkPipelineLayout layout;
            [[maybe_unused]] auto result = _device->dispatch().createPipelineLayout(&pipeline_layout_ci, nullptr, &layout);
            assert(result == VK_SUCCESS);

            auto& set_state = _descriptor_set_states[pass_index];

            set_state.layout = layout;
            set_state.set_layouts = std::move(set_layouts);

            for (std::size_t i = 0; i < _device->frames_in_flight(); ++i)
            {
                VkDescriptorPool pool = _per_frame[i].desc_pool;
                VkDescriptorSetAllocateInfo alloc_info = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .pNext = nullptr,
                    .descriptorPool = pool,
                    .descriptorSetCount = static_cast<uint32_t>(set_state.set_layouts.size()),
                    .pSetLayouts = set_state.set_layouts.data(),
                };

                [[maybe_unused]] auto res = _device->dispatch().allocateDescriptorSets(
                    &alloc_info, _descriptor_set_states[pass_index].per_frame_descriptors[i].descriptor_sets.data());
                assert(res == VK_SUCCESS);

                for (auto& [set_id, writes] : binding_writes)
                {
                    for (auto& write : writes)
                    {
                        write.dstSet =
                            _descriptor_set_states[pass_index].per_frame_descriptors[i].descriptor_sets[set_id];
                        _descriptor_set_states[pass_index].writes.push_back(write);
                        _descriptor_set_states[pass_index].vk_set_to_set_index[write.dstSet] = set_id;
                    }
                }

                for (auto& write : _descriptor_set_states[pass_index].writes)
                {
                    if (write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
                        write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                    {
                        auto per_frame_size = write.pBufferInfo->range;
                        _descriptor_set_states[pass_index].per_frame_descriptors[i].dynamic_offsets.push_back(
                            static_cast<uint32_t>(per_frame_size * i));
                    }
                }

                auto writes = _descriptor_set_states[pass_index].writes;
                tempest::erase_if(writes, [](const VkWriteDescriptorSet& write) { return write.descriptorCount == 0; });

                _device->dispatch().updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0,
                                                         nullptr);

                if (i < _device->frames_in_flight() - 1)
                {
                    _descriptor_set_states[pass_index].writes.clear();
                }
            }

            ++pass_index;
        }
    }

    render_graph_compiler::render_graph_compiler(abstract_allocator* alloc, graphics::render_device* device)
        : graphics::render_graph_compiler(alloc, device)

    {
    }

    unique_ptr<graphics::render_graph> render_graph_compiler::compile() &&
    {
        _resource_lib->compile();

        return tempest::make_unique<vk::render_graph>(
            _alloc, static_cast<render_device*>(_device), _builders,
            unique_ptr<vk::render_graph_resource_library>(
                static_cast<vk::render_graph_resource_library*>(_resource_lib.release())),
            _imgui_enabled, _gpu_profiling_enabled);
    }
} // namespace tempest::graphics::vk