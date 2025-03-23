#ifndef tempest_graphics_vk_render_device_hpp
#define tempest_graphics_vk_render_device_hpp

#include <tempest/functional.hpp>
#include <tempest/object_pool.hpp>
#include <tempest/optional.hpp>
#include <tempest/render_device.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

#include <array>
#include <deque>
#include <optional>

#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include "../../windowing/glfw_window.hpp"

namespace tempest::graphics::vk
{
    class render_device;

    struct image
    {
        VmaAllocation allocation{};
        VmaAllocationInfo alloc_info{};
        VkImage image;
        VkImageView view;
        VkImageCreateInfo img_info;
        VkImageViewCreateInfo view_info;
        bool persistent{};
        string name;
    };

    struct buffer
    {
        bool per_frame_resource{};
        VmaAllocation allocation{};
        VmaAllocationInfo alloc_info{};
        VkBuffer vk_buffer{};
        VkBufferCreateInfo info;
        string name;
    };

    struct graphics_pipeline
    {
        VkShaderModule vertex_module;
        VkShaderModule fragment_module;
        vector<VkDescriptorSetLayout> set_layouts;
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
        string name;
    };

    struct compute_pipeline
    {
        VkShaderModule compute_module;
        vector<VkDescriptorSetLayout> set_layouts;
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
        string name;
    };

    struct swapchain
    {
        glfw::window* win{nullptr};
        uint32_t image_index{};
        vkb::Swapchain sc{};
        VkSurfaceKHR surface{};
        vector<image_resource_handle> image_handles{};
    };

    struct sampler
    {
        VkSampler vk_sampler;
        VkSamplerCreateInfo info;
        string name;
    };

    class resource_deletion_queue
    {
      public:
        explicit resource_deletion_queue(size_t frames_in_flight);

        void add_to_queue(size_t current_frame, function<void()> deleter);
        void flush_frame(size_t current_frame);
        void flush_all();

      private:
        struct delete_info
        {
            size_t frame;
            function<void()> deleter;
        };

        vector<delete_info> _queue;
        size_t _frames_in_flight;
    };

    struct queue_info
    {
        VkQueue queue;
        uint32_t queue_family_index;
        uint32_t queue_index;
        VkQueueFlags flags;
    };

    class command_list final : public graphics::command_list
    {
      public:
        command_list(VkCommandBuffer buffer, vkb::DispatchTable* dispatch, render_device* device);
        ~command_list() override = default;

        operator VkCommandBuffer() const noexcept;

        command_list& push_constants(uint32_t offset, span<const byte> data,
                                     compute_pipeline_resource_handle handle) override;
        command_list& push_constants(uint32_t offset, span<const byte> data,
                                     graphics_pipeline_resource_handle handle) override;

        command_list& set_viewport(float x, float y, float width, float height, float min_depth = 0.0f,
                                   float max_depth = 1.0f, bool flip = true) override;
        command_list& set_scissor_region(int32_t x, int32_t y, uint32_t width, uint32_t height) override;
        command_list& draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0,
                           uint32_t first_index = 0) override;
        command_list& draw(buffer_resource_handle buf, uint32_t offset, uint32_t count, uint32_t stride) override;
        command_list& draw_indexed(buffer_resource_handle buf, uint32_t offset, uint32_t count,
                                   uint32_t stride) override;
        command_list& use_pipeline(graphics_pipeline_resource_handle pipeline) override;
        command_list& use_index_buffer(buffer_resource_handle buf, uint32_t offset) override;
        command_list& set_cull_mode(bool front, bool back) override;

        command_list& blit(image_resource_handle src, image_resource_handle dst) override;
        command_list& copy(buffer_resource_handle src, buffer_resource_handle dst, size_t src_offset = 0,
                           size_t dst_offset = 0, size_t byte_count = std::numeric_limits<size_t>::max()) override;
        command_list& copy(buffer_resource_handle src, image_resource_handle dst, size_t buffer_offset,
                           uint32_t region_width, uint32_t region_height, uint32_t mip_level, int32_t offset_x = 0,
                           int32_t offset_y = 0) override;
        command_list& clear_color(image_resource_handle handle, float r, float g, float b, float a) override;
        command_list& fill_buffer(buffer_resource_handle handle, size_t offset, size_t size, uint32_t data) override;

        command_list& transition_image(image_resource_handle img, image_resource_usage old_usage,
                                       image_resource_usage new_usage) override;
        command_list& transition_image(image_resource_handle img, image_resource_usage old_usage,
                                       image_resource_usage new_usage, uint32_t base_mip, uint32_t mip_count);
        command_list& generate_mip_chain(image_resource_handle img, image_resource_usage usage, uint32_t base_mip = 0,
                                         uint32_t mip_count = std::numeric_limits<uint32_t>::max()) override;

        command_list& use_pipeline(compute_pipeline_resource_handle pipeline) override;
        command_list& dispatch(uint32_t x, uint32_t y, uint32_t z) override;

