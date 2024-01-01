#include <tempest/files.hpp>
#include <tempest/imgui_context.hpp>
#include <tempest/input.hpp>
#include <tempest/mat4.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>
#include <tempest/texture_asset.hpp>
#include <tempest/transformations.hpp>
#include <tempest/window.hpp>

#include <chrono>
#include <iostream>
#include <numbers>
#include <random>

using namespace tempest;

namespace
{
    inline constexpr std::size_t global_memory_allocator_size = 1024 * 1024 * 64;
}

struct camera_data
{
    math::mat4<float> proj;
    math::mat4<float> view;
    math::mat4<float> view_proj;
    math::vec3<float> position;
};

struct alignas(16) wave_parameter
{
    math::vec2<float> direction;
    float frequency;
    float amplitude;
    float phase;
    float steepness;
};

struct alignas(16) water_sim_state
{
    float frequency;
    float frequency_multiplier;
    float initial_seed;
    float seed_iter;
    float amplitude;
    float amplitude_multiplier;
    float initial_speed;
    float speed_ramp;
    float drag;
    float height;
    float max_peak;
    float peak_offset;

    float time;
    int num_waves;
};

graphics::graphics_pipeline_resource_handle create_water_pipeline(graphics::render_device& device);
water_sim_state generate_water_sim_state(int num_waves);
void draw_gui(water_sim_state& state, std::uint32_t fps);

