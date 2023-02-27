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

    class command_buffer final : public icommand_buffer
    {
      public:
        command_buffer(vuk::CommandBuffer* buf);
        ~command_buffer() override = default;

        icommand_buffer& use_full_viewport(std::uint32_t vp_index = 0) override;
        icommand_buffer& use_full_scissor(std::uint32_t sc_index = 0) override;
        icommand_buffer& use_default_raster_state() override;
        icommand_buffer& use_default_color_blend(std::string_view render_target_name) override;
        icommand_buffer& use_graphics_pipeline(std::string_view pipeline_name) override;
        icommand_buffer& draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
                              std::uint32_t first_instance) override;

      private:
        vuk::CommandBuffer* _buf;
    };

    struct renderer_graph final : public irenderer_graph
    {
        irenderer_graph& set_final_target(render_target target) override;
        irenderer_graph& add_pass(const render_pass& pass) override;

        vuk::Future finalize(vuk::Future back_buffer);

        vuk::RenderGraph _vuk_graph;
        std::string_view _final_target_name;
    };

    class resource_allocator final : public iresource_allocator
    {
      public:
        void set_context(vuk::Context& ctx);
        ~resource_allocator() override = default;

        void create_named_pipeline(std::span<shader_source> sources, std::string_view name) override;

      private:
        std::optional<std::reference_wrapper<vuk::Context>> _ctx;
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

        inline iresource_allocator& get_allocator() override
        {
            return *_resource_alloc;
        }

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

        std::optional<resource_allocator> _resource_alloc;

        void _release();
    };
} // namespace tempest::graphics::vk

#endif // renderer_hpp__
