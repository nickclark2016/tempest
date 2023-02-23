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
        assert(initialize_glfw() && "Failed to initialize GLFW.");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        _win = glfwCreateWindow(static_cast<int>(info.width), static_cast<int>(info.height), info.title.data(), nullptr,
                                nullptr);
        assert(_win && "Failed to create GLFW Window.");
    }

    window::window(window&& other) noexcept
        : _win{std::move(other._win)}, _width{std::move(other._width)}, _height{std::move(other._height)}
    {
        other._win = nullptr;
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