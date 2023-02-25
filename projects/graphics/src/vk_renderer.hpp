#ifndef tempest_vk_renderer_hpp__
#define tempest_vk_renderer_hpp__

#include "vk_instance.hpp"
#include <tempest/renderer.hpp>

#include <VkBootstrap.h>

#include <array>

namespace tempest::graphics::vk
{
    namespace
    {
        inline constexpr std::size_t FRAMES_IN_FLIGHT = 2;
    }

    struct frame_payload
    {
        VkSemaphore present;
        VkSemaphore render;
        VkFence render_wait;
    };

    class renderer final : public irenderer
    {
      public:
        renderer(const iinstance& instance, const idevice& dev, const iwindow& win);
        renderer(const renderer&) = delete;
        renderer(renderer&& other) noexcept;

        ~renderer() override;

        renderer& operator=(const renderer&) = delete;
        renderer& operator=(renderer&& rhs) noexcept;

      private:
        vkb::Swapchain _swapchain;
        VkSurfaceKHR _surface{};

        vkb::Device _device{}; // non-owning
        vkb::Instance _inst{}; // non-owning
        vkb::DispatchTable _dispatch{}; // non-owning

        std::array<frame_payload, FRAMES_IN_FLIGHT> _frames{};
        std::vector<VkImageView> _swapchain_images;

        void _release();
    };
} // namespace tempest::graphics::vk

#endif // renderer_hpp__
