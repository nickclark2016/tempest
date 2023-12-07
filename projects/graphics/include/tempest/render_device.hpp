#ifndef tempest_graphics_render_device_hpp
#define tempest_graphics_render_device_hpp

#include "types.hpp"

#include <tempest/memory.hpp>

#include <compare>
#include <cstdint>
#include <memory>
#include <string>

namespace tempest::graphics
{
    class render_device;

    struct physical_device_context
    {
        std::uint32_t id;
        std::string name;
    };

    class render_context
    {
      public:
        virtual ~render_context() = default;

        virtual bool has_suitable_device() const noexcept = 0;
        virtual std::uint32_t device_count() const noexcept = 0;
        virtual render_device& create_device(std::uint32_t idx = 0) = 0;
        virtual std::vector<physical_device_context> enumerate_suitable_devices() = 0;

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
        virtual std::span<std::byte> map_buffer(buffer_resource_handle handle) = 0;
        virtual std::span<std::byte> map_buffer_frame(buffer_resource_handle handle, std::uint64_t frame_offset = 0) = 0;
        virtual std::size_t get_buffer_frame_offset(buffer_resource_handle handle, std::uint64_t frame_offset = 0) = 0;
        virtual void unmap_buffer(buffer_resource_handle handle) = 0;

        virtual image_resource_handle create_image(const image_create_info& ci) = 0;
        virtual void release_image(image_resource_handle handle) = 0;

        virtual sampler_resource_handle create_sampler(const sampler_create_info& ci) = 0;
        virtual void release_sampler(sampler_resource_handle handle) = 0;

        virtual graphics_pipeline_resource_handle create_graphics_pipeline(const graphics_pipeline_create_info& ci) = 0;
        virtual void release_graphics_pipeline(graphics_pipeline_resource_handle handle) = 0;

        virtual compute_pipeline_resource_handle create_compute_pipeline(const compute_pipeline_create_info& ci) = 0;
        virtual void release_compute_pipeline(compute_pipeline_resource_handle handle) = 0;

        virtual swapchain_resource_handle create_swapchain(const swapchain_create_info& ci) = 0;
        virtual void release_swapchain(swapchain_resource_handle handle) = 0;
        virtual void recreate_swapchain(swapchain_resource_handle handle) = 0;
        virtual image_resource_handle fetch_current_image(swapchain_resource_handle handle) = 0;

        virtual size_t frame_in_flight() const noexcept = 0;
        virtual size_t frames_in_flight() const noexcept = 0;

        virtual buffer_resource_handle get_staging_buffer() = 0;
        virtual command_execution_service& get_command_executor() = 0;

        virtual ~render_device() = default;
    };

    class renderer_utilities
    {
      public:
        static std::vector<image_resource_handle> upload_textures(render_device& dev,
                                                                  std::span<texture_data_descriptor> textures,
                                                                  buffer_resource_handle staging_buffer,
                                                                  bool use_entire_buffer = false);
    };
} // namespace tempest::graphics

#endif // tempest_graphics_render_device_hpp