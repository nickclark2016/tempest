#ifndef tempest_graphics_renderer_impl_hpp
#define tempest_graphics_renderer_impl_hpp

#include <tempest/renderer.hpp>

#include "device.hpp"
#include "forward_pbr_pass.hpp"

#include <tempest/memory.hpp>

#include <VkBootstrap.h>

namespace tempest::graphics
{
    struct buffer_suballocator
    {
        gfx_device* device{nullptr};
        buffer_handle current_buf{.index{invalid_resource_handle}};
        buffer_handle previous_buf{.index{invalid_resource_handle}};
        buffer_create_info ci{};

        explicit buffer_suballocator(const buffer_create_info& initial, gfx_device* dev);

        void release();
        void reallocate_and_wait(std::size_t new_capacity);

        core::best_fit_scheme<std::size_t> scheme;
    };

    struct irenderer::impl
    {
        std::unique_ptr<gfx_device> device;
        std::optional<buffer_suballocator> vertex_buffer_allocator;

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
        descriptor_set_layout_handle mesh_data_layout{};
        descriptor_set_handle mesh_data_set;

        std::optional<forward_pbr_pass> pbr_forward{std::nullopt};

        void set_up();
        void render();
        void clean_up();

        void _create_triangle_pipeline();
        void _create_blit_pipeline();
        void _create_mesh_buffers();
    };
} // namespace tempest::graphics

#endif // tempest_graphics_renderer_impl_hpp