#include <tempest/passes/pbr.hpp>

#include <tempest/files.hpp>

namespace tempest::graphics::passes
{
    bool pbr_pass::init(render_device& device)
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/pbr.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/pbr.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            pbr_pass::scene_constant_buffer_desc.to_binding_info(),
            pbr_pass::vertex_pull_buffer_desc.to_binding_info(),
            pbr_pass::mesh_layout_buffer_desc.to_binding_info(),
            pbr_pass::object_buffer_desc.to_binding_info(),
            pbr_pass::instance_buffer_desc.to_binding_info(),
            pbr_pass::materials_buffer_desc.to_binding_info(),
            pbr_pass::ao_image_desc.to_binding_info(),
            pbr_pass::linear_sampler_desc.to_binding_info(),
            pbr_pass::texture_array_desc.to_binding_info(),
        };

        descriptor_binding_info set1_bindings[] = {
            pbr_pass::light_parameter_desc.to_binding_info(),
            pbr_pass::shadow_map_parameter_desc.to_binding_info(),
            pbr_pass::shadow_map_mt_desc.to_binding_info(),
            pbr_pass::light_grid_desc.to_binding_info(),
            pbr_pass::global_light_index_count_desc.to_binding_info(),
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
            resource_format::rgba8_srgb,
        };

        color_blend_attachment_state blending[] = {
            {
                // Color Buffer
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
                .name = "PBR Opaque Vertex Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "PBR Opaque Fragment Shader Module",
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

    bool pbr_pass::draw_batch(render_device& dev, command_list& cmds, const draw_command_state& state) const
    {
        auto indirect_buffer_frame_offset = dev.get_buffer_frame_offset(state.indirect_command_buffer);

        auto offset = static_cast<uint32_t>(state.first_indirect_command * sizeof(indexed_indirect_command) +
                                            indirect_buffer_frame_offset);
        auto count = static_cast<uint32_t>(state.indirect_command_count);
        auto size = static_cast<uint32_t>(sizeof(indexed_indirect_command));

        cmds.set_cull_mode(false, !state.double_sided)
            .use_pipeline(_pipeline)
            .draw_indexed(state.indirect_command_buffer, offset, count, size);

        return true;
    }

    void pbr_pass::release(render_device& device)
    {
        if (_pipeline)
        {
            device.release_graphics_pipeline(_pipeline);
        }
    }

    bool pbr_oit_gather_pass::init(render_device& device)
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/pbr_oit_gather.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/pbr_oit_gather.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            pbr_oit_gather_pass::scene_constant_buffer_desc.to_binding_info(),
            pbr_oit_gather_pass::vertex_pull_buffer_desc.to_binding_info(),
            pbr_oit_gather_pass::mesh_layout_buffer_desc.to_binding_info(),
            pbr_oit_gather_pass::object_buffer_desc.to_binding_info(),
            pbr_oit_gather_pass::instance_buffer_desc.to_binding_info(),
            pbr_oit_gather_pass::materials_buffer_desc.to_binding_info(),
            pbr_oit_gather_pass::oit_moment_image_desc.to_binding_info(),
            pbr_oit_gather_pass::oit_zero_moment_image_desc.to_binding_info(),
            pbr_oit_gather_pass::ao_image_desc.to_binding_info(),
            pbr_oit_gather_pass::linear_sampler_desc.to_binding_info(),
            pbr_oit_gather_pass::texture_array_desc.to_binding_info(),
        };

        descriptor_binding_info set1_bindings[] = {
            pbr_oit_gather_pass::light_parameter_desc.to_binding_info(),
            pbr_oit_gather_pass::shadow_map_parameter_desc.to_binding_info(),
            pbr_oit_gather_pass::shadow_map_mt_desc.to_binding_info(),
            pbr_oit_gather_pass::light_grid_desc.to_binding_info(),
            pbr_oit_gather_pass::global_light_index_count_desc.to_binding_info(),
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

        resource_format color_formats[] = {resource_format::RGBA16_FLOAT};

        color_blend_attachment_state blending[] = {
            {
                .enabled = false,
                .write_enabled = false,
            },
        };

        auto pipeline = device.create_graphics_pipeline({
            .layout{
                .set_layouts = layouts,
            },
            .target{
                .color_attachment_formats = color_formats,
                .depth_attachment_format = resource_format::D24_FLOAT,
            },
            .vertex_shader{
                .bytes = vertex_shader_source,
                .entrypoint = "main",
                .name = "PBR OIT Gather Vertex Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "PBR OIT Gather Fragment Shader Module",
            },
            .depth_testing{
                .enable_test = true,
                .enable_write = false,
                .depth_test_op = compare_operation::GREATER_OR_EQUALS,
            },
            .blending = {blending},
            .name = "PBR OIT Gather Graphics Pipeline",
        });

        _pipeline = pipeline;

        return pipeline != graphics_pipeline_resource_handle{};
    }

    bool pbr_oit_gather_pass::draw_batch(render_device& dev, command_list& cmds, const draw_command_state& state) const
    {
        auto indirect_buffer_frame_offset = dev.get_buffer_frame_offset(state.indirect_command_buffer);

        auto offset = static_cast<uint32_t>(state.first_indirect_command * sizeof(indexed_indirect_command) +
                                            indirect_buffer_frame_offset);
        auto count = static_cast<uint32_t>(state.indirect_command_count);
        auto size = static_cast<uint32_t>(sizeof(indexed_indirect_command));

        cmds.set_cull_mode(false, !state.double_sided)
            .use_pipeline(_pipeline)
            .draw_indexed(state.indirect_command_buffer, offset, count, size);

        return true;
    }

    void pbr_oit_gather_pass::release(render_device& device)
    {
        if (_pipeline)
        {
            device.release_graphics_pipeline(_pipeline);
        }
    }

    bool pbr_oit_resolve_pass::init(render_device& device)
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/pbr_oit_resolve.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/pbr_oit_resolve.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            pbr_oit_resolve_pass::scene_constant_buffer_desc.to_binding_info(),
            pbr_oit_resolve_pass::vertex_pull_buffer_desc.to_binding_info(),
            pbr_oit_resolve_pass::mesh_layout_buffer_desc.to_binding_info(),
            pbr_oit_resolve_pass::object_buffer_desc.to_binding_info(),
            pbr_oit_resolve_pass::instance_buffer_desc.to_binding_info(),
            pbr_oit_resolve_pass::materials_buffer_desc.to_binding_info(),
            pbr_oit_resolve_pass::oit_moment_image_desc.to_binding_info(),
            pbr_oit_resolve_pass::oit_zero_moment_image_desc.to_binding_info(),
            pbr_oit_resolve_pass::ao_image_desc.to_binding_info(),
            pbr_oit_resolve_pass::linear_sampler_desc.to_binding_info(),
            pbr_oit_resolve_pass::texture_array_desc.to_binding_info(),
        };

        descriptor_binding_info set1_bindings[] = {
            pbr_oit_resolve_pass::light_parameter_desc.to_binding_info(),
            pbr_oit_resolve_pass::shadow_map_parameter_desc.to_binding_info(),
            pbr_oit_resolve_pass::shadow_map_mt_desc.to_binding_info(),
            pbr_oit_resolve_pass::light_grid_desc.to_binding_info(),
            pbr_oit_resolve_pass::global_light_index_count_desc.to_binding_info(),
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

        color_blend_attachment_state blending[] = {
            {
                .enabled = true,
                .color =
                    {
                        .src = blend_factor::ONE,
                        .dst = blend_factor::ONE,
                        .op = blend_operation::ADD,
                    },
                .alpha =
                    {
                        .src = blend_factor::ONE,
                        .dst = blend_factor::ONE,
                        .op = blend_operation::ADD,
                    },
            },
        };

        resource_format color_formats[] = {resource_format::RGBA16_FLOAT};

        auto pipeline = device.create_graphics_pipeline({
            .layout{
                .set_layouts = layouts,
            },
            .target{
                .color_attachment_formats = color_formats,
                .depth_attachment_format = resource_format::D24_FLOAT,
            },
            .vertex_shader{
                .bytes = vertex_shader_source,
                .entrypoint = "main",
                .name = "PBR OIT Resolve Vertex Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "PBR OIT Resolve Fragment Shader Module",
            },
            .depth_testing{
                .enable_test = true,
                .enable_write = false,
                .depth_test_op = compare_operation::GREATER_OR_EQUALS,
            },
            .blending = {blending},
            .name = "PBR OIT Resolve Graphics Pipeline",
        });

        _pipeline = pipeline;

        return pipeline != graphics_pipeline_resource_handle{};
    }

    bool pbr_oit_resolve_pass::draw_batch(render_device& dev, command_list& cmds, const draw_command_state& state) const
    {
        auto indirect_buffer_frame_offset = dev.get_buffer_frame_offset(state.indirect_command_buffer);

        auto offset = static_cast<uint32_t>(state.first_indirect_command * sizeof(indexed_indirect_command) +
                                            indirect_buffer_frame_offset);
        auto count = static_cast<uint32_t>(state.indirect_command_count);
        auto size = static_cast<uint32_t>(sizeof(indexed_indirect_command));

        cmds.set_cull_mode(false, !state.double_sided)
            .use_pipeline(_pipeline)
            .draw_indexed(state.indirect_command_buffer, offset, count, size);

        return true;
    }

    void pbr_oit_resolve_pass::release(render_device& device)
    {
        if (_pipeline)
        {
            device.release_graphics_pipeline(_pipeline);
        }
    }

    bool pbr_oit_blend_pass::init(render_device& device)
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/pbr_oit_blend.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/pbr_oit_blend.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            oit_moment_image_desc.to_binding_info(),
            oit_zero_moment_image_desc.to_binding_info(),
            oit_accum_image_desc.to_binding_info(),
            linear_sampler_desc.to_binding_info(),
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
        };

        color_blend_attachment_state blending[] = {
            {
                .enabled = true,
                .color =
                    {
                        .src = blend_factor::SRC_ALPHA,
                        .dst = blend_factor::ONE_MINUS_SRC_ALPHA,
                        .op = blend_operation::ADD,
                    },
                .alpha =
                    {
                        .src = blend_factor::ONE,
                        .dst = blend_factor::ONE_MINUS_SRC_ALPHA,
                        .op = blend_operation::ADD,
                    },
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
                .name = "PBR OIT Blend Vertex Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "PBR OIT Blend Fragment Shader Module",
            },
            .vertex_layout{
                .topology = primitive_topology::TRIANGLE_FAN,
                .elements = {},
            },
            .depth_testing{
                .enable_test = false,
                .enable_write = false,
                .depth_test_op = compare_operation::NEVER,
            },
            .blending = {blending},
            .name = "PBR OIT Blend Graphics Pipeline",
        });

        _pipeline = pipeline;

        return pipeline != graphics_pipeline_resource_handle{};
    }

    bool pbr_oit_blend_pass::blend([[maybe_unused]] render_device& dev, command_list& cmds) const
    {
        cmds.set_cull_mode(false, true).use_pipeline(_pipeline).draw(3, 1, 0, 0);

        return true;
    }

    void pbr_oit_blend_pass::release(render_device& device)
    {
        if (_pipeline)
        {
            device.release_graphics_pipeline(_pipeline);
        }
    }
} // namespace tempest::graphics::passes