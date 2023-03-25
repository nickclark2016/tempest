#include <tempest/window.hpp>

#include "glfw_window.hpp"

namespace tempest::graphics
{
    std::unique_ptr<iwindow> window_factory::create(const create_info& info)
    {
        return std::make_unique<glfw::window>(info);
    }
}
