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
        static constexpr descriptor_bind_point scene_constant_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 0,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point skybox_texture_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 1,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point linear_sampler_desc = {
            .type = descriptor_binding_type::SAMPLER,
            .binding = 2,
            .set = 0,
            .count = 1,
        };

        bool init(render_device& device);
        bool draw_batch(render_device& dev, command_list& cmds) const;
        void release(render_device& device);

      private:
        graphics_pipeline_resource_handle _pipeline;
    };
} // namespace tempest::graphics::passes

#endif
