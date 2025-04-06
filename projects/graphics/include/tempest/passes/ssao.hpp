#ifndef tempest_graphics_passes_ssao_hpp
#define tempest_graphics_passes_ssao_hpp

#include <tempest/passes/pass.hpp>
#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>

namespace tempest::graphics::passes
{
    class ssao_pass
    {
      public:
        static constexpr descriptor_bind_point scene_constants_buffer_desc = {
            .type = descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC,
            .binding = 0,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point depth_image_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 1,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point normal_image_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 2,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point noise_image_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 3,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point linear_sampler_desc = {
            .type = descriptor_binding_type::SAMPLER,
            .binding = 4,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point point_sampler_desc = {
            .type = descriptor_binding_type::SAMPLER,
            .binding = 5,
            .set = 0,
            .count = 1,
        };

        struct constants
        {
            math::mat4<float> projection;
            math::mat4<float> inv_projection;
            math::mat4<float> view;
            math::mat4<float> inv_view;
            array<math::vec4<float>, 64> kernel;
            math::vec2<float> noise_scale;
            float radius;
            float bias;
        };

        bool init(render_device& device);
        bool draw_batch(render_device& device, command_list& cmds) const;
        void release(render_device& device);

        image_resource_handle noise_image() const noexcept
        {
            return _noise_image;
        }

        span<const math::vec4<float>> kernel() const noexcept
        {
            return _kernel;
        }

        math::vec2<float> noise_scale(float width, float height) const noexcept
        {
            return {
                width / noise_size,
                height / noise_size,
            };
        }

      private:
        graphics_pipeline_resource_handle _pipeline;
        image_resource_handle _noise_image;

        tempest::array<math::vec4<float>, 64> _kernel; // SSAO kernel

        static constexpr size_t noise_size = 16;
    };

    class ssao_blur_pass
    {
      public:
        static constexpr descriptor_bind_point ssao_image_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding = 0,
            .set = 0,
            .count = 1,
        };

        static constexpr descriptor_bind_point point_sampler_desc = {
            .type = descriptor_binding_type::SAMPLER,
            .binding = 1,
            .set = 0,
            .count = 1,
        };

        bool init(render_device& device);
        bool draw_batch(render_device& device, command_list& cmds) const;
        void release(render_device& device);

      private:
        graphics_pipeline_resource_handle _pipeline;
    };
} // namespace tempest::graphics::passes

#endif // tempest_graphics_passes_ssao_hpp
