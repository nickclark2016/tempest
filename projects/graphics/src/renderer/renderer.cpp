#include <tempest/renderer.hpp>

#include "renderer_impl.hpp"

namespace tempest::graphics
{
    irenderer::~irenderer()
    {
        _impl->clean_up();
    }

    void irenderer::render()
    {
        _impl->render();
    }

    std::unique_ptr<irenderer> irenderer::create(const core::version& ver, iwindow& win, core::allocator& allocator)
    {
        auto renderer = std::unique_ptr<irenderer>(new irenderer());
        renderer->_impl = std::unique_ptr<impl>(new impl);

        gfx_device_create_info create_info = {
            .global_allocator{&allocator},
            .win{reinterpret_cast<glfw::window*>(&win)},
#ifdef _DEBUG
            .enable_debug{true},
#else
            .enable_debug{false}
#endif
        };

        renderer->_impl->device.reset(new gfx_device(create_info));
        renderer->_impl->set_up();

        return renderer;
    }
} // namespace tempest::graphics