#include <tempest/render_system.hpp>

#include <tempest/array.hpp>
#include <tempest/files.hpp>
#include <tempest/imgui_context.hpp>
#include <tempest/logger.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

#include <cmath>
#include <cstring>

namespace tempest::graphics
{
    namespace
    {
        auto log = logger::logger_factory::create({"tempest::render_system"});

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

    render_system::render_system(ecs::registry& entities, const render_system_settings& settings)
        : _allocator{64 * 1024 * 1024}, _registry{&entities}, _settings{settings}, _shadow_map_subresource_allocator{
                                                                                       {8192, 8192},
                                                                                       {
                                                                                           .alignment = {32, 32},
                                                                                           .column_count = 2,
                                                                                       },
                                                                                   }
    {
        _context = render_context::create(&_allocator);

        auto devices = _context->enumerate_suitable_devices();
        if (devices.empty())
        {
            log->critical("No suitable devices found for rendering");
            std::terminate();
        }

        log->info("Found {} suitable devices. Selecting device {}", devices.size(), devices[0].name.c_str());

        _device = &_context->create_device(0);
    }

    void render_system::register_window(iwindow& win)
    {
        auto swapchain_handle = _device->create_swapchain({
            .win = &win,
            .desired_frame_count = 3,
            .use_vsync = false,
        });

        _swapchains.insert({&win, swapchain_handle});

        imgui_context::initialize_for_window(win);
    }

    void render_system::unregister_window(iwindow& win)
    {
        auto it = _swapchains.find(&win);
        if (it != _swapchains.end())
        {
            _device->release_swapchain(it->second);
            _swapchains.erase(it);
        }
    }

