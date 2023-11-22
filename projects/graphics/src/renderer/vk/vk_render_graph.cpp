#include "vk_render_graph.hpp"

#include <tempest/logger.hpp>

#include <algorithm>
#include <ranges>

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

        VkPipelineStageFlags compute_image_stage_access(resource_access_type type, image_resource_usage usage)
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
                return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            case image_resource_usage::SAMPLED:
                return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            case image_resource_usage::STORAGE: {
                return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }
            case image_resource_usage::TRANSFER_SOURCE:
                [[fallthrough]];
            case image_resource_usage::TRANSFER_DESTINATION:
                return VK_PIPELINE_STAGE_TRANSFER_BIT;
            case image_resource_usage::PRESENT:
                return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            }
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
            std::exit(EXIT_FAILURE);
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
        auto handle = _device->allocate_buffer();
        _buffers_to_compile.push_back(deferred_buffer_create_info{
            .info{.loc{desc.location}, .size{desc.size}, .name{std::string(desc.name)}},
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
            _all_passes.push_back(bldr);
        }
    }

    void render_graph::execute()
    {
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

        for (auto& swapchain : _active_swapchain_set)
        {
        }

        auto queue = _device->get_queue();

        // write barriers
        auto cmd_buffer_alloc = _device->acquire_frame_local_command_buffer_allocator();
        auto cmds = cmd_buffer_alloc.allocate();

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
                    .stage_mask{compute_image_stage_access(swap.type, swap.usage)},
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

            for (const auto& img : pass_ref.image_usage())
            {
                auto image_state_it = _last_known_state.images.find(img.img.as_uint64());
                auto vk_img = _device->access_image(img.img);

                render_graph_image_state next_state = {
                    .stage_mask{compute_image_stage_access(img.type, img.usage)},
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

                if (image_state_it != _last_known_state.images.end()) [[likely]]
                {
                    render_graph_image_state last_state = image_state_it->second;

                    // populate transition
                    VkImageMemoryBarrier img_barrier = {
                        .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                        .pNext{nullptr},
                        .srcAccessMask{last_state.access_mask},
                        .dstAccessMask{next_state.access_mask},
                        .oldLayout{last_state.image_layout},
                        .newLayout{next_state.image_layout},
                        .srcQueueFamilyIndex{last_state.queue_family},
                        .dstQueueFamilyIndex{next_state.queue_family},
                        .image{vk_img->image},
                        .subresourceRange{vk_img->view_info.subresourceRange},
                    };

                    if (last_state.image_layout != next_state.image_layout ||
                        last_state.queue_family != next_state.queue_family)
                    {
                        src_stage_mask |= last_state.stage_mask;
                        dst_stage_mask |= next_state.stage_mask;
                        image_barriers.push_back(img_barrier);
                    }
                }

                // write new state for the end of the frame
                _last_known_state.images[img.img.as_uint64()] = next_state;
            }

            for (const auto& buf : pass_ref.buffer_usage())
            {
                const auto buffer_state_it = _last_known_state.buffers.find(buf.buf.as_uint64());
                if (buffer_state_it != _last_known_state.buffers.end()) [[likely]]
                {
                    render_graph_buffer_state last_state = buffer_state_it->second;
                }
            }

            if (!image_barriers.empty() || !buffer_barriers.empty())
            {
                cmd_buffer_alloc.dispatch->cmdPipelineBarrier(cmds, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0,
                                                              nullptr, static_cast<uint32_t>(image_barriers.size()),
                                                              image_barriers.empty() ? nullptr : image_barriers.data());
            }

            pass_ref.execute(cmds);
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
        }

        if (!transition_to_present.empty())
        {
            cmd_buffer_alloc.dispatch->cmdPipelineBarrier(
                cmds, final_transition_flags, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                static_cast<uint32_t>(transition_to_present.size()), transition_to_present.data());
        }

        cmd_buffer_alloc.dispatch->endCommandBuffer(cmds);

        // return allocator
        _device->release_frame_local_command_buffer_allocator(std::move(cmd_buffer_alloc));
    } // namespace tempest::graphics::vk

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