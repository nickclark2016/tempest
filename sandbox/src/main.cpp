#include <tempest/input.hpp>
#include <tempest/logger.hpp>
#include <tempest/math.hpp>
#include <tempest/memory.hpp>
#include <tempest/renderer.hpp>
#include <tempest/window.hpp>

#include <cassert>
#include <fstream>
#include <sstream>

namespace
{
    inline constexpr std::size_t global_memory_allocator_size = 1024 * 1024 * 64;
}

int main()
{
    auto logger = tempest::logger::logger_factory::create({
        .prefix{"Sandbox"},
    });
    logger->info("Starting Sandbox Application.");

    auto global_allocator = tempest::core::heap_allocator(global_memory_allocator_size);

    auto window = tempest::graphics::window_factory::create({
        .title{"Tempest Sandbox"},
        .width{1080},
        .height{720},
    });

    auto renderer = tempest::graphics::irenderer::create({.major{0}, .minor{0}, .patch{1}}, *window, global_allocator);

    while (!window->should_close())
    {
        tempest::input::poll();
        renderer->render();
    }

    logger->info("Exiting Sandbox Application.");

    return 0;
}