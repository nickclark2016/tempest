#include "tempest/vk/device.hpp"
#include <tempest/rhi.hpp>
#include <tempest/vk/render_surface.hpp>

namespace tempest::rhi::vk
{
    render_surface::render_surface(swapchain_create_desc desc) : _swapchain_desc(tempest::move(desc))
    {
    }

    render_surface::~render_surface()
    {
        // Get us the vk device handle, avoid a vtable
        auto* vk_device = static_cast<vk::device*>(_swapchain_desc.rhi_device);

        // Destroy the swapchain attachments
        for (auto attachment : _swapchain_desc.attachments)
        {
            vk_device->destroy_texture_view(attachment.view);
            vk_device->release_texture_handle(attachment.image);
        }

        // Destroy the swapchain
        if (_swapchain_desc.swapchain != VK_NULL_HANDLE)
        {
            vk_device->get_dispatch_table().destroySwapchainKHR(_swapchain_desc.swapchain, nullptr);
        }
    }

    auto render_surface::get_format() const -> render_surface_format
    {
        return _swapchain_desc.format;
    }

    auto render_surface::get_present_mode() const -> present_mode
    {
        return _swapchain_desc.present;
    }

    auto render_surface::get_width() const -> uint32_t
    {
        return _swapchain_desc.width;
    }

    auto render_surface::get_height() const -> uint32_t
    {
        return _swapchain_desc.height;
    }

    auto render_surface::acquire_next_image(device_sync_point signal_values, uint64_t timeout_ns)
        -> expected<swapchain_image, swapchain_error>
    {
        return unexpected{.value = swapchain_error::unspecified};
    }

    auto render_surface::present(device_sync_point wait_values) -> expected<void, swapchain_error>
    {
        return unexpected{.value = swapchain_error::unspecified};
    }
} // namespace tempest::rhi::vk