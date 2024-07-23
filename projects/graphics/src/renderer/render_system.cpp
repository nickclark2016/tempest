#include <tempest/render_system.hpp>

#include <tempest/files.hpp>
#include <tempest/imgui_context.hpp>
#include <tempest/logger.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/transform_component.hpp>

#include <cstring>

namespace tempest::graphics
{
    namespace
    {
        auto log = logger::logger_factory::create({"tempest::render_system"});

        static std::array<math::vec2<float>, 16> jitter = {{
            {0.5f, 0.333333f},
            {0.25f, 0.666667f},
            {0.75f, 0.111111f},
            {0.125000f, 0.444444f},
            {0.625000f, 0.777778f},
            {0.375000f, 0.222222f},
            {0.875000f, 0.555556f},
            {0.062500f, 0.888889f},
            {0.562500f, 0.037037f},
            {0.312500f, 0.370370f},
            {0.812500f, 0.703704f},
            {0.187500f, 0.148148f},
            {0.687500f, 0.481481f},
            {0.437500f, 0.814815f},
            {0.937500f, 0.259259f},
            {0.031250f, 0.592593f},
        }};
    } // namespace

    render_system::render_system(ecs::registry& entities, const render_system_settings& settings)
        : _allocator{64 * 1024 * 1024}, _registry{&entities}, _settings{settings}
    {
        _context = render_context::create(&_allocator);

        auto devices = _context->enumerate_suitable_devices();
        if (devices.empty())
        {
            log->critical("No suitable devices found for rendering");
            std::terminate();
        }

        log->info("Found {} suitable devices. Selecting device {}", devices.size(), devices[0].name);

        _device = &_context->create_device(0);
    }

