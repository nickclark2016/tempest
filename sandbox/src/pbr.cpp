#include <tempest/files.hpp>
#include <tempest/imgui_context.hpp>
#include <tempest/input.hpp>
#include <tempest/memory.hpp>
#include <tempest/mesh_asset.hpp>
#include <tempest/mesh_component.hpp>
#include <tempest/render_camera.hpp>
#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>
#include <tempest/transformations.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <ranges>
#include <utility>

using namespace tempest;

namespace
{
    inline constexpr std::size_t global_memory_allocator_size = 1024 * 1024 * 64;
}

struct pbr_scene_constants
{
    graphics::render_camera camera;
    graphics::directional_light sun;
    math::vec2<float> screen_size;
};

struct ssao_constants
{
    math::mat4<float> projection;
    math::mat4<float> inv_projection;
    math::mat4<float> view_matrix;
    math::mat4<float> inv_view;
    std::array<math::vec4<float>, 64> kernel;
    math::vec2<float> noise_scale;
    float radius;
    float bias;
};

graphics::material_type convert_material_type(assets::material_type type);
graphics::graphics_pipeline_resource_handle create_z_pass_pipeline(graphics::render_device& device);
graphics::graphics_pipeline_resource_handle create_ssao_pipeline(graphics::render_device& device);
graphics::graphics_pipeline_resource_handle create_ssao_blur_pipeline(graphics::render_device& device);
graphics::graphics_pipeline_resource_handle create_pbr_pipeline(graphics::render_device& device);

