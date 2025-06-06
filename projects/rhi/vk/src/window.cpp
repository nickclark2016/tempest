#include <tempest/vk/rhi.hpp>

#include "window.hpp"

#include <tempest/logger.hpp>

#include <GLFW/glfw3.h>

namespace tempest::rhi::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::vk::window_surface"}});
    }

    window_surface::window_surface(GLFWwindow* win, string name, uint32_t width, uint32_t height) noexcept
        : _window(win), _name(tempest::move(name)), _width(width), _height(height)
    {
    }

    window_surface::~window_surface()
    {
        if (_window)
        {
            glfwDestroyWindow(_window);
        }
    }

    void window_surface::register_keyboard_callback(function<void(const core::key_state&)>&& cb) noexcept
    {
        _keyboard_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_mouse_callback(function<void(const core::mouse_button_state&)>&& cb) noexcept
    {
        _mouse_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_cursor_callback(function<void(float, float)>&& cb) noexcept
    {
        _cursor_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_scroll_callback(function<void(float, float)>&& cb) noexcept
    {
        _scroll_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_close_callback(function<void()>&& cb) noexcept
    {
        _close_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_resize_callback(function<void(uint32_t, uint32_t)>&& cb) noexcept
    {
        _resize_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_focus_callback(function<void(bool)>&& cb) noexcept
    {
        _focus_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::register_minimize_callback(function<void(bool)>&& cb) noexcept
    {
        _minimize_callbacks.push_back(tempest::move(cb));
    }

    void window_surface::execute_keyboard_callbacks(const core::key_state& state) const noexcept
    {
        for (const auto& cb : _keyboard_callbacks)
        {
            cb(state);
        }
    }

    void window_surface::execute_mouse_callbacks(const core::mouse_button_state& state) const noexcept
    {
        for (const auto& cb : _mouse_callbacks)
        {
            cb(state);
        }
    }

    void window_surface::execute_cursor_callbacks(float x, float y) const noexcept
    {
        for (const auto& cb : _cursor_callbacks)
        {
            cb(x, y);
        }
    }

    void window_surface::execute_scroll_callbacks(float x, float y) const noexcept
    {
        for (const auto& cb : _scroll_callbacks)
        {
            cb(x, y);
        }
    }

    void window_surface::execute_close_callbacks() const noexcept
    {
        for (const auto& cb : _close_callbacks)
        {
            cb();
        }
    }

    void window_surface::execute_resize_callbacks(uint32_t width, uint32_t height) const noexcept
    {
        for (const auto& cb : _resize_callbacks)
        {
            cb(width, height);
        }
    }

    void window_surface::execute_focus_callbacks(bool focused) const noexcept
    {
        for (const auto& cb : _focus_callbacks)
        {
            cb(focused);
        }
    }

    void window_surface::execute_minimize_callbacks(bool minimized) const noexcept
    {
        for (const auto& cb : _minimize_callbacks)
        {
            cb(minimized);
        }
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

        auto win = make_unique<window_surface>(window, desc.name, desc.width, desc.height);

        glfwSetWindowUserPointer(window, win.get());

        // Keyboard
        glfwSetKeyCallback(window,
                           [](GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action, int mods) {
                               auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
                               auto key_state = core::key_state{
                                   .k = static_cast<core::key>(key),
                                   .action = static_cast<core::key_action>(action),
                                   .modifiers = static_cast<core::key_modifier>(mods),
                               };

                               win->execute_keyboard_callbacks(key_state);
                           });

        // Mouse
        glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, [[maybe_unused]] int mods) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            auto mouse_state = core::mouse_button_state{
                .button = static_cast<core::mouse_button>(button),
                .action = static_cast<core::mouse_action>(action),
            };

            win->execute_mouse_callbacks(mouse_state);
        });

        // Cursor
        glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_cursor_callbacks(static_cast<float>(xpos), static_cast<float>(ypos));
        });

        // Scroll
        glfwSetScrollCallback(window, [](GLFWwindow* window, double xoffset, double yoffset) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_scroll_callbacks(static_cast<float>(xoffset), static_cast<float>(yoffset));
        });

        // Close
        glfwSetWindowCloseCallback(window, [](GLFWwindow* window) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_close_callbacks();
            glfwDestroyWindow(window);
            win->release_handle();
        });

        // Resize
        glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_resize_callbacks(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
            win->set_size(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        });

        // Focus
        glfwSetWindowFocusCallback(window, [](GLFWwindow* window, int focused) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_focus_callbacks(focused == GLFW_TRUE);
        });

        // Minimize
        glfwSetWindowIconifyCallback(window, [](GLFWwindow* window, int minimized) {
            auto* win = static_cast<window_surface*>(glfwGetWindowUserPointer(window));
            win->execute_minimize_callbacks(minimized == GLFW_TRUE);
            win->set_minimized(minimized == GLFW_TRUE);
        });

        return win;
    }
} // namespace tempest::rhi::vk