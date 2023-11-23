#ifndef tempest_graphics_vk_render_graph_hpp
#define tempest_graphics_vk_render_graph_hpp

#include "vk_render_device.hpp"

#include <tempest/memory.hpp>
#include <tempest/object_pool.hpp>
#include <tempest/render_graph.hpp>

#include <vulkan/vulkan.h>

#include <array>
#include <bitset>

namespace tempest::graphics::vk
{
    class render_device;
    class render_graph_resource_library;

    struct render_graph_buffer_state
    {
        VkPipelineStageFlags stage_mask;
        VkAccessFlags access_mask;
        VkBuffer buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
        std::uint32_t queue_family;
    };

    struct render_graph_image_state
    {
        VkPipelineStageFlags stage_mask;
        VkAccessFlags access_mask;
        VkImageLayout image_layout;
        VkImage image;
        VkImageAspectFlags aspect;
        std::uint32_t base_mip;
        std::uint32_t mip_count;
        std::uint32_t base_array_layer;
        std::uint32_t layer_count;
        std::uint32_t queue_family;
    };

    struct swapchain_resource_state
    {
        swapchain_resource_handle swapchain;
        VkImageLayout image_layout;
        VkPipelineStageFlags stage_mask;
        VkAccessFlags access_mask;
    };

    struct per_frame_data
    {
        VkFence commands_complete;
    };

    struct render_graph_resource_state
    {
        std::unordered_map<std::uint64_t, render_graph_buffer_state> buffers;
        std::unordered_map<std::uint64_t, render_graph_image_state> images;
        std::unordered_map<std::uint64_t, swapchain_resource_state> swapchain;
    };

    class render_graph : public graphics::render_graph
    {
      public:
        explicit render_graph(core::allocator* alloc, render_device* device,
                              std::span<graphics::graph_pass_builder> pass_builders,
                              std::unique_ptr<render_graph_resource_library>&& resources);
        ~render_graph() override;
        void execute() override;

      private:
        using pass_active_mask = std::bitset<1024>;

        std::unique_ptr<render_graph_resource_library> _resource_lib;

        std::vector<per_frame_data> _per_frame;
        std::vector<graph_pass_builder> _all_passes;

        pass_active_mask _active_passes;
        std::vector<std::reference_wrapper<graph_pass_builder>> _active_pass_set;
        std::vector<swapchain_resource_handle> _active_swapchain_set;

        core::allocator* _alloc;
        render_device* _device;

        render_graph_resource_state _last_known_state;
    };

    class render_graph_resource_library : public graphics::render_graph_resource_library
    {
      public:
        explicit render_graph_resource_library(core::allocator* alloc, render_device* device);
        ~render_graph_resource_library();

        image_resource_handle find_texture(std::string_view name) override;
        image_resource_handle load(const image_desc& desc) override;
        void add_image_usage(image_resource_handle handle, image_resource_usage usage) override;

        buffer_resource_handle find_buffer(std::string_view name) override;
        buffer_resource_handle load(const buffer_desc& desc) override;
        void add_buffer_usage(buffer_resource_handle handle, buffer_resource_usage usage) override;

        bool compile() override;

      private:
        render_device* _device;

        struct deferred_image_create_info
        {
            image_create_info info;
            image_resource_handle allocation;
        };

        struct deferred_buffer_create_info
        {
            buffer_create_info info;
            buffer_resource_handle allocation;
        };

        std::vector<deferred_image_create_info> _images_to_compile;
        std::vector<image_resource_handle> _compiled_images;

        std::vector<deferred_buffer_create_info> _buffers_to_compile;
        std::vector<buffer_resource_handle> _compiled_buffers;
    };

    class render_graph_compiler : public graphics::render_graph_compiler
    {
      public:
        explicit render_graph_compiler(core::allocator* alloc, graphics::render_device* device);
        std::unique_ptr<graphics::render_graph> compile() && override;
    };
} // namespace tempest::graphics::vk

#endif // tempest_graphics_vk_render_graph_hpp