#ifndef tempest_graphics_renderer_impl_hpp__
#define tempest_graphics_renderer_impl_hpp__

#include <tempest/renderer.hpp>

#include "device.hpp"

#include <tempest/memory.hpp>

#include <VkBootstrap.h>

namespace tempest::graphics
{
    struct irenderer::impl
    {
        std::unique_ptr<gfx_device> device;

        pipeline_handle triangle_pipeline{};

        void set_up();
        void render();
        void clean_up();
    };
}

#endif // tempest_graphics_renderer_impl_hpp__