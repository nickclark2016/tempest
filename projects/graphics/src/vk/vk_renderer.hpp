#ifndef tempest_vk_renderer_hpp__
#define tempest_vk_renderer_hpp__

#include <tempest/renderer.hpp>
#include <tempest/window.hpp>

#include <VkBootstrap.h>
#include <vuk/Allocator.hpp>
#include <vuk/Context.hpp>
#include <vuk/Future.hpp>
#include <vuk/RenderGraph.hpp>
#include <vuk/resources/DeviceFrameResource.hpp>

#include <array>
#include <mutex>
#include <optional>
#include <vector>

namespace tempest::graphics
{
    namespace
    {
        inline constexpr std::size_t FRAMES_IN_FLIGHT = 3;
    }

    struct irenderer::impl
    {
        vkb::Instance instance{};
        vkb::PhysicalDevice physical_device{};
        vkb::Device logical_device{};

        std::optional<vuk::Context> vuk_context;
        std::optional<vuk::DeviceSuperFrameResource> superframe_resources;
        std::optional<vuk::Allocator> vuk_allocator;

        bool rendering_suspended{false};

        VkQueue gfx_queue{};
        VkQueue transfer_queue{};
        VkQueue compute_queue{};

        vuk::Compiler compiler;

        vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>> present_ready;
        vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>> render_complete;

        vuk::SwapchainRef swapchain;
        VkSurfaceKHR surface;

        std::mutex setup_lock;
        std::vector<vuk::Future> resource_futures;

        std::optional<std::reference_wrapper<iwindow>> win;

        double frame_time;
        std::size_t frame_counter;
    };
} // namespace tempest::graphics

#endif // tempest_vk_renderer_hpp__