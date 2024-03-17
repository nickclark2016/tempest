#include <tempest/files.hpp>
#include <tempest/imgui_context.hpp>
#include <tempest/input.hpp>
#include <tempest/memory.hpp>
#include <tempest/graphics_components.hpp>
#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vec2.hpp>
#include <tempest/window.hpp>

#include <array>
#include <cstddef>
#include <format>
#include <iostream>
#include <string>

using namespace tempest;

namespace
{
    inline constexpr std::size_t global_memory_allocator_size = 1024 * 1024 * 64;
    inline constexpr std::uint32_t spectrum_texture_dim = 1024;
} // namespace

struct water_fft_constants
{
    float time_from_start;
    float delta_time;
    float gravity;
    float repeat_time;
    float damping;
    float depth;
    float low_cutoff;
    float high_cutoff;
    int seed;
    math::vec2<float> wind;
    math::vec2<float> lambda;
    math::vec2<float> normal_strength;
    unsigned int n;
    math::vec4<unsigned int> length_scalar;
    float foam_bias;
    float foam_decay_rate;
    float foam_add;
    float foam_threshold;
};

struct wave_spectrum
{
    float scale;
    float angle;
    float spread_blend;
    float swell;
    float alpha;
    float peak_omega;
    float gamma;
    float short_waves_fade;
};

struct wave_spectrum_ui
{
    float scale;
    float wind_speed;
    float wind_direction;
    float fetch;
    float spread_blend;
    float swell;
    float peak_enhancement;
    float short_waves_fade;

    wave_spectrum to_model(float gravity) const noexcept;
};

struct fft_layer_state
{
    int length_scalar;
    float tile_factor;
    bool visualize_tile;
    bool visualize_layer;
    bool contribute_displacement;
    std::array<wave_spectrum_ui, 2> spectrums;
    float foam_subtract;
};

struct ocean_fft_state
{
    int seed;
    float low_cutoff;
    float high_cutoff;
    float gravity;
    float depth;
    float repeat_time;
    float speed;
    math::vec2<float> lambda;
    float displacement_depth_falloff;
    bool update_spectrum;

    std::array<fft_layer_state, 4> spectrums;

    float normal_strength;
    float normal_depth_falloff;

    math::vec4<float> ambient;
    math::vec4<float> diffuse_reflect;
    math::vec4<float> specular_reflect;
    math::vec4<float> fresnel_color;

    float shininess;
    float spec_norm_strength;
    float fresnel_bias;
    float fresnel_strength;
    float fresnel_shininess;
    float fresnel_normal_strength;

    math::vec4<float> bubble_color;
    float bubble_density;
    float roughness;
    float foam_roughness;
    float height_modifier;
    float wave_peak_scatter_strength;
    float scatter_strength;
    float scatter_shadows_strength;
    float environment_light_strength;

    math::vec4<float> foam_color;
    float foam_bias;
    float foam_threshold;
    float foam_add;
    float foam_decay;
    float foam_depth_falloff;

    bool update = false;
};

struct camera_data
{
    math::mat4<float> proj;
    math::mat4<float> view;
    math::mat4<float> view_proj;
    math::vec3<float> position;
};

struct water_gfx_constants
{
    camera_data camera;
    graphics::directional_light sun;
    math::vec4<float> tiling;
    math::vec4<float> foam_subtract;
    math::vec4<float> scatter_color;
    math::vec4<float> bubble_color;
    math::vec4<float> foam_color;
    float normal_strength;
    float displacement_depth_attenuation;
    float far_over_near;
    float foam_depth_atten;
    float foam_roughness;
    float roughness;
    float normal_depth_atten;
    float height_modifier;
    float bubble_density;
    float wave_peak_scatter_strength;
    float scatter_strength;
    float scatter_shadow_strength;
};

void draw_gui(ocean_fft_state& state);

graphics::compute_pipeline_resource_handle create_fft_init_pipeline(graphics::render_device& device);
graphics::compute_pipeline_resource_handle create_fft_pack_spectrum_pipeline(graphics::render_device& device);
graphics::compute_pipeline_resource_handle create_fft_update_spectrum_for_fft(graphics::render_device& device);
graphics::compute_pipeline_resource_handle create_horizontal_fft(graphics::render_device& device);
graphics::compute_pipeline_resource_handle create_vertical_fft(graphics::render_device& device);
graphics::compute_pipeline_resource_handle create_map_assembly(graphics::render_device& device);
graphics::graphics_pipeline_resource_handle create_water_graphics(graphics::render_device& device);

graphics::mesh_layout create_water_plane(std::vector<std::uint32_t>& data);

