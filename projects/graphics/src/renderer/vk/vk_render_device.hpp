#ifndef tempest_graphics_vk_render_device_hpp
#define tempest_graphics_vk_render_device_hpp

#include <tempest/object_pool.hpp>
#include <tempest/render_device.hpp>

#include <array>
#include <deque>
#include <functional>
#include <optional>
#include <vector>

#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include "../../windowing/glfw_window.hpp"

namespace tempest::graphics::vk
{
    class render_device;

    struct image
    {
        VmaAllocation allocation;
        VmaAllocationInfo alloc_info;
        VkImage image;
        VkImageView view;
        VkImageCreateInfo img_info;
        VkImageViewCreateInfo view_info;
        std::string name;
    };

    struct buffer
    {
        bool per_frame_resource;
        VmaAllocation allocation;
        VmaAllocationInfo alloc_info;
        VkBuffer buffer;
        VkBufferCreateInfo info;
        std::string name;
    };

    struct graphics_pipeline
    {
        VkShaderModule vertex_module;
        VkShaderModule fragment_module;
        std::vector<VkDescriptorSetLayout> set_layouts;
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
        std::string name;
    };

    struct compute_pipeline
    {
        VkShaderModule compute_module;
        std::vector<VkDescriptorSetLayout> set_layouts;
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
        std::string name;
    };

    struct swapchain
    {
        glfw::window* win;
        std::uint32_t image_index;
        vkb::Swapchain sc;
        VkSurfaceKHR surface;
        std::vector<image_resource_handle> image_handles;
    };

    struct sampler
    {
        VkSampler vk_sampler;
        VkSamplerCreateInfo info;
        std::string name;
    };

    class resource_deletion_queue
    {
      public:
        explicit resource_deletion_queue(std::size_t frames_in_flight);

        void add_to_queue(std::size_t current_frame, std::function<void()> deleter);
        void flush_frame(std::size_t current_frame);
        void flush_all();

      private:
        struct delete_info
        {
            std::size_t frame;
            std::function<void()> deleter;
        };

        std::vector<delete_info> _queue;
        std::size_t _frames_in_flight;
    };

    struct queue_info
    {
        VkQueue queue;
        std::uint32_t queue_family_index;
        std::uint32_t queue_index;
        VkQueueFlags flags;
    };

    class command_list : public graphics::command_list
    {
      public:
        command_list(VkCommandBuffer buffer, vkb::DispatchTable* dispatch, render_device* device);
        ~command_list() override = default;

        operator VkCommandBuffer() const noexcept;

        command_list& set_viewport(float x, float y, float width, float height, float min_depth = 0.0f,
                                   float max_depth = 1.0f, std::uint32_t viewport_id = 0) override;
        command_list& set_scissor_region(std::int32_t x, std::int32_t y, std::uint32_t width,
                                         std::uint32_t height) override;
        command_list& draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0,
                           std::uint32_t first_index = 0) override;
        command_list& use_pipeline(graphics_pipeline_resource_handle pipeline) override;

        command_list& blit(image_resource_handle src, image_resource_handle dst) override;
        command_list& copy(buffer_resource_handle src, buffer_resource_handle dst, std::size_t src_offset = 0,
                           std::size_t dst_offset = 0,
                           std::size_t byte_count = std::numeric_limits<std::size_t>::max()) override;
        command_list& copy(buffer_resource_handle src, image_resource_handle dst, std::size_t buffer_offset,
                           std::uint32_t region_width, std::uint32_t region_height, std::uint32_t mip_level,
                           std::int32_t offset_x = 0, std::int32_t offset_y = 0) override;

        command_list& transition_image(image_resource_handle img, image_resource_usage old_usage,
                                       image_resource_usage new_usage) override;