    void render_system::on_initialize()
    {
        auto rgc = render_graph_compiler::create_compiler(&_allocator, _device);

        auto color_buffer = rgc->create_image({
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::RGBA8_SRGB,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "Color Buffer",
        });

        auto depth_buffer = rgc->create_image({
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::D24_FLOAT,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "Depth Buffer",
        });

        auto velocity_buffer = rgc->create_image({
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::RG32_FLOAT,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "Velocity Buffer",
        });

        auto shadow_map_mt_buffer = rgc->create_image({
            .width = _shadow_map_subresource_allocator.extent().x,
            .height = _shadow_map_subresource_allocator.extent().y,
            .fmt = resource_format::D24_FLOAT,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "Shadow Map Megatexture Buffer",
        });

        vector<image_resource_handle> hi_z_images;
        for (uint32_t i = 0; i < 5; ++i)
        {
            // compute width and height of non-power of 2
            hi_z_images.push_back(rgc->create_image({
                .width = 1920u >> i,
                .height = 1080u >> i,
                .fmt = resource_format::R32_FLOAT,
                .type = image_type::IMAGE_2D,
                .persistent = true,
                .name = "Hierarchical Z Buffer",
            }));
        }

        auto encoded_normals_buffer = rgc->create_image({
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::RG16_FLOAT,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "Encoded Normals Buffer",
        });

        // Build resource buffers
        _vertex_pull_buffer = rgc->create_buffer({
            .size = 1024 * 1024 * 512,
            .location = memory_location::DEVICE,
            .name = "Vertex Pull Buffer",
        });

        _mesh_layout_buffer = rgc->create_buffer({
            .size = 1024 * 64 * sizeof(mesh_layout),
            .location = memory_location::DEVICE,
            .name = "Mesh Layout Buffer",
        });

        _scene_buffer = rgc->create_buffer({
            .size = 1024 * 64,
            .location = memory_location::DEVICE,
            .name = "Scene Buffer",
            .per_frame_memory = true,
        });

        auto dir_shadow_buffer = rgc->create_buffer({
            .size = 96 * 1024,
            .location = memory_location::AUTO,
            .name = "Shadow Map Parameter Buffer",
            .per_frame_memory = true,
        });

        auto light_buffer = rgc->create_buffer({
            .size = sizeof(gpu_light) * 1024,
            .location = memory_location::DEVICE,
            .name = "Light Parameter Buffer",
            .per_frame_memory = true,
        });

        _materials_buffer = rgc->create_buffer({
            .size = 1024 * 64 * sizeof(gpu_material_data),
            .location = memory_location::DEVICE,
            .name = "Materials Buffer",
        });

        _instance_buffer = rgc->create_buffer({
            .size = 1024 * 64 * sizeof(uint32_t),
            .location = memory_location::DEVICE,
            .name = "Instance Buffer",
            .per_frame_memory = true,
        });

        _object_buffer = rgc->create_buffer({
            .size = 1024 * 64 * sizeof(gpu_object_data),
            .location = memory_location::DEVICE,
            .name = "Object Data",
            .per_frame_memory = true,
        });

        _indirect_buffer = rgc->create_buffer({
            .size = 1024 * 64 * sizeof(indexed_indirect_command),
            .location = memory_location::AUTO,
            .name = "Indirect Commands Buffer",
            .per_frame_memory = true,
        });

        _linear_sampler = _device->create_sampler({
            .mag = filter::LINEAR,
            .min = filter::LINEAR,
            .mipmap = mipmap_mode::LINEAR,
            .enable_aniso = true,
            .max_anisotropy = 8.0f,
        });
        _samplers.push_back(_linear_sampler);

        _point_sampler = _device->create_sampler({
            .mag = filter::NEAREST,
            .min = filter::NEAREST,
            .mipmap = mipmap_mode::NEAREST,
            .enable_aniso = true,
            .max_anisotropy = 8.0f,
        });
        _samplers.push_back(_point_sampler);

        _linear_sampler_no_aniso = _device->create_sampler({
            .mag = filter::LINEAR,
            .min = filter::LINEAR,
            .mipmap = mipmap_mode::LINEAR,
            .enable_aniso = false,
            .max_anisotropy = 1.0f,
        });
        _samplers.push_back(_linear_sampler_no_aniso);

        _point_sampler_no_aniso = _device->create_sampler({
            .mag = filter::NEAREST,
            .min = filter::NEAREST,
            .mipmap = mipmap_mode::NEAREST,
            .enable_aniso = false,
            .max_anisotropy = 1.0f,
        });
        _samplers.push_back(_point_sampler_no_aniso);

        _hi_z_buffer_constants = rgc->create_buffer({
            .size = sizeof(hi_z_data),
            .location = memory_location::DEVICE,
            .name = "Hi Z Buffer Constants",
            .per_frame_memory = true,
        });

        _scene_data.sun = gpu_light{
            .color_intensity = math::vec4<float>(1.0f, 1.0f, 1.0f, 1.0f),
            .direction = math::vec4<float>(0.0f, -1.0f, 0.0f, 0.0f),
            .light_type = gpu_light_type::DIRECTIONAL,
        };
        _scene_data.screen_size = math::vec2<float>(1920.0f, 1080.0f);
        _scene_data.ambient_light = math::vec3<float>(0.1f, 0.1f, 0.1f);

        _hi_z_data = {
            .size = math::vec2<uint32_t>(1920, 1080),
            .mip_count = 5,
        };

        auto upload_pass = rgc->add_graph_pass(
            "Upload Pass", queue_operation_type::TRANSFER,
            [&, dir_shadow_buffer, light_buffer](graph_pass_builder& builder) {
                builder.add_transfer_destination_buffer(_scene_buffer)
                    .add_transfer_destination_buffer(_object_buffer)
                    .add_transfer_destination_buffer(_instance_buffer)
                    .add_transfer_destination_buffer(_indirect_buffer)
                    .add_transfer_destination_buffer(_hi_z_buffer_constants)
                    .add_transfer_destination_buffer(dir_shadow_buffer)
                    .add_transfer_destination_buffer(light_buffer)
                    .add_transfer_source_buffer(_device->get_staging_buffer())
                    .add_host_write_buffer(_device->get_staging_buffer())
                    .on_execute([&, dir_shadow_buffer, light_buffer](command_list& cmds) {
                        staging_buffer_writer writer{*_device};

                        if (_last_updated_frame + _device->frames_in_flight() > _device->current_frame())
                        {
                            uint32_t instances_written = 0;

                            for (const auto& [key, batch] : _draw_batches)
                            {
                                writer.write(cmds,
                                             span<const gpu_object_data>{batch.objects.values(),
                                                                         batch.objects.values() + batch.objects.size()},
                                             _object_buffer, instances_written * sizeof(gpu_object_data));
                                writer.write(cmds, span<const indexed_indirect_command>{batch.commands},
                                             _indirect_buffer, instances_written * sizeof(indexed_indirect_command));

                                vector<uint32_t> instances(batch.objects.size());

                                std::iota(instances.begin(), instances.end(), instances_written);
                                writer.write(cmds, span<const uint32_t>{instances}, _instance_buffer,
                                             instances_written * sizeof(uint32_t));

                                instances_written += static_cast<uint32_t>(batch.objects.size());
                            }
                        }

                        // Build scene data
                        if (_camera_entity != ecs::tombstone)
                        {
                            auto camera_data = _registry->get<camera_component>(_camera_entity);
                            auto transform = _registry->get<ecs::transform_component>(_camera_entity);

                            auto quat_rot = math::quat(transform.rotation());
                            auto f = math::extract_forward(quat_rot);
                            auto u = math::extract_up(quat_rot);

                            auto camera_view = math::look_at(transform.position(), transform.position() + f, u);
                            auto camera_projection = math::perspective(
                                camera_data.aspect_ratio, camera_data.vertical_fov / camera_data.aspect_ratio,
                                camera_data.near_plane);

                            _scene_data.camera.prev_proj = _scene_data.camera.proj;
                            _scene_data.camera.prev_view = _scene_data.camera.view;

                            _scene_data.camera.view = camera_view;
                            _scene_data.camera.inv_view = math::inverse(camera_view);
                            _scene_data.camera.proj = camera_projection;
                            _scene_data.camera.inv_proj = math::inverse(camera_projection);
                            _scene_data.camera.position = transform.position();
                        }

                        // Build and upload point and spot light data
                        vector<gpu_light> light_data;
                        for (const auto& [ent, point_light, transform] :
                             _registry->view<point_light_component, ecs::transform_component>())
                        {
                            auto sq_range = point_light.range * point_light.range;
                            auto inv_sq_range = sq_range > 0.0f ? 1.0f / sq_range : 0.0f;

                            gpu_light light = {
                                .color_intensity = math::vec4<float>(point_light.color.x, point_light.color.y,
                                                                     point_light.color.z, point_light.intensity),
                                .position_falloff = math::vec4<float>(transform.position().x, transform.position().y,
                                                                      transform.position().z, inv_sq_range),
                                .light_type = gpu_light_type::POINT,
                                .shadow_map_count = 0,
                            };

                            light_data.push_back(light);
                        }

                        _scene_data.point_light_count = static_cast<uint32_t>(light_data.size());

                        // Compute shadow data
                        _cpu_shadow_map_build_params.clear();
                        _gpu_shadow_map_use_parameters.clear();
                        _shadow_map_subresource_allocator.clear();

                        uint32_t shadow_maps_written = 0;

                        for (const auto& [ent, dir_light, shadow_map, transform] :
                             _registry
                                 ->view<directional_light_component, shadow_map_component, ecs::transform_component>())
                        {
                            auto params = compute_shadow_map_cascades(shadow_map, transform,
                                                                      _registry->get<camera_component>(_camera_entity));

                            _scene_data.sun.shadow_map_count = static_cast<uint32_t>(params.projections.size());
                            for (size_t i = 0; i < params.projections.size(); ++i)
                            {
                                auto region = _shadow_map_subresource_allocator.allocate(shadow_map.size);
                                assert(region.has_value());

                                cpu_shadow_map_parameter cpu_params = {
                                    .proj_matrix = params.projections[i],
                                    .shadow_map_bounds =
                                        {
                                            region->position.x,
                                            region->position.y,
                                            region->extent.x,
                                            region->extent.y,
                                        },
                                    .light_entity = ent,
                                };

                                _scene_data.sun.shadow_map_indices[i] = shadow_maps_written++;

                                _cpu_shadow_map_build_params.push_back(cpu_params);

                                gpu_shadow_map_parameter gpu_param = {
                                    .light_proj_matrix = params.projections[i],
                                    .shadow_map_region =
                                        {
                                            static_cast<float>(region->position.x) /
                                                (_shadow_map_subresource_allocator.extent().x),
                                            static_cast<float>(region->position.y) /
                                                (_shadow_map_subresource_allocator.extent().y),
                                            static_cast<float>(region->extent.x) /
                                                (_shadow_map_subresource_allocator.extent().x),
                                            static_cast<float>(region->extent.y) /
                                                (_shadow_map_subresource_allocator.extent().x),
                                        },
                                    .cascade_split_far = params.cascade_splits[i],
                                };

                                _gpu_shadow_map_use_parameters.push_back(gpu_param);
                            }
                        }

                        // Upload Directional Shadow Map Data
                        writer.write<gpu_shadow_map_parameter>(cmds, span(_gpu_shadow_map_use_parameters),
                                                               dir_shadow_buffer);

                        // Upload scene data
                        writer.write(cmds, span<const gpu_scene_data>{&_scene_data, static_cast<size_t>(1)},
                                     _scene_buffer);

                        // Upload hierarchical z data
                        writer.write(cmds, span<const hi_z_data>{&_hi_z_data, static_cast<size_t>(1)},
                                     _hi_z_buffer_constants);

                        span<const gpu_light> light_span{light_data};
                        writer.write(cmds, light_span, light_buffer);

                        writer.finish();
                    });
            });

        _z_prepass_pass =
            rgc->add_graph_pass("Z Pre Pass", queue_operation_type::GRAPHICS, [&](graph_pass_builder& bldr) {
                bldr.depends_on(upload_pass)
                    .add_depth_attachment(depth_buffer, resource_access_type::READ_WRITE, load_op::CLEAR,
                                          store_op::STORE, 0.0f)
                    .add_color_attachment(encoded_normals_buffer, resource_access_type::WRITE, load_op::CLEAR,
                                          store_op::STORE, {0.0f, 0.0f, 0.0f, 0.0f})
                    .add_structured_buffer(_scene_buffer, resource_access_type::READ, 0, 0)
                    .add_structured_buffer(_vertex_pull_buffer, resource_access_type::READ, 0, 1)
                    .add_structured_buffer(_mesh_layout_buffer, resource_access_type::READ, 0, 2)
                    .add_structured_buffer(_object_buffer, resource_access_type::READ, 0, 3)
                    .add_structured_buffer(_instance_buffer, resource_access_type::READ, 0, 4)
                    .add_structured_buffer(_materials_buffer, resource_access_type::READ, 0, 5)
                    .add_sampler(_linear_sampler, 0, 6, pipeline_stage::FRAGMENT)
                    .add_external_sampled_images(512, 0, 7, pipeline_stage::FRAGMENT)
                    .add_indirect_argument_buffer(_indirect_buffer)
                    .add_index_buffer(_vertex_pull_buffer)
                    .should_execute([this]() { return _settings.aa_mode != anti_aliasing_mode::MSAA; })
                    .on_execute([&](command_list& cmds) {
                        cmds.set_scissor_region(0, 0, 1920, 1080)
                            .set_viewport(0, 0, 1920, 1080)
                            .use_index_buffer(_vertex_pull_buffer, 0);

                        for (auto [key, batch] : _draw_batches)
                        {
                            if (key.alpha_type == alpha_behavior::OPAQUE || key.alpha_type == alpha_behavior::MASK)
                            {
                                cmds.use_pipeline(_z_prepass_pipeline)
                                    .draw_indexed(
                                        _indirect_buffer,
                                        static_cast<uint32_t>(_device->get_buffer_frame_offset(_indirect_buffer)),
                                        static_cast<uint32_t>(batch.objects.size()), sizeof(indexed_indirect_command));
                            }
                        }
                    });
            });

        auto build_hi_z_pass =
            rgc->add_graph_pass("Build Hi Z Pass", queue_operation_type::COMPUTE, [&](graph_pass_builder& bldr) {
                bldr.depends_on(_z_prepass_pass)
                    .depends_on(upload_pass)
                    .add_constant_buffer(_hi_z_buffer_constants, 0, 0)
                    .add_sampled_image(depth_buffer, 0, 1)
                    .add_storage_image(hi_z_images, resource_access_type::READ_WRITE, 0, 2)
                    .add_sampler(_linear_sampler, 0, 7, pipeline_stage::COMPUTE)
                    .should_execute([this]() { return _settings.aa_mode != anti_aliasing_mode::MSAA; })
                    .on_execute([&](command_list& cmds) {
                        cmds.use_pipeline(_hzb_build_pipeline)
                            .dispatch(math::div_ceil(1920, 32), math::div_ceil(1080, 32), 1);
                    });
            });

        auto pbr_commands = [&](command_list& cmds) {
            cmds.set_scissor_region(0, 0, 1920, 1080)
                .set_viewport(0, 0, 1920, 1080)
                .use_index_buffer(_vertex_pull_buffer, 0);

            uint32_t draw_calls_issued = 0;

            for (auto [key, batch] : _draw_batches)
            {
                graphics_pipeline_resource_handle pipeline;
                if (key.alpha_type == alpha_behavior::OPAQUE || key.alpha_type == alpha_behavior::MASK)
                {
                    pipeline = _pbr_opaque_pipeline;
                }
                else if (key.alpha_type == alpha_behavior::TRANSPARENT)
                {
                    pipeline = _pbr_transparencies_pipeline;
                }

                cmds.use_pipeline(pipeline).draw_indexed(
                    _indirect_buffer,
                    static_cast<uint32_t>(_device->get_buffer_frame_offset(_indirect_buffer) +
                                          draw_calls_issued * sizeof(indexed_indirect_command)),
                    static_cast<uint32_t>(batch.objects.size()), sizeof(indexed_indirect_command));

                draw_calls_issued += static_cast<uint32_t>(batch.commands.size());
            }
        };

        _shadow_map_pass =
            rgc->add_graph_pass("Shadow Map Pass", queue_operation_type::GRAPHICS, [&](graph_pass_builder& bldr) {
                bldr.depends_on(upload_pass)
                    .add_depth_attachment(shadow_map_mt_buffer, resource_access_type::READ_WRITE, load_op::CLEAR,
                                          store_op::STORE, 0.0f)
                    .add_structured_buffer(_vertex_pull_buffer, resource_access_type::READ, 0, 1)
                    .add_structured_buffer(_mesh_layout_buffer, resource_access_type::READ, 0, 2)
                    .add_structured_buffer(_object_buffer, resource_access_type::READ, 0, 3)
                    .add_structured_buffer(_instance_buffer, resource_access_type::READ, 0, 4)
                    .add_structured_buffer(_materials_buffer, resource_access_type::READ, 0, 5)
                    .add_sampler(_linear_sampler, 0, 6, pipeline_stage::FRAGMENT)
                    .add_external_sampled_images(512, 0, 7, pipeline_stage::FRAGMENT)
                    .add_indirect_argument_buffer(_indirect_buffer)
                    .add_index_buffer(_vertex_pull_buffer)
                    .allow_push_constants(64)
                    .on_execute([&](command_list& cmds) {
                        // Render directional shadow maps
                        cmds.use_pipeline(_directional_shadow_map_pipeline);

                        auto lights_with_shadows = _registry->view<directional_light_component, shadow_map_component>();

                        uint32_t lights_written = 0;

                        for (const auto& [ent, dir_light, shadows] : lights_with_shadows)
                        {
                            for (auto i = 0u; i < shadows.cascade_count; ++i)
                            {
                                const auto& params = _cpu_shadow_map_build_params[lights_written];
                                const auto& region = params.shadow_map_bounds;

                                // Set up viewport and scissor test
                                cmds.set_scissor_region(region.x, region.y, region.z, region.w)
                                    .set_viewport(static_cast<float>(region.x), static_cast<float>(region.y),
                                                  static_cast<float>(region.z), static_cast<float>(region.w), 0.0f,
                                                  1.0f, false);

                                // Push the shadow map projection matrix
                                cmds.push_constants(0, params.proj_matrix, _directional_shadow_map_pipeline);

                                uint32_t draw_calls_issued = 0;
                                for (auto [key, batch] : _draw_batches)
                                {
                                    if (key.alpha_type == alpha_behavior::OPAQUE ||
                                        key.alpha_type == alpha_behavior::MASK)
                                    {
                                        cmds.draw_indexed(
                                            _indirect_buffer,
                                            static_cast<uint32_t>(_device->get_buffer_frame_offset(_indirect_buffer) +
                                                                  draw_calls_issued * sizeof(indexed_indirect_command)),
                                            static_cast<uint32_t>(batch.objects.size()),
                                            sizeof(indexed_indirect_command));

                                        draw_calls_issued += static_cast<uint32_t>(batch.commands.size());
                                    }
                                }

                                lights_written++;
                            }
                        }
                    });
            });

        _pbr_pass = rgc->add_graph_pass("PBR Pass", queue_operation_type::GRAPHICS, [&](graph_pass_builder& bldr) {
            bldr.depends_on(_z_prepass_pass)
                .depends_on(build_hi_z_pass)
                .depends_on(upload_pass)
                .depends_on(_shadow_map_pass)
                .add_color_attachment(color_buffer, resource_access_type::READ_WRITE, load_op::CLEAR, store_op::STORE,
                                      {0.5f, 1.0f, 1.0f, 1.0f})
                .add_color_attachment(velocity_buffer, resource_access_type::READ_WRITE, load_op::CLEAR,
                                      store_op::STORE, {0.0f, 0.0f, 0.0f, 0.0f})
                .add_depth_attachment(depth_buffer, resource_access_type::READ_WRITE, load_op::LOAD,
                                      store_op::DONT_CARE)
                .add_structured_buffer(_scene_buffer, resource_access_type::READ, 0, 0)
                .add_structured_buffer(_vertex_pull_buffer, resource_access_type::READ, 0, 1)
                .add_structured_buffer(_mesh_layout_buffer, resource_access_type::READ, 0, 2)
                .add_structured_buffer(_object_buffer, resource_access_type::READ, 0, 3)
                .add_structured_buffer(_instance_buffer, resource_access_type::READ, 0, 4)
                .add_structured_buffer(_materials_buffer, resource_access_type::READ, 0, 5)
                .add_sampler(_linear_sampler, 0, 6, pipeline_stage::FRAGMENT)
                .add_external_sampled_images(512, 0, 7, pipeline_stage::FRAGMENT)
                .add_structured_buffer(light_buffer, resource_access_type::READ, 1, 0)
                .add_structured_buffer(dir_shadow_buffer, resource_access_type::READ, 1, 1)
                .add_sampled_image(shadow_map_mt_buffer, 1, 2)
                .add_indirect_argument_buffer(_indirect_buffer)
                .add_index_buffer(_vertex_pull_buffer)
                .on_execute(pbr_commands);
        });

        auto imgui_pass = rgc->add_graph_pass(
            "ImGUI Graph Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
                bldr.add_color_attachment(color_buffer, graphics::resource_access_type::WRITE, graphics::load_op::LOAD,
                                          graphics::store_op::STORE)
                    .draw_imgui()
                    .depends_on(_pbr_pass)
                    .should_execute([this]() {
                        return _settings.aa_mode != anti_aliasing_mode::TAA && _settings.enable_imgui &&
                               _create_imgui_hierarchy;
                    })
                    .on_execute([]([[maybe_unused]] auto& cmds) {});
            });

