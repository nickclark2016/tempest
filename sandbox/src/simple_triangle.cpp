#include <tempest/files.hpp>
#include <tempest/input.hpp>
#include <tempest/mat4.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>
#include <tempest/texture_asset.hpp>
#include <tempest/transformations.hpp>
#include <tempest/window.hpp>

#include <chrono>
#include <cstring>
#include <iostream>
#include <numbers>
#include <random>

using namespace tempest;

namespace
{
    inline constexpr std::size_t global_memory_allocator_size = 1024 * 1024 * 64;
}

graphics::graphics_pipeline_resource_handle create_triangle_pipeline(graphics::render_device& device);

void simple_triangle_demo()
{
    auto global_allocator = heap_allocator(global_memory_allocator_size);

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

    auto triangle_pipeline = create_triangle_pipeline(graphics_device);

    auto rgc = graphics::render_graph_compiler::create_compiler(&global_allocator, &graphics_device);

    auto color_buffer = rgc->create_image({
        .width = 1920,
        .height = 1080,
        .fmt = graphics::resource_format::RGBA8_SRGB,
        .type = graphics::image_type::IMAGE_2D,
        .name = "Color Buffer Target",
    });

    auto depth_buffer = rgc->create_image({
        .width = 1920,
        .height = 1080,
        .fmt = graphics::resource_format::D32_FLOAT,
        .type = graphics::image_type::IMAGE_2D,
        .name = "Depth Buffer Target",
    });

    auto vertex_buffer = rgc->create_buffer({
        .size = sizeof(float) * 8 * 3,
        .location = graphics::memory_location::HOST,
        .name = "Vertex Buffer",
        .per_frame_memory = false,
    });

    auto win = graphics::window_factory::create({
        .title = "Tempest Render Graph Demo",
        .width = 1920,
        .height = 1080,
    });

    auto swapchain = graphics_device.create_swapchain({.win = win.get(), .desired_frame_count = 3});

    auto triangle_pass = rgc->add_graph_pass(
        "triangle_pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_color_attachment(color_buffer, graphics::resource_access_type::WRITE, graphics::load_op::CLEAR,
                                      graphics::store_op::STORE, math::vec4<float>(0.0f, 0.0f, 0.0f, 1.0f))
                .add_depth_attachment(depth_buffer, graphics::resource_access_type::READ_WRITE,
                                      graphics::load_op::CLEAR, graphics::store_op::STORE, 1.0f)
                .add_structured_buffer(vertex_buffer, graphics::resource_access_type::READ, 0, 0)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.set_viewport(0, 0, 1920, 1080)
                        .set_scissor_region(0, 0, 1920, 1080)
                        .use_pipeline(triangle_pipeline)
                        .draw(3);
                });
        });

    auto blit_pass =
        rgc->add_graph_pass("swapchain target blit", graphics::queue_operation_type::GRAPHICS_AND_TRANSFER,
                            [&](graphics::graph_pass_builder& bldr) {
                                bldr.add_blit_source(color_buffer)
                                    .add_external_blit_target(swapchain)
                                    .depends_on(triangle_pass)
                                    .on_execute([&](graphics::command_list& cmds) {
                                        cmds.blit(color_buffer, graphics_device.fetch_current_image(swapchain));
                                    });
                            });

    auto graph = std::move(*rgc).compile();

    {
        // clang-format off
        float vertices[] = {
             0.0f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
        };
        // clang-format on

        auto vertex_buffer_data = graphics_device.map_buffer(vertex_buffer);
        std::memcpy(vertex_buffer_data.data(), vertices, sizeof(vertices));
        graphics_device.unmap_buffer(vertex_buffer);
    }

    auto last_tick_time = std::chrono::high_resolution_clock::now();
    auto last_frame_time = last_tick_time;
    std::uint32_t fps_counter = 0;

    while (!win->should_close())
    {
        core::input::poll();

        graph->execute();

        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> time_since_tick = current_time - last_tick_time;
        std::chrono::duration<double> frame_time = current_time - last_frame_time;
        last_frame_time = current_time;

        float ft = static_cast<float>(frame_time.count());

        ++fps_counter;

        if (time_since_tick.count() >= 1.0)
        {
            std::cout << fps_counter << " FPS" << std::endl;
            fps_counter = 0;
            last_tick_time = current_time;
        }
    }

    graphics_device.release_graphics_pipeline(triangle_pipeline);
    graphics_device.release_swapchain(swapchain);
}

graphics::graphics_pipeline_resource_handle create_triangle_pipeline(graphics::render_device& device)
{
    auto vertex_shader_bytes = core::read_bytes("data/simple_triangle/simple_triangle.vx.spv");
    auto fragment_shader_bytes = core::read_bytes("data/simple_triangle/simple_triangle.px.spv");

    graphics::resource_format color_buffer_fmt[] = {graphics::resource_format::RGBA8_SRGB};
    graphics::color_blend_attachment_state blending[] = {
        {
            .enabled = false,
        },
    };

    graphics::descriptor_binding_info buffer_bindings[] = {
        {
            .type = graphics::descriptor_binding_type::STRUCTURED_BUFFER,
            .binding_index = 0,
            .binding_count = 1,
        },
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        {
            .set = 0,
            .bindings = buffer_bindings,
        },
    };

    graphics::graphics_pipeline_create_info quad_pipeline_ci = {
        .layout{
            .set_layouts = layouts,
        },
        .target{
            .color_attachment_formats = color_buffer_fmt,
            .depth_attachment_format = graphics::resource_format::D32_FLOAT,
        },
        .vertex_shader{
            .bytes = vertex_shader_bytes,
            .entrypoint = "VSMain",
            .name = "Triangle Vertex Shader",
        },
        .fragment_shader{
            .bytes = fragment_shader_bytes,
            .entrypoint = "PSMain",
            .name = "Triangle Fragment Shader",
        },
        .vertex_layout{},
        .depth_testing{
            .enable_test = true,
            .enable_write = true,
            .depth_test_op = graphics::compare_operation::LESS,
        },

        .blending{.attachment_blend_ops{blending}},
        .name{"Triangle Pipeline"},
    };

    auto handle = device.create_graphics_pipeline(quad_pipeline_ci);
    return handle;
}
