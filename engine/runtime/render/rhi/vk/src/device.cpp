#include <tempest/vk/device.hpp>

#include <tempest/vk/execution_port.hpp>

#include <vulkan/vulkan.hpp>

namespace tempest::rhi::vk
{
    auto device::create(vkb::PhysicalDevice physical_device, vkb::Device dev, device_desc desc)
        -> unique_ptr<rhi::device>
    {
        return unique_ptr<rhi::device>{
            new device{tempest::move(physical_device), tempest::move(dev), tempest::move(desc)}};
    }

    device::~device()
    {
        vkb::destroy_device(_device);
    }

    auto device::wait_idle() -> void
    {
        _dispatch_table.deviceWaitIdle();
    }

    auto device::wait_for_sync(host_sync_point sync_point) -> void
    {
        auto* sem = get_semaphore(sync_point.semaphore);
        auto timeline_info = VkSemaphoreWaitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .pNext = nullptr,
            .flags = 0,
            .semaphoreCount = 1,
            .pSemaphores = &sem,
            .pValues = &sync_point.value,
        };
        _dispatch_table.waitSemaphores(&timeline_info, UINT64_MAX);
    }

    auto device::is_ray_tracing_supported() const -> bool
    {
        return _desc.features.ray_tracing;
    }

    auto device::is_mesh_shading_supported() const -> bool
    {
        return _desc.features.mesh_shading;
    }

    auto device::is_ray_query_supported() const -> bool
    {
        return _desc.features.ray_query;
    }

    auto device::get_surface_capabilities(render_surface& surface) -> surface_capabilities
    {
        return {};
    }

    auto device::create_render_surface(const render_surface_desc& desc) -> unique_ptr<render_surface>
    {
        return nullptr;
    }

    auto device::get_graphics_execution_port() -> rhi::execution_port&
    {
        return *_graphics_execution_port;
    }

    auto device::get_async_compute_execution_port() -> rhi::execution_port&
    {
        return *_async_compute_execution_port;
    }

    auto device::get_async_transfer_execution_port() -> rhi::execution_port&
    {
        return *_async_transfer_execution_port;
    }

    auto device::create_buffer(const buffer_desc& desc) -> buffer_handle
    {
        return {};
    }

    auto device::create_texture(const texture_desc& desc) -> texture_handle
    {
        return {};
    }

    auto device::create_texture_view(texture_handle texture, const texture_view_desc& desc) -> texture_view_handle
    {
        return {};
    }

    auto device::create_sampler(const sampler_desc& desc) -> sampler_handle
    {
        return {};
    }

    auto device::create_graphics_pipeline(const graphics_pipeline_desc& desc) -> graphics_pipeline_handle
    {
        return {};
    }

    auto device::create_compute_pipeline(const compute_pipeline_desc& desc) -> compute_pipeline_handle
    {
        return {};
    }

    auto device::create_event() -> event_handle
    {
        return {};
    }

    auto device::create_timeline_semaphore() -> semaphore_handle
    {
        return {};
    }

    auto device::create_binary_semaphore() -> semaphore_handle
    {
        return {};
    }

    auto device::destroy_buffer(buffer_handle buffer) -> void
    {
    }

    auto device::destroy_texture(texture_handle texture) -> void
    {
    }

    auto device::destroy_texture_view(texture_view_handle view) -> void
    {
    }

    auto device::destroy_sampler(sampler_handle sampler) -> void
    {
    }

    auto device::destroy_graphics_pipeline(graphics_pipeline_handle pipeline) -> void
    {
    }

    auto device::destroy_compute_pipeline(compute_pipeline_handle pipeline) -> void
    {
    }

    auto device::destroy_event(event_handle event) -> void
    {
    }

    auto device::destroy_semaphore(semaphore_handle semaphore) -> void
    {
    }

    device::device(vkb::PhysicalDevice physical_device, vkb::Device dev, device_desc desc)
        : _physical_device{tempest::move(physical_device)}, _device{tempest::move(dev)}, _desc{tempest::move(desc)},
          _dispatch_table{_device.make_table()}
    {
    }
} // namespace tempest::rhi::vk