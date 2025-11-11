#ifndef tempest_rhi_vk_rhi_hpp
#define tempest_rhi_vk_rhi_hpp

#include <VkBootstrapDispatch.h>
#include <tempest/inplace_vector.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/rhi.hpp>
#include <tempest/slot_map.hpp>
#include <tempest/vk/rhi_resource_tracker.hpp>

#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include <queue>
#include <vulkan/vulkan_core.h>

#if TEMPEST_ENABLE_AFTERMATH
#include <tempest/vk/aftermath/gpu_crash_tracker.hpp>
#endif

namespace tempest::rhi::vk
{
    class device;
    class work_queue;

    class instance : public rhi::instance
    {
      public:
        explicit instance(vkb::Instance instance, vector<vkb::PhysicalDevice> devices) noexcept;
        ~instance() override;

        vector<rhi_device_description> get_devices() const noexcept override;
        rhi::device& acquire_device(uint32_t device_index) noexcept override;

      private:
        vkb::Instance _vkb_instance;
        vector<vkb::PhysicalDevice> _vkb_phys_devices;
        vector<unique_ptr<vk::device>> _devices;

        friend unique_ptr<rhi::instance> create_instance() noexcept;
    };

    struct work_group
    {
        VkCommandPool pool = VK_NULL_HANDLE;
        vector<VkCommandBuffer> cmd_buffers;
        // Maintain a parallel vector of handles
        vector<typed_rhi_handle<rhi_handle_type::command_list>> cmd_buffer_handles;

        int32_t current_buffer_index = -1;

        vkb::DispatchTable* dispatch{};
        device* parent{};

        void reset() noexcept;
        typed_rhi_handle<rhi_handle_type::command_list> acquire_next_command_buffer() noexcept;
        optional<typed_rhi_handle<rhi_handle_type::command_list>> current_command_buffer() const noexcept;
    };

    class work_queue : public rhi::work_queue
    {
      public:
        work_queue() = default;
        explicit work_queue(device* parent, vkb::DispatchTable* dispatch, VkQueue queue, uint32_t queue_family_index,
                            uint32_t fif, resource_tracker* res_tracker) noexcept;
        ~work_queue() override;

        typed_rhi_handle<rhi_handle_type::command_list> get_next_command_list() noexcept override;

        bool submit(span<const submit_info> infos,
                    typed_rhi_handle<rhi_handle_type::fence> fence =
                        typed_rhi_handle<rhi_handle_type::fence>::null_handle) noexcept override;
        vector<rhi::work_queue::present_result> present(const present_info& info) noexcept override;

        void start_frame(uint32_t frame_in_flight);

        // Timeline tracking
        uint64_t query_completed_timeline_value() const noexcept
        {
            uint64_t val;
            _dispatch->getSemaphoreCounterValue(_resource_tracking_sem, &val);
            return val;
        }

        uint64_t get_last_submitted_timeline_value() const noexcept
        {
            return _last_submitted_value;
        }

        VkSemaphore get_timeline_semaphore() const noexcept
        {
            return _resource_tracking_sem;
        }

        // commands
        void begin_command_list(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                bool one_time_submit) noexcept override;
        void end_command_list(typed_rhi_handle<rhi_handle_type::command_list> command_list) noexcept override;

        // Image commands
        void transition_image(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                              span<const image_barrier> image_barriers) noexcept override;
        void clear_color_image(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                               typed_rhi_handle<rhi_handle_type::image> image, image_layout layout, float r, float g,
                               float b, float a) noexcept override;
        void blit(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::image> src, image_layout src_layout, uint32_t src_mip,
                  typed_rhi_handle<rhi_handle_type::image> dst, image_layout dst_layout,
                  uint32_t dst_mip) noexcept override;
        void generate_mip_chain(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                typed_rhi_handle<rhi_handle_type::image> img, image_layout current_layout,
                                uint32_t base_mip, uint32_t mip_count) noexcept override;

        // Buffer commands
        void copy(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::buffer> src, typed_rhi_handle<rhi_handle_type::buffer> dst,
                  size_t src_offset = 0, size_t dst_offset = 0,
                  size_t byte_count = numeric_limits<size_t>::max()) noexcept override;
        void fill(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::buffer> handle, size_t offset, size_t size,
                  uint32_t data) noexcept override;
        void copy(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::buffer> src, typed_rhi_handle<rhi_handle_type::image> dst,
                  image_layout layout, size_t src_offset = 0, uint32_t dst_mip = 0) noexcept override;

