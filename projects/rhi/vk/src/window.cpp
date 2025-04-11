#include <tempest/vk/rhi.hpp>

#include "window.hpp"

#include <tempest/logger.hpp>

#include <glfw/glfw3.h>

namespace tempest::rhi::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::vk::window_surface"}});
    }

    window_surface::window_surface(GLFWwindow* win, string name) noexcept : _window(win), _name(tempest::move(name))
    {
    }

    window_surface::~window_surface()
    {
        glfwDestroyWindow(_window);
    }

    unique_ptr<rhi::window_surface> create_window_surface(const rhi::window_surface_desc& desc) noexcept
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        auto window = glfwCreateWindow(desc.width, desc.height, desc.name.c_str(),
                                       desc.fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
        if (window == nullptr)
        {
            logger->error("Failed to create GLFW window: {}", desc.name.c_str());
            return nullptr;
        }

        return make_unique<window_surface>(window, desc.name);
    }
} // namespace tempest::rhi::vk