void pbr_demo()
{
    auto global_allocator = core::heap_allocator(global_memory_allocator_size);

    auto graphics_ctx = graphics::render_context::create(&global_allocator);

    auto devices = graphics_ctx->enumerate_suitable_devices();

    std::uint32_t id = std::numeric_limits<std::uint32_t>::max();

    if (devices.size() > 1)
    {
        std::cout << "Found Suitable Devices:\n";

        for (auto& device : devices)
        {
            std::cout << device.id << " " << device.name << "\n";
        }
        std::cout << "Found multiple suitable rendering devices. Select device: ";

        std::cin >> id;
        if (id >= devices.size() || !std::cin.good())
        {
            std::cerr << "Invalid Device Selected.";
            std::exit(EXIT_FAILURE);
        }
    }
    else if (devices.size() == 1)
    {
        std::cout << "Found single suitable rendering device: " << devices[0].name << "\n";
        id = 0;
    }
    else
    {
        std::cerr << "Found no suitable rendering devices. Exiting.";
        std::exit(EXIT_FAILURE);
    }

    auto& graphics_device = graphics_ctx->create_device(id);
    auto win = graphics::window_factory::create({
        .title{"Tempest Render Graph Demo"},
        .width{1920},
        .height{1080},
    });
    graphics::imgui_context::initialize_for_window(*win);

    auto swapchain = graphics_device.create_swapchain({.win{win.get()}, .desired_frame_count{3}});

    auto rgc = graphics::render_graph_compiler::create_compiler(&global_allocator, &graphics_device);

    auto constants_buffer = rgc->create_buffer({
        .size = sizeof(pbr_scene_constants),
        .location = graphics::memory_location::DEVICE,
        .name = "PBR Scene Constants",
        .per_frame_memory = true,
    });

    auto ssao_constants_buffer = rgc->create_buffer({
        .size = sizeof(ssao_constants),
        .location = graphics::memory_location::DEVICE,
        .name = "SSAO Constants",
        .per_frame_memory = true,
    });

    auto point_lights_buffer = rgc->create_buffer({
        .size = sizeof(graphics::point_light) * 4096,
        .location = graphics::memory_location::DEVICE,
        .name = "Point Lights Buffer",
        .per_frame_memory = true,
    });

    auto vertex_pull_buffer = rgc->create_buffer({
        .size = 1024 * 1024 * 512,
        .location = graphics::memory_location::DEVICE,
        .name = "Vertex Pull Buffer",
        .per_frame_memory = false,
    });

    auto mesh_layout_buffer = rgc->create_buffer({
        .size = sizeof(graphics::mesh_layout) * 4096,
        .location = graphics::memory_location::DEVICE,
        .name = "Mesh Layout Buffer",
        .per_frame_memory = false,
    });

    auto object_data_buffer = rgc->create_buffer({
        .size = sizeof(graphics::object_payload) * 64 * 1024,
        .location = graphics::memory_location::DEVICE,
        .name = "Object Payload Buffer",
        .per_frame_memory = true,
    });

    auto instance_data_buffer = rgc->create_buffer({
        .size = sizeof(std::uint32_t) * 64 * 1024,
        .location = graphics::memory_location::DEVICE,
        .name = "Instance Buffer",
        .per_frame_memory = true,
    });

    auto material_buffer = rgc->create_buffer({
        .size = sizeof(graphics::material_payload) * 1024,
        .location = graphics::memory_location::DEVICE,
        .name = "Material Payload Buffer",
        .per_frame_memory = false,
    });

    auto indirect_commands = rgc->create_buffer({
        .size = sizeof(graphics::indexed_indirect_command) * 4096,
        .location = graphics::memory_location::HOST,
        .name = "Indirect Arguments",
        .per_frame_memory = true,
    });

    auto color_buffer = rgc->create_image({
        .width{1920},
        .height{1080},
        .fmt{graphics::resource_format::RGBA8_SRGB},
        .type{graphics::image_type::IMAGE_2D},
        .name{"Color Buffer Target"},
    });

    auto ssao_buffer = rgc->create_image({
        .width{1920},
        .height{1080},
        .fmt{graphics::resource_format::R8_UNORM},
        .type{graphics::image_type::IMAGE_2D},
        .name{"SSAO Buffer Target"},
    });

    auto ssao_blur_buffer = rgc->create_image({
        .width{1920},
        .height{1080},
        .fmt{graphics::resource_format::R8_UNORM},
        .type{graphics::image_type::IMAGE_2D},
        .name{"SSAO Blurred Buffer Target"},
    });

    auto normals_buffer = rgc->create_image({
        .width = 1920,
        .height = 1080,
        .fmt = graphics::resource_format::RGBA8_UNORM,
        .type = graphics::image_type::IMAGE_2D,
        .name = "Encoded Normals",
    });

    auto specular_smoothness_buffer = rgc->create_image({
        .width = 1920,
        .height = 1080,
        .fmt = graphics::resource_format::RGBA8_UNORM,
        .type = graphics::image_type::IMAGE_2D,
        .name = "Encoded Specular Smoothness",
    });

    auto depth_buffer = rgc->create_image({
        .width{1920},
        .height{1080},
        .fmt{graphics::resource_format::D32_FLOAT},
        .type{graphics::image_type::IMAGE_2D},
        .name{"Depth Buffer Target"},
    });

    rgc->enable_imgui();

    auto scene = assets::load_scene("assets/glTF-Sample-Assets/Models/Sponza/GLTF/Sponza.gltf");
    std::vector<graphics::texture_data_descriptor> cube_scene_texture_decs;
    for (auto& tex_asset : scene->textures)
    {
        graphics::texture_data_descriptor desc{
            .fmt = tex_asset.linear ? graphics::resource_format::RGBA8_UNORM : graphics::resource_format::RGBA8_SRGB,
            .mips{
                {
                    {
                        .width = tex_asset.width,
                        .height = tex_asset.height,
                        .bytes = tex_asset.data,
                    },
                },
            },
        };

        cube_scene_texture_decs.push_back(desc);
    }

    auto textures = graphics::renderer_utilities::upload_textures(graphics_device, cube_scene_texture_decs,
                                                                  graphics_device.get_staging_buffer(), true, true);
    textures.resize(512);
    
    auto linear_sampler = graphics_device.create_sampler({
        .mag = graphics::filter::LINEAR,
        .min = graphics::filter::LINEAR,
        .mipmap = graphics::mipmap_mode::LINEAR,
        .enable_aniso = true,
        .max_anisotropy{16.0f},
    });

    auto linear_no_aniso_sampler = graphics_device.create_sampler({
        .mag = graphics::filter::LINEAR,
        .min = graphics::filter::LINEAR,
        .mipmap = graphics::mipmap_mode::LINEAR,
        .enable_aniso = false,
    });

    unsigned int noise_size = 4;
    unsigned int kernel_size = 64;

    auto nearest_sampler = graphics_device.create_sampler({
        .mag = graphics::filter::NEAREST,
        .min = graphics::filter::NEAREST,
        .mipmap = graphics::mipmap_mode::NEAREST,
    });

    cube_scene_texture_decs.clear();

    pbr_scene_constants scene_data{
        .camera{
            .proj = math::perspective(16.0f / 9.0f, 90.0f * 9.0f / 16.0f, 0.1f),
            .view = math::look_at(math::vec3<float>(4.0f, 2.5f, 0.0f), math::vec3<float>(-1.0f, 2.5f, 0.0f),
                                  math::vec3<float>(0.0f, 1.0, 0.0f)),
            .view_proj = 1.0f,
            .eye_position = {4.0f, 2.5f, 0.0f},
        },
        .sun{
            .light_direction{0.0f, -1.0f, 0.0f},
            .color_illum{1.0f, 1.0f, 1.0f, 1.0f},
        },
        .screen_size{
            1920.0f,
            1080.0f,
        },
    };

    ssao_constants ssao_consts{
        .projection = scene_data.camera.proj,
        .inv_projection = math::inverse(scene_data.camera.proj),
        .view_matrix = scene_data.camera.view,
        .inv_view = math::inverse(scene_data.camera.view),
        .noise_scale = {1920.0f / noise_size, 1080.0f / noise_size},
        .radius = 0.5f,
        .bias = 0.025f,
    };

    std::default_random_engine engine(0);
    std::uniform_real_distribution<float> float_generator(0.0f, 1.0f);

    auto our_lerp = [](auto a, auto b, auto f) { return a + f * (b - a); };

    for (auto i : std::ranges::iota_view(0u, kernel_size))
    {
        float x = float_generator(engine) * 2.0f - 1.0f;
        float y = float_generator(engine) * 2.0f - 1.0f;
        float z = float_generator(engine);

        auto sample = math::normalize(math::vec3(x, y, z));
        sample *= float_generator(engine);
        auto scale = static_cast<float>(i) / kernel_size;
        scale = our_lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;

        ssao_consts.kernel[i] = math::vec4(sample.x, sample.y, sample.z, 1.0f);
    }

    std::vector<std::byte> noise_bytes(sizeof(float) * 2 * noise_size * noise_size);
    for (std::size_t i = 0; i < noise_bytes.size() / sizeof(float); i += 2)
    {
        float r = float_generator(engine) * 2.0f - 1.0f;
        float g = float_generator(engine) * 2.0f - 1.0f;
        math::vec2<float> rgba = math::vec2(r, g);
        std::memcpy(noise_bytes.data() + 4 * i, &rgba, sizeof(rgba));
    }

    std::vector<graphics::texture_mip_descriptor> noise_tex_mip_desc = {
        {
            .width = noise_size,
            .height = noise_size,
            .bytes = noise_bytes,
        },
    };

    graphics::texture_data_descriptor noise_tex_data_desc[] = {
        {
            .fmt = graphics::resource_format::RG32_FLOAT,
            .mips = noise_tex_mip_desc,
            .name = "SSAO Noise Texture",
        },
    };

    auto noise_texture_handle = graphics::renderer_utilities::upload_textures(
        graphics_device, noise_tex_data_desc, graphics_device.get_staging_buffer(), true)[0];

    std::vector<graphics::object_payload> objects;
    std::vector<graphics::mesh_layout> mesh_layouts;
    std::vector<graphics::material_payload> materials;
    std::vector<graphics::indexed_indirect_command> indirect_draw_commands;
    std::vector<std::uint32_t> instances;

    for (auto& node : scene->nodes)
    {
        math::mat4<float> parent_transform(1.0f);

        std::uint32_t parent = node.parent;
        while (parent != std::numeric_limits<std::uint32_t>::max())
        {
            auto& ancestor = scene->nodes[parent];
            auto ancestor_transform = math::transform(ancestor.position, ancestor.rotation, ancestor.scale);
            parent_transform = ancestor_transform * parent_transform;
            parent = ancestor.parent;
        }

        auto mesh_id = node.mesh_id;
        auto material = scene->meshes[mesh_id].material_id;
        auto transform = parent_transform * math::transform(node.position, node.rotation, node.scale);
        auto inv_transform = math::transpose(math::inverse(transform));

        if (mesh_id == std::numeric_limits<std::uint32_t>::max())
        {
            continue;
        }

        objects.push_back({
            .transform = transform,
            .inv_transform = inv_transform,
            .mesh_id = mesh_id,
            .material_id = material,
            .parent_id = node.parent,
            .self_id = static_cast<std::uint32_t>(objects.size()),
        });

        instances.push_back(static_cast<std::uint32_t>(instances.size()));
    }

    for (auto& mat : scene->materials)
    {
        materials.push_back(graphics::material_payload{
            .type = convert_material_type(mat.type),
            .albedo_map_id = mat.base_color_texture,
            .normal_map_id = mat.normal_map_texture,
            .metallic_map_id = mat.metallic_roughness_texture,
            .roughness_map_id = mat.metallic_roughness_texture,
            .ao_map_id = mat.occlusion_map_texture,
            .base_color_factor = mat.base_color_factor,
        });
    }

    std::size_t opaque_count = 0;
    std::size_t mask_count = 0;

    auto pbr_opaque = create_pbr_pipeline(graphics_device);
    auto z_pass = create_z_pass_pipeline(graphics_device);
    auto ssao_pipeline = create_ssao_pipeline(graphics_device);
    auto ssao_blur_pipeline = create_ssao_blur_pipeline(graphics_device);

    auto upload_pass = rgc->add_graph_pass(
        "Upload Pass", graphics::queue_operation_type::TRANSFER, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_transfer_destination_buffer(constants_buffer)
                .add_transfer_destination_buffer(object_data_buffer)
                .add_transfer_destination_buffer(instance_data_buffer)
                .add_transfer_destination_buffer(ssao_constants_buffer)
                .on_execute([&](graphics::command_list& cmds) {
                    auto staging_buffer = graphics_device.get_staging_buffer();
                    auto staging_buffer_ptr = graphics_device.map_buffer_frame(staging_buffer);

                    std::size_t write_offset = 0;
                    std::memcpy(staging_buffer_ptr.data(), &scene_data, sizeof(pbr_scene_constants));

                    cmds.copy(staging_buffer, constants_buffer, graphics_device.get_buffer_frame_offset(staging_buffer),
                              graphics_device.get_buffer_frame_offset(constants_buffer), sizeof(pbr_scene_constants));

                    write_offset += sizeof(pbr_scene_constants);

                    std::memcpy(staging_buffer_ptr.data() + write_offset, objects.data(),
                                sizeof(graphics::object_payload) * objects.size());
                    cmds.copy(staging_buffer, object_data_buffer,
                              graphics_device.get_buffer_frame_offset(staging_buffer) + write_offset,
                              graphics_device.get_buffer_frame_offset(object_data_buffer),
                              sizeof(graphics::object_payload) * objects.size());

                    write_offset += sizeof(graphics::object_payload) * objects.size();

                    std::memcpy(staging_buffer_ptr.data() + write_offset, instances.data(),
                                sizeof(std::uint32_t) * instances.size());
                    cmds.copy(staging_buffer, instance_data_buffer,
                              graphics_device.get_buffer_frame_offset(staging_buffer) + write_offset,
                              graphics_device.get_buffer_frame_offset(instance_data_buffer),
                              sizeof(std::uint32_t) * instances.size());

                    write_offset += sizeof(std::uint32_t) * instances.size();

                    std::memcpy(staging_buffer_ptr.data() + write_offset, &ssao_consts, sizeof(ssao_constants));
                    cmds.copy(staging_buffer, ssao_constants_buffer,
                              graphics_device.get_buffer_frame_offset(staging_buffer) + write_offset,
                              graphics_device.get_buffer_frame_offset(ssao_constants_buffer), sizeof(ssao_constants));

                    write_offset += sizeof(ssao_constants);

                    graphics_device.unmap_buffer(staging_buffer);

                    auto cmds_ptr = graphics_device.map_buffer_frame(indirect_commands);
                    std::memcpy(cmds_ptr.data(), indirect_draw_commands.data(),
                                sizeof(graphics::indexed_indirect_command) * indirect_draw_commands.size());
                    graphics_device.unmap_buffer(indirect_commands);
                });
        });

    auto depth_pre_pass = rgc->add_graph_pass(
        "Z Pre Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.depends_on(upload_pass)
                .add_color_attachment(normals_buffer, graphics::resource_access_type::WRITE)
                .add_depth_attachment(depth_buffer, graphics::resource_access_type::READ_WRITE,
                                      graphics::load_op::CLEAR, graphics::store_op::STORE, 0.0f)
                .add_constant_buffer(constants_buffer, 0, 0)
                .add_structured_buffer(vertex_pull_buffer, graphics::resource_access_type::READ, 0, 2)
                .add_structured_buffer(mesh_layout_buffer, graphics::resource_access_type::READ, 0, 3)
                .add_structured_buffer(object_data_buffer, graphics::resource_access_type::READ, 0, 4)
                .add_structured_buffer(instance_data_buffer, graphics::resource_access_type::READ, 0, 5)
                .add_structured_buffer(material_buffer, graphics::resource_access_type::READ, 0, 6)
                .add_sampler(linear_no_aniso_sampler, 0, 7, graphics::pipeline_stage::FRAGMENT)
                .add_external_sampled_images(textures, 0, 9, graphics::pipeline_stage::FRAGMENT)
                .add_index_buffer(vertex_pull_buffer)
                .add_indirect_argument_buffer(indirect_commands)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.set_scissor_region(0, 0, 1920, 1080)
                        .set_viewport(0, 0, 1920, 1080)
                        .use_pipeline(z_pass)
                        .use_index_buffer(vertex_pull_buffer, 0)
                        .draw_indexed(
                            indirect_commands,
                            static_cast<std::uint32_t>(graphics_device.get_buffer_frame_offset(indirect_commands)),
                            static_cast<std::uint32_t>(opaque_count + mask_count),
                            sizeof(graphics::indexed_indirect_command));
                });
        });

    auto ssao_pass = rgc->add_graph_pass(
        "SSAO Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.depends_on(depth_pre_pass)
                .add_color_attachment(ssao_buffer, graphics::resource_access_type::WRITE)
                .add_constant_buffer(ssao_constants_buffer, 0, 0)
                .add_sampled_image(depth_buffer, 0, 1)
                .add_external_sampled_image(noise_texture_handle, 0, 2, graphics::pipeline_stage::FRAGMENT)
                .add_sampled_image(normals_buffer, 0, 4)
                .add_sampler(nearest_sampler, 0, 5, graphics::pipeline_stage::FRAGMENT)
                .add_sampler(linear_no_aniso_sampler, 0, 6, graphics::pipeline_stage::FRAGMENT)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.set_scissor_region(0, 0, 1920, 1080)
                        .set_viewport(0, 0, 1920, 1080, 0, 1, 0, false)
                        .use_pipeline(ssao_pipeline)
                        .draw(3, 1, 0, 0);
                });
        });

    auto ssao_blur_pass = rgc->add_graph_pass(
        "SSAO Blur Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.depends_on(ssao_pass)
                .add_color_attachment(ssao_blur_buffer, graphics::resource_access_type::WRITE)
                .add_sampled_image(ssao_buffer, 0, 3)
                .add_sampler(linear_no_aniso_sampler, 0, 5, graphics::pipeline_stage::FRAGMENT)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.set_scissor_region(0, 0, 1920, 1080)
                        .set_viewport(0, 0, 1920, 1080, 0, 1, 0, false)
                        .use_pipeline(ssao_blur_pipeline)
                        .draw(3, 1, 0, 0);
                });
        });

    auto pbr_opaque_pass = rgc->add_graph_pass(
        "PBR Opaque Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.depends_on(ssao_blur_pass)
                .add_color_attachment(color_buffer, graphics::resource_access_type::READ_WRITE,
                                      graphics::load_op::CLEAR, graphics::store_op::STORE, {0.0f, 0.0f, 0.0f, 1.0f})
                .add_color_attachment(specular_smoothness_buffer, graphics::resource_access_type::READ_WRITE,
                                      graphics::load_op::CLEAR, graphics::store_op::STORE, {0.0f, 0.0f, 0.0f, 0.0f})
                .add_depth_attachment(depth_buffer, graphics::resource_access_type::READ, graphics::load_op::LOAD,
                                      graphics::store_op::DONT_CARE)
                .add_constant_buffer(constants_buffer, 0, 0)
                .add_structured_buffer(point_lights_buffer, graphics::resource_access_type::READ, 0, 1)
                .add_structured_buffer(vertex_pull_buffer, graphics::resource_access_type::READ, 0, 2)
                .add_structured_buffer(mesh_layout_buffer, graphics::resource_access_type::READ, 0, 3)
                .add_structured_buffer(object_data_buffer, graphics::resource_access_type::READ, 0, 4)
                .add_structured_buffer(instance_data_buffer, graphics::resource_access_type::READ, 0, 5)
                .add_structured_buffer(material_buffer, graphics::resource_access_type::READ, 0, 6)
                .add_sampler(linear_sampler, 0, 7, graphics::pipeline_stage::FRAGMENT)
                .add_sampled_image(ssao_blur_buffer, 0, 8)
                .add_external_sampled_images(textures, 0, 9, graphics::pipeline_stage::FRAGMENT)
                .add_indirect_argument_buffer(indirect_commands)
                .add_index_buffer(vertex_pull_buffer)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.set_scissor_region(0, 0, 1920, 1080)
                        .set_viewport(0, 0, 1920, 1080)
                        .use_pipeline(pbr_opaque)
                        .use_index_buffer(vertex_pull_buffer, 0)
                        .draw_indexed(
                            indirect_commands,
                            static_cast<std::uint32_t>(graphics_device.get_buffer_frame_offset(indirect_commands)),
                            static_cast<std::uint32_t>(opaque_count + mask_count),
                            sizeof(graphics::indexed_indirect_command));
                });
        });

    std::ignore =
        rgc->add_graph_pass("Swapchain Blit Graph Pass", graphics::queue_operation_type::GRAPHICS_AND_TRANSFER,
                            [&](graphics::graph_pass_builder& bldr) {
                                bldr.add_blit_source(color_buffer)
                                    .add_external_blit_target(swapchain)
                                    .depends_on(pbr_opaque_pass)
                                    .on_execute([&](graphics::command_list& cmds) {
                                        cmds.blit(color_buffer, graphics_device.fetch_current_image(swapchain));
                                    });
                            });

    auto graph = std::move(*rgc).compile();

    {
        auto staging_buffer = graphics_device.get_staging_buffer();
        auto staging_buffer_ptr = graphics_device.map_buffer(staging_buffer);

        std::vector<core::mesh> meshes;
        meshes.reserve(scene->meshes.size());

        for (auto& mesh : scene->meshes)
        {
            meshes.push_back(std::move(mesh.mesh));
        }

        scene->meshes.clear();

        mesh_layouts = graphics::renderer_utilities::upload_meshes(graphics_device, meshes, vertex_pull_buffer);
        auto& executor = graphics_device.get_command_executor();

        {
            auto& cmds = executor.get_commands();
            std::memcpy(staging_buffer_ptr.data(), mesh_layouts.data(),
                        sizeof(graphics::mesh_layout) * mesh_layouts.size());
            cmds.copy(staging_buffer, mesh_layout_buffer, 0, 0, sizeof(graphics::mesh_layout) * mesh_layouts.size());
            executor.submit_and_wait();
        }

        {
            auto& cmds = executor.get_commands();
            std::memcpy(staging_buffer_ptr.data(), materials.data(),
                        sizeof(graphics::material_payload) * materials.size());
            cmds.copy(staging_buffer, material_buffer, 0, 0, sizeof(graphics::material_payload) * materials.size());
            executor.submit_and_wait();
        }

        graphics_device.unmap_buffer(staging_buffer);
    }

    auto end_opaque = std::stable_partition(std::begin(instances), std::end(instances), [&](std::uint32_t instance) {
        auto& mat = materials[objects[instance].material_id];
        return mat.type == graphics::material_type::OPAQUE;
    });

    opaque_count = std::distance(std::begin(instances), end_opaque);

    auto end_mask = std::stable_partition(end_opaque, std::end(instances), [&](std::uint32_t instance) {
        auto& mat = materials[objects[instance].material_id];
        return mat.type == graphics::material_type::MASK;
    });

    mask_count = std::distance(end_opaque, end_mask);

    std::stable_partition(end_mask, std::end(instances), [&](std::uint32_t instance) {
        auto& mat = materials[objects[instance].material_id];
        return mat.type == graphics::material_type::TRANSPARENT;
    });

    for (auto instance : instances)
    {
        auto& object = objects[instance];

        auto& mesh = mesh_layouts[object.mesh_id];

        indirect_draw_commands.push_back({
            .index_count = mesh.index_count,
            .instance_count = 1,
            .first_index = (mesh.mesh_start_offset + mesh.index_offset) / 4,
            .vertex_offset = 0,
            .first_instance = object.self_id,
        });
    }

    std::sort(std::begin(instances), std::end(instances));

    auto last_tick_time = std::chrono::high_resolution_clock::now();
    auto last_frame_time = last_tick_time;
    std::uint32_t fps_counter = 0;
    std::uint32_t last_fps = 0;

    scene = std::nullopt;

    while (!win->should_close())
    {
        input::poll();
        graph->execute();

        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> time_since_tick = current_time - last_tick_time;
        std::chrono::duration<double> frame_time = current_time - last_frame_time;
        last_frame_time = current_time;

        ++fps_counter;

        if (time_since_tick.count() >= 1.0)
        {
            last_fps = fps_counter;
            fps_counter = 0;
            last_tick_time = current_time;
            std::cout << last_fps << std::endl;
        }
    }

    for (auto& texture : textures)
    {
        graphics_device.release_image(texture);
    }

    graphics_device.release_image(noise_texture_handle);
    graphics_device.release_sampler(nearest_sampler);
    graphics_device.release_sampler(linear_sampler);
    graphics_device.release_sampler(linear_no_aniso_sampler);
    graphics_device.release_graphics_pipeline(ssao_blur_pipeline);
    graphics_device.release_graphics_pipeline(ssao_pipeline);
    graphics_device.release_graphics_pipeline(z_pass);
    graphics_device.release_graphics_pipeline(pbr_opaque);
    graphics_device.release_swapchain(swapchain);
}

