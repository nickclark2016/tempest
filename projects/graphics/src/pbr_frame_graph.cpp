#include <tempest/frame_graph.hpp>

#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/enum.hpp>
#include <tempest/files.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/int.hpp>
#include <tempest/mat4.hpp>
#include <tempest/pbr_frame_graph.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>

namespace tempest::graphics
{
    pbr_frame_graph::pbr_frame_graph(rhi::device& device, pbr_frame_graph_config cfg, pbr_frame_graph_inputs inputs)
        : _device(&device), _cfg(cfg), _inputs(inputs), _builder{graph_builder{}}, _executor{none()}
    {
        _initialize();
    }

    pbr_frame_graph::~pbr_frame_graph()
    {
        _release_depth_prepass(_resource_handles.depth_prepass);
    }

    optional<graph_builder&> pbr_frame_graph::get_builder() noexcept
    {
        if (_builder.has_value())
        {
            return optional<graph_builder&>(_builder.value());
        }
        return none();
    }

    void pbr_frame_graph::compile(queue_configuration cfg)
    {
        auto exec_plan = move(_builder).value().compile(cfg);
        _builder = none();
        _executor = graph_executor(*_device);
        _executor->set_execution_plan(tempest::move(exec_plan));
    }

    void pbr_frame_graph::execute()
    {
        TEMPEST_ASSERT(_executor.has_value());
        _executor->execute();
    }

    void pbr_frame_graph::_initialize()
    {
        _resource_handles.depth_prepass = _add_depth_prepass(*_builder);
    }

    pbr_frame_graph::depth_prepass_outputs pbr_frame_graph::_add_depth_prepass(graph_builder& builder)
    {
        auto depth = builder.create_temporal_image({
            .format = _cfg.depth_format,
            .type = rhi::image_type::image_2d,
            .width = _cfg.render_target_width,
            .height = _cfg.render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::depth_attachment, rhi::image_usage::sampled),
            .name = "Depth Buffer",
        });

        auto encoded_normals = builder.create_temporal_image({
            .format = rhi::image_format::rg16_float,
            .type = rhi::image_type::image_2d,
            .width = _cfg.render_target_width,
            .height = _cfg.render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled),
            .name = "Encoded Normal Buffer",
        });

        auto scene_descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();

        // Scene constants
        scene_descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
        });

        // Vertex pull buffer
        scene_descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });

        // Mesh layout buffer
        scene_descriptor_set_bindings.push_back({
            .binding_index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });

        // Object buffer
        scene_descriptor_set_bindings.push_back({
            .binding_index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });

        // Instance buffer
        scene_descriptor_set_bindings.push_back({
            .binding_index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });

        // Material buffer
        scene_descriptor_set_bindings.push_back({
            .binding_index = 5,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        // Linear sampler for textures
        scene_descriptor_set_bindings.push_back({
            .binding_index = 15,
            .type = rhi::descriptor_type::sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        // Bindless texture array
        scene_descriptor_set_bindings.push_back({
            .binding_index = 16,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1024,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
            .flags = make_enum_mask(rhi::descriptor_binding_flags::partially_bound,
                                    rhi::descriptor_binding_flags::variable_length),
        });

        auto scene_descriptors = _device->create_descriptor_set_layout(scene_descriptor_set_bindings);

        auto descriptor_set_layouts = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>>{};
        descriptor_set_layouts.push_back(scene_descriptors);

        auto pipeline_layout =
            _device->create_pipeline_layout({.descriptor_set_layouts = descriptor_set_layouts, .push_constants = {}});

        auto vert_source = core::read_bytes("assets/shaders/zprepass.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/zprepass.frag.spv");

        auto color_formats = vector<rhi::image_format>();
        color_formats.push_back(rhi::image_format::rg16_float);

        auto blending = vector<rhi::color_blend_attachment>();
        blending.push_back({
            .blend_enable = false,
            .src_color_blend_factor = rhi::blend_factor::one,
            .dst_color_blend_factor = rhi::blend_factor::zero,
            .color_blend_op = rhi::blend_op::add,
            .src_alpha_blend_factor = rhi::blend_factor::one,
            .dst_alpha_blend_factor = rhi::blend_factor::zero,
            .alpha_blend_op = rhi::blend_op::add,
        });

        auto pipeline_desc = rhi::graphics_pipeline_desc{
            .color_attachment_formats = tempest::move(color_formats),
            .depth_attachment_format = _cfg.depth_format,
            .stencil_attachment_format = none(),
            .vertex_shader = tempest::move(vert_source),
            .tessellation_control_shader = {},
            .tessellation_evaluation_shader = {},
            .geometry_shader = {},
            .fragment_shader = tempest::move(frag_source),
            .input_assembly =
                {
                    .topology = rhi::primitive_topology::triangle_list,
                },
            .vertex_input = none(),
            .tessellation = none(),
            .multisample =
                {
                    .sample_count = rhi::image_sample_count::sample_count_1,
                    .sample_shading = none(),
                    .alpha_to_coverage = false,
                    .alpha_to_one = false,
                },
            .rasterization =
                {
                    .depth_clamp_enable = false,
                    .rasterizer_discard_enable = false,
                    .polygon_mode = rhi::polygon_mode::fill,
                    .cull_mode = make_enum_mask(rhi::cull_mode::back),
                    .vertex_winding = rhi::vertex_winding::counter_clockwise,
                    .depth_bias = none(),
                    .line_width = 1.0f,
                },
            .depth_stencil =
                {
                    .depth =
                        rhi::depth_test{
                            .write_enable = true,
                            .compare_op = rhi::compare_op::greater_equal,
                            .depth_bounds_test_enable = false,
                            .min_depth_bounds = 0.0f,
                            .max_depth_bounds = 1.0f,
                        },
                    .stencil = none(),
                },
            .color_blend =
                {
                    .attachments = tempest::move(blending),
                    .blend_constants = {},
                },
            .layout = pipeline_layout,
            .name = "Depth Prepass Pipeline",
        };

        auto pipeline = _device->create_graphics_pipeline(pipeline_desc);

        builder.create_graphics_pass(
            "Depth Prepass",
            [&](graphics_task_builder& task) {
                task.write(depth, rhi::image_layout::depth);
                task.write(encoded_normals, rhi::image_layout::color_attachment);
            },
            &_depth_prepass_task, this);

        return {
            .depth = depth,
            .encoded_normals = encoded_normals,
            .pipeline = pipeline,
        };
    }

    void pbr_frame_graph::_release_depth_prepass(depth_prepass_outputs& outputs)
    {
        _device->destroy_graphics_pipeline(outputs.pipeline);

        outputs = {};
    }

    void pbr_frame_graph::_depth_prepass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self)
    {
    }
} // namespace tempest::graphics