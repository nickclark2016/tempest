#include <tempest/frame_graph.hpp>

#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/enum.hpp>
#include <tempest/files.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/int.hpp>
#include <tempest/mat4.hpp>
#include <tempest/math_utils.hpp>
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
        _release_frame_upload_pass(_pass_output_resource_handles.upload_pass);
        _release_depth_prepass(_pass_output_resource_handles.depth_prepass);
        _release_ssao_pass(_pass_output_resource_handles.ssao);
        _release_ssao_blur_pass(_pass_output_resource_handles.ssao_blur);
        _release_light_clustering_pass(_pass_output_resource_handles.light_clustering);
        _release_light_culling_pass(_pass_output_resource_handles.light_culling);
        _release_shadow_map_pass(_pass_output_resource_handles.shadow_map);
        _release_pbr_opaque_pass(_pass_output_resource_handles.pbr_opaque);
        _release_mboit_gather_pass(_pass_output_resource_handles.mboit_gather);
        _release_mboit_resolve_pass(_pass_output_resource_handles.mboit_resolve);
        _release_mboit_blend_pass(_pass_output_resource_handles.mboit_blend);
        _release_tonemapping_pass(_pass_output_resource_handles.tonemapping);
        _release_global_resources();
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
        _create_global_resources();
        _pass_output_resource_handles.upload_pass = _add_frame_upload_pass(*_builder);
        _pass_output_resource_handles.depth_prepass = _add_depth_prepass(*_builder);
        _pass_output_resource_handles.ssao = _add_ssao_pass(*_builder);
        _pass_output_resource_handles.ssao_blur = _add_ssao_blur_pass(*_builder);
        _pass_output_resource_handles.light_clustering = _add_light_clustering_pass(*_builder);
        _pass_output_resource_handles.light_culling = _add_light_culling_pass(*_builder);
        _pass_output_resource_handles.shadow_map = _add_shadow_map_pass(*_builder);
        _pass_output_resource_handles.pbr_opaque = _add_pbr_opaque_pass(*_builder);
        _pass_output_resource_handles.mboit_gather = _add_mboit_gather_pass(*_builder);
        _pass_output_resource_handles.mboit_resolve = _add_mboit_resolve_pass(*_builder);
        _pass_output_resource_handles.mboit_blend = _add_mboit_blend_pass(*_builder);
        _pass_output_resource_handles.tonemapping = _add_tonemapping_pass(*_builder);
    }

    void pbr_frame_graph::_create_global_resources()
    {
        auto vertex_pull_buffer = _device->create_buffer({
            .size = _cfg.vertex_data_buffer_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Vertex Pull Buffer",
        });

        _global_resources.vertex_pull_buffer = vertex_pull_buffer;
        _global_resources.graph_vertex_pull_buffer = _builder->import_buffer("Vertex Pull Buffer", vertex_pull_buffer);

        auto mesh_buffer = _device->create_buffer({
            .size = _cfg.max_mesh_count * sizeof(mesh_layout),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Mesh Buffer",
        });

        _global_resources.mesh_buffer = mesh_buffer;
        _global_resources.graph_mesh_buffer = _builder->import_buffer("Mesh Buffer", mesh_buffer);

        auto material_buffer = _device->create_buffer({
            .size = _cfg.max_material_count * sizeof(material_data),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Material Buffer",
        });

        _global_resources.material_buffer = material_buffer;
        _global_resources.graph_material_buffer = _builder->import_buffer("Material Buffer", material_buffer);

        // Objects and instances are dynamic per-frame, so we create them as per-frame buffers
        auto object_buffer = _builder->create_per_frame_buffer({
            .size = _cfg.max_object_count * sizeof(object_data),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Object Buffer",
        });

        _global_resources.graph_object_buffer = object_buffer;

        auto instance_buffer = _builder->create_per_frame_buffer({
            .size = _cfg.max_object_count * sizeof(uint32_t),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Instance Buffer",
        });

        _global_resources.graph_instance_buffer = instance_buffer;

        auto light_buffer = _builder->create_per_frame_buffer({
            .size = _cfg.max_lights * sizeof(light),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Light Buffer",
        });

        _global_resources.graph_light_buffer = light_buffer;

        auto staging_buffer = _builder->create_per_frame_buffer({
            .size = _cfg.staging_buffer_size_per_frame,
            .location = rhi::memory_location::automatic,
            .usage = make_enum_mask(rhi::buffer_usage::transfer_src),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "Per-Frame Staging Buffer",
        });

        _global_resources.graph_per_frame_staging_buffer = staging_buffer;
    }

    void pbr_frame_graph::_release_global_resources()
    {
        _device->destroy_buffer(_global_resources.vertex_pull_buffer);
        _device->destroy_buffer(_global_resources.mesh_buffer);
        _device->destroy_buffer(_global_resources.material_buffer);
    }

    pbr_frame_graph::frame_upload_pass_outputs pbr_frame_graph::_add_frame_upload_pass(graph_builder& builder)
    {
        auto scene_constants_buffer = builder.create_per_frame_buffer({
            .size = sizeof(scene_constants),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::constant, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Scene Constants Buffer",
        });

        builder.create_transfer_pass(
            "Frame Upload Pass",
            [&](transfer_task_builder& task) {
                task.write(scene_constants_buffer, make_enum_mask(rhi::pipeline_stage::copy),
                           make_enum_mask(rhi::memory_access::transfer_write));
                task.read(_global_resources.graph_vertex_pull_buffer, make_enum_mask(rhi::pipeline_stage::copy),
                          make_enum_mask(rhi::memory_access::transfer_read));
            },
            &_upload_pass_task, this);

        return {
            .scene_constants = scene_constants_buffer,
        };
    }

    void pbr_frame_graph::_release_frame_upload_pass(frame_upload_pass_outputs& outputs)
    {
        outputs = {};
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

        auto scene_descriptors = _device->create_descriptor_set_layout(
            scene_descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::descriptor_buffer));

        auto descriptor_buffer = builder.create_per_frame_buffer({
            .size = _device->get_descriptor_set_layout_size(scene_descriptors),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::descriptor, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Scene Descriptor Set Buffer",
        });

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
                task.write(
                    depth, rhi::image_layout::depth,
                    make_enum_mask(rhi::pipeline_stage::early_fragment_tests, rhi::pipeline_stage::late_fragment_tests),
                    make_enum_mask(rhi::memory_access::depth_stencil_attachment_write));
                task.write(encoded_normals, rhi::image_layout::color_attachment,
                           make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                           make_enum_mask(rhi::memory_access::color_attachment_write));
                task.read(_pass_output_resource_handles.upload_pass.scene_constants,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(descriptor_buffer,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::descriptor_buffer_read));
            },
            &_depth_prepass_task, this, descriptor_buffer);

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

    pbr_frame_graph::ssao_pass_outputs pbr_frame_graph::_add_ssao_pass(graph_builder& builder)
    {
        auto ssao_output = builder.create_render_target({
            .format = rhi::image_format::r32_float,
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
            .name = "SSAO Output Buffer",
        });

        auto ssao_noise = _device->create_image({
            .format = rhi::image_format::rg16_snorm,
            .type = rhi::image_type::image_2d,
            .width = 4,
            .height = 4,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::sampled, rhi::image_usage::transfer_dst),
            .name = "SSAO Noise Texture",
        });

        // Descriptor Set Layout
        // 0 - Scene Constants
        // 1 - Depth Texture
        // 2 - Encoded Normal Texture
        // 3 - SSAO Noise Texture
        // 4 - Linear Sampler
        // 5 - Point Sampler

        auto scene_descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();
        scene_descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        scene_descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        scene_descriptor_set_bindings.push_back({
            .binding_index = 2,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        scene_descriptor_set_bindings.push_back({
            .binding_index = 3,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        scene_descriptor_set_bindings.push_back({
            .binding_index = 4,
            .type = rhi::descriptor_type::sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        scene_descriptor_set_bindings.push_back({
            .binding_index = 5,
            .type = rhi::descriptor_type::sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        auto scene_descriptors = _device->create_descriptor_set_layout(
            scene_descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::descriptor_buffer));

        auto descriptor_set_layouts = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>>{};
        descriptor_set_layouts.push_back(scene_descriptors);

        auto pipeline_layout = _device->create_pipeline_layout({
            .descriptor_set_layouts = descriptor_set_layouts,
            .push_constants = {},
        });

        auto vert_source = core::read_bytes("assets/shaders/ssao.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/ssao.frag.spv");

        auto color_formats = vector<rhi::image_format>();
        color_formats.push_back(rhi::image_format::r32_float);

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
            .color_attachment_formats = color_formats,
            .depth_attachment_format = none(),
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
                    .depth = none(),
                    .stencil = none(),
                },
            .color_blend =
                {
                    .attachments = tempest::move(blending),
                    .blend_constants = {},
                },
            .layout = pipeline_layout,
            .name = "SSAO Pass Pipeline",
        };

        auto pipeline = _device->create_graphics_pipeline(pipeline_desc);

        auto descriptor_buffer = builder.create_per_frame_buffer({
            .size = _device->get_descriptor_set_layout_size(scene_descriptors),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::descriptor, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "SSAO Descriptor Set Buffer",
        });

        builder.create_graphics_pass(
            "SSAO Pass",
            [&](graphics_task_builder& task) {
                task.read(_pass_output_resource_handles.depth_prepass.depth, rhi::image_layout::shader_read_only,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.depth_prepass.encoded_normals,
                          rhi::image_layout::shader_read_only, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.upload_pass.scene_constants,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(descriptor_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::descriptor_buffer_read));
                task.write(ssao_output, rhi::image_layout::color_attachment,
                           make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                           make_enum_mask(rhi::memory_access::color_attachment_write));
            },
            &_ssao_pass_task, this, descriptor_buffer);

        return {
            .ssao_output = ssao_output,
            .pipeline = pipeline,
            .ssao_noise_image = ssao_noise,
        };
    }

    void pbr_frame_graph::_release_ssao_pass(ssao_pass_outputs& outputs)
    {
        _device->destroy_graphics_pipeline(outputs.pipeline);
        _device->destroy_image(outputs.ssao_noise_image);

        outputs = {};
    }

    pbr_frame_graph::ssao_blur_pass_outputs pbr_frame_graph::_add_ssao_blur_pass(graph_builder& builder)
    {
        auto ssao_blurred_output = builder.create_per_frame_image({
            .format = rhi::image_format::r32_float,
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
            .name = "SSAO Blurred Output Buffer",
        });

        auto scene_descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();
        scene_descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        scene_descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        auto scene_descriptors = _device->create_descriptor_set_layout(
            scene_descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::push));

        auto descriptor_set_layouts = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>>{};
        descriptor_set_layouts.push_back(scene_descriptors);

        auto pipeline_layout = _device->create_pipeline_layout({
            .descriptor_set_layouts = descriptor_set_layouts,
            .push_constants = {},
        });

        auto vert_source = core::read_bytes("assets/shaders/ssao_blur.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/ssao_blur.frag.spv");

        auto color_formats = vector<rhi::image_format>();
        color_formats.push_back(rhi::image_format::r32_float);

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
            .color_attachment_formats = color_formats,
            .depth_attachment_format = none(),
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
                    .depth = none(),
                    .stencil = none(),
                },
            .color_blend =
                {
                    .attachments = tempest::move(blending),
                    .blend_constants = {},
                },
            .layout = pipeline_layout,
            .name = "SSAO Blur Pass Pipeline",
        };

        auto pipeline = _device->create_graphics_pipeline(pipeline_desc);

        builder.create_graphics_pass(
            "SSAO Blur Pass",
            [&](graphics_task_builder& task) {
                task.read(_pass_output_resource_handles.ssao.ssao_output, rhi::image_layout::shader_read_only,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.write(ssao_blurred_output, rhi::image_layout::color_attachment,
                           make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                           make_enum_mask(rhi::memory_access::color_attachment_write));
            },
            &_ssao_blur_pass_task, this);

        return {
            .ssao_blurred_output = ssao_blurred_output,
            .pipeline = pipeline,
        };
    }

    void pbr_frame_graph::_release_ssao_blur_pass(ssao_blur_pass_outputs& outputs)
    {
        _device->destroy_graphics_pipeline(outputs.pipeline);

        outputs = {};
    }

    pbr_frame_graph::light_clustering_pass_outputs pbr_frame_graph::_add_light_clustering_pass(graph_builder& builder)
    {
        const auto num_light_clusters = _cfg.light_clustering.cluster_count_x * _cfg.light_clustering.cluster_count_y *
                                        _cfg.light_clustering.cluster_count_z;
        const auto light_cluster_byte_size =
            math::round_to_next_multiple(sizeof(lighting_cluster_bounds) * num_light_clusters, 256);

        auto light_cluster_buffer = builder.create_buffer({
            .size = light_cluster_byte_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Light Cluster Buffer",
        });

        auto layout_bindings = vector<rhi::descriptor_binding_layout>{};
        layout_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::compute),
        });

        auto descriptor_set_layout = _device->create_descriptor_set_layout(
            layout_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::push));

        auto descriptor_set_layouts = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>>{};
        descriptor_set_layouts.push_back(descriptor_set_layout);

        auto push_constants = vector<rhi::push_constant_range>{};
        push_constants.push_back({
            .offset = 0,
            .range = sizeof(cluster_grid_create_info),
            .stages = make_enum_mask(rhi::shader_stage::compute),
        });

        auto layout = _device->create_pipeline_layout({
            .descriptor_set_layouts = descriptor_set_layouts,
            .push_constants = push_constants,
        });

        auto comp_source = core::read_bytes("assets/shaders/build_cluster_grid.comp.spv");

        auto pipeline_desc = rhi::compute_pipeline_desc{
            .compute_shader = tempest::move(comp_source),
            .layout = layout,
            .name = "Light Clustering Pipeline",
        };

        auto pipeline = _device->create_compute_pipeline(pipeline_desc);

        builder.create_compute_pass(
            "Light Clustering Pass",
            [&](compute_task_builder& task) {
                task.write(light_cluster_buffer, make_enum_mask(rhi::pipeline_stage::compute_shader),
                           make_enum_mask(rhi::memory_access::shader_write));
                task.read(_pass_output_resource_handles.upload_pass.scene_constants,
                          make_enum_mask(rhi::pipeline_stage::compute_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
            },
            &_light_clustering_pass_task, this);

        return {
            .light_cluster_bounds = light_cluster_buffer,
            .pipeline = pipeline,
        };
    }

    void pbr_frame_graph::_release_light_clustering_pass(light_clustering_pass_outputs& outputs)
    {
        _device->destroy_compute_pipeline(outputs.pipeline);

        outputs = {};
    }

    pbr_frame_graph::light_culling_pass_outputs pbr_frame_graph::_add_light_culling_pass(graph_builder& builder)
    {
        return {};
    }

    void pbr_frame_graph::_release_light_culling_pass(light_culling_pass_outputs& outputs)
    {
        outputs = {};
    }

    pbr_frame_graph::shadow_map_pass_outputs pbr_frame_graph::_add_shadow_map_pass(graph_builder& builder)
    {
        return {};
    }

    void pbr_frame_graph::_release_shadow_map_pass(shadow_map_pass_outputs& outputs)
    {
        outputs = {};
    }

    pbr_frame_graph::pbr_opaque_pass_outputs pbr_frame_graph::_add_pbr_opaque_pass(graph_builder& builder)
    {
        return {};
    }

    void pbr_frame_graph::_release_pbr_opaque_pass(pbr_opaque_pass_outputs& outputs)
    {
        outputs = {};
    }

    pbr_frame_graph::mboit_gather_pass_outputs pbr_frame_graph::_add_mboit_gather_pass(graph_builder& builder)
    {
        return {};
    }

    void pbr_frame_graph::_release_mboit_gather_pass(mboit_gather_pass_outputs& outputs)
    {
        outputs = {};
    }

    pbr_frame_graph::mboit_resolve_pass_outputs pbr_frame_graph::_add_mboit_resolve_pass(graph_builder& builder)
    {
        return {};
    }

    void pbr_frame_graph::_release_mboit_resolve_pass(mboit_resolve_pass_outputs& outputs)
    {
        outputs = {};
    }

    pbr_frame_graph::mboit_blend_pass_outputs pbr_frame_graph::_add_mboit_blend_pass(graph_builder& builder)
    {
        return {};
    }

    void pbr_frame_graph::_release_mboit_blend_pass(mboit_blend_pass_outputs& outputs)
    {
        outputs = {};
    }

    pbr_frame_graph::tonemapping_pass_outputs pbr_frame_graph::_add_tonemapping_pass(graph_builder& builder)
    {
        return {};
    }

    void pbr_frame_graph::_release_tonemapping_pass(tonemapping_pass_outputs& outputs)
    {
        outputs = {};
    }

    void pbr_frame_graph::_upload_pass_task(transfer_task_execution_context& ctx, pbr_frame_graph* self)
    {
        // No actual rendering commands needed, just resource uploads
    }

    void pbr_frame_graph::_depth_prepass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                              graph_resource_handle<rhi::rhi_handle_type::buffer> descriptors)
    {
        auto render_pass_begin = rhi::work_queue::render_pass_info{};
        render_pass_begin.name = "Depth Prepass";
        render_pass_begin.width = self->_cfg.render_target_width;
        render_pass_begin.height = self->_cfg.render_target_height;
        render_pass_begin.layers = 1;
        render_pass_begin.depth_attachment = rhi::work_queue::depth_attachment_info{
            .image = self->_executor->get_image(self->_pass_output_resource_handles.depth_prepass.depth),
            .layout = rhi::image_layout::depth,
            .clear_depth = 0.0f,
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::store,
        };
        render_pass_begin.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image = self->_executor->get_image(self->_pass_output_resource_handles.depth_prepass.encoded_normals),
            .layout = rhi::image_layout::color_attachment,
            .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::store,
        });

        ctx.begin_render_pass(render_pass_begin);
        ctx.end_render_pass();
    }

    void pbr_frame_graph::_ssao_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                          graph_resource_handle<rhi::rhi_handle_type::buffer> descriptors)
    {
        auto render_pass_begin = rhi::work_queue::render_pass_info{};
        render_pass_begin.name = "SSAO Pass";
        render_pass_begin.width = self->_cfg.render_target_width;
        render_pass_begin.height = self->_cfg.render_target_height;
        render_pass_begin.layers = 1;
        render_pass_begin.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image = self->_executor->get_image(self->_pass_output_resource_handles.ssao.ssao_output),
            .layout = rhi::image_layout::color_attachment,
            .clear_color =
                {
                    0.0f,
                    0.0f,
                    0.0f,
                    0.0f,
                },
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::store,
        });

        ctx.begin_render_pass(render_pass_begin);
        ctx.end_render_pass();
    }

    void pbr_frame_graph::_ssao_blur_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self)
    {
        auto render_pass_begin = rhi::work_queue::render_pass_info{};
        render_pass_begin.name = "SSAO Blur Pass";
        render_pass_begin.width = self->_cfg.render_target_width;
        render_pass_begin.height = self->_cfg.render_target_height;
        render_pass_begin.layers = 1;
        render_pass_begin.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image = self->_executor->get_image(self->_pass_output_resource_handles.ssao_blur.ssao_blurred_output),
            .layout = rhi::image_layout::color_attachment,
            .clear_color =
                {
                    0.0f,
                    0.0f,
                    0.0f,
                    0.0f,
                },
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::store,
        });

        ctx.begin_render_pass(render_pass_begin);
        ctx.end_render_pass();
    }

    void pbr_frame_graph::_light_clustering_pass_task(compute_task_execution_context& ctx, pbr_frame_graph* self)
    {
    }

    void pbr_frame_graph::_light_culling_pass_task(compute_task_execution_context& ctx, pbr_frame_graph* self,
                                                   graph_resource_handle<rhi::rhi_handle_type::buffer> descriptors)
    {
    }

    void pbr_frame_graph::_shadow_map_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                                graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors)
    {
    }

    void pbr_frame_graph::_pbr_opaque_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                                graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors,
                                                graph_resource_handle<rhi::rhi_handle_type::buffer> shadow_descriptors)
    {
    }

    void pbr_frame_graph::_mboit_gather_pass_task(
        graphics_task_execution_context& ctx, pbr_frame_graph* self,
        graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors,
        graph_resource_handle<rhi::rhi_handle_type::buffer> shadow_descriptors)
    {
    }

    void pbr_frame_graph::_mboit_resolve_pass_task(
        graphics_task_execution_context& ctx, pbr_frame_graph* self,
        graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors,
        graph_resource_handle<rhi::rhi_handle_type::buffer> shadow_descriptors)
    {
    }

    void pbr_frame_graph::_mboit_blend_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                                 graph_resource_handle<rhi::rhi_handle_type::buffer> oit_descriptors)
    {
    }

    void pbr_frame_graph::_tonemapping_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self)
    {
    }
} // namespace tempest::graphics