#include <tempest/files.hpp>
#include <tempest/input.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>
#include <tempest/window.hpp>

#include <chrono>
#include <iostream>

using namespace tempest;

namespace
{
    inline constexpr std::size_t global_memory_allocator_size = 1024 * 1024 * 64;
}

graphics::graphics_pipeline_resource_handle create_textured_quad_pipeline(graphics::render_device& device);

void render_graph_demo()
{
    auto global_allocator = core::heap_allocator(global_memory_allocator_size);

    auto win = graphics::window_factory::create({
        .title{"Tempest Render Graph Demo"},
        .width{1920},
        .height{1080},
    });

    auto graphics_ctx = graphics::render_context::create(&global_allocator);
    auto& graphics_device = graphics_ctx->get_device(0);

    auto quad_pipeline = create_textured_quad_pipeline(graphics_device);

    auto rgc = graphics::render_graph_compiler::create_compiler(&global_allocator, &graphics_device);

    auto color_buffer = rgc->create_image({
        .width{1920},
        .height{1080},
        .fmt{graphics::resource_format::RGBA8_SRGB},
        .type{graphics::image_type::IMAGE_2D},
        .name{"Color Buffer Target"},
    });

    auto vertex_buffer = rgc->create_buffer({
        .size{sizeof(float) * 8 * 6},
        .location{graphics::memory_location::DEVICE},
        .name{"Vertex Buffer"},
        .per_frame_memory{false},
    });

    auto texture_sampler = graphics_device.create_sampler({
        .mag{graphics::filter::NEAREST},
        .min{graphics::filter::NEAREST},
        .mipmap{graphics::mipmap_mode::LINEAR},
        .mip_lod_bias{0.0f},
        .name{"Linear Sampler"},
    });

    auto swapchain = graphics_device.create_swapchain({.win{win.get()}, .desired_frame_count{3}});

    // clang-format off
    std::uint8_t pixels[] = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 255, 255,
    };

    float vertices[] = {
        -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
    };
    // clang-format on

    graphics::texture_data_descriptor texture_data[] = {
        {
            .fmt{graphics::resource_format::RGBA8_UNORM},
            .mips{
                graphics::texture_mip_descriptor{
                    .width{2},
                    .height{2},
                    .bytes{reinterpret_cast<std::byte*>(pixels), 16},
                },
            },
            .name{"Test Texture"},
        },
    };

    auto textures = graphics::renderer_utilities::upload_textures(graphics_device, texture_data,
                                                                  graphics_device.get_staging_buffer(), false);

    auto quad_pass = rgc->add_graph_pass(
        "quad_pass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_color_attachment(color_buffer, graphics::resource_access_type::WRITE, graphics::load_op::CLEAR)
                .add_structured_buffer(vertex_buffer, graphics::resource_access_type::READ, 0, 0)
                .add_external_sampled_images(textures, 0, 1, graphics::pipeline_stage::FRAGMENT)
                .add_sampler(texture_sampler, 0, 2, graphics::pipeline_stage::FRAGMENT)
                .on_execute([&](graphics::command_list& cmds) {
                    cmds.set_viewport(0, 0, 1920, 1080)
                        .set_scissor_region(0, 0, 1920, 1080)
                        .use_pipeline(quad_pipeline)
                        .draw(6);
                });
        });

    auto blit_pass =
        rgc->add_graph_pass("swapchain_target_blit_pass", graphics::queue_operation_type::GRAPHICS_AND_TRANSFER,
                            [&](graphics::graph_pass_builder& bldr) {
                                bldr.add_blit_source(color_buffer)
                                    .add_external_blit_target(swapchain)
                                    .depends_on(quad_pass)
                                    .on_execute([&](graphics::command_list& cmds) {
                                        cmds.blit(color_buffer, graphics_device.fetch_current_image(swapchain));
                                    });
                            });

    auto graph = std::move(*rgc).compile();

    {
        auto staging_buffer_ptr = graphics_device.map_buffer(graphics_device.get_staging_buffer());
        std::memcpy(staging_buffer_ptr.data(), vertices, sizeof(vertices));
        graphics_device.unmap_buffer(graphics_device.get_staging_buffer());

        auto& cmd_executor = graphics_device.get_command_executor();
        auto& cmds = cmd_executor.get_commands();

        cmds.copy(graphics_device.get_staging_buffer(), vertex_buffer, 0, 0, sizeof(vertices));

        cmd_executor.submit_and_wait();
    }

    auto last_time = std::chrono::high_resolution_clock::now();
    std::uint32_t fps_counter = 0;

    while (!win->should_close())
    {
        input::poll();

        graph->execute();

        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> frame_time = current_time - last_time;

        ++fps_counter;

        if (frame_time.count() >= 1.0)
        {
            std::cout << fps_counter << " FPS" << std::endl;
            fps_counter = 0;
            last_time = current_time;
        }
    }

    for (auto texture : textures)
    {
        graphics_device.release_image(texture);
    }

    graphics_device.release_sampler(texture_sampler);
    graphics_device.release_graphics_pipeline(quad_pipeline);
    graphics_device.release_swapchain(swapchain);
}

graphics::graphics_pipeline_resource_handle create_textured_quad_pipeline(graphics::render_device& device)
{
    auto vertex_shader_bytes = core::read_bytes("data/simple_quad/simple_quad.vx.spv");
    auto fragment_shader_bytes = core::read_bytes("data/simple_quad/simple_quad.px.spv");

    graphics::resource_format color_buffer_fmt[] = {graphics::resource_format::RGBA8_SRGB};
    graphics::color_blend_attachment_state blending[] = {
        {
            .enabled{false},
        },
    };

    graphics::descriptor_binding_info bindings[] = {
        {
            .type{graphics::descriptor_binding_type::STRUCTURED_BUFFER},
            .binding_index{0},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::SAMPLED_IMAGE},
            .binding_index{1},
            .binding_count{1},
        },
        {
            .type{graphics::descriptor_binding_type::SAMPLER},
            .binding_index{2},
            .binding_count{1},
        },
    };

    graphics::descriptor_set_layout_create_info layout = {
        .set{0},
        .bindings{bindings},
    };

    graphics::descriptor_set_layout_create_info layouts[] = {
        layout,
    };

    graphics::graphics_pipeline_create_info quad_pipeline_ci = {
        .layout{
            .set_layouts{
                layouts,
            },
        },
        .target{
            .color_attachment_formats{color_buffer_fmt},
        },
        .vertex_shader{
            .bytes{vertex_shader_bytes},
            .entrypoint{"VSMain"},
            .name{"simple_quad_vertex_shader"},
        },
        .fragment_shader{
            .bytes{fragment_shader_bytes},
            .entrypoint{"PSMain"},
            .name{"simple_quad_fragment_shader"},
        },
        .vertex_layout{},
        .depth_testing{
            .enable_test{false},
            .enable_write{false},
        },

        .blending{.attachment_blend_ops{blending}},
        .name{"Textured Quad Pipeline"},
    };

    auto handle = device.create_graphics_pipeline(quad_pipeline_ci);
    return handle;
}