#include <tempest/input.hpp>
#include <tempest/instance.hpp>
#include <tempest/logger.hpp>
#include <tempest/renderer.hpp>
#include <tempest/window.hpp>

int main()
{
    auto win = tempest::graphics::window_factory::create({
        .title{"Sandbox"},
        .width{1920},
        .height{1080},
    });

    auto renderer = tempest::graphics::irenderer::create();

    while (!win->should_close())
    {
        tempest::input::poll();

        renderer->draw();
    }

    return 0;
}