        // Barrier commands
        void pipeline_barriers(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                               span<const image_barrier> image_barriers,
                               span<const buffer_barrier> buffer_barriers) noexcept override;

        // Rendering commands
        void begin_rendering(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                             const render_pass_info& render_pass_info) noexcept override;
        void end_rendering(typed_rhi_handle<rhi_handle_type::command_list> command_list) noexcept override;
        void bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::graphics_pipeline> pipeline) noexcept override;
        void draw(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::buffer> indirect_buffer, uint32_t offset, uint32_t draw_count,
                  uint32_t stride) noexcept override;
        void draw(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t vertex_count,
                  uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) noexcept override;
        void draw(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t index_count,
                  uint32_t instance_count, uint32_t first_index, int32_t vertex_offset,
                  uint32_t first_instance) noexcept override;
        void bind_index_buffer(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                               typed_rhi_handle<rhi_handle_type::buffer> buffer, uint32_t offset,
                               rhi::index_format index_type) noexcept override;
        void bind_vertex_buffers(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t first_binding,
                                 span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers,
                                 span<const size_t> offsets) noexcept override;
        void set_scissor_region(typed_rhi_handle<rhi_handle_type::command_list> command_list, int32_t x, int32_t y,
                                uint32_t width, uint32_t height, uint32_t region_index = 0) noexcept override;
        void set_viewport(typed_rhi_handle<rhi_handle_type::command_list> command_list, float x, float y, float width,
                          float height, float min_depth, float max_depth, uint32_t viewport_index = 0,
                          bool flipped = false) noexcept override;
        void set_cull_mode(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                           enum_mask<cull_mode> cull) noexcept override;