        for (auto& [win, sc_handle] : _swapchains)
        {
            rgc->add_graph_pass("Swapchain Resolve", queue_operation_type::GRAPHICS_AND_TRANSFER,
                                [&](graph_pass_builder& builder) {
                                    builder.add_external_blit_target(sc_handle)
                                        .add_blit_source(color_buffer)
                                        .depends_on(imgui_pass)
                                        .depends_on(_pbr_pass)
                                        .should_execute([this]() {
                                            return _settings.aa_mode == anti_aliasing_mode::NONE ||
                                                   _settings.aa_mode == anti_aliasing_mode::MSAA;
                                        })
                                        .on_execute([&, sc_handle, color_buffer](command_list& cmds) {
                                            cmds.blit(color_buffer, _device->fetch_current_image(sc_handle));
                                        });
                                });
        }

        if (_settings.enable_imgui)
        {
            rgc->enable_imgui();
        }

        if (_settings.enable_profiling)
        {
            rgc->enable_gpu_profiling();
        }

        _graph = tempest::move(*rgc).compile();
    }

    void render_system::after_initialize()
    {
        auto staging_buffer = _device->get_staging_buffer();
        auto mapped_staging_buffer = _device->map_buffer(staging_buffer);

        // upload mesh layouts
        {
            auto& commands = _device->get_command_executor().get_commands();
            std::memcpy(mapped_staging_buffer.data(), _meshes.data(), _meshes.size() * sizeof(mesh_layout));
            commands.copy(staging_buffer, _mesh_layout_buffer, 0, 0, _meshes.size() * sizeof(mesh_layout));
            _device->get_command_executor().submit_and_wait();
        }

        // upload materials
        {
            auto& commands = _device->get_command_executor().get_commands();
            std::memcpy(mapped_staging_buffer.data(), _materials.data(), _materials.size() * sizeof(gpu_material_data));
            commands.copy(staging_buffer, _materials_buffer, 0, 0, _materials.size() * sizeof(gpu_material_data));
            _device->get_command_executor().submit_and_wait();
        }

        _device->unmap_buffer(staging_buffer);

        _graph->update_external_sampled_images(_pbr_pass, _images, 0, 7, pipeline_stage::FRAGMENT);
        _graph->update_external_sampled_images(_z_prepass_pass, _images, 0, 7, pipeline_stage::FRAGMENT);
        _graph->update_external_sampled_images(_shadow_map_pass, _images, 0, 7, pipeline_stage::FRAGMENT);

        _pbr_opaque_pipeline = create_pbr_pipeline(false);
        _pbr_transparencies_pipeline = create_pbr_pipeline(true);
        _z_prepass_pipeline = create_z_prepass_pipeline();
        _directional_shadow_map_pipeline = create_directional_shadow_map_pipeline();
        _hzb_build_pipeline = create_hzb_build_pipeline();

        _last_updated_frame = _device->current_frame();

        // find camera
        for (ecs::entity ent : _registry->entities())
        {
            if (_registry->has<camera_component>(ent))
            {
                _camera_entity = ent;
                break;
            }
        }
    }

