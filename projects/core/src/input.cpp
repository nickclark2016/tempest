#include <tempest/input.hpp>

#include <GLFW/glfw3.h>

namespace tempest::core
{
    void input::poll()
    {
        glfwPollEvents();
    }
}