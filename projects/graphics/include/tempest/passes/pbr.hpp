#ifndef tempst_graphics_passes_pbr_hpp
#define tempst_graphics_passes_pbr_hpp

#include <tempest/passes/pass.hpp>

#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>

namespace tempest::graphics::passes
{
    class pbr_pass
    {
      public:
        bool init(render_device& device);
        bool draw_batch(render_device& dev, command_list& cmds, const draw_command_state& state) const;
        void release(render_device& device);

      private:
        graphics_pipeline_resource_handle _pipeline;
    };

    class pbr_oit_gather_pass
    {
      public:
        bool init(render_device& device);
        bool draw_batch(render_device& dev, command_list& cmds, const draw_command_state& state) const;
        void release(render_device& device);

      private:
        graphics_pipeline_resource_handle _pipeline;
    };

    class pbr_oit_resolve_pass
    {
      public:
        bool init(render_device& device);
        bool draw_batch(render_device& dev, command_list& cmds, const draw_command_state& state) const;
        void release(render_device& device);

      private:
        graphics_pipeline_resource_handle _pipeline;
    };

    class pbr_oit_blend_pass
    {
      public:
        bool init(render_device& device);
        bool blend(render_device& dev, command_list& cmds) const;
        void release(render_device& device);

      private:
        graphics_pipeline_resource_handle _pipeline;
    };
} // namespace tempest::graphics::passes

#endif // tempst_graphics_passes_pbr_hpp
