#include <tempest/input.hpp>
#include <tempest/instance.hpp>
#include <tempest/logger.hpp>
#include <tempest/renderer.hpp>
#include <tempest/window.hpp>

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

    tempest::graphics::render_target resources[] = {
        {
            .name{"tempest_render_graph_target"},
            .output_name{"tempest_render_graph_target_final"},
            .type{tempest::graphics::render_target_type::COLOR_WRITE},
        },
    };

    while (!win->should_close())
    {
        tempest::input::poll();

        auto graph = renderer->create_render_graph();
        graph->add_pass({.resources{resources}}).set_final_target(resources[0]);

        renderer->execute(*graph);
    }

    logger->info("Exiting Sandbox Application.");

    return 0;
}