#ifndef tempest_graphics_device_hpp__
#define tempest_graphics_device_hpp__

#include "command_buffer.hpp"
#include "fwd.hpp"
#include "resources.hpp"

#include <tempest/memory.hpp>
#include <tempest/object_pool.hpp>

#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include "../windowing/glfw_window.hpp" // must be below vk includes for glfw reasons

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace tempest::graphics
{
    struct window_info
    {
        glfw::window* win{nullptr};
        VkSurfaceKHR surface;
        vkb::Swapchain swapchain;
        std::vector<VkImage> images;
        std::vector<VkImageView> views;
        std::vector<VkFramebuffer> swapchain_targets;
        std::uint32_t image_index{0};
    };

    struct gfx_timestamp
    {
        std::uint32_t start;
        std::uint32_t end;
        double elapsed_time_ms;

        std::uint16_t parent_index;
        std::uint16_t depth;
        std::uint32_t color;
        std::uint32_t frame_index;

        std::string_view name;
    };

    class gfx_timestamp_manager
    {
      public:
        gfx_timestamp_manager(core::allocator* alloc, std::uint16_t query_per_frame, std::uint16_t max_frames);
        gfx_timestamp_manager(const gfx_timestamp_manager&) = delete;
        gfx_timestamp_manager(gfx_timestamp_manager&& other) noexcept;
        ~gfx_timestamp_manager();

        gfx_timestamp_manager& operator=(const gfx_timestamp_manager&) = delete;
        gfx_timestamp_manager& operator=(gfx_timestamp_manager&& rhs) noexcept;

        bool has_valid_queries() const noexcept;
        void reset();
        std::uint32_t resolve(std::uint32_t current_frame, gfx_timestamp* timestamps_to_fill);

        std::uint32_t push(std::uint32_t current_frame, std::string_view name);
        std::uint32_t pop(std::uint32_t current_frame);

        std::uint32_t queries_per_frame() const noexcept;

      private:
        void _release();

        core::allocator* _alloc{nullptr};
        gfx_timestamp* _timestamps{nullptr};
        std::uint64_t* _timestamp_data{nullptr};

        std::uint32_t _queries_per_frame{0};
        std::uint32_t _current_query{0};
        std::uint32_t _parent_index{0};
        std::uint32_t _depth{0};

        bool _is_current_frame_resolved{false};
    };

    struct gfx_device_create_info
    {
        core::allocator* global_allocator{nullptr};
        core::stack_allocator* temp_allocator{nullptr};
        glfw::window* win{nullptr};
        std::uint16_t gpu_time_queries_per_frame{32};
        bool enable_gpu_time_queries{false};
        bool enable_debug{false};
    };

    class gfx_device
    {
        friend class command_buffer;
        friend class command_buffer_ring;

        static constexpr std::size_t frames_in_flight = 3;

      public:
        explicit gfx_device(const gfx_device_create_info& info);
        gfx_device(const gfx_device&) = delete;
        gfx_device(gfx_device&&) noexcept = delete;

        ~gfx_device();

        gfx_device& operator=(const gfx_device&) = delete;
        gfx_device& operator=(gfx_device&&) noexcept = delete;

        void start_frame();
        void end_frame();

        buffer* access_buffer(buffer_handle handle);
        const buffer* access_buffer(buffer_handle handle) const;
        buffer_handle create_buffer(const buffer_create_info& ci);
        void release_buffer(buffer_handle handle);

        shader_state* access_shader_state(shader_state_handle handle);
        const shader_state* access_shader_state(shader_state_handle handle) const;
        shader_state_handle create_shader_state(const shader_state_create_info& ci);
        void release_shader_state(shader_state_handle handle);

        pipeline* access_pipeline(pipeline_handle handle);
        const pipeline* access_pipeline(pipeline_handle handle) const;
        pipeline_handle create_pipeline(const pipeline_create_info& ci);
        void release_pipeline(pipeline_handle handle);

        texture* access_texture(texture_handle handle);
        const texture* access_texture(texture_handle handle) const;
        texture_handle create_texture(const texture_create_info& ci);
        void release_texture(texture_handle handle);

        sampler* access_sampler(sampler_handle handle);
        const sampler* access_sampler(sampler_handle handle) const;
        sampler_handle create_sampler(const sampler_create_info& ci);
        void release_sampler(sampler_handle handle);

        descriptor_set_layout* access_descriptor_set_layout(descriptor_set_layout_handle handle);
        const descriptor_set_layout* access_descriptor_set_layout(descriptor_set_layout_handle handle) const;
        descriptor_set_layout_handle create_descriptor_set_layout(const descriptor_set_layout_create_info& ci);
        void release_descriptor_set_layout(descriptor_set_layout_handle handle);

        descriptor_set* access_descriptor_set(descriptor_set_handle handle);
        const descriptor_set* access_descriptor_set(descriptor_set_handle handle) const;
        descriptor_set_handle create_descriptor_set(const descriptor_set_create_info& ci);
        void release_descriptor_set(descriptor_set_handle handle);

        render_pass* access_render_pass(render_pass_handle handle);
        const render_pass* access_render_pass(render_pass_handle handle) const;
        render_pass_handle create_render_pass(const render_pass_create_info& ci);
        void release_render_pass(render_pass_handle handle);

        command_buffer& get_command_buffer(queue_type type, bool begin);
        command_buffer& get_instant_command_buffer();

        inline render_pass_attachment_info get_swapchain_attachment_info() const noexcept
        {
            return _swapchain_attachment_info;
        }

        inline render_pass_handle get_swapchain_pass() const noexcept
        {
            return _swapchain_render_pass;
        }

        void queue_command_buffer(const command_buffer& buffer);
        void execute_immediate(const command_buffer& buffer);

      private:
        vkb::Instance _instance{};
        vkb::PhysicalDevice _physical_device{};
        vkb::Device _logical_device{};
        vkb::DispatchTable _dispatch;

        VkPhysicalDeviceProperties _physical_device_properties{};
        VkAllocationCallbacks* _alloc_callbacks{};
        bool _has_debug_utils_extension;

        VkQueue _graphics_queue{};
        std::uint32_t _graphics_queue_family{};

        VkQueue _compute_queue{};
        std::uint32_t _compute_queue_family{};

        VkQueue _transfer_queue{};
        std::uint32_t _transfer_queue_family{};

        window_info _winfo{};

        core::allocator* _global_allocator;
        core::stack_allocator* _temporary_allocator;

        VmaAllocator _vma_alloc;

        std::array<VkSemaphore, frames_in_flight> _present_ready;
        std::array<VkSemaphore, frames_in_flight> _render_complete;
        std::array<VkFence, frames_in_flight> _command_buffer_complete;

        std::optional<gfx_timestamp_manager> _timestamps;
        bool _gpu_timestamp_reset{true};
        VkQueryPool _timestamp_query_pool{nullptr};

        std::size_t _current_frame{0};
        std::size_t _previous_frame{0};
        std::size_t _absolute_frame{0};

        std::vector<resource_update_desc> _deletion_queue;

        std::uint32_t _dynamic_buffer_storage_per_frame;
        buffer_handle _global_dynamic_buffer;

        core::object_pool _buffer_pool;
        core::object_pool _texture_pool;
        core::object_pool _shader_state_pool;
        core::object_pool _pipeline_pool;
        core::object_pool _render_pass_pool;
        core::object_pool _descriptor_set_layout_pool;
        core::object_pool _descriptor_set_pool;
        core::object_pool _sampler_pool;

        sampler_handle _default_sampler{.index{invalid_resource_handle}};
        render_pass_handle _swapchain_render_pass{.index{invalid_resource_handle}};
        render_pass_attachment_info _swapchain_attachment_info{};

        std::optional<command_buffer_ring> _cmd_ring;
        std::array<command_buffer, 8> _queued_commands_buffers;
        std::uint32_t _queued_command_buffer_count{0};
        VkDescriptorPool _global_desc_pool{nullptr};

        std::unordered_map<std::uint64_t, VkRenderPass>
            _render_pass_cache; // TODO: investigate a flat hash map solution

        void _advance_frame_counter() noexcept;
        void _set_resource_name(VkObjectType type, std::uint64_t handle, std::string_view name);
        void _release_resources_imm();

        void _destroy_buffer_imm(resource_handle hnd);
        void _destroy_desc_set_layout_imm(resource_handle hnd);
        void _destroy_texture_imm(resource_handle hnd);
        void _destroy_shader_state_imm(resource_handle hnd);
        void _destroy_pipeline_imm(resource_handle hnd);
        void _destroy_render_pass_imm(resource_handle hnd);
        void _destroy_desc_set_imm(resource_handle hnd);
        void _destroy_sampler_imm(resource_handle hnd);

        VkRenderPass _fetch_vk_render_pass(const render_pass_attachment_info& out, std::string_view name);
        VkRenderPass _create_vk_render_pass(const render_pass_attachment_info& out, std::string_view name);
        void _create_swapchain_pass(const render_pass_create_info& ci, render_pass* pass);
        void _create_framebuffer(render_pass* pass, std::span<texture_handle> colors, texture_handle depth_stencil);
        render_pass_attachment_info _fill_render_pass_attachment_info(const render_pass_create_info& ci);

        void _recreate_swapchain();
        void _destroy_swapchain_resources();

        void _fill_write_descriptor_sets(const descriptor_set_layout* desc_set_layout, VkDescriptorSet vk_desc_set,
                                         std::span<VkWriteDescriptorSet> desc_write,
                                         std::span<VkDescriptorBufferInfo> buf_info,
                                         std::span<VkDescriptorImageInfo> img_info, std::uint32_t& resource_count,
                                         std::span<const resource_handle> resources,
                                         std::span<const sampler_handle> samplers,
                                         std::span<const std::uint16_t> bindings);
    };
} // namespace tempest::graphics

#endif // tempest_graphics_device_hpp__
