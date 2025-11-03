#include <tempest/frame_graph.hpp>

#include <cstring>
#include <random>

#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/enum.hpp>
#include <tempest/exception.hpp>
#include <tempest/files.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/int.hpp>
#include <tempest/logger.hpp>
#include <tempest/mat4.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/optional.hpp>
#include <tempest/pbr_frame_graph.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/to_underlying.hpp>
#include <tempest/traits.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>

namespace tempest::graphics
{
    namespace
    {
        auto log = logger::logger_factory::create({.prefix = "pbr_frame_graph"});
    }

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

        for (const auto& sub : exec_plan.submissions)
        {
            log->debug("Submission: {}", to_underlying(sub.type));
            for (const auto& pass : sub.passes)
            {
                log->debug("Pass '{}'", pass.name.c_str());
            }
        }

        _builder = none();
        _executor = graph_executor(*_device);
        _executor->set_execution_plan(tempest::move(exec_plan));
    }

    void pbr_frame_graph::execute()
    {
        _global_resources.utilization.staging_buffer_bytes_written = 0;

        TEMPEST_ASSERT(_executor.has_value());
        _executor->execute();
    }

    void pbr_frame_graph::upload_objects_sync(span<const ecs::archetype_entity> entities,
                                              const core::mesh_registry& meshes, const core::texture_registry& textures,
                                              const core::material_registry& materials)
    {
        // Wait for the device to idle for synchronous upload
        _device->wait_idle();

        vector<guid> mesh_guids;
        vector<guid> texture_guids;
        vector<guid> material_guids;

        for (const auto entity : entities)
        {
            const auto hierarchy_view = ecs::archetype_entity_hierarchy_view(*_inputs.entity_registry, entity);
            for (const auto e : hierarchy_view)
            {
                const auto mesh_component = _inputs.entity_registry->try_get<core::mesh_component>(e);
                const auto material_component = _inputs.entity_registry->try_get<core::material_component>(e);

                // Both are needed to render the object
                if (mesh_component == nullptr || material_component == nullptr)
                {
                    continue;
                }

                // Make sure the GUIDs are both valid
                const auto mesh_opt = meshes.find(mesh_component->mesh_id);
                const auto material_opt = materials.find(material_component->material_id);

                if (!mesh_opt.has_value() || !material_opt.has_value())
                {
                    continue;
                }

                // Add the mesh and material GUIDs to the vectors
                mesh_guids.push_back(mesh_component->mesh_id);
                material_guids.push_back(material_component->material_id);

                const auto& material = *material_opt;

                if (const auto base_color = material.get_texture(core::material::base_color_texture_name))
                {
                    texture_guids.push_back(*base_color);
                }

                if (const auto mr_texture = material.get_texture(core::material::metallic_roughness_texture_name))
                {
                    texture_guids.push_back(*mr_texture);
                }

                if (const auto normal_texture = material.get_texture(core::material::normal_texture_name))
                {
                    texture_guids.push_back(*normal_texture);
                }

                if (const auto occlusion_texture = material.get_texture(core::material::occlusion_texture_name))
                {
                    texture_guids.push_back(*occlusion_texture);
                }

                if (const auto emissive_texture = material.get_texture(core::material::emissive_texture_name))
                {
                    texture_guids.push_back(*emissive_texture);
                }

                if (const auto transmissive_texture = material.get_texture(core::material::transmissive_texture_name))
                {
                    texture_guids.push_back(*transmissive_texture);
                }

                if (const auto volume_thickness_texture =
                        material.get_texture(core::material::volume_thickness_texture_name))
                {
                    texture_guids.push_back(*volume_thickness_texture);
                }
            }
        }

        // Meshs and textures need to be uploaded before materials, since materials relies on textures being written to
        // the CPU buffers
        _load_meshes(mesh_guids, meshes);
        _load_textures(texture_guids, textures, true);
        _load_materials(material_guids, materials);

        // Build the render components
        for (const auto entity : entities)
        {
            const auto hierarchy_view = ecs::archetype_entity_hierarchy_view(*_inputs.entity_registry, entity);
            for (const auto e : hierarchy_view)
            {
                const auto mesh_component = _inputs.entity_registry->try_get<core::mesh_component>(e);
                const auto material_component = _inputs.entity_registry->try_get<core::material_component>(e);
                // Both are needed to render the object
                if (mesh_component == nullptr || material_component == nullptr)
                {
                    continue;
                }

                // Make sure the GUIDs are both valid
                const auto mesh_opt = meshes.find(mesh_component->mesh_id);
                const auto material_opt = materials.find(material_component->material_id);
                if (!mesh_opt.has_value() || !material_opt.has_value())
                {
                    continue;
                }

                // Build the renderable component
                const auto mesh_index = _meshes.mesh_to_index[mesh_component->mesh_id];
                const auto material_index = _materials.material_to_index[material_component->material_id];
                const auto is_double_side = material_opt->get_bool(core::material::double_sided_name).value_or(false);

                // Check if there is an existing renderable component
                const auto rc = _inputs.entity_registry->try_get<renderable_component>(e);
                const auto object_id = rc ? rc->object_id : _global_resources.utilization.loaded_object_count++;

                // Create the renderable component
                const auto renderable = renderable_component{
                    .mesh_id = static_cast<uint32_t>(mesh_index),
                    .material_id = static_cast<uint32_t>(material_index),
                    .object_id = object_id,
                    .double_sided = is_double_side,
                };

                _inputs.entity_registry->assign_or_replace(e, renderable);

                // If the object has no transform, assign the default transform
                if (!_inputs.entity_registry->has<ecs::transform_component>(e))
                {
                    _inputs.entity_registry->assign_or_replace(e, ecs::transform_component{});
                }
            }
        }
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
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::index,
                                    rhi::buffer_usage::transfer_dst),
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

        // Create samplers
        auto linear_sampler_desc = rhi::sampler_desc{
            .mag = rhi::filter::linear,
            .min = rhi::filter::linear,
            .mipmap = rhi::mipmap_mode::linear,
            .address_u = rhi::address_mode::repeat,
            .address_v = rhi::address_mode::repeat,
            .address_w = rhi::address_mode::repeat,
            .mip_lod_bias = 0.0f,
            .min_lod = 0.0f,
            .max_lod = numeric_limits<float>::max(),
            .max_anisotropy = 1.0f,
            .compare = rhi::compare_op::never,
            .name = "Linear Sampler",
        };

        _global_resources.linear_sampler = _device->create_sampler(linear_sampler_desc);

        auto linear_with_aniso_sampler_desc = linear_sampler_desc;
        linear_with_aniso_sampler_desc.max_anisotropy = _cfg.max_anisotropy;
        linear_with_aniso_sampler_desc.name = "Linear with Anisotropy Sampler";

        _global_resources.linear_with_aniso_sampler = _device->create_sampler(linear_with_aniso_sampler_desc);

        auto point_sampler_desc = rhi::sampler_desc{
            .mag = rhi::filter::nearest,
            .min = rhi::filter::nearest,
            .mipmap = rhi::mipmap_mode::nearest,
            .address_u = rhi::address_mode::repeat,
            .address_v = rhi::address_mode::repeat,
            .address_w = rhi::address_mode::repeat,
            .mip_lod_bias = 0.0f,
            .min_lod = 0.0f,
            .max_lod = numeric_limits<float>::max(),
            .max_anisotropy = 1.0f,
            .compare = rhi::compare_op::never,
            .name = "Point Sampler",
        };

        _global_resources.point_sampler = _device->create_sampler(point_sampler_desc);

        auto point_with_aniso_sampler_desc = point_sampler_desc;
        point_with_aniso_sampler_desc.max_anisotropy = _cfg.max_anisotropy;
        point_with_aniso_sampler_desc.name = "Point with Anisotropy Sampler";

        _global_resources.point_with_aniso_sampler = _device->create_sampler(point_with_aniso_sampler_desc);
    }

    void pbr_frame_graph::_release_global_resources()
    {
        _device->destroy_buffer(_global_resources.vertex_pull_buffer);
        _device->destroy_buffer(_global_resources.mesh_buffer);
        _device->destroy_buffer(_global_resources.material_buffer);

        _device->destroy_sampler(_global_resources.linear_sampler);
        _device->destroy_sampler(_global_resources.linear_with_aniso_sampler);
        _device->destroy_sampler(_global_resources.point_sampler);
        _device->destroy_sampler(_global_resources.point_with_aniso_sampler);
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

        auto indirect_draw_commands_buffer = builder.create_per_frame_buffer({
            .size = _cfg.max_object_count * sizeof(indexed_indirect_command),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::indirect, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "Indirect Draw Commands Buffer",
        });

        builder.create_transfer_pass(
            "Frame Upload Pass",
            [&](transfer_task_builder& task) {
                task.write(scene_constants_buffer, make_enum_mask(rhi::pipeline_stage::copy),
                           make_enum_mask(rhi::memory_access::transfer_write));
                task.read(_global_resources.graph_vertex_pull_buffer, make_enum_mask(rhi::pipeline_stage::copy),
                          make_enum_mask(rhi::memory_access::transfer_read));
                task.read_write(_global_resources.graph_per_frame_staging_buffer,
                                make_enum_mask(rhi::pipeline_stage::copy),
                                make_enum_mask(rhi::memory_access::transfer_read),
                                make_enum_mask(rhi::pipeline_stage::none), make_enum_mask(rhi::memory_access::none));
                task.write(indirect_draw_commands_buffer, make_enum_mask(rhi::pipeline_stage::host),
                           make_enum_mask(rhi::memory_access::host_write));

                // Writes to the object and instance buffers
                task.write(_global_resources.graph_object_buffer, make_enum_mask(rhi::pipeline_stage::copy),
                           make_enum_mask(rhi::memory_access::transfer_write));
                task.write(_global_resources.graph_instance_buffer, make_enum_mask(rhi::pipeline_stage::copy),
                           make_enum_mask(rhi::memory_access::transfer_write));

                // Writes to the light buffer
                // task.write(_global_resources.graph_light_buffer, make_enum_mask(rhi::pipeline_stage::copy),
                //           make_enum_mask(rhi::memory_access::transfer_write));
            },
            &_upload_pass_task, this);

        return {
            .scene_constants = scene_constants_buffer,
            .draw_commands = indirect_draw_commands_buffer,
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
            .count = _cfg.max_bindless_textures,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
            .flags = make_enum_mask(rhi::descriptor_binding_flags::partially_bound,
                                    rhi::descriptor_binding_flags::variable_length),
        });

        auto scene_descriptors = _device->create_descriptor_set_layout(
            scene_descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::descriptor_buffer));

        auto descriptor_buffer = builder.create_per_frame_buffer({
            .size = _device->get_descriptor_set_layout_size(scene_descriptors),
            .location = rhi::memory_location::automatic,
            .usage = make_enum_mask(rhi::buffer_usage::descriptor, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
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
                task.write(depth, rhi::image_layout::depth, make_enum_mask(rhi::pipeline_stage::all_fragment_tests),
                           make_enum_mask(rhi::memory_access::depth_stencil_attachment_write));
                task.write(encoded_normals, rhi::image_layout::color_attachment,
                           make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                           make_enum_mask(rhi::memory_access::color_attachment_write));
                task.read(_pass_output_resource_handles.upload_pass.scene_constants,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(descriptor_buffer,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
            },
            &_depth_prepass_task, this, descriptor_buffer);

        return {
            .depth = depth,
            .encoded_normals = encoded_normals,
            .pipeline = pipeline,
            .pipeline_layout = pipeline_layout,
            .scene_descriptor_layout = scene_descriptors,
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

        auto ssao_constant_buffer = builder.create_per_frame_buffer({
            .size = sizeof(ssao_constants),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::constant, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "SSAO Constants Buffer",
        });

        const auto noise_image_width = 16u;
        const auto noise_image_height = 16u;

        auto ssao_noise = _device->create_image({
            .format = rhi::image_format::rg16_snorm,
            .type = rhi::image_type::image_2d,
            .width = noise_image_width,
            .height = noise_image_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::sampled, rhi::image_usage::transfer_dst),
            .name = "SSAO Noise Texture",
        });

        // Populate the noise image and kernel

        auto rd = std::random_device{};
        auto generator = std::mt19937{rd()};
        auto distribution = std::uniform_real_distribution{0.0f, 1.0f};

        auto noise_data = vector<short>(2 * noise_image_width * noise_image_height);

        const auto num_noise_samples = noise_image_width * noise_image_height;
        for (auto idx = 0u; idx < num_noise_samples; ++idx)
        {
            const auto r = distribution(generator);
            const auto g = distribution(generator);

            // Encode the red and green channels as signed short values
            const auto encoded_r = static_cast<short>(math::lerp(-32768.0f, 32767.0f, r));
            const auto encoded_g = static_cast<short>(math::lerp(-32768.0f, 32767.0f, g));

            noise_data[2 * idx + 0] = encoded_r;
            noise_data[2 * idx + 1] = encoded_g;
        }

        auto staging_buffer = _device->create_buffer({
            .size = static_cast<size_t>(noise_data.size() * sizeof(short)),
            .location = rhi::memory_location::automatic,
            .usage = make_enum_mask(rhi::buffer_usage::transfer_src),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "SSAO Noise Staging Buffer",
        });

        auto staging_buffer_bytes = _device->map_buffer(staging_buffer);
        std::memcpy(staging_buffer_bytes, noise_data.data(), noise_data.size() * sizeof(short));
        _device->unmap_buffer(staging_buffer);

        auto& wq = _device->get_primary_work_queue();
        auto cmds = wq.get_next_command_list();
        wq.begin_command_list(cmds, true);

        // Transition noise image to dst layout
        const auto pre_transfer_barriers = array{
            rhi::work_queue::image_barrier{
                .image = ssao_noise,
                .old_layout = rhi::image_layout::undefined,
                .new_layout = rhi::image_layout::transfer_dst,
                .src_stages = make_enum_mask(rhi::pipeline_stage::top),
                .src_access = make_enum_mask(rhi::memory_access::none),
                .dst_stages = make_enum_mask(rhi::pipeline_stage::copy),
                .dst_access = make_enum_mask(rhi::memory_access::transfer_write),
                .src_queue = nullptr,
                .dst_queue = nullptr,
            },
        };
        wq.transition_image(cmds, pre_transfer_barriers);

        wq.copy(cmds, staging_buffer, ssao_noise, rhi::image_layout::transfer_dst, 0, 0);

        // Transition noise image to shader read layout
        const auto post_transfer_barriers = array{
            rhi::work_queue::image_barrier{
                .image = ssao_noise,
                .old_layout = rhi::image_layout::transfer_dst,
                .new_layout = rhi::image_layout::shader_read_only,
                .src_stages = make_enum_mask(rhi::pipeline_stage::copy),
                .src_access = make_enum_mask(rhi::memory_access::transfer_write),
                .dst_stages = make_enum_mask(rhi::pipeline_stage::fragment_shader),
                .dst_access = make_enum_mask(rhi::memory_access::shader_read),
                .src_queue = nullptr,
                .dst_queue = nullptr,
            },
        };

        wq.transition_image(cmds, post_transfer_barriers);

        wq.end_command_list(cmds);

        auto result_fence = _device->create_fence({
            .signaled = false,
        });

        auto submit_info = rhi::work_queue::submit_info{};
        submit_info.command_lists.push_back(cmds);

        const auto submits = array{submit_info};

        wq.submit(submits, result_fence);

        for (size_t i = 0; i < ssao_constants::ssao_kernel_size; ++i)
        {
            const auto sample = math::vec3<float>{
                distribution(generator) * 2.0f - 1.0f,
                distribution(generator) * 2.0f - 1.0f,
                distribution(generator),
            };

            const auto normalized_sample = math::normalize(sample);
            const auto scaled_sample = normalized_sample * distribution(generator);

            const auto scale = static_cast<float>(i) / static_cast<float>(ssao_constants::ssao_kernel_size);
            const auto adjusted_scale = math::lerp(0.1f, 1.0f, scale * scale);

            const auto lerp_adjusted_sample = scaled_sample * adjusted_scale;

            _ssao_data.noise_kernel.push_back(
                math::vec4<float>{lerp_adjusted_sample.x, lerp_adjusted_sample.y, lerp_adjusted_sample.z, 0.0f});
        }

        _ssao_data.bias = 0.025f;
        _ssao_data.radius = 0.5f;
        _ssao_data.noise_scale = math::vec2<float>{
            static_cast<float>(_cfg.render_target_width) / static_cast<float>(noise_image_width),
            static_cast<float>(_cfg.render_target_height) / static_cast<float>(noise_image_height),
        };

        // Descriptor Set Layout
        // 0 - Scene Constants
        // 1 - SSAO Constants
        // 2 - Depth Texture
        // 3 - Encoded Normal Texture
        // 4 - SSAO Noise Texture
        // 5 - Linear Sampler
        // 6 - Point Sampler

        auto scene_descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();
        scene_descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        scene_descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::constant_buffer,
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
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        scene_descriptor_set_bindings.push_back({
            .binding_index = 5,
            .type = rhi::descriptor_type::sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        scene_descriptor_set_bindings.push_back({
            .binding_index = 6,
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
            .location = rhi::memory_location::automatic,
            .usage = make_enum_mask(rhi::buffer_usage::descriptor, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "SSAO Descriptor Set Buffer",
        });

        builder.create_transfer_pass(
            "Upload SSAO Constants",
            [&](transfer_task_builder& task) {
                task.write(ssao_constant_buffer, make_enum_mask(rhi::pipeline_stage::copy),
                           make_enum_mask(rhi::memory_access::transfer_write));
            },
            &_ssao_upload_task, this);

        builder.create_graphics_pass(
            "SSAO Pass",
            [&](graphics_task_builder& task) {
                task.read(ssao_constant_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
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
                          make_enum_mask(rhi::memory_access::shader_read));
                task.write(ssao_output, rhi::image_layout::color_attachment,
                           make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                           make_enum_mask(rhi::memory_access::color_attachment_write));
            },
            &_ssao_pass_task, this, descriptor_buffer);

        const auto fences = array{result_fence};
        _device->wait(fences);

        _device->destroy_fence(result_fence);
        _device->destroy_buffer(staging_buffer);

        return {
            .ssao_output = ssao_output,
            .ssao_constants_buffer = ssao_constant_buffer,
            .pipeline = pipeline,
            .pipeline_layout = pipeline_layout,
            .ssao_noise_image = ssao_noise,
            .descriptor_layout = scene_descriptors,
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
            .pipeline_layout = pipeline_layout,
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
                          make_enum_mask(rhi::memory_access::shader_read, rhi::memory_access::constant_buffer_read));
            },
            &_light_clustering_pass_task, this);

        return {
            .light_cluster_bounds = light_cluster_buffer,
            .pipeline = pipeline,
            .pipeline_layout = layout,
            .descriptor_layout = descriptor_set_layout,
        };
    }

    void pbr_frame_graph::_release_light_clustering_pass(light_clustering_pass_outputs& outputs)
    {
        _device->destroy_compute_pipeline(outputs.pipeline);

        outputs = {};
    }

    pbr_frame_graph::light_culling_pass_outputs pbr_frame_graph::_add_light_culling_pass(graph_builder& builder)
    {
        const auto num_light_clusters = _cfg.light_clustering.cluster_count_x * _cfg.light_clustering.cluster_count_y *
                                        _cfg.light_clustering.cluster_count_z;
        const auto light_range_size = math::round_to_next_multiple(sizeof(light_grid_range) * num_light_clusters, 256);

        auto light_range_buffer = builder.create_buffer({
            .size = light_range_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Light Grid Range Buffer",
        });

        auto light_indices_buffer = builder.create_buffer({
            .size = math::round_to_next_multiple(
                sizeof(uint32_t) * _cfg.light_clustering.max_lights_per_cluster * num_light_clusters, 256),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Light Indices Buffer",
        });

        auto light_count_buffer = builder.create_buffer({
            .size = math::round_to_next_multiple(sizeof(uint32_t), 256),
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Light Count Buffer",
        });

        auto layout_bindings = vector<rhi::descriptor_binding_layout>{};
        layout_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::compute),
        });
        layout_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::compute),
        });
        layout_bindings.push_back({
            .binding_index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::compute),
        });
        layout_bindings.push_back({
            .binding_index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::compute),
        });
        layout_bindings.push_back({
            .binding_index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::compute),
        });
        layout_bindings.push_back({
            .binding_index = 5,
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
            .range = sizeof(light_culling_info),
            .stages = make_enum_mask(rhi::shader_stage::compute),
        });

        auto layout = _device->create_pipeline_layout({
            .descriptor_set_layouts = descriptor_set_layouts,
            .push_constants = push_constants,
        });

        auto comp_source = core::read_bytes("assets/shaders/cull_lights.comp.spv");

        auto pipeline_desc = rhi::compute_pipeline_desc{
            .compute_shader = tempest::move(comp_source),
            .layout = layout,
            .name = "Light Culling Pipeline",
        };

        auto pipeline = _device->create_compute_pipeline(pipeline_desc);

        auto light_grid = _pass_output_resource_handles.light_clustering.light_cluster_bounds;

        builder.create_transfer_pass(
            "Reset Light Count Buffer",
            [&](transfer_task_builder& task) {
                task.write(light_count_buffer, make_enum_mask(rhi::pipeline_stage::all_transfer),
                           make_enum_mask(rhi::memory_access::transfer_write));
            },
            [](transfer_task_execution_context& ctx, auto light_count) { ctx.fill_buffer(light_count, 0, 4, 0); },
            light_count_buffer);

        builder.create_compute_pass(
            "Light Culling Pass",
            [&](compute_task_builder& task) {
                task.write(light_range_buffer, make_enum_mask(rhi::pipeline_stage::compute_shader),
                           make_enum_mask(rhi::memory_access::shader_write));
                task.write(light_indices_buffer, make_enum_mask(rhi::pipeline_stage::compute_shader),
                           make_enum_mask(rhi::memory_access::shader_write));
                task.read_write(light_count_buffer, make_enum_mask(rhi::pipeline_stage::compute_shader),
                                make_enum_mask(rhi::memory_access::shader_read),
                                make_enum_mask(rhi::pipeline_stage::compute_shader),
                                make_enum_mask(rhi::memory_access::shader_write));
                task.read_write(light_grid, make_enum_mask(rhi::pipeline_stage::compute_shader),
                                make_enum_mask(rhi::memory_access::shader_read),
                                make_enum_mask(rhi::pipeline_stage::compute_shader),
                                make_enum_mask(rhi::memory_access::shader_write));
                task.read(_global_resources.graph_light_buffer, make_enum_mask(rhi::pipeline_stage::compute_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.upload_pass.scene_constants,
                          make_enum_mask(rhi::pipeline_stage::compute_shader),
                          make_enum_mask(rhi::memory_access::shader_read, rhi::memory_access::constant_buffer_read));
            },
            &_light_culling_pass_task, this);

        return {
            .light_grid = light_grid,
            .light_grid_ranges = light_range_buffer,
            .light_indices = light_indices_buffer,
            .light_index_count = light_count_buffer,
            .pipeline = pipeline,
            .pipeline_layout = layout,
            .descriptor_layout = descriptor_set_layout,
        };
    }

    void pbr_frame_graph::_release_light_culling_pass(light_culling_pass_outputs& outputs)
    {
        _device->destroy_compute_pipeline(outputs.pipeline);

        outputs = {};
    }

    pbr_frame_graph::shadow_map_pass_outputs pbr_frame_graph::_add_shadow_map_pass(graph_builder& builder)
    {
        auto shadow_mega_texture = builder.create_render_target({
            .format = rhi::image_format::d32_float,
            .type = rhi::image_type::image_2d,
            .width = _cfg.shadows.shadow_map_width,
            .height = _cfg.shadows.shadow_map_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::depth_attachment, rhi::image_usage::sampled),
            .name = "Shadow Map Mega Texture",
        });

        const auto shadow_buffer_size =
            math::round_to_next_multiple(sizeof(shadow_map_parameter) * _cfg.shadows.max_shadow_casting_lights *
                                             shadow_map_cascade_info::max_cascade_count,
                                         256);

        auto shadow_data_buffer = builder.create_per_frame_buffer({
            .size = shadow_buffer_size,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::buffer_usage::structured, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::none,
            .access_pattern = rhi::host_access_pattern::none,
            .name = "Shadow Map Data Buffer",
        });

        // Descriptors
        // 1 - Vertex pull buffer
        // 2 - Meshes
        // 3 - Objects
        // 4 - Instances
        // 5 - Materials
        // 15 - Linear Sampler
        // 16+ - Bindless Textures

        auto descriptor_bindings = vector<rhi::descriptor_binding_layout>();
        descriptor_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        descriptor_bindings.push_back({
            .binding_index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        descriptor_bindings.push_back({
            .binding_index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        descriptor_bindings.push_back({
            .binding_index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        descriptor_bindings.push_back({
            .binding_index = 5,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        descriptor_bindings.push_back({
            .binding_index = 15,
            .type = rhi::descriptor_type::sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        descriptor_bindings.push_back({
            .binding_index = 16,
            .type = rhi::descriptor_type::sampled_image,
            .count = _cfg.max_bindless_textures,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        auto descriptor_set_layout = _device->create_descriptor_set_layout(
            descriptor_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::descriptor_buffer));

        auto descriptor_set_layouts = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>>{};
        descriptor_set_layouts.push_back(descriptor_set_layout);

        auto push_constants = vector<rhi::push_constant_range>{};
        push_constants.push_back({
            .offset = 0,
            .range = sizeof(directional_shadow_pass_constants),
            .stages = make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
        });

        auto pipeline_layout = _device->create_pipeline_layout({
            .descriptor_set_layouts = descriptor_set_layouts,
            .push_constants = push_constants,
        });

        auto vert_source = core::read_bytes("assets/shaders/directional_shadow_map.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/directional_shadow_map.frag.spv");

        auto pipeline_desc = rhi::graphics_pipeline_desc{
            .color_attachment_formats = {},
            .depth_attachment_format = rhi::image_format::d32_float,
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
                    .attachments = {},
                    .blend_constants = {},
                },
            .layout = pipeline_layout,
            .name = "Shadow Map Pass Pipeline",
        };

        auto pipeline = _device->create_graphics_pipeline(pipeline_desc);

        auto descriptor_buffer = builder.create_per_frame_buffer({
            .size = _device->get_descriptor_set_layout_size(descriptor_set_layout),
            .location = rhi::memory_location::automatic,
            .usage = make_enum_mask(rhi::buffer_usage::descriptor, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "Shadow Map Pass Descriptor Set Buffer",
        });

        builder.create_transfer_pass(
            "Shadow Data Upload",
            [&](transfer_task_builder& task) {
                task.write(shadow_data_buffer, make_enum_mask(rhi::pipeline_stage::copy),
                           make_enum_mask(rhi::memory_access::transfer_write));
                task.read(shadow_data_buffer, make_enum_mask(rhi::pipeline_stage::none),
                          make_enum_mask(rhi::memory_access::none));
            },
            &_shadow_upload_pass_task, this);

        builder.create_graphics_pass(
            "Shadow Map Pass",
            [&](graphics_task_builder& task) {
                task.read(_global_resources.graph_vertex_pull_buffer,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_mesh_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_object_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_instance_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_material_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(descriptor_buffer,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.write(shadow_mega_texture, rhi::image_layout::depth,
                           make_enum_mask(rhi::pipeline_stage::all_fragment_tests),
                           make_enum_mask(rhi::memory_access::depth_stencil_attachment_write));
            },
            &_shadow_map_pass_task, this, descriptor_buffer);

        _shadow_data.shelf_pack.emplace(
            math::vec2{
                _cfg.shadows.shadow_map_width,
                _cfg.shadows.shadow_map_height,
            },
            shelf_pack_allocator::allocator_options{
                .alignment =
                    {
                        32,
                        32,
                    },
                .column_count = 4,
            });

        return {
            .shadow_map_megatexture = shadow_mega_texture,
            .shadow_data = shadow_data_buffer,
            .directional_shadow_pipeline = pipeline,
            .directional_shadow_pipeline_layout = pipeline_layout,
            .scene_descriptor_layout = descriptor_set_layout,
        };
    }

    void pbr_frame_graph::_release_shadow_map_pass(shadow_map_pass_outputs& outputs)
    {
        _device->destroy_graphics_pipeline(outputs.directional_shadow_pipeline);

        outputs = {};
    }

    pbr_frame_graph::pbr_opaque_pass_outputs pbr_frame_graph::_add_pbr_opaque_pass(graph_builder& builder)
    {
        auto hdr_color_output = builder.create_per_frame_image({
            .format = _cfg.hdr_color_format,
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
            .name = "PBR Opaque Pass Color Output",
        });

        // Scene Descriptors
        // 0 - Scene Constants
        // 1 - Vertex Pull Buffer
        // 2 - Meshes
        // 3 - Objects
        // 4 - Instances
        // 5 - Materials
        // 6 - Ambient Occlusion Texture
        // 15 - Linear Sampler
        // 16+ - Bindless Textures

        // Light and Shadow Descriptors
        // 0 - Lights
        // 1 - Shadow map parameters
        // 2 - Shadow map mega texture
        // 3 - Light grid bounds
        // 4 - Light indices
        auto scene_descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();
        scene_descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 5,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 6,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 15,
            .type = rhi::descriptor_type::sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 16,
            .type = rhi::descriptor_type::sampled_image,
            .count = _cfg.max_bindless_textures,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        auto scene_descriptors = _device->create_descriptor_set_layout(
            scene_descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::descriptor_buffer));

        auto shadow_descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 2,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        auto shadow_descriptors = _device->create_descriptor_set_layout(
            shadow_descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::descriptor_buffer));

        auto descriptor_set_layouts = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>>{};
        descriptor_set_layouts.push_back(scene_descriptors);
        descriptor_set_layouts.push_back(shadow_descriptors);

        auto pipeline_layout = _device->create_pipeline_layout({
            .descriptor_set_layouts = descriptor_set_layouts,
            .push_constants = {},
        });

        auto vert_source = core::read_bytes("assets/shaders/pbr.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/pbr.frag.spv");

        auto color_formats = vector<rhi::image_format>();
        color_formats.push_back(_cfg.hdr_color_format);

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
            .name = "PBR Opaque Pass Pipeline",
        };

        auto pipeline = _device->create_graphics_pipeline(pipeline_desc);

        auto scene_descriptor_buffer = builder.create_per_frame_buffer({
            .size = _device->get_descriptor_set_layout_size(scene_descriptors),
            .location = rhi::memory_location::automatic,
            .usage = make_enum_mask(rhi::buffer_usage::descriptor, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "PBR Opaque Pass Scene Descriptor Set Buffer",
        });

        auto shadow_descriptor_buffer = builder.create_per_frame_buffer({
            .size = _device->get_descriptor_set_layout_size(shadow_descriptors),
            .location = rhi::memory_location::automatic,
            .usage = make_enum_mask(rhi::buffer_usage::descriptor, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "PBR Opaque Pass Shadow Descriptor Set Buffer",
        });

        builder.create_graphics_pass(
            "PBR Opaque Pass",
            [&](graphics_task_builder& task) {
                task.read_write(_pass_output_resource_handles.depth_prepass.depth, rhi::image_layout::depth,
                                make_enum_mask(rhi::pipeline_stage::all_fragment_tests),
                                make_enum_mask(rhi::memory_access::depth_stencil_attachment_read),
                                make_enum_mask(rhi::pipeline_stage::all_fragment_tests),
                                make_enum_mask(rhi::memory_access::depth_stencil_attachment_write));
                task.read(_pass_output_resource_handles.ssao_blur.ssao_blurred_output,
                          rhi::image_layout::shader_read_only, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.read(_global_resources.graph_vertex_pull_buffer,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_mesh_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_object_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_instance_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_material_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_light_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.read(scene_descriptor_buffer,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(shadow_descriptor_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.read(_pass_output_resource_handles.light_culling.light_grid_ranges,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.light_culling.light_indices,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.shadow_map.shadow_map_megatexture,
                          rhi::image_layout::shader_read_only, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.shadow_map.shadow_data,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.ssao_blur.ssao_blurred_output,
                          rhi::image_layout::shader_read_only, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.write(hdr_color_output, rhi::image_layout::color_attachment,
                           make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                           make_enum_mask(rhi::memory_access::color_attachment_write));
            },
            &_pbr_opaque_pass_task, this, scene_descriptor_buffer, shadow_descriptor_buffer);

        return {
            .hdr_color = hdr_color_output,
            .pipeline = pipeline,
            .pipeline_layout = pipeline_layout,
            .scene_descriptor_layout = scene_descriptors,
            .shadow_and_lighting_descriptor_layout = shadow_descriptors,
        };
    }

    void pbr_frame_graph::_release_pbr_opaque_pass(pbr_opaque_pass_outputs& outputs)
    {
        _device->destroy_graphics_pipeline(outputs.pipeline);

        outputs = {};
    }

    pbr_frame_graph::mboit_gather_pass_outputs pbr_frame_graph::_add_mboit_gather_pass(graph_builder& builder)
    {
        auto transparency_accumulation = builder.create_image({
            .format = rhi::image_format::rgba16_float,
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
            .name = "MBOIT Transparency Accumulation Buffer",
        });

        auto moments_target = builder.create_image({
            .format = rhi::image_format::rgba16_float,
            .type = rhi::image_type::image_2d,
            .width = _cfg.render_target_width,
            .height = _cfg.render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage =
                make_enum_mask(rhi::image_usage::storage, rhi::image_usage::sampled, rhi::image_usage::transfer_dst),
            .name = "MBOIT Moments Target",
        });

        auto zeroth_moment_buffer = builder.create_image({
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
            .usage =
                make_enum_mask(rhi::image_usage::storage, rhi::image_usage::sampled, rhi::image_usage::transfer_dst),
            .name = "MBOIT Zeroth Moment Buffer",
        });

        // Scene Descriptors
        // 0 - Scene Constants
        // 1 - Vertex Pull Buffer
        // 2 - Meshes
        // 3 - Objects
        // 4 - Instances
        // 5 - Materials
        // 6 - Moments Buffer
        // 7 - Zeroth Moment Buffer
        // 8 - Ambient Occlusion Texture
        // 15 - Linear Sampler
        // 16+ - Bindless Textures

        // Shadow and Light Descriptors
        // 0 - Lights
        // 1 - Shadow map parameters
        // 2 - Shadow map mega texture
        // 3 - Light grid bounds
        // 4 - Light indices

        auto scene_descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();
        scene_descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 5,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 6,
            .type = rhi::descriptor_type::storage_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 7,
            .type = rhi::descriptor_type::storage_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 8,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 15,
            .type = rhi::descriptor_type::sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 16,
            .type = rhi::descriptor_type::sampled_image,
            .count = _cfg.max_bindless_textures,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        auto scene_descriptors = _device->create_descriptor_set_layout(
            scene_descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::descriptor_buffer));

        auto shadow_descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 2,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        auto shadow_descriptors = _device->create_descriptor_set_layout(
            shadow_descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::descriptor_buffer));

        auto descriptor_set_layouts = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>>{};
        descriptor_set_layouts.push_back(scene_descriptors);
        descriptor_set_layouts.push_back(shadow_descriptors);

        auto pipeline_layout = _device->create_pipeline_layout({
            .descriptor_set_layouts = descriptor_set_layouts,
            .push_constants = {},
        });

        auto color_formats = vector<rhi::image_format>();
        color_formats.push_back(rhi::image_format::rgba16_float);

        auto blending = vector<rhi::color_blend_attachment>();
        blending.push_back({
            .blend_enable = false,
            .src_color_blend_factor = rhi::blend_factor::src_alpha,
            .dst_color_blend_factor = rhi::blend_factor::one_minus_src_alpha,
            .color_blend_op = rhi::blend_op::add,
            .src_alpha_blend_factor = rhi::blend_factor::one,
            .dst_alpha_blend_factor = rhi::blend_factor::one_minus_constant_alpha,
            .alpha_blend_op = rhi::blend_op::add,
        });

        auto vert_source = core::read_bytes("assets/shaders/pbr_oit_gather.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/pbr_oit_gather.frag.spv");

        auto pipeline_desc = rhi::graphics_pipeline_desc{
            .color_attachment_formats = color_formats,
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
                            .write_enable = false,
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
            .name = "MBOIT Gather Pass Pipeline",
        };

        auto pipeline = _device->create_graphics_pipeline(pipeline_desc);

        auto scene_descriptor_buffer = builder.create_per_frame_buffer({
            .size = _device->get_descriptor_set_layout_size(scene_descriptors),
            .location = rhi::memory_location::automatic,
            .usage = make_enum_mask(rhi::buffer_usage::descriptor, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "MBOIT Gather Pass Scene Descriptor Set Buffer",
        });

        auto shadow_descriptor_buffer = builder.create_per_frame_buffer({
            .size = _device->get_descriptor_set_layout_size(shadow_descriptors),
            .location = rhi::memory_location::automatic,
            .usage = make_enum_mask(rhi::buffer_usage::descriptor, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "MBOIT Gather Pass Shadow Descriptor Set Buffer",
        });

        builder.create_transfer_pass(
            "MBOIT Clear Buffer Pass",
            [&](transfer_task_builder& task) {
                task.write(moments_target, rhi::image_layout::transfer_dst, make_enum_mask(rhi::pipeline_stage::clear),
                           make_enum_mask(rhi::memory_access::transfer_write));
                task.write(zeroth_moment_buffer, rhi::image_layout::transfer_dst,
                           make_enum_mask(rhi::pipeline_stage::clear),
                           make_enum_mask(rhi::memory_access::transfer_write));
            },
            [](transfer_task_execution_context& ctx, auto zero_moment, auto moments) {
                ctx.clear_color(zero_moment, 0.0f, 0.0f, 0.0f, 0.0f);
                ctx.clear_color(moments, 0.0f, 0.0f, 0.0f, 0.0f);
            },
            zeroth_moment_buffer, moments_target);

        builder.create_graphics_pass(
            "MBOIT Gather Pass",
            [&](graphics_task_builder& task) {
                task.read(_pass_output_resource_handles.depth_prepass.depth, rhi::image_layout::depth_stencil_read_only,
                          make_enum_mask(rhi::pipeline_stage::all_fragment_tests),
                          make_enum_mask(rhi::memory_access::depth_stencil_attachment_read));

                task.read(_global_resources.graph_vertex_pull_buffer,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_mesh_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_object_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_instance_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_material_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_light_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.read(_pass_output_resource_handles.light_culling.light_grid,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.light_culling.light_grid_ranges,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.light_culling.light_indices,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.shadow_map.shadow_map_megatexture,
                          rhi::image_layout::shader_read_only, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.ssao_blur.ssao_blurred_output,
                          rhi::image_layout::shader_read_only, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.read(scene_descriptor_buffer,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(shadow_descriptor_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.write(transparency_accumulation, rhi::image_layout::color_attachment,
                           make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                           make_enum_mask(rhi::memory_access::color_attachment_write));

                task.read_write(moments_target, rhi::image_layout::general,
                                make_enum_mask(rhi::pipeline_stage::fragment_shader),
                                make_enum_mask(rhi::memory_access::shader_read),
                                make_enum_mask(rhi::pipeline_stage::fragment_shader),
                                make_enum_mask(rhi::memory_access::shader_write));
                task.read_write(zeroth_moment_buffer, rhi::image_layout::general,
                                make_enum_mask(rhi::pipeline_stage::fragment_shader),
                                make_enum_mask(rhi::memory_access::shader_read),
                                make_enum_mask(rhi::pipeline_stage::fragment_shader),
                                make_enum_mask(rhi::memory_access::shader_write));
            },
            &_mboit_gather_pass_task, this, scene_descriptor_buffer, shadow_descriptor_buffer);

        return {
            .transparency_accumulation = transparency_accumulation,
            .moments_buffer = moments_target,
            .zeroth_moment_buffer = zeroth_moment_buffer,
            .pipeline = pipeline,
            .pipeline_layout = pipeline_layout,
            .scene_descriptor_layout = scene_descriptors,
            .shadow_and_lighting_descriptor_layout = shadow_descriptors,
        };
    }

    void pbr_frame_graph::_release_mboit_gather_pass(mboit_gather_pass_outputs& outputs)
    {
        _device->destroy_graphics_pipeline(outputs.pipeline);

        outputs = {};
    }

    pbr_frame_graph::mboit_resolve_pass_outputs pbr_frame_graph::_add_mboit_resolve_pass(graph_builder& builder)
    {
        auto transparency_accumulator = _pass_output_resource_handles.mboit_gather.transparency_accumulation;
        auto moments_buffer = _pass_output_resource_handles.mboit_gather.moments_buffer;
        auto zeroth_moment_buffer = _pass_output_resource_handles.mboit_gather.zeroth_moment_buffer;

        // Scene Descriptors
        // 0 - Scene Constants
        // 1 - Vertex Pull Buffer
        // 2 - Meshes
        // 3 - Objects
        // 4 - Instances
        // 5 - Materials
        // 6 - Moments Buffer
        // 7 - Zeroth Moment Buffer
        // 8 - Ambient Occlusion Texture
        // 15 - Linear Sampler
        // 16+ - Bindless Textures

        // Shadow and Light Descriptors
        // 0 - Lights
        // 1 - Shadow map parameters
        // 2 - Shadow map mega texture
        // 3 - Light grid bounds
        // 4 - Light indices

        auto scene_descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();
        scene_descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::vertex),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 5,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 6,
            .type = rhi::descriptor_type::storage_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 7,
            .type = rhi::descriptor_type::storage_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 8,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 15,
            .type = rhi::descriptor_type::sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        scene_descriptor_set_bindings.push_back({
            .binding_index = 16,
            .type = rhi::descriptor_type::sampled_image,
            .count = _cfg.max_bindless_textures,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        auto scene_descriptors = _device->create_descriptor_set_layout(
            scene_descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::descriptor_buffer));

        auto shadow_descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 2,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        shadow_descriptor_set_bindings.push_back({
            .binding_index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        auto shadow_descriptors = _device->create_descriptor_set_layout(
            shadow_descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::descriptor_buffer));

        auto descriptor_set_layouts = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>>{};
        descriptor_set_layouts.push_back(scene_descriptors);
        descriptor_set_layouts.push_back(shadow_descriptors);

        auto pipeline_layout = _device->create_pipeline_layout({
            .descriptor_set_layouts = descriptor_set_layouts,
            .push_constants = {},
        });

        auto color_formats = vector<rhi::image_format>();
        color_formats.push_back(rhi::image_format::rgba16_float);

        auto blending = vector<rhi::color_blend_attachment>();
        blending.push_back({
            .blend_enable = true,
            .src_color_blend_factor = rhi::blend_factor::one,
            .dst_color_blend_factor = rhi::blend_factor::one,
            .color_blend_op = rhi::blend_op::add,
            .src_alpha_blend_factor = rhi::blend_factor::one,
            .dst_alpha_blend_factor = rhi::blend_factor::one,
            .alpha_blend_op = rhi::blend_op::add,
        });

        auto vert_source = core::read_bytes("assets/shaders/pbr_oit_resolve.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/pbr_oit_resolve.frag.spv");

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
                    .depth =
                        rhi::depth_test{
                            .write_enable = false,
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
            .name = "MBOIT Resolve Pass Pipeline",
        };

        auto pipeline = _device->create_graphics_pipeline(pipeline_desc);

        auto scene_descriptor_buffer = builder.create_per_frame_buffer({
            .size = _device->get_descriptor_set_layout_size(scene_descriptors),
            .location = rhi::memory_location::automatic,
            .usage = make_enum_mask(rhi::buffer_usage::descriptor, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "MBOIT Resolve Pass Scene Descriptor Set Buffer",
        });

        auto shadow_descriptor_buffer = builder.create_per_frame_buffer({
            .size = _device->get_descriptor_set_layout_size(shadow_descriptors),
            .location = rhi::memory_location::automatic,
            .usage = make_enum_mask(rhi::buffer_usage::descriptor, rhi::buffer_usage::transfer_dst),
            .access_type = rhi::host_access_type::coherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "MBOIT Resolve Pass Shadow Descriptor Set Buffer",
        });

        builder.create_graphics_pass(
            "MBOIT Resolve Pass",
            [&](graphics_task_builder& task) {
                task.read(_pass_output_resource_handles.depth_prepass.depth, rhi::image_layout::depth_stencil_read_only,
                          make_enum_mask(rhi::pipeline_stage::all_fragment_tests),
                          make_enum_mask(rhi::memory_access::depth_stencil_attachment_read));

                task.read(_global_resources.graph_vertex_pull_buffer,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_mesh_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_object_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_instance_buffer, make_enum_mask(rhi::pipeline_stage::vertex_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_material_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_global_resources.graph_light_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.read(_pass_output_resource_handles.light_culling.light_grid,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.light_culling.light_grid_ranges,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.light_culling.light_indices,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.shadow_map.shadow_map_megatexture,
                          rhi::image_layout::shader_read_only, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.ssao_blur.ssao_blurred_output,
                          rhi::image_layout::shader_read_only, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.read(scene_descriptor_buffer,
                          make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(shadow_descriptor_buffer, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.write(transparency_accumulator, rhi::image_layout::color_attachment,
                           make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                           make_enum_mask(rhi::memory_access::color_attachment_write));

                task.write(moments_buffer, rhi::image_layout::general,
                           make_enum_mask(rhi::pipeline_stage::fragment_shader),
                           make_enum_mask(rhi::memory_access::shader_write, rhi::memory_access::shader_read));
                task.write(zeroth_moment_buffer, rhi::image_layout::general,
                           make_enum_mask(rhi::pipeline_stage::fragment_shader),
                           make_enum_mask(rhi::memory_access::shader_write, rhi::memory_access::shader_read));
            },
            &_mboit_resolve_pass_task, this, scene_descriptor_buffer, shadow_descriptor_buffer);

        return {
            .transparency_accumulation = transparency_accumulator,
            .moments_buffer = moments_buffer,
            .zeroth_moment_buffer = zeroth_moment_buffer,
            .pipeline = pipeline,
            .pipeline_layout = pipeline_layout,
            .scene_descriptor_layout = scene_descriptors,
            .shadow_and_lighting_descriptor_layout = shadow_descriptors,
        };
    }

    void pbr_frame_graph::_release_mboit_resolve_pass(mboit_resolve_pass_outputs& outputs)
    {
        _device->destroy_graphics_pipeline(outputs.pipeline);

        outputs = {};
    }

    pbr_frame_graph::mboit_blend_pass_outputs pbr_frame_graph::_add_mboit_blend_pass(graph_builder& builder)
    {
        auto hdr_color = _pass_output_resource_handles.pbr_opaque.hdr_color;

        // Bindings
        // 0 - Moments Buffer
        // 1 - Zeroth Moment Buffer
        // 2 - Transparency Accumulation Buffer
        // 3 - Linear Sampler

        auto descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();
        descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::storage_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::storage_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        descriptor_set_bindings.push_back({
            .binding_index = 2,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        descriptor_set_bindings.push_back({
            .binding_index = 3,
            .type = rhi::descriptor_type::sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        auto descriptor_set = _device->create_descriptor_set_layout(
            descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::push));

        auto descriptor_sets = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>>{};
        descriptor_sets.push_back(descriptor_set);

        auto pipeline_layout = _device->create_pipeline_layout({
            .descriptor_set_layouts = descriptor_sets,
            .push_constants = {},
        });

        auto vert_source = core::read_bytes("assets/shaders/pbr_oit_blend.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/pbr_oit_blend.frag.spv");

        auto color_formats = vector<rhi::image_format>();
        color_formats.push_back(_cfg.hdr_color_format);

        auto blending = vector<rhi::color_blend_attachment>();
        blending.push_back(rhi::color_blend_attachment{
            .blend_enable = true,
            .src_color_blend_factor = rhi::blend_factor::src_alpha,
            .dst_color_blend_factor = rhi::blend_factor::one_minus_src_alpha,
            .color_blend_op = rhi::blend_op::add,
            .src_alpha_blend_factor = rhi::blend_factor::one,
            .dst_alpha_blend_factor = rhi::blend_factor::one_minus_src_alpha,
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
            .name = "MBOIT Blend Pass Pipeline",
        };

        auto pipeline = _device->create_graphics_pipeline(pipeline_desc);

        builder.create_graphics_pass(
            "MBOIT Blend Pass",
            [&](graphics_task_builder& task) {
                task.read(_pass_output_resource_handles.mboit_resolve.transparency_accumulation,
                          rhi::image_layout::shader_read_only, make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.mboit_resolve.moments_buffer, rhi::image_layout::general,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));
                task.read(_pass_output_resource_handles.mboit_resolve.zeroth_moment_buffer, rhi::image_layout::general,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.read_write(hdr_color, rhi::image_layout::color_attachment,
                                make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                                make_enum_mask(rhi::memory_access::color_attachment_read),
                                make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                                make_enum_mask(rhi::memory_access::color_attachment_write));
            },
            &_mboit_blend_pass_task, this);

        return {
            .hdr_color = hdr_color,
            .pipeline = pipeline,
            .pipeline_layout = pipeline_layout,
        };
    }

    void pbr_frame_graph::_release_mboit_blend_pass(mboit_blend_pass_outputs& outputs)
    {
        _device->destroy_graphics_pipeline(outputs.pipeline);

        outputs = {};
    }

    pbr_frame_graph::tonemapping_pass_outputs pbr_frame_graph::_add_tonemapping_pass(graph_builder& builder)
    {
        auto tonemapped_buffer = builder.create_render_target({
            .format = _cfg.tonemapped_color_format,
            .type = rhi::image_type::image_2d,
            .width = _cfg.render_target_width,
            .height = _cfg.render_target_height,
            .depth = 1,
            .array_layers = 1,
            .mip_levels = 1,
            .sample_count = rhi::image_sample_count::sample_count_1,
            .tiling = rhi::image_tiling_type::optimal,
            .location = rhi::memory_location::device,
            .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled,
                                    rhi::image_usage::transfer_src),
            .name = "MBOIT Moments Target",
        });

        auto descriptor_set_bindings = vector<rhi::descriptor_binding_layout>();
        descriptor_set_bindings.push_back({
            .binding_index = 0,
            .type = rhi::descriptor_type::sampled_image,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });
        descriptor_set_bindings.push_back({
            .binding_index = 1,
            .type = rhi::descriptor_type::sampler,
            .count = 1,
            .stages = make_enum_mask(rhi::shader_stage::fragment),
        });

        auto descriptor_set = _device->create_descriptor_set_layout(
            descriptor_set_bindings, make_enum_mask(rhi::descriptor_set_layout_flags::push));

        auto descriptor_sets = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::descriptor_set_layout>>{};
        descriptor_sets.push_back(descriptor_set);

        auto pipeline_layout = _device->create_pipeline_layout({
            .descriptor_set_layouts = descriptor_sets,
            .push_constants = {},
        });

        auto vert_source = core::read_bytes("assets/shaders/tonemap.vert.spv");
        auto frag_source = core::read_bytes("assets/shaders/tonemap.frag.spv");

        auto color_formats = vector<rhi::image_format>();
        color_formats.push_back(_cfg.tonemapped_color_format);

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
            .name = "Tonemapping Pass Pipeline",
        };

        auto pipeline = _device->create_graphics_pipeline(pipeline_desc);

        builder.create_graphics_pass(
            "Tonemapping Pass",
            [&](graphics_task_builder& task) {
                task.read(_pass_output_resource_handles.mboit_blend.hdr_color, rhi::image_layout::shader_read_only,
                          make_enum_mask(rhi::pipeline_stage::fragment_shader),
                          make_enum_mask(rhi::memory_access::shader_read));

                task.write(tonemapped_buffer, rhi::image_layout::color_attachment,
                           make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                           make_enum_mask(rhi::memory_access::color_attachment_write));
            },
            &_tonemapping_pass_task, this);

        return {
            .tonemapped_color = tonemapped_buffer,
            .pipeline = pipeline,
            .pipeline_layout = pipeline_layout,
        };
    }

    void pbr_frame_graph::_release_tonemapping_pass(tonemapping_pass_outputs& outputs)
    {
        _device->destroy_graphics_pipeline(outputs.pipeline);

        outputs = {};
    }

    void pbr_frame_graph::_upload_pass_task(transfer_task_execution_context& ctx, pbr_frame_graph* self)
    {
        // No actual rendering commands needed, just resource uploads
        const auto staging_buffer_offset =
            self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_per_frame_staging_buffer) +
            self->_global_resources.utilization.staging_buffer_bytes_written;
        auto staging_buffer_bytes = self->_device->map_buffer(
            self->_executor->get_buffer(self->_global_resources.graph_per_frame_staging_buffer));
        auto staging_bytes_written = static_cast<size_t>(0u);

        // Find the camera to upload
        auto camera = ecs::archetype_entity{ecs::tombstone};
        auto camera_data = optional<camera_component>();
        auto camera_transform = optional<ecs::transform_component>();

        self->_inputs.entity_registry->each(
            [&](ecs::self_component entity, const camera_component& cam_comp, const ecs::transform_component& tx) {
                camera = entity.entity;
                camera_data = cam_comp;
                camera_transform = tx;
            });

        const auto quat_rot = math::quat(camera_transform->rotation());
        const auto f = math::extract_forward(quat_rot);
        const auto u = math::extract_up(quat_rot);

        const auto projection =
            math::perspective(camera_data->aspect_ratio, camera_data->vertical_fov, camera_data->near_plane);
        const auto view = math::look_at(camera_transform->position(), camera_transform->position() + f, u);

        // Set up and upload the scene constants
        auto scene_constants_data = scene_constants{};
        scene_constants_data.cam = {
            .proj = projection,
            .inv_proj = math::inverse(projection),
            .view = view,
            .inv_view = math::inverse(view),
            .position = camera_transform->position(),
        };
        scene_constants_data.ambient_light_color = math::vec3<float>(253, 242, 200) / 255.0f * 0.1f;
        scene_constants_data.screen_size = math::vec2(static_cast<float>(self->_cfg.render_target_width),
                                                      static_cast<float>(self->_cfg.render_target_height));

        self->_scene_data.primary_camera = scene_constants_data.cam;

        // Set up the lights
        self->_inputs.entity_registry->each([&](ecs::self_component self_entity, point_light_component point_light,
                                                const ecs::transform_component& transform) {
            auto gpu_light = light{};
            gpu_light.color_intensity =
                math::vec4(point_light.color.r, point_light.color.g, point_light.color.b, point_light.intensity);
            gpu_light.type = light_type::point;
            gpu_light.position_falloff =
                math::vec4(transform.position().x, transform.position().y, transform.position().z, point_light.range);
            gpu_light.enabled = true;

            self->_scene_data.point_lights.insert_or_replace(self_entity.entity, gpu_light);
        });

        self->_inputs.entity_registry->each([&](ecs::self_component self_entity, directional_light_component dir_light,
                                                const ecs::transform_component& transform) {
            auto gpu_light = light{};
            gpu_light.color_intensity =
                math::vec4(dir_light.color.r, dir_light.color.g, dir_light.color.b, dir_light.intensity);
            gpu_light.type = light_type::directional;

            const auto light_rot = math::rotate(transform.rotation());
            const auto light_dir = light_rot * math::vec4(0.0f, 0.0f, 1.0f, 0.0f);

            gpu_light.direction_angle = math::vec4(light_dir.x, light_dir.y, light_dir.z, 0.0f);
            gpu_light.enabled = true;
            self->_scene_data.dir_lights.insert_or_replace(self_entity.entity, gpu_light);
        });

        self->_shadow_data.shelf_pack->clear();
        self->_shadow_data.shadow_map_parameters.clear();

        auto sun_entity = ecs::archetype_entity{ecs::tombstone};

        for (const auto& [e, _] : self->_scene_data.dir_lights)
        {
            sun_entity = e;
            break;
        }

        scene_constants_data.light_grid_count_and_size = math::vec4{
            self->_cfg.light_clustering.cluster_count_x,
            self->_cfg.light_clustering.cluster_count_y,
            self->_cfg.light_clustering.cluster_count_z,
            self->_cfg.render_target_width / self->_cfg.light_clustering.cluster_count_x,
        };
        scene_constants_data.light_grid_z_bounds = math::vec2{
            0.1f,
            1000.0f,
        };

        self->_shadow_data.light_shadow_data.clear();

        auto shadow_maps_written = 0u;
        self->_inputs.entity_registry->each(
            [&](ecs::self_component self_entity, shadow_map_component shadows, ecs::transform_component transform) {
                const auto cascade_info = _calculate_shadow_map_cascades(shadows, transform, *camera_data, view);
                self->_shadow_data.light_shadow_data.insert({self_entity.entity, cascade_info});

                auto light = [&]() {
                    auto point_light_it = self->_scene_data.point_lights.find(self_entity.entity);
                    if (point_light_it != self->_scene_data.point_lights.end())
                    {
                        return point_light_it->second;
                    }

                    auto dir_light_it = self->_scene_data.dir_lights.find(self_entity.entity);
                    if (dir_light_it != self->_scene_data.dir_lights.end())
                    {
                        return dir_light_it->second;
                    }

                    terminate();
                }();

                light.shadow_map_count = shadows.cascade_count;
                for (auto i = 0u; i < shadows.cascade_count; ++i)
                {
                    light.shadow_map_indices[i] = shadow_maps_written++;
                }

                if (light.type == light_type::directional)
                {
                    self->_scene_data.dir_lights.insert_or_replace(self_entity.entity, light);
                }
                else
                {
                    self->_scene_data.point_lights.insert_or_replace(self_entity.entity, light);
                }
            });

        self->_inputs.entity_registry->each([&]([[maybe_unused]] directional_light_component light,
                                                shadow_map_component shadows, ecs::self_component self_entity) {
            const auto cascade_it = self->_shadow_data.light_shadow_data.find(self_entity.entity);
            if (cascade_it == self->_shadow_data.light_shadow_data.end())
            {
                return;
            }

            const auto& cascade = cascade_it->second;

            for (auto i = 0u; i < shadows.cascade_count; ++i)
            {
                const auto region = self->_shadow_data.shelf_pack->allocate(shadows.size);
                const auto x_pos = region->position.x;
                const auto y_pos = region->position.y;
                const auto width = region->extent.x;
                const auto height = region->extent.y;

                const auto& allocator = *self->_shadow_data.shelf_pack;

                self->_shadow_data.shadow_map_parameters.push_back(shadow_map_parameter{
                    .light_proj_matrix = cascade.frustum_view_projections[i],
                    .shadow_map_region =
                        {
                            static_cast<float>(x_pos) / allocator.extent().x,
                            static_cast<float>(y_pos) / allocator.extent().y,
                            static_cast<float>(width) / allocator.extent().x,
                            static_cast<float>(height) / allocator.extent().y,
                        },
                    .cascade_split_far = cascade.cascade_distances[i],
                });
            }
        });

        scene_constants_data.sun = self->_scene_data.dir_lights[sun_entity];

        // Copy scene constants to staging buffer
        std::memcpy(staging_buffer_bytes + staging_buffer_offset + staging_bytes_written, &scene_constants_data,
                    sizeof(scene_constants));

        const auto scene_constants_offset = staging_bytes_written;

        staging_bytes_written += sizeof(scene_constants);

        // Build out the draw commands
        for (auto&& [_, draw_batch] : self->_drawables.draw_batches)
        {
            draw_batch.commands.clear();
        }

        auto entity_registry = self->_inputs.entity_registry;
        entity_registry->each([&](ecs::self_component self_entity, renderable_component renderable) {
            const auto entity = self_entity.entity;
            auto object_payload = object_data{
                .model = math::mat4(1.0f),
                .inv_tranpose_model = math::mat4(1.0f),
                .mesh_id = static_cast<uint32_t>(renderable.mesh_id),
                .material_id = static_cast<uint32_t>(renderable.material_id),
                .parent_id = ~0u,
                .self_id = static_cast<uint32_t>(renderable.object_id),
            };

            auto ancestors = ecs::archetype_entity_ancestor_view(*entity_registry, entity);
            for (auto ancestor : ancestors)
            {
                if (auto parent_tx = entity_registry->try_get<ecs::transform_component>(ancestor))
                {
                    object_payload.model = parent_tx->matrix() * object_payload.model;
                }
            }

            object_payload.inv_tranpose_model = math::transpose(math::inverse(object_payload.model));

            const auto alpha = static_cast<alpha_behavior>(self->_materials.materials[renderable.material_id].type);
            const auto key = draw_batch_key{
                .alpha_type = alpha,
                .double_sided = renderable.double_sided,
            };

            auto& draw_batch = self->_drawables.draw_batches[key];
            const auto& mesh = self->_meshes.meshes[renderable.mesh_id];

            if (draw_batch.objects.find(entity) == draw_batch.objects.end())
            {
                draw_batch.objects.insert(entity, object_payload);
            }
            else
            {
                draw_batch.objects[entity] = object_payload;
            }

            draw_batch.commands.push_back({
                .index_count = mesh.index_count,
                .instance_count = 1,
                .first_index = (mesh.mesh_start_offset + mesh.index_offset) / static_cast<uint32_t>(sizeof(uint32_t)),
                .vertex_offset = 0,
                .first_instance = static_cast<uint32_t>(draw_batch.objects.index_of(entity)),
            });
        });

        auto instance_written_count = 0u;
        for (const auto& [_, batch] : self->_drawables.draw_batches)
        {
            for (auto& cmd : batch.commands)
            {
                cmd.first_instance += instance_written_count;
            }
            instance_written_count += static_cast<uint32_t>(batch.objects.size());
        }

        // Upload the object data buffer

        const auto object_buffer = self->_global_resources.graph_object_buffer;
        auto object_buffer_offset = self->_executor->get_current_frame_resource_offset(object_buffer);
        auto object_buffer_written = static_cast<size_t>(0u);

        const auto object_buffer_staging_offset = staging_bytes_written;

        for (auto&& [_, draw_batch] : self->_drawables.draw_batches)
        {
            std::memcpy(staging_buffer_bytes + staging_buffer_offset + staging_bytes_written,
                        draw_batch.objects.values(), draw_batch.objects.size() * sizeof(object_data));
            staging_bytes_written += draw_batch.objects.size() * sizeof(object_data);
            object_buffer_written += draw_batch.objects.size() * sizeof(object_data);
        }

        // Write instances

        auto instance_buffer = self->_global_resources.graph_instance_buffer;
        auto instance_buffer_offset = self->_executor->get_current_frame_resource_offset(instance_buffer);
        auto instance_bytes_written = static_cast<size_t>(0u);

        const auto instance_buffer_staging_offset = staging_bytes_written;

        auto instances_written = 0u;
        for (auto&& [_, draw_batch] : self->_drawables.draw_batches)
        {
            auto instance_indices = vector<uint32_t>{draw_batch.objects.size()};
            std::iota(instance_indices.begin(), instance_indices.end(), instances_written);
            std::memcpy(staging_buffer_bytes + staging_buffer_offset + staging_bytes_written, instance_indices.data(),
                        instance_indices.size() * sizeof(uint32_t));
            staging_bytes_written += instance_indices.size() * sizeof(uint32_t);
            draw_batch.indirect_command_offset = instances_written;

            instances_written += static_cast<uint32_t>(draw_batch.objects.size());
            instance_bytes_written += draw_batch.objects.size() * sizeof(uint32_t);
        }

        // Upload the point and spot lights

        auto light_buffer = self->_global_resources.graph_light_buffer;
        auto light_buffer_offset = self->_executor->get_current_frame_resource_offset(light_buffer);
        auto light_buffer_written = static_cast<size_t>(0u);

        const auto light_buffer_staging_offset = staging_bytes_written;

        std::memcpy(staging_buffer_bytes + staging_buffer_offset + staging_bytes_written,
                    self->_scene_data.point_lights.values(), self->_scene_data.point_lights.size() * sizeof(light));
        staging_bytes_written += self->_scene_data.point_lights.size() * sizeof(light);
        light_buffer_written += self->_scene_data.point_lights.size() * sizeof(light);

        // Unmap the staging buffer and push copy commands
        self->_device->unmap_buffer(
            self->_executor->get_buffer(self->_global_resources.graph_per_frame_staging_buffer));

        // Copy from staging buffer to the actual scene constants buffer
        ctx.copy_buffer_to_buffer(self->_global_resources.graph_per_frame_staging_buffer,
                                  self->_pass_output_resource_handles.upload_pass.scene_constants,
                                  staging_buffer_offset + scene_constants_offset,
                                  self->_executor->get_current_frame_resource_offset(
                                      self->_pass_output_resource_handles.upload_pass.scene_constants),
                                  sizeof(scene_constants));

        ctx.copy_buffer_to_buffer(
            self->_global_resources.graph_per_frame_staging_buffer, self->_global_resources.graph_object_buffer,
            staging_buffer_offset + object_buffer_staging_offset, object_buffer_offset, object_buffer_written);

        ctx.copy_buffer_to_buffer(
            self->_global_resources.graph_per_frame_staging_buffer, self->_global_resources.graph_instance_buffer,
            staging_buffer_offset + instance_buffer_staging_offset, instance_buffer_offset, instance_bytes_written);

        ctx.copy_buffer_to_buffer(
            self->_global_resources.graph_per_frame_staging_buffer, self->_global_resources.graph_light_buffer,
            staging_buffer_offset + light_buffer_staging_offset, light_buffer_offset, light_buffer_written);

        // Upload draw commands
        const auto draw_command_buffer = self->_pass_output_resource_handles.upload_pass.draw_commands;
        auto draw_command_bytes = self->_device->map_buffer(self->_executor->get_buffer(draw_command_buffer));
        auto draw_command_offset = self->_executor->get_current_frame_resource_offset(draw_command_buffer);
        for (auto&& [_, draw_batch] : self->_drawables.draw_batches)
        {
            std::memcpy(draw_command_bytes + draw_command_offset, draw_batch.commands.data(),
                        sizeof(indexed_indirect_command) * draw_batch.commands.size());
            draw_command_offset += sizeof(indexed_indirect_command) * draw_batch.commands.size();
        }
        self->_device->unmap_buffer(self->_executor->get_buffer(draw_command_buffer));

        self->_global_resources.utilization.staging_buffer_bytes_written = static_cast<uint32_t>(staging_bytes_written);
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

        auto scene_constants = self->_pass_output_resource_handles.upload_pass.scene_constants;
        auto vertex_pull_buffer = self->_global_resources.graph_vertex_pull_buffer;
        auto mesh_buffer = self->_global_resources.graph_mesh_buffer;
        auto object_buffer = self->_global_resources.graph_object_buffer;
        auto instance_buffer = self->_global_resources.graph_instance_buffer;
        auto material_buffer = self->_global_resources.graph_material_buffer;

        auto scene_descriptor_write_desc = rhi::descriptor_set_desc{};
        scene_descriptor_write_desc.layout = self->_pass_output_resource_handles.depth_prepass.scene_descriptor_layout;

        scene_descriptor_write_desc.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .offset = static_cast<uint32_t>(self->_executor->get_current_frame_resource_offset(scene_constants)),
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(scene_constants)),
            .buffer = ctx.find_buffer(scene_constants),
        });
        scene_descriptor_write_desc.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(self->_executor->get_current_frame_resource_offset(vertex_pull_buffer)),
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(vertex_pull_buffer)),
            .buffer = ctx.find_buffer(vertex_pull_buffer),
        });

        scene_descriptor_write_desc.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(self->_executor->get_current_frame_resource_offset(mesh_buffer)),
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(mesh_buffer)),
            .buffer = ctx.find_buffer(mesh_buffer),
        });

        scene_descriptor_write_desc.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(self->_executor->get_current_frame_resource_offset(object_buffer)),
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(object_buffer)),
            .buffer = ctx.find_buffer(object_buffer),
        });

        scene_descriptor_write_desc.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(self->_executor->get_current_frame_resource_offset(instance_buffer)),
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(instance_buffer)),
            .buffer = ctx.find_buffer(instance_buffer),
        });

        scene_descriptor_write_desc.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 5,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(self->_executor->get_current_frame_resource_offset(material_buffer)),
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(material_buffer)),
            .buffer = ctx.find_buffer(material_buffer),
        });

        auto samplers = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>>{};
        samplers.push_back(self->_global_resources.linear_sampler);

        scene_descriptor_write_desc.samplers.push_back(rhi::sampler_binding_descriptor{
            .index = 15,
            .samplers = move(samplers),
        });

        auto images = vector<rhi::image_binding_info>();
        const auto image_count =
            min(self->_cfg.max_bindless_textures, static_cast<uint32_t>(self->_bindless_textures.images.size()));

        for (uint32_t i = 0; i < image_count; i++)
        {
            images.push_back(rhi::image_binding_info{
                .image = self->_bindless_textures.images[i],
                .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
                .layout = rhi::image_layout::shader_read_only,
            });
        }

        scene_descriptor_write_desc.images.push_back(rhi::image_binding_descriptor{
            .index = 16,
            .type = rhi::descriptor_type::sampled_image,
            .images = move(images),
        });

        auto scene_descriptor_buffer_bytes = self->_device->map_buffer(ctx.find_buffer(descriptors));

        self->_device->write_descriptor_buffer(scene_descriptor_write_desc, scene_descriptor_buffer_bytes,
                                               self->_executor->get_current_frame_resource_offset(descriptors));

        self->_device->unmap_buffer(ctx.find_buffer(descriptors));

        auto desc_bufs = vector<graph_resource_handle<rhi::rhi_handle_type::buffer>>{};
        desc_bufs.push_back(descriptors);

        ctx.begin_render_pass(render_pass_begin);
        ctx.bind_descriptor_buffers(self->_pass_output_resource_handles.depth_prepass.pipeline_layout,
                                    rhi::bind_point::graphics, 0, desc_bufs);

        ctx.bind_pipeline(self->_pass_output_resource_handles.depth_prepass.pipeline);
        ctx.bind_index_buffer(self->_global_resources.vertex_pull_buffer, rhi::index_format::uint32, 0);

        ctx.set_scissor(0, 0, self->_cfg.render_target_width, self->_cfg.render_target_height);
        ctx.set_viewport(0.0f, 0.0f, static_cast<float>(self->_cfg.render_target_width),
                         static_cast<float>(self->_cfg.render_target_height), 0.0f, 1.0f);
        ctx.set_cull_mode(make_enum_mask(rhi::cull_mode::back));

        const auto draw_command_buffer = self->_pass_output_resource_handles.upload_pass.draw_commands;
        const auto draw_command_buffer_offset = self->_executor->get_current_frame_resource_offset(draw_command_buffer);

        for (const auto& [key, draw_batch] : self->_drawables.draw_batches)
        {
            if (key.alpha_type == alpha_behavior::opaque)
            {
                ctx.draw_indirect(
                    draw_command_buffer,
                    static_cast<uint32_t>(draw_command_buffer_offset +
                                          draw_batch.indirect_command_offset * sizeof(indexed_indirect_command)),
                    static_cast<uint32_t>(draw_batch.commands.size()), sizeof(indexed_indirect_command));
            }
        }

        ctx.end_render_pass();
    }

    void pbr_frame_graph::_ssao_upload_task(transfer_task_execution_context& ctx, pbr_frame_graph* self)
    {
        auto consts = ssao_constants{};
        for (auto i = static_cast<size_t>(0); i < ssao_constants::ssao_kernel_size; ++i)
        {
            consts.ssao_sample_kernel[i] = self->_ssao_data.noise_kernel[i];
        }
        consts.noise_scale = self->_ssao_data.noise_scale;
        consts.bias = self->_ssao_data.bias;
        consts.radius = self->_ssao_data.radius;

        const auto staging_buffer_offset =
            self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_per_frame_staging_buffer) +
            self->_global_resources.utilization.staging_buffer_bytes_written;
        auto staging_buffer_bytes = self->_device->map_buffer(
            self->_executor->get_buffer(self->_global_resources.graph_per_frame_staging_buffer));

        std::memcpy(staging_buffer_bytes + staging_buffer_offset, &consts, sizeof(ssao_constants));

        self->_device->unmap_buffer(
            self->_executor->get_buffer(self->_global_resources.graph_per_frame_staging_buffer));

        ctx.copy_buffer_to_buffer(self->_global_resources.graph_per_frame_staging_buffer,
                                  self->_pass_output_resource_handles.ssao.ssao_constants_buffer, staging_buffer_offset,
                                  self->_executor->get_current_frame_resource_offset(
                                      self->_pass_output_resource_handles.ssao.ssao_constants_buffer),
                                  sizeof(ssao_constants));

        self->_global_resources.utilization.staging_buffer_bytes_written += sizeof(ssao_constants);
    }

    void pbr_frame_graph::_ssao_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                          graph_resource_handle<rhi::rhi_handle_type::buffer> descriptors)
    {
        auto ssao_descriptors = rhi::descriptor_set_desc{};
        ssao_descriptors.layout = self->_pass_output_resource_handles.ssao.descriptor_layout;

        // Binding 0: Scene Constants
        // Binding 1: SSAO Constants
        // Binding 2: Depth Texture
        // Binding 3: Normal Texture
        // Binding 4: Noise Texture
        // Binding 5: Linear Sampler
        // Binding 6: Point Sampler

        ssao_descriptors.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .offset = static_cast<uint32_t>(self->_executor->get_current_frame_resource_offset(
                self->_pass_output_resource_handles.upload_pass.scene_constants)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.upload_pass.scene_constants)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.upload_pass.scene_constants),
        });

        ssao_descriptors.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 1,
            .type = rhi::descriptor_type::constant_buffer,
            .offset = static_cast<uint32_t>(self->_executor->get_current_frame_resource_offset(
                self->_pass_output_resource_handles.ssao.ssao_constants_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.ssao.ssao_constants_buffer)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.ssao.ssao_constants_buffer),
        });

        auto depth_image_bindings = vector<rhi::image_binding_info>{};
        depth_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.depth_prepass.depth),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::shader_read_only,
        });

        ssao_descriptors.images.push_back(rhi::image_binding_descriptor{
            .index = 2,
            .type = rhi::descriptor_type::sampled_image,
            .images = tempest::move(depth_image_bindings),
        });

        auto normal_image_bindings = vector<rhi::image_binding_info>{};
        normal_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.depth_prepass.encoded_normals),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::shader_read_only,
        });

        ssao_descriptors.images.push_back(rhi::image_binding_descriptor{
            .index = 3,
            .type = rhi::descriptor_type::sampled_image,
            .images = tempest::move(normal_image_bindings),
        });

        auto noise_image_bindings = vector<rhi::image_binding_info>{};
        noise_image_bindings.push_back(rhi::image_binding_info{
            .image = self->_pass_output_resource_handles.ssao.ssao_noise_image,
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::shader_read_only,
        });

        ssao_descriptors.images.push_back(rhi::image_binding_descriptor{
            .index = 4,
            .type = rhi::descriptor_type::sampled_image,
            .images = tempest::move(noise_image_bindings),
        });

        auto linear_samplers = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>>{};
        linear_samplers.push_back(self->_global_resources.linear_sampler);

        ssao_descriptors.samplers.push_back(rhi::sampler_binding_descriptor{
            .index = 5,
            .samplers = tempest::move(linear_samplers),
        });

        auto point_samplers = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>>{};
        point_samplers.push_back(self->_global_resources.point_sampler);

        ssao_descriptors.samplers.push_back(rhi::sampler_binding_descriptor{
            .index = 6,
            .samplers = tempest::move(point_samplers),
        });

        auto ssao_descriptor_buffer_bytes = self->_device->map_buffer(ctx.find_buffer(descriptors));
        self->_device->write_descriptor_buffer(ssao_descriptors, ssao_descriptor_buffer_bytes,
                                               self->_executor->get_current_frame_resource_offset(descriptors));
        self->_device->unmap_buffer(ctx.find_buffer(descriptors));

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
        ctx.bind_descriptor_buffers(self->_pass_output_resource_handles.ssao.pipeline_layout, rhi::bind_point::graphics,
                                    0, array{descriptors});

        ctx.bind_pipeline(self->_pass_output_resource_handles.ssao.pipeline);
        ctx.set_cull_mode(make_enum_mask(rhi::cull_mode::back));
        ctx.set_scissor(0, 0, self->_cfg.render_target_width, self->_cfg.render_target_height);
        ctx.set_viewport(0.0f, 0.0f, static_cast<float>(self->_cfg.render_target_width),
                         static_cast<float>(self->_cfg.render_target_height), 0.0f, 1.0f, false);

        ctx.draw(3);

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

        // 0: SSAO Texture
        // 1: Linear Sampler

        auto images = vector<rhi::image_binding_info>{};
        images.push_back({
            .image = ctx.find_image(self->_pass_output_resource_handles.ssao.ssao_output),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::shader_read_only,
        });

        auto image_bindings = array{
            rhi::image_binding_descriptor{
                .index = 0,
                .type = rhi::descriptor_type::sampled_image,
                .array_offset = 0,
                .images = tempest::move(images),
            },
        };

        auto samplers = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>>{};
        samplers.push_back(self->_global_resources.linear_sampler);

        auto sampler_bindings = array{
            rhi::sampler_binding_descriptor{
                .index = 1,
                .samplers = tempest::move(samplers),
            },
        };

        ctx.begin_render_pass(render_pass_begin);
        ctx.push_descriptors(self->_pass_output_resource_handles.ssao_blur.pipeline_layout, rhi::bind_point::graphics,
                             0, {}, image_bindings, sampler_bindings);
        ctx.bind_pipeline(self->_pass_output_resource_handles.ssao_blur.pipeline);
        ctx.set_cull_mode(make_enum_mask(rhi::cull_mode::back));
        ctx.set_scissor(0, 0, self->_cfg.render_target_width, self->_cfg.render_target_height);
        ctx.set_viewport(0.0f, 0.0f, static_cast<float>(self->_cfg.render_target_width),
                         static_cast<float>(self->_cfg.render_target_height), 0.0f, 1.0f, false);
        ctx.draw(3);
        ctx.end_render_pass();
    }

    void pbr_frame_graph::_light_clustering_pass_task(compute_task_execution_context& ctx, pbr_frame_graph* self)
    {
        const auto grid_ci = cluster_grid_create_info{
            .inv_proj = self->_scene_data.primary_camera.inv_proj,
            .screen_bounds =
                {
                    static_cast<float>(self->_cfg.render_target_width),
                    static_cast<float>(self->_cfg.render_target_height), 0.1f,
                    1000.0f, // TODO: Parameterize near and far planes
                },
            .workgroup_count_tile_size_px =
                {
                    self->_cfg.light_clustering.cluster_count_x,
                    self->_cfg.light_clustering.cluster_count_y,
                    self->_cfg.light_clustering.cluster_count_z,
                    self->_cfg.render_target_width / self->_cfg.light_clustering.cluster_count_x,
                },
        };

        auto binding_write = rhi::descriptor_set_desc{};
        binding_write.layout = self->_pass_output_resource_handles.light_clustering.descriptor_layout;
        binding_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 0,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(
                self->_pass_output_resource_handles.light_clustering.light_cluster_bounds)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.light_clustering.light_cluster_bounds),
        });

        ctx.push_descriptors(self->_pass_output_resource_handles.light_clustering.pipeline_layout,
                             rhi::bind_point::compute, 0, binding_write.buffers, {}, {});

        ctx.push_constants(self->_pass_output_resource_handles.light_clustering.pipeline_layout,
                           make_enum_mask(rhi::shader_stage::compute), 0, grid_ci);

        ctx.bind_pipeline(self->_pass_output_resource_handles.light_clustering.pipeline);

        ctx.dispatch(self->_cfg.light_clustering.cluster_count_x, self->_cfg.light_clustering.cluster_count_y,
                     self->_cfg.light_clustering.cluster_count_z);
    }

    void pbr_frame_graph::_light_culling_pass_task(compute_task_execution_context& ctx, pbr_frame_graph* self)
    {
        const auto culling_ci = light_culling_info{
            .inv_proj = self->_scene_data.primary_camera.inv_proj,
            .screen_bounds =
                {
                    static_cast<float>(self->_cfg.render_target_width),
                    static_cast<float>(self->_cfg.render_target_height), 0.0f,
                    1000.0f, // TODO: Parameterize near and far planes
                },
            .workgroup_count_tile_size_px =
                {
                    self->_cfg.light_clustering.cluster_count_x,
                    self->_cfg.light_clustering.cluster_count_y,
                    self->_cfg.light_clustering.cluster_count_z,
                    self->_cfg.render_target_width / self->_cfg.light_clustering.cluster_count_x,
                },
            .light_count = static_cast<uint32_t>(self->_scene_data.point_lights.size()),
        };

        // Binding 0: Scene Globals
        // Binding 1: Cluster Bounds
        // Binding 2: Lights
        // Binding 3: Light Index List
        // Binding 4: Light Grid
        // Binding 5: Light Counter

        auto binding_write = rhi::descriptor_set_desc{};
        binding_write.layout = self->_pass_output_resource_handles.light_culling.descriptor_layout;

        binding_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.upload_pass.scene_constants)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.upload_pass.scene_constants),
        });

        binding_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(
                self->_pass_output_resource_handles.light_clustering.light_cluster_bounds)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.light_clustering.light_cluster_bounds),
        });

        binding_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_light_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_light_buffer),
        });

        binding_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.light_culling.light_indices)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.light_culling.light_indices),
        });

        binding_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(
                self->_pass_output_resource_handles.light_culling.light_grid_ranges)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.light_culling.light_grid_ranges),
        });

        binding_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 5,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(
                self->_pass_output_resource_handles.light_culling.light_index_count)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.light_culling.light_index_count),
        });

        ctx.push_descriptors(self->_pass_output_resource_handles.light_culling.pipeline_layout,
                             rhi::bind_point::compute, 0, binding_write.buffers, {}, {});
        ctx.push_constants(self->_pass_output_resource_handles.light_culling.pipeline_layout,
                           make_enum_mask(rhi::shader_stage::compute), 0, culling_ci);
        ctx.bind_pipeline(self->_pass_output_resource_handles.light_culling.pipeline);
        ctx.dispatch(1, 1, self->_cfg.light_clustering.cluster_count_z / 4);
    }

    void pbr_frame_graph::_shadow_upload_pass_task(transfer_task_execution_context& ctx, pbr_frame_graph* self)
    {
        const auto staging_buffer_offset =
            self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_per_frame_staging_buffer) +
            self->_global_resources.utilization.staging_buffer_bytes_written;
        auto staging_buffer_bytes = self->_device->map_buffer(
            self->_executor->get_buffer(self->_global_resources.graph_per_frame_staging_buffer));

        std::memcpy(staging_buffer_bytes + staging_buffer_offset, self->_shadow_data.shadow_map_parameters.data(),
                    self->_shadow_data.shadow_map_parameters.size() * sizeof(shadow_map_parameter));

        self->_device->unmap_buffer(
            self->_executor->get_buffer(self->_global_resources.graph_per_frame_staging_buffer));

        self->_global_resources.utilization.staging_buffer_bytes_written +=
            static_cast<uint32_t>(self->_shadow_data.shadow_map_parameters.size() * sizeof(shadow_map_parameter));

        ctx.copy_buffer_to_buffer(self->_global_resources.graph_per_frame_staging_buffer,
                                  self->_pass_output_resource_handles.shadow_map.shadow_data, staging_buffer_offset,
                                  self->_executor->get_current_frame_resource_offset(
                                      self->_pass_output_resource_handles.shadow_map.shadow_data),
                                  self->_shadow_data.shadow_map_parameters.size() * sizeof(shadow_map_parameter));
    }

    void pbr_frame_graph::_shadow_map_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                                graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors)
    {
        auto render_pass_begin = rhi::work_queue::render_pass_info{};
        render_pass_begin.name = "Shadow Map Pass";
        render_pass_begin.width = self->_cfg.shadows.shadow_map_width;
        render_pass_begin.height = self->_cfg.shadows.shadow_map_height;
        render_pass_begin.layers = 1;
        render_pass_begin.depth_attachment = rhi::work_queue::depth_attachment_info{
            .image = self->_executor->get_image(self->_pass_output_resource_handles.shadow_map.shadow_map_megatexture),
            .layout = rhi::image_layout::depth,
            .clear_depth = 0.0f,
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::store,
        };

        // Scene Descriptors
        // Binding 1: Vertex Pull Buffer
        // Binding 2: Mesh Buffer
        // Binding 3: Object Buffer
        // Binding 4: Instance Buffer
        // Binding 5: Material Buffer
        // Binding 15: Linear Sampler
        // Binding 16: Bindless Textures

        auto scene_descriptor_write = rhi::descriptor_set_desc{};
        scene_descriptor_write.layout = self->_pass_output_resource_handles.shadow_map.scene_descriptor_layout;

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_vertex_pull_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_vertex_pull_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_vertex_pull_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_mesh_buffer)),
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_mesh_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_mesh_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_object_buffer)),
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_object_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_object_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_instance_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_instance_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_instance_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 5,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_material_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_material_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_material_buffer),
        });

        auto samplers = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>>{};
        samplers.push_back(self->_global_resources.linear_sampler);
        scene_descriptor_write.samplers.push_back(rhi::sampler_binding_descriptor{
            .index = 15,
            .samplers = move(samplers),
        });

        auto images = vector<rhi::image_binding_info>();
        const auto image_count =
            min(self->_cfg.max_bindless_textures, static_cast<uint32_t>(self->_bindless_textures.images.size()));

        for (uint32_t i = 0; i < image_count; i++)
        {
            images.push_back(rhi::image_binding_info{
                .image = self->_bindless_textures.images[i],
                .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
                .layout = rhi::image_layout::shader_read_only,
            });
        }
        scene_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 16,
            .type = rhi::descriptor_type::sampled_image,
            .images = move(images),
        });

        auto scene_descriptor_buffer_bytes = self->_device->map_buffer(ctx.find_buffer(scene_descriptors));
        self->_device->write_descriptor_buffer(scene_descriptor_write, scene_descriptor_buffer_bytes,
                                               self->_executor->get_current_frame_resource_offset(scene_descriptors));
        self->_device->unmap_buffer(ctx.find_buffer(scene_descriptors));

        ctx.begin_render_pass(render_pass_begin);

        ctx.bind_descriptor_buffers(self->_pass_output_resource_handles.shadow_map.directional_shadow_pipeline_layout,
                                    rhi::bind_point::graphics, 0, array{scene_descriptors});

        ctx.bind_pipeline(self->_pass_output_resource_handles.shadow_map.directional_shadow_pipeline);

        ctx.set_scissor(0, 0, self->_cfg.shadows.shadow_map_width, self->_cfg.shadows.shadow_map_height);
        ctx.set_viewport(0.0f, 0.0f, static_cast<float>(self->_cfg.shadows.shadow_map_width),
                         static_cast<float>(self->_cfg.shadows.shadow_map_height), 0.0f, 1.0f);
        ctx.set_cull_mode(make_enum_mask(rhi::cull_mode::back));
        ctx.bind_index_buffer(self->_global_resources.vertex_pull_buffer, rhi::index_format::uint32, 0);

        const auto draw_command_buffer = self->_pass_output_resource_handles.upload_pass.draw_commands;
        const auto draw_command_buffer_offset = self->_executor->get_current_frame_resource_offset(draw_command_buffer);

        self->_inputs.entity_registry->each([&]([[maybe_unused]] directional_light_component dir_light,
                                                [[maybe_unused]] shadow_map_component shadows,
                                                ecs::self_component self_entity) {
            const auto light_it = self->_scene_data.dir_lights.find(self_entity.entity);
            if (light_it == self->_scene_data.dir_lights.end())
            {
                return;
            }

            const auto& light = light_it->second;
            for (auto cascade_index = 0u; cascade_index < light.shadow_map_count; ++cascade_index)
            {
                auto shadow_map_index = light.shadow_map_indices[cascade_index];
                const auto& parameters = self->_shadow_data.shadow_map_parameters[shadow_map_index];

                // Reconstruct the viewport for this cascade
                const auto x = parameters.shadow_map_region.x * self->_shadow_data.shelf_pack->extent().x;
                const auto y = parameters.shadow_map_region.y * self->_shadow_data.shelf_pack->extent().y;
                const auto width = parameters.shadow_map_region.z * self->_shadow_data.shelf_pack->extent().x;
                const auto height = parameters.shadow_map_region.w * self->_shadow_data.shelf_pack->extent().y;

                ctx.set_scissor(static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(width),
                                static_cast<uint32_t>(height));

                ctx.set_viewport(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width),
                                 static_cast<float>(height), 0.0f, 1.0f, false);

                ctx.push_constants(self->_pass_output_resource_handles.shadow_map.directional_shadow_pipeline_layout,
                                   make_enum_mask(rhi::shader_stage::vertex, rhi::shader_stage::fragment), 0,
                                   parameters.light_proj_matrix);

                for (const auto& [key, draw_batch] : self->_drawables.draw_batches)
                {
                    if (key.alpha_type == alpha_behavior::opaque || key.alpha_type == alpha_behavior::mask)
                    {
                        ctx.draw_indirect(
                            draw_command_buffer,
                            static_cast<uint32_t>(draw_command_buffer_offset + draw_batch.indirect_command_offset *
                                                                                   sizeof(indexed_indirect_command)),
                            static_cast<uint32_t>(draw_batch.commands.size()), sizeof(indexed_indirect_command));
                    }
                }
            }
        });

        ctx.end_render_pass();
    }

    void pbr_frame_graph::_pbr_opaque_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self,
                                                graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors,
                                                graph_resource_handle<rhi::rhi_handle_type::buffer> shadow_descriptors)
    {
        auto render_pass_begin = rhi::work_queue::render_pass_info{};
        render_pass_begin.name = "PBR Opaque Pass";
        render_pass_begin.width = self->_cfg.render_target_width;
        render_pass_begin.height = self->_cfg.render_target_height;
        render_pass_begin.layers = 1;
        render_pass_begin.depth_attachment = rhi::work_queue::depth_attachment_info{
            .image = self->_executor->get_image(self->_pass_output_resource_handles.depth_prepass.depth),
            .layout = rhi::image_layout::depth,
            .clear_depth = 0.0f, // IGNORED
            .load_op = rhi::work_queue::load_op::load,
            .store_op = rhi::work_queue::store_op::none,
        };

        render_pass_begin.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image = self->_executor->get_image(self->_pass_output_resource_handles.pbr_opaque.hdr_color),
            .layout = rhi::image_layout::color_attachment,
            .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::store,
        });

        // Scene Descriptors
        // Binding 0: Scene Constants
        // Binding 1: Vertex Pull Buffer
        // Binding 2: Mesh Buffer
        // Binding 3: Object Buffer
        // Binding 4: Instance Buffer
        // Binding 5: Material Buffer
        // Binding 6: Ambient Occlusion Texture
        // Binding 15: Linear Sampler
        // Binding 16: Bindless Textures

        // Light and Shadow Descriptors
        // Binding 0: Light Buffer
        // Binding 1: Shadow Matrix Buffer
        // Binding 2: Shadow Map Megatexture
        // Binding 3: Light Grid Bounds
        // Binding 4: Light Indices

        auto scene_descriptor_write = rhi::descriptor_set_desc{};
        scene_descriptor_write.layout = self->_pass_output_resource_handles.pbr_opaque.scene_descriptor_layout;

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .offset = static_cast<uint32_t>(self->_executor->get_current_frame_resource_offset(
                self->_pass_output_resource_handles.upload_pass.scene_constants)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.upload_pass.scene_constants)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.upload_pass.scene_constants),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_vertex_pull_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_vertex_pull_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_vertex_pull_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_mesh_buffer)),
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_mesh_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_mesh_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_object_buffer)),
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_object_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_object_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_instance_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_instance_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_instance_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 5,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_material_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_material_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_material_buffer),
        });

        auto ambient_occlusion_image_bindings = vector<rhi::image_binding_info>{};
        ambient_occlusion_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.ssao_blur.ssao_blurred_output),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::shader_read_only,
        });

        scene_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 6,
            .type = rhi::descriptor_type::sampled_image,
            .images = tempest::move(ambient_occlusion_image_bindings),
        });

        auto linear_samplers = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>>{};
        linear_samplers.push_back(self->_global_resources.linear_sampler);

        scene_descriptor_write.samplers.push_back(rhi::sampler_binding_descriptor{
            .index = 15,
            .samplers = tempest::move(linear_samplers),
        });

        auto bindless_images = vector<rhi::image_binding_info>();
        const auto image_count =
            min(self->_cfg.max_bindless_textures, static_cast<uint32_t>(self->_bindless_textures.images.size()));
        for (uint32_t i = 0; i < image_count; i++)
        {
            bindless_images.push_back(rhi::image_binding_info{
                .image = self->_bindless_textures.images[i],
                .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
                .layout = rhi::image_layout::shader_read_only,
            });
        }

        scene_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 16,
            .type = rhi::descriptor_type::sampled_image,
            .images = tempest::move(bindless_images),
        });

        auto scene_descriptor_buffer_bytes = self->_device->map_buffer(ctx.find_buffer(scene_descriptors));
        self->_device->write_descriptor_buffer(scene_descriptor_write, scene_descriptor_buffer_bytes,
                                               self->_executor->get_current_frame_resource_offset(scene_descriptors));
        self->_device->unmap_buffer(ctx.find_buffer(scene_descriptors));

        auto shadow_light_descriptor_write = rhi::descriptor_set_desc{};
        shadow_light_descriptor_write.layout =
            self->_pass_output_resource_handles.pbr_opaque.shadow_and_lighting_descriptor_layout;
        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 0,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_light_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_light_buffer),
        });

        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.shadow_map.shadow_data)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.shadow_map.shadow_data),
        });

        auto shadow_map_image_bindings = vector<rhi::image_binding_info>{};
        shadow_map_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.shadow_map.shadow_map_megatexture),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::shader_read_only,
        });

        shadow_light_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 2,
            .type = rhi::descriptor_type::sampled_image,
            .array_offset = 0,
            .images = tempest::move(shadow_map_image_bindings),
        });

        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(
                self->_pass_output_resource_handles.light_culling.light_grid_ranges)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.light_culling.light_grid_ranges),
        });

        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.light_culling.light_indices)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.light_culling.light_indices),
        });

        auto shadow_descriptor_buffer_bytes = self->_device->map_buffer(ctx.find_buffer(shadow_descriptors));
        self->_device->write_descriptor_buffer(shadow_light_descriptor_write, shadow_descriptor_buffer_bytes,
                                               self->_executor->get_current_frame_resource_offset(shadow_descriptors));
        self->_device->unmap_buffer(ctx.find_buffer(shadow_descriptors));

        ctx.begin_render_pass(render_pass_begin);

        ctx.bind_descriptor_buffers(self->_pass_output_resource_handles.pbr_opaque.pipeline_layout,
                                    rhi::bind_point::graphics, 0, array{scene_descriptors, shadow_descriptors});

        ctx.bind_pipeline(self->_pass_output_resource_handles.pbr_opaque.pipeline);
        ctx.bind_index_buffer(self->_global_resources.vertex_pull_buffer, rhi::index_format::uint32, 0);
        ctx.set_cull_mode(make_enum_mask(rhi::cull_mode::back));
        ctx.set_scissor(0, 0, self->_cfg.render_target_width, self->_cfg.render_target_height);
        ctx.set_viewport(0.0f, 0.0f, static_cast<float>(self->_cfg.render_target_width),
                         static_cast<float>(self->_cfg.render_target_height), 0.0f, 1.0f);

        const auto indirect_command_offset = self->_executor->get_current_frame_resource_offset(
            self->_pass_output_resource_handles.upload_pass.draw_commands);

        for (const auto& [key, batch] : self->_drawables.draw_batches)
        {
            if (key.alpha_type == alpha_behavior::opaque || key.alpha_type == alpha_behavior::mask)
            {
                ctx.draw_indirect(self->_pass_output_resource_handles.upload_pass.draw_commands,
                                  static_cast<uint32_t>(indirect_command_offset + batch.indirect_command_offset *
                                                                                      sizeof(indexed_indirect_command)),
                                  static_cast<uint32_t>(batch.commands.size()),
                                  static_cast<uint32_t>(sizeof(indexed_indirect_command)));
            }
        }

        ctx.end_render_pass();
    }

    void pbr_frame_graph::_mboit_gather_pass_task(
        graphics_task_execution_context& ctx, pbr_frame_graph* self,
        graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors,
        graph_resource_handle<rhi::rhi_handle_type::buffer> shadow_descriptors)
    {
        auto render_pass_begin = rhi::work_queue::render_pass_info{};
        render_pass_begin.name = "MBOIT Gather Pass";
        render_pass_begin.width = self->_cfg.render_target_width;
        render_pass_begin.height = self->_cfg.render_target_height;
        render_pass_begin.layers = 1;
        render_pass_begin.depth_attachment = rhi::work_queue::depth_attachment_info{
            .image = self->_executor->get_image(self->_pass_output_resource_handles.depth_prepass.depth),
            .layout = rhi::image_layout::depth_stencil_read_only,
            .clear_depth = 0.0f,
            .load_op = rhi::work_queue::load_op::load,
            .store_op = rhi::work_queue::store_op::none,
        };

        render_pass_begin.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image =
                self->_executor->get_image(self->_pass_output_resource_handles.mboit_gather.transparency_accumulation),
            .layout = rhi::image_layout::color_attachment,
            .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::dont_care,
        });

        auto scene_descriptor_write = rhi::descriptor_set_desc{};
        scene_descriptor_write.layout = self->_pass_output_resource_handles.pbr_opaque.scene_descriptor_layout;

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .offset = static_cast<uint32_t>(self->_executor->get_current_frame_resource_offset(
                self->_pass_output_resource_handles.upload_pass.scene_constants)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.upload_pass.scene_constants)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.upload_pass.scene_constants),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_vertex_pull_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_vertex_pull_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_vertex_pull_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_mesh_buffer)),
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_mesh_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_mesh_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_object_buffer)),
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_object_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_object_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_instance_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_instance_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_instance_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 5,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_material_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_material_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_material_buffer),
        });

        auto moments_image_bindings = vector<rhi::image_binding_info>{};
        moments_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.ssao_blur.ssao_blurred_output),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::general,
        });

        scene_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 6,
            .type = rhi::descriptor_type::storage_image,
            .images = tempest::move(moments_image_bindings),
        });

        auto zeroth_moment_image_bindings = vector<rhi::image_binding_info>{};
        zeroth_moment_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.ssao_blur.ssao_blurred_output),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::general,
        });

        scene_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 7,
            .type = rhi::descriptor_type::storage_image,
            .images = tempest::move(zeroth_moment_image_bindings),
        });

        auto ambient_occlusion_image_bindings = vector<rhi::image_binding_info>{};
        ambient_occlusion_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.ssao_blur.ssao_blurred_output),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::shader_read_only,
        });

        scene_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 8,
            .type = rhi::descriptor_type::sampled_image,
            .images = tempest::move(ambient_occlusion_image_bindings),
        });

        auto linear_samplers = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>>{};
        linear_samplers.push_back(self->_global_resources.linear_sampler);

        scene_descriptor_write.samplers.push_back(rhi::sampler_binding_descriptor{
            .index = 15,
            .samplers = tempest::move(linear_samplers),
        });

        auto bindless_images = vector<rhi::image_binding_info>();
        const auto image_count =
            min(self->_cfg.max_bindless_textures, static_cast<uint32_t>(self->_bindless_textures.images.size()));
        for (uint32_t i = 0; i < image_count; i++)
        {
            bindless_images.push_back(rhi::image_binding_info{
                .image = self->_bindless_textures.images[i],
                .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
                .layout = rhi::image_layout::shader_read_only,
            });
        }

        scene_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 16,
            .type = rhi::descriptor_type::sampled_image,
            .images = tempest::move(bindless_images),
        });

        auto scene_descriptor_buffer_bytes = self->_device->map_buffer(ctx.find_buffer(scene_descriptors));
        self->_device->write_descriptor_buffer(scene_descriptor_write, scene_descriptor_buffer_bytes,
                                               self->_executor->get_current_frame_resource_offset(scene_descriptors));
        self->_device->unmap_buffer(ctx.find_buffer(scene_descriptors));

        auto shadow_light_descriptor_write = rhi::descriptor_set_desc{};
        shadow_light_descriptor_write.layout =
            self->_pass_output_resource_handles.pbr_opaque.shadow_and_lighting_descriptor_layout;
        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 0,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_light_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_light_buffer),
        });

        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.shadow_map.shadow_data)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.shadow_map.shadow_data),
        });

        auto shadow_map_image_bindings = vector<rhi::image_binding_info>{};
        shadow_map_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.shadow_map.shadow_map_megatexture),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::shader_read_only,
        });

        shadow_light_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 2,
            .type = rhi::descriptor_type::sampled_image,
            .array_offset = 0,
            .images = tempest::move(shadow_map_image_bindings),
        });

        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(
                self->_pass_output_resource_handles.light_culling.light_grid_ranges)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.light_culling.light_grid_ranges),
        });

        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.light_culling.light_indices)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.light_culling.light_indices),
        });

        auto shadow_descriptor_buffer_bytes = self->_device->map_buffer(ctx.find_buffer(shadow_descriptors));
        self->_device->write_descriptor_buffer(shadow_light_descriptor_write, shadow_descriptor_buffer_bytes,
                                               self->_executor->get_current_frame_resource_offset(shadow_descriptors));
        self->_device->unmap_buffer(ctx.find_buffer(shadow_descriptors));

        ctx.begin_render_pass(render_pass_begin);

        ctx.bind_descriptor_buffers(self->_pass_output_resource_handles.mboit_gather.pipeline_layout,
                                    rhi::bind_point::graphics, 0, array{scene_descriptors, shadow_descriptors});

        ctx.bind_pipeline(self->_pass_output_resource_handles.mboit_gather.pipeline);
        ctx.bind_index_buffer(self->_global_resources.vertex_pull_buffer, rhi::index_format::uint32, 0);
        ctx.set_cull_mode(make_enum_mask(rhi::cull_mode::back));
        ctx.set_scissor(0, 0, self->_cfg.render_target_width, self->_cfg.render_target_height);
        ctx.set_viewport(0.0f, 0.0f, static_cast<float>(self->_cfg.render_target_width),
                         static_cast<float>(self->_cfg.render_target_height), 0.0f, 1.0f);

        const auto indirect_command_offset = self->_executor->get_current_frame_resource_offset(
            self->_pass_output_resource_handles.upload_pass.draw_commands);

        for (const auto& [key, batch] : self->_drawables.draw_batches)
        {
            if (key.alpha_type == alpha_behavior::transmissive || key.alpha_type == alpha_behavior::transparent)
            {
                ctx.draw_indirect(self->_pass_output_resource_handles.upload_pass.draw_commands,
                                  static_cast<uint32_t>(indirect_command_offset + batch.indirect_command_offset *
                                                                                      sizeof(indexed_indirect_command)),
                                  static_cast<uint32_t>(batch.commands.size()),
                                  static_cast<uint32_t>(sizeof(indexed_indirect_command)));
            }
        }

        ctx.end_render_pass();
    }

    void pbr_frame_graph::_mboit_resolve_pass_task(
        graphics_task_execution_context& ctx, pbr_frame_graph* self,
        graph_resource_handle<rhi::rhi_handle_type::buffer> scene_descriptors,
        graph_resource_handle<rhi::rhi_handle_type::buffer> shadow_descriptors)
    {
        auto render_pass_begin = rhi::work_queue::render_pass_info{};
        render_pass_begin.name = "MBOIT Resolve Pass";
        render_pass_begin.width = self->_cfg.render_target_width;
        render_pass_begin.height = self->_cfg.render_target_height;
        render_pass_begin.layers = 1;
        render_pass_begin.depth_attachment = rhi::work_queue::depth_attachment_info{
            .image = self->_executor->get_image(self->_pass_output_resource_handles.depth_prepass.depth),
            .layout = rhi::image_layout::depth_stencil_read_only,
            .clear_depth = 0.0f,
            .load_op = rhi::work_queue::load_op::load,
            .store_op = rhi::work_queue::store_op::none,
        };

        render_pass_begin.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image =
                self->_executor->get_image(self->_pass_output_resource_handles.mboit_resolve.transparency_accumulation),
            .layout = rhi::image_layout::color_attachment,
            .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::store,
        });

        auto scene_descriptor_write = rhi::descriptor_set_desc{};
        scene_descriptor_write.layout = self->_pass_output_resource_handles.pbr_opaque.scene_descriptor_layout;

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 0,
            .type = rhi::descriptor_type::constant_buffer,
            .offset = static_cast<uint32_t>(self->_executor->get_current_frame_resource_offset(
                self->_pass_output_resource_handles.upload_pass.scene_constants)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.upload_pass.scene_constants)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.upload_pass.scene_constants),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_vertex_pull_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_vertex_pull_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_vertex_pull_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 2,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_mesh_buffer)),
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_mesh_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_mesh_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_object_buffer)),
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_object_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_object_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_instance_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_instance_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_instance_buffer),
        });

        scene_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 5,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = static_cast<uint32_t>(
                self->_executor->get_current_frame_resource_offset(self->_global_resources.graph_material_buffer)),
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_global_resources.graph_material_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_material_buffer),
        });

        auto moments_image_bindings = vector<rhi::image_binding_info>{};
        moments_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.ssao_blur.ssao_blurred_output),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::general,
        });

        scene_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 6,
            .type = rhi::descriptor_type::storage_image,
            .images = tempest::move(moments_image_bindings),
        });

        auto zeroth_moment_image_bindings = vector<rhi::image_binding_info>{};
        zeroth_moment_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.ssao_blur.ssao_blurred_output),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::general,
        });

        scene_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 7,
            .type = rhi::descriptor_type::storage_image,
            .images = tempest::move(zeroth_moment_image_bindings),
        });

        auto ambient_occlusion_image_bindings = vector<rhi::image_binding_info>{};
        ambient_occlusion_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.ssao_blur.ssao_blurred_output),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::shader_read_only,
        });

        scene_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 8,
            .type = rhi::descriptor_type::sampled_image,
            .images = tempest::move(ambient_occlusion_image_bindings),
        });

        auto linear_samplers = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>>{};
        linear_samplers.push_back(self->_global_resources.linear_sampler);

        scene_descriptor_write.samplers.push_back(rhi::sampler_binding_descriptor{
            .index = 15,
            .samplers = tempest::move(linear_samplers),
        });

        auto bindless_images = vector<rhi::image_binding_info>();
        const auto image_count =
            min(self->_cfg.max_bindless_textures, static_cast<uint32_t>(self->_bindless_textures.images.size()));
        for (uint32_t i = 0; i < image_count; i++)
        {
            bindless_images.push_back(rhi::image_binding_info{
                .image = self->_bindless_textures.images[i],
                .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
                .layout = rhi::image_layout::shader_read_only,
            });
        }

        scene_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 16,
            .type = rhi::descriptor_type::sampled_image,
            .images = tempest::move(bindless_images),
        });

        auto scene_descriptor_buffer_bytes = self->_device->map_buffer(ctx.find_buffer(scene_descriptors));
        self->_device->write_descriptor_buffer(scene_descriptor_write, scene_descriptor_buffer_bytes,
                                               self->_executor->get_current_frame_resource_offset(scene_descriptors));
        self->_device->unmap_buffer(ctx.find_buffer(scene_descriptors));

        auto shadow_light_descriptor_write = rhi::descriptor_set_desc{};
        shadow_light_descriptor_write.layout =
            self->_pass_output_resource_handles.pbr_opaque.shadow_and_lighting_descriptor_layout;
        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 0,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size =
                static_cast<uint32_t>(self->_executor->get_resource_size(self->_global_resources.graph_light_buffer)),
            .buffer = ctx.find_buffer(self->_global_resources.graph_light_buffer),
        });

        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 1,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.shadow_map.shadow_data)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.shadow_map.shadow_data),
        });

        auto shadow_map_image_bindings = vector<rhi::image_binding_info>{};
        shadow_map_image_bindings.push_back(rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.shadow_map.shadow_map_megatexture),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::shader_read_only,
        });

        shadow_light_descriptor_write.images.push_back(rhi::image_binding_descriptor{
            .index = 2,
            .type = rhi::descriptor_type::sampled_image,
            .array_offset = 0,
            .images = tempest::move(shadow_map_image_bindings),
        });

        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 3,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(self->_executor->get_resource_size(
                self->_pass_output_resource_handles.light_culling.light_grid_ranges)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.light_culling.light_grid_ranges),
        });

        shadow_light_descriptor_write.buffers.push_back(rhi::buffer_binding_descriptor{
            .index = 4,
            .type = rhi::descriptor_type::structured_buffer,
            .offset = 0,
            .size = static_cast<uint32_t>(
                self->_executor->get_resource_size(self->_pass_output_resource_handles.light_culling.light_indices)),
            .buffer = ctx.find_buffer(self->_pass_output_resource_handles.light_culling.light_indices),
        });

        auto shadow_descriptor_buffer_bytes = self->_device->map_buffer(ctx.find_buffer(shadow_descriptors));
        self->_device->write_descriptor_buffer(shadow_light_descriptor_write, shadow_descriptor_buffer_bytes,
                                               self->_executor->get_current_frame_resource_offset(shadow_descriptors));
        self->_device->unmap_buffer(ctx.find_buffer(shadow_descriptors));

        ctx.begin_render_pass(render_pass_begin);
        ctx.end_render_pass();
    }

    void pbr_frame_graph::_mboit_blend_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self)
    {
        auto render_pass_begin = rhi::work_queue::render_pass_info{};
        render_pass_begin.name = "MBOIT Blend Pass";
        render_pass_begin.width = self->_cfg.render_target_width;
        render_pass_begin.height = self->_cfg.render_target_height;
        render_pass_begin.layers = 1;
        render_pass_begin.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image = self->_executor->get_image(self->_pass_output_resource_handles.pbr_opaque.hdr_color),
            .layout = rhi::image_layout::color_attachment,
            .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
            .load_op = rhi::work_queue::load_op::load,
            .store_op = rhi::work_queue::store_op::store,
        });

        ctx.begin_render_pass(render_pass_begin);
        ctx.end_render_pass();
    }

    void pbr_frame_graph::_tonemapping_pass_task(graphics_task_execution_context& ctx, pbr_frame_graph* self)
    {
        auto render_pass_begin = rhi::work_queue::render_pass_info{};
        render_pass_begin.name = "Tonemapping Pass";
        render_pass_begin.width = self->_cfg.render_target_width;
        render_pass_begin.height = self->_cfg.render_target_height;
        render_pass_begin.layers = 1;
        render_pass_begin.color_attachments.push_back(rhi::work_queue::color_attachment_info{
            .image = self->_executor->get_image(self->_pass_output_resource_handles.tonemapping.tonemapped_color),
            .layout = rhi::image_layout::color_attachment,
            .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
            .load_op = rhi::work_queue::load_op::clear,
            .store_op = rhi::work_queue::store_op::store,
        });

        ctx.begin_render_pass(render_pass_begin);

        auto hdr_color_image = rhi::image_binding_info{
            .image = ctx.find_image(self->_pass_output_resource_handles.pbr_opaque.hdr_color),
            .sampler = rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>::null_handle,
            .layout = rhi::image_layout::shader_read_only,
        };
        auto hdr_color_images = vector<rhi::image_binding_info>{};
        hdr_color_images.push_back(hdr_color_image);

        auto image_writes = vector<rhi::image_binding_descriptor>{};
        auto image_binding_desc = rhi::image_binding_descriptor{
            .index = 0,
            .type = rhi::descriptor_type::sampled_image,
            .array_offset = 0,
            .images = tempest::move(hdr_color_images),
        };
        image_writes.push_back(image_binding_desc);

        auto samplers = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler>>{};
        samplers.push_back(self->_global_resources.linear_sampler);

        auto sampler_writes = vector<rhi::sampler_binding_descriptor>{};
        auto sampler_binding_desc = rhi::sampler_binding_descriptor{
            .index = 1,
            .samplers = tempest::move(samplers),
        };
        sampler_writes.push_back(sampler_binding_desc);

        ctx.bind_pipeline(self->_pass_output_resource_handles.tonemapping.pipeline);
        ctx.push_descriptors(self->_pass_output_resource_handles.tonemapping.pipeline_layout, rhi::bind_point::graphics,
                             0, {}, image_writes, sampler_writes);
        ctx.set_scissor(0, 0, self->_cfg.render_target_width, self->_cfg.render_target_height);
        ctx.set_viewport(0.0f, 0.0f, static_cast<float>(self->_cfg.render_target_width),
                         static_cast<float>(self->_cfg.render_target_height), 0.0f, 1.0f, false);
        ctx.set_cull_mode(make_enum_mask(rhi::cull_mode::none));
        ctx.draw(3, 1, 0, 0);

        ctx.end_render_pass();
    }

    pbr_frame_graph::shadow_map_cascade_info pbr_frame_graph::_calculate_shadow_map_cascades(
        const shadow_map_component& shadows, const ecs::transform_component& light_transform,
        const camera_component& camera_data, const math::mat4<float>& view_matrix)
    {
        const auto near_plane = camera_data.near_plane;
        const auto far_plane = camera_data.far_shadow_plane;
        const auto clip_range = far_plane - near_plane;

        const auto clip_ratio = far_plane / clip_range;

        shadow_map_cascade_info results;
        results.cascade_distances.resize(shadows.cascade_count);
        results.frustum_view_projections.resize(shadows.cascade_count);

        // Compute splits
        // https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
        for (size_t i = 0; i < shadows.cascade_count; ++i)
        {
            const auto p = static_cast<float>(i + 1) / static_cast<float>(shadows.cascade_count);
            const auto logarithm = near_plane * std::pow(clip_ratio, p);
            const auto uniform = near_plane + clip_range * p;
            const auto d = 0.95f * (logarithm - uniform) + uniform;

            results.cascade_distances[i] = (d - near_plane) / clip_range;
        }

        const auto projection_with_clip = math::perspective(camera_data.aspect_ratio, camera_data.vertical_fov,
                                                            camera_data.near_plane, camera_data.far_shadow_plane);
        const auto inv_view_proj = math::inverse(projection_with_clip * view_matrix);

        auto last_split = 0.0f;
        for (uint32_t cascade = 0; cascade < shadows.cascade_count; ++cascade)
        {
            array frustum_corners = {
                math::vec3<float>{-1.0f, 1.0f, 0.0f}, math::vec3<float>{1.0f, 1.0f, 0.0f},
                math::vec3<float>{1.0f, -1.0f, 0.0f}, math::vec3<float>{-1.0f, -1.0f, 0.0f},
                math::vec3<float>{-1.0f, 1.0f, 1.0f}, math::vec3<float>{1.0f, 1.0f, 1.0f},
                math::vec3<float>{1.0f, -1.0f, 1.0f}, math::vec3<float>{-1.0f, -1.0f, 1.0f},
            };

            for (auto& corner : frustum_corners)
            {
                auto inv_corner = inv_view_proj * math::vec4<float>(corner.x, corner.y, corner.z, 1.0f);
                auto normalized = inv_corner / inv_corner.w;
                corner = {normalized.x, normalized.y, normalized.z};
            }

            const auto split_distance = results.cascade_distances[cascade];

            for (auto idx = 0; idx < 4; ++idx)
            {
                const auto edge = frustum_corners[idx + 4] - frustum_corners[idx];
                const auto normalized_far = frustum_corners[idx] + edge * split_distance;
                const auto normalized_near = frustum_corners[idx] + edge * last_split;

                frustum_corners[idx + 4] = normalized_far;
                frustum_corners[idx] = normalized_near;
            }

            auto frustum_center = math::vec3<float>(0.0f);
            for (const auto& corner : frustum_corners)
            {
                frustum_center += corner;
            }
            frustum_center /= static_cast<float>(8);

            float radius = 0.0f;
            for (const auto& corner : frustum_corners)
            {
                const auto distance = math::norm(corner - frustum_center);
                radius = tempest::max(radius, distance);
            }
            radius = std::ceil(radius * 16.0f) / 16.0f;

            auto max_extents = math::vec3<float>(radius);
            auto min_extents = -max_extents;

            const auto light_rotation = math::rotate(light_transform.rotation());
            const auto light_direction_xyzw = light_rotation * math::vec4(0.0f, 0.0f, 1.0f, 0.0f);
            const auto light_direction =
                math::vec3(light_direction_xyzw.x, light_direction_xyzw.y, light_direction_xyzw.z);

            const auto light_view =
                math::look_at(frustum_center - light_direction * radius, frustum_center, math::vec3(0.0f, 1.0f, 0.0f));
            const auto light_projection = math::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y,
                                                      min_extents.z - max_extents.z, 0.0f);

            results.cascade_distances[cascade] = (near_plane + split_distance * clip_range) * -1.0f;
            results.frustum_view_projections[cascade] = light_projection * light_view;

            last_split = results.cascade_distances[cascade];
        }

        return results;
    }

    void pbr_frame_graph::_load_meshes(span<const guid> mesh_ids, const core::mesh_registry& mesh_registry)
    {
        auto result = flat_unordered_map<guid, mesh_layout>{};

        auto bytes_written = 0u;
        auto vertex_bytes_required = 0u;
        auto layout_bytes_required = 0u;

        for (const auto& mesh_id : mesh_ids)
        {
            auto mesh_opt = mesh_registry.find(mesh_id);
            assert(mesh_opt.has_value());

            auto& mesh = *mesh_opt;

            // Compute vertex size in bytes
            auto vertex_size = sizeof(float) * 3    // position
                               + sizeof(float) * 3  // normal
                               + sizeof(float) * 2  // uv
                               + sizeof(float) * 4; // tangent
            if (mesh.has_colors)
            {
                vertex_size += sizeof(float) * 4; // color
            }

            vertex_bytes_required +=
                static_cast<uint32_t>(vertex_size * mesh.vertices.size() + sizeof(uint32_t) * mesh.indices.size());
            layout_bytes_required += static_cast<uint32_t>(sizeof(mesh_layout));
        }

        const auto total_bytes_required = vertex_bytes_required + layout_bytes_required;

        auto staging = _device->create_buffer({
            .size = total_bytes_required,
            .location = rhi::memory_location::host,
            .usage = make_enum_mask(rhi::buffer_usage::transfer_src),
            .access_type = rhi::host_access_type::incoherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "Staging Buffer",
        });

        auto dst = _device->map_buffer(staging);

        for (const auto& mesh_id : mesh_ids)
        {
            optional<const core::mesh&> mesh_opt = mesh_registry.find(mesh_id);
            const auto& mesh = *mesh_opt;

            // Region 0
            // - Positions (3 floats)
            // Region 1
            // - Normals (3 floats)
            // - UVs (2 floats)
            // - Tangents (3 floats)
            // - Colors (4 floats, optional)

            mesh_layout layout = {
                .mesh_start_offset = static_cast<uint32_t>(bytes_written),
                .positions_offset = 0,
                .interleave_offset = 3 * static_cast<uint32_t>(sizeof(float) * mesh.vertices.size()),
                .interleave_stride = 0,
                .uvs_offset = 0,
                .normals_offset = static_cast<uint32_t>(2 * sizeof(float)),
                .tangents_offset = static_cast<uint32_t>(5 * sizeof(float)),
                .index_offset = 0,
                .index_count = 0,
            };

            auto last_offset = 9 * sizeof(float);

            if (mesh.has_colors)
            {
                layout.color_offset = static_cast<uint32_t>(last_offset);
                last_offset += sizeof(float) * 4;
            }

            layout.interleave_stride = static_cast<uint32_t>(last_offset);
            layout.index_offset =
                layout.interleave_offset + layout.interleave_stride * static_cast<uint32_t>(mesh.vertices.size());
            layout.index_count = static_cast<uint32_t>(mesh.indices.size());

            result[mesh_id] = layout;

            // Position attribute
            size_t vertices_written = 0;
            for (const auto& vertex : mesh.vertices)
            {
                std::memcpy(dst + bytes_written + vertices_written * 3 * sizeof(float), &vertex.position,
                            sizeof(float) * 3);

                ++vertices_written;
            }

            bytes_written += layout.interleave_offset;

            // Interleaved, non-position attributes
            vertices_written = 0;
            for (const auto& vertex : mesh.vertices)
            {
                std::memcpy(dst + bytes_written + layout.uvs_offset + vertices_written * layout.interleave_stride,
                            &vertex.uv, 2 * sizeof(float));
                std::memcpy(dst + bytes_written + layout.normals_offset + vertices_written * layout.interleave_stride,
                            &vertex.normal, 3 * sizeof(float));
                std::memcpy(dst + bytes_written + layout.tangents_offset + vertices_written * layout.interleave_stride,
                            &vertex.tangent, 3 * sizeof(float));

                if (mesh.has_colors)
                {
                    std::memcpy(dst + bytes_written + layout.color_offset + vertices_written * layout.interleave_stride,
                                &vertex.color, 4 * sizeof(float));
                }

                ++vertices_written;
            }

            bytes_written += layout.interleave_stride * static_cast<uint32_t>(mesh.vertices.size());

            // Indices
            std::memcpy(dst + bytes_written, mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size());

            bytes_written += static_cast<uint32_t>(sizeof(uint32_t) * mesh.indices.size());
        }

        // Write the layouts
        for (auto&& [guid, layout] : result)
        {
            std::memcpy(dst + bytes_written, &layout, sizeof(mesh_layout));
            bytes_written += static_cast<uint32_t>(sizeof(mesh_layout));

            _meshes.mesh_to_index.insert({guid, _meshes.meshes.size()});
            _meshes.meshes.push_back(layout);
        }

        // Flush the staging buffer
        _device->unmap_buffer(staging);
        _device->flush_buffers(span(&staging, 1));

        // Upload the staging buffer to the GPU
        auto& work_queue = _device->get_primary_work_queue();
        auto cmd_buf = work_queue.get_next_command_list();

        work_queue.begin_command_list(cmd_buf, true);
        work_queue.copy(cmd_buf, staging, _global_resources.vertex_pull_buffer, 0,
                        _global_resources.utilization.vertex_bytes_written, vertex_bytes_required);
        work_queue.copy(cmd_buf, staging, _global_resources.mesh_buffer, vertex_bytes_required,
                        _global_resources.utilization.mesh_layout_bytes_written, layout_bytes_required);
        work_queue.end_command_list(cmd_buf);

        rhi::work_queue::submit_info submit_info;
        submit_info.command_lists.push_back(cmd_buf);

        // Get a fence for the copy operation
        auto complete_fence = _device->create_fence({
            .signaled = false,
        });

        // Submit
        work_queue.submit(span(&submit_info, 1), complete_fence);

        // Wait for the copy operation to complete
        _device->wait(span(&complete_fence, 1));

        // Clean up the resources
        _device->destroy_buffer(staging);
        _device->destroy_fence(complete_fence);

        _global_resources.utilization.vertex_bytes_written += total_bytes_required;
        _global_resources.utilization.mesh_layout_bytes_written += layout_bytes_required;
    }

    namespace
    {
        rhi::image_format convert_format(core::texture_format fmt)
        {
            switch (fmt)
            {
            case core::texture_format::rgba8_srgb:
                return rhi::image_format::rgba8_srgb;
            case core::texture_format::rgba8_unorm:
                return rhi::image_format::rgba8_unorm;
            case core::texture_format::rgba16_unorm:
                return rhi::image_format::rgba16_unorm;
            case core::texture_format::rgba32_float:
                return rhi::image_format::rgba32_float;
            default:
                log->error("Unsupported texture format");
            }

            unreachable();
        }
    } // namespace

    void pbr_frame_graph::_load_textures(span<const guid> texture_ids, const core::texture_registry& texture_registry,
                                         bool generate_mip_maps)
    {
        // Ensure we aren't uploading existing textures
        vector<guid> next_texture_ids;
        for (const auto& tex_guid : texture_ids)
        {
            if (_bindless_textures.image_to_index.find(tex_guid) != _bindless_textures.image_to_index.end() ||
                tempest::find(next_texture_ids.begin(), next_texture_ids.end(), tex_guid) != next_texture_ids.end())
            {
                continue;
            }
            next_texture_ids.push_back(tex_guid);
        }

        // Create the images
        vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::image>> images;

        for (const auto& tex_guid : next_texture_ids)
        {
            auto texture_opt = texture_registry.get_texture(tex_guid);
            assert(texture_opt.has_value());

            const auto& texture = *texture_opt;
            const auto mip_count = generate_mip_maps ? bit_width(min(texture.width, texture.height))
                                                     : static_cast<std::uint32_t>(texture.mips.size());

            const auto image_desc = rhi::image_desc{
                .format = convert_format(texture.format),
                .type = rhi::image_type::image_2d,
                .width = texture.width,
                .height = texture.height,
                .depth = 1,
                .array_layers = 1,
                .mip_levels = mip_count,
                .sample_count = rhi::image_sample_count::sample_count_1,
                .tiling = rhi::image_tiling_type::optimal,
                .location = rhi::memory_location::device,
                .usage = make_enum_mask(rhi::image_usage::sampled, rhi::image_usage::transfer_dst,
                                        rhi::image_usage::transfer_src),
                .name = texture.name,
            };

            auto image = _device->create_image(image_desc);
            images.push_back(image);
        }

        // Set up the staging buffer
        constexpr auto staging_buffer_size = 1024u * 1024 * 128; // 128 MB
        auto staging = _device->create_buffer({
            .size = staging_buffer_size,
            .location = rhi::memory_location::host,
            .usage = make_enum_mask(rhi::buffer_usage::transfer_src),
            .access_type = rhi::host_access_type::incoherent,
            .access_pattern = rhi::host_access_pattern::sequential,
            .name = "Staging Buffer",
        });

        auto staging_ptr = _device->map_buffer(staging);

        // Get the command buffer ready
        auto& work_queue = _device->get_primary_work_queue();
        auto cmd_buf = work_queue.get_next_command_list();
        work_queue.begin_command_list(cmd_buf, true);

        uint32_t images_written = 0;
        size_t staging_bytes_written = 0;

        for (const auto& tex_guid : next_texture_ids)
        {
            auto texture_opt = texture_registry.get_texture(tex_guid);
            const auto& texture = *texture_opt;

            auto image = images[images_written];

            // Change to a general image layout to be prepared for the copy
            rhi::work_queue::image_barrier image_barrier = {
                .image = image,
                .old_layout = rhi::image_layout::undefined,
                .new_layout = rhi::image_layout::general,
                .src_stages = make_enum_mask(rhi::pipeline_stage::all_transfer),
                .src_access = make_enum_mask(rhi::memory_access::none),
                .dst_stages = make_enum_mask(rhi::pipeline_stage::copy),
                .dst_access = make_enum_mask(rhi::memory_access::transfer_write),
            };

            work_queue.transition_image(cmd_buf, span(&image_barrier, 1));

            uint32_t mips_written = 0;

            for (const auto& mip : texture.mips)
            {
                // Ensure there is enough space in the staging buffer
                const auto bytes_in_mip = mip.data.size();
                const auto bytes_required = staging_bytes_written + bytes_in_mip;

                if (bytes_required > staging_buffer_size)
                {
                    _device->unmap_buffer(staging);
                    _device->flush_buffers(span(&staging, 1));

                    work_queue.end_command_list(cmd_buf);
                    auto finished = _device->create_fence({.signaled = false});

                    rhi::work_queue::submit_info submit_info = {};
                    submit_info.command_lists.push_back(cmd_buf);

                    work_queue.submit(span(&submit_info, 1), finished);

                    _device->wait(span(&finished, 1));

                    _device->destroy_fence(finished);

                    // Start a new command buffer
                    cmd_buf = work_queue.get_next_command_list();
                    work_queue.begin_command_list(cmd_buf, true);

                    staging_bytes_written = 0;
                }

                // Copy the mip data to the staging buffer
                std::memcpy(staging_ptr + staging_bytes_written, mip.data.data(), bytes_in_mip);

                work_queue.copy(cmd_buf, staging, image, rhi::image_layout::general,
                                static_cast<uint32_t>(staging_bytes_written), mips_written++);

                staging_bytes_written += bytes_in_mip;
            }

            ++images_written;
        }

        // Make sure to clean up and submit the final commands
        if (staging_bytes_written > 0)
        {
            _device->unmap_buffer(staging);
            _device->flush_buffers(span(&staging, 1));
            work_queue.end_command_list(cmd_buf);

            rhi::work_queue::submit_info submit_info = {};
            submit_info.command_lists.push_back(cmd_buf);
            auto finished = _device->create_fence({.signaled = false});

            work_queue.submit(span(&submit_info, 1), finished);
            _device->wait(span(&finished, 1));
            _device->destroy_fence(finished);
            _device->destroy_buffer(staging);
        }

        auto commands = work_queue.get_next_command_list();
        work_queue.begin_command_list(commands, true);

        // Build out the image mips
        if (generate_mip_maps)
        {
            auto image_index = 0u;
            for (const auto& tex_guid : next_texture_ids)
            {
                auto texture_opt = texture_registry.get_texture(tex_guid);
                const auto& texture = *texture_opt;
                const auto& image = images[image_index++];

                // Generate mip maps from the number of mips specified in the image source to the number of mips
                // requested for creation
                const auto max_mip_count = static_cast<uint32_t>(bit_width(min(texture.width, texture.height)));
                const auto mip_to_build_from = static_cast<std::uint32_t>(texture.mips.size()) - 1;
                const auto num_mips_to_generate = max_mip_count - mip_to_build_from;

                work_queue.generate_mip_chain(commands, image, rhi::image_layout::general, mip_to_build_from,
                                              num_mips_to_generate);
            }
        }

        // Transition the image to a shader read layout
        for (const auto& image : images)
        {
            rhi::work_queue::image_barrier image_barrier = {
                .image = image,
                .old_layout = rhi::image_layout::general,
                .new_layout = rhi::image_layout::shader_read_only,
                .src_stages = make_enum_mask(rhi::pipeline_stage::all_transfer),
                .src_access = make_enum_mask(rhi::memory_access::transfer_read, rhi::memory_access::transfer_write),
                .dst_stages = make_enum_mask(rhi::pipeline_stage::vertex_shader, rhi::pipeline_stage::fragment_shader,
                                             rhi::pipeline_stage::compute_shader),
                .dst_access = make_enum_mask(rhi::memory_access::shader_read),
            };

            work_queue.transition_image(commands, span(&image_barrier, 1));
        }

        work_queue.end_command_list(commands);
        rhi::work_queue::submit_info submit_info = {};
        submit_info.command_lists.push_back(commands);
        auto finished = _device->create_fence({.signaled = false});
        work_queue.submit(span(&submit_info, 1), finished);
        _device->wait(span(&finished, 1));
        _device->destroy_fence(finished);

        size_t image_index = 0;
        for (const auto& guid : next_texture_ids)
        {
            _bindless_textures.image_to_index.insert({guid, _bindless_textures.images.size()});
            _bindless_textures.images.push_back(images[image_index++]);
        }
    }

    void pbr_frame_graph::_load_materials(span<const guid> material_ids,
                                          const core::material_registry& material_registry)
    {
        for (const auto& guid : material_ids)
        {
            if (_materials.material_to_index.find(guid) != _materials.material_to_index.end())
            {
                continue;
            }

            const auto material_opt = material_registry.find(guid);
            if (!material_opt.has_value())
            {
                continue;
            }

            const auto& material = *material_opt;

            const auto base_color_factor =
                material.get_vec4(core::material::base_color_factor_name).value_or(math::vec4<float>{1.0f});
            const auto emissive_factor =
                material.get_vec3(core::material::emissive_factor_name).value_or(math::vec3<float>{0.0f});
            const auto normal_scale = material.get_scalar(core::material::normal_scale_name).value_or(1.0f);
            const auto metallic_factor = material.get_scalar(core::material::metallic_factor_name).value_or(1.0f);
            const auto roughness_factor = material.get_scalar(core::material::roughness_factor_name).value_or(1.0f);
            const auto alpha_cutoff = material.get_scalar(core::material::alpha_cutoff_name).value_or(0.0f);
            const auto transmissive_factor =
                material.get_scalar(core::material::transmissive_factor_name).value_or(0.0f);
            const auto thickness_factor =
                material.get_scalar(core::material::volume_thickness_factor_name).value_or(0.0f);
            const auto attenuation_distance =
                material.get_scalar(core::material::volume_attenuation_distance_name).value_or(0.0f);
            const auto attenuation_color =
                material.get_vec3(core::material::volume_attenuation_color_name).value_or(math::vec3<float>{0.0f});

            const auto material_type = [&material]() -> pbr_frame_graph::material_type {
                auto material_type_str = material.get_string(core::material::alpha_mode_name).value_or("OPAQUE");
                if (material_type_str == "OPAQUE")
                {
                    return material_type::opaque;
                }
                else if (material_type_str == "MASK")
                {
                    return material_type::mask;
                }
                else if (material_type_str == "BLEND")
                {
                    return material_type::blend;
                }
                else if (material_type_str == "TRANSMISSIVE")
                {
                    return material_type::transmissive;
                }
                else
                {
                    return material_type::opaque;
                }
            }();

            auto gpu_material = material_data{
                .base_color_factor = base_color_factor,
                .emissive_factor = math::vec4(emissive_factor.x, emissive_factor.y, emissive_factor.z, 1.0f),
                .attenuation_color = math::vec4(attenuation_color.x, attenuation_color.y, attenuation_color.z, 1.0f),
                .normal_scale = normal_scale,
                .metallic_factor = metallic_factor,
                .roughness_factor = roughness_factor,
                .alpha_cutoff = alpha_cutoff,
                .reflectance = 0.0f,
                .transmission_factor = transmissive_factor,
                .thickness_factor = thickness_factor,
                .attenuation_distance = attenuation_distance,
                .type = material_type,
            };

            if (const auto albedo_map = material.get_texture(core::material::base_color_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*albedo_map];
                gpu_material.base_color_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.base_color_texture_id = material_data::invalid_texture_id;
            }

            if (const auto metallic_map = material.get_texture(core::material::metallic_roughness_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*metallic_map];
                gpu_material.metallic_roughness_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.metallic_roughness_texture_id = material_data::invalid_texture_id;
            }

            if (const auto normal_map = material.get_texture(core::material::normal_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*normal_map];
                gpu_material.normal_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.normal_texture_id = material_data::invalid_texture_id;
            }

            if (const auto occlusion_map = material.get_texture(core::material::occlusion_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*occlusion_map];
                gpu_material.occlusion_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.occlusion_texture_id = material_data::invalid_texture_id;
            }

            if (const auto emissive_map = material.get_texture(core::material::emissive_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*emissive_map];
                gpu_material.emissive_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.emissive_texture_id = material_data::invalid_texture_id;
            }

            if (const auto transmissive_map = material.get_texture(core::material::transmissive_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*transmissive_map];
                gpu_material.transmission_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.transmission_texture_id = material_data::invalid_texture_id;
            }

            if (const auto thickness_map = material.get_texture(core::material::volume_thickness_texture_name))
            {
                const auto texture_id = _bindless_textures.image_to_index[*thickness_map];
                gpu_material.thickness_texture_id = static_cast<int16_t>(texture_id);
            }
            else
            {
                gpu_material.thickness_texture_id = material_data::invalid_texture_id;
            }

            _materials.material_to_index.insert({guid, _materials.materials.size()});
            _materials.materials.push_back(gpu_material);
        }

        // Upload the materials to GPU using the staging buffer
        const auto staging_buffer = _executor->get_buffer(_global_resources.graph_per_frame_staging_buffer);
        const auto staging_buffer_write_offset = 0; // Always write at the start for now, since we wait idle beforehand
        const auto write_length = _materials.materials.size() * sizeof(material_data);
        auto staging_buffer_ptr = _device->map_buffer(staging_buffer);
        std::memcpy(staging_buffer_ptr + staging_buffer_write_offset, _materials.materials.data(), write_length);
        _device->unmap_buffer(staging_buffer);

        _device->flush_buffers(span(&staging_buffer, 1));

        auto& wq = _device->get_primary_work_queue();
        auto cmds = wq.get_next_command_list();
        wq.begin_command_list(cmds, true);
        wq.copy(cmds, staging_buffer, _global_resources.material_buffer, staging_buffer_write_offset, 0, write_length);
        wq.end_command_list(cmds);

        auto submit_info = rhi::work_queue::submit_info{};
        submit_info.command_lists.push_back(cmds);
        auto fence = _device->create_fence({.signaled = false});
        wq.submit({&submit_info, 1}, fence);
        _device->wait({&fence, 1});
    }
} // namespace tempest::graphics