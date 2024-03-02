#ifndef tempest_graphcis_render_system_hpp
#define tempest_graphcis_render_system_hpp

#include "render_device.hpp"
#include "render_graph.hpp"
#include "window.hpp"

#include <tempest/registry.hpp>
#include <tempest/memory.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace tempest::graphics
{
    class render_system
    {
      public:
        render_system(ecs::registry& entities);

        void register_window(iwindow& win);
        void unregister_window(iwindow& win);

        void on_initialize();

        void render();

        void on_close();

      private:
        core::heap_allocator _allocator;
        ecs::registry _registry;

        std::unique_ptr<render_context> _context;
        render_device* _device;
        std::unique_ptr<render_graph> _graph;
        std::unordered_map<iwindow*, swapchain_resource_handle> _swapchains;

        std::vector<image_resource_handle> _images;
        std::vector<buffer_resource_handle> _buffers;
        std::vector<graphics_pipeline_resource_handle> _graphics_pipelines;
        std::vector<compute_pipeline_resource_handle> _compute_pipelines;
        std::vector<sampler_resource_handle> _samplers;
    };
}

#endif // tempest_graphcis_render_system_hpp