graphics::material_type convert_material_type(assets::material_type type)
{
    switch (type)
    {
    case assets::material_type::OPAQUE:
        return graphics::material_type::OPAQUE;
    case assets::material_type::BLEND:
        return graphics::material_type::TRANSPARENT;
    case assets::material_type::MASK:
        return graphics::material_type::MASK;
    }

    std::exit(EXIT_FAILURE);
}

graphics::graphics_pipeline_resource_handle create_z_pass_pipeline(graphics::render_device& device)
{
    auto vertex_shader = core::read_bytes("data/pbr/pbr.z.vx.spv");
    auto fragment_shader = core::read_bytes("data/pbr/pbr.z.px.spv");

    graphics::descriptor_binding_info set0_bindings[] = {
        {
            .type = graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC,
            .binding_index = 0,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER,
            .binding_index = 2,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER,
            .binding_index = 3,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding_index = 4,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding_index = 5,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER,
            .binding_index = 6,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLER,
            .binding_index = 7,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLED_IMAGE,
            .binding_index = 9,
            .binding_count = 512,
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set{0},
            .bindings{set0_bindings},
        },
    };

    graphics::resource_format color_attachment_formats[] = {
        graphics::resource_format::RGBA8_UNORM,
    };

    graphics::color_blend_attachment_state blending[] = {
        {
            .enabled{false},
        },
    };

    return device.create_graphics_pipeline({
        .layout{
            .set_layouts = layouts,
        },
        .target{
            .color_attachment_formats = color_attachment_formats,
            .depth_attachment_format = graphics::resource_format::D32_FLOAT,
        },
        .vertex_shader{
            .bytes = vertex_shader,
            .entrypoint = "ZVSMain",
            .name = "Opaque Z Vertex Module",
        },
        .fragment_shader{
            .bytes = fragment_shader,
            .entrypoint = "ZPSMain",
            .name = "Opaque Z Fragment Module",
        },
        .depth_testing{
            .enable_test = true,
            .enable_write = true,
            .depth_test_op = graphics::compare_operation::GREATER_OR_EQUALS,
        },
        .blending{
            .attachment_blend_ops = blending,
        },
        .name = "Opaque Z Pipeline",
    });
}

