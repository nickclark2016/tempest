#include <tempest/render_graph.hpp>

#include "vk/vk_render_device.hpp"
#include "vk/vk_render_graph.hpp"

namespace tempest::graphics
{
    namespace
    {
        constexpr pipeline_stage infer_stage(image_resource_usage usage)
        {
            switch (usage)
            {
            case image_resource_usage::COLOR_ATTACHMENT: {
                return pipeline_stage::COLOR_OUTPUT;
            }
            case image_resource_usage::DEPTH_ATTACHMENT: {
                return pipeline_stage::COLOR_OUTPUT;
            }
            case image_resource_usage::SAMPLED: {
                return pipeline_stage::FRAGMENT;
            }
            case image_resource_usage::STORAGE:
                [[fallthrough]];
            case image_resource_usage::RW_STORAGE: {
                return pipeline_stage::COMPUTE;
            }
            case image_resource_usage::TRANSFER_SOURCE: {
                return pipeline_stage::TRANSFER;
            }
            case image_resource_usage::TRANSFER_DESTINATION: {
                return pipeline_stage::TRANSFER;
            }
            }

            return pipeline_stage::BEGIN;
        }
    } // namespace

    graph_pass_builder::graph_pass_builder(render_graph_resource_library& lib, std::string_view name)
        : _resource_lib{lib}, _name{name}
    {
    }

    graph_pass_builder& graph_pass_builder::add_color_output(image_resource_handle handle, pipeline_stage first_write,
                                                             pipeline_stage last_write)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::COLOR_ATTACHMENT);
        _image_states.push_back(image_resource_state{
            .img{handle},
            .usage{image_resource_usage::COLOR_ATTACHMENT},
            .first_access{first_write},
            .last_access{last_write},
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_depth_output(image_resource_handle handle, pipeline_stage first_write,
                                                             pipeline_stage last_write)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::DEPTH_ATTACHMENT);
        _image_states.push_back(image_resource_state{
            .img{handle},
            .usage{image_resource_usage::DEPTH_ATTACHMENT},
            .first_access{first_write},
            .last_access{last_write},
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_sampled_image(image_resource_handle handle, pipeline_stage first_read,
                                                              pipeline_stage last_read)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::SAMPLED);
        _image_states.push_back(image_resource_state{
            .img{handle},
            .usage{image_resource_usage::SAMPLED},
            .first_access{first_read},
            .last_access{last_read},
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_blit_target(image_resource_handle handle, pipeline_stage first_write,
                                                            pipeline_stage last_write)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::TRANSFER_DESTINATION);
        _image_states.push_back(image_resource_state{
            .img{handle},
            .usage{image_resource_usage::TRANSFER_DESTINATION},
            .first_access{first_write},
            .last_access{last_write},
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_blit_source(image_resource_handle handle, pipeline_stage first_read,
                                                            pipeline_stage last_read)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::TRANSFER_SOURCE);
        _image_states.push_back(image_resource_state{
            .img{handle},
            .usage{image_resource_usage::TRANSFER_SOURCE},
            .first_access{first_read},
            .last_access{last_read},
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_storage_image(image_resource_handle handle, pipeline_stage first_read,
                                                              pipeline_stage last_read)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::STORAGE);
        _image_states.push_back(image_resource_state{
            .img{handle},
            .usage{image_resource_usage::STORAGE},
            .first_access{first_read},
            .last_access{last_read},
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_writable_storage_image(image_resource_handle handle,
                                                                       pipeline_stage first_read,
                                                                       pipeline_stage last_read)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::RW_STORAGE);
        _image_states.push_back(image_resource_state{
            .img{handle},
            .usage{image_resource_usage::RW_STORAGE},
            .first_access{first_read},
            .last_access{last_read},
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_structured_buffer(buffer_resource_handle handle,
                                                                  pipeline_stage first_read, pipeline_stage last_read)
    {
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_rw_structured_buffer(buffer_resource_handle handle,
                                                                     pipeline_stage first_access,
                                                                     pipeline_stage last_access)
    {
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_vertex_buffer(buffer_resource_handle handle, pipeline_stage first_read,
                                                              pipeline_stage last_read)
    {
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_index_buffer(buffer_resource_handle handle, pipeline_stage first_read,
                                                             pipeline_stage last_read)
    {
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_constant_buffer(buffer_resource_handle handle,
                                                                pipeline_stage first_read, pipeline_stage last_read)
    {
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_indirect_argument_buffer(buffer_resource_handle handle,
                                                                         pipeline_stage first_read,
                                                                         pipeline_stage last_read)
    {
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_transfer_source_buffer(buffer_resource_handle handle,
                                                                       pipeline_stage first_read,
                                                                       pipeline_stage last_read)
    {
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_transfer_destination_buffer(buffer_resource_handle handle,
                                                                            pipeline_stage first_write,
                                                                            pipeline_stage last_write)
    {
        return *this;
    }

    graph_pass_builder& graph_pass_builder::on_execute(std::function<void(command_list&)> commands)
    {
        _commands = commands;
        return *this;
    }

    void graph_pass_builder::_infer()
    {
        for (auto& state : _image_states)
        {
            if (state.first_access == pipeline_stage::INFER)
            {
                state.first_access = infer_stage(state.usage);
            }

            if (state.last_access == pipeline_stage::INFER)
            {
                state.last_access = infer_stage(state.usage);
            }
        }
    }

    render_graph_compiler::render_graph_compiler(core::allocator* alloc, render_device* device)
        : _device{device}, _alloc{alloc}, _resource_lib{std::make_unique<vk::render_graph_resource_library>(
                                              _alloc, static_cast<vk::render_device*>(device))}
    {
    }

    image_resource_handle render_graph_compiler::create_image(image_desc desc)
    {
        return _resource_lib->load(desc);
    }

    buffer_resource_handle render_graph_compiler::create_buffer(buffer_desc desc)
    {
        return _resource_lib->load(desc);
    }

    graph_pass_handle render_graph_compiler::add_graph_pass(std::string_view name,
                                                            std::function<void(graph_pass_builder&)> build)
    {
        graph_pass_builder bldr(*_resource_lib, name);

        build(bldr);

        _builders.push_back(std::move(bldr));

        graph_pass_handle handle;
        handle.id = static_cast<std::uint32_t>(_builders.size() - 1);
        handle.generation = 0;
        return handle;
    }

    std::unique_ptr<render_graph_compiler> render_graph_compiler::create_compiler(core::allocator* alloc,
                                                                                  render_device* device)
    {
        return std::make_unique<vk::render_graph_compiler>(alloc, device);
    }

    void render_graph::execute()
    {
    }
} // namespace tempest::graphics