    void render_system::render()
    {
        if (_last_updated_frame + _device->frames_in_flight() > _device->current_frame())
        {
            for (auto [_, batch] : _draw_batches)
            {
                batch.commands.clear();
            }

            for (const auto& [ent, transform, renderable] :
                 _registry->view<ecs::transform_component, renderable_component>())
            {
                gpu_object_data object_payload = {
                    .model = math::mat4<float>(1.0f),
                    .inv_tranpose_model = math::mat4<float>(1.0f),
                    .prev_model = math::mat4<float>(1.0f),
                    .mesh_id = static_cast<uint32_t>(renderable.mesh_id),
                    .material_id = static_cast<uint32_t>(renderable.material_id),
                    .parent_id = ~0u,
                    .self_id = static_cast<uint32_t>(renderable.object_id),
                };

                auto ancestor_view = ecs::ancestor_entity_view(*_registry, ent);
                for (auto ent : ancestor_view)
                {
                    if (auto tx = _registry->try_get<ecs::transform_component>(ent))
                    {
                        object_payload.model = tx->matrix() * object_payload.model;
                    }
                }

                object_payload.inv_tranpose_model = math::transpose(math::inverse(object_payload.model));

                draw_batch_key key = {
                    .alpha_type = static_cast<alpha_behavior>(_materials[renderable.material_id].material_type),
                };

                auto& draw_batch = _draw_batches[key];
                const auto& mesh = _meshes[renderable.mesh_id];

                auto object_data_it = draw_batch.objects.find(ent);
                if (object_data_it == draw_batch.objects.end()) [[unlikely]]
                {
                    log->info("New object added to draw batch - ID: {}, Mesh ID: {}, Material ID: {} - Entity {}:{}",
                              renderable.object_id, renderable.mesh_id, renderable.material_id,
                              ecs::entity_traits<ecs::entity>::as_entity(ent),
                              ecs::entity_traits<ecs::entity>::as_version(ent));

                    object_payload.prev_model = object_payload.model;

                    draw_batch.objects.insert(ent, object_payload);
                }
                else
                {
                    const auto& prev_data = draw_batch.objects[ent];
                    object_payload.prev_model = prev_data.model;
                    draw_batch.objects[ent] = object_payload;
                }

                draw_batch.commands.push_back({
                    .index_count = mesh.index_count,
                    .instance_count = 1,
                    .first_index = (mesh.mesh_start_offset + mesh.index_offset) / 4,
                    .vertex_offset = 0,
                    .first_instance = static_cast<uint32_t>(draw_batch.objects.index_of(ent)),
                });
            }

            // Iterate through the draw batches and update first instance based on the number of instances in the
            // previous batches
            uint32_t instances_written = 0;
            for (auto [_, batch] : _draw_batches)
            {
                for (auto& cmd : batch.commands)
                {
                    cmd.first_instance += instances_written;
                }

                instances_written += static_cast<uint32_t>(batch.objects.size());
            }

            // Find the directional light for the scene
            for (auto [ent, dir_light, tx] : _registry->view<directional_light_component, ecs::transform_component>())
            {
                _scene_data.sun.color_intensity =
                    math::vec4<float>(dir_light.color.x, dir_light.color.y, dir_light.color.z, dir_light.intensity);

                // Rotate 0, 0, 1 by the rotation of the transform
                auto light_rot = math::rotate(tx.rotation());
                auto light_dir = light_rot * math::vec4<float>(0.0f, 0.0f, 1.0f, 0.0f);
                _scene_data.sun.direction = math::vec4<float>(light_dir.x, light_dir.y, light_dir.z, 0.0f);
            }
        }

        if (_create_imgui_hierarchy && _settings.enable_imgui)
        {
            imgui_context::create_frame([this]() { _create_imgui_hierarchy(); });
        }

        if (_static_data_dirty) [[unlikely]]
        {
            _static_data_dirty = false;
            _last_updated_frame = _device->current_frame() + _device->frames_in_flight();
        }

        _graph->execute();
    }