graphics::graphics_pipeline_resource_handle create_ssao_pipeline(graphics::render_device& device)
{
    auto vertex_shader = core::read_bytes("data/ssao/ssao.vx.spv");
    auto fragment_shader = core::read_bytes("data/ssao/ssao.px.spv");

    graphics::descriptor_binding_info set0_bindings[] = {
        {
            .type = graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC,
            .binding_index = 0,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLED_IMAGE,
            .binding_index = 1,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLED_IMAGE,
            .binding_index = 2,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLED_IMAGE,
            .binding_index = 4,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLER,
            .binding_index = 5,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLER,
            .binding_index = 6,
            .binding_count = 1,
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set{0},
            .bindings{set0_bindings},
        },
    };

    graphics::color_blend_attachment_state blending[] = {
        {
            .enabled{false},
        },
    };

    graphics::resource_format color_buffer_fmt[] = {graphics::resource_format::R8_UNORM};

    return device.create_graphics_pipeline({
        .layout{
            .set_layouts = layouts,
        },
        .target{
            .color_attachment_formats = color_buffer_fmt,
        },
        .vertex_shader{
            .bytes = vertex_shader,
            .entrypoint = "VSMain",
            .name = "SSAO Vertex Shader Module",
        },
        .fragment_shader{
            .bytes = fragment_shader,
            .entrypoint = "PSMain",
            .name = "SSAO Fragment Shader Module",
        },
        .depth_testing{
            .enable_test = false,
            .enable_write = false,
            .depth_test_op = graphics::compare_operation::GREATER_OR_EQUALS,
        },
        .blending{
            .attachment_blend_ops = blending,
        },
        .name = "SSAO Pipeline",
    });
}

