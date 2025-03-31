#ifndef tempest_graphics_passes_clustered_lighting_hpp
#define tempest_graphics_passes_clustered_lighting_hpp

#include <tempest/passes/pass.hpp>

#include <tempest/mat4.hpp>
#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>

namespace tempest::graphics::passes
{
    struct lighting_cluster_bounds
    {
        math::vec4<float> min_bounds;
        math::vec4<float> max_bounds;
    };

    class build_cluster_grid_pass
    {
      public:
        struct push_constants
        {
            math::mat4<float> inv_projection;
            math::vec4<float> screen_bounds;
            math::vec4<uint32_t> workgroup_count_tile_size;
        };

        bool init(render_device& device);
        bool execute(render_device& dev, command_list& cmds, const compute_command_state& state, push_constants pc) const;
        void release(render_device& device);

      private:
        compute_pipeline_resource_handle _pipeline;
    };
} // namespace tempest::graphics::passes

#endif // tempest_graphics_passes_clustered_lighting_hpp