      private:
        VkCommandBuffer _cmds;
        vkb::DispatchTable* _dispatch;
        render_device* _device;
    };

    struct command_buffer_allocator
    {
        queue_info queue;
        VkCommandPool pool;

        std::vector<VkCommandBuffer> cached_commands;
        std::size_t command_buffer_index{0};

        vkb::DispatchTable* dispatch;
        render_device* device;

        void reset();
        command_list allocate();
        void release();
    };

    struct command_buffer_recycler
    {
        struct command_buffer_recycle_payload
        {
            command_buffer_allocator allocator;
            std::size_t recycled_frame;
        };

        std::size_t frames_in_flight;
        queue_info queue;
        std::vector<command_buffer_allocator> global_pool;
        std::deque<command_buffer_recycle_payload> recycle_pool;

        command_buffer_allocator acquire(vkb::DispatchTable& dispatch, render_device* device);
        void release(command_buffer_allocator&& allocator, std::size_t current_frame);
        void recycle(std::size_t current_frame, vkb::DispatchTable& dispatch);
        void release_all(vkb::DispatchTable& dispatch);
    };

    struct sync_primitive_recycler
    {
        struct fence_recycle_payload
        {
            VkFence fence;
            std::size_t recycled_frame;
        };

        struct semaphore_recycle_payload
        {
            VkSemaphore sem;
            std::size_t recycled_frame;
        };

        std::vector<VkFence> global_fence_pool;
        std::deque<fence_recycle_payload> recycle_fence_pool;
        std::vector<VkSemaphore> global_semaphore_pool;
        std::deque<semaphore_recycle_payload> recycle_semaphore_pool;

        std::size_t frames_in_flight;

        VkFence acquire_fence(vkb::DispatchTable& dispatch);
        VkSemaphore acquire_semaphore(vkb::DispatchTable& dispatch);
        void release(VkFence&& fen, std::size_t current_frame);
        void release(VkSemaphore&& sem, std::size_t current_frame);
        void recycle(std::size_t current_frame, vkb::DispatchTable& dispatch);

        void release_all(vkb::DispatchTable& dispatch);
    };

    class command_execution_service : public graphics::command_execution_service
    {
      public:
        command_execution_service(vkb::DispatchTable& dispatch, render_device& device);
        ~command_execution_service();

        command_list& get_commands() override;
        void submit_and_wait() override;

      private:
        vkb::DispatchTable* _dispatch;
        render_device* _device;
        VkCommandPool _pool{VK_NULL_HANDLE};
        VkCommandBuffer _buffer{VK_NULL_HANDLE};
        std::optional<command_list> _cmds;
        bool _is_recording{false};
    };

    class render_device : public graphics::render_device
    {
      public:
        explicit render_device(core::allocator* alloc, vkb::Instance instance, vkb::PhysicalDevice physical);
        ~render_device() override;

        void start_frame() noexcept override;
        void end_frame() noexcept override;

        buffer* access_buffer(buffer_resource_handle handle) noexcept;
        const buffer* access_buffer(buffer_resource_handle handle) const noexcept;
        buffer_resource_handle allocate_buffer();
        buffer_resource_handle create_buffer(const buffer_create_info& ci) override;
        buffer_resource_handle create_buffer(const buffer_create_info& ci, buffer_resource_handle handle);
        void release_buffer(buffer_resource_handle handle) override;

        std::span<std::byte> map_buffer(buffer_resource_handle handle) override;
        std::span<std::byte> map_buffer_frame(buffer_resource_handle handlee, std::uint64_t frame_offset) override;
        std::size_t get_buffer_frame_offset(buffer_resource_handle handle, std::uint64_t frame_offset) override;
        void unmap_buffer(buffer_resource_handle handle) override;

        image* access_image(image_resource_handle handle) noexcept;
        const image* access_image(image_resource_handle handle) const noexcept;
        image_resource_handle allocate_image();
        image_resource_handle create_image(const image_create_info& ci) override;
        image_resource_handle create_image(const image_create_info& ci, image_resource_handle handle);
        void release_image(image_resource_handle handle) override;

        sampler* access_sampler(sampler_resource_handle handle) noexcept;
        const sampler* access_sampler(sampler_resource_handle handle) const noexcept;
        sampler_resource_handle allocate_sampler();
        sampler_resource_handle create_sampler(const sampler_create_info& ci) override;
        sampler_resource_handle create_sampler(const sampler_create_info& ci, sampler_resource_handle handle);
        void release_sampler(sampler_resource_handle handle) override;

        graphics_pipeline* access_graphics_pipeline(graphics_pipeline_resource_handle handle) noexcept;
        const graphics_pipeline* access_graphics_pipeline(graphics_pipeline_resource_handle handle) const noexcept;
        graphics_pipeline_resource_handle allocate_graphics_pipeline();
        graphics_pipeline_resource_handle create_graphics_pipeline(const graphics_pipeline_create_info& ci) override;
        graphics_pipeline_resource_handle create_graphics_pipeline(const graphics_pipeline_create_info& ci,
                                                                   graphics_pipeline_resource_handle handle);
        void release_graphics_pipeline(graphics_pipeline_resource_handle handle) override;

        compute_pipeline* access_compute_pipeline(compute_pipeline_resource_handle handle) noexcept;
        const compute_pipeline* access_compute_pipeline(compute_pipeline_resource_handle handle) const noexcept;
        compute_pipeline_resource_handle allocate_compute_pipeline();
        compute_pipeline_resource_handle create_compute_pipeline(const compute_pipeline_create_info& ci) override;
        compute_pipeline_resource_handle create_compute_pipeline(const compute_pipeline_create_info& ci,
                                                                 compute_pipeline_resource_handle handle);
        void release_compute_pipeline(compute_pipeline_resource_handle handle) override;

        swapchain* access_swapchain(swapchain_resource_handle handle) noexcept;
        const swapchain* access_swapchain(swapchain_resource_handle handle) const noexcept;
        swapchain_resource_handle allocate_swapchain();
        swapchain_resource_handle create_swapchain(const swapchain_create_info& info) override;
        swapchain_resource_handle create_swapchain(const swapchain_create_info& info, swapchain_resource_handle handle);
        void release_swapchain(swapchain_resource_handle handle) override;
        void recreate_swapchain(swapchain_resource_handle handle) override;
        image_resource_handle fetch_current_image(swapchain_resource_handle handle) override;

        command_execution_service& get_command_executor() override
        {
            return _executor.value();
        }

        buffer_resource_handle get_staging_buffer() override
        {
            return _staging_buffer;
        }

        VkResult acquire_next_image(swapchain_resource_handle handle, VkSemaphore sem, VkFence fen);

        queue_info get_queue() const noexcept
        {
            return _queue;
        }

        size_t frame_in_flight() const noexcept override
        {
            return _current_frame % _frames_in_flight;
        }

        size_t frames_in_flight() const noexcept override
        {
            return _frames_in_flight;
        }

        command_buffer_allocator acquire_frame_local_command_buffer_allocator();
        void release_frame_local_command_buffer_allocator(command_buffer_allocator&& allocator);

        VkSemaphore acquire_semaphore()
        {
            return _sync_prim_recycler.acquire_semaphore(_dispatch);
        }

        void release_semaphore(VkSemaphore&& sem)
        {
            _sync_prim_recycler.release(std::move(sem), _current_frame);
        }

        VkFence acquire_fence()
        {
            return _sync_prim_recycler.acquire_fence(_dispatch);
        }

        void release_fence(VkFence&& fence)
        {
            return _sync_prim_recycler.release(std::move(fence), _current_frame);
        }

        vkb::DispatchTable& dispatch()
        {
            return _dispatch;
        }

        const vkb::DispatchTable& dispatch() const
        {
            return _dispatch;
        }

        void idle()
        {
            _dispatch.deviceWaitIdle();
        }

        vkb::Instance instance() const noexcept
        {
            return _instance;
        }

        vkb::PhysicalDevice physical_device() const noexcept
        {
            return _physical;
        }
        
        vkb::Device logical_device() const noexcept
        {
            return _device;
        }

      private:
        core::allocator* _alloc;
        vkb::Instance _instance;
        vkb::PhysicalDevice _physical;
        vkb::Device _device;
        vkb::DispatchTable _dispatch;
        VmaAllocator _vk_alloc{VK_NULL_HANDLE};

        std::optional<core::generational_object_pool> _images;
        std::optional<core::generational_object_pool> _buffers;
        std::optional<core::generational_object_pool> _graphics_pipelines;
        std::optional<core::generational_object_pool> _compute_pipelines;
        std::optional<core::generational_object_pool> _swapchains;
        std::optional<core::generational_object_pool> _samplers;

        queue_info _queue;
        std::optional<command_execution_service> _executor;

        std::optional<resource_deletion_queue> _delete_queue;

        std::size_t _current_frame{0};
        static constexpr std::size_t _frames_in_flight{2};

        command_buffer_recycler _recycled_cmd_buf_pool{};
        sync_primitive_recycler _sync_prim_recycler{};

        buffer_resource_handle _staging_buffer;
    };

    class render_context : public graphics::render_context
    {
      public:
        explicit render_context(core::allocator* alloc);
        ~render_context() override;

        bool has_suitable_device() const noexcept override;
        std::uint32_t device_count() const noexcept override;
        graphics::render_device& create_device(std::uint32_t idx) override;
        std::vector<physical_device_context> enumerate_suitable_devices() override;

      private:
        std::vector<std::unique_ptr<render_device>> _devices;

        vkb::Instance _instance;
    };
} // namespace tempest::graphics::vk

#endif // tempest_graphics_vk_render_device_hpp