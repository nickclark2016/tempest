#include <tempest/renderer.hpp>

#include "vk_renderer.hpp"

namespace tempest::graphics
{
    std::unique_ptr<irenderer> irenderer::create(const iinstance& inst, const idevice& dev, const iwindow& win)
    {
        return std::make_unique<vk::renderer>(inst, dev, win);
    }
} // namespace tempest::graphics