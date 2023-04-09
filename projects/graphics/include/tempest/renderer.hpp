#ifndef tempest_graphics_renderer_hpp
#define tempest_graphics_renderer_hpp

#include <tempest/memory.hpp>
#include <tempest/version.hpp>

#include "window.hpp"

#include <memory>

namespace tempest::graphics
{
    class irenderer
    {
        struct impl;

      public:
        ~irenderer();
        void render();

        static std::unique_ptr<irenderer> create(const core::version& ver, iwindow& win, core::allocator& allocator);

      private:
        std::unique_ptr<impl> _impl;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_renderer_hpp
