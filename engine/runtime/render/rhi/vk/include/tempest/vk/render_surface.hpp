#ifndef tempest_rhi_vk_render_surface_hpp
#define tempest_rhi_vk_render_surface_hpp

#include <tempest/api.hpp>
#include <tempest/rhi.hpp>
#include <tempest/vk/device.hpp>

#include <vulkan/vulkan.h>

namespace tempest::rhi::vk
{
    struct swapchain_attachment
    {
        texture_handle image;
        texture_view_handle view;
    };

    struct swapchain_create_desc
    {
        device* rhi_device;
        VkSurfaceKHR surface;
        VkSwapchainKHR swapchain;
        vector<swapchain_attachment> attachments;
        uint32_t width;
        uint32_t height;
        render_surface_format format;
        present_mode present;
    };

    class TEMPEST_API render_surface final : public rhi::render_surface
    {
      public:
        explicit render_surface(swapchain_create_desc desc);
        render_surface(const render_surface&) = delete;
        render_surface(render_surface&&) noexcept = delete;
        ~render_surface() override;

        render_surface& operator=(const render_surface&) = delete;     // NOLINT(modernize-use-trailing-return-type)
        render_surface& operator=(render_surface&&) noexcept = delete; // NOLINT(modernize-use-trailing-return-type)

        [[nodiscard]] auto get_format() const -> render_surface_format override;
        [[nodiscard]] auto get_present_mode() const -> present_mode override;
        [[nodiscard]] auto get_width() const -> uint32_t override;
        [[nodiscard]] auto get_height() const -> uint32_t override;
        [[nodiscard]] auto acquire_next_image(device_sync_point signal_values, uint64_t timeout_ns)
            -> expected<swapchain_image, swapchain_error> override;
        [[nodiscard]] auto present(device_sync_point wait_values) -> expected<void, swapchain_error> override;

      private:
        swapchain_create_desc _swapchain_desc;
    };
} // namespace tempest::rhi::vk

#endif // tempest_rhi_vk_render_surface_hpp