        // Compute commands
        void bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::compute_pipeline> pipeline) noexcept override;
        void dispatch(typed_rhi_handle<rhi_handle_type::command_list> command_list, uint32_t x, uint32_t y,
                      uint32_t z) noexcept override;

        // Descriptor commands
        void bind(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                  typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout, bind_point point,
                  uint32_t first_set_index, span<const typed_rhi_handle<rhi_handle_type::descriptor_set>> sets,
                  span<const uint32_t> dynamic_offsets) noexcept override;

        void push_descriptors(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                              typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout, bind_point point,
                              uint32_t set_index, span<const buffer_binding_descriptor> buffers,
                              span<const image_binding_descriptor> images,
                              span<const sampler_binding_descriptor> samplers) noexcept override;

        void push_constants(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                            typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout,
                            enum_mask<rhi::shader_stage> stages, uint32_t offset,
                            span<const byte> values) noexcept override;

        void bind_descriptor_buffers(typed_rhi_handle<rhi_handle_type::command_list> command_list,
                                     typed_rhi_handle<rhi_handle_type::pipeline_layout> pipeline_layout,
                                     bind_point point, uint32_t first_set_index,
                                     span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers,
                                     span<const uint64_t> offsets) noexcept override;

        void reset(uint64_t frame_in_flight) override;

        // Debugging
        void begin_debug_region(typed_rhi_handle<rhi_handle_type::command_list> command_list,string_view name) override;
        void end_debug_region(typed_rhi_handle<rhi_handle_type::command_list> command_list) override;
        void set_debug_marker(typed_rhi_handle<rhi_handle_type::command_list> command_list, string_view name) override;

      private:
        vkb::DispatchTable* _dispatch;
        VkQueue _queue;
        uint32_t _queue_family_index;

        vector<work_group> _work_groups;
        device* _parent;

        resource_tracker* _res_tracker;

        stack_allocator _allocator{64 * 1024};

        VkSemaphore _resource_tracking_sem{VK_NULL_HANDLE};
        uint64_t _next_timeline_value{1};
        uint64_t _last_submitted_value{0};

        // Set of all used buffers, images, etc
        flat_unordered_map<typed_rhi_handle<rhi_handle_type::command_list>,
                           vector<typed_rhi_handle<rhi_handle_type::buffer>>>
            used_buffers;

        flat_unordered_map<typed_rhi_handle<rhi_handle_type::command_list>,
                           vector<typed_rhi_handle<rhi_handle_type::image>>>
            used_images;
        flat_unordered_map<typed_rhi_handle<rhi_handle_type::command_list>,
                           vector<typed_rhi_handle<rhi_handle_type::graphics_pipeline>>>
            used_gfx_pipelines;
        flat_unordered_map<typed_rhi_handle<rhi_handle_type::command_list>,
                           vector<typed_rhi_handle<rhi_handle_type::compute_pipeline>>>
            used_compute_pipelines;
        flat_unordered_map<typed_rhi_handle<rhi_handle_type::command_list>,
                           vector<typed_rhi_handle<rhi_handle_type::sampler>>>
            used_samplers;
    };

    struct image
    {
        VmaAllocation allocation;
        VmaAllocationInfo allocation_info;
        VkImage image;
        VkImageView image_view;
        bool swapchain_image;
        VkImageAspectFlags image_aspect;
        VkImageCreateInfo create_info;
        VkImageViewCreateInfo view_create_info;
        string name;
    };

    struct buffer
    {
        VmaAllocation allocation;
        VmaAllocationInfo allocation_info;
        VkBuffer buffer;
        VkDeviceAddress address;
        VkBufferUsageFlags usage;
    };

    struct sampler
    {
        VkSampler sampler;
        VkSamplerCreateInfo create_info;
    };

    struct fence
    {
        VkFence fence;
    };

    struct semaphore
    {
        VkSemaphore semaphore;
        semaphore_type type;
    };

    struct fif_data
    {
        typed_rhi_handle<rhi_handle_type::semaphore> image_acquired;
    };

    struct swapchain
    {
        vkb::Swapchain swapchain;
        VkSurfaceKHR surface;
        inplace_vector<typed_rhi_handle<rhi_handle_type::image>, 8> images;
        inplace_vector<typed_rhi_handle<rhi::rhi_handle_type::semaphore>, 8> render_complete;
        inplace_vector<fif_data, 4> frames;
        const rhi::window_surface* window;
    };

    struct graphics_pipeline
    {
        inplace_vector<VkShaderModule, 5> shader_modules;
        VkPipeline pipeline;
        VkPipelineLayout layout;

        graphics_pipeline_desc desc;
    };

    struct compute_pipeline
    {
        VkShaderModule shader_module;
        VkPipeline pipeline;
        VkPipelineLayout layout;

        compute_pipeline_desc desc;
    };

    struct delete_queue
    {
        VmaAllocator allocator{};
        vkb::DispatchTable* dispatch{};
        vkb::Instance* instance{};

        struct delete_resource
        {
            uint64_t last_used_frame{};
            VkObjectType type{};
            void* handle{};
            VmaAllocation allocation{};
            VkDescriptorPool desc_pool{};
        };

        void enqueue(VkObjectType type, void* handle, uint64_t frame);
        void enqueue(VkObjectType type, void* handle, VmaAllocation allocation, uint64_t frame);
        void enqueue(VkObjectType type, void* handle, VkDescriptorPool desc_pool, uint64_t frame);
        void release_resources(uint64_t frame);
        void release_resource(delete_resource res);
        void destroy();

        std::queue<delete_resource> dq{};
    };

    struct descriptor_set
    {
        VkDescriptorSet set;
        VkDescriptorPool pool;
        VkDescriptorSetLayout layout;

        vector<typed_rhi_handle<rhi_handle_type::buffer>> bound_buffers;
        vector<typed_rhi_handle<rhi_handle_type::image>> bound_images;
        vector<typed_rhi_handle<rhi_handle_type::sampler>> bound_samplers;
    };

    class descriptor_set_layout_cache
    {
      public:
        struct cache_key
        {
            vector<descriptor_binding_layout> desc;
            enum_mask<descriptor_set_layout_flags> flags;
            size_t hash;

            bool operator==(const cache_key& other) const noexcept
            {
                // Check against hash first, because it's a cheaper condition with a reasonable chance of being true
                return hash == other.hash && desc == other.desc;
            }

            bool operator!=(const cache_key& other) const noexcept
            {
                return !(*this == other);
            }
        };

        struct slot_entry
        {
            cache_key key;
            VkDescriptorSetLayout layout;
            uint32_t ref_count;
        };

        struct cache_key_hash
        {
            size_t operator()(const cache_key& key) const noexcept
            {
                return key.hash;
            }
        };

        descriptor_set_layout_cache(device* dev) noexcept;
        descriptor_set_layout_cache(const descriptor_set_layout_cache&) = delete;
        descriptor_set_layout_cache(descriptor_set_layout_cache&&) noexcept = delete;
        ~descriptor_set_layout_cache() noexcept = default;

        descriptor_set_layout_cache& operator=(const descriptor_set_layout_cache&) = delete;
        descriptor_set_layout_cache& operator=(descriptor_set_layout_cache&&) noexcept = delete;

        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> get_or_create_layout(
            const vector<descriptor_binding_layout>& desc, enum_mask<descriptor_set_layout_flags> flags) noexcept;

        bool release_layout(typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept;

        VkDescriptorSetLayout get_layout(
            typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) const noexcept;

        void destroy() noexcept;

        bool add_usage(typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept;

      private:
        flat_unordered_map<cache_key, typed_rhi_handle<rhi_handle_type::descriptor_set_layout>, cache_key_hash> _cache;
        slot_map<slot_entry> _cache_slots;
        device* _dev;

        size_t _compute_cache_hash(const vector<descriptor_binding_layout>& desc,
                                   enum_mask<descriptor_set_layout_flags> flags) const noexcept;

        stack_allocator _allocator{16 * 1024};
    };

    class pipeline_layout_cache
    {
      public:
        struct cache_key
        {
            pipeline_layout_desc desc;
            size_t hash;

            bool operator==(const cache_key& other) const noexcept
            {
                // Check against hash first, because it's a cheaper condition with a reasonable chance of being true
                return hash == other.hash && desc == other.desc;
            }

            bool operator!=(const cache_key& other) const noexcept
            {
                return !(*this == other);
            }
        };

        struct slot_entry
        {
            cache_key key;
            VkPipelineLayout layout;
            uint32_t ref_count;
        };

        struct cache_key_hash
        {
            size_t operator()(const cache_key& key) const noexcept
            {
                return key.hash;
            }
        };

        pipeline_layout_cache(device* dev) noexcept;
        pipeline_layout_cache(const pipeline_layout_cache&) = delete;
        pipeline_layout_cache(pipeline_layout_cache&&) noexcept = delete;
        ~pipeline_layout_cache() noexcept = default;

        pipeline_layout_cache& operator=(const pipeline_layout_cache&) = delete;
        pipeline_layout_cache& operator=(pipeline_layout_cache&&) noexcept = delete;

        typed_rhi_handle<rhi_handle_type::pipeline_layout> get_or_create_layout(
            const pipeline_layout_desc& desc) noexcept;

        bool release_layout(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept;

        VkPipelineLayout get_layout(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) const noexcept;

        void destroy() noexcept;

        bool add_usage(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept;

      private:
        flat_unordered_map<cache_key, typed_rhi_handle<rhi_handle_type::pipeline_layout>, cache_key_hash> _cache;
        slot_map<slot_entry> _cache_slots;
        device* _dev;
        size_t _compute_cache_hash(const pipeline_layout_desc& desc) const noexcept;
        stack_allocator _allocator{4 * 1024};
    };

    class device : public rhi::device
    {
      public:
        explicit device(vkb::Device dev, vkb::Instance* instance);
        ~device() override;

        device(const device&) = delete;
        device(device&&) noexcept = delete;
        device& operator=(const device&) = delete;
        device& operator=(device&&) noexcept = delete;

        typed_rhi_handle<rhi_handle_type::buffer> create_buffer(const buffer_desc& desc) noexcept override;
        typed_rhi_handle<rhi_handle_type::image> create_image(const image_desc& desc) noexcept override;
        typed_rhi_handle<rhi_handle_type::fence> create_fence(const fence_info& info) noexcept override;
        typed_rhi_handle<rhi_handle_type::semaphore> create_semaphore(const semaphore_info& info) noexcept override;
        typed_rhi_handle<rhi_handle_type::render_surface> create_render_surface(
            const render_surface_desc& desc) noexcept override;
        typed_rhi_handle<rhi_handle_type::descriptor_set_layout> create_descriptor_set_layout(
            const vector<descriptor_binding_layout>& desc,
            enum_mask<descriptor_set_layout_flags> flags) noexcept override;
        typed_rhi_handle<rhi_handle_type::pipeline_layout> create_pipeline_layout(
            const pipeline_layout_desc& desc) noexcept override;
        typed_rhi_handle<rhi_handle_type::graphics_pipeline> create_graphics_pipeline(
            const graphics_pipeline_desc& desc) noexcept override;
        typed_rhi_handle<rhi_handle_type::descriptor_set> create_descriptor_set(
            const descriptor_set_desc& desc) noexcept override;
        typed_rhi_handle<rhi_handle_type::compute_pipeline> create_compute_pipeline(
            const compute_pipeline_desc& desc) noexcept override;
        typed_rhi_handle<rhi_handle_type::sampler> create_sampler(const sampler_desc& desc) noexcept override;

        void destroy_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept override;
        void destroy_image(typed_rhi_handle<rhi_handle_type::image> handle) noexcept override;
        void destroy_fence(typed_rhi_handle<rhi_handle_type::fence> handle) noexcept override;
        void destroy_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) noexcept override;
        void destroy_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept override;
        void destroy_descriptor_set_layout(
            typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept override;
        void destroy_pipeline_layout(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept override;
        void destroy_graphics_pipeline(typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle) noexcept override;
        void destroy_descriptor_set(typed_rhi_handle<rhi_handle_type::descriptor_set> handle) noexcept override;
        void destroy_compute_pipeline(typed_rhi_handle<rhi_handle_type::compute_pipeline> handle) noexcept override;
        void destroy_sampler(typed_rhi_handle<rhi_handle_type::sampler> handle) noexcept override;

        void recreate_render_surface(typed_rhi_handle<rhi_handle_type::render_surface> handle,
                                     const render_surface_desc& desc) noexcept override;

        rhi::work_queue& get_primary_work_queue() noexcept override;
        rhi::work_queue& get_dedicated_transfer_queue() noexcept override;
        rhi::work_queue& get_dedicated_compute_queue() noexcept override;

        render_surface_info query_render_surface_info(const rhi::window_surface& window) noexcept override;
        span<const typed_rhi_handle<rhi_handle_type::image>> get_render_surfaces(
            typed_rhi_handle<rhi_handle_type::render_surface> handle) noexcept override;
        expected<swapchain_image_acquire_info_result, swapchain_error_code> acquire_next_image(
            typed_rhi_handle<rhi_handle_type::render_surface> swapchain,
            typed_rhi_handle<rhi_handle_type::fence> signal_fence) noexcept override;

        bool is_signaled(typed_rhi_handle<rhi_handle_type::fence> fence) const noexcept override;
        bool reset(span<const typed_rhi_handle<rhi_handle_type::fence>> fences) const noexcept override;
        bool wait(span<const typed_rhi_handle<rhi_handle_type::fence>> fences) const noexcept override;

        byte* map_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept override;
        void unmap_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept override;
        void flush_buffers(span<const typed_rhi_handle<rhi_handle_type::buffer>> buffers) noexcept override;
        size_t get_buffer_size(typed_rhi_handle<rhi_handle_type::buffer> handle) const noexcept override;

        uint32_t get_render_surface_width(
            typed_rhi_handle<rhi_handle_type::render_surface> handle) const noexcept override;
        uint32_t get_render_surface_height(
            typed_rhi_handle<rhi_handle_type::render_surface> handle) const noexcept override;
        const rhi::window_surface* get_window_surface(
            typed_rhi_handle<rhi_handle_type::render_surface> handle) const noexcept override;

        // Descriptor buffer support
        bool supports_descriptor_buffers() const noexcept override;
        size_t get_descriptor_buffer_alignment() const noexcept override;
        size_t get_descriptor_set_layout_size(
            rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout> layout) const noexcept override;
        size_t write_descriptor_buffer(const descriptor_set_desc& desc, byte* dest,
                                       size_t offset) const noexcept override;

        // Miscellaneous
        void release_resources() override;
        void finish_frame() override;

        uint32_t frame_in_flight() const noexcept;
        uint32_t frames_in_flight() const noexcept override;

        void wait_idle() override
        {
            _dispatch_table.deviceWaitIdle();
        }

        typed_rhi_handle<rhi_handle_type::image> acquire_image(image img) noexcept;

        typed_rhi_handle<rhi_handle_type::command_list> acquire_command_list(VkCommandBuffer buf) noexcept;
        VkCommandBuffer get_command_buffer(typed_rhi_handle<rhi_handle_type::command_list> handle) const noexcept;
        void release_command_list(typed_rhi_handle<rhi_handle_type::command_list> handle) noexcept;

        typed_rhi_handle<rhi_handle_type::render_surface> create_render_surface(
            const rhi::render_surface_desc& desc,
            typed_rhi_handle<rhi_handle_type::render_surface> old_swapchain) noexcept;

        VkFence get_fence(typed_rhi_handle<rhi_handle_type::fence> handle) const noexcept;
        VkSemaphore get_semaphore(typed_rhi_handle<rhi_handle_type::semaphore> handle) const noexcept;
        VkSwapchainKHR get_swapchain(typed_rhi_handle<rhi_handle_type::render_surface> handle) const noexcept;
        VkDescriptorSetLayout get_descriptor_set_layout(
            typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) const noexcept;
        VkPipelineLayout get_pipeline_layout(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) const noexcept;

        optional<const vk::buffer&> get_buffer(typed_rhi_handle<rhi_handle_type::buffer> handle) const noexcept;
        optional<const vk::image&> get_image(typed_rhi_handle<rhi_handle_type::image> handle) const noexcept;
        optional<const vk::graphics_pipeline&> get_graphics_pipeline(
            typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle) const noexcept;
        optional<const vk::descriptor_set&> get_descriptor_set(
            typed_rhi_handle<rhi_handle_type::descriptor_set> handle) const noexcept;
        optional<const vk::sampler&> get_sampler(typed_rhi_handle<rhi_handle_type::sampler> handle) const noexcept;
        optional<const vk::compute_pipeline&> get_compute_pipeline(
            typed_rhi_handle<rhi_handle_type::compute_pipeline> handle) const noexcept;

        void release_resource_immediate(typed_rhi_handle<rhi_handle_type::buffer> handle) noexcept;
        void release_resource_immediate(typed_rhi_handle<rhi_handle_type::image> handle) noexcept;
        void release_resource_immediate(typed_rhi_handle<rhi_handle_type::sampler> handle) noexcept;
        bool release_resource_immediate(typed_rhi_handle<rhi_handle_type::descriptor_set_layout> handle) noexcept;
        bool release_resource_immediate(typed_rhi_handle<rhi_handle_type::pipeline_layout> handle) noexcept;
        void release_resource_immediate(typed_rhi_handle<rhi_handle_type::graphics_pipeline> handle) noexcept;
        void release_resource_immediate(typed_rhi_handle<rhi_handle_type::descriptor_set> handle) noexcept;
        void release_resource_immediate(typed_rhi_handle<rhi_handle_type::compute_pipeline> handle) noexcept;

        const vkb::DispatchTable& get_dispatch_table() const noexcept
        {
            return _dispatch_table;
        }

        vector<pair<const vk::work_queue*, uint64_t>> compute_current_work_queue_timeline_values() const noexcept;

        bool is_debug_device() const noexcept
        {
            return _is_debug_device;
        }

        bool can_name_objects() const noexcept
        {
            return _can_name;
        }

      private:
        vkb::Instance* _vkb_instance;
        vkb::Device _vkb_device;
        vkb::DispatchTable _dispatch_table;
        vkb::InstanceDispatchTable _instance_dispatch_table;
        VmaAllocator _vma_allocator{};
        bool _is_debug_device;
        bool _can_name;

        optional<work_queue> _primary_work_queue;
        optional<work_queue> _dedicated_transfer_queue;
        optional<work_queue> _dedicated_compute_queue;

        delete_queue _delete_queue;

        slot_map<buffer> _buffers;
        slot_map<fence> _fences;
        slot_map<image> _images;
        slot_map<semaphore> _semaphores;
        slot_map<swapchain> _swapchains;
        slot_map<graphics_pipeline> _graphics_pipelines;
        slot_map<compute_pipeline> _compute_pipelines;
        slot_map<descriptor_set> _descriptor_sets;
        slot_map<sampler> _samplers;

        slot_map<VkCommandBuffer> _command_buffers;

        uint64_t _current_frame{0};
        static constexpr uint64_t num_frames_in_flight = 2;

        // Resource tracking
        resource_tracker _resource_tracker;

        // Descriptor layout cache
        descriptor_set_layout_cache _descriptor_set_layout_cache;
        pipeline_layout_cache _pipeline_layout_cache;

        // Descriptors
        VkDescriptorPool _desc_pool;
        stack_allocator _desc_pool_allocator{128 * 1024};

        // Descriptor buffers
        VkPhysicalDeviceDescriptorBufferPropertiesEXT _descriptor_buffer_properties{};

        void name_object(VkObjectType type, void* handle, const char* name) noexcept;

#if TEMPEST_ENABLE_AFTERMATH
        aftermath::gpu_crash_tracker::marker_map _marker_map;
        aftermath::gpu_crash_tracker _crash_tracker;
#endif
    };

    unique_ptr<rhi::instance> create_instance() noexcept;
    unique_ptr<rhi::window_surface> create_window_surface(const rhi::window_surface_desc& desc) noexcept;
} // namespace tempest::rhi::vk

#endif // tempest_rhi_vk_rhi_hpp