    void render_system::on_close()
    {
        imgui_context::shutdown();

        for (auto& [win, sc_handle] : _swapchains)
        {
            _device->release_swapchain(sc_handle);
        }

        for (auto& img : _images)
        {
            _device->release_image(img);
        }

        for (auto& buf : _buffers)
        {
            _device->release_buffer(buf);
        }

        for (auto& pipeline : _graphics_pipelines)
        {
            _device->release_graphics_pipeline(pipeline);
        }

        for (auto& pipeline : _compute_pipelines)
        {
            _device->release_compute_pipeline(pipeline);
        }

        for (auto& sampler : _samplers)
        {
            _device->release_sampler(sampler);
        }

        _device->release_buffer(_vertex_pull_buffer);
        _device->release_buffer(_scene_buffer);
        _device->release_buffer(_materials_buffer);
        _device->release_buffer(_instance_buffer);
        _device->release_buffer(_object_buffer);
        _device->release_buffer(_indirect_buffer);

        _swapchains.clear();
        _images.clear();
        _buffers.clear();
        _graphics_pipelines.clear();
        _compute_pipelines.clear();
        _samplers.clear();
    }

    void render_system::update_settings(const render_system_settings& settings)
    {
        _settings = settings;
        _settings_dirty = true;
    }

    flat_unordered_map<guid, mesh_layout> render_system::load_meshes(span<const guid> mesh_guids,
                                                                     core::mesh_registry& mesh_reg)
    {
        flat_unordered_map<guid, mesh_layout> mesh_layouts;
        vector<guid> new_mesh_ids;
        for (const auto& guid : mesh_guids)
        {
            if (_mesh_id_map.find(guid) == _mesh_id_map.end())
            {
                new_mesh_ids.push_back(guid);
            }
            else
            {
                mesh_layouts[guid] = _meshes[_mesh_id_map[guid]];
            }
        }

        auto mesh_layout_mapping = renderer_utilities::upload_meshes(
            *_device, mesh_guids, mesh_reg, _vertex_pull_buffer, _mesh_bytes, _device->get_staging_buffer());

        for (const auto& [guid, layout] : mesh_layout_mapping)
        {
            log->info("Uploaded mesh with guid: {} at index {}", to_string(guid).c_str(), _meshes.size());
            _mesh_id_map[guid] = _meshes.size();
            _meshes.push_back(layout);
            mesh_layouts[guid] = layout;
        }

        return mesh_layouts;
    }

