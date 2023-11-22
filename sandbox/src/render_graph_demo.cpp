#include <tempest/files.hpp>
#include <tempest/input.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_device.hpp>
#include <tempest/render_graph.hpp>
#include <tempest/window.hpp>

using namespace tempest;

namespace
{
    inline constexpr std::size_t global_memory_allocator_size = 1024 * 1024 * 64;
}

graphics::graphics_pipeline_resource_handle create_visbuffer_pipeline(graphics::render_device& device);

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

    auto rgc = graphics::render_graph_compiler::create_compiler(&global_allocator, &graphics_device);

    auto depth_buffer = rgc->create_image({
        .width{1920},
        .height{1080},
        .fmt{graphics::resource_format::D32_FLOAT},
        .type{graphics::image_type::IMAGE_2D},
        .name{"depth_buffer"},
    });

    auto color_buffer = rgc->create_image({
        .width{1920},
        .height{1080},
        .fmt{graphics::resource_format::RGBA8_SRGB},
        .type{graphics::image_type::IMAGE_2D},
        .name{"color_target"},
    });

    auto vertex_data_buffer = rgc->create_buffer({
        .size{1024 * 1024 * 1024},
        .location{graphics::memory_location::DEVICE},
        .name{"Vertex Data Buffer"},
    });

    auto object_data_buffer = rgc->create_buffer({
        .size{1024 * 1024 * 128 * 3},
        .name{"vertex_data_buffer"},
    });

    auto scene_data_buffer = rgc->create_buffer({
        .size{1024 * 16 * 3},
        .name{"scene_data_buffer"},
    });

    auto material_data_buffer = rgc->create_buffer({
        .size{1024 * 64},
        .location{graphics::memory_location::DEVICE},
        .name{"material_data_buffer"},
    });

    auto indirect_commands_buffer = rgc->create_buffer({
        .size{1024 * 12 * 3},
        .location{graphics::memory_location::DEVICE},
        .name{"indirect_arguments_buffer"},
    });

    auto swapchain = graphics_device.create_swapchain({.win{win.get()}, .desired_frame_count{3}});

    auto depth_prepass = rgc->add_graph_pass(
        "depth_prepass", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_depth_attachment(depth_buffer, graphics::resource_access_type::READ_WRITE)
                .add_constant_buffer(scene_data_buffer)
                .add_structured_buffer(vertex_data_buffer, graphics::resource_access_type::READ)
                .add_structured_buffer(object_data_buffer, graphics::resource_access_type::READ)
                .add_indirect_argument_buffer(indirect_commands_buffer)
                .on_execute([&](auto& cmds) {});
        });

    auto forward_opaque_pass = rgc->add_graph_pass(
        "forward_opaque", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_depth_attachment(depth_buffer, graphics::resource_access_type::READ)
                .add_color_attachment(color_buffer, graphics::resource_access_type::WRITE)
                .add_structured_buffer(vertex_data_buffer, graphics::resource_access_type::READ)
                .add_structured_buffer(object_data_buffer, graphics::resource_access_type::READ)
                .add_structured_buffer(material_data_buffer, graphics::resource_access_type::READ)
                .add_indirect_argument_buffer(indirect_commands_buffer)
                .depends_on(depth_prepass)
                .on_execute([&](auto& cmds) {});
        });

    auto forward_transparencies_pass = rgc->add_graph_pass(
        "forward_transparent", graphics::queue_operation_type::GRAPHICS, [&](graphics::graph_pass_builder& bldr) {
            bldr.add_depth_attachment(depth_buffer, graphics::resource_access_type::READ_WRITE)
                .add_color_attachment(color_buffer, graphics::resource_access_type::WRITE)
                .add_structured_buffer(vertex_data_buffer, graphics::resource_access_type::READ)
                .add_structured_buffer(object_data_buffer, graphics::resource_access_type::READ)
                .add_structured_buffer(material_data_buffer, graphics::resource_access_type::READ)
                .add_indirect_argument_buffer(indirect_commands_buffer)
                .depends_on(forward_opaque_pass)
                .on_execute([&](auto& cmds) {});
        });

    auto blit_pass =
        rgc->add_graph_pass("swapchain_target_blit_pass", graphics::queue_operation_type::GRAPHICS_AND_TRANSFER,
                            [&](graphics::graph_pass_builder& bldr) {
                                bldr.add_blit_source(color_buffer)
                                    .add_external_blit_target(swapchain)
                                    .depends_on(forward_transparencies_pass)
                                    .on_execute([&](auto& cmds) {});
                            });

    auto graph = std::move(*rgc).compile();

    while (!win->should_close())
    {
        input::poll();

        graphics_device.start_frame();

        graph->execute();

        graphics_device.end_frame();
    }

    graphics_device.release_swapchain(swapchain);
}