#include <tempest/renderer.hpp>

#include "vk_renderer.hpp"

namespace tempest::graphics
{
    std::unique_ptr<irenderer> irenderer::create()
    {
        return std::make_unique<vk::renderer>();
    }
} // namespace tempest::graphics