#include <tempest/render_system.hpp>

#include <tempest/files.hpp>
#include <tempest/logger.hpp>
#include <tempest/transform_component.hpp>

#include <cstring>

namespace tempest::graphics
{
    namespace
    {
        auto log = logger::logger_factory::create({"tempest::graphics::render_system"});
    }

    render_system::render_system(ecs::registry& entities) : _allocator{64 * 1024 * 1024}, _registry{&entities}
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
            .fmt = resource_format::D32_FLOAT,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "Depth Buffer",
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
            .mag = graphics::filter::LINEAR,
            .min = graphics::filter::LINEAR,
            .mipmap = graphics::mipmap_mode::LINEAR,
            .enable_aniso = true,
            .max_anisotropy = 8.0f,
        });
        _samplers.push_back(_linear_sampler);

        gpu_camera_data camera;
        camera.position = math::vec3<float>(-5.0f, 2.0f, 0.0f);
        camera.view =
            math::look_at(camera.position, math::vec3<float>(0.0f, 2.0f, 0.0f), math::vec3<float>(0.0f, 1.0f, 0.0f));
        camera.inv_view = math::inverse(camera.view);
        camera.proj = math::perspective(16.0f / 9.0f, 90.0f * 9.0f / 16.0f, 0.1f);
        camera.inv_proj = math::inverse(camera.proj);

        _scene_data.camera = camera;
        _scene_data.sun = gpu_light{
            .color = math::vec4<float>(1.0f, 1.0f, 1.0f, 1.0f),
            .direction = math::vec3<float>(0.0f, -1.0f, 0.0f),
            .light_type = gpu_light_type::DIRECTIONAL,
        };
        _scene_data.screen_size = math::vec2<float>(1920.0f, 1080.0f);

        auto upload_pass =
            rgc->add_graph_pass("Upload Pass", queue_operation_type::TRANSFER, [&](graph_pass_builder& builder) {
                builder.add_transfer_destination_buffer(_scene_buffer)
                    .add_transfer_destination_buffer(_object_buffer)
                    .add_transfer_destination_buffer(_instance_buffer)
                    .add_transfer_destination_buffer(_indirect_buffer)
                    .add_transfer_source_buffer(_device->get_staging_buffer())
                    .on_execute([&](command_list& cmds) {
                        auto staging_buffer = _device->get_staging_buffer();
                        auto staging_buffer_data = _device->map_buffer_frame(staging_buffer);

                        std::uint32_t bytes_written = 0;

                        if (_last_updated_frame + _device->frames_in_flight() > _device->current_frame())
                        {
                            _objects.resize(_object_count);
                            _instances.resize(_object_count);
                            _indirect_draw_commands.clear();

                            for (ecs::entity ent : _registry->entities())
                            {
                                if (_registry->has<ecs::transform_component, renderable_component>(ent))
                                {
                                    auto& transform = _registry->get<ecs::transform_component>(ent);
                                    auto& renderable = _registry->get<renderable_component>(ent);

                                    auto& mesh_layout = _meshes[renderable.mesh_id];
                                    auto& material = _materials[renderable.material_id];

                                    auto& object = _objects.emplace_back(gpu_object_data{
                                        .model = transform.matrix(),
                                        .inv_tranpose_model = math::transpose(math::inverse(transform.matrix())),
                                        .mesh_id = static_cast<std::uint32_t>(renderable.mesh_id),
                                        .material_id = static_cast<std::uint32_t>(renderable.material_id),
                                        .self_id = static_cast<std::uint32_t>(renderable.object_id),
                                    });

                                    _objects[renderable.object_id] = object;

                                    _instances[renderable.object_id] = renderable.object_id;
                                }
                            }

                            auto end_opaque = std::stable_partition(
                                std::begin(_instances), std::end(_instances), [&](std::uint32_t instance) {
                                    auto& mat = _materials[_objects[instance].material_id];
                                    return mat.material_type == gpu_material_type::PBR_OPAQUE;
                                });

                            _opaque_object_count = std::distance(std::begin(_instances), end_opaque);

                            auto end_mask =
                                std::stable_partition(end_opaque, std::end(_instances), [&](std::uint32_t instance) {
                                    auto& mat = _materials[_objects[instance].material_id];
                                    return mat.material_type == gpu_material_type::PBR_MASK;
                                });

                            _mask_object_count = std::distance(end_opaque, end_mask);

                            for (auto instance : _instances)
                            {
                                auto& object = _objects[instance];

                                auto& mesh = _meshes[object.mesh_id];

                                _indirect_draw_commands.push_back({
                                    .index_count = mesh.index_count,
                                    .instance_count = 1,
                                    .first_index = (mesh.mesh_start_offset + mesh.index_offset) / 4,
                                    .vertex_offset = 0,
                                    .first_instance = object.self_id,
                                });
                            }

                            std::sort(std::begin(_instances), std::end(_instances));

                            std::memcpy(staging_buffer_data.data(), _objects.data(),
                                        _objects.size() * sizeof(gpu_object_data));
                            cmds.copy(staging_buffer, _object_buffer, bytes_written,
                                      _device->get_buffer_frame_offset(_object_buffer),
                                      static_cast<std::uint32_t>(_objects.size() * sizeof(gpu_object_data)));
                            bytes_written += static_cast<std::uint32_t>(_objects.size() * sizeof(gpu_object_data));

                            std::memcpy(staging_buffer_data.data() + bytes_written, _instances.data(),
                                        _instances.size() * sizeof(std::uint32_t));
                            cmds.copy(staging_buffer, _instance_buffer, bytes_written,
                                      _device->get_buffer_frame_offset(_instance_buffer),
                                      static_cast<std::uint32_t>(_instances.size() * sizeof(std::uint32_t)));
                            bytes_written += static_cast<std::uint32_t>(_instances.size() * sizeof(std::uint32_t));

                            std::memcpy(staging_buffer_data.data() + bytes_written, _indirect_draw_commands.data(),
                                        _indirect_draw_commands.size() * sizeof(indexed_indirect_command));
                            cmds.copy(staging_buffer, _indirect_buffer, bytes_written,
                                      _device->get_buffer_frame_offset(_indirect_buffer),
                                      static_cast<std::uint32_t>(_indirect_draw_commands.size() *
                                                                 sizeof(indexed_indirect_command)));
                            bytes_written += static_cast<std::uint32_t>(_indirect_draw_commands.size() *
                                                                        sizeof(indexed_indirect_command));
                        }

                        // upload scene data
                        if (_camera_entity != ecs::tombstone)
                        {
                            auto camera_data = _registry->get<camera_component>(_camera_entity);
                            auto camera_view = math::look_at(
                                camera_data.position, camera_data.position + camera_data.forward, camera_data.up);
                            _scene_data.camera.view = camera_view;
                            _scene_data.camera.inv_view = math::inverse(camera_view);
                            _scene_data.camera.position = camera_data.position;
                        }

                        std::memcpy(staging_buffer_data.data() + bytes_written, &_scene_data, sizeof(gpu_scene_data));
                        cmds.copy(staging_buffer, _scene_buffer, bytes_written,
                                  _device->get_buffer_frame_offset(_scene_buffer), sizeof(gpu_scene_data));
                        bytes_written += static_cast<std::uint32_t>(sizeof(gpu_scene_data));

                        _device->unmap_buffer(staging_buffer);
                    });
            });

        _pbr_opaque_pass = rgc->add_graph_pass(
            "PBR Opaque Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
                bldr.depends_on(upload_pass)
                    .add_color_attachment(color_buffer, graphics::resource_access_type::READ_WRITE,
                                          graphics::load_op::CLEAR, graphics::store_op::STORE, {0.0f, 0.0f, 0.0f, 1.0f})
                    .add_depth_attachment(depth_buffer, graphics::resource_access_type::READ_WRITE,
                                          graphics::load_op::CLEAR, graphics::store_op::DONT_CARE, 0.0f)
                    .add_constant_buffer(_scene_buffer, 0, 0)
                    .add_structured_buffer(_vertex_pull_buffer, graphics::resource_access_type::READ, 0, 1)
                    .add_structured_buffer(_mesh_layout_buffer, graphics::resource_access_type::READ, 0, 2)
                    .add_structured_buffer(_object_buffer, graphics::resource_access_type::READ, 0, 3)
                    .add_structured_buffer(_instance_buffer, graphics::resource_access_type::READ, 0, 4)
                    .add_structured_buffer(_materials_buffer, graphics::resource_access_type::READ, 0, 5)
                    .add_sampler(_linear_sampler, 0, 6, graphics::pipeline_stage::FRAGMENT)
                    .add_external_sampled_images(512, 0, 7, graphics::pipeline_stage::FRAGMENT)
                    .add_indirect_argument_buffer(_indirect_buffer)
                    .add_index_buffer(_vertex_pull_buffer)
                    .on_execute([&](graphics::command_list& cmds) {
                        cmds.set_scissor_region(0, 0, 1920, 1080)
                            .set_viewport(0, 0, 1920, 1080)
                            .use_pipeline(_pbr_opaque_pipeline)
                            .use_index_buffer(_vertex_pull_buffer, 0)
                            .draw_indexed(
                                _indirect_buffer,
                                static_cast<std::uint32_t>(_device->get_buffer_frame_offset(_indirect_buffer)),
                                static_cast<std::uint32_t>(_opaque_object_count + _mask_object_count),
                                sizeof(graphics::indexed_indirect_command));
                    });
            });

        for (auto& [win, sc_handle] : _swapchains)
        {
            auto resolve_pass = rgc->add_graph_pass(
                "Resolve Pass", queue_operation_type::GRAPHICS_AND_TRANSFER, [&](graph_pass_builder& builder) {
                    builder.add_blit_source(color_buffer)
                        .add_external_blit_target(sc_handle)
                        .depends_on(_pbr_opaque_pass)
                        // .should_execute([&, sc_handle]() { return _swapchains.contains(win); })
                        .on_execute([&, sc_handle, color_buffer](command_list& cmds) {
                            cmds.blit(color_buffer, _device->fetch_current_image(sc_handle));
                        });
                });
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

        _graph->update_external_sampled_images(_pbr_opaque_pass, _images, 0, 7, graphics::pipeline_stage::FRAGMENT);

        create_pbr_opaque_pipeline();

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
        _graph->execute();
    }

    void render_system::on_close()
    {
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

    std::vector<mesh_layout> render_system::load_mesh(std::span<core::mesh> meshes)
    {
        auto mesh_layouts = renderer_utilities::upload_meshes(*_device, meshes, _vertex_pull_buffer, _mesh_bytes);

        for (auto& mesh : mesh_layouts)
        {
            _meshes.push_back(mesh);
        }

        return mesh_layouts;
    }

    void render_system::load_textures(std::span<texture_data_descriptor> texture_sources, bool generate_mip_maps)
    {
        auto textures = renderer_utilities::upload_textures(*_device, texture_sources, _device->get_staging_buffer(),
                                                            true, generate_mip_maps);

        for (auto& tex : textures)
        {
            _images.push_back(tex);
        }
    }

    void render_system::load_material(graphics::material_payload& material)
    {
        auto mat = gpu_material_data{
            .base_color_factor = material.base_color_factor,
            .emissive_factor = math::vec3<float>(0.0f),
            .normal_scale = 1.0f,
            .metallic_factor = 1.0f,
            .roughness_factor = 1.0f,
            .alpha_cutoff = material.alpha_cutoff,
            .base_color_texture_id = material.albedo_map_id == std::numeric_limits<std::uint32_t>::max()
                                         ? -1
                                         : static_cast<std::int32_t>(texture_count() + material.albedo_map_id),
            .normal_texture_id = material.normal_map_id == std::numeric_limits<std::uint32_t>::max()
                                     ? -1
                                     : static_cast<std::int32_t>(texture_count() + material.normal_map_id),
            .metallic_roughness_texture_id =
                material.metallic_map_id == std::numeric_limits<std::uint32_t>::max()
                    ? -1
                    : static_cast<std::int32_t>(texture_count() + material.metallic_map_id),
            .emissive_texture_id = -1,
            .occlusion_texture_id = material.ao_map_id == std::numeric_limits<std::uint32_t>::max()
                                        ? -1
                                        : static_cast<std::int32_t>(texture_count() + material.ao_map_id),
            .material_type = static_cast<gpu_material_type>(material.type),
        };

        _materials.push_back(mat);
    }

    void render_system::create_pbr_opaque_pipeline()
    {
        auto vertex_shader_source = core::read_bytes("assets/shaders/pbr.vert.spv");
        auto fragment_shader_source = core::read_bytes("assets/shaders/pbr.frag.spv");

        graphics::descriptor_binding_info set0_bindings[] = {
            {
                .type = graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC,
                .binding_index = 0,
                .binding_count = 1,
            },
            {
                .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER,
                .binding_index = 1,
                .binding_count = 1,
            },
            {
                .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER,
                .binding_index = 2,
                .binding_count = 1,
            },
            {
                .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
                .binding_index = 3,
                .binding_count = 1,
            },
            {
                .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
                .binding_index = 4,
                .binding_count = 1,
            },
            {
                .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER,
                .binding_index = 5,
                .binding_count = 1,
            },
            {
                .type = graphics::descriptor_binding_type::SAMPLER,
                .binding_index = 6,
                .binding_count = 1,
            },
            {
                .type = graphics::descriptor_binding_type::SAMPLED_IMAGE,
                .binding_index = 7,
                .binding_count = 512,
            },
        };

        graphics::descriptor_set_layout_create_info layouts[] = {
            {
                .set = 0,
                .bindings = set0_bindings,
            },
        };

        graphics::resource_format color_buffer_fmt[] = {graphics::resource_format::RGBA8_SRGB};
        graphics::color_blend_attachment_state blending[] = {
            {
                .enabled = false,
            },
        };

        _pbr_opaque_pipeline = _device->create_graphics_pipeline({
            .layout{
                .set_layouts = layouts,
            },
            .target{
                .color_attachment_formats = color_buffer_fmt,
                .depth_attachment_format = graphics::resource_format::D32_FLOAT,
            },
            .vertex_shader{
                .bytes = vertex_shader_source,
                .entrypoint = "main",
                .name = "PBR Opaque Shader Module",
            },
            .fragment_shader{
                .bytes = fragment_shader_source,
                .entrypoint = "main",
                .name = "PBR Opaque Shader Module",
            },
            .depth_testing{
                .enable_test = true,
                .enable_write = true,
                .depth_test_op = graphics::compare_operation::GREATER_OR_EQUALS,
            },
            .blending{
                .attachment_blend_ops = blending,
            },
            .name = "PBR Opaque Graphics Pipeline",
        });

        _graphics_pipelines.push_back(_pbr_opaque_pipeline);
    }
} // namespace tempest::graphics