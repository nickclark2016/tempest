#ifndef tempest_vk_renderer_hpp__
#define tempest_vk_renderer_hpp__

#include "glfw_window.hpp"
#include "vk_instance.hpp"

#include <tempest/renderer.hpp>

#include <vuk/Allocator.hpp>
#include <vuk/Context.hpp>
#include <vuk/RenderGraph.hpp>
#include <vuk/Swapchain.hpp>
#include <vuk/resources/DeviceFrameResource.hpp>

#include <array>
#include <unordered_map>

namespace tempest::graphics::vk
{
    namespace
    {
        constexpr std::size_t FRAMES_IN_FLIGHT = 3;
    }

    struct renderer_graph final : public irenderer_graph
    {
        irenderer_graph& set_final_target(render_target target) override;
        irenderer_graph& add_pass(const render_pass& pass) override;

        vuk::Future finalize(vuk::Future back_buffer);

        vuk::RenderGraph _vuk_graph;
        std::string_view _final_target_name;
    };

    class renderer final : public irenderer
    {
      public:
        renderer(iwindow& win);
        renderer(const renderer&) = delete;
        renderer(renderer&& other) noexcept = default;

        ~renderer() override;

        renderer& operator=(const renderer&) = delete;
        renderer& operator=(renderer&& rhs) noexcept = default;

        inline std::unique_ptr<irenderer_graph> create_render_graph() override
        {
            return std::make_unique<renderer_graph>();
        }

        void execute(irenderer_graph& graph) override;

      private:
        std::optional<vuk::Context> _ctx;
        std::optional<vuk::DeviceSuperFrameResource> _resources;
        std::optional<vuk::Allocator> _allocator;

        glfw::window* _win;
        vuk::SwapchainRef _swapchain_ref;

        instance _inst;

        vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>> _present_ready;
        vuk::Unique<std::array<VkSemaphore, FRAMES_IN_FLIGHT>> _render_complete;
        vuk::Compiler _compiler;

        VkQueue _graphics_queue;
        VkQueue _transfer_queue;
        VkQueue _compute_queue;

        VkSurfaceKHR _surface;

        void _release();
    };
} // namespace tempest::graphics::vk

#endif // renderer_hpp__
