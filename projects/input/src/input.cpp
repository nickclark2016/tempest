#include <tempest/input.hpp>

#include <GLFW/glfw3.h>

namespace tempest::input
{
    void poll()
    {
        glfwPollEvents();
    }
}