#include <tempest/render_system.hpp>

#include "device.hpp"
#include "passes/blit_pass.hpp"

#include <optional>

namespace tempest::graphics
{
    class render_system::render_system_impl
    {
      public:
        render_system_impl(const core::version& ver, iwindow& win, core::allocator& allocator)
            : _ver{ver}, _win{win}, _alloc{allocator}
        {
            gfx_device_create_info create_info = {
                .global_allocator{&allocator},
                .win{reinterpret_cast<glfw::window*>(&win)},
#ifdef _DEBUG
                .enable_debug{true},
#else
                .enable_debug{false}
#endif
            };

            _device.emplace(create_info);

            _blit.initialize(*_device, 1280, 720, VK_FORMAT_R8G8B8A8_SRGB);
        }

        ~render_system_impl()
        {
            _blit.release(*_device);
        }

        void render()
        {
        }

      private:
        core::version _ver;
        iwindow& _win;
        core::allocator& _alloc;

        std::optional<gfx_device> _device;

        blit_pass _blit;
    };

    render_system::render_system(const core::version& ver, iwindow& win, core::allocator& allocator)
        : _impl{std::make_unique<render_system_impl>(ver, win, allocator)}
    {
    }

    render_system::~render_system() = default;
    
    void render_system::render()
    {
    }
} // namespace tempest::graphics