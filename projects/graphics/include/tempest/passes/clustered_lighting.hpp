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
    
    struct alignas(16) light_grid_range
    {
        std::uint32_t offset;
        std::uint32_t range;
    };

    class build_cluster_grid_pass
    {
      public:
        static constexpr descriptor_bind_point light_cluster_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 0,
            .set = 0,
            .count = 1,
        };

        struct push_constants
        {
            math::mat4<float> inv_projection;
            math::vec4<float> screen_bounds;
            math::vec4<uint32_t> workgroup_count_tile_size;
        };

        bool init(render_device& device);
        bool execute(render_device& dev, command_list& cmds, const compute_command_state& state,
                     push_constants pc) const;
        void release(render_device& device);

      private:
        compute_pipeline_resource_handle _pipeline;
    };

    class cull_light_cluster_pass
    {
      public:
        struct push_constants
        {
            math::mat4<float> inv_projection;
            math::vec4<float> screen_bounds;
            math::vec4<uint32_t> workgroup_count_tile_size;
            uint32_t light_count;
        };

        static constexpr descriptor_bind_point light_cluster_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 0,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point light_parameter_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 1,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point global_light_index_list_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 2,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point light_grid_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 3,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point global_index_count_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 4,
            .set = 0,
            .count = 1,
        };

        bool init(render_device& device);
        bool execute(render_device& dev, command_list& cmds, const compute_command_state& state,
                     push_constants pc) const;
        void release(render_device& device);

      private:
        compute_pipeline_resource_handle _pipeline;
    };
} // namespace tempest::graphics::passes

#endif // tempest_graphics_passes_clustered_lighting_hpp