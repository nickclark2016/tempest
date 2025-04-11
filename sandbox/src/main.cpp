#include <tempest/logger.hpp>
#include <tempest/input.hpp>
#include <tempest/rhi.hpp>

namespace rhi = tempest::rhi;

static auto logger = tempest::logger::logger_factory::create({.prefix{"sandbox"}});

int main() {
    auto instance = rhi::vk::create_instance();
    auto window = rhi::vk::create_window_surface({
        .width = 1920,
        .height = 1080,
        .name = "Sandbox",
        .fullscreen = false,
    });
    
    auto& device = instance->acquire_device(0);
    auto& default_work_queue = device.get_primary_work_queue();

    auto render_surface = device.create_render_surface({
        .window = window.get(),
        .min_image_count = 2,
        .format = {
            .space = rhi::color_space::SRGB_NONLINEAR,
            .format = rhi::image_format::BGRA8_SRGB,
        },
        .present_mode = rhi::present_mode::IMMEDIATE,
        .width = 1920,
        .height = 1080,
        .layers = 1,
    });

    while (!window->should_close())
    {
        tempest::core::input::poll();

        auto cmds = default_work_queue.get_command_list();
    }

    return instance ? 0 : 1;
}