    vector<mesh_layout> render_system::load_meshes(span<core::mesh> meshes)
    {
        auto mesh_layouts = renderer_utilities::upload_meshes(*_device, meshes, _vertex_pull_buffer, _mesh_bytes);

        for (const auto& mesh : mesh_layouts)
        {
            _meshes.push_back(mesh);
        }

        return mesh_layouts;
    }

    void render_system::load_textures(span<texture_data_descriptor> texture_sources, bool generate_mip_maps)
    {
        auto textures = renderer_utilities::upload_textures(*_device, texture_sources, _device->get_staging_buffer(),
                                                            true, generate_mip_maps);

        for (auto& tex : textures)
        {
            _images.push_back(tex);
        }
    }

    void render_system::load_textures(span<const guid> texture_ids, const core::texture_registry& tex_reg,
                                      bool generate_mip_maps)
    {
        // Get the unique texture ids
        vector<guid> new_texture_ids;
        for (const auto& guid : texture_ids)
        {
            if (_image_id_map.find(guid) == _image_id_map.end() &&
                std::find(new_texture_ids.begin(), new_texture_ids.end(), guid) == new_texture_ids.end())
            {
                new_texture_ids.push_back(guid);
            }
        }

        auto textures = renderer_utilities::upload_textures(*_device, new_texture_ids, tex_reg,
                                                            _device->get_staging_buffer(), true, generate_mip_maps);

        size_t idx = 0;
        for (auto& tex : textures)
        {
            if (_image_id_map.find(new_texture_ids[idx]) == _image_id_map.end())
            {
                _image_id_map[new_texture_ids[idx]] = _images.size();
                _images.push_back(tex);

                ++idx;
            }
        }
    }

    void render_system::load_material(material_payload& material)
    {
        auto mat = gpu_material_data{
            .base_color_factor = material.base_color_factor,
            .emissive_factor = math::vec4<float>(material.emissive_factor.x, material.emissive_factor.y,
                                                 material.emissive_factor.z, 1.0f),
            .normal_scale = material.normal_scale,
            .metallic_factor = material.metallic_factor,
            .roughness_factor = material.roughness_factor,
            .alpha_cutoff = material.alpha_cutoff,
            .reflectance = material.reflectance,
            .base_color_texture_id = material.albedo_map_id == std::numeric_limits<uint32_t>::max()
                                         ? gpu_material_data::INVALID_TEXTURE_ID
                                         : static_cast<int16_t>(texture_count() + material.albedo_map_id),
            .normal_texture_id = material.normal_map_id == std::numeric_limits<uint32_t>::max()
                                     ? gpu_material_data::INVALID_TEXTURE_ID
                                     : static_cast<int16_t>(texture_count() + material.normal_map_id),
            .metallic_roughness_texture_id = material.metallic_map_id == std::numeric_limits<uint32_t>::max()
                                                 ? gpu_material_data::INVALID_TEXTURE_ID
                                                 : static_cast<int16_t>(texture_count() + material.metallic_map_id),
            .emissive_texture_id = material.emissive_map_id == std::numeric_limits<uint32_t>::max()
                                       ? gpu_material_data::INVALID_TEXTURE_ID
                                       : static_cast<int16_t>(texture_count() + material.emissive_map_id),
            .occlusion_texture_id = material.ao_map_id == std::numeric_limits<uint32_t>::max()
                                        ? gpu_material_data::INVALID_TEXTURE_ID
                                        : static_cast<int16_t>(texture_count() + material.ao_map_id),
            .material_type = static_cast<gpu_material_type>(material.type),
        };

        _materials.push_back(mat);
    }

    void render_system::load_materials(span<const guid> material_guids, const core::material_registry& mat_reg)
    {
        for (const auto& guid : material_guids)
        {
            // Check if this material already exists
            if (_material_id_map.find(guid) != _material_id_map.end())
            {
                continue;
            }

            auto material = mat_reg.get_material(guid);
            if (material)
            {
                auto base_color_factor = material->get_vec4(core::material::base_color_factor_name)
                                             .value_or(math::vec4<float>(1.0f, 1.0f, 1.0f, 1.0f));
                auto emissive_factor = material->get_vec3(core::material::emissive_factor_name)
                                           .value_or(math::vec3<float>(0.0f, 0.0f, 0.0f));
                auto normal_scale = material->get_scalar(core::material::normal_scale_name).value_or(1.0f);
                auto metallic_factor = material->get_scalar(core::material::metallic_factor_name).value_or(1.0f);
                auto roughness_factor = material->get_scalar(core::material::roughness_factor_name).value_or(1.0f);
                auto alpha_cutoff = material->get_scalar(core::material::alpha_cutoff_name).value_or(0.5f);

                // TODO: Rework material types
                auto material_type = [&]() -> gpu_material_type {
                    auto material_type_str = material->get_string(core::material::alpha_mode_name).value_or("OPAQUE");
                    if (material_type_str == "OPAQUE")
                    {
                        return gpu_material_type::PBR_OPAQUE;
                    }
                    else if (material_type_str == "MASK")
                    {
                        return gpu_material_type::PBR_MASK;
                    }
                    else if (material_type_str == "TRANSPARENT")
                    {
                        return gpu_material_type::PBR_BLEND;
                    }
                    else
                    {
                        return gpu_material_type::PBR_OPAQUE;
                    }
                }();

                auto gpu_material = gpu_material_data{
                    .base_color_factor = base_color_factor,
                    .emissive_factor = math::vec4<float>(emissive_factor.x, emissive_factor.y, emissive_factor.z, 1.0f),
                    .normal_scale = normal_scale,
                    .metallic_factor = metallic_factor,
                    .roughness_factor = roughness_factor,
                    .alpha_cutoff = alpha_cutoff,
                    .reflectance = 0.0f,
                    .material_type = material_type,
                };

                if (const auto albedo_map = material->get_texture(core::material::base_color_texture_name))
                {
                    auto tex_id = _image_id_map[*albedo_map];
                    gpu_material.base_color_texture_id = static_cast<int16_t>(tex_id);
                }
                else
                {
                    gpu_material.base_color_texture_id = gpu_material_data::INVALID_TEXTURE_ID;
                }

                if (const auto normal_map = material->get_texture(core::material::normal_texture_name))
                {
                    auto tex_id = _image_id_map[*normal_map];
                    gpu_material.normal_texture_id = static_cast<int16_t>(tex_id);
                }
                else
                {
                    gpu_material.normal_texture_id = gpu_material_data::INVALID_TEXTURE_ID;
                }

                if (const auto metallic_map = material->get_texture(core::material::metallic_roughness_texture_name))
                {
                    auto tex_id = _image_id_map[*metallic_map];
                    gpu_material.metallic_roughness_texture_id = static_cast<int16_t>(tex_id);
                }
                else
                {
                    gpu_material.metallic_roughness_texture_id = gpu_material_data::INVALID_TEXTURE_ID;
                }

                if (const auto emissive_map = material->get_texture(core::material::emissive_texture_name))
                {
                    auto tex_id = _image_id_map[*emissive_map];
                    gpu_material.emissive_texture_id = static_cast<int16_t>(tex_id);
                }
                else
                {
                    gpu_material.emissive_texture_id = gpu_material_data::INVALID_TEXTURE_ID;
                }

                if (const auto ao_map = material->get_texture(core::material::occlusion_texture_name))
                {
                    auto tex_id = _image_id_map[*ao_map];
                    gpu_material.occlusion_texture_id = static_cast<int16_t>(tex_id);
                }
                else
                {
                    gpu_material.occlusion_texture_id = gpu_material_data::INVALID_TEXTURE_ID;
                }

                log->info("Uploaded material with guid: {} at index {}", to_string(guid).c_str(), _materials.size());

                _material_id_map[guid] = _materials.size();
                _materials.push_back(gpu_material);
            }
        }
    }