void fft_water_demo()
{
    auto start_time = std::chrono::steady_clock::now();

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

    ocean_fft_state fft_state = {
        .seed = 1,
        .low_cutoff = 0.0001f,
        .high_cutoff = 9000.0f,
        .gravity = 9.81f,
        .depth = 20.0f,
        .repeat_time = 200.0f,
        .speed = 1.0f,
        .lambda = {1.0f, 1.0f},
        .update = true,
    };

    water_fft_constants fft_constants = {
        .gravity = 9.81f,
        .repeat_time = 200.0f,
        .depth = 20.0f,
        .low_cutoff = 0.0001f,
        .high_cutoff = 9000.0f,
        .seed = 1,
        .wind = {1, 1},
        .lambda = {1, 1},
        .n = spectrum_texture_dim,
        .length_scalar = {512, 128, 64, 32},
        .foam_bias = 0.85f,
        .foam_decay_rate = 0.0375f,
        .foam_add = 0.1f,
        .foam_threshold = 0.005f,
    };

    std::array<wave_spectrum, 8> wave_spectrums = {
        {
            (wave_spectrum_ui{
                 .scale = 0.5f,
                 .wind_speed = 20.0f,
                 .wind_direction = 22.0f,
                 .fetch = 100000000,
                 .spread_blend = 1,
                 .swell = 0.42f,
                 .peak_enhancement = 1.0f,
                 .short_waves_fade = 1.0f,
             })
                .to_model(fft_constants.gravity),
            (wave_spectrum_ui{
                 .scale = 0.5f,
                 .wind_speed = 24.9f,
                 .wind_direction = 59.0f,
                 .fetch = 1000000,
                 .spread_blend = 1,
                 .swell = 1.0f,
                 .peak_enhancement = 1.0f,
                 .short_waves_fade = 1.0f,
             })
                .to_model(fft_constants.gravity),
            (wave_spectrum_ui{
                 .scale = 0.25f,
                 .wind_speed = 20.0f,
                 .wind_direction = 97.0f,
                 .fetch = 1000000,
                 .spread_blend = 0.14f,
                 .swell = 1.0f,
                 .peak_enhancement = 1.0f,
                 .short_waves_fade = 0.5f,
             })
                .to_model(fft_constants.gravity),
            (wave_spectrum_ui{
                 .scale = 0.25f,
                 .wind_speed = 20.0f,
                 .wind_direction = 67.0f,
                 .fetch = 100000,
                 .spread_blend = 0.47f,
                 .swell = 1.0f,
                 .peak_enhancement = 1.0f,
                 .short_waves_fade = 1.0f,
             })
                .to_model(fft_constants.gravity),
            (wave_spectrum_ui{
                 .scale = 0.15f,
                 .wind_speed = 5.0f,
                 .wind_direction = 105.0f,
                 .fetch = 100000,
                 .spread_blend = 0.2f,
                 .swell = 1.0f,
                 .peak_enhancement = 1.0f,
                 .short_waves_fade = 0.5f,
             })
                .to_model(fft_constants.gravity),
            (wave_spectrum_ui{
                 .scale = 0.1f,
                 .wind_speed = 1.0f,
                 .wind_direction = 19.0f,
                 .fetch = 10000,
                 .spread_blend = 0.298f,
                 .swell = 0.695f,
                 .peak_enhancement = 1.0f,
                 .short_waves_fade = 0.5f,
             })
                .to_model(fft_constants.gravity),
            (wave_spectrum_ui{
                 .scale = 1.0f,
                 .wind_speed = 1.0f,
                 .wind_direction = 209.0f,
                 .fetch = 200000,
                 .spread_blend = 0.56f,
                 .swell = 1.0f,
                 .peak_enhancement = 1.0f,
                 .short_waves_fade = 0.0001f,
             })
                .to_model(fft_constants.gravity),
            (wave_spectrum_ui{
                 .scale = 0.23f,
                 .wind_speed = 1.0f,
                 .wind_direction = 0.0f,
                 .fetch = 1000,
                 .spread_blend = 0.0f,
                 .swell = 0.0f,
                 .peak_enhancement = 1.0f,
                 .short_waves_fade = 0.0001f,
             })
                .to_model(fft_constants.gravity),
        },
    };

    graphics::imgui_context::initialize_for_window(*win);
    auto swapchain = graphics_device.create_swapchain({.win{win.get()}, .desired_frame_count{3}});

    auto rgc = graphics::render_graph_compiler::create_compiler(&global_allocator, &graphics_device);
    rgc->enable_imgui();

    auto color_buffer = rgc->create_image({
        .width{1920},
        .height{1080},
        .fmt{graphics::resource_format::RGBA8_SRGB},
        .type{graphics::image_type::IMAGE_2D},
        .name{"Color Buffer Target"},
    });

    auto depth_buffer = rgc->create_image({
        .width{1920},
        .height{1080},
        .fmt{graphics::resource_format::D32_FLOAT},
        .type{graphics::image_type::IMAGE_2D},
        .name{"Depth Buffer Target"},
    });

    auto spectrum_parameter_buffer = rgc->create_buffer({
        .size = sizeof(wave_spectrums),
        .location{graphics::memory_location::DEVICE},
        .name{"Water FFT Spectrum Buffer"},
        .per_frame_memory{true},
    });

    auto initial_spectrum_textures = rgc->create_image({
        .width{spectrum_texture_dim},
        .height{spectrum_texture_dim},
        .depth{1},
        .layers{4},
        .fmt{graphics::resource_format::RGBA16_FLOAT},
        .type{graphics::image_type::IMAGE_2D_ARRAY},
        .persistent{true},
        .name{"Initial Water FFT Spectrum Textures"},
    });

    auto displacement_textures = rgc->create_image({
        .width{spectrum_texture_dim},
        .height{spectrum_texture_dim},
        .depth{1},
        .layers{4},
        .fmt{graphics::resource_format::RGBA16_FLOAT},
        .type{graphics::image_type::IMAGE_2D_ARRAY},
        .persistent{true},
        .name{"Water FFT Displacement Textures"},
    });

    auto slope_textures = rgc->create_image({
        .width{spectrum_texture_dim},
        .height{spectrum_texture_dim},
        .depth{1},
        .layers{4},
        .fmt{graphics::resource_format::RG16_FLOAT},
        .type{graphics::image_type::IMAGE_2D_ARRAY},
        .name{"Water FFT Slope Textures"},
    });

    auto spectrum_textures = rgc->create_image({
        .width{spectrum_texture_dim},
        .height{spectrum_texture_dim},
        .depth{1},
        .layers{8},
        .fmt{graphics::resource_format::RGBA16_FLOAT},
        .type{graphics::image_type::IMAGE_2D_ARRAY},
        .name{"Water FFT Spectrum Textures"},
    });

    auto constants_buffer = rgc->create_buffer({
        .size{sizeof(water_fft_constants)},
        .location{graphics::memory_location::DEVICE},
        .name{"Water FFT Constant Buffer"},
        .per_frame_memory{true},
    });

    auto gfx_constants_buffer = rgc->create_buffer({
        .size = sizeof(water_gfx_constants),
        .location = graphics::memory_location::DEVICE,
        .name = "Water FFT Graphics Constant Buffer",
        .per_frame_memory = true,
    });

    auto vertex_buffer = rgc->create_buffer({
        .size = 1024 * 1024 * 128,
        .location = graphics::memory_location::DEVICE,
        .name = "Vertex Buffer",
        .per_frame_memory = false,
    });

    auto mesh_buffer = rgc->create_buffer({
        .size = 4096 * sizeof(graphics::mesh_layout),
        .location = graphics::memory_location::DEVICE,
        .name = "Mesh Layout Buffer",
        .per_frame_memory = false,
    });

    auto object_data_buffer = rgc->create_buffer({
        .size = 1024 * 32 * sizeof(graphics::object_payload),
        .location = graphics::memory_location::DEVICE,
        .name = "Object Payload Buffer",
        .per_frame_memory = true,
    });

    auto fft_state_init = create_fft_init_pipeline(graphics_device);
    auto fft_conjugate_pack = create_fft_pack_spectrum_pipeline(graphics_device);
    auto fft_update_spectrum = create_fft_update_spectrum_for_fft(graphics_device);
    auto fft_horizontal = create_horizontal_fft(graphics_device);
    auto fft_vertical = create_vertical_fft(graphics_device);
    auto fft_map_assemble = create_map_assembly(graphics_device);
    auto fft_water_shader = create_water_graphics(graphics_device);
    auto fft_water_sampler = graphics_device.create_sampler({
        .mag = graphics::filter::LINEAR,
        .min = graphics::filter::LINEAR,
        .mipmap = graphics::mipmap_mode::LINEAR,
    });

    std::vector<std::uint32_t> vertex_data;
    std::vector<graphics::mesh_layout> meshes;
    std::vector<graphics::object_payload> objects;
    water_gfx_constants gfx_constants = {
        .camera{
            .proj{math::perspective(16.0f / 9.0f, 90.0f * 9.0f / 16.0f, 0.1f)},
            .view{math::look_at(math::vec3<float>(-16.0f, 6.0f, 0.0f), math::vec3<float>(0.0f, 6.0f, 0.0f),
                                math::vec3<float>(0.0f, 1.0, 0.0f))},
            .view_proj{1.0f},
            .position{-16.0f, 6.0f, 0.0f},
        },
        .sun{
            .light_direction{-1.29f, -1.0f, 4.86f},
            .color_illum{0.8f, 0.794f, 0.78f, 25000.0f},
        },
        .tiling = {4.0f, 8.0f, 64.0f, 128.0f},
        .foam_subtract = {0.04f, -0.04f, -0.46f, -0.38f},
        .scatter_color = {0.16f, 0.0736f, 0.16f, 1.0f},
        .bubble_color = {0.0f, 0.02f, 0.016f, 1.0f},
        .foam_color = {0.50f, 0.5568f, 0.492f, 1.0f},
        .normal_strength = 10.0f,
        .displacement_depth_attenuation = 1.0f,
        .foam_depth_atten = 0.1f,
        .foam_roughness = 0.0f,
        .roughness = 0.075f,
        .normal_depth_atten = 1.0f,
        .height_modifier = 0.5f,
        .bubble_density = 1.0f,
        .wave_peak_scatter_strength = 1.0f,
        .scatter_strength = 0.2f,
        .scatter_shadow_strength = 0.5f,
    };

    auto transform = math::transform(math::vec3<float>{}, math::vec3<float>{}, math::vec3<float>{10.0f, 1.0f, 10.0f});

    objects.push_back(graphics::object_payload{
        .transform = {transform},
        .inv_transform = math::inverse(transform),
        .mesh_id = 0,
        .material_id = 0,
    });

    auto water_plane_layout = create_water_plane(vertex_data);
    meshes.push_back(water_plane_layout);

    fft_state.update = true;

    auto state_upload_pass = rgc->add_graph_pass(
        "Water Sim State Buffer Upload Graph Pass", graphics::queue_operation_type::TRANSFER,
        [&](graphics::graph_pass_builder& bldr) {
            bldr.add_transfer_source_buffer(graphics_device.get_staging_buffer())
                .add_transfer_destination_buffer(constants_buffer)
                .add_transfer_destination_buffer(spectrum_parameter_buffer)
                .add_transfer_destination_buffer(object_data_buffer)
                .add_transfer_destination_buffer(gfx_constants_buffer)
                .add_transfer_target(initial_spectrum_textures)
                .on_execute([&](graphics::command_list& cmds) {
                    auto staging = graphics_device.get_staging_buffer();
                    std::span<std::byte> src_ptr = graphics_device.map_buffer_frame(staging);
                    std::memcpy(src_ptr.data(), &fft_constants, sizeof(water_fft_constants));
                    std::memcpy(src_ptr.data() + sizeof(water_fft_constants), wave_spectrums.data(),
                                sizeof(wave_spectrums));
                    std::memcpy(src_ptr.data() + sizeof(water_fft_constants) + sizeof(wave_spectrums), objects.data(),
                                objects.size() * sizeof(graphics::object_payload));
                    std::memcpy(src_ptr.data() + sizeof(water_fft_constants) + sizeof(wave_spectrums) +
                                    objects.size() * sizeof(graphics::object_payload),
                                &gfx_constants, sizeof(water_gfx_constants));
                    graphics_device.unmap_buffer(staging);

                    cmds.copy(staging, constants_buffer, 0, graphics_device.get_buffer_frame_offset(constants_buffer),
                              sizeof(water_fft_constants))
                        .copy(staging, spectrum_parameter_buffer, sizeof(water_fft_constants),
                              graphics_device.get_buffer_frame_offset(spectrum_parameter_buffer),
                              sizeof(wave_spectrums))
                        .copy(staging, object_data_buffer, sizeof(water_fft_constants) + sizeof(wave_spectrums),
                              graphics_device.get_buffer_frame_offset(object_data_buffer),
                              objects.size() * sizeof(graphics::object_payload))
                        .copy(staging, gfx_constants_buffer,
                              sizeof(water_fft_constants) + sizeof(wave_spectrums) +
                                  objects.size() * sizeof(graphics::object_payload),
                              graphics_device.get_buffer_frame_offset(gfx_constants_buffer),
                              sizeof(water_gfx_constants));
                });
        });

    auto state_init_pass = rgc->add_graph_pass(
        "Water Simulation State Initialization", graphics::queue_operation_type::COMPUTE,
        [&](graphics::graph_pass_builder& bldr) {
            bldr.should_execute([&]() -> bool { return fft_state.update; })
                .depends_on(state_upload_pass)
                .add_constant_buffer(constants_buffer, 0, 0)
                .add_structured_buffer(spectrum_parameter_buffer, graphics::resource_access_type::READ_WRITE, 0, 5)
                .add_storage_image(initial_spectrum_textures, graphics::resource_access_type::READ_WRITE, 0, 2)
                .add_transfer_target(displacement_textures)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.use_pipeline(fft_state_init)
                        .dispatch(spectrum_texture_dim / 8, spectrum_texture_dim / 8, 1)
                        .clear_color(displacement_textures, 0, 0, 0, 1);
                });
        });

    auto sim_state_conjugate_pass = rgc->add_graph_pass(
        "Water Simulation State Spectrum Conjugate", graphics::queue_operation_type::COMPUTE,
        [&](graphics::graph_pass_builder& bldr) {
            bldr.should_execute([&]() -> bool { return fft_state.update; })
                .depends_on(state_init_pass)
                .add_constant_buffer(constants_buffer, 0, 0)
                .add_storage_image(initial_spectrum_textures, graphics::resource_access_type::READ_WRITE, 0, 2)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.use_pipeline(fft_conjugate_pack)
                        .dispatch(spectrum_texture_dim / 8, spectrum_texture_dim / 8, 1);
                    fft_state.update = false;
                });
        });

    auto update_spectrum_pass = rgc->add_graph_pass(
        "Water Simulation State Update Spectrum", graphics::queue_operation_type::COMPUTE,
        [&](graphics::graph_pass_builder& bldr) {
            bldr.depends_on(sim_state_conjugate_pass)
                .depends_on(state_upload_pass)
                .add_constant_buffer(constants_buffer, 0, 0)
                .add_storage_image(spectrum_textures, graphics::resource_access_type::READ_WRITE, 0, 1)
                .add_storage_image(initial_spectrum_textures, graphics::resource_access_type::READ, 0, 2)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.use_pipeline(fft_update_spectrum)
                        .dispatch(spectrum_texture_dim / 8, spectrum_texture_dim / 8, 1);
                });
        });

    auto horizontal_fft = rgc->add_graph_pass(
        "Water Simulation Horizontal FFT", graphics::queue_operation_type::COMPUTE,
        [&](graphics::graph_pass_builder& bldr) {
            bldr.depends_on(update_spectrum_pass)
                .add_constant_buffer(constants_buffer, 0, 0)
                .add_storage_image(spectrum_textures, graphics::resource_access_type::READ_WRITE, 0, 1)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.use_pipeline(fft_horizontal).dispatch(1, spectrum_texture_dim, 1);
                });
        });

    auto vertical_fft = rgc->add_graph_pass(
        "Water Simulation Vertical FFT", graphics::queue_operation_type::COMPUTE,
        [&](graphics::graph_pass_builder& bldr) {
            bldr.depends_on(horizontal_fft)
                .add_constant_buffer(constants_buffer, 0, 0)
                .add_storage_image(spectrum_textures, graphics::resource_access_type::READ_WRITE, 0, 1)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.use_pipeline(fft_vertical).dispatch(1, spectrum_texture_dim, 1);
                });
        });

    auto map_assembly = rgc->add_graph_pass(
        "Water Simulation Map Assembly", graphics::queue_operation_type::COMPUTE,
        [&](graphics::graph_pass_builder& bldr) {
            bldr.depends_on(vertical_fft)
                .add_constant_buffer(constants_buffer, 0, 0)
                .add_storage_image(spectrum_textures, graphics::resource_access_type::READ, 0, 1)
                .add_storage_image(displacement_textures, graphics::resource_access_type::WRITE, 0, 3)
                .add_storage_image(slope_textures, graphics::resource_access_type::WRITE, 0, 4)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.use_pipeline(fft_map_assemble).dispatch(spectrum_texture_dim / 8, spectrum_texture_dim / 8, 1);
                });
        });

    auto water_gfx_pass = rgc->add_graph_pass(
        "Water Graphics Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_constant_buffer(gfx_constants_buffer, 0, 0)
                .add_structured_buffer(vertex_buffer, graphics::resource_access_type::READ, 0, 1)
                .add_structured_buffer(mesh_buffer, graphics::resource_access_type::READ, 0, 2)
                .add_structured_buffer(object_data_buffer, graphics::resource_access_type::READ, 0, 3)
                .add_sampled_image(displacement_textures, 0, 4)
                .add_sampled_image(slope_textures, 0, 5)
                .add_sampler(fft_water_sampler, 0, 6, graphics::pipeline_stage::VERTEX)
                .add_color_attachment(color_buffer, graphics::resource_access_type::WRITE, graphics::load_op::CLEAR,
                                      graphics::store_op::STORE)
                .add_depth_attachment(depth_buffer, graphics::resource_access_type::WRITE, graphics::load_op::CLEAR,
                                      graphics::store_op::DONT_CARE, 0.0f)
                .depends_on(map_assembly)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.set_viewport(0, 0, 1920, 1080)
                        .set_scissor_region(0, 0, 1920, 1080)
                        .use_pipeline(fft_water_shader)
                        .draw(128 * 128 * 6);
                });
        });

    auto imgui_pass = rgc->add_graph_pass(
        "ImGUI Graph Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_color_attachment(color_buffer, graphics::resource_access_type::WRITE, graphics::load_op::LOAD,
                                      graphics::store_op::STORE)
                .draw_imgui()
                .depends_on(water_gfx_pass)
                .on_execute([](auto& cmds) {});
        });

    std::ignore =
        rgc->add_graph_pass("Swapchain Blit Graph Pass", graphics::queue_operation_type::GRAPHICS_AND_TRANSFER,
                            [&](graphics::graph_pass_builder& bldr) {
                                bldr.add_blit_source(color_buffer)
                                    .add_external_blit_target(swapchain)
                                    .depends_on(imgui_pass)
                                    .on_execute([&](graphics::command_list& cmds) {
                                        cmds.blit(color_buffer, graphics_device.fetch_current_image(swapchain));
                                    });
                            });

    auto graph = std::move(*rgc).compile();

    {
        auto& executor = graphics_device.get_command_executor();

        auto staging = graphics_device.get_staging_buffer();
        auto staging_data = graphics_device.map_buffer(staging);

        {
            auto& cmds = executor.get_commands();
            std::memcpy(staging_data.data(), vertex_data.data(), vertex_data.size() * sizeof(std::uint32_t));
            cmds.copy(staging, vertex_buffer, 0, 0, vertex_data.size() * sizeof(std::uint32_t));
            executor.submit_and_wait();
        }

        {
            auto& cmds = executor.get_commands();
            std::memcpy(staging_data.data(), meshes.data(), meshes.size() * sizeof(graphics::mesh_layout));
            cmds.copy(staging, mesh_buffer, 0, 0, meshes.size() * sizeof(graphics::mesh_layout));
            executor.submit_and_wait();
        }

        graphics_device.unmap_buffer(staging);
    }

    auto last_time = std::chrono::steady_clock::now();

    while (!win->should_close())
    {
        core::input::poll();
        draw_gui(fft_state);

        graph->execute();

        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<float> seconds_since_start = current_time - start_time;
        std::chrono::duration<float> delta_time_sec = current_time - last_time;

        last_time = current_time;

        fft_constants.time_from_start = seconds_since_start.count();
        fft_constants.delta_time = delta_time_sec.count();

        // std::cout << delta_time_sec.count() << "s\n";
    }

    graphics::imgui_context::shutdown();

    graphics_device.release_sampler(fft_water_sampler);
    graphics_device.release_graphics_pipeline(fft_water_shader);
    graphics_device.release_compute_pipeline(fft_map_assemble);
    graphics_device.release_compute_pipeline(fft_vertical);
    graphics_device.release_compute_pipeline(fft_horizontal);
    graphics_device.release_compute_pipeline(fft_update_spectrum);
    graphics_device.release_compute_pipeline(fft_conjugate_pack);
    graphics_device.release_compute_pipeline(fft_state_init);
    graphics_device.release_swapchain(swapchain);
}