graphics::graphics_pipeline_resource_handle create_ssao_blur_pipeline(graphics::render_device& device)
{
    auto vertex_shader = core::read_bytes("data/ssao/ssao.vx.spv");
    auto fragment_shader = core::read_bytes("data/ssao/ssao.blur.px.spv");

    graphics::descriptor_binding_info set0_bindings[] = {
        {
            .type = graphics::descriptor_binding_type::SAMPLED_IMAGE,
            .binding_index = 3,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLER,
            .binding_index = 5,
            .binding_count = 1,
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set{0},
            .bindings{set0_bindings},
        },
    };

    graphics::color_blend_attachment_state blending[] = {
        {
            .enabled{false},
        },
    };

    graphics::resource_format color_buffer_fmt[] = {graphics::resource_format::R8_UNORM};

    return device.create_graphics_pipeline({
        .layout{
            .set_layouts = layouts,
        },
        .target{
            .color_attachment_formats = color_buffer_fmt,
        },
        .vertex_shader{
            .bytes = vertex_shader,
            .entrypoint = "VSMain",
            .name = "SSAO Blur Vertex Shader Module",
        },
        .fragment_shader{
            .bytes = fragment_shader,
            .entrypoint = "BlurMain",
            .name = "SSAO Blur Fragment Shader Module",
        },
        .depth_testing{
            .enable_test = false,
            .enable_write = false,
            .depth_test_op = graphics::compare_operation::GREATER_OR_EQUALS,
        },
        .blending{
            .attachment_blend_ops = blending,
        },
        .name = "SSAO Blur Pipeline",
    });
}

