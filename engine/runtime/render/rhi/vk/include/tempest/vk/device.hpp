#ifndef tempest_rhi_vk_device_hpp
#define tempest_rhi_vk_device_hpp

#include <tempest/rhi.hpp>
#include <tempest/slot_map.hpp>

#include <VkBootstrap.h>
#include <VkBootstrapDispatch.h>
#include <vk_mem_alloc.h>

namespace tempest::rhi::vk
{
    class execution_port;

    struct buffer
    {
        VkBuffer handle;
        VmaAllocation allocation;
        VmaAllocationInfo allocation_info;
    };

    struct texture
    {
        VkImage handle;
        VmaAllocation allocation;
        VmaAllocationInfo allocation_info;

        VkFormat format;
        VkImageUsageFlags usage_flags;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t mip_levels;
        uint32_t array_layers;
    };

    struct texture_view
    {
        VkImageView handle;
        VkImageViewType view_type;
        VkFormat format;
        VkImageSubresourceRange subresource_range;
    };

    struct sampler
    {
        VkSampler handle;
    };

    struct graphics_pipeline
    {
        VkPipeline handle;
    };

    struct compute_pipeline
    {
        VkPipeline handle;
    };

    struct raw_surface
    {
        VkSurfaceKHR handle;
        native_wsi_handle native_surface_handles;
    };

    class TEMPEST_API device final : public rhi::device
    {
      public:
        static auto create(vkb::Instance instance, vkb::PhysicalDevice physical_device, vkb::Device device,
                           device_desc desc) -> unique_ptr<rhi::device>;

        device(const device&) = delete;
        device(device&&) noexcept = delete;
        ~device() override;

