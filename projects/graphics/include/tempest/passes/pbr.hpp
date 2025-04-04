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
        static constexpr descriptor_bind_point scene_constant_buffer_desc = {
            .type = descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC,
            .binding = 0,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point vertex_pull_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 1,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point mesh_layout_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 2,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point object_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 3,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point instance_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 4,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point materials_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 5,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point linear_sampler_desc = {
            .type = descriptor_binding_type::SAMPLER,
            .binding = 15,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point texture_array_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 16,
            .set = 0,
            .count = 512,
        };

        static constexpr descriptor_bind_point light_parameter_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 0,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point shadow_map_parameter_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 1,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point shadow_map_mt_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 2,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point light_grid_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 3,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point global_light_index_count_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 4,
            .set = 1,
            .count = 1,
        };

        bool init(render_device& device);
        bool draw_batch(render_device& dev, command_list& cmds, const draw_command_state& state) const;
        void release(render_device& device);

      private:
        graphics_pipeline_resource_handle _pipeline;
    };

    class pbr_oit_gather_pass
    {
      public:
        static constexpr descriptor_bind_point scene_constant_buffer_desc = {
            .type = descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC,
            .binding = 0,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point vertex_pull_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 1,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point mesh_layout_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 2,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point object_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 3,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point instance_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 4,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point materials_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 5,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point oit_moment_image_desc = {
            .type = descriptor_binding_type::STORAGE_IMAGE,
            .binding = 6,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point oit_zero_moment_image_desc = {
            .type = descriptor_binding_type::STORAGE_IMAGE,
            .binding = 7,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point linear_sampler_desc = {
            .type = descriptor_binding_type::SAMPLER,
            .binding = 15,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point texture_array_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 16,
            .set = 0,
            .count = 512,
        };

        static constexpr descriptor_bind_point light_parameter_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 0,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point shadow_map_parameter_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 1,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point shadow_map_mt_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 2,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point light_grid_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 3,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point global_light_index_count_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 4,
            .set = 1,
            .count = 1,
        };

        bool init(render_device& device);
        bool draw_batch(render_device& dev, command_list& cmds, const draw_command_state& state) const;
        void release(render_device& device);

      private:
        graphics_pipeline_resource_handle _pipeline;
    };

    class pbr_oit_resolve_pass
    {
      public:
        static constexpr descriptor_bind_point scene_constant_buffer_desc = {
            .type = descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC,
            .binding = 0,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point vertex_pull_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 1,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point mesh_layout_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 2,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point object_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 3,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point instance_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 4,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point materials_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 5,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point oit_moment_image_desc = {
            .type = descriptor_binding_type::STORAGE_IMAGE,
            .binding = 6,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point oit_zero_moment_image_desc = {
            .type = descriptor_binding_type::STORAGE_IMAGE,
            .binding = 7,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point linear_sampler_desc = {
            .type = descriptor_binding_type::SAMPLER,
            .binding = 15,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point texture_array_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 16,
            .set = 0,
            .count = 512,
        };

        static constexpr descriptor_bind_point light_parameter_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 0,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point shadow_map_parameter_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding = 1,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point shadow_map_mt_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 2,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point light_grid_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 3,
            .set = 1,
            .count = 1,
        };

        static constexpr descriptor_bind_point global_light_index_count_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding = 4,
            .set = 1,
            .count = 1,
        };

        bool init(render_device& device);
        bool draw_batch(render_device& dev, command_list& cmds, const draw_command_state& state) const;
        void release(render_device& device);

      private:
        graphics_pipeline_resource_handle _pipeline;
    };

    class pbr_oit_blend_pass
    {
      public:
        static constexpr descriptor_bind_point oit_moment_image_desc = {
            .type = descriptor_binding_type::STORAGE_IMAGE,
            .binding = 0,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point oit_zero_moment_image_desc = {
            .type = descriptor_binding_type::STORAGE_IMAGE,
            .binding = 1,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point oit_accum_image_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 2,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point linear_sampler_desc = {
            .type = descriptor_binding_type::SAMPLER,
            .binding = 3,
            .set = 0,
            .count = 1,
        };

        bool init(render_device& device);
        bool blend(render_device& dev, command_list& cmds) const;
        void release(render_device& device);

      private:
        graphics_pipeline_resource_handle _pipeline;
    };
} // namespace tempest::graphics::passes

#endif // tempst_graphics_passes_pbr_hpp
