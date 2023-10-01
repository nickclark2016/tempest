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

    auto visibility_buffer = rgc->create_image({
        .width{1920},
        .height{1080},
        .fmt{graphics::resource_format::RG32_UINT},
        .type{graphics::image_type::IMAGE_2D},
        .name{"visibility_buffer"},
    });

    auto depth_buffer = rgc->create_image({
        .width{1920},
        .height{1080},
        .fmt{graphics::resource_format::D32_FLOAT},
        .type{graphics::image_type::IMAGE_2D},
        .name{"depth_buffer"},
    });

    auto vertex_data_buffer = rgc->create_buffer({
        .size{1024 * 1024 * 1024},
        .location{graphics::memory_location::DEVICE},
    });

    auto object_data_buffer = rgc->create_buffer({
        .size{1024 * 1024 * 128 * 3},
    });

    auto scene_data_buffer = rgc->create_buffer({
        .size{1024 * 16 * 3},
    });

    auto material_data_buffer = rgc->create_buffer({
        .size{1024 * 64},
        .location{graphics::memory_location::DEVICE},
    });

    auto material_start_buffer = rgc->create_buffer({
        .size{1024 * 4},
        .location{graphics::memory_location::DEVICE},
    });

    auto material_count_buffer = rgc->create_buffer({
        .size{1024 * 4},
        .location{graphics::memory_location::DEVICE},
    });

    auto pixel_xy_buffer = rgc->create_buffer({
        .size{1920 * 1080 * 8},
        .location{graphics::memory_location::DEVICE},
    });

    auto indirect_commands_buffer = rgc->create_buffer({
        .size{1024 * 12 * 3},
    });

    auto visibility_clear_pass =
        rgc->add_graph_pass("visibility_buffer_start_clear", [&](graphics::graph_pass_builder& bldr) {
            bldr.add_rw_structured_buffer(material_count_buffer)
                .add_rw_structured_buffer(material_start_buffer)
                .on_execute([&](graphics::command_list& cmds) {});
        });

    auto visibility_buffer_pass = rgc->add_graph_pass("visibility_buffer", [&](graphics::graph_pass_builder& bldr) {
        bldr.add_color_output(visibility_buffer)
            .add_depth_output(depth_buffer)
            .add_structured_buffer(vertex_data_buffer)
            .add_structured_buffer(object_data_buffer)
            .add_constant_buffer(scene_data_buffer)
            .on_execute([&](graphics::command_list& cmds) {});
    });

    auto material_count_pass =
        rgc->add_graph_pass("visibility_material_count", [&](graphics::graph_pass_builder& bldr) {
            bldr.add_storage_image(visibility_buffer)
                .add_rw_structured_buffer(material_count_buffer)
                .on_execute([&](graphics::command_list& cmds) {});
        });

    auto material_start_pass =
        rgc->add_graph_pass("visibility_material_start", [&](graphics::graph_pass_builder& bldr) {
            bldr.add_structured_buffer(material_count_buffer)
                .add_rw_structured_buffer(material_start_buffer)
                .on_execute([&](graphics::command_list& cmds) {});
        });

    auto material_start_clear_pass =
        rgc->add_graph_pass("visibility_material_count_clear_pass", [&](graphics::graph_pass_builder& bldr) {
            bldr.add_rw_structured_buffer(material_count_buffer).on_execute([&](graphics::command_list& cmds) {});
        });

    auto material_pixel_sort_pass =
        rgc->add_graph_pass("visibility_material_sort", [&](graphics::graph_pass_builder& bldr) {
            bldr.add_storage_image(visibility_buffer)
                .add_rw_structured_buffer(material_count_buffer)
                .add_rw_structured_buffer(pixel_xy_buffer)
                .add_structured_buffer(material_start_buffer)
                .on_execute([&](graphics::command_list& cmds) {});
        });

    auto graph = std::move(*rgc).compile();

    while (!win->should_close())
    {
        input::poll();

        graphics_device.start_frame();

        graph->execute();

        graphics_device.end_frame();
    }
}