    void render_system::mark_dirty()
    {
        _static_data_dirty = true;
    }

    void render_system::draw_profiler()
    {
        _graph->show_gpu_profiling();
    }

    graphics_pipeline_resource_handle render_system::create_pbr_pipeline(bool enable_blend)
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/pbr.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/pbr.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            scene_constant_buffer,
            vertex_pull_buffer_desc,
            mesh_layout_buffer_desc,
            object_buffer_desc,
            instance_buffer_desc,
            materials_buffer_desc,
            linear_sampler_desc,
            texture_array_desc,
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

        if (enable_blend)
        {
            blending[0].enabled = true;
            blending[0].color.src = blend_factor::ONE;
            blending[0].color.dst = blend_factor::ONE_MINUS_SRC_ALPHA;
            blending[0].color.op = blend_operation::ADD;
            blending[0].alpha.src = blend_factor::ONE;
            blending[0].alpha.dst = blend_factor::ONE_MINUS_SRC_ALPHA;
            blending[0].alpha.op = blend_operation::ADD;
        }

        auto pipeline = _device->create_graphics_pipeline({
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

        _graphics_pipelines.push_back(pipeline);

        return pipeline;
    }

    graphics_pipeline_resource_handle render_system::create_z_prepass_pipeline()
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/zprepass.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/zprepass.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            {
                .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
                .binding_index = 0,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::STRUCTURED_BUFFER,
                .binding_index = 1,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::STRUCTURED_BUFFER,
                .binding_index = 2,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
                .binding_index = 3,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
                .binding_index = 4,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::STRUCTURED_BUFFER,
                .binding_index = 5,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::SAMPLER,
                .binding_index = 6,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::SAMPLED_IMAGE,
                .binding_index = 7,
                .binding_count = 512,
            },
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
        };

        resource_format color_buffer_fmt[] = {resource_format::RG16_FLOAT};

        color_blend_attachment_state blending[] = {
            {
                .enabled = false,
            },
        };