graphics::graphics_pipeline_resource_handle create_pbr_pipeline(graphics::render_device& device)
{
    auto vertex_shader = core::read_bytes("data/pbr/pbr.vx.spv");
    auto fragment_shader = core::read_bytes("data/pbr/pbr.px.spv");

    graphics::descriptor_binding_info set0_bindings[] = {
        {
            .type = graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC,
            .binding_index = 0,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding_index = 1,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER,
            .binding_index = 2,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER,
            .binding_index = 3,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding_index = 4,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC,
            .binding_index = 5,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER,
            .binding_index = 6,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLER,
            .binding_index = 7,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLED_IMAGE,
            .binding_index = 8,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLED_IMAGE,
            .binding_index = 9,
            .binding_count = 512,
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set{0},
            .bindings{set0_bindings},
        },
    };

    graphics::resource_format color_buffer_fmt[] = {graphics::resource_format::RGBA8_SRGB,
                                                    graphics::resource_format::RGBA8_UNORM};
    graphics::color_blend_attachment_state blending[] = {
        {
            .enabled{false},
        },
        {
            .enabled{false},
        },
    };

    return device.create_graphics_pipeline({
        .layout{
            .set_layouts = layouts,
        },
        .target{
            .color_attachment_formats = color_buffer_fmt,
            .depth_attachment_format = graphics::resource_format::D32_FLOAT,
        },
        .vertex_shader{
            .bytes = vertex_shader,
            .entrypoint = "VSMain",
            .name = "PBR Opaque Shader Module",
        },
        .fragment_shader{
            .bytes = fragment_shader,
            .entrypoint = "PSMain",
            .name = "PBR Opaque Shader Module",
        },
        .depth_testing{
            .enable_test = true,
            .enable_write = false,
            .depth_test_op = graphics::compare_operation::GREATER_OR_EQUALS,
        },
        .blending{
            .attachment_blend_ops = blending,
        },
        .name = "PBR Opaque Graphics Pipeline",
    });
}
