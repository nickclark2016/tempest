#include "vk_render_graph.hpp"

#include <tempest/logger.hpp>

#include <algorithm>

namespace tempest::graphics::vk
{
    namespace
    {
        auto logger = logger::logger_factory::create({.prefix{"tempest::graphics::vk::render_graph"}});
    } // namespace

    render_graph_resource_library::render_graph_resource_library(core::allocator* alloc, render_device* device)
        : _device{device}
    {
    }

    render_graph_resource_library::~render_graph_resource_library()
    {
        for (auto img : _compiled_images)
        {
            _device->release_image(img);
        }
    }

    image_resource_handle render_graph_resource_library::find_texture(std::string_view name)
    {
        return image_resource_handle();
    }

    image_resource_handle render_graph_resource_library::load(const image_desc& desc)
    {
        auto handle = _device->allocate_image();

        image_create_info ci = {
            .type{desc.type},
            .width{desc.width},
            .height{desc.height},
            .depth{desc.depth},
            .layers{desc.layers},
            .mip_count{1},
            .format{desc.fmt},
            .samples{desc.samples},
            .name{std::string(desc.name)},
        };

        _images_to_compile.push_back(deferred_image_create_info{
            .info{ci},
            .allocation{handle},
        });

        return handle;
    }

    void render_graph_resource_library::add_image_usage(image_resource_handle handle, image_resource_usage usage)
    {
        auto image_it = std::find_if(std::begin(_images_to_compile), std::end(_images_to_compile),
                                     [handle](const auto& def) { return def.allocation == handle; });
        if (image_it != std::end(_images_to_compile))
        {
            switch (usage)
            {
            case image_resource_usage::COLOR_ATTACHMENT: {
                image_it->info.color_attachment = true;
                break;
            }
            case image_resource_usage::DEPTH_ATTACHMENT: {
                image_it->info.depth_attachment = true;
                break;
            }
            case image_resource_usage::SAMPLED: {
                image_it->info.sampled = true;
                break;
            }
            case image_resource_usage::STORAGE:
                [[fallthrough]];
            case image_resource_usage::RW_STORAGE: {
                image_it->info.storage = true;
                break;
            }
            case image_resource_usage::TRANSFER_SOURCE: {
                image_it->info.transfer_source = true;
                break;
            }
            case image_resource_usage::TRANSFER_DESTINATION: {
                image_it->info.transfer_destination = true;
                break;
            }
            }
        }
    }

    buffer_resource_handle render_graph_resource_library::find_buffer(std::string_view name)
    {
        return buffer_resource_handle();
    }

    buffer_resource_handle render_graph_resource_library::load(const buffer_desc& desc)
    {
        return buffer_resource_handle();
    }

    void render_graph_resource_library::add_buffer_usage(buffer_resource_handle handle, buffer_resource_usage usage)
    {
    }

    bool render_graph_resource_library::compile()
    {
        for (auto& image_info : _images_to_compile)
        {
            auto compiled = _device->create_image(image_info.info, image_info.allocation);
            if (!compiled)
            {
                return false;
            }
            _compiled_images.push_back(compiled);
        }

        return true;
    }

    render_graph::render_graph(core::allocator* alloc, std::unique_ptr<render_graph_resource_library>&& resources)
        : _alloc{alloc}, _resource_lib{std::move(resources)}
    {
    }

    void render_graph::execute()
    {
    }

    render_graph_compiler::render_graph_compiler(core::allocator* alloc, graphics::render_device* device)
        : graphics::render_graph_compiler(alloc, device)

    {
    }

    std::unique_ptr<graphics::render_graph> render_graph_compiler::compile() &&
    {
        _resource_lib->compile();
        return std::make_unique<vk::render_graph>(
            _alloc, std::unique_ptr<vk::render_graph_resource_library>(
                        static_cast<vk::render_graph_resource_library*>(_resource_lib.release())));
    }
} // namespace tempest::graphics::vk