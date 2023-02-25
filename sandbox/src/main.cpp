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

    auto gfx_instance = tempest::graphics::instance_factory::create({
        .name{"Sandbox"},
        .version_major{0},
        .version_minor{0},
        .version_patch{1},
    });

    auto& gfx_device = gfx_instance->get_devices()[0];

    auto renderer = tempest::graphics::irenderer::create(*gfx_instance, *gfx_device, *win);

    while (!win->should_close())
    {
        tempest::input::poll();

        renderer->draw([](tempest::graphics::icommands& cmds) {});
    }

    return 0;
}