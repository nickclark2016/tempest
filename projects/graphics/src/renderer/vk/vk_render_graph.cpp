#include "vk_render_graph.hpp"

#include <tempest/logger.hpp>

#include <algorithm>
#include <cassert>
#include <map>
#include <ranges>
#include <unordered_set>

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
            }
            logger->critical("Failed to compute expected image layout.");
            std::exit(EXIT_FAILURE);
        }

        VkPipelineStageFlags compute_image_stage_access(resource_access_type type, image_resource_usage usage,
                                                        pipeline_stage stage)
        {
            switch (usage)
            {
            case image_resource_usage::COLOR_ATTACHMENT: {
                switch (type)
                {
                case resource_access_type::READ_WRITE:
                    [[fallthrough]];
                case resource_access_type::READ:
                    return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                case resource_access_type::WRITE:
                    return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                }
                break;
            }
            case image_resource_usage::DEPTH_ATTACHMENT:
                return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            case image_resource_usage::SAMPLED:
                return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            case image_resource_usage::STORAGE: {
                switch (stage)
                {
                case pipeline_stage::COMPUTE:
                    return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                case pipeline_stage::FRAGMENT:
                    return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                }
                break;
            }
            case image_resource_usage::TRANSFER_SOURCE:
                [[fallthrough]];
            case image_resource_usage::TRANSFER_DESTINATION:
                return VK_PIPELINE_STAGE_TRANSFER_BIT;
            case image_resource_usage::PRESENT:
                return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            }

            logger->critical("Failed to determine VkPipelineStageFlags for image access.");
            std::exit(EXIT_FAILURE);
        }

        VkPipelineStageFlags compute_buffer_stage_access(resource_access_type type, buffer_resource_usage usage,
                                                         queue_operation_type ops)
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
                    return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                case queue_operation_type::COMPUTE:
                    [[fallthrough]];
                case queue_operation_type::COMPUTE_AND_TRANSFER:
                    return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                }
                break;
            }
            case buffer_resource_usage::VERTEX:
                [[fallthrough]];
            case buffer_resource_usage::INDEX:
                return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            case buffer_resource_usage::INDIRECT_ARGUMENT:
                return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
            case buffer_resource_usage::TRANSFER_DESTINATION:
                [[fallthrough]];
            case buffer_resource_usage::TRANSFER_SOURCE:
                return VK_PIPELINE_STAGE_TRANSFER_BIT;
            }

            logger->critical("Failed to determine VkPipelineStageFlags for buffer access.");
            std::exit(EXIT_FAILURE);
        }

        VkAccessFlags compute_image_access_mask(resource_access_type type, image_resource_usage usage,
                                                queue_operation_type ops)
        {
            switch (usage)
            {
            case image_resource_usage::COLOR_ATTACHMENT: {
                switch (type)
                {
                case resource_access_type::READ:
                    return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                case resource_access_type::WRITE:
                    return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                case resource_access_type::READ_WRITE:
                    return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                }
                break;
            }
            case image_resource_usage::DEPTH_ATTACHMENT: {
                switch (type)
                {
                case resource_access_type::READ:
                    return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                case resource_access_type::WRITE:
                    return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                case resource_access_type::READ_WRITE:
                    return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }
                break;
            }
            case image_resource_usage::SAMPLED:
                return VK_ACCESS_SHADER_READ_BIT;
            case image_resource_usage::STORAGE: {
                switch (type)
                {
                case resource_access_type::READ:
                    return VK_ACCESS_SHADER_READ_BIT;
                case resource_access_type::WRITE:
                    return VK_ACCESS_SHADER_WRITE_BIT;
                case resource_access_type::READ_WRITE:
                    return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                }
                break;
            }
            case image_resource_usage::TRANSFER_DESTINATION:
                return VK_ACCESS_TRANSFER_WRITE_BIT;
            case image_resource_usage::TRANSFER_SOURCE:
                return VK_ACCESS_TRANSFER_READ_BIT;
            case image_resource_usage::PRESENT:
                return VK_ACCESS_NONE;
            }

            logger->critical("Failed to determine VkAccessFlags for image access.");
            std::exit(EXIT_FAILURE);
        }

        VkAccessFlags compute_buffer_access_mask(resource_access_type type, buffer_resource_usage usage)
        {
            switch (usage)
            {
            case buffer_resource_usage::STRUCTURED: {
                switch (type)
                {
                case resource_access_type::READ:
                    return VK_ACCESS_SHADER_READ_BIT;
                case resource_access_type::WRITE:
                    return VK_ACCESS_SHADER_WRITE_BIT;
                case resource_access_type::READ_WRITE:
                    return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                }
                break;
            }
            case buffer_resource_usage::CONSTANT:
                return VK_ACCESS_SHADER_READ_BIT;
            case buffer_resource_usage::VERTEX:
                return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            case buffer_resource_usage::INDEX:
                return VK_ACCESS_INDEX_READ_BIT;
            case buffer_resource_usage::INDIRECT_ARGUMENT:
                return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            case buffer_resource_usage::TRANSFER_DESTINATION:
                return VK_ACCESS_TRANSFER_WRITE_BIT;
            case buffer_resource_usage::TRANSFER_SOURCE:
                return VK_ACCESS_TRANSFER_READ_BIT;
            }

            logger->critical("Failed to determine VkAccessFlags for buffer access.");
            std::exit(EXIT_FAILURE);
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
            }

            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    } // namespace

    render_graph_resource_library::render_graph_resource_library(core::allocator* alloc, render_device* device)
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

    image_resource_handle render_graph_resource_library::find_texture(std::string_view name)
    {
        return image_resource_handle();
    }

    image_resource_handle render_graph_resource_library::load(const image_desc& desc)
    {
        auto handle = _device->allocate_image();

        image_create_info ci = {
            .type{desc.type},
            .width{desc.width},
            .height{desc.height},
            .depth{desc.depth},
            .layers{desc.layers},
            .mip_count{1},
            .format{desc.fmt},
            .samples{desc.samples},
            .name{std::string(desc.name)},
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
            }
        }
    }

    buffer_resource_handle render_graph_resource_library::find_buffer(std::string_view name)
    {
        return buffer_resource_handle();
    }

    buffer_resource_handle render_graph_resource_library::load(const buffer_desc& desc)
    {
        auto mem = desc.per_frame_memory;
        auto handle = _device->allocate_buffer();

        auto aligned_size = (desc.size + 64 - 1) & -64;

        _buffers_to_compile.push_back(deferred_buffer_create_info{
            .info{
                .per_frame{desc.per_frame_memory},
                .loc{desc.location},
                .size{aligned_size * (desc.per_frame_memory ? _device->frames_in_flight() : 1)},
                .name{std::string(desc.name)},
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

    render_graph::render_graph(core::allocator* alloc, render_device* device,
                               std::span<graphics::graph_pass_builder> pass_builders,
                               std::unique_ptr<render_graph_resource_library>&& resources)
        : _alloc{alloc}, _device{device}, _resource_lib{std::move(resources)}
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
    }

    render_graph::~render_graph()
    {
        for (auto& write : _descriptor_set_states[0].writes)
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

        _device->idle();

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
    }

    void render_graph::execute()
    {
        _device->start_frame();

        // first, check to see if the pass states are the same
        bool active_change_detected = false;
        for (std::size_t i = 0; i < _all_passes.size(); ++i)
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
                if (!pass.should_execute())
                {
                    continue;
                }

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
                               return std::ref(*std::find_if(
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

        std::vector<VkSemaphore> image_acquired_sems;
        std::vector<VkSemaphore> render_complete_sems;
        std::vector<VkPipelineStageFlags> wait_stages;
        std::vector<VkSwapchainKHR> swapchains;
        std::vector<uint32_t> image_indices;

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
            wait_stages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            render_complete_sems.push_back(render_complete_sem);
            swapchains.push_back(swap->sc.swapchain);
            image_indices.push_back(swap->image_index);
        }

        auto queue = _device->get_queue();

        VkCommandBufferBeginInfo begin = {
            .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO},
            .pInheritanceInfo{nullptr},
        };
        cmd_buffer_alloc.dispatch->beginCommandBuffer(cmds, &begin);

        for (auto pass_ref_wrapper : _active_pass_set)
        {
            auto& pass_ref = pass_ref_wrapper.get();

            // compute the transitions we need
            std::vector<VkImageMemoryBarrier> image_barriers;
            std::vector<VkBufferMemoryBarrier> buffer_barriers;
            VkPipelineStageFlags src_stage_mask{};
            VkPipelineStageFlags dst_stage_mask{};

            for (const auto& swap : pass_ref.external_swapchain_usage())
            {
                auto swapchain_state_it = _last_known_state.swapchain.find(swap.swap.as_uint64());
                auto swapchain = _device->access_swapchain(swap.swap);
                auto vk_img = _device->access_image(swapchain->image_handles[swapchain->image_index]);

                swapchain_resource_state next_state = {
                    .swapchain{swap.swap},
                    .image_layout{compute_layout(swap.usage)},
                    .stage_mask{compute_image_stage_access(swap.type, swap.usage, swap.first_access)},
                    .access_mask{compute_image_access_mask(swap.type, swap.usage, queue_operation_type::GRAPHICS)},
                };

                VkImageMemoryBarrier barrier = {
                    .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                    .pNext{nullptr},
                    .srcAccessMask{VK_ACCESS_NONE},
                    .dstAccessMask{next_state.access_mask},
                    .oldLayout{VK_IMAGE_LAYOUT_UNDEFINED},
                    .newLayout{next_state.image_layout},
                    .srcQueueFamilyIndex{queue.queue_family_index},
                    .dstQueueFamilyIndex{queue.queue_family_index},
                    .image{vk_img->image},
                    .subresourceRange{
                        .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                        .baseMipLevel{0},
                        .levelCount{1},
                        .baseArrayLayer{0},
                        .layerCount{1},
                    },
                };

                if (swapchain_state_it != _last_known_state.swapchain.end())
                {
                    swapchain_resource_state last_state = swapchain_state_it->second;

                    barrier.oldLayout = last_state.image_layout;
                    barrier.srcAccessMask = last_state.access_mask;

                    src_stage_mask |= last_state.stage_mask;
                    dst_stage_mask |= next_state.stage_mask;
                }

                if (barrier.oldLayout != barrier.newLayout)
                {
                    image_barriers.push_back(barrier);
                }

                _last_known_state.swapchain[swap.swap.as_uint64()] = next_state;
            }

            if (dst_stage_mask == 0 && !image_barriers.empty())
            {
                dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            }

            for (const auto& img : pass_ref.image_usage())
            {
                auto image_state_it = _last_known_state.images.find(img.img.as_uint64());
                auto vk_img = _device->access_image(img.img);

                render_graph_image_state next_state = {
                    .stage_mask{compute_image_stage_access(img.type, img.usage, img.first_access)},
                    .access_mask{compute_image_access_mask(img.type, img.usage, pass_ref.operation_type())},
                    .image_layout{compute_layout(img.usage)},
                    .image{vk_img->image},
                    .aspect{vk_img->view_info.subresourceRange.aspectMask},
                    .base_mip{vk_img->view_info.subresourceRange.baseMipLevel},
                    .mip_count{vk_img->view_info.subresourceRange.levelCount},
                    .base_array_layer{vk_img->view_info.subresourceRange.baseArrayLayer},
                    .layer_count{vk_img->view_info.subresourceRange.layerCount},
                    .queue_family{queue.queue_family_index},
                };

                // populate transition
                VkImageMemoryBarrier img_barrier = {
                    .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                    .pNext{nullptr},
                    .srcAccessMask{0},
                    .dstAccessMask{next_state.access_mask},
                    .oldLayout{VK_IMAGE_LAYOUT_UNDEFINED},
                    .newLayout{next_state.image_layout},
                    .srcQueueFamilyIndex{next_state.queue_family},
                    .dstQueueFamilyIndex{next_state.queue_family},
                    .image{vk_img->image},
                    .subresourceRange{vk_img->view_info.subresourceRange},
                };

                if (image_state_it != _last_known_state.images.end()) [[likely]]
                {
                    render_graph_image_state last_state = image_state_it->second;

                    img_barrier.oldLayout = last_state.image_layout;
                    img_barrier.srcAccessMask = last_state.access_mask;
                    img_barrier.srcQueueFamilyIndex = last_state.queue_family;

                    src_stage_mask |= last_state.stage_mask;
                }

                if (img_barrier.oldLayout != img_barrier.newLayout ||
                    img_barrier.srcQueueFamilyIndex != img_barrier.dstQueueFamilyIndex)
                {
                    dst_stage_mask |= next_state.stage_mask;
                    image_barriers.push_back(img_barrier);
                }

                // write new state for the end of the frame
                _last_known_state.images[img.img.as_uint64()] = next_state;
            }

            for (const auto& buf : pass_ref.buffer_usage())
            {
                const auto buffer_state_it = _last_known_state.buffers.find(buf.buf.as_uint64());
                auto vk_buf = _device->access_buffer(buf.buf);

                auto size_per_frame = vk_buf->per_frame_resource ? vk_buf->alloc_info.size / _device->frames_in_flight()
                                                                 : vk_buf->alloc_info.size;
                auto offset = vk_buf->per_frame_resource ? size_per_frame * _device->frame_in_flight() : 0;

                render_graph_buffer_state next_state = {
                    .stage_mask{compute_buffer_stage_access(buf.type, buf.usage, pass_ref.operation_type())},
                    .access_mask{compute_buffer_access_mask(buf.type, buf.usage)},
                    .buffer{vk_buf->buffer},
                    .offset{0},
                    .size{VK_WHOLE_SIZE}, // TODO: figure out the proper mechanism to get offsets
                    .queue_family{queue.queue_family_index},

                };

                VkBufferMemoryBarrier buf_barrier = {
                    .sType{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER},
                    .pNext{nullptr},
                    .srcAccessMask{VK_ACCESS_NONE},
                    .dstAccessMask{next_state.access_mask},
                    .srcQueueFamilyIndex{queue.queue_family_index},
                    .dstQueueFamilyIndex{next_state.queue_family},
                    .buffer{vk_buf->buffer},
                    .offset{next_state.offset},
                    .size{next_state.size},
                };

                if ((next_state.access_mask & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
                {
                    dst_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
                }

                if (buffer_state_it != _last_known_state.buffers.end()) [[likely]]
                {
                    render_graph_buffer_state last_state = buffer_state_it->second;

                    buf_barrier.srcAccessMask = last_state.access_mask;
                    buf_barrier.srcQueueFamilyIndex = last_state.queue_family;

                    src_stage_mask |= last_state.stage_mask;
                }

                // if we've got a queue ownership transformation or a write access, force a barrier
                if (buf_barrier.srcQueueFamilyIndex != buf_barrier.dstQueueFamilyIndex ||
                    ((buf_barrier.srcAccessMask | buf_barrier.dstAccessMask) &
                     (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
                {
                    dst_stage_mask |= next_state.stage_mask;
                    buffer_barriers.push_back(buf_barrier);
                }

                _last_known_state.buffers[buf.buf.as_uint64()] = next_state;
            }

            if (!image_barriers.empty() || !buffer_barriers.empty())
            {
                if (src_stage_mask == 0)
                {
                    src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                }

                if (dst_stage_mask == 0)
                {
                    dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                }

                cmd_buffer_alloc.dispatch->cmdPipelineBarrier(cmds, src_stage_mask, dst_stage_mask, 0, 0, nullptr,
                                                              static_cast<std::uint32_t>(buffer_barriers.size()),
                                                              buffer_barriers.empty() ? nullptr
                                                                                      : buffer_barriers.data(),
                                                              static_cast<uint32_t>(image_barriers.size()),
                                                              image_barriers.empty() ? nullptr : image_barriers.data());
            }

            if (pass_ref.operation_type() == queue_operation_type::GRAPHICS)
            {
                VkRect2D area;
                std::vector<VkRenderingAttachmentInfo> color_attachments;
                VkRenderingAttachmentInfo depth_attachment;
                bool has_depth = false;

                for (const auto& sc : pass_ref.external_swapchain_usage())
                {
                    auto swap = _device->access_swapchain(sc.swap);
                    auto vk_img = _device->access_image(swap->image_handles[swap->image_index]);

                    if (sc.usage == image_resource_usage::COLOR_ATTACHMENT)
                    {
                        VkRenderingAttachmentInfo info = {
                            .sType{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},
                            .pNext{nullptr},
                            .imageView{vk_img->view},
                            .imageLayout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                            .resolveMode{VK_RESOLVE_MODE_NONE},
                            .resolveImageView{VK_NULL_HANDLE},
                            .resolveImageLayout{VK_IMAGE_LAYOUT_UNDEFINED},
                            .loadOp{compute_load_op(sc.load)},
                            .storeOp{compute_store_op(sc.store)},
                            .clearValue{},
                        };

                        area.offset = {
                            .x{0},
                            .y{0},
                        };

                        area.extent = {
                            .width{vk_img->img_info.extent.width},
                            .height{vk_img->img_info.extent.height},
                        };

                        color_attachments.push_back(info);
                    }
                }

                for (const auto& img : pass_ref.image_usage())
                {
                    auto vk_img = _device->access_image(img.img);

                    if (img.usage == image_resource_usage::COLOR_ATTACHMENT)
                    {
                        VkRenderingAttachmentInfo info = {
                            .sType{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},
                            .pNext{nullptr},
                            .imageView{vk_img->view},
                            .imageLayout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                            .resolveMode{VK_RESOLVE_MODE_NONE},
                            .resolveImageView{VK_NULL_HANDLE},
                            .resolveImageLayout{VK_IMAGE_LAYOUT_UNDEFINED},
                            .loadOp{compute_load_op(img.load)},
                            .storeOp{compute_store_op(img.store)},
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
                            .x{0},
                            .y{0},
                        };

                        area.extent = {
                            .width{vk_img->img_info.extent.width},
                            .height{vk_img->img_info.extent.height},
                        };

                        color_attachments.push_back(info);
                    }
                    else if (img.usage == image_resource_usage::DEPTH_ATTACHMENT)
                    {
                        depth_attachment = {
                            .sType{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},
                            .pNext{nullptr},
                            .imageView{vk_img->view},
                            .imageLayout{VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL},
                            .resolveMode{VK_RESOLVE_MODE_NONE},
                            .resolveImageView{VK_NULL_HANDLE},
                            .resolveImageLayout{VK_IMAGE_LAYOUT_UNDEFINED},
                            .loadOp{compute_load_op(img.load)},
                            .storeOp{compute_store_op(img.store)},
                            .clearValue{
                                .depthStencil{
                                    .depth{img.clear_depth},
                                },
                            },
                        };

                        has_depth = true;
                    }
                }

                VkRenderingInfo render_info = {
                    .sType{VK_STRUCTURE_TYPE_RENDERING_INFO},
                    .pNext{nullptr},
                    .renderArea{area},
                    .layerCount{1},
                    .viewMask{0},
                    .colorAttachmentCount{static_cast<uint32_t>(color_attachments.size())},
                    .pColorAttachments{color_attachments.empty() ? nullptr : color_attachments.data()},
                    .pDepthAttachment{has_depth ? &depth_attachment : nullptr},
                };

                dispatch->cmdBeginRendering(cmds, &render_info);
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
                    static_cast<std::uint32_t>(_descriptor_set_states[pass_idx].set_layouts.size()),
                    set_frame_state.descriptor_sets.data(),
                    static_cast<std::uint32_t>(set_frame_state.dynamic_offsets.size()),
                    set_frame_state.dynamic_offsets.empty() ? nullptr : set_frame_state.dynamic_offsets.data());
            }

            pass_ref.execute(cmds);

            if (pass_ref.operation_type() == queue_operation_type::GRAPHICS)
            {
                dispatch->cmdEndRendering(cmds);
            }
        }

        VkPipelineStageFlags final_transition_flags{0};
        std::vector<VkImageMemoryBarrier> transition_to_present;
        transition_to_present.reserve(_last_known_state.swapchain.size());

        for (auto [_, state] : _last_known_state.swapchain)
        {
            auto swapchain = _device->access_swapchain(state.swapchain);

            VkImageMemoryBarrier barrier = {
                .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                .pNext{nullptr},
                .srcAccessMask{state.access_mask},
                .dstAccessMask{VK_ACCESS_NONE},
                .oldLayout{state.image_layout},
                .newLayout{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
                .srcQueueFamilyIndex{queue.queue_family_index},
                .dstQueueFamilyIndex{queue.queue_family_index},
                .image{_device->access_image(swapchain->image_handles[swapchain->image_index])->image},
                .subresourceRange{
                    .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                    .baseMipLevel{0},
                    .levelCount{1},
                    .baseArrayLayer{0},
                    .layerCount{1},
                },
            };

            final_transition_flags |= state.stage_mask;

            transition_to_present.push_back(std::move(barrier));

            _last_known_state.swapchain[state.swapchain.as_uint64()] = {
                .swapchain{state.swapchain},
                .image_layout{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
                .stage_mask{VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT},
                .access_mask{VK_ACCESS_NONE},
            };
        }

        if (!transition_to_present.empty())
        {
            cmd_buffer_alloc.dispatch->cmdPipelineBarrier(
                cmds, final_transition_flags, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                static_cast<uint32_t>(transition_to_present.size()), transition_to_present.data());
        }

        cmd_buffer_alloc.dispatch->endCommandBuffer(cmds);

        VkCommandBuffer to_submit = cmds;

        VkSubmitInfo submit_info = {
            .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO},
            .pNext{nullptr},
            .waitSemaphoreCount{static_cast<uint32_t>(image_acquired_sems.size())},
            .pWaitSemaphores{image_acquired_sems.data()},
            .pWaitDstStageMask{wait_stages.data()},
            .commandBufferCount{1},
            .pCommandBuffers{&to_submit},
            .signalSemaphoreCount{static_cast<uint32_t>(render_complete_sems.size())},
            .pSignalSemaphores{render_complete_sems.data()},
        };

        dispatch->queueSubmit(queue.queue, 1, &submit_info, commands_complete);

        std::vector<VkResult> results(swapchains.size(), VK_SUCCESS);
        VkPresentInfoKHR present = {
            .sType{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR},
            .pNext{nullptr},
            .waitSemaphoreCount{submit_info.signalSemaphoreCount},
            .pWaitSemaphores{submit_info.pSignalSemaphores},
            .swapchainCount{static_cast<uint32_t>(swapchains.size())},
            .pSwapchains{swapchains.data()},
            .pImageIndices{image_indices.data()},
            .pResults{results.data()},
        };

        dispatch->queuePresentKHR(queue.queue, &present);

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

        _last_known_state.images.clear();
        _last_known_state.swapchain.clear();

        _device->end_frame();
    }

    void render_graph::build_descriptor_sets()
    {
        std::size_t set_count{0};
        VkDescriptorPoolSize sizes[11] = {}; // VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1
        for (std::size_t i = 0; i < 11; ++i)
        {
            sizes[i] = {
                .type{static_cast<VkDescriptorType>(i)},
                .descriptorCount{0},
            };
        }

        for (const auto& pass : _all_passes)
        {
            std::unordered_set<std::uint32_t> sets;

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

            for (const auto& external_img : pass.external_sampled_images())
            {
                sizes[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE].descriptorCount +=
                    static_cast<std::uint32_t>(external_img.images.size());
                sets.insert(external_img.set);
            }

            set_count += sets.size();
        }

        auto first_not_sized = std::partition(std::begin(sizes), std::end(sizes),
                                              [](VkDescriptorPoolSize sz) { return sz.descriptorCount > 0; });
        auto pool_size_count = std::distance(std::begin(sizes), first_not_sized);

        VkDescriptorPoolCreateInfo ci = {
            .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO},
            .pNext{nullptr},
            .flags{0},
            .maxSets{static_cast<std::uint32_t>(set_count)},
            .poolSizeCount{static_cast<std::uint32_t>(pool_size_count)},
            .pPoolSizes{sizes},
        };

        for (auto& frame : _per_frame)
        {
            _device->dispatch().createDescriptorPool(&ci, nullptr, &frame.desc_pool);
        }

        std::size_t pass_index{0};

        for (auto& pass : _all_passes)
        {
            std::unordered_map<std::uint32_t, std::vector<VkDescriptorSetLayoutBinding>> bindings;
            std::map<std::uint32_t, std::vector<VkWriteDescriptorSet>> binding_writes;

            for (auto& buffer : pass.buffer_usage())
            {
                auto vk_buf = _device->access_buffer(buffer.buf);
                auto type = get_descriptor_type(buffer.usage, vk_buf->per_frame_resource);
                if (type == VK_DESCRIPTOR_TYPE_MAX_ENUM)
                {
                    continue;
                }

                bindings[buffer.set].push_back(VkDescriptorSetLayoutBinding{
                    .binding{buffer.binding},
                    .descriptorType{type},
                    .descriptorCount{1},
                    .stageFlags{compute_accessible_stages(pass.operation_type())},
                    .pImmutableSamplers{nullptr},
                });

                auto buf = _device->access_buffer(buffer.buf);
                auto buffer_size = vk_buf->per_frame_resource ? buf->alloc_info.size / _device->frames_in_flight()
                                                              : buf->alloc_info.size;

                binding_writes[buffer.set].push_back(VkWriteDescriptorSet{
                    .sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                    .pNext{nullptr},
                    .dstBinding{buffer.binding},
                    .dstArrayElement{0},
                    .descriptorCount{1},
                    .descriptorType{type},
                    .pImageInfo{nullptr},
                    .pBufferInfo{
                        new VkDescriptorBufferInfo{
                            .buffer{buf->buffer},
                            .offset{0},
                            .range{buffer_size},
                        },
                    },
                    .pTexelBufferView{nullptr},
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
                    .binding{img.binding},
                    .descriptorType{type},
                    .descriptorCount{1},
                    .stageFlags{compute_accessible_stages(pass.operation_type())},
                    .pImmutableSamplers{nullptr},
                });

                auto vk_img = _device->access_image(img.img);

                binding_writes[img.set].push_back(VkWriteDescriptorSet{
                    .sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                    .pNext{nullptr},
                    .dstBinding{img.binding},
                    .dstArrayElement{0},
                    .descriptorCount{1},
                    .descriptorType{type},
                    .pImageInfo{new VkDescriptorImageInfo{
                        .sampler{VK_NULL_HANDLE},
                        .imageView{vk_img->view},
                        .imageLayout{compute_layout(img.usage)},
                    }},
                    .pBufferInfo{nullptr},
                    .pTexelBufferView{nullptr},
                });
            }

            for (auto& img : pass.external_sampled_images())
            {
                auto type = get_descriptor_type(img.usage);
                if (type == VK_DESCRIPTOR_TYPE_MAX_ENUM)
                {
                    continue;
                }

                bindings[img.set].push_back(VkDescriptorSetLayoutBinding{
                    .binding{img.binding},
                    .descriptorType{type},
                    .descriptorCount{1},
                    .stageFlags{compute_accessible_stages(pass.operation_type())},
                    .pImmutableSamplers{nullptr},
                });

                auto img_count = img.images.size();
                auto images = new VkDescriptorImageInfo[img_count];

                binding_writes[img.set].push_back(VkWriteDescriptorSet{
                    .sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                    .pNext{nullptr},
                    .dstBinding{img.binding},
                    .dstArrayElement{0},
                    .descriptorCount{static_cast<std::uint32_t>(img_count)},
                    .descriptorType{type},
                    .pImageInfo{images},
                    .pBufferInfo{nullptr},
                    .pTexelBufferView{nullptr},
                });

                for (std::size_t i = 0; i < img_count; ++i)
                {
                    images[i] = {
                        .sampler{VK_NULL_HANDLE},
                        .imageView{_device->access_image(img.images[i])->view},
                        .imageLayout{compute_layout(img.usage)},
                    };
                }
            }

            for (auto& smp : pass.external_samplers())
            {
                bindings[smp.set].push_back(VkDescriptorSetLayoutBinding{
                    .binding{smp.binding},
                    .descriptorType{VK_DESCRIPTOR_TYPE_SAMPLER},
                    .descriptorCount{1},
                    .stageFlags{compute_accessible_stages(pass.operation_type())},
                    .pImmutableSamplers{nullptr},
                });

                auto sampler_count = smp.samplers.size();
                auto samplers = new VkDescriptorImageInfo[sampler_count];

                binding_writes[smp.set].push_back(VkWriteDescriptorSet{
                    .sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                    .pNext{nullptr},
                    .dstBinding{smp.binding},
                    .dstArrayElement{0},
                    .descriptorCount{static_cast<std::uint32_t>(sampler_count)},
                    .descriptorType{VK_DESCRIPTOR_TYPE_SAMPLER},
                    .pImageInfo{samplers},
                    .pBufferInfo{nullptr},
                    .pTexelBufferView{nullptr},
                });

                for (std::size_t i = 0; i < sampler_count; ++i)
                {
                    samplers[i] = {
                        .sampler{_device->access_sampler(smp.samplers[i])->vk_sampler},
                    };
                }
            }

            if (bindings.empty())
            {
                ++pass_index;
                continue;
            }

            std::vector<VkDescriptorSetLayout> set_layouts;
            for (auto& [id, binding_arr] : bindings)
            {
                VkDescriptorSetLayoutCreateInfo layout_ci = {
                    .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO},
                    .pNext{nullptr},
                    .flags{0},
                    .bindingCount{static_cast<std::uint32_t>(binding_arr.size())},
                    .pBindings{binding_arr.data()},
                };

                VkDescriptorSetLayout layout;
                auto result = _device->dispatch().createDescriptorSetLayout(&layout_ci, nullptr, &layout);
                assert(result == VK_SUCCESS);

                set_layouts.push_back(layout);
            }

            VkPipelineLayoutCreateInfo pipeline_layout_ci = {
                .sType{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO},
                .pNext{nullptr},
                .flags{0},
                .setLayoutCount{static_cast<std::uint32_t>(set_layouts.size())},
                .pSetLayouts{set_layouts.data()},
                .pushConstantRangeCount{0},
                .pPushConstantRanges{nullptr},
            };

            VkPipelineLayout layout;
            auto result = _device->dispatch().createPipelineLayout(&pipeline_layout_ci, nullptr, &layout);
            assert(result == VK_SUCCESS);

            auto& set_state = _descriptor_set_states[pass_index];

            set_state.layout = layout;
            set_state.set_layouts = std::move(set_layouts);

            for (std::size_t i = 0; i < _device->frames_in_flight(); ++i)
            {
                VkDescriptorPool pool = _per_frame[i].desc_pool;
                VkDescriptorSetAllocateInfo alloc_info = {
                    .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO},
                    .pNext{nullptr},
                    .descriptorPool{pool},
                    .descriptorSetCount{static_cast<std::uint32_t>(set_state.set_layouts.size())},
                    .pSetLayouts{set_state.set_layouts.data()}};

                auto result = _device->dispatch().allocateDescriptorSets(
                    &alloc_info, _descriptor_set_states[pass_index].per_frame_descriptors[i].descriptor_sets.data());
                assert(result == VK_SUCCESS);

                for (auto& [set_id, writes] : binding_writes)
                {
                    for (auto& write : writes)
                    {
                        write.dstSet =
                            _descriptor_set_states[pass_index].per_frame_descriptors[i].descriptor_sets[set_id];
                        _descriptor_set_states[pass_index].writes.push_back(write);
                    }
                }

                for (auto& write : _descriptor_set_states[pass_index].writes)
                {
                    if (write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
                        write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                    {
                        auto per_frame_size = write.pBufferInfo->range;
                        _descriptor_set_states[pass_index].per_frame_descriptors[i].dynamic_offsets.push_back(
                            static_cast<std::uint32_t>(per_frame_size * i));
                    }
                }

                _device->dispatch().updateDescriptorSets(
                    static_cast<std::uint32_t>(_descriptor_set_states[pass_index].writes.size()),
                    _descriptor_set_states[pass_index].writes.data(), 0, nullptr);

                _descriptor_set_states[pass_index].writes.clear();
            }

            ++pass_index;
        }
    }

    render_graph_compiler::render_graph_compiler(core::allocator* alloc, graphics::render_device* device)
        : graphics::render_graph_compiler(alloc, device)

    {
    }

    std::unique_ptr<graphics::render_graph> render_graph_compiler::compile() &&
    {
        _resource_lib->compile();

        return std::make_unique<vk::render_graph>(
            _alloc, static_cast<render_device*>(_device), _builders,
            std::unique_ptr<vk::render_graph_resource_library>(
                static_cast<vk::render_graph_resource_library*>(_resource_lib.release())));
    }
} // namespace tempest::graphics::vk