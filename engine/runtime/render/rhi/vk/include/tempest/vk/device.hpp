#ifndef tempest_rhi_vk_device_hpp
#define tempest_rhi_vk_device_hpp

#include <tempest/rhi.hpp>
#include <tempest/slot_map.hpp>

#include <VkBootstrap.h>
#include <VkBootstrapDispatch.h>

namespace tempest::rhi::vk
{
    class execution_port;

    class TEMPEST_API device final : public rhi::device
    {
      public:
        static auto create(vkb::PhysicalDevice physical_device, vkb::Device device, device_desc desc)
            -> unique_ptr<rhi::device>;

        device(const device&) = delete;
        device(device&&) noexcept = delete;
        ~device() override;

        device& operator=(const device&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        device& operator=(device&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        auto wait_idle() -> void override;
        auto wait_for_sync(host_sync_point sync_point) -> void override;

        [[nodiscard]] auto is_ray_tracing_supported() const -> bool override;
        [[nodiscard]] auto is_mesh_shading_supported() const -> bool override;
        [[nodiscard]] auto is_ray_query_supported() const -> bool override;

        [[nodiscard]] auto get_surface_capabilities(render_surface& surface) -> surface_capabilities override;
        [[nodiscard]] auto create_render_surface(const render_surface_desc& desc)
            -> unique_ptr<render_surface> override;

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

        [[nodiscard]] auto get_buffer(buffer_handle handle) const -> VkBuffer
        {
            const auto iter = _buffers.find(handle.handle);
            if (iter == _buffers.end())
            {
                return VK_NULL_HANDLE;
            }
            return *iter;
        }

        [[nodiscard]] auto get_texture(texture_handle handle) const -> VkImage
        {
            const auto iter = _textures.find(handle.handle);
            if (iter == _textures.end())
            {
                return VK_NULL_HANDLE;
            }
            return *iter;
        }

        [[nodiscard]] auto get_texture_view(texture_view_handle handle) const -> VkImageView
        {
            return _texture_views.at(handle.handle);
        }

        [[nodiscard]] auto get_sampler(sampler_handle handle) const -> VkSampler
        {
            const auto iter = _samplers.find(handle.handle);
            if (iter == _samplers.end())
            {
                return VK_NULL_HANDLE;
            }
            return *iter;
        }

        [[nodiscard]] auto get_graphics_pipeline(graphics_pipeline_handle handle) const -> VkPipeline
        {
            const auto iter = _graphics_pipelines.find(handle.handle);
            if (iter == _graphics_pipelines.end())
            {
                return VK_NULL_HANDLE;
            }
            return *iter;
        }

        [[nodiscard]] auto get_compute_pipeline(compute_pipeline_handle handle) const -> VkPipeline
        {
            const auto iter = _compute_pipelines.find(handle.handle);
            if (iter == _compute_pipelines.end())
            {
                return VK_NULL_HANDLE;
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

      private:
        device(vkb::PhysicalDevice physical_device, vkb::Device device, device_desc desc);

        vkb::PhysicalDevice _physical_device;
        vkb::Device _device;
        device_desc _desc;

        vkb::DispatchTable _dispatch_table;

        slot_map<VkBuffer> _buffers;
        slot_map<VkImage> _textures;
        slot_map<VkImageView> _texture_views;
        slot_map<VkSampler> _samplers;
        slot_map<VkPipeline> _graphics_pipelines;
        slot_map<VkPipeline> _compute_pipelines;
        slot_map<VkSemaphore> _semaphores;
        slot_map<VkEvent> _events;

        unique_ptr<execution_port> _graphics_execution_port = nullptr;
        unique_ptr<execution_port> _async_compute_execution_port = nullptr;
        unique_ptr<execution_port> _async_transfer_execution_port = nullptr;
    };
} // namespace tempest::rhi::vk

#endif // tempest_rhi_vk_device_hpp