        device& operator=(const device&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        device& operator=(device&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        auto wait_idle() -> void override;
        auto wait_for_sync(host_sync_point sync_point) -> void override;

        [[nodiscard]] auto get_device_desc() const noexcept -> const device_desc& override
        {
            return _desc;
        }

        [[nodiscard]] auto is_ray_tracing_supported() const -> bool override;
        [[nodiscard]] auto is_mesh_shading_supported() const -> bool override;
        [[nodiscard]] auto is_ray_query_supported() const -> bool override;

        [[nodiscard]] auto create_raw_surface(native_wsi_handle native_window_handle)
            -> expected<raw_surface_handle, raw_surface_creation_error> override;
        [[nodiscard]] auto get_surface_capabilities(raw_surface_handle surface) -> surface_capabilities override;
        [[nodiscard]] auto create_render_surface(const render_surface_desc& desc)
            -> unique_ptr<rhi::render_surface> override;
        auto destroy_render_surface(unique_ptr<rhi::render_surface> surface) -> void override;
        auto destroy_raw_surface(raw_surface_handle surface) -> void override;

        [[nodiscard]] auto get_semaphore_value(semaphore_handle semaphore) const -> uint64_t override;

        [[nodiscard]] auto get_graphics_execution_port() -> rhi::execution_port& override;
        [[nodiscard]] auto get_async_compute_execution_port() -> rhi::execution_port& override;
        [[nodiscard]] auto get_async_transfer_execution_port() -> rhi::execution_port& override;

        [[nodiscard]] auto create_buffer(const buffer_desc& desc) -> buffer_handle override;
        [[nodiscard]] auto create_texture(const texture_desc& desc) -> texture_handle override;
        [[nodiscard]] auto create_texture_view(texture_handle texture, const texture_view_desc& desc)
            -> texture_view_handle override;
        [[nodiscard]] auto create_sampler(const sampler_desc& desc) -> sampler_handle override;
        [[nodiscard]] auto create_graphics_pipeline(const graphics_pipeline_desc& desc)
            -> graphics_pipeline_handle override;
        [[nodiscard]] auto create_compute_pipeline(const compute_pipeline_desc& desc)
            -> compute_pipeline_handle override;
        [[nodiscard]] auto create_event() -> event_handle override;
        [[nodiscard]] auto create_timeline_semaphore() -> semaphore_handle override;
        [[nodiscard]] auto create_binary_semaphore() -> semaphore_handle override;

        auto destroy_buffer(buffer_handle buffer) -> void override;
        auto destroy_texture(texture_handle texture) -> void override;
        auto destroy_texture_view(texture_view_handle view) -> void override;
        auto destroy_sampler(sampler_handle sampler) -> void override;
        auto destroy_graphics_pipeline(graphics_pipeline_handle pipeline) -> void override;
        auto destroy_compute_pipeline(compute_pipeline_handle pipeline) -> void override;
        auto destroy_event(event_handle event) -> void override;
        auto destroy_semaphore(semaphore_handle semaphore) -> void override;

        [[nodiscard]] auto allocate_descriptor(descriptor_type type) -> descriptor_handle override;
        auto free_descriptor(descriptor_type type, descriptor_handle descriptor) -> void override;
        auto write_sampler_descriptor(descriptor_handle slot, sampler_handle sampler) -> void override;
        auto write_sampled_image_descriptor(descriptor_handle slot, texture_view_handle view,
                                            image_layout layout = image_layout::general) -> void override;
        auto write_storage_image_descriptor(descriptor_handle slot, texture_view_handle view,
                                            image_layout layout = image_layout::general) -> void override;

        auto set_debug_name(buffer_handle handle, cstring_view name) -> void override;
        auto set_debug_name(texture_handle handle, cstring_view name) -> void override;
        auto set_debug_name(texture_view_handle handle, cstring_view name) -> void override;
        auto set_debug_name(sampler_handle handle, cstring_view name) -> void override;
        auto set_debug_name(graphics_pipeline_handle handle, cstring_view name) -> void override;
        auto set_debug_name(compute_pipeline_handle handle, cstring_view name) -> void override;
        auto set_debug_name(event_handle handle, cstring_view name) -> void override;
        auto set_debug_name(semaphore_handle handle, cstring_view name) -> void override;

        auto set_object_name(uint64_t object_handle, VkObjectType object_type, cstring_view name) const -> void;

        [[nodiscard]] auto get_sampler_descriptor_buffer_address() const noexcept -> VkDeviceAddress
        {
            return _sampler_descriptor_buffer_address;
        }

        [[nodiscard]] auto get_resource_descriptor_buffer_address() const noexcept -> VkDeviceAddress
        {
            return _resource_descriptor_buffer_address;
        }

        [[nodiscard]] auto get_storage_image_descriptor_buffer_offset() const noexcept -> VkDeviceSize
        {
            return _storage_image_buffer_offset;
        }

        [[nodiscard]] auto get_buffer(buffer_handle handle) const -> optional<buffer>
        {
            const auto iter = _buffers.find(handle.handle);
            if (iter == _buffers.end())
            {
                return nullopt;
            }
            return *iter;
        }

        [[nodiscard]] auto get_texture(texture_handle handle) const -> optional<texture>
        {
            const auto iter = _textures.find(handle.handle);
            if (iter == _textures.end())
            {
                return nullopt;
            }
            return *iter;
        }

        auto release_texture_handle(texture_handle handle) -> void
        {
            _textures.erase(handle.handle);
        }

        [[nodiscard]] auto get_texture_view(texture_view_handle handle) const -> optional<texture_view>
        {
            const auto iter = _texture_views.find(handle.handle);
            if (iter == _texture_views.end())
            {
                return nullopt;
            }
            return *iter;
        }

        [[nodiscard]] auto get_sampler(sampler_handle handle) const -> optional<sampler>
        {
            const auto iter = _samplers.find(handle.handle);
            if (iter == _samplers.end())
            {
                return nullopt;
            }
            return *iter;
        }

        [[nodiscard]] auto get_graphics_pipeline(graphics_pipeline_handle handle) const -> optional<graphics_pipeline>
        {
            const auto iter = _graphics_pipelines.find(handle.handle);
            if (iter == _graphics_pipelines.end())
            {
                return nullopt;
            }
            return *iter;
        }

        [[nodiscard]] auto get_compute_pipeline(compute_pipeline_handle handle) const -> optional<compute_pipeline>
        {
            const auto iter = _compute_pipelines.find(handle.handle);
            if (iter == _compute_pipelines.end())
            {
                return nullopt;
            }
            return *iter;
        }

        [[nodiscard]] auto get_semaphore(semaphore_handle handle) const -> VkSemaphore
        {
            const auto iter = _semaphores.find(handle.handle);
            if (iter == _semaphores.end())
            {
                return VK_NULL_HANDLE;
            }
            return *iter;
        }

        [[nodiscard]] auto get_event(event_handle handle) const -> VkEvent
        {
            const auto iter = _events.find(handle.handle);
            if (iter == _events.end())
            {
                return VK_NULL_HANDLE;
            }
            return *iter;
        }

        [[nodiscard]] auto get_raw_surface(raw_surface_handle handle) const -> optional<raw_surface>
        {
            const auto iter = _raw_surfaces.find(handle.handle);
            if (iter == _raw_surfaces.end())
            {
                return nullopt;
            }
            return *iter;
        }

        [[nodiscard]] auto get_global_pipeline_layout() const noexcept -> VkPipelineLayout
        {
            return _default_pipeline_layout;
        }

        auto wait_for_sync(span<const host_sync_point> wait_values, uint64_t timeout_ns = ~0ULL) const
            -> expected<void, submit_error>;

        [[nodiscard]] auto get_device() const noexcept -> const vkb::Device&
        {
            return _device;
        }

        [[nodiscard]] auto get_physical_device() const noexcept -> const vkb::PhysicalDevice&
        {
            return _physical_device;
        }

        [[nodiscard]] auto get_dispatch_table() const noexcept -> const vkb::DispatchTable&
        {
            return _dispatch_table;
        }

        [[nodiscard]] auto get_allocator() const noexcept -> VmaAllocator
        {
            return _allocator;
        }

      private:
        struct descriptor_slot_state
        {
            uint32_t generation = 1;
            bool allocated = false;
        };

        device(vkb::Instance instance, vkb::PhysicalDevice physical_device, vkb::Device device, device_desc desc);

        vkb::PhysicalDevice _physical_device;
        vkb::Device _device;
        device_desc _desc;

        vkb::InstanceDispatchTable _instance_dispatch_table;
        vkb::DispatchTable _dispatch_table;
        VmaAllocator _allocator = VK_NULL_HANDLE;

        slot_map<buffer> _buffers;
        slot_map<texture> _textures;
        slot_map<texture_view> _texture_views;
        slot_map<sampler> _samplers;
        slot_map<graphics_pipeline> _graphics_pipelines;
        slot_map<compute_pipeline> _compute_pipelines;
        slot_map<VkSemaphore> _semaphores;
        slot_map<VkEvent> _events;
        slot_map<raw_surface> _raw_surfaces;

        unique_ptr<execution_port> _graphics_execution_port = nullptr;
        unique_ptr<execution_port> _async_compute_execution_port = nullptr;
        unique_ptr<execution_port> _async_transfer_execution_port = nullptr;

        static constexpr uint32_t max_active_samplers = 1024;
        static constexpr uint32_t max_active_textures = 4096;
        static constexpr uint32_t max_active_storage_images = 1024;

        // All pipelines created by the device will use this pipeline layout by default.
        // This allows us to emulate descriptor heaps.
        VkDescriptorSetLayout _storage_image_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout _sampled_image_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout _sampler_set_layout = VK_NULL_HANDLE;
        VkPipelineLayout _default_pipeline_layout = VK_NULL_HANDLE;

        // Descriptor buffer properties & memory
        size_t _sampler_descriptor_size = 0;
        size_t _sampled_image_descriptor_size = 0;
        size_t _storage_image_descriptor_size = 0;
        VkDeviceSize _descriptor_buffer_offset_alignment = 0;
        VkDeviceSize _storage_image_buffer_offset = 0;

        VkBuffer _sampler_descriptor_buffer = VK_NULL_HANDLE;
        VmaAllocation _sampler_descriptor_allocation = VK_NULL_HANDLE;
        byte* _sampler_descriptor_buffer_ptr = nullptr;
        VkDeviceAddress _sampler_descriptor_buffer_address = 0;

        VkBuffer _resource_descriptor_buffer = VK_NULL_HANDLE;
        VmaAllocation _resource_descriptor_allocation = VK_NULL_HANDLE;
        byte* _resource_descriptor_buffer_ptr = nullptr;
        VkDeviceAddress _resource_descriptor_buffer_address = 0;

        vector<descriptor_slot_state> _sampler_slots;
        vector<uint32_t> _sampler_free_list;
        vector<descriptor_slot_state> _sampled_image_slots;
        vector<uint32_t> _sampled_image_free_list;
        vector<descriptor_slot_state> _storage_image_slots;
        vector<uint32_t> _storage_image_free_list;
    };
} // namespace tempest::rhi::vk

#endif // tempest_rhi_vk_device_hpp
