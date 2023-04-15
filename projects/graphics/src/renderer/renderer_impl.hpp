#ifndef tempest_graphics_renderer_impl_hpp
#define tempest_graphics_renderer_impl_hpp

#include <tempest/renderer.hpp>

#include "device.hpp"
#include "forward_pbr_pass.hpp"

#include <tempest/memory.hpp>

#include <VkBootstrap.h>

namespace tempest::graphics
{
    struct irenderer::impl
    {
        std::unique_ptr<gfx_device> device;

        texture_handle depth_target{};
        texture_handle color_target{};
        VkFormat color_target_format{VK_FORMAT_R8G8B8A8_SRGB};

        pipeline_handle blit_pipeline{};
        render_pass_handle blit_pass{};
        descriptor_set_layout_handle blit_desc_set_layout{};
        descriptor_set_handle blit_desc_set{};
        sampler_handle default_sampler{};

        render_pass_handle triangle_pass{};
        pipeline_handle triangle_pipeline{};

        std::optional<forward_pbr_pass> pbr_forward;

        void set_up();
        void render();
        void clean_up();

        void _create_triangle_pipeline();
        void _create_blit_pipeline();
    };
}

#endif // tempest_graphics_renderer_impl_hpp