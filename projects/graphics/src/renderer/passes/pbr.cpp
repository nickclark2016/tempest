#include <tempest/passes/pbr.hpp>

#include <tempest/files.hpp>

namespace tempest::graphics::passes
{
    namespace
    {
        // Bindings
        descriptor_binding_info scene_constant_buffer = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding_index = 0,
            .binding_count = 1,
        };

        descriptor_binding_info vertex_pull_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding_index = 1,
            .binding_count = 1,
        };

        descriptor_binding_info mesh_layout_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding_index = 2,
            .binding_count = 1,
        };

        descriptor_binding_info object_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding_index = 3,
            .binding_count = 1,
        };

        descriptor_binding_info materials_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding_index = 4,
            .binding_count = 1,
        };

        descriptor_binding_info instance_buffer_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER,
            .binding_index = 5,
            .binding_count = 1,
        };

        descriptor_binding_info linear_sampler_desc = {
            .type = descriptor_binding_type::SAMPLER,
            .binding_index = 6,
            .binding_count = 1,
        };

        descriptor_binding_info texture_array_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding_index = 7,
            .binding_count = 512,
        };

        descriptor_binding_info light_parameter_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding_index = 0,
            .binding_count = 1,
        };

        descriptor_binding_info shadow_map_parameter_desc = {
            .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding_index = 1,
            .binding_count = 1,
        };

        descriptor_binding_info shadow_map_mt_desc = {
            .type = descriptor_binding_type::SAMPLED_IMAGE,
            .binding_index = 2,
            .binding_count = 1,
        };
    } // namespace

    bool pbr_pass::init(render_device& device)
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/pbr.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/pbr.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            scene_constant_buffer, vertex_pull_buffer_desc, mesh_layout_buffer_desc, object_buffer_desc,
            instance_buffer_desc,  materials_buffer_desc,   linear_sampler_desc,     texture_array_desc,
        };

        descriptor_binding_info set1_bindings[] = {
            light_parameter_desc,
            shadow_map_parameter_desc,
            shadow_map_mt_desc,
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
            {
                .set = 1,
                .bindings = set1_bindings,
            },
        };

        resource_format color_buffer_fmt[] = {
            resource_format::RGBA8_SRGB,
            resource_format::RG32_FLOAT,
        };

        color_blend_attachment_state blending[] = {
            {
                // Color Buffer
                .enabled = false,
                .color = {},
                .alpha = {},
            },
            {
                // Velocity Buffer
                .enabled = false,
                .color = {},
                .alpha = {},
            },
        };

        auto pipeline = device.create_graphics_pipeline({
            .layout{
                .set_layouts = layouts,
            },
            .target{
                .color_attachment_formats = color_buffer_fmt,
                .depth_attachment_format = resource_format::D24_FLOAT,
            },
            .vertex_shader{
                .bytes = vertex_shader_source,
                .entrypoint = "main",
                .name = "PBR Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "PBR Shader Module",
            },
            .depth_testing{
                .enable_test = true,
                .enable_write = true,
                .depth_test_op = compare_operation::GREATER_OR_EQUALS,
            },
            .blending{
                .attachment_blend_ops = blending,
            },
            .name = "PBR Graphics Pipeline",
        });

        _pipeline = pipeline;

        return pipeline != graphics_pipeline_resource_handle{};
    }

    bool pbr_pass::draw_batch(render_device& dev, command_list& cmds, const draw_command_state& state)
    {
        auto indirect_buffer_frame_offset = dev.get_buffer_frame_offset(state.indirect_command_buffer);

        cmds.set_cull_mode(false, !state.double_sided)
            .use_pipeline(_pipeline)
            .draw_indexed(state.indirect_command_buffer,
                          state.first_indirect_command * sizeof(indexed_indirect_command) +
                              indirect_buffer_frame_offset,
                          state.indirect_command_count, sizeof(indexed_indirect_command));

        return false;
    }

    void pbr_pass::release(render_device& device)
    {
        if (_pipeline)
        {
            device.release_graphics_pipeline(_pipeline);
        }
    }

    bool pbr_oit_pass::init(render_device& device)
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/pbr_oit.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/pbr_oit.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            scene_constant_buffer, vertex_pull_buffer_desc, mesh_layout_buffer_desc, object_buffer_desc,
            instance_buffer_desc,  materials_buffer_desc,   linear_sampler_desc,     texture_array_desc,
        };

        descriptor_binding_info set1_bindings[] = {
            light_parameter_desc,
            shadow_map_parameter_desc,
            shadow_map_mt_desc,
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
            {
                .set = 1,
                .bindings = set1_bindings,
            },
        };

        resource_format color_buffer_fmt[] = {
            resource_format::R32_UINT,
            resource_format::RG32_FLOAT,
        };

        color_blend_attachment_state blending[] = {
            {
                // Color Buffer
                .enabled = false,
                .color = {},
                .alpha = {},
            },
            {
                // Velocity Buffer
                .enabled = false,
                .color = {},
                .alpha = {},
            },
        };

        auto pipeline = device.create_graphics_pipeline({
            .layout{
                .set_layouts = layouts,
            },
            .target{
                .color_attachment_formats = color_buffer_fmt,
                .depth_attachment_format = resource_format::D24_FLOAT,
            },
            .vertex_shader{
                .bytes = vertex_shader_source,
                .entrypoint = "main",
                .name = "PBR OIT Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "PBR OIT Shader Module",
            },
            .depth_testing{
                .enable_test = true,
                .enable_write = true,
                .depth_test_op = compare_operation::GREATER_OR_EQUALS,
            },
            .blending{
                .attachment_blend_ops = blending,
            },
            .name = "PBR OIT Graphics Pipeline",
        });

        _pipeline = pipeline;

        return pipeline != graphics_pipeline_resource_handle{};
    }

    bool pbr_oit_pass::draw_batch(render_device& dev, command_list& cmds, const draw_command_state& state)
    {
        return false;
    }

    void pbr_oit_pass::release(render_device& device)
    {
        if (_pipeline)
        {
            device.release_graphics_pipeline(_pipeline);
        }
    }
} // namespace tempest::graphics::passes