void draw_gui(ocean_fft_state& state)
{
    using graphics::imgui_context;

    imgui_context::create_frame([&]() {
        imgui_context::create_window("FFT Water Demo", [&]() {
            imgui_context::create_header("General Settings", [&]() {
                imgui_context::create_table("##GeneralFFTSettings", 2, [&]() {
                    imgui_context::next_row();
                    imgui_context::next_column();
                    imgui_context::label("Seed");
                    imgui_context::next_column();
                    state.seed = imgui_context::int_slider("##FftSeed", 0, 100, state.seed);

                    imgui_context::next_row();
                    imgui_context::next_column();
                    imgui_context::label("Gravity");
                    imgui_context::next_column();
                    state.gravity = imgui_context::float_slider("##FftGravity", 0.0001f, 20.0f, state.gravity);

                    imgui_context::next_row();
                    imgui_context::next_column();
                    imgui_context::label("Depth");
                    imgui_context::next_column();
                    state.depth = imgui_context::float_slider("##FftDepth", 0.0001f, 100.0f, state.depth);

                    imgui_context::next_row();
                    imgui_context::next_column();
                    imgui_context::label("Low Cutoff");
                    imgui_context::next_column();
                    state.low_cutoff =
                        imgui_context::float_slider("##FftLowCutoff", 0.0001f, 10000.0f, state.low_cutoff);

                    imgui_context::next_row();
                    imgui_context::next_column();
                    imgui_context::label("High Cutoff");
                    imgui_context::next_column();
                    state.high_cutoff =
                        imgui_context::float_slider("##FftHighCutoff", 0.0001f, 10000.0f, state.high_cutoff);

                    imgui_context::next_row();
                    imgui_context::next_column();
                    imgui_context::label("Repeat Time");
                    imgui_context::next_column();
                    state.repeat_time = imgui_context::float_slider("##FftRepeatTime", 1.0f, 500.0f, state.repeat_time);

                    imgui_context::next_row();
                    imgui_context::next_column();
                    imgui_context::label("Speed");
                    imgui_context::next_column();
                    state.speed = imgui_context::float_slider("##FftSpeed", 0.1f, 10.0f, state.speed);

                    imgui_context::next_row();
                    imgui_context::next_column();
                    imgui_context::label("Displacement Depth Falloff");
                    imgui_context::next_column();
                    state.displacement_depth_falloff = imgui_context::float_slider(
                        "##FftDisplacementDepthFalloff", 0.01f, 10.0f, state.displacement_depth_falloff);

                    imgui_context::next_row();
                    imgui_context::next_column();
                    imgui_context::label("Lambda");
                    imgui_context::next_column();
                    state.lambda =
                        imgui_context::float2_slider("##FftDisplacementDepthFalloff", -1.0f, 1.0f, state.lambda);
                });
            });

            std::uint32_t layer_num = 1;
            for (auto& parameter : state.spectrums)
            {
                auto section_header = std::format("Layer {} Parameters", layer_num);
                auto table_header = std::format("##Layer{}FFTSettings", layer_num);
                auto length_scale = std::format("Layer {} Length Scale", layer_num);
                auto length_scale_label = std::format("##Layer{}FFTLengthScale", layer_num);
                auto tile_factor = std::format("Layer {} Tile Factor", layer_num);
                auto tile_factor_label = std::format("##Layer{}FFTTileFactor", layer_num);
                auto visualize_tile = std::format("Layer {} Visualize Tiles", layer_num);
                auto visualize_tile_label = std::format("##Layer{}FFTVisualizeTiles", layer_num);
                auto visualize_layer = std::format("Layer {} Visualize Layer", layer_num);
                auto visualize_layer_label = std::format("##Layer{}FFTVisualizeLayer", layer_num);
                auto contrib_displacement = std::format("Layer {} Contribute Displacement", layer_num);
                auto contrib_displacement_label = std::format("##Layer{}FFTContributeDisplacement", layer_num);

                imgui_context::create_header(section_header, [&]() {
                    imgui_context::create_table(table_header, 2, [&]() {
                        imgui_context::next_row();
                        imgui_context::next_column();
                        imgui_context::label(length_scale);
                        imgui_context::next_column();
                        parameter.length_scalar =
                            imgui_context::int_slider(length_scale_label, 1, 1000, parameter.length_scalar);

                        imgui_context::next_row();
                        imgui_context::next_column();
                        imgui_context::label(tile_factor);
                        imgui_context::next_column();
                        parameter.tile_factor =
                            imgui_context::float_slider(tile_factor_label, 0.001f, 10.0f, parameter.tile_factor);

                        imgui_context::next_row();
                        imgui_context::next_column();
                        imgui_context::label(visualize_tile);
                        imgui_context::next_column();
                        parameter.visualize_tile =
                            imgui_context::checkbox(visualize_tile_label, parameter.visualize_tile);

                        imgui_context::next_row();
                        imgui_context::next_column();
                        imgui_context::label(visualize_layer);
                        imgui_context::next_column();
                        parameter.visualize_layer =
                            imgui_context::checkbox(visualize_layer_label, parameter.visualize_layer);

                        imgui_context::next_row();
                        imgui_context::next_column();
                        imgui_context::label(contrib_displacement);
                        imgui_context::next_column();
                        parameter.contribute_displacement =
                            imgui_context::checkbox(contrib_displacement_label, parameter.contribute_displacement);
                    });

                    for (std::uint32_t i = 0; i < 2; ++i)
                    {
                        auto wave_num = 2 * (layer_num - 1) + i + 1;
                        auto spectrum_tree_node = std::format("Wave {} Spectrum Parameters", wave_num);
                        auto spectrum_tree_table_label = std::format("##FFTWave{}SpectrumParameters", wave_num);

                        auto scale = std::format("Wave {} Scale", wave_num);
                        auto scale_control_label = std::format("##Wave{}ScaleLabel", wave_num);

                        auto wind_speed = std::format("Wave {} Wind Speed", wave_num);
                        auto wind_speed_control_label = std::format("##Wave{}WindSpeedLabel", wave_num);

                        auto wind_direction = std::format("Wave {} Wind Direction", wave_num);
                        auto wind_direction_control_label = std::format("##Wave{}WindDirectionLabel", wave_num);

                        auto fetch = std::format("Wave {} Fetch", wave_num);
                        auto fetch_control_label = std::format("##Wave{}FetchLabel", wave_num);

                        auto spread_blend = std::format("Wave {} Spread Blend", wave_num);
                        auto spread_blend_control_label = std::format("##Wave{}SpreadBlendLabel", wave_num);

                        auto swell = std::format("Wave {} Swell", wave_num);
                        auto swell_control_label = std::format("##Wave{}SwellLabel", wave_num);

                        auto peak_enhancement = std::format("Wave {} Peak Enhancement", wave_num);
                        auto peak_enhancement_control_label = std::format("##Wave{}PeakEnhancementLabel", wave_num);

                        auto short_waves_fade = std::format("Wave {} Short Waves Fade", wave_num);
                        auto short_waves_fade_control_label = std::format("##Wave{}ShortWavesFadeLabel", wave_num);

                        imgui_context::create_tree_node(spectrum_tree_node, [&]() {
                            imgui_context::create_table(spectrum_tree_table_label, 2, [&]() {
                                imgui_context::next_row();
                                imgui_context::next_column();
                                imgui_context::label(scale);
                                imgui_context::next_column();
                                parameter.spectrums[i].scale = imgui_context::float_slider(
                                    scale_control_label, 0, 5, parameter.spectrums[i].scale);

                                imgui_context::next_row();
                                imgui_context::next_column();
                                imgui_context::label(wind_speed);
                                imgui_context::next_column();
                                parameter.spectrums[i].wind_speed = imgui_context::float_slider(
                                    wind_speed_control_label, 0, 10, parameter.spectrums[i].wind_speed);

                                imgui_context::next_row();
                                imgui_context::next_column();
                                imgui_context::label(wind_direction);
                                imgui_context::next_column();
                                parameter.spectrums[i].wind_direction = imgui_context::float_slider(
                                    wind_direction_control_label, 0, 360, parameter.spectrums[i].wind_direction);

                                imgui_context::next_row();
                                imgui_context::next_column();
                                imgui_context::label(fetch);
                                imgui_context::next_column();
                                parameter.spectrums[i].fetch = imgui_context::float_slider(
                                    fetch_control_label, 0, 20, parameter.spectrums[i].fetch);

                                imgui_context::next_row();
                                imgui_context::next_column();
                                imgui_context::label(spread_blend);
                                imgui_context::next_column();
                                parameter.spectrums[i].spread_blend = imgui_context::float_slider(
                                    spread_blend_control_label, 0, 1, parameter.spectrums[i].spread_blend);

                                imgui_context::next_row();
                                imgui_context::next_column();
                                imgui_context::label(swell);
                                imgui_context::next_column();
                                parameter.spectrums[i].swell = imgui_context::float_slider(
                                    swell_control_label, 0.01f, 1, parameter.spectrums[i].swell);

                                imgui_context::next_row();
                                imgui_context::next_column();
                                imgui_context::label(peak_enhancement);
                                imgui_context::next_column();
                                parameter.spectrums[i].peak_enhancement = imgui_context::float_slider(
                                    peak_enhancement_control_label, 0, 20, parameter.spectrums[i].peak_enhancement);

                                imgui_context::next_row();
                                imgui_context::next_column();
                                imgui_context::label(short_waves_fade);
                                imgui_context::next_column();
                                parameter.spectrums[i].short_waves_fade = imgui_context::float_slider(
                                    short_waves_fade_control_label, 0, 20, parameter.spectrums[i].short_waves_fade);
                            });
                        });
                    }
                });

                ++layer_num;
            }

            std::ignore = imgui_context::button("Reinitialize Water Simulation");
        });
    });
}

