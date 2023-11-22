#ifndef tempest_graphics_render_device_hpp
#define tempest_graphics_render_device_hpp

#include "types.hpp"

#include <tempest/memory.hpp>

#include <compare>
#include <memory>

namespace tempest::graphics
{
    class render_device;

    class render_context
    {
      public:
        virtual ~render_context() = default;

        virtual bool has_suitable_device() const noexcept = 0;
        virtual std::uint32_t device_count() const noexcept = 0;
        virtual render_device& get_device(std::uint32_t idx = 0) = 0;

        static std::unique_ptr<render_context> create(core::allocator* alloc);

      protected:
        core::allocator* _alloc;

        explicit render_context(core::allocator* alloc);
    };

    class render_device
    {
      public:
        virtual void start_frame() noexcept = 0;
        virtual void end_frame() noexcept = 0;

        virtual buffer_resource_handle create_buffer(const buffer_create_info& ci) = 0;
        virtual void release_buffer(buffer_resource_handle handle) = 0;

        virtual image_resource_handle create_image(const image_create_info& ci) = 0;
        virtual void release_image(image_resource_handle handle) = 0;

        virtual graphics_pipeline_resource_handle create_graphics_pipeline(const graphics_pipeline_create_info& ci) = 0;
        virtual void release_graphics_pipeline(graphics_pipeline_resource_handle handle) = 0;

        virtual swapchain_resource_handle create_swapchain(const swapchain_create_info& ci) = 0;
        virtual void release_swapchain(swapchain_resource_handle handle) = 0;
        virtual void recreate_swapchain(swapchain_resource_handle handle, std::uint32_t width,
                                        std::uint32_t height) = 0;

        virtual size_t frame_in_flight() const noexcept = 0;
        virtual size_t frames_in_flight() const noexcept = 0;

        virtual ~render_device() = default;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_render_device_hpp