void fbm_water_demo()
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

    auto water_pipeline = create_water_pipeline(graphics_device);

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

    auto camera_data_buffer = rgc->create_buffer({
        .size{sizeof(camera_data)},
        .location{graphics::memory_location::DEVICE},
        .name{"Camera Data Buffer"},
        .per_frame_memory{true},
    });

    auto lighting_data_buffer = rgc->create_buffer({
        .size{sizeof(graphics::directional_light)},
        .location{graphics::memory_location::DEVICE},
        .name{"Lighting Data Buffer"},
        .per_frame_memory{true},
    });

    auto wave_data_buffer = rgc->create_buffer({
        .size{sizeof(water_sim_state)},
        .location{graphics::memory_location::DEVICE},
        .name{"Simulation Parameter Buffer"},
        .per_frame_memory{true},
    });

    auto win = graphics::window_factory::create({
        .title{"Tempest Render Graph Demo"},
        .width{1920},
        .height{1080},
    });

    graphics::imgui_context::initialize_for_window(*win);

    auto swapchain = graphics_device.create_swapchain({.win{win.get()}, .desired_frame_count{3}});

    auto water_sim_state = generate_water_sim_state(16);

    camera_data cameras = {
        .proj{math::perspective(16.0f / 9.0f, 90.0f * 9.0f / 16.0f, 0.01f)},
        .view{math::look_direction(math::vec3<float>(0.0f, 10.0f, 0.0f), math::vec3<float>(15.0f, 2.0f, 15.0f),
                            math::vec3<float>(0.0f, 1.0, 0.0f))},
        .view_proj{1.0f},
        .position{0.0f, 10.0f, 0.0f},
    };

    auto state_upload_pass = rgc->add_graph_pass(
        "Water Sim State Buffer Upload Graph Pass", graphics::queue_operation_type::TRANSFER, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_transfer_source_buffer(graphics_device.get_staging_buffer())
                .add_transfer_destination_buffer(camera_data_buffer)
                .add_transfer_destination_buffer(wave_data_buffer)
                .on_execute([&](graphics::command_list& cmds) {
                    auto sb_ptr = graphics_device.map_buffer_frame(graphics_device.get_staging_buffer());
                    std::memcpy(sb_ptr.data(), &water_sim_state, sizeof(water_sim_state));
                    std::memcpy(sb_ptr.data() + sizeof(water_sim_state), &cameras, sizeof(camera_data));
                    graphics_device.unmap_buffer(graphics_device.get_staging_buffer());
                    cmds.copy(graphics_device.get_staging_buffer(), wave_data_buffer, 0,
                              graphics_device.get_buffer_frame_offset(wave_data_buffer));
                    cmds.copy(graphics_device.get_staging_buffer(), camera_data_buffer, sizeof(water_sim_state),
                              graphics_device.get_buffer_frame_offset(camera_data_buffer));
                });
        });

    auto water_sim_pass = rgc->add_graph_pass(
        "Water Simulation Graph Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_color_attachment(color_buffer, graphics::resource_access_type::WRITE, graphics::load_op::CLEAR,
                                      graphics::store_op::STORE, math::vec4<float>(0.0f))
                .add_depth_attachment(depth_buffer, graphics::resource_access_type::READ_WRITE,
                                      graphics::load_op::CLEAR, graphics::store_op::STORE, 0.0f)
                .add_constant_buffer(camera_data_buffer, 0, 0)
                .add_constant_buffer(lighting_data_buffer, 0, 1)
                .add_constant_buffer(wave_data_buffer, 0, 2)
                .depends_on(state_upload_pass)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.set_viewport(0, 0, 1920, 1080)
                        .set_scissor_region(0, 0, 1920, 1080)
                        .use_pipeline(water_pipeline)
                        .draw(1024 * 1024 * 6);
                });
        });

    auto imgui_pass =
        rgc->add_graph_pass("ImGUI Graph Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_color_attachment(color_buffer, graphics::resource_access_type::WRITE, graphics::load_op::LOAD,
                                      graphics::store_op::STORE)
                .draw_imgui()
                .depends_on(water_sim_pass)
                .on_execute([](auto& cmds) {});
        });

    auto blit_pass =
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
        auto staging_buffer_ptr = graphics_device.map_buffer(graphics_device.get_staging_buffer());
        std::memcpy(staging_buffer_ptr.data(), &cameras, sizeof(camera_data));
        graphics_device.unmap_buffer(graphics_device.get_staging_buffer());

        auto& cmd_executor = graphics_device.get_command_executor();
        auto& cmds = cmd_executor.get_commands();

        for (std::size_t i = 0; i < graphics_device.frames_in_flight(); ++i)
        {
            cmds.copy(graphics_device.get_staging_buffer(), camera_data_buffer, 0,
                      graphics_device.get_buffer_frame_offset(camera_data_buffer, i), sizeof(camera_data));
        }

        cmd_executor.submit_and_wait();
    }

    {
        graphics::directional_light sun = {
            .light_direction{-1.0, 1.0, -1.0f},
            .color_illum{1.0f, 1.0f, 1.0f, 25000.0f},
        };

        auto staging_buffer_ptr = graphics_device.map_buffer(graphics_device.get_staging_buffer());
        std::memcpy(staging_buffer_ptr.data(), &sun, sizeof(graphics::directional_light));
        graphics_device.unmap_buffer(graphics_device.get_staging_buffer());

        auto& cmd_executor = graphics_device.get_command_executor();
        auto& cmds = cmd_executor.get_commands();

        for (std::size_t i = 0; i < graphics_device.frames_in_flight(); ++i)
        {
            cmds.copy(graphics_device.get_staging_buffer(), lighting_data_buffer, 0,
                      graphics_device.get_buffer_frame_offset(lighting_data_buffer, i),
                      sizeof(graphics::directional_light));
        }

        cmd_executor.submit_and_wait();
    }

    auto last_tick_time = std::chrono::high_resolution_clock::now();
    auto last_frame_time = last_tick_time;
    std::uint32_t fps_counter = 0;
    std::uint32_t last_fps = 0;

    while (!win->should_close())
    {
        input::poll();

        draw_gui(water_sim_state, last_fps);

        graph->execute();

        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> time_since_tick = current_time - last_tick_time;
        std::chrono::duration<double> frame_time = current_time - last_frame_time;
        last_frame_time = current_time;

        float ft = static_cast<float>(frame_time.count());
        water_sim_state.time += ft;
        cameras.position.x += (ft * 3.0f);
        cameras.position.z += (ft * 3.0f);
        cameras.view = math::look_at(cameras.position, cameras.position + math::vec3<float>(15, -8, 15),
                                     math::vec3<float>(0.0f, 1.0f, 0.0f));

        ++fps_counter;

        if (time_since_tick.count() >= 1.0)
        {
            last_fps = fps_counter;
            fps_counter = 0;
            last_tick_time = current_time;
        }
    }

    graphics_device.release_graphics_pipeline(water_pipeline);
    graphics_device.release_swapchain(swapchain);

    graphics::imgui_context::shutdown();
}

graphics::graphics_pipeline_resource_handle create_water_pipeline(graphics::render_device& device)
{
    auto vertex_shader_bytes = core::read_bytes("data/water/water.vx.spv");
    auto fragment_shader_bytes = core::read_bytes("data/water/water.px.spv");

    graphics::resource_format color_buffer_fmt[] = {graphics::resource_format::RGBA8_SRGB};
    graphics::color_blend_attachment_state blending[] = {
        {
            .enabled{false},
        },
    };

    graphics::descriptor_binding_info buffer_bindings[] = {
        {
            .type{graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC},
            .binding_index{0},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC},
            .binding_index{1},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::CONSTANT_BUFFER_DYNAMIC},
            .binding_index{2},
            .binding_count{1},
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set{0},
            .bindings{buffer_bindings},
        },
    };

    graphics::graphics_pipeline_create_info quad_pipeline_ci = {
        .layout{
            .set_layouts{
                layouts,
            },
        },
        .target{
            .color_attachment_formats{color_buffer_fmt},
            .depth_attachment_format{graphics::resource_format::D32_FLOAT},
        },
        .vertex_shader{
            .bytes{vertex_shader_bytes},
            .entrypoint{"VSMain"},
            .name{"water_vertex_shader"},
        },
        .fragment_shader{
            .bytes{fragment_shader_bytes},
            .entrypoint{"PSMain"},
            .name{"water_fragment_shader"},
        },
        .vertex_layout{},
        .depth_testing{
            .enable_test{true},
            .enable_write{true},
            .depth_test_op{graphics::compare_operation::GREATER_OR_EQUALS},
        },

        .blending{.attachment_blend_ops{blending}},
        .name{"Water Pipeline"},
    };

    auto handle = device.create_graphics_pipeline(quad_pipeline_ci);
    return handle;
}