graphics::compute_pipeline_resource_handle create_fft_init_pipeline(graphics::render_device& device)
{
    auto compute_shader = core::read_bytes("data/fft_water/fft_water.init_state.cx.spv");

    graphics::descriptor_binding_info set0_bindings[] = {
        {
            .type{graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC},
            .binding_index{0},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::STRUCTURED_BUFFER_DYNAMIC},
            .binding_index{5},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::STORAGE_IMAGE},
            .binding_index{2},
            .binding_count{1},
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set{0},
            .bindings{set0_bindings},
        },
    };

    return device.create_compute_pipeline({
        .layout{
            .set_layouts{layouts},
        },
        .compute_shader{
            .bytes = compute_shader,
            .entrypoint = "InitializeFFTState",
            .name = "FFT State Initialization Shader Module",
        },
        .name = "FFT State Initialization Pipeline",
    });
}

graphics::compute_pipeline_resource_handle create_fft_pack_spectrum_pipeline(graphics::render_device& device)
{
    auto compute_shader = core::read_bytes("data/fft_water/fft_water.pack_spectrum.cx.spv");

    graphics::descriptor_binding_info set0_bindings[] = {
        {
            .type{graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC},
            .binding_index{0},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::STORAGE_IMAGE},
            .binding_index{2},
            .binding_count{1},
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set{0},
            .bindings{set0_bindings},
        },
    };

    return device.create_compute_pipeline({
        .layout{
            .set_layouts{layouts},
        },
        .compute_shader{
            .bytes = compute_shader,
            .entrypoint = "PackSpectrumConjugate",
            .name = "FFT Pack Spectrum Conjugate Shader Module",
        },
        .name = "FFT Pack Spectrum Conjugate Pipeline",
    });
}

