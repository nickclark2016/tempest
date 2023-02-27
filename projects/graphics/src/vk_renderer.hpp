#ifndef tempest_vk_renderer_hpp__
#define tempest_vk_renderer_hpp__

#include "vk_instance.hpp"
#include <tempest/renderer.hpp>

#include <vuk/Allocator.hpp>
#include <vuk/Context.hpp>
#include <vuk/resources/DeviceFrameResource.hpp>

#include <array>

namespace tempest::graphics::vk
{
    namespace
    {
        constexpr std::size_t FRAMES_IN_FLIGHT = 3;
    }

    class renderer final : public irenderer
    {
      public:
        renderer();
        renderer(const renderer&) = delete;
        renderer(renderer&& other) noexcept = default;

        ~renderer() override;

        renderer& operator=(const renderer&) = delete;
        renderer& operator=(renderer&& rhs) noexcept = default;

        void draw() override;

      private:
        std::optional<vuk::Context> _ctx;
        std::optional<vuk::DeviceSuperFrameResource> _resources;
        std::optional<vuk::Allocator> _allocator;

        instance _inst;

        vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>> _present_ready;
        vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>> _render_complete;

        VkQueue _graphics_queue;
        VkQueue _transfer_queue;
        VkQueue _compute_queue;

        void _release();
    };
} // namespace tempest::graphics::vk

#endif // renderer_hpp__
