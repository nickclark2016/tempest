#include <tempest/vk/render_surface.hpp>
#include <tempest/rhi.hpp>
#include <tempest/vk/device.hpp>
#include <tempest/vk/execution_port.hpp>

namespace tempest::rhi::vk
{
    render_surface::render_surface(swapchain_create_desc desc) : _swapchain_desc(tempest::move(desc))
    {
    }

    render_surface::~render_surface()
    {
        // Get us the vk device handle, avoid a vtable
        auto* const vk_device = static_cast<vk::device*>(_swapchain_desc.rhi_device);

        // Destroy the swapchain attachments
        for (const auto& attachment : _swapchain_desc.attachments)
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
        auto* const vk_device = static_cast<vk::device*>(_swapchain_desc.rhi_device);
        auto vk_sem = vk_device->get_semaphore(signal_values.semaphore);
        TEMPEST_ASSERT(vk_sem != VK_NULL_HANDLE);

        auto image_index = uint32_t{0};
        const auto result = vk_device->get_dispatch_table().acquireNextImageKHR(
            _swapchain_desc.swapchain, timeout_ns, vk_sem, VK_NULL_HANDLE, &image_index);

        if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
        {
            _last_acquired_index = image_index;
            TEMPEST_ASSERT(image_index < _swapchain_desc.attachments.size());
            const auto& attachment = _swapchain_desc.attachments[image_index];
            return swapchain_image{
                .texture = attachment.image,
                .view = attachment.view,
                .swapchain_image_index = image_index,
            };
        }

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            return unexpected{.value = swapchain_error::out_of_date};
        }

        if (result == VK_NOT_READY || result == VK_TIMEOUT)
        {
            return unexpected{.value = swapchain_error::unspecified};
        }

        return unexpected{.value = swapchain_error::unspecified};
    }

    auto render_surface::present(device_sync_point wait_values) -> expected<void, swapchain_error>
    {
        return present(_swapchain_desc.rhi_device->get_graphics_execution_port(), wait_values);
    }

    auto render_surface::present(rhi::execution_port& port, device_sync_point wait_values)
        -> expected<void, swapchain_error>
    {
        auto* const vk_device = static_cast<vk::device*>(_swapchain_desc.rhi_device);
        auto vk_sem = vk_device->get_semaphore(wait_values.semaphore);
        TEMPEST_ASSERT(vk_sem != VK_NULL_HANDLE);
        TEMPEST_ASSERT(_last_acquired_index < _swapchain_desc.attachments.size());

        auto& vk_port = static_cast<vk::execution_port&>(port);
        auto lock = lock_guard{vk_port.get_submit_mutex()};

        auto image_index = _last_acquired_index;
        auto result_code = VkResult{VK_SUCCESS};
        auto present_info = VkPresentInfoKHR{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &vk_sem,
            .swapchainCount = 1,
            .pSwapchains = &_swapchain_desc.swapchain,
            .pImageIndices = &image_index,
            .pResults = &result_code,
        };

        const auto result = vk_device->get_dispatch_table().queuePresentKHR(vk_port.get_queue(), &present_info);

        if (result == VK_SUCCESS)
        {
            return {};
        }

        if (result == VK_SUBOPTIMAL_KHR)
        {
            return unexpected{.value = swapchain_error::suboptimal};
        }

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            return unexpected{.value = swapchain_error::out_of_date};
        }

        return unexpected{.value = swapchain_error::unspecified};
    }
} // namespace tempest::rhi::vk