graphics::compute_pipeline_resource_handle create_fft_update_spectrum_for_fft(graphics::render_device& device)
{
    auto compute_shader = core::read_bytes("data/fft_water/fft_water.update_spectrum.cx.spv");

    graphics::descriptor_binding_info set0_bindings[] = {
        {
            .type{graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC},
            .binding_index{0},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::STORAGE_IMAGE},
            .binding_index{1},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::STORAGE_IMAGE},
            .binding_index{2},
            .binding_count{1},
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set{0},
            .bindings{set0_bindings},
        },
    };

    return device.create_compute_pipeline({
        .layout{
            .set_layouts{layouts},
        },
        .compute_shader{
            .bytes = compute_shader,
            .entrypoint = "UpdateSpectrumForFFT",
            .name = "FFT Update Spectrum Shader Module",
        },
        .name = "FFT Update Spectrum Pipeline",
    });
}

graphics::compute_pipeline_resource_handle create_horizontal_fft(graphics::render_device& device)
{
    auto compute_shader = core::read_bytes("data/fft_water/fft_water.horizontal_fft.cx.spv");

    graphics::descriptor_binding_info set0_bindings[] = {
        {
            .type{graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC},
            .binding_index{0},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::STORAGE_IMAGE},
            .binding_index{1},
            .binding_count{1},
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set{0},
            .bindings{set0_bindings},
        },
    };

    return device.create_compute_pipeline({
        .layout{
            .set_layouts{layouts},
        },
        .compute_shader{
            .bytes = compute_shader,
            .entrypoint = "HorizontalFFT",
            .name = "FFT Horizontal FFT Module",
        },
        .name = "FFT Horizontal FFT Pipeline",
    });
}

