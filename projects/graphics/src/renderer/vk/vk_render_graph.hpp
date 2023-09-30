#ifndef tempest_graphics_vk_render_graph_hpp
#define tempest_graphics_vk_render_graph_hpp

#include "vk_render_device.hpp"

#include <tempest/memory.hpp>
#include <tempest/object_pool.hpp>
#include <tempest/render_graph.hpp>

#include <vulkan/vulkan.h>

namespace tempest::graphics::vk
{
    class render_graph_resource_library;

    class render_graph : public graphics::render_graph
    {
      public:
        explicit render_graph(core::allocator* alloc, std::unique_ptr<render_graph_resource_library>&& resources);
        void execute() override;

      private:
        std::unique_ptr<render_graph_resource_library> _resource_lib;

        core::allocator* _alloc;
    };

    class render_graph_resource_library : public graphics::render_graph_resource_library
    {
      public:
        explicit render_graph_resource_library(core::allocator* alloc, render_device* device);
        ~render_graph_resource_library();

        image_resource_handle find_texture(std::string_view name) override;
        image_resource_handle load(const image_desc& desc) override;
        void add_image_usage(image_resource_handle handle, image_resource_usage usage) override;

        buffer_resource_handle find_buffer(std::string_view name) override;
        buffer_resource_handle load(const buffer_desc& desc) override;
        void add_buffer_usage(buffer_resource_handle handle, buffer_resource_usage usage) override;

        bool compile() override;

      private:
        render_device* _device;

        struct deferred_image_create_info
        {
            image_create_info info;
            image_resource_handle allocation;
        };

        struct deferred_buffer_create_info
        {
            buffer_create_info info;
            buffer_resource_handle allocation;
        };

        std::vector<deferred_image_create_info> _images_to_compile;
        std::vector<image_resource_handle> _compiled_images;
    };

    class render_graph_compiler : public graphics::render_graph_compiler
    {
      public:
        explicit render_graph_compiler(core::allocator* alloc, graphics::render_device* device);
        std::unique_ptr<graphics::render_graph> compile() && override;
    };
} // namespace tempest::graphics::vk

#endif // tempest_graphics_vk_render_graph_hpp