water_sim_state generate_water_sim_state(int num_waves)
{
    water_sim_state state{
        .frequency{1.0f},
        .frequency_multiplier{1.16f},
        .initial_seed{4.0f},
        .seed_iter{4.3f},
        .amplitude{1.0f},
        .amplitude_multiplier{0.83f},
        .initial_speed{2.0f},
        .speed_ramp{1.07f},
        .drag{0.5f},
        .height{1.48f},
        .max_peak{1.0f},
        .peak_offset{1.14f},
        .time{0},
        .num_waves{num_waves},
    };

    return state;
}

void draw_gui(water_sim_state& water, std::uint32_t fps)
{
    graphics::imgui_context::create_frame([&]() {
        graphics::imgui_context::create_window("Editor", [&]() {
            graphics::imgui_context::create_tree_node("Performance Metrics", [&]() {
                graphics::imgui_context::create_table("##Performance Metrics", 2, [&]() {
                    graphics::imgui_context::next_row();
                    graphics::imgui_context::next_column();
                    graphics::imgui_context::label("Frames per Second");
                    graphics::imgui_context::next_column();
                    graphics::imgui_context::label(std::format("{}", fps));
                });
            });

            graphics::imgui_context::create_tree_node("Water Simulation Parameters", [&]() {
                graphics::imgui_context::create_table("##Water Simulation Properties", 2, [&]() {
                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Wave Frequency");
                        graphics::imgui_context::next_column();
                        water.frequency = graphics::imgui_context::float_slider("##wave_freq", 0, 10, water.frequency);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Wave Frequency Multiplier");
                        graphics::imgui_context::next_column();
                        water.frequency_multiplier = graphics::imgui_context::float_slider(
                            "##wave_freq_multiplier", 0, 5, water.frequency_multiplier);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Initial Seed");
                        graphics::imgui_context::next_column();
                        water.initial_seed =
                            graphics::imgui_context::float_slider("##initial_seed", -1024, 1024, water.initial_seed);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Seed Iterator");
                        graphics::imgui_context::next_column();
                        water.seed_iter =
                            graphics::imgui_context::float_slider("##seed_iter", -1024, 1024, water.seed_iter);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Wave Amplitude");
                        graphics::imgui_context::next_column();
                        water.amplitude =
                            graphics::imgui_context::float_slider("##wave_amplitude", 0, 5, water.amplitude);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Wave Amplitude Multiplier");
                        graphics::imgui_context::next_column();
                        water.amplitude_multiplier = graphics::imgui_context::float_slider(
                            "##wave_amplitude_multiplier", 0, 1, water.amplitude_multiplier);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Initial Wave Speed");
                        graphics::imgui_context::next_column();
                        water.initial_speed =
                            graphics::imgui_context::float_slider("##wave_speed", 0, 10, water.initial_seed);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Wave Speed Ramp");
                        graphics::imgui_context::next_column();
                        water.speed_ramp =
                            graphics::imgui_context::float_slider("##wave_speed_ramp", 0, 10, water.speed_ramp);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Wave Drag");
                        graphics::imgui_context::next_column();
                        water.drag = graphics::imgui_context::float_slider("##wave_drag", 0, 1, water.drag);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Wave Height");
                        graphics::imgui_context::next_column();
                        water.height = graphics::imgui_context::float_slider("##wave_height", 0, 10, water.height);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Wave Max Peak");
                        graphics::imgui_context::next_column();
                        water.max_peak =
                            graphics::imgui_context::float_slider("##wave_max_peak", 0, 10, water.max_peak);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Wave Peak Offset");
                        graphics::imgui_context::next_column();
                        water.peak_offset =
                            graphics::imgui_context::float_slider("##wave_peak_offset", 0, 10, water.peak_offset);
                    }

                    {
                        graphics::imgui_context::next_row();
                        graphics::imgui_context::next_column();
                        graphics::imgui_context::label("Wave Count");
                        graphics::imgui_context::next_column();
                        water.num_waves = graphics::imgui_context::int_slider("##wave_count", 0, 256, water.num_waves);
                    }
                });
            });
        });
    });
}