graphics::compute_pipeline_resource_handle create_vertical_fft(graphics::render_device& device)
{
    auto compute_shader = core::read_bytes("data/fft_water/fft_water.vertical_fft.cx.spv");

    graphics::descriptor_binding_info set0_bindings[] = {
        {
            .type{graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC},
            .binding_index{0},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::STORAGE_IMAGE},
            .binding_index{1},
            .binding_count{1},
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set{0},
            .bindings{set0_bindings},
        },
    };

    return device.create_compute_pipeline({
        .layout{
            .set_layouts{layouts},
        },
        .compute_shader{
            .bytes = compute_shader,
            .entrypoint = "VerticalFFT",
            .name = "FFT Vertical FFT Module",
        },
        .name = "FFT Vertical FFT Pipeline",
    });
}

graphics::compute_pipeline_resource_handle create_map_assembly(graphics::render_device& device)
{
    auto compute_shader = core::read_bytes("data/fft_water/fft_water.assemble_maps.cx.spv");

    graphics::descriptor_binding_info set0_bindings[] = {
        {
            .type{graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC},
            .binding_index{0},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::STORAGE_IMAGE},
            .binding_index{1},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::STORAGE_IMAGE},
            .binding_index{3},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::STORAGE_IMAGE},
            .binding_index{4},
            .binding_count{1},
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set{0},
            .bindings{set0_bindings},
        },
    };

    return device.create_compute_pipeline({
        .layout{
            .set_layouts{layouts},
        },
        .compute_shader{
            .bytes = compute_shader,
            .entrypoint = "AssembleMaps",
            .name = "FFT Assemble Maps Module",
        },
        .name = "FFT Assemble Maps Pipeline",
    });
}

