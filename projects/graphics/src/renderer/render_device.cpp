#include <tempest/render_device.hpp>

#include "vk/vk_render_device.hpp"

namespace tempest::graphics
{
    std::unique_ptr<render_context> render_context::create(core::allocator* alloc)
    {
        return std::make_unique<vk::render_context>(alloc);
    }

    render_context::render_context(core::allocator* alloc) : _alloc{alloc}
    {
    }
} // namespace tempest::graphics