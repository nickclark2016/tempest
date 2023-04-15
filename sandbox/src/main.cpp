#include <tempest/input.hpp>
#include <tempest/logger.hpp>
#include <tempest/math.hpp>
#include <tempest/memory.hpp>
#include <tempest/renderer.hpp>
#include <tempest/window.hpp>

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
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
        .width{1280},
        .height{720},
    });

    auto renderer = tempest::graphics::irenderer::create({.major{0}, .minor{0}, .patch{1}}, *window, global_allocator);

    tempest::math::mat<float, 4, 4> testmat(1.0f);

    auto last_time = std::chrono::high_resolution_clock::now();
    std::uint32_t fps_counter = 0;

    while (!window->should_close())
    {
        tempest::input::poll();
        renderer->render();
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

    logger->info("Exiting Sandbox Application.");

    return 0;
}