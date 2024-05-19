#ifndef tempest_graphics_vk_render_graph_hpp
#define tempest_graphics_vk_render_graph_hpp

#include "vk_render_device.hpp"

#include <tempest/memory.hpp>
#include <tempest/object_pool.hpp>
#include <tempest/render_graph.hpp>

#include <vulkan/vulkan.h>

#include <backends/imgui_impl_vulkan.h>

#include <array>
#include <bitset>

namespace tempest::graphics::vk
{
    class render_device;
    class render_graph_resource_library;

    struct render_graph_buffer_state
    {
        VkPipelineStageFlags2 stage_mask{0};
        VkAccessFlags2 access_mask{0};
        VkBuffer buffer{VK_NULL_HANDLE};
        VkDeviceSize offset{0};
        VkDeviceSize size{0};
        std::uint32_t queue_family{0};
    };

    struct render_graph_image_state
    {
        bool persistent{false};
        VkPipelineStageFlags2 stage_mask{0};
        VkAccessFlags2 access_mask{0};
        VkImageLayout image_layout{VK_IMAGE_LAYOUT_UNDEFINED};
        VkImage image{VK_NULL_HANDLE};
        VkImageAspectFlags aspect{0};
        std::uint32_t base_mip{0};
        std::uint32_t mip_count{0};
        std::uint32_t base_array_layer{0};
        std::uint32_t layer_count{0};
        std::uint32_t queue_family{0};
    };

    struct swapchain_resource_state
    {
        swapchain_resource_handle swapchain;
        VkImageLayout image_layout{VK_IMAGE_LAYOUT_UNDEFINED};
        VkPipelineStageFlags2 stage_mask{0};
        VkAccessFlags2 access_mask{0};
    };

    struct external_image_state
    {
        std::uint32_t count{0};
        std::uint32_t binding{0};
        std::uint32_t set{0};
        
        core::vector<VkDescriptorImageInfo> images;
    };

    struct external_sampler_state
    {
        std::uint32_t count{0};
        std::uint32_t binding{0};
        std::uint32_t set{0};

        core::vector<VkDescriptorImageInfo> samplers;
    };

    struct per_frame_data
    {
        VkDescriptorPool desc_pool{VK_NULL_HANDLE};
        VkFence commands_complete{VK_NULL_HANDLE};
    };

    struct descriptor_set_frame_state
    {
        core::vector<std::uint32_t> dynamic_offsets{};
        std::array<VkDescriptorSet, 8> descriptor_sets{};
        std::size_t last_frame_changed{0};
    };

    struct descriptor_set_state
    {
        std::unordered_map<VkDescriptorSet, std::uint32_t> vk_set_to_set_index;
        core::vector<VkDescriptorSetLayout> set_layouts;
        VkPipelineLayout layout{VK_NULL_HANDLE};
        core::vector<descriptor_set_frame_state> per_frame_descriptors;
        core::vector<VkWriteDescriptorSet> writes;
        std::size_t last_update_frame{0};
    };

    struct render_graph_resource_state
    {
        std::unordered_map<std::uint64_t, render_graph_buffer_state> buffers;
        std::unordered_map<std::uint64_t, render_graph_image_state> images;
        std::unordered_map<std::uint64_t, swapchain_resource_state> swapchain;

        core::vector<external_image_state> external_images;
        core::vector<external_sampler_state> external_samplers;
    };

    struct imgui_render_graph_context
    {
        VkInstance instance;
        VkDevice dev;
        PFN_vkGetInstanceProcAddr instance_proc_addr;
        PFN_vkGetDeviceProcAddr dev_proc_addr;

        VkDescriptorPool imgui_desc_pool;
        ImGui_ImplVulkan_InitInfo init_info;
        bool initialized{false};
    };

    class render_graph : public graphics::render_graph
    {
      public:
        explicit render_graph(core::abstract_allocator* alloc, render_device* device,
                              core::span<graphics::graph_pass_builder> pass_builders,
                              std::unique_ptr<render_graph_resource_library>&& resources, bool imgui_enabled);
        ~render_graph() override;

        void update_external_sampled_images(graph_pass_handle pass, core::span<image_resource_handle> images,
                                            std::uint32_t set,
                                   std::uint32_t binding, pipeline_stage stage) override;

        void execute() override;

      private:
        void build_descriptor_sets();

        using pass_active_mask = std::bitset<1024>;

        std::unique_ptr<render_graph_resource_library> _resource_lib;

        core::vector<per_frame_data> _per_frame;
        core::vector<graph_pass_builder> _all_passes;

        pass_active_mask _active_passes;
        core::vector<std::reference_wrapper<graph_pass_builder>> _active_pass_set;
        core::vector<swapchain_resource_handle> _active_swapchain_set;
        std::unordered_map<std::uint64_t, std::size_t> _pass_index_map;

        core::abstract_allocator* _alloc;
        render_device* _device;

        render_graph_resource_state _last_known_state;
        bool _recreated_sc_last_frame{false};

        core::vector<descriptor_set_state> _descriptor_set_states;

        std::optional<imgui_render_graph_context> _imgui_ctx;
    };

    class render_graph_resource_library : public graphics::render_graph_resource_library
    {
      public:
        explicit render_graph_resource_library(core::abstract_allocator* alloc, render_device* device);
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

        core::vector<deferred_image_create_info> _images_to_compile;
        core::vector<image_resource_handle> _compiled_images;

        core::vector<deferred_buffer_create_info> _buffers_to_compile;
        core::vector<buffer_resource_handle> _compiled_buffers;
    };

    class render_graph_compiler : public graphics::render_graph_compiler
    {
      public:
        explicit render_graph_compiler(core::abstract_allocator* alloc, graphics::render_device* device);
        std::unique_ptr<graphics::render_graph> compile() && override;
    };
} // namespace tempest::graphics::vk

#endif // tempest_graphics_vk_render_graph_hpp