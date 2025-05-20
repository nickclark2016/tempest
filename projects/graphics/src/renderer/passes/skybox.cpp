#include <tempest/passes/skybox.hpp>

#include <tempest/files.hpp>

namespace tempest::graphics::passes
{
    bool skybox_pass::init(render_device& device)
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/skybox.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/skybox.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            skybox_pass::scene_constant_buffer_desc.to_binding_info(),
            skybox_pass::skybox_texture_desc.to_binding_info(),
            skybox_pass::linear_sampler_desc.to_binding_info(),
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
        };

        color_blend_attachment_state blending[] = {
            {
                // Skybox Color Buffer
                .enabled = false,
                .color = {},
                .alpha = {},
            },
        };

        resource_format color_formats[] = {resource_format::rgba8_srgb};
        auto pipeline = device.create_graphics_pipeline({
            .layout{
                .set_layouts = layouts,
            },
            .target{
                .color_attachment_formats = color_formats,
            },
            .vertex_shader{
                .bytes = vertex_shader_source,
                .entrypoint = "main",
                .name = "Skybox Vertex Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "Skybox Fragment Shader Module",
            },
            .depth_testing{
                .enable_test = false,
                .enable_write = false,
                .depth_test_op = compare_operation::NEVER,
            },
            .blending{
                // Disable blending for the skybox
                .attachment_blend_ops = blending,
            },
            .name = "Skybox Graphics Pipeline",
        });
        _pipeline = pipeline;
        return pipeline != graphics_pipeline_resource_handle{};
    }

    bool skybox_pass::draw_batch([[maybe_unused]] render_device& dev, command_list& cmds) const
    {
        cmds.set_cull_mode(false, true)
            .use_pipeline(_pipeline)
            .draw(3, 1, 0, 0); // Draw a single triangle for the skybox

        return true;
    }

    void skybox_pass::release(render_device& device)
    {
        if (_pipeline)
        {
            device.release_graphics_pipeline(_pipeline);
        }
    }
} // namespace tempest::graphics::passes