      private:
        VkCommandBuffer _cmds;
        vkb::DispatchTable* _dispatch;
        render_device* _device;
    };

    struct command_buffer_allocator
    {
        queue_info queue;
        VkCommandPool pool;

        vector<VkCommandBuffer> cached_commands;
        size_t command_buffer_index{0};

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
            size_t recycled_frame;
        };

        size_t frames_in_flight;
        queue_info queue;
        vector<command_buffer_allocator> global_pool{};
        std::deque<command_buffer_recycle_payload> recycle_pool{};

        command_buffer_allocator acquire(vkb::DispatchTable& dispatch, render_device* device);
        void release(command_buffer_allocator&& allocator, size_t current_frame);
        void recycle(size_t current_frame, vkb::DispatchTable& dispatch);
        void release_all(vkb::DispatchTable& dispatch);
    };

    struct sync_primitive_recycler
    {
        struct fence_recycle_payload
        {
            VkFence fence;
            size_t recycled_frame;
        };

        struct semaphore_recycle_payload
        {
            VkSemaphore sem;
            size_t recycled_frame;
        };

        vector<VkFence> global_fence_pool{};
        std::deque<fence_recycle_payload> recycle_fence_pool{};
        vector<VkSemaphore> global_semaphore_pool{};
        std::deque<semaphore_recycle_payload> recycle_semaphore_pool{};

        size_t frames_in_flight;

        VkFence acquire_fence(vkb::DispatchTable& dispatch);
        VkSemaphore acquire_semaphore(vkb::DispatchTable& dispatch);
        void release(VkFence&& fen, size_t current_frame);
        void release(VkSemaphore&& sem, size_t current_frame);
        void recycle(size_t current_frame, vkb::DispatchTable& dispatch);

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
        tempest::optional<command_list> _cmds;
        bool _is_recording{false};
    };

    class render_device : public graphics::render_device
    {
      public:
        explicit render_device(abstract_allocator* alloc, vkb::Instance instance, vkb::PhysicalDevice physical);
        ~render_device() override;

        void start_frame() noexcept override;
        void end_frame() noexcept override;

        buffer* access_buffer(buffer_resource_handle handle) noexcept;
        const buffer* access_buffer(buffer_resource_handle handle) const noexcept;
        buffer_resource_handle allocate_buffer();
        buffer_resource_handle create_buffer(const buffer_create_info& ci) override;
        buffer_resource_handle create_buffer(const buffer_create_info& ci, buffer_resource_handle handle);
        void release_buffer(buffer_resource_handle handle) override;

        span<byte> map_buffer(buffer_resource_handle handle) override;
        span<byte> map_buffer_frame(buffer_resource_handle handlee, uint64_t frame_offset) override;
        size_t get_buffer_frame_offset(buffer_resource_handle handle, uint64_t frame_offset) override;
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

        size_t current_frame() const noexcept override
        {
            return _current_frame;
        }

        command_buffer_allocator acquire_frame_local_command_buffer_allocator();
        void release_frame_local_command_buffer_allocator(command_buffer_allocator&& allocator);

        VkSemaphore acquire_semaphore()
        {
            return _sync_prim_recycler.acquire_semaphore(_dispatch);
        }

        void release_semaphore(VkSemaphore&& sem)
        {
            _sync_prim_recycler.release(tempest::move(sem), _current_frame);
        }

        VkFence acquire_fence()
        {
            return _sync_prim_recycler.acquire_fence(_dispatch);
        }

        void release_fence(VkFence&& fence)
        {
            return _sync_prim_recycler.release(tempest::move(fence), _current_frame);
        }

        vkb::DispatchTable& dispatch()
        {
            return _dispatch;
        }

        const vkb::DispatchTable& dispatch() const
        {
            return _dispatch;
        }

        void idle() override
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

        VmaAllocator vma_allocator() const noexcept
        {
            return _vk_alloc;
        }

      private:
        abstract_allocator* _alloc;
        vkb::Instance _instance;
        vkb::PhysicalDevice _physical;
        vkb::Device _device;
        vkb::DispatchTable _dispatch;
        VmaAllocator _vk_alloc{VK_NULL_HANDLE};

        tempest::optional<core::generational_object_pool> _images;
        tempest::optional<core::generational_object_pool> _buffers;
        tempest::optional<core::generational_object_pool> _graphics_pipelines;
        tempest::optional<core::generational_object_pool> _compute_pipelines;
        tempest::optional<core::generational_object_pool> _swapchains;
        tempest::optional<core::generational_object_pool> _samplers;

        queue_info _queue;
        tempest::optional<command_execution_service> _executor;

        tempest::optional<resource_deletion_queue> _delete_queue;

        size_t _current_frame{0};
        static constexpr size_t _frames_in_flight{2};

        command_buffer_recycler _recycled_cmd_buf_pool{};
        sync_primitive_recycler _sync_prim_recycler{};

        buffer_resource_handle _staging_buffer;

        bool _supports_aniso_filtering{false};
        float _max_aniso{1.0f};
    };

    class render_context : public graphics::render_context
    {
      public:
        explicit render_context(abstract_allocator* alloc);
        ~render_context() override;

        bool has_suitable_device() const noexcept override;
        uint32_t device_count() const noexcept override;
        graphics::render_device& create_device(uint32_t idx) override;
        vector<physical_device_context> enumerate_suitable_devices() override;

      private:
        vector<tempest::unique_ptr<render_device>> _devices;

        vkb::Instance _instance;
    };
} // namespace tempest::graphics::vk

#endif // tempest_graphics_vk_render_device_hpp