#ifndef tempest_graphics_render_device_hpp
#define tempest_graphics_render_device_hpp

#include "graphics_components.hpp"
#include "types.hpp"

#include <tempest/memory.hpp>
#include <tempest/span.hpp>
#include <tempest/texture.hpp>
#include <tempest/vector.hpp>
#include <tempest/vertex.hpp>

#include <compare>
#include <cstdint>
#include <string>

namespace tempest::graphics
{
    class render_device;

    struct physical_device_context
    {
        uint32_t id;
        string name;
    };

    class render_context
    {
      public:
        virtual ~render_context() = default;

        virtual bool has_suitable_device() const noexcept = 0;
        virtual uint32_t device_count() const noexcept = 0;
        virtual render_device& create_device(uint32_t idx = 0) = 0;
        virtual vector<physical_device_context> enumerate_suitable_devices() = 0;

        static unique_ptr<render_context> create(abstract_allocator* alloc);

      protected:
        abstract_allocator* _alloc;

        explicit render_context(abstract_allocator* alloc);
    };

    class render_device
    {
      public:
        virtual void start_frame() noexcept = 0;
        virtual void end_frame() noexcept = 0;

        virtual buffer_resource_handle create_buffer(const buffer_create_info& ci) = 0;
        virtual void release_buffer(buffer_resource_handle handle) = 0;
        virtual span<byte> map_buffer(buffer_resource_handle handle) = 0;
        virtual span<byte> map_buffer_frame(buffer_resource_handle handle, uint64_t frame_offset = 0) = 0;
        virtual size_t get_buffer_frame_offset(buffer_resource_handle handle, uint64_t frame_offset = 0) = 0;
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
        virtual size_t current_frame() const noexcept = 0;

        virtual buffer_resource_handle get_staging_buffer() = 0;
        virtual command_execution_service& get_command_executor() = 0;

        virtual void idle() = 0;

        virtual ~render_device() = default;
    };

    class renderer_utilities
    {
      public:
        static vector<image_resource_handle> upload_textures(render_device& dev, span<texture_data_descriptor> textures,
                                                             buffer_resource_handle staging_buffer,
                                                             bool use_entire_buffer = false,
                                                             bool generate_mip_maps = false);

        static vector<image_resource_handle> upload_textures(render_device& dev, span<const guid> texture_ids,
                                                             const core::texture_registry& texture_registry,
                                                             buffer_resource_handle staging_buffer,
                                                             bool use_entire_buffer = false,
                                                             bool generate_mip_maps = false);

        static vector<mesh_layout> upload_meshes(render_device& device, span<core::mesh> meshes,
                                                 buffer_resource_handle target, uint32_t& offset);

        static flat_unordered_map<guid, mesh_layout> upload_meshes(render_device& device, span<const guid> mesh_ids,
                                                                   core::mesh_registry& mesh_registry,
                                                                   buffer_resource_handle target, uint32_t& offset,
                                                                   buffer_resource_handle staging_buffer);
    };

    class staging_buffer_writer
    {
      public:
        explicit staging_buffer_writer(render_device& dev);
        staging_buffer_writer(render_device& dev, buffer_resource_handle staging_buffer,
                              uint32_t staging_buffer_offset);

        staging_buffer_writer& write(command_list& cmds, span<const byte> data, buffer_resource_handle target,
                                     uint32_t write_offset = 0);

        template <typename T>
        staging_buffer_writer& write(command_list& cmds, span<const T> data, buffer_resource_handle target,
                                     uint32_t write_offset = 0)
        {
            return write(cmds, as_bytes(data), target, write_offset);
        }

        void finish();
        void reset(uint32_t staging_buffer_offset = 0);
        void mark(size_t offset) noexcept;

      private:
        render_device* _dev;
        size_t _staging_buffer_offset;
        size_t _bytes_written{0};
        buffer_resource_handle _staging_buffer;
        span<byte> _mapped_buffer{};
    };
} // namespace tempest::graphics

#endif // tempest_graphics_render_device_hpp