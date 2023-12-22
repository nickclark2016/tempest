#include <tempest/imgui_context.hpp>
#include <tempest/input.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>
#include <tempest/window.hpp>

#include <cstddef>
#include <iostream>

using namespace tempest;

namespace
{
    inline constexpr std::size_t global_memory_allocator_size = 1024 * 1024 * 64;
}

void draw_gui();

void fft_water_demo()
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

    auto state_upload_pass =
        rgc->add_graph_pass("Water Sim State Buffer Upload Graph Pass", graphics::queue_operation_type::TRANSFER,
                            [&](graphics::graph_pass_builder& bldr) {
                                bldr.add_transfer_source_buffer(graphics_device.get_staging_buffer())
                                    .on_execute([&](graphics::command_list& cmds) {

                                    });
                            });

    auto water_sim_pass = rgc->add_graph_pass(
        "Water Simulation Graph Pass", graphics::queue_operation_type::GRAPHICS,
        [&](graphics::graph_pass_builder& bldr) {
            bldr.add_color_attachment(color_buffer, graphics::resource_access_type::WRITE, graphics::load_op::CLEAR,
                                      graphics::store_op::STORE, math::vec4<float>(0.0f))
                .add_depth_attachment(depth_buffer, graphics::resource_access_type::READ_WRITE,
                                      graphics::load_op::CLEAR, graphics::store_op::STORE, 1.0f)
                .depends_on(state_upload_pass)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.set_viewport(0, 0, 1920, 1080).set_scissor_region(0, 0, 1920, 1080);
                });
        });

    auto imgui_pass = rgc->add_graph_pass(
        "ImGUI Graph Pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_color_attachment(color_buffer, graphics::resource_access_type::WRITE, graphics::load_op::LOAD,
                                      graphics::store_op::STORE)
                .draw_imgui()
                .depends_on(water_sim_pass)
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

    while (!win->should_close())
    {
        input::poll();
        draw_gui();
        graph->execute();
    }

    graphics::imgui_context::shutdown();

    graphics_device.release_swapchain(swapchain);
}

void draw_gui()
{
    graphics::imgui_context::create_frame([]() {
        graphics::imgui_context::create_window("FFT Water Demo", []() {

        });
    });
}