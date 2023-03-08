#include <tempest/renderer.hpp>

#include "vk_renderer.hpp"

namespace tempest::graphics
{
    std::unique_ptr<irenderer> irenderer::create(const core::version& version_info, iwindow& win)
    {
        return std::unique_ptr<irenderer>(new irenderer(version_info, win));
    }

    irenderer::irenderer(const core::version& version_info, iwindow& win) : _impl{new impl()}
    {
    }

    irenderer::~irenderer() = default;
    
    void irenderer::render()
    {
    }
} // namespace tempest::graphics