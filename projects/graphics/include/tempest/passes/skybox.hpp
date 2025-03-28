#ifndef tempest_graphics_passes_skybox_hpp
#define tempest_graphics_passes_skybox_hpp

#include <tempest/passes/pass.hpp>

#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>

namespace tempest::graphics::passes
{
    class skybox_pass
    {
      public:
        bool init(render_device& device);
        bool draw_batch(render_device& dev, command_list& cmds) const;
        void release(render_device& device);

      private:
        graphics_pipeline_resource_handle _pipeline;
    };
} // namespace tempest::graphics::passes

#endif