graphics::graphics_pipeline_resource_handle create_water_graphics(graphics::render_device& device)
{
    auto vertex_shader = core::read_bytes("data/fft_water/fft_water.vx.spv");
    auto fragment_shader = core::read_bytes("data/fft_water/fft_water.px.spv");

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
            .type = graphics::descriptor_binding_type::SAMPLED_IMAGE,
            .binding_index = 4,
            .binding_count = 1,
        },
        {
            .type = graphics::descriptor_binding_type::SAMPLED_IMAGE,
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

    graphics::resource_format color_buffer_fmt[] = {graphics::resource_format::RGBA8_SRGB};
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
            .color_attachment_formats = color_buffer_fmt,
            .depth_attachment_format = graphics::resource_format::D32_FLOAT,
        },
        .vertex_shader{
            .bytes = vertex_shader,
            .entrypoint = "VSMain",
            .name = "Water Vertex Shader Module",
        },
        .fragment_shader{
            .bytes = fragment_shader,
            .entrypoint = "PSMain",
            .name = "Water Fragment Shader Module",
        },
        .depth_testing{
            .enable_test = true,
            .enable_write = true,
            .depth_test_op = graphics::compare_operation::GREATER_OR_EQUALS,
        },
        .blending{
            .attachment_blend_ops = blending,
        },
        .name = "Water Graphics Pipeline",
    });
}

