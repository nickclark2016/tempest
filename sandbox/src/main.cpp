#include <tempest/input.hpp>
#include <tempest/instance.hpp>
#include <tempest/logger.hpp>
#include <tempest/renderer.hpp>
#include <tempest/window.hpp>

#include <cassert>
#include <fstream>
#include <sstream>

namespace
{
    std::vector<uint32_t> read_spirv(const std::string& path)
    {
        std::ostringstream buf;
        std::ifstream input(path.c_str(), std::ios::ate | std::ios::binary);
        assert(input);
        size_t file_size = (size_t)input.tellg();
        std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
        input.seekg(0);
        input.read(reinterpret_cast<char*>(buffer.data()), file_size);
        return buffer;
    }
} // namespace

int main()
{
    auto logger = tempest::logger::logger_factory::create({
        .prefix{"Sandbox"},
    });

    logger->info("Starting Sandbox Application.");

    auto win = tempest::graphics::window_factory::create({
        .title{"Sandbox"},
        .width{1920},
        .height{1080},
    });

    auto renderer = tempest::graphics::irenderer::create(*win);

    auto vert_spv = read_spirv("data/triangle.vs.spv");
    auto frag_spv = read_spirv("data/triangle.fs.spv");
    tempest::graphics::iresource_allocator::shader_source sources[] = {
        {
            .name{"data/triangle.vs.spv"},
            .data{std::move(vert_spv)},
        },
        {
            .name{"data/triangle.fs.spv"},
            .data{std::move(frag_spv)},
        },
    };

    renderer->get_allocator().create_named_pipeline(sources, "triangle");

    tempest::graphics::render_target resources[] = {
        {
            .name{tempest::graphics::irenderer_graph::BACK_BUFFER},
            .output_name{"color_buffer_final"},
            .type{tempest::graphics::render_target_type::COLOR_WRITE},
        },
    };

    tempest::graphics::render_pass triangle_pass = {
        .resources{resources},
        .execute{[](tempest::graphics::icommand_buffer& cmds) {
            cmds.use_full_viewport()
                .use_full_scissor()
                .use_default_raster_state()
                .use_default_color_blend(tempest::graphics::irenderer_graph::BACK_BUFFER)
                .use_graphics_pipeline("triangle")
                .draw(3, 1, 0, 0);
        }},
    };

    while (!win->should_close())
    {
        tempest::input::poll();

        std::unique_ptr<tempest::graphics::irenderer_graph> graph = renderer->create_render_graph();
        (*graph)
            .add_pass({
                .resources{resources},
                .execute{[](tempest::graphics::icommand_buffer& cmds) {
                    cmds.use_full_viewport()
                        .use_full_scissor()
                        .use_default_raster_state()
                        .use_default_color_blend(tempest::graphics::irenderer_graph::BACK_BUFFER)
                        .use_graphics_pipeline("triangle")
                        .draw(3, 1, 0, 0);
                }},
            })
            .set_final_target(resources[0]);

        renderer->execute(*graph);
    }

    logger->info("Exiting Sandbox Application.");

    return 0;
}