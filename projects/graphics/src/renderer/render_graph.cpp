#include <tempest/render_graph.hpp>

#include "vk/vk_render_device.hpp"
#include "vk/vk_render_graph.hpp"

#include <algorithm>
#include <ranges>
#include <unordered_set>

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
            case image_resource_usage::STORAGE: {
                return pipeline_stage::COMPUTE;
            }
            case image_resource_usage::TRANSFER_SOURCE:
                [[fallthrough]];
            case image_resource_usage::TRANSFER_DESTINATION: {
                return pipeline_stage::TRANSFER;
            }
            default:
                break;
            }

            return pipeline_stage::BEGIN;
        }

        constexpr pipeline_stage infer_stage(buffer_resource_usage usage)
        {
            switch (usage)
            {
            case buffer_resource_usage::STRUCTURED:
            case buffer_resource_usage::CONSTANT:
            case buffer_resource_usage::VERTEX:
            case buffer_resource_usage::INDEX: {
                return pipeline_stage::VERTEX;
            }
            case buffer_resource_usage::INDIRECT_ARGUMENT: {
                return pipeline_stage::DRAW_INDIRECT;
            }
            case buffer_resource_usage::TRANSFER_SOURCE:
                [[fallthrough]];
            case buffer_resource_usage::TRANSFER_DESTINATION: {
                return pipeline_stage::TRANSFER;
            }
            default:
                break;
            }

            return pipeline_stage::BEGIN;
        }
    } // namespace

    graph_pass_builder::graph_pass_builder(render_graph_resource_library& lib, std::string_view name,
                                           queue_operation_type type)
        : _resource_lib{lib}, _name{name}, _op_type{type}
    {
    }

    graph_pass_builder& graph_pass_builder::add_color_attachment(image_resource_handle handle,
                                                                 resource_access_type access, load_op load,
                                                                 store_op store, math::vec4<float> clear_color,
                                                                 pipeline_stage first_access,
                                                                 pipeline_stage last_access)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::COLOR_ATTACHMENT);
        _image_states.push_back(image_resource_state{
            .type = access,
            .handles = {{handle}},
            .usage = image_resource_usage::COLOR_ATTACHMENT,
            .first_access = first_access,
            .last_access = last_access,
            .load = load,
            .store = store,
            .clear_color = clear_color,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_external_color_attachment(swapchain_resource_handle swap,
                                                                          resource_access_type access, load_op load,
                                                                          store_op store, pipeline_stage first_write,
                                                                          pipeline_stage last_write)
    {
        _external_swapchain_states.push_back(swapchain_resource_state{
            .type = access,
            .swap = swap,
            .usage = image_resource_usage::COLOR_ATTACHMENT,
            .first_access = first_write,
            .last_access = last_write,
            .load = load,
            .store = store,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_depth_attachment(image_resource_handle handle,
                                                                 resource_access_type access, load_op load,
                                                                 store_op store, float clear_depth,
                                                                 pipeline_stage first_access,
                                                                 pipeline_stage last_access)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::DEPTH_ATTACHMENT);
        _image_states.push_back(image_resource_state{
            .type = access,
            .handles = {{handle}},
            .usage = image_resource_usage::DEPTH_ATTACHMENT,
            .first_access = first_access,
            .last_access = last_access,
            .load = load,
            .store = store,
            .clear_depth = clear_depth,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_sampled_image(image_resource_handle handle, std::uint32_t set,
                                                              std::uint32_t binding, pipeline_stage first_read,
                                                              pipeline_stage last_read)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::SAMPLED);
        _image_states.push_back(image_resource_state{
            .type = resource_access_type::READ,
            .handles = {{handle}},
            .usage = image_resource_usage::SAMPLED,
            .first_access = first_read,
            .last_access = last_read,
            .set = set,
            .binding = binding,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_external_sampled_image(image_resource_handle handle, std::uint32_t set,
                                                                       std::uint32_t binding, pipeline_stage usage)
    {
        _external_image_states.push_back(external_image_resource_state{
            .type = resource_access_type::READ,
            .usage = image_resource_usage::SAMPLED,
            .images = {{handle}},
            .stages = usage,
            .count = 1,
            .set = set,
            .binding = binding,
        });

        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_external_sampled_images(span<image_resource_handle> handles,
                                                                        std::uint32_t set, std::uint32_t binding,
                                                                        pipeline_stage usage)
    {
        _external_image_states.clear();

        _external_image_states.push_back(external_image_resource_state{
            .type = resource_access_type::READ,
            .usage = image_resource_usage::SAMPLED,
            .images = {handles.begin(), handles.end()},
            .stages = usage,
            .count = static_cast<std::uint32_t>(handles.size()),
            .set = set,
            .binding = binding,
        });

        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_external_sampled_images(std::uint32_t count, std::uint32_t set,
                                                                        std::uint32_t binding, pipeline_stage usage)
    {
        _external_image_states.clear();

        _external_image_states.push_back(external_image_resource_state{
            .type = resource_access_type::READ,
            .usage = image_resource_usage::SAMPLED,
            .stages = usage,
            .count = count,
            .set = set,
            .binding = binding,
        });

        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_external_storage_image(image_resource_handle handle,
                                                                       resource_access_type access, std::uint32_t set,
                                                                       std::uint32_t binding, pipeline_stage usage)
    {
        _external_image_states.push_back(external_image_resource_state{
            .type = resource_access_type::READ,
            .usage = image_resource_usage::STORAGE,
            .images = {{handle}},
            .stages = usage,
            .count = 1,
            .set = set,
            .binding = binding,
        });

        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_blit_target(image_resource_handle handle, pipeline_stage first_write,
                                                            pipeline_stage last_write)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::TRANSFER_DESTINATION);
        _image_states.push_back(image_resource_state{
            .type = resource_access_type::WRITE,
            .handles = {{handle}},
            .usage = image_resource_usage::TRANSFER_DESTINATION,
            .first_access = first_write,
            .last_access = last_write,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_external_blit_target(swapchain_resource_handle swap,
                                                                     pipeline_stage first_write,
                                                                     pipeline_stage last_write)
    {
        _external_swapchain_states.push_back(swapchain_resource_state{
            .type = resource_access_type::WRITE,
            .swap = swap,
            .usage = image_resource_usage::TRANSFER_DESTINATION,
            .first_access = first_write,
            .last_access = last_write,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_blit_source(image_resource_handle handle, pipeline_stage first_read,
                                                            pipeline_stage last_read)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::TRANSFER_SOURCE);
        _image_states.push_back(image_resource_state{
            .type = resource_access_type::READ,
            .handles = {{handle}},
            .usage = image_resource_usage::TRANSFER_SOURCE,
            .first_access = first_read,
            .last_access = last_read,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_storage_image(image_resource_handle handle, resource_access_type access,
                                                              std::uint32_t set, std::uint32_t binding,
                                                              pipeline_stage first_access, pipeline_stage last_access)
    {
        _resource_lib.add_image_usage(handle, image_resource_usage::STORAGE);
        _image_states.push_back(image_resource_state{
            .type = access,
            .handles = {{handle}},
            .usage = image_resource_usage::STORAGE,
            .first_access = first_access,
            .last_access = last_access,
            .set = set,
            .binding = binding,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_storage_image(span<image_resource_handle> handle,
                                                              resource_access_type access, std::uint32_t set,
                                                              std::uint32_t binding, pipeline_stage first_access,
                                                              pipeline_stage last_access)
    {
        for (auto hnd : handle)
        {
            _resource_lib.add_image_usage(hnd, image_resource_usage::STORAGE);
        }

        _image_states.push_back(image_resource_state{
            .type = access,
            .handles = {handle.begin(), handle.end()},
            .usage = image_resource_usage::STORAGE,
            .first_access = first_access,
            .last_access = last_access,
            .set = set,
            .binding = binding,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_structured_buffer(buffer_resource_handle handle,
                                                                  resource_access_type access, std::uint32_t set,
                                                                  std::uint32_t binding, pipeline_stage first_access,
                                                                  pipeline_stage last_access)
    {
        _resource_lib.add_buffer_usage(handle, buffer_resource_usage::STRUCTURED);
        _buffer_states.push_back(buffer_resource_state{
            .type = access,
            .buf = handle,
            .usage = buffer_resource_usage::STRUCTURED,
            .first_access = first_access,
            .last_access = last_access,
            .set = set,
            .binding = binding,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_vertex_buffer(buffer_resource_handle handle, pipeline_stage first_read,
                                                              pipeline_stage last_read)
    {
        _resource_lib.add_buffer_usage(handle, buffer_resource_usage::VERTEX);
        _buffer_states.push_back(buffer_resource_state{
            .type = resource_access_type::READ,
            .buf = handle,
            .usage = buffer_resource_usage::VERTEX,
            .first_access = first_read,
            .last_access = last_read,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_index_buffer(buffer_resource_handle handle, pipeline_stage first_read,
                                                             pipeline_stage last_read)
    {
        _resource_lib.add_buffer_usage(handle, buffer_resource_usage::INDEX);
        _buffer_states.push_back(buffer_resource_state{
            .type = resource_access_type::READ,
            .buf = handle,
            .usage = buffer_resource_usage::INDEX,
            .first_access = first_read,
            .last_access = last_read,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_constant_buffer(buffer_resource_handle handle, std::uint32_t set,
                                                                std::uint32_t binding, pipeline_stage first_read,
                                                                pipeline_stage last_read)
    {
        _resource_lib.add_buffer_usage(handle, buffer_resource_usage::CONSTANT);
        _buffer_states.push_back(buffer_resource_state{
            .type = resource_access_type::READ,
            .buf = handle,
            .usage = buffer_resource_usage::CONSTANT,
            .first_access = first_read,
            .last_access = last_read,
            .set = set,
            .binding = binding,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_indirect_argument_buffer(buffer_resource_handle handle,
                                                                         pipeline_stage first_read,
                                                                         pipeline_stage last_read)
    {
        _resource_lib.add_buffer_usage(handle, buffer_resource_usage::INDIRECT_ARGUMENT);
        _buffer_states.push_back(buffer_resource_state{
            .type = resource_access_type::READ,
            .buf = handle,
            .usage = buffer_resource_usage::INDIRECT_ARGUMENT,
            .first_access = first_read,
            .last_access = last_read,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_transfer_source_buffer(buffer_resource_handle handle,
                                                                       pipeline_stage first_read,
                                                                       pipeline_stage last_read)
    {
        _resource_lib.add_buffer_usage(handle, buffer_resource_usage::TRANSFER_SOURCE);
        _buffer_states.push_back(buffer_resource_state{
            .type = resource_access_type::READ,
            .buf = handle,
            .usage = buffer_resource_usage::TRANSFER_SOURCE,
            .first_access = first_read,
            .last_access = last_read,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_transfer_destination_buffer(buffer_resource_handle handle,
                                                                            pipeline_stage first_write,
                                                                            pipeline_stage last_write)
    {
        _resource_lib.add_buffer_usage(handle, buffer_resource_usage::TRANSFER_DESTINATION);
        _buffer_states.push_back(buffer_resource_state{
            .type = resource_access_type::WRITE,
            .buf = handle,
            .usage = buffer_resource_usage::TRANSFER_DESTINATION,
            .first_access = first_write,
            .last_access = last_write,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_host_write_buffer(buffer_resource_handle handle,
                                                                  pipeline_stage first_write, pipeline_stage last_write)
    {
        _buffer_states.push_back(buffer_resource_state{
            .type = resource_access_type::WRITE,
            .buf = handle,
            .usage = buffer_resource_usage::HOST_WRITE,
            .first_access = first_write,
            .last_access = last_write,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::add_sampler(sampler_resource_handle handle, std::uint32_t set,
                                                        std::uint32_t binding, pipeline_stage usage)
    {
        _sampler_states.push_back(external_sampler_resource_state{
            .samplers = {{handle}},
            .stages = usage,
            .set = set,
            .binding = binding,
        });

        return *this;
    }

    graph_pass_builder& graph_pass_builder::depends_on(graph_pass_handle src)
    {
        _depends_on.push_back(src);
        return *this;
    }

    graph_pass_builder& graph_pass_builder::resolve_image(image_resource_handle src, image_resource_handle dst,
                                                          pipeline_stage first_access, pipeline_stage last_access)
    {
        _resource_lib.add_image_usage(src, image_resource_usage::TRANSFER_SOURCE);
        _resource_lib.add_image_usage(dst, image_resource_usage::TRANSFER_DESTINATION);
        _resolve_images.push_back(resolve_image_state{
            .src = src,
            .dst = dst,
            .first_access = first_access,
            .last_access = last_access,
        });
        return *this;
    }

    graph_pass_builder& graph_pass_builder::on_execute(std::function<void(command_list&)> commands)
    {
        _commands = commands;
        return *this;
    }

    graph_pass_builder& graph_pass_builder::should_execute(std::function<bool()> fn)
    {
        _should_execute = fn;
        return *this;
    }

    std::string_view graph_pass_builder::name() const noexcept
    {
        return _name;
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

        for (auto& state : _buffer_states)
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

    render_graph_compiler::render_graph_compiler(abstract_allocator* alloc, render_device* device)
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

    graph_pass_handle render_graph_compiler::add_graph_pass(std::string_view name, queue_operation_type type,
                                                            std::function<void(graph_pass_builder&)> build)
    {
        graph_pass_builder bldr(*_resource_lib, name, type);

        build(bldr);

        _builders.push_back(std::move(bldr));

        graph_pass_handle handle;
        handle.id = static_cast<std::uint32_t>(_builders.size() - 1);
        handle.generation = 0;

        _builders.back()._self = handle;

        return handle;
    }

    void render_graph_compiler::enable_imgui(bool enabled)
    {
        _imgui_enabled = enabled;
    }

    void render_graph_compiler::enable_gpu_profiling(bool enabled)
    {
        _gpu_profiling_enabled = enabled;
    }

    std::unique_ptr<render_graph_compiler> render_graph_compiler::create_compiler(abstract_allocator* alloc,
                                                                                  render_device* device)
    {
        return std::make_unique<vk::render_graph_compiler>(alloc, device);
    }

    void dependency_graph::add_graph_pass(std::uint64_t pass_id)
    {
        if (!_adjacency_list.contains(pass_id))
        {
            _adjacency_list[pass_id] = {};
        }
    }

    void dependency_graph::add_graph_dependency(std::uint64_t src_pass, std::uint64_t dst_pass)
    {
        _adjacency_list[src_pass].push_back(dst_pass);
        if (!_adjacency_list.contains(dst_pass))
        {
            add_graph_pass(dst_pass);
        }
    }

    namespace
    {
        void dfs(const std::unordered_map<std::uint64_t, vector<std::uint64_t>>& adjacency_list,
                 std::unordered_set<std::uint64_t>& visited, std::uint64_t node, vector<std::uint64_t>& results)
        {
            visited.insert(node);

            const auto it = adjacency_list.find(node);
            if (it != adjacency_list.cend())
            {
                for (std::uint64_t adjacency : it->second)
                {
                    if (!visited.contains(adjacency))
                    {
                        dfs(adjacency_list, visited, adjacency, results);
                    }
                }
            }
            results.push_back(node);
        }
    } // namespace

    vector<std::uint64_t> dependency_graph::toposort() const noexcept
    {
        std::unordered_set<std::uint64_t> visited;
        vector<std::uint64_t> result;
        result.reserve(_adjacency_list.size());

        for (std::size_t node : std::ranges::views::keys(_adjacency_list))
        {
            if (!visited.contains(node))
            {
                dfs(_adjacency_list, visited, node, result);
            }
        }

        std::reverse(tempest::begin(result), tempest::end(result));

        return result;
    }
} // namespace tempest::graphics