graphics::mesh_layout create_water_plane(std::vector<std::uint32_t>& data)
{
    const unsigned int vertex_count = 128;

    graphics::mesh_layout layout = {
        .mesh_start_offset = 0,
        .positions_offset = 0,
    };

    std::size_t write_index = data.size();
    auto word_count = ((vertex_count + 1) * (vertex_count + 1) * (3 + 2 + 3 + 3)) + (vertex_count * vertex_count * 6);
    data.resize(data.size() + word_count);

    for (auto x = 0u; x <= vertex_count; ++x)
    {
        for (auto z = 0u; z <= vertex_count; ++z)
        {
            auto x_pos = (static_cast<float>(x) / vertex_count * 16.0f) - 8.0f;
            auto y_pos = 0;
            auto z_pos = (static_cast<float>(z) / vertex_count * 16.0f) - 8.0f;

            data[write_index++] = std::bit_cast<std::uint32_t>(x_pos);
            data[write_index++] = std::bit_cast<std::uint32_t>(y_pos);
            data[write_index++] = std::bit_cast<std::uint32_t>(z_pos);
        }
    }

    layout.interleave_offset = static_cast<std::uint32_t>(write_index) * sizeof(std::uint32_t);
    layout.interleave_stride = 32; // UV + Normal + Tangent
    layout.uvs_offset = 0;
    layout.normals_offset = 8;
    layout.tangents_offset = 20;

    for (auto x = 0u; x <= vertex_count; ++x)
    {
        for (auto z = 0u; z <= vertex_count; ++z)
        {
            auto uv_x = static_cast<float>(x) / vertex_count;
            auto uv_y = static_cast<float>(z) / vertex_count;
            auto normal_x = 0.0f;
            auto normal_y = 1.0f;
            auto normal_z = 0.0f;
            auto tangent_x = 1.0f;
            auto tangent_y = 0.0f;
            auto tangent_z = 0.0f;

            data[write_index++] = std::bit_cast<std::uint32_t>(uv_x);
            data[write_index++] = std::bit_cast<std::uint32_t>(uv_y);
            data[write_index++] = std::bit_cast<std::uint32_t>(normal_x);
            data[write_index++] = std::bit_cast<std::uint32_t>(normal_y);
            data[write_index++] = std::bit_cast<std::uint32_t>(normal_z);
            data[write_index++] = std::bit_cast<std::uint32_t>(tangent_x);
            data[write_index++] = std::bit_cast<std::uint32_t>(tangent_y);
            data[write_index++] = std::bit_cast<std::uint32_t>(tangent_z);
        }
    }

    layout.index_count = (vertex_count * vertex_count * 6);
    layout.index_offset = static_cast<std::uint32_t>(write_index * sizeof(std::uint32_t));

    for (unsigned int vi = 0, x = 0; x < vertex_count; ++vi, ++x)
    {
        for (unsigned int z = 0; z < vertex_count; write_index += 6, ++vi, ++z)
        {
            data[write_index + 0] = vi;
            data[write_index + 1] = vi + vertex_count + 2;
            data[write_index + 2] = vi + 1;
            data[write_index + 3] = vi;
            data[write_index + 4] = vi + vertex_count + 1;
            data[write_index + 5] = vi + vertex_count + 2;
        }
    }

    return layout;
}

namespace
{
    float jonswap_alpha(float gravity, float fetch, float wind_speed)
    {
        return 0.076f * std::powf(gravity * fetch / wind_speed / wind_speed, -0.22f);
    }

    float jonswap_frequency(float gravity, float fetch, float wind_speed)
    {
        return 22 * std::powf(wind_speed * fetch / gravity / gravity, -0.33f);
    }
} // namespace

wave_spectrum wave_spectrum_ui::to_model(float gravity) const noexcept
{
    return wave_spectrum{
        .scale = scale,
        .angle = math::as_radians(wind_direction),
        .spread_blend = spread_blend,
        .swell = swell,
        .alpha = jonswap_alpha(gravity, fetch, wind_speed),
        .peak_omega = jonswap_frequency(gravity, fetch, wind_speed),
        .gamma = peak_enhancement,
        .short_waves_fade = short_waves_fade,
    };
}
