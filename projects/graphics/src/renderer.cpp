#include <tempest/renderer.hpp>

#include "vk_renderer.hpp"

namespace tempest::graphics
{
    std::unique_ptr<irenderer> irenderer::create(iwindow& win)
    {
        return std::make_unique<vk::renderer>(win);
    }
} // namespace tempest::graphics