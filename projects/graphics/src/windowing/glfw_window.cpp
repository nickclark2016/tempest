#include "glfw_window.hpp"

#include <tempest/window.hpp>

#include <cassert>
#include <utility>

namespace tempest::graphics::glfw
{
    namespace
    {
        bool initialize_glfw()
        {
            static bool init = glfwInit();
            return init;
        }
    } // namespace

    window::window(const window_factory::create_info& info) : _width{info.width}, _height{info.height}
    {
        auto init = initialize_glfw();
        assert(init && "Failed to initialize GLFW.");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        _win = glfwCreateWindow(static_cast<int>(info.width), static_cast<int>(info.height), info.title.data(), nullptr,
                                nullptr);

        assert(_win && "Failed to create GLFW Window.");
        glfwSetWindowUserPointer(_win, this);

        glfwSetWindowSizeCallback(_win, [](GLFWwindow* win, int width, int height) {
            window* w = reinterpret_cast<window*>(glfwGetWindowUserPointer(win));
            w->_width = width;
            w->_height = height;
        });
    }

    window::window(window&& other) noexcept
        : _win{std::move(other._win)}, _width{std::move(other._width)}, _height{std::move(other._height)}
    {
        other._win = nullptr;
        glfwSetWindowUserPointer(_win, this);
    }

    window::~window()
    {
        _release();
    }

    window& window::operator=(window&& rhs) noexcept
    {
        if (&rhs == this)
        {
            return *this;
        }

        _release();

        std::swap(_win, rhs._win);
        std::swap(_width, rhs._width);
        std::swap(_height, rhs._height);
        glfwSetWindowUserPointer(_win, this);

        return *this;
    }

    void window::_release()
    {
        if (_win)
        {
            glfwDestroyWindow(_win);
            _win = nullptr;
        }
    }
} // namespace tempest::graphics::glfw