    void render_system::register_window(iwindow& win)
    {
        auto swapchain_handle = _device->create_swapchain({
            .win = &win,
            .desired_frame_count = 3,
            .use_vsync = false,
        });

        _swapchains.emplace(&win, swapchain_handle);

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

        auto ms_color_buffer = rgc->create_image({
            .samples = sample_count::COUNT_4,
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::RGBA8_SRGB,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "MSAA Color Buffer",
        });

        auto resolved_color_buffer = rgc->create_image({
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::RGBA8_SRGB,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "Resolved Color Buffer",
        });

        auto sharpened_color_buffer = rgc->create_image({
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::RGBA8_SRGB,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "Sharpened Color Buffer",
        });

        auto history_color_buffer = rgc->create_image({
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::RGBA8_SRGB,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "History Color Buffer",
        });

        auto depth_buffer = rgc->create_image({
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::D24_FLOAT,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "Depth Buffer",
        });

        auto ms_depth_buffer = rgc->create_image({
            .samples = sample_count::COUNT_4,
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::D24_FLOAT,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "MSAA Depth Buffer",
        });

        auto velocity_buffer = rgc->create_image({
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::RG32_FLOAT,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "Velocity Buffer",
        });

        auto ms_velocity_buffer = rgc->create_image({
            .samples = sample_count::COUNT_4,
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::RG32_FLOAT,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "MSAA Velocity Buffer",
        });

        std::vector<image_resource_handle> hi_z_images;
        for (std::uint32_t i = 0; i < 5; ++i)
        {
            // compute width and height of non-power of 2
            auto width = 1920u >> i;
            auto height = 1080u >> i;

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

        auto ms_encoded_normals_buffer = rgc->create_image({
            .samples = sample_count::COUNT_4,
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

        _materials_buffer = rgc->create_buffer({
            .size = 1024 * 64 * sizeof(gpu_material_data),
            .location = memory_location::DEVICE,
            .name = "Materials Buffer",
        });

        _instance_buffer = rgc->create_buffer({
            .size = 1024 * 64 * sizeof(std::uint32_t),
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
            .color = math::vec4<float>(1.0f, 1.0f, 1.0f, 1.0f),
            .direction = math::vec3<float>(0.0f, -1.0f, 0.0f),
            .light_type = gpu_light_type::DIRECTIONAL,
        };
        _scene_data.screen_size = math::vec2<float>(1920.0f, 1080.0f);
        _scene_data.ambient_light = math::vec3<float>(0.1f, 0.1f, 0.1f);

        _hi_z_data = {
            .size = math::vec2<std::uint32_t>(1920, 1080),
            .mip_count = 5,
        };

        for (int i = 0; i < 16; ++i)
        {
            math::vec2<float>& result = jitter[i];

            result.x = ((result.x - 0.5f) / _scene_data.screen_size.x) * 2;
            result.y = ((result.y - 0.5f) / _scene_data.screen_size.y) * 2;
        }

        auto upload_pass =
            rgc->add_graph_pass("Upload Pass", queue_operation_type::TRANSFER, [&](graph_pass_builder& builder) {
                builder.add_transfer_destination_buffer(_scene_buffer)
                    .add_transfer_destination_buffer(_object_buffer)
                    .add_transfer_destination_buffer(_instance_buffer)
                    .add_transfer_destination_buffer(_indirect_buffer)
                    .add_transfer_destination_buffer(_hi_z_buffer_constants)
                    .add_transfer_source_buffer(_device->get_staging_buffer())
                    .add_host_write_buffer(_device->get_staging_buffer())
                    .on_execute([&](command_list& cmds) {
                        staging_buffer_writer writer{*_device};

                        if (_last_updated_frame + _device->frames_in_flight() > _device->current_frame())
                        {
                            std::uint32_t instances_written = 0;

                            for (const auto& [key, batch] : _draw_batches)
                            {
                                writer.write(cmds,
                                             span<const gpu_object_data>{batch.objects.values(),
                                                                         batch.objects.values() + batch.objects.size()},
                                             _object_buffer, instances_written * sizeof(gpu_object_data));
                                writer.write(cmds, span<const indexed_indirect_command>{batch.commands},
                                             _indirect_buffer, instances_written * sizeof(indexed_indirect_command));

                                std::vector<std::uint32_t> instances(batch.objects.size());

                                std::iota(instances.begin(), instances.end(), instances_written);
                                writer.write(cmds, span<const std::uint32_t>{instances}, _instance_buffer,
                                             instances_written * sizeof(std::uint32_t));

                                instances_written += static_cast<std::uint32_t>(batch.objects.size());
                            }
                        }

                        // upload scene data
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

                        if (_settings.aa_mode == anti_aliasing_mode::TAA)
                        {
                            auto jitter_value = jitter[_device->current_frame() % 16];
                            _scene_data.jitter.x = jitter_value.x * 0.85f;
                            _scene_data.jitter.y = jitter_value.y * 0.85f;
                        }
                        else
                        {
                            _scene_data.jitter = math::vec4<float>(0.0f, 0.0f, 0.0f, 0.0f);
                        }
                        _scene_data.jitter.z = 0.0f;
                        _scene_data.jitter.w = 0.0f;

                        writer.write(cmds, span<const gpu_scene_data>{&_scene_data, static_cast<size_t>(1)},
                                     _scene_buffer);

                        writer.write(cmds, span<const hi_z_data>{&_hi_z_data, static_cast<size_t>(1)},
                                     _hi_z_buffer_constants);

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
                    .add_constant_buffer(_scene_buffer, 0, 0)
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
                                        static_cast<std::uint32_t>(_device->get_buffer_frame_offset(_indirect_buffer)),
                                        static_cast<std::uint32_t>(batch.objects.size()),
                                        sizeof(indexed_indirect_command));
                            }
                        }
                    });
            });

        _z_prepass_msaa_pass =
            rgc->add_graph_pass("Z Pre Pass", queue_operation_type::GRAPHICS, [&](graph_pass_builder& bldr) {
                bldr.depends_on(upload_pass)
                    .add_depth_attachment(ms_depth_buffer, resource_access_type::READ_WRITE, load_op::CLEAR,
                                          store_op::STORE, 0.0f)
                    .add_color_attachment(ms_encoded_normals_buffer, resource_access_type::WRITE, load_op::CLEAR,
                                          store_op::STORE, {0.0f, 0.0f, 0.0f, 0.0f})
                    .add_constant_buffer(_scene_buffer, 0, 0)
                    .add_structured_buffer(_vertex_pull_buffer, resource_access_type::READ, 0, 1)
                    .add_structured_buffer(_mesh_layout_buffer, resource_access_type::READ, 0, 2)
                    .add_structured_buffer(_object_buffer, resource_access_type::READ, 0, 3)
                    .add_structured_buffer(_instance_buffer, resource_access_type::READ, 0, 4)
                    .add_structured_buffer(_materials_buffer, resource_access_type::READ, 0, 5)
                    .add_sampler(_linear_sampler, 0, 6, pipeline_stage::FRAGMENT)
                    .add_external_sampled_images(512, 0, 7, pipeline_stage::FRAGMENT)
                    .add_indirect_argument_buffer(_indirect_buffer)
                    .add_index_buffer(_vertex_pull_buffer)
                    .resolve_image(ms_encoded_normals_buffer, encoded_normals_buffer)
                    .resolve_image(ms_depth_buffer, depth_buffer)
                    .should_execute([this]() { return _settings.aa_mode == anti_aliasing_mode::MSAA; })
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
                                        static_cast<std::uint32_t>(_device->get_buffer_frame_offset(_indirect_buffer)),
                                        static_cast<std::uint32_t>(batch.objects.size()),
                                        sizeof(indexed_indirect_command));
                            }
                        }
                    });
            });

        auto build_hi_z_pass =
            rgc->add_graph_pass("Build Hi Z Pass", queue_operation_type::COMPUTE, [&](graph_pass_builder& bldr) {
                bldr.depends_on(_z_prepass_pass)
                    .depends_on(_z_prepass_msaa_pass)
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

            std::uint32_t draw_calls_issued = 0;

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
                    static_cast<std::uint32_t>(_device->get_buffer_frame_offset(_indirect_buffer) +
                                               draw_calls_issued * sizeof(indexed_indirect_command)),
                    static_cast<std::uint32_t>(batch.objects.size()), sizeof(indexed_indirect_command));

                draw_calls_issued += static_cast<std::uint32_t>(batch.commands.size());
            }
        };

        _pbr_pass = rgc->add_graph_pass("PBR Pass", queue_operation_type::GRAPHICS, [&](graph_pass_builder& bldr) {
            bldr.depends_on(_z_prepass_pass)
                .depends_on(build_hi_z_pass)
                .depends_on(upload_pass)
                .add_color_attachment(color_buffer, resource_access_type::READ_WRITE, load_op::CLEAR, store_op::STORE,
                                      {0.5f, 1.0f, 1.0f, 1.0f})
                .add_color_attachment(velocity_buffer, resource_access_type::READ_WRITE, load_op::CLEAR,
                                      store_op::STORE, {0.0f, 0.0f, 0.0f, 0.0f})
                .add_depth_attachment(depth_buffer, resource_access_type::READ_WRITE, load_op::LOAD,
                                      store_op::DONT_CARE)
                .add_constant_buffer(_scene_buffer, 0, 0)
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
                .on_execute(pbr_commands);
        });

        _pbr_msaa_pass =
            rgc->add_graph_pass("PBR MSAA Pass", queue_operation_type::GRAPHICS, [&](graph_pass_builder& bldr) {
                bldr.depends_on(_z_prepass_msaa_pass)
                    .depends_on(build_hi_z_pass)
                    .depends_on(upload_pass)
                    .add_color_attachment(ms_color_buffer, resource_access_type::READ_WRITE, load_op::CLEAR,
                                          store_op::STORE, {0.5f, 1.0f, 1.0f, 1.0f})
                    .add_color_attachment(ms_velocity_buffer, resource_access_type::READ_WRITE, load_op::CLEAR,
                                          store_op::STORE, {0.0f, 0.0f, 0.0f, 0.0f})
                    .add_depth_attachment(ms_depth_buffer, resource_access_type::READ_WRITE, load_op::LOAD,
                                          store_op::DONT_CARE)
                    .add_constant_buffer(_scene_buffer, 0, 0)
                    .add_structured_buffer(_vertex_pull_buffer, resource_access_type::READ, 0, 1)
                    .add_structured_buffer(_mesh_layout_buffer, resource_access_type::READ, 0, 2)
                    .add_structured_buffer(_object_buffer, resource_access_type::READ, 0, 3)
                    .add_structured_buffer(_instance_buffer, resource_access_type::READ, 0, 4)
                    .add_structured_buffer(_materials_buffer, resource_access_type::READ, 0, 5)
                    .add_sampler(_linear_sampler, 0, 6, pipeline_stage::FRAGMENT)
                    .add_external_sampled_images(512, 0, 7, pipeline_stage::FRAGMENT)
                    .add_indirect_argument_buffer(_indirect_buffer)
                    .add_index_buffer(_vertex_pull_buffer)
                    .resolve_image(ms_color_buffer, color_buffer)
                    .resolve_image(ms_velocity_buffer, velocity_buffer)
                    .resolve_image(ms_depth_buffer, depth_buffer)
                    .should_execute([this]() { return _settings.aa_mode == anti_aliasing_mode::MSAA; })
                    .on_execute(pbr_commands);
            });

        auto first_frame_taa_copy_pass = rgc->add_graph_pass(
            "TAA Build Initial History", queue_operation_type::GRAPHICS_AND_TRANSFER, [&](graph_pass_builder& bldr) {
                bldr.depends_on(_pbr_pass)
                    .add_blit_target(history_color_buffer)
                    .add_blit_source(color_buffer)
                    .should_execute([&]() {
                        return _device->current_frame() == 0 && _settings_dirty &&
                               _settings.aa_mode == anti_aliasing_mode::TAA;
                    })
                    .on_execute([history_color_buffer, color_buffer](command_list& cmds) {
                        cmds.blit(color_buffer, history_color_buffer);
                    });
            });

        auto taa_resolve_pass =
            rgc->add_graph_pass("TAA Resolve", queue_operation_type::GRAPHICS, [&](graph_pass_builder& bldr) {
                bldr.depends_on(_pbr_pass)
                    .depends_on(first_frame_taa_copy_pass)
                    .add_color_attachment(resolved_color_buffer, resource_access_type::READ_WRITE, load_op::CLEAR,
                                          store_op::STORE, {0.0f, 0.0f, 0.0f, 0.0f})
                    .add_sampled_image(color_buffer, 0, 0)
                    .add_sampled_image(history_color_buffer, 0, 1)
                    .add_sampled_image(velocity_buffer, 0, 2)
                    .add_sampler(_point_sampler_no_aniso, 0, 3, pipeline_stage::FRAGMENT)
                    .add_sampler(_linear_sampler_no_aniso, 0, 4, pipeline_stage::FRAGMENT)
                    .should_execute([this]() { return _settings.aa_mode == anti_aliasing_mode::TAA; })
                    .on_execute([&](command_list& cmds) { cmds.use_pipeline(_taa_resolve_handle).draw(3, 1, 0, 0); });
            });

        auto sharpening_pass =
            rgc->add_graph_pass("Sharpen Pass", queue_operation_type::GRAPHICS, [&](graph_pass_builder& bldr) {
                bldr.depends_on(taa_resolve_pass)
                    .add_color_attachment(sharpened_color_buffer, resource_access_type::READ_WRITE, load_op::LOAD,
                                          store_op::STORE)
                    .add_sampled_image(resolved_color_buffer, 0, 0)
                    .should_execute([this]() { return _settings.aa_mode == anti_aliasing_mode::TAA; })
                    .on_execute([&](command_list& cmds) { cmds.use_pipeline(_sharpen_handle).draw(3, 1, 0, 0); });
            });

        auto imgui_pass = rgc->add_graph_pass(
            "ImGUI Graph Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
                bldr.add_color_attachment(color_buffer, graphics::resource_access_type::WRITE, graphics::load_op::LOAD,
                                          graphics::store_op::STORE)
                    .draw_imgui()
                    .depends_on(_pbr_pass)
                    .depends_on(_pbr_msaa_pass)
                    .should_execute([this]() {
                        return _settings.aa_mode != anti_aliasing_mode::TAA && _settings.enable_imgui &&
                               _create_imgui_hierarchy;
                    })
                    .on_execute([]([[maybe_unused]] auto& cmds) {});
            });

        auto imgui_pass_with_taa = rgc->add_graph_pass(
            "ImGUI Graph Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
                bldr.add_color_attachment(sharpened_color_buffer, graphics::resource_access_type::WRITE,
                                          graphics::load_op::LOAD, graphics::store_op::STORE)
                    .draw_imgui()
                    .depends_on(sharpening_pass)
                    .should_execute([this]() {
                        return _settings.aa_mode == anti_aliasing_mode::TAA && _settings.enable_imgui &&
                               _create_imgui_hierarchy;
                    })
                    .on_execute([](auto& cmds) {});
            });

        for (auto& [win, sc_handle] : _swapchains)
        {
            rgc->add_graph_pass("Swapchain Resolve", queue_operation_type::GRAPHICS_AND_TRANSFER,
                                [&](graph_pass_builder& builder) {
                                    builder.add_external_blit_target(sc_handle)
                                        .add_blit_source(color_buffer)
                                        .depends_on(imgui_pass)
                                        .depends_on(_pbr_msaa_pass)
                                        .depends_on(_pbr_pass)
                                        .should_execute([this]() {
                                            return _settings.aa_mode == anti_aliasing_mode::NONE ||
                                                   _settings.aa_mode == anti_aliasing_mode::MSAA;
                                        })
                                        .on_execute([&, sc_handle, color_buffer](command_list& cmds) {
                                            cmds.blit(color_buffer, _device->fetch_current_image(sc_handle));
                                        });
                                });

            rgc->add_graph_pass(
                "Swapchain Resolve", queue_operation_type::GRAPHICS_AND_TRANSFER, [&](graph_pass_builder& builder) {
                    builder.add_blit_source(sharpened_color_buffer)
                        .add_external_blit_target(sc_handle)
                        .add_blit_target(history_color_buffer)
                        .depends_on(imgui_pass_with_taa)
                        .should_execute([this]() { return _settings.aa_mode == anti_aliasing_mode::TAA; })
                        .on_execute([&, sc_handle, sharpened_color_buffer, history_color_buffer](command_list& cmds) {
                            cmds.blit(sharpened_color_buffer, _device->fetch_current_image(sc_handle));
                            cmds.blit(sharpened_color_buffer, history_color_buffer);
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

        _graph = std::move(*rgc).compile();
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
        _graph->update_external_sampled_images(_pbr_msaa_pass, _images, 0, 7, pipeline_stage::FRAGMENT);
        _graph->update_external_sampled_images(_z_prepass_msaa_pass, _images, 0, 7, pipeline_stage::FRAGMENT);

        _pbr_opaque_pipeline = create_pbr_pipeline(false);
        _pbr_transparencies_pipeline = create_pbr_pipeline(true);
        _z_prepass_pipeline = create_z_prepass_pipeline();
        _hzb_build_pipeline = create_hzb_build_pipeline();
        _taa_resolve_handle = create_taa_resolve_pipeline();
        _sharpen_handle = create_sharpen_pipeline();

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
                    .model = transform.matrix(),
                    .inv_tranpose_model = math::transpose(math::inverse(transform.matrix())),
                    .mesh_id = static_cast<std::uint32_t>(renderable.mesh_id),
                    .material_id = static_cast<std::uint32_t>(renderable.material_id),
                    .self_id = static_cast<std::uint32_t>(renderable.object_id),
                };

                if (const auto relationship = _registry->try_get<ecs::relationship_component<ecs::entity>>(ent))
                {
                    if (const auto parent_transform =
                            _registry->try_get<ecs::transform_component>(relationship->parent))
                    {
                        object_payload.model = parent_transform->matrix() * object_payload.model;
                        object_payload.inv_tranpose_model = math::transpose(math::inverse(object_payload.model));
                    }
                }

                draw_batch_key key = {
                    .alpha_type = static_cast<alpha_behavior>(_materials[renderable.material_id].material_type),
                };

                auto& draw_batch = _draw_batches[key];
                auto& mesh = _meshes[renderable.mesh_id];

                auto object_data_it = draw_batch.objects.find(ent);
                if (object_data_it == draw_batch.objects.end()) [[unlikely]]
                {
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
                    .first_instance = static_cast<std::uint32_t>(draw_batch.objects.index_of(ent)),
                });
            }

            // Iterate through the draw batches and update first instance based on the number of instances in the
            // previous batches
            std::uint32_t instances_written = 0;
            for (auto [_, batch] : _draw_batches)
            {
                for (auto& cmd : batch.commands)
                {
                    cmd.first_instance += instances_written;
                }

                instances_written += static_cast<std::uint32_t>(batch.objects.size());
            }
        }

        if (_create_imgui_hierarchy && _settings.enable_imgui)
        {
            imgui_context::create_frame([this]() {
                _create_imgui_hierarchy();
                _graph->show_gpu_profiling();
            });
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

    vector<mesh_layout> render_system::load_mesh(span<core::mesh> meshes)
    {
        auto mesh_layouts = renderer_utilities::upload_meshes(*_device, meshes, _vertex_pull_buffer, _mesh_bytes);

        for (auto& mesh : mesh_layouts)
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
            .base_color_texture_id = material.albedo_map_id == std::numeric_limits<std::uint32_t>::max()
                                         ? gpu_material_data::INVALID_TEXTURE_ID
                                         : static_cast<std::int16_t>(texture_count() + material.albedo_map_id),
            .normal_texture_id = material.normal_map_id == std::numeric_limits<std::uint32_t>::max()
                                     ? gpu_material_data::INVALID_TEXTURE_ID
                                     : static_cast<std::int16_t>(texture_count() + material.normal_map_id),
            .metallic_roughness_texture_id =
                material.metallic_map_id == std::numeric_limits<std::uint32_t>::max()
                    ? gpu_material_data::INVALID_TEXTURE_ID
                    : static_cast<std::int16_t>(texture_count() + material.metallic_map_id),
            .emissive_texture_id = material.emissive_map_id == std::numeric_limits<std::uint32_t>::max()
                                       ? gpu_material_data::INVALID_TEXTURE_ID
                                       : static_cast<std::int16_t>(texture_count() + material.emissive_map_id),
            .occlusion_texture_id = material.ao_map_id == std::numeric_limits<std::uint32_t>::max()
                                        ? gpu_material_data::INVALID_TEXTURE_ID
                                        : static_cast<std::int16_t>(texture_count() + material.ao_map_id),
            .material_type = static_cast<gpu_material_type>(material.type),
        };

        _materials.push_back(mat);
    }

    void render_system::mark_dirty()
    {
        _static_data_dirty = true;
    }

    graphics_pipeline_resource_handle render_system::create_pbr_pipeline(bool enable_blend)
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/pbr.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/pbr.frag.spv");

        descriptor_binding_info set0_bindings[] = {
            {
                .type = descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC,
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

        resource_format color_buffer_fmt[] = {
            resource_format::RGBA8_SRGB,
            resource_format::RG32_FLOAT,
        };

        color_blend_attachment_state blending[] = {{
                                                       // Color Buffer
                                                       .enabled = false,
                                                   },
                                                   {
                                                       // Velocity Buffer
                                                       .enabled = false,
                                                   }};

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
                .type = descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC,
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
            },
            .blending{
                .attachment_blend_ops = blending,
            },
            .name = "Sharpen Pipeline",
        });

        _graphics_pipelines.push_back(pipeline);

        return pipeline;
    }
} // namespace tempest::graphics