        auto pipeline = _device->create_graphics_pipeline({
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
                .name = "Z Prepass Vertex Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "Z Prepass Fragment Shader Module",
            },
            .depth_testing{
                .enable_test = true,
                .enable_write = true,
                .depth_test_op = compare_operation::GREATER_OR_EQUALS,
            },
            .blending{
                .attachment_blend_ops = blending,
            },
            .name = "Z Prepass Pipeline",
        });

        _graphics_pipelines.push_back(pipeline);

        return pipeline;
    }

    compute_pipeline_resource_handle render_system::create_hzb_build_pipeline()
    {
        auto compute_shader_source = core::read_bytes("assets/shaders/hzb.comp.spv");

        descriptor_binding_info set0_bindings[] = {
            {
                .type = descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC,
                .binding_index = 0,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::SAMPLED_IMAGE,
                .binding_index = 1,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::STORAGE_IMAGE,
                .binding_index = 2,
                .binding_count = 5,
            },
            {
                .type = descriptor_binding_type::SAMPLER,
                .binding_index = 7,
                .binding_count = 1,
            },
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
        };

        auto pipeline = _device->create_compute_pipeline({
            .layout{
                .set_layouts = layouts,
            },
            .compute_shader{
                .bytes = compute_shader_source,
                .entrypoint = "main",
                .name = "HZB Build Compute Shader Module",
            },
            .name = "HZB Build Pipeline",
        });

        _compute_pipelines.push_back(pipeline);

        return pipeline;
    }

    graphics_pipeline_resource_handle render_system::create_taa_resolve_pipeline()
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/taa.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/taa.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            {
                .type = descriptor_binding_type::SAMPLED_IMAGE,
                .binding_index = 0,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::SAMPLED_IMAGE,
                .binding_index = 1,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::SAMPLED_IMAGE,
                .binding_index = 2,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::SAMPLER,
                .binding_index = 3,
                .binding_count = 1,
            },
            {
                .type = descriptor_binding_type::SAMPLER,
                .binding_index = 4,
                .binding_count = 1,
            },
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
        };

        resource_format color_buffer_fmt[] = {resource_format::RGBA8_SRGB};

        color_blend_attachment_state blending[] = {
            {
                .enabled = false,
            },
        };

        auto pipeline = _device->create_graphics_pipeline({
            .layout{
                .set_layouts = layouts,
            },
            .target{
                .color_attachment_formats = color_buffer_fmt,
            },
            .vertex_shader{
                .bytes = vertex_shader_source,
                .entrypoint = "main",
                .name = "TAA Resolve Vertex Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "TAA Resolve Fragment Shader Module",
            },
            .depth_testing{
                .enable_test = false,
                .enable_write = false,
                .depth_test_op = compare_operation::NEVER,
            },
            .blending{
                .attachment_blend_ops = blending,
            },
            .name = "TAA Resolve Pipeline",
        });

        _graphics_pipelines.push_back(pipeline);

        return pipeline;
    }

    graphics_pipeline_resource_handle render_system::create_sharpen_pipeline()
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/sharpen.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/sharpen.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            {
                .type = descriptor_binding_type::SAMPLED_IMAGE,
                .binding_index = 0,
                .binding_count = 1,
            },
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
        };

        resource_format color_buffer_fmt[] = {resource_format::RGBA8_SRGB};

        color_blend_attachment_state blending[] = {
            {
                .enabled = false,
            },
        };

        auto pipeline = _device->create_graphics_pipeline({
            .layout{
                .set_layouts = layouts,
            },
            .target{
                .color_attachment_formats = color_buffer_fmt,
            },
            .vertex_shader{
                .bytes = vertex_shader_source,
                .entrypoint = "main",
                .name = "Sharpen Vertex Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "Sharpen Shader Module",
            },
            .depth_testing{
                .enable_test = false,
                .enable_write = false,
                .depth_test_op = compare_operation::NEVER,
            },
            .blending{
                .attachment_blend_ops = blending,
            },
            .name = "Sharpen Pipeline",
        });

        _graphics_pipelines.push_back(pipeline);

        return pipeline;
    }

    graphics_pipeline_resource_handle render_system::create_directional_shadow_map_pipeline()
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/directional_shadow_map.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/directional_shadow_map.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            vertex_pull_buffer_desc, mesh_layout_buffer_desc, object_buffer_desc, instance_buffer_desc,
            materials_buffer_desc,   linear_sampler_desc,     texture_array_desc,
        };

        descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
        };

        push_constant_layout push_constants[] = {
            {
                .offset = 0,
                .range = 64, // single mat4
            },
        };

        // Depth buffer format
        resource_format depth_buffer_fmt[] = {resource_format::D24_FLOAT};

        auto pipeline = _device->create_graphics_pipeline({
            .layout{
                .set_layouts = layouts,
                .push_constants = push_constants,
            },
            .target{
                .depth_attachment_format = depth_buffer_fmt[0],
            },
            .vertex_shader{
                .bytes = vertex_shader_source,
                .entrypoint = "main",
                .name = "Directional Shadow Map Vertex Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "Directional Shadow Map Fragment Shader Module",
            },
            .depth_testing{
                .enable_test = true,
                .enable_write = true,
                .clamp_depth = true,
                .depth_test_op = compare_operation::GREATER,
            },
            .blending{
                .attachment_blend_ops = {},
            },
            .name = "Directional Shadow Map Pipeline",
        });

        _graphics_pipelines.push_back(pipeline);

        return pipeline;
    }

    // TODO: Move this to a compute shader
    render_system::shadow_map_parameters render_system::compute_shadow_map_cascades(
        const shadow_map_component& shadowing, const ecs::transform_component& light_transform,
        const camera_component& camera_data)
    {
        float near_plane = camera_data.near_plane;
        float far_plane = camera_data.far_shadow_plane;
        float clip_range = far_plane - near_plane;

        float ratio = far_plane / near_plane;

        shadow_map_parameters params;
        params.cascade_splits.resize(shadowing.cascade_count);
        params.projections.resize(shadowing.cascade_count);

        // Compute splits
        // https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
        for (uint32_t cascade = 0; cascade < shadowing.cascade_count; ++cascade)
        {
            float p = (cascade + 1) / static_cast<float>(shadowing.cascade_count);
            float log = near_plane * std::pow(ratio, p);
            float uniform = near_plane + clip_range * p;
            float d = 0.95f * (log - uniform) + uniform;
            float res = (d - near_plane) / clip_range;

            params.cascade_splits[cascade] = res;
        }

        auto proj_with_clip = math::perspective(camera_data.aspect_ratio, camera_data.vertical_fov,
                                                camera_data.near_plane, camera_data.far_shadow_plane);

        auto inv_view_proj = math::inverse(proj_with_clip * _scene_data.camera.view);

        float last_split = 0.0f;
        for (uint32_t cascade = 0; cascade < shadowing.cascade_count; ++cascade)
        {
            // clang-format off
            array<math::vec3<float>, 8> frustum_corners = {
                math::vec3<float>{ -1.0f,  1.0f, 0.0f },
                math::vec3<float>{  1.0f,  1.0f, 0.0f },
                math::vec3<float>{  1.0f, -1.0f, 0.0f },
                math::vec3<float>{ -1.0f, -1.0f, 0.0f },
                math::vec3<float>{ -1.0f,  1.0f, 1.0f },
                math::vec3<float>{  1.0f,  1.0f, 1.0f },
                math::vec3<float>{  1.0f, -1.0f, 1.0f },
                math::vec3<float>{ -1.0f, -1.0f, 1.0f },
            };
            // clang-format on

            // Compute frustum corners
            for (math::vec3<float>& corner : frustum_corners)
            {
                auto inv_corner = inv_view_proj * math::vec4<float>(corner.x, corner.y, corner.z, 1.0f);
                auto normalized = inv_corner / inv_corner.w;
                corner = {normalized.x, normalized.y, normalized.z};
            }

            float split_distance = params.cascade_splits[cascade];

            for (auto idx = 0; idx < 4; ++idx)
            {
                auto edge = frustum_corners[idx + 4] - frustum_corners[idx];
                auto normalized_far = frustum_corners[idx] + edge * split_distance;
                auto normalized_near = frustum_corners[idx] + edge * last_split;

                frustum_corners[idx + 4] = normalized_far;
                frustum_corners[idx] = normalized_near;
            }

            // Compute the center of the frustum
            math::vec3<float> frustum_center = 0.0f;
            for (const auto& corner : frustum_corners)
            {
                frustum_center += corner;
            }
            frustum_center /= 8.0f;

            float radius = 0.0f;
            for (const auto& corner : frustum_corners)
            {
                float dist = math::norm(corner - frustum_center);
                radius = std::max(radius, dist);
            }

            radius = std::ceil(radius * 16.0f) / 16.0f;

            math::vec3<float> max_extents = radius;
            math::vec3<float> min_extents = -max_extents;

            // Compute light direction
            auto light_rot = math::rotate(light_transform.rotation());
            auto light_dir_xyzw = light_rot * math::vec4<float>(0.0f, 0.0f, 1.0f, 0.0f);
            auto light_dir = math::vec3<float>(light_dir_xyzw.x, light_dir_xyzw.y, light_dir_xyzw.z);

            // Light View Matrix
            auto light_view =
                math::look_at(frustum_center - light_dir * radius, frustum_center, math::vec3<float>(0.0f, 1.0f, 0.0f));
            auto light_proj = math::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y,
                                          min_extents.z - max_extents.z, 0.0f);

            params.cascade_splits[cascade] = (near_plane + split_distance * clip_range) * -1.0f;
            params.projections[cascade] = light_proj * light_view;

            last_split = params.cascade_splits[cascade];
        }

        return params;
    }
} // namespace tempest::graphics