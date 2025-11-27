#include <tempest/assert.hpp>
#include <tempest/enum.hpp>
#include <tempest/frame_graph.hpp>

// TODO: Implement deque + queue
// TODO: Implement unordered_set + set
#include <tempest/flat_unordered_map.hpp>
#include <tempest/int.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/vector.hpp>

namespace tempest::graphics
{
    namespace
    {
        base_graph_resource_handle copy(const base_graph_resource_handle& handle)
        {
            return base_graph_resource_handle(handle.handle, handle.version, handle.type);
        }

        inline constexpr enum_mask<rhi::memory_access> read_access_mask = make_enum_mask(
            rhi::memory_access::color_attachment_read, rhi::memory_access::depth_stencil_attachment_read,
            rhi::memory_access::shader_read, rhi::memory_access::shader_sampled_read,
            rhi::memory_access::shader_storage_read, rhi::memory_access::index_read,
            rhi::memory_access::vertex_attribute_read, rhi::memory_access::constant_buffer_read,
            rhi::memory_access::transfer_read, rhi::memory_access::host_read, rhi::memory_access::memory_read);

        inline constexpr enum_mask<rhi::memory_access> write_access_mask = make_enum_mask(
            rhi::memory_access::color_attachment_write, rhi::memory_access::depth_stencil_attachment_write,
            rhi::memory_access::shader_write, rhi::memory_access::shader_storage_write,
            rhi::memory_access::transfer_write, rhi::memory_access::host_write, rhi::memory_access::memory_write);

        constexpr bool is_read_access(enum_mask<rhi::memory_access> access)
        {
            return (access & read_access_mask) != enum_mask<rhi::memory_access>(rhi::memory_access::none);
        }

        constexpr bool is_write_access(enum_mask<rhi::memory_access> access)
        {
            return (access & write_access_mask) != enum_mask<rhi::memory_access>(rhi::memory_access::none);
        }

        inline constexpr enum_mask<rhi::memory_access> get_access_mask_for_layout(rhi::image_layout layout)
        {
            switch (layout)
            {
            case rhi::image_layout::color_attachment:
                return make_enum_mask(rhi::memory_access::color_attachment_read,
                                      rhi::memory_access::color_attachment_write);
            case rhi::image_layout::depth_stencil_read_write:
                return make_enum_mask(rhi::memory_access::depth_stencil_attachment_read,
                                      rhi::memory_access::depth_stencil_attachment_write);
            case rhi::image_layout::depth_stencil_read_only:
                return make_enum_mask(rhi::memory_access::depth_stencil_attachment_read);
            case rhi::image_layout::depth:
                return make_enum_mask(rhi::memory_access::depth_stencil_attachment_read,
                                      rhi::memory_access::depth_stencil_attachment_write);
            case rhi::image_layout::depth_read_only:
                return make_enum_mask(rhi::memory_access::depth_stencil_attachment_read);
            case rhi::image_layout::stencil:
                return make_enum_mask(rhi::memory_access::depth_stencil_attachment_read,
                                      rhi::memory_access::depth_stencil_attachment_write);
            case rhi::image_layout::stencil_read_only:
                return make_enum_mask(rhi::memory_access::depth_stencil_attachment_read);
            case rhi::image_layout::general:
                return make_enum_mask(rhi::memory_access::memory_read, rhi::memory_access::memory_write);
            case rhi::image_layout::present:
                return make_enum_mask(rhi::memory_access::memory_read, rhi::memory_access::memory_write);
            case rhi::image_layout::shader_read_only:
                return make_enum_mask(rhi::memory_access::shader_read, rhi::memory_access::shader_sampled_read);
            case rhi::image_layout::transfer_dst:
                return make_enum_mask(rhi::memory_access::transfer_write);
            case rhi::image_layout::transfer_src:
                return make_enum_mask(rhi::memory_access::transfer_read);
            case rhi::image_layout::undefined:
                return make_enum_mask(rhi::memory_access::none);
            }

            unreachable();
        }

    } // namespace

    void task_builder::read(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle)
    {
        read(handle, make_enum_mask(rhi::pipeline_stage::all), read_access_mask);
    }

    void task_builder::read(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle,
                            enum_mask<rhi::pipeline_stage> read_hints, enum_mask<rhi::memory_access> access_hints)
    {
        accesses.push_back(scheduled_resource_access{
            .handle = copy(handle),
            .stages = read_hints,
            .accesses = access_hints,
            .layout = rhi::image_layout::undefined,
        });
    }

    void task_builder::read(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout)
    {
        read(handle, layout, make_enum_mask(rhi::pipeline_stage::all), get_access_mask_for_layout(layout));
    }

    void task_builder::read(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout,
                            enum_mask<rhi::pipeline_stage> read_hints, enum_mask<rhi::memory_access> access_hints)
    {
        accesses.push_back(scheduled_resource_access{
            .handle = copy(handle),
            .stages = read_hints,
            .accesses = access_hints,
            .layout = layout,
        });
    }

    void task_builder::read(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle,
                            rhi::image_layout layout)
    {
        read(handle, layout, make_enum_mask(rhi::pipeline_stage::all), get_access_mask_for_layout(layout));
    }

    void task_builder::read(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle,
                            rhi::image_layout layout, enum_mask<rhi::pipeline_stage> read_hints,
                            enum_mask<rhi::memory_access> access_hints)
    {
        accesses.push_back(scheduled_resource_access{
            .handle = copy(handle),
            .stages = read_hints,
            .accesses = access_hints,
            .layout = layout,
        });
    }

    void task_builder::write(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle)
    {
        write(handle, make_enum_mask(rhi::pipeline_stage::all), write_access_mask);
    }

    void task_builder::write(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle,
                             enum_mask<rhi::pipeline_stage> write_hints, enum_mask<rhi::memory_access> access_hints)
    {
        handle.version += 1;
        auto current = copy(handle);

        accesses.push_back(scheduled_resource_access{
            .handle = tempest::move(current),
            .stages = write_hints,
            .accesses = access_hints,
            .layout = rhi::image_layout::undefined,
        });
    }

    void task_builder::write(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout)
    {
        write(handle, layout, make_enum_mask(rhi::pipeline_stage::all), get_access_mask_for_layout(layout));
    }

    void task_builder::write(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout,
                             enum_mask<rhi::pipeline_stage> write_hints, enum_mask<rhi::memory_access> access_hints)
    {
        handle.version += 1;
        auto current = copy(handle);

        accesses.push_back(scheduled_resource_access{
            .handle = tempest::move(current),
            .stages = write_hints,
            .accesses = access_hints,
            .layout = layout,
        });
    }

    void task_builder::write(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle,
                             rhi::image_layout layout)
    {
        write(handle, layout, make_enum_mask(rhi::pipeline_stage::all), get_access_mask_for_layout(layout));
    }

    void task_builder::write(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle,
                             rhi::image_layout layout, enum_mask<rhi::pipeline_stage> write_hints,
                             enum_mask<rhi::memory_access> access_hints)
    {
        handle.version += 1;
        auto current = copy(handle);

        accesses.push_back(scheduled_resource_access{
            .handle = tempest::move(current),
            .stages = write_hints,
            .accesses = access_hints,
            .layout = layout,
        });
    }

    void task_builder::read_write(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle)
    {
        read_write(handle, make_enum_mask(rhi::pipeline_stage::all), read_access_mask,
                   make_enum_mask(rhi::pipeline_stage::all), write_access_mask);
    }

    void task_builder::read_write(graph_resource_handle<rhi::rhi_handle_type::buffer>& handle,
                                  enum_mask<rhi::pipeline_stage> read_hints,
                                  enum_mask<rhi::memory_access> read_access_hints,
                                  enum_mask<rhi::pipeline_stage> write_hints,
                                  enum_mask<rhi::memory_access> write_access_hints)
    {
        auto current = copy(handle);

        accesses.push_back(scheduled_resource_access{
            .handle = copy(current),
            .stages = read_hints,
            .accesses = read_access_hints,
            .layout = rhi::image_layout::undefined,
        });

        handle.version += 1;
        current = copy(handle);

        accesses.push_back(scheduled_resource_access{
            .handle = tempest::move(current),
            .stages = write_hints,
            .accesses = write_access_hints,
            .layout = rhi::image_layout::undefined,
        });
    }

    void task_builder::read_write(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout)
    {
        read_write(handle, layout, make_enum_mask(rhi::pipeline_stage::all), get_access_mask_for_layout(layout),
                   make_enum_mask(rhi::pipeline_stage::all), get_access_mask_for_layout(layout));
    }

    void task_builder::read_write(graph_resource_handle<rhi::rhi_handle_type::image>& handle, rhi::image_layout layout,
                                  enum_mask<rhi::pipeline_stage> read_hints,
                                  enum_mask<rhi::memory_access> read_access_hints,
                                  enum_mask<rhi::pipeline_stage> write_hints,
                                  enum_mask<rhi::memory_access> write_access_hints)
    {
        auto current = copy(handle);
        accesses.push_back(scheduled_resource_access{
            .handle = copy(current),
            .stages = read_hints,
            .accesses = read_access_hints,
            .layout = layout,
        });
        handle.version += 1;
        current = copy(handle);
        accesses.push_back(scheduled_resource_access{
            .handle = tempest::move(current),
            .stages = write_hints,
            .accesses = write_access_hints,
            .layout = layout,
        });
    }

    void task_builder::read_write(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle,
                                  rhi::image_layout layout)
    {
        read_write(handle, layout, make_enum_mask(rhi::pipeline_stage::all), get_access_mask_for_layout(layout),
                   make_enum_mask(rhi::pipeline_stage::all), get_access_mask_for_layout(layout));
    }

    void task_builder::read_write(graph_resource_handle<rhi::rhi_handle_type::render_surface>& handle,
                                  rhi::image_layout layout, enum_mask<rhi::pipeline_stage> read_hints,
                                  enum_mask<rhi::memory_access> read_access_hints,
                                  enum_mask<rhi::pipeline_stage> write_hints,
                                  enum_mask<rhi::memory_access> write_access_hints)
    {
        auto current = copy(handle);
        accesses.push_back(scheduled_resource_access{
            .handle = copy(current),
            .stages = read_hints,
            .accesses = read_access_hints,
            .layout = layout,
        });
        handle.version += 1;
        current = copy(handle);
        accesses.push_back(scheduled_resource_access{
            .handle = tempest::move(current),
            .stages = write_hints,
            .accesses = write_access_hints,
            .layout = layout,
        });
    }

    void task_builder::depends_on(string task_name)
    {
        dependencies.push_back(tempest::move(task_name));
    }

    void compute_task_builder::prefer_async()
    {
        _prefer_async = true;
    }

    void transfer_task_builder::prefer_async()
    {
        _prefer_async = true;
    }

    rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> task_execution_context::find_buffer(
        graph_resource_handle<rhi::rhi_handle_type::buffer> handle) const
    {
        return _executor->get_buffer(handle);
    }

    rhi::typed_rhi_handle<rhi::rhi_handle_type::image> task_execution_context::find_image(
        graph_resource_handle<rhi::rhi_handle_type::image> handle) const
    {
        return _executor->get_image(handle);
    }

    rhi::typed_rhi_handle<rhi::rhi_handle_type::image> task_execution_context::find_image(
        graph_resource_handle<rhi::rhi_handle_type::render_surface> handle) const
    {
        return _executor->get_image(handle);
    }

    void task_execution_context::bind_descriptor_buffers(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout, rhi::bind_point point, uint32_t first_set,
        span<const rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>> buffers, span<const uint64_t> offsets)
    {
        _queue->bind_descriptor_buffers(_cmd_list, layout, point, first_set, buffers, offsets);
    }

    void task_execution_context::bind_descriptor_buffers(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout, rhi::bind_point point, uint32_t first_set,
        span<const graph_resource_handle<rhi::rhi_handle_type::buffer>> buffers)
    {
        vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>> rhi_buffers;
        rhi_buffers.reserve(buffers.size());

        vector<uint64_t> offsets;
        offsets.reserve(buffers.size());

        for (const auto& handle : buffers)
        {
            rhi_buffers.push_back(find_buffer(handle));
            offsets.push_back(_executor->get_current_frame_resource_offset(handle));
        }

        _queue->bind_descriptor_buffers(_cmd_list, layout, point, first_set, rhi_buffers, offsets);
    }

    void task_execution_context::push_descriptors(rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout,
                                                  rhi::bind_point point, uint32_t set_idx,
                                                  span<const rhi::buffer_binding_descriptor> buffers,
                                                  span<const rhi::image_binding_descriptor> images,
                                                  span<const rhi::sampler_binding_descriptor> samplers)
    {
        _queue->push_descriptors(_cmd_list, layout, point, set_idx, buffers, images, samplers);
    }

    void task_execution_context::_raw_push_constants(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::pipeline_layout> layout, enum_mask<rhi::shader_stage> stages,
        uint32_t offset, span<const byte> data)
    {
        _queue->push_constants(_cmd_list, layout, stages, offset, data);
    }

    graph_resource_handle<rhi::rhi_handle_type::buffer> graph_builder::import_buffer(
        string name, rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> buffer)
    {
        auto handle =
            graph_resource_handle<rhi::rhi_handle_type::buffer>(_next_resource_id++, 0, rhi::rhi_handle_type::buffer);
        auto entry = resource_entry{
            .name = name,
            .handle = copy(handle),
            .resource = external_resource{buffer},
            .per_frame = false,
            .temporal = false,
            .render_target = false,
            .presentable = false,
        };

        _resources.push_back(tempest::move(entry));

        return handle;
    }

    graph_resource_handle<rhi::rhi_handle_type::image> graph_builder::import_image(
        string name, rhi::typed_rhi_handle<rhi::rhi_handle_type::image> image)
    {
        auto handle =
            graph_resource_handle<rhi::rhi_handle_type::image>(_next_resource_id++, 0, rhi::rhi_handle_type::image);
        auto entry = resource_entry{
            .name = name,
            .handle = copy(handle),
            .resource = external_resource{image},
            .per_frame = false,
            .temporal = false,
            .render_target = false,
            .presentable = false,
        };

        _resources.push_back(tempest::move(entry));

        return handle;
    }

    graph_resource_handle<rhi::rhi_handle_type::render_surface> graph_builder::import_render_surface(
        string name, rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> surface)
    {
        auto handle = graph_resource_handle<rhi::rhi_handle_type::render_surface>(_next_resource_id++, 0,
                                                                                  rhi::rhi_handle_type::render_surface);
        auto entry = resource_entry{
            .name = name,
            .handle = copy(handle),
            .resource = external_resource{surface},
            .per_frame = false,
            .temporal = false,
            .render_target = true,
            .presentable = true,
        };

        _resources.push_back(tempest::move(entry));

        return handle;
    }

    graph_resource_handle<rhi::rhi_handle_type::buffer> graph_builder::create_per_frame_buffer(rhi::buffer_desc desc)
    {
        auto handle =
            graph_resource_handle<rhi::rhi_handle_type::buffer>(_next_resource_id++, 0, rhi::rhi_handle_type::buffer);
        auto entry = resource_entry{
            .name = desc.name,
            .handle = copy(handle),
            .resource = internal_resource{desc},
            .per_frame = true,
            .temporal = false,
            .render_target = false,
            .presentable = false,
        };

        _resources.push_back(tempest::move(entry));

        return handle;
    }

    graph_resource_handle<rhi::rhi_handle_type::image> graph_builder::create_per_frame_image(rhi::image_desc desc)
    {
        auto handle =
            graph_resource_handle<rhi::rhi_handle_type::image>(_next_resource_id++, 0, rhi::rhi_handle_type::image);
        auto entry = resource_entry{
            .name = desc.name,
            .handle = copy(handle),
            .resource = internal_resource{desc},
            .per_frame = true,
            .temporal = false,
            .render_target = false,
            .presentable = false,
        };

        _resources.push_back(tempest::move(entry));

        return handle;
    }

    graph_resource_handle<rhi::rhi_handle_type::buffer> graph_builder::create_temporal_buffer(rhi::buffer_desc desc)
    {
        auto handle =
            graph_resource_handle<rhi::rhi_handle_type::buffer>(_next_resource_id++, 0, rhi::rhi_handle_type::buffer);
        auto entry = resource_entry{
            .name = desc.name,
            .handle = copy(handle),
            .resource = internal_resource{desc},
            .per_frame = false,
            .temporal = true,
            .render_target = false,
            .presentable = false,
        };

        _resources.push_back(tempest::move(entry));

        return handle;
    }

    graph_resource_handle<rhi::rhi_handle_type::image> graph_builder::create_temporal_image(rhi::image_desc desc)
    {
        auto handle =
            graph_resource_handle<rhi::rhi_handle_type::image>(_next_resource_id++, 0, rhi::rhi_handle_type::image);
        auto entry = resource_entry{
            .name = desc.name,
            .handle = copy(handle),
            .resource = internal_resource{desc},
            .per_frame = false,
            .temporal = true,
            .render_target = false,
            .presentable = false,
        };

        _resources.push_back(tempest::move(entry));

        return handle;
    }

    graph_resource_handle<rhi::rhi_handle_type::buffer> graph_builder::create_buffer(rhi::buffer_desc desc)
    {
        auto handle =
            graph_resource_handle<rhi::rhi_handle_type::buffer>(_next_resource_id++, 0, rhi::rhi_handle_type::buffer);
        auto entry = resource_entry{
            .name = desc.name,
            .handle = copy(handle),
            .resource = internal_resource{desc},
            .per_frame = false,
            .temporal = false,
            .render_target = false,
            .presentable = false,
        };

        _resources.push_back(tempest::move(entry));

        return handle;
    }

    graph_resource_handle<rhi::rhi_handle_type::image> graph_builder::create_image(rhi::image_desc desc)
    {
        auto handle =
            graph_resource_handle<rhi::rhi_handle_type::image>(_next_resource_id++, 0, rhi::rhi_handle_type::image);
        auto entry = resource_entry{
            .name = desc.name,
            .handle = copy(handle),
            .resource = internal_resource{desc},
            .per_frame = false,
            .temporal = false,
            .render_target = false,
            .presentable = false,
        };

        _resources.push_back(tempest::move(entry));

        return handle;
    }

    graph_resource_handle<rhi::rhi_handle_type::image> graph_builder::create_render_target(rhi::image_desc desc)
    {
        auto handle =
            graph_resource_handle<rhi::rhi_handle_type::image>(_next_resource_id++, 0, rhi::rhi_handle_type::image);
        auto entry = resource_entry{
            .name = desc.name,
            .handle = copy(handle),
            .resource = internal_resource{desc},
            .per_frame = false,
            .temporal = false,
            .render_target = true,
            .presentable = false,
        };

        _resources.push_back(tempest::move(entry));

        return handle;
    }

    graph_execution_plan graph_builder::compile(queue_configuration cfg) &&
    {
        return graph_compiler(tempest::move(_resources), tempest::move(_passes), cfg).compile();
    }

    void graph_builder::_create_pass_entry(string name, work_type type,
                                           function<void(task_execution_context&)> execution_context,
                                           task_builder& builder, bool async)
    {
        auto pass = pass_entry{};
        pass.name = tempest::move(name);
        pass.type = type;
        pass.execution_context = tempest::move(execution_context);
        pass.async = async;
        pass.explicit_dependencies = tempest::move(builder.dependencies);

        for (auto&& res : builder.accesses)
        {
            pass.resource_accesses.push_back({
                .handle = copy(res.handle),
                .stages = res.stages,
                .accesses = res.accesses,
                .layout = res.layout,
            });

            if (is_write_access(res.accesses))
            {
                pass.outputs.push_back(copy(res.handle));
            }
        }

        _passes.push_back(tempest::move(pass));
    }

    graph_compiler::graph_compiler(vector<resource_entry> resources, vector<pass_entry> passes, queue_configuration cfg)
        : _resources{tempest::move(resources)}, _passes{tempest::move(passes)}, _cfg{cfg}
    {
    }

    graph_execution_plan graph_compiler::compile()
    {
        const auto live_set = _gather_live_set();
        const auto dependency_graph = _build_dependency_graph(live_set);
        const auto sorted_passes = _topo_sort_kahns(dependency_graph);
        const auto queue_assignments = _assign_queue_type(live_set);
        const auto submit_batches = _create_submit_batches(sorted_passes, queue_assignments);
        return _build_execution_plan(submit_batches, live_set.resource_indices);
    }

    graph_compiler::live_set graph_compiler::_gather_live_set() const
    {
        auto live = live_set{};

        auto work_list = vector<size_t>{};

        for (size_t resource_idx = 0; resource_idx < _resources.size(); ++resource_idx)
        {
            const auto& resource = _resources[resource_idx];
            if (holds_alternative<external_resource>(resource.resource))
            {
                live.resource_indices.push_back(resource_idx);
            }

            // Find any writers for this resource
            for (size_t pass_idx = 0; pass_idx < _passes.size(); ++pass_idx)
            {
                const auto& pass = _passes[pass_idx];
                if (tempest::find_if(pass.outputs.cbegin(), pass.outputs.cend(), [&](const auto& output) {
                        return resource.handle.handle == output.handle && resource.handle.type == output.type;
                    }) != pass.outputs.cend())
                {

                    if (tempest::find(live.pass_indices.cbegin(), live.pass_indices.cend(), pass_idx) ==
                        live.pass_indices.cend())
                    {
                        live.pass_indices.push_back(pass_idx);
                    }

                    if (tempest::find(work_list.cbegin(), work_list.cend(), pass_idx) == work_list.cend())
                    {
                        work_list.push_back(pass_idx);
                    }

                    // Ensure resource is marked as live
                    if (tempest::find(live.resource_indices.cbegin(), live.resource_indices.cend(), resource_idx) ==
                        live.resource_indices.cend())
                    {
                        live.resource_indices.push_back(resource_idx);
                    }
                }
            }
        }

        while (!work_list.empty())
        {
            const auto pass_index = work_list.back();
            work_list.pop_back();

            const auto& pass = _passes[pass_index];

            for (auto& access : pass.resource_accesses)
            {
                const auto resource_it =
                    tempest::find_if(_resources.cbegin(), _resources.cend(), [&](const auto& resource) {
                        return resource.handle.handle == access.handle.handle &&
                               resource.handle.type == access.handle.type;
                    });

                if (resource_it != _resources.cend())
                {
                    const auto resource_index = tempest::distance(_resources.begin(), resource_it);
                    const auto live_resource_it =
                        tempest::find(live.resource_indices.cbegin(), live.resource_indices.cend(), resource_index);
                    if (live_resource_it == live.resource_indices.cend())
                    {
                        live.resource_indices.push_back(resource_index);

                        for (size_t producer_index = 0; producer_index < _passes.size(); ++producer_index)
                        {
                            const auto pass_output_it = tempest::find_if(
                                _passes[producer_index].outputs.cbegin(), _passes[producer_index].outputs.cend(),
                                [&](const auto& output) {
                                    return output.handle == access.handle.handle && output.type == access.handle.type;
                                });

                            if (pass_output_it != _passes[producer_index].outputs.cend() &&
                                tempest::find(live.pass_indices.cbegin(), live.pass_indices.cend(), producer_index) !=
                                    live.pass_indices.cend())
                            {
                                work_list.push_back(producer_index);
                            }
                        }
                    }
                }
            }
        }
        return live;
    }

    graph_compiler::dependency_graph graph_compiler::_build_dependency_graph(const live_set& live) const
    {
        auto dep_graph = dependency_graph{};

        dep_graph.passes.insert(dep_graph.passes.end(), live.pass_indices.cbegin(), live.pass_indices.cend());
        dep_graph.resources.insert(dep_graph.resources.end(), live.resource_indices.cbegin(),
                                   live.resource_indices.cend());

        for (auto consumer_index : live.pass_indices)
        {
            const auto& consumer = _passes[consumer_index];

            // Build a set of resource handles the consumer writes (for read/write coalescing)
            auto consumer_write_handles = vector<base_graph_resource_handle>();
            for (const auto& a : consumer.resource_accesses)
            {
                if (is_write_access(a.accesses))
                {
                    consumer_write_handles.push_back(a.handle);
                }
            }

            for (const auto& access : consumer.resource_accesses)
            {
                // If the consumer both reads and writes this resource, *skip* the read access
                if (is_read_access(access.accesses) &&
                    tempest::find_if(consumer_write_handles.cbegin(), consumer_write_handles.cend(),
                                     [&](const auto& a) { return a.handle == access.handle.handle; }) !=
                        consumer_write_handles.cend())
                {
                    continue;
                }

                const auto resource_it = tempest::find_if(_resources.cbegin(), _resources.cend(), [&](const auto& res) {
                    return res.handle.handle == access.handle.handle && res.handle.type == access.handle.type;
                });

                if (resource_it == _resources.cend())
                {
                    continue; // resource not found
                }

                // For all earlier producers in the live pass list
                for (size_t producer_index : dep_graph.passes)
                {
                    if (producer_index >= consumer_index)
                    {
                        continue;
                    }

                    const auto& producer = _passes[producer_index];

                    // Find if there is a direct explicit dependency
                    const auto explicit_dep_it = tempest::find(consumer.explicit_dependencies.cbegin(),
                                                               consumer.explicit_dependencies.cend(), producer.name);
                    if (explicit_dep_it != consumer.explicit_dependencies.cend())
                    {
                        auto explicit_dep = dependency_edge{};
                        explicit_dep.producer_pass_index = producer_index;
                        explicit_dep.consumer_pass_index = consumer_index;
                        explicit_dep.resource = base_graph_resource_handle::null();

                        dep_graph.edges.push_back(tempest::move(explicit_dep));
                    }

                    // Producer must *write* this resource to be considered a producer:
                    auto producer_access_it = tempest::find_if(
                        producer.resource_accesses.cbegin(), producer.resource_accesses.cend(), [&](const auto& res) {
                            return res.handle.handle == access.handle.handle && res.handle.type == access.handle.type &&
                                   is_write_access(res.accesses);
                        });

                    if (producer_access_it == producer.resource_accesses.cend())
                    {
                        continue; // producer does not write this resource
                    }

                    if (producer_index == consumer_index)
                    {
                        continue; // skip self-dependencies
                    }

                    auto dependency = dependency_edge{};
                    dependency.producer_pass_index = producer_index;
                    dependency.consumer_pass_index = consumer_index;
                    dependency.resource = copy(access.handle);

                    dependency.producer_stages = producer_access_it->stages;
                    dependency.producer_access = producer_access_it->accesses;

                    dependency.consumer_stages = access.stages;
                    dependency.consumer_access = access.accesses;

                    dep_graph.edges.push_back(tempest::move(dependency));
                }
            }
        }

        return dep_graph;
    }

    vector<size_t> graph_compiler::_topo_sort_kahns(const dependency_graph& graph) const
    {
        auto result = vector<size_t>{};
        auto in_degree = flat_unordered_map<size_t, size_t>{};

        for (size_t pass_idx : graph.passes)
        {
            in_degree[pass_idx] = 0;
        }

        for (const auto& edge : graph.edges)
        {
            in_degree[edge.consumer_pass_index]++;
        }

        auto ready = vector<size_t>{};
        for (const auto& [pass_idx, degree] : in_degree)
        {
            if (degree == 0)
            {
                ready.push_back(pass_idx);
            }
        }

        while (!ready.empty())
        {
            auto pass_idx = ready.back();
            ready.pop_back();
            result.push_back(pass_idx);
            for (const auto& edge : graph.edges)
            {
                if (edge.producer_pass_index == pass_idx)
                {
                    in_degree[edge.consumer_pass_index]--;
                    if (in_degree[edge.consumer_pass_index] == 0)
                    {
                        ready.push_back(edge.consumer_pass_index);
                    }
                }
            }
        }

        return result;
    }

    flat_unordered_map<size_t, work_type> graph_compiler::_assign_queue_type(const live_set& live) const
    {
        auto assignments = flat_unordered_map<size_t, work_type>();

        for (const auto& pass_index : live.pass_indices)
        {
            const auto& pass = _passes[pass_index];
            if (pass.async)
            {
                // Ensure the pass type is supported by the configuration
                // If the pass type is transfer and there is a transfer queue, assign it to transfer
                // If the pass type is compute and there is a compute queue, assign it to compute
                // If the pass type is transfer and there is no transfer queue but there is a compute queue, assign it
                // to compute
                // If the pass type is compute and there is no compute queue but there is a transfer queue, assign it to
                // graphics
                // If the pass type is transfer and there is no transfer or compute queue, assign it to graphics

                if (pass.type == work_type::transfer && _cfg.transfer_queues > 0)
                {
                    assignments[pass_index] = work_type::transfer;
                }
                else if (pass.type == work_type::compute && _cfg.compute_queues > 0)
                {
                    assignments[pass_index] = work_type::compute;
                }
                else if (pass.type == work_type::transfer && _cfg.compute_queues > 0)
                {
                    assignments[pass_index] = work_type::compute;
                }
                else
                {
                    assignments[pass_index] = work_type::graphics;
                }
            }
            else
            {
                // Default to graphics for non-async passes
                assignments[pass_index] = work_type::graphics;
            }
        }

        return assignments;
    }

    bool graph_compiler::_requires_split(size_t pass_idx, work_type queue,
                                         const flat_unordered_map<size_t, work_type>& queue_assignment,
                                         const flat_unordered_map<uint64_t, work_type>& acquired_resource_handles) const
    {
        if (queue_assignment.find(pass_idx)->second != queue)
        {
            return true;
        }

        const auto& pass = _passes[pass_idx];

        for (const auto& access : pass.resource_accesses)
        {
            const auto handle = access.handle.handle;
            const auto it = acquired_resource_handles.find(handle);
            if (it != acquired_resource_handles.cend() && it->second != queue)
            {
                return true;
            }
        }

        return false;
    }

    vector<graph_compiler::submit_batch> graph_compiler::_create_submit_batches(
        span<const size_t> topo_order, const flat_unordered_map<size_t, work_type>& queue_assignments) const
    {
        auto batches = vector<submit_batch>{};
        auto current_batch = submit_batch{};

        auto batch_passes = vector<size_t>{};

        auto resource_queue_assignments =
            flat_unordered_map<uint64_t, work_type>(); // storing the resource handle's handle value

        for (auto pass : topo_order)
        {
            if (current_batch.pass_indices.empty())
            {
                current_batch.type = queue_assignments.find(pass)->second;
                current_batch.pass_indices.push_back(pass);
                batch_passes.push_back(pass);

                // Gather the resources acquired by this pass
                const auto& pass_entry = _passes[pass];
                for (const auto& access : pass_entry.resource_accesses)
                {
                    resource_queue_assignments[access.handle.handle] = queue_assignments.find(pass)->second;
                }

                continue;
            }

            if (_requires_split(pass, current_batch.type, queue_assignments, resource_queue_assignments))
            {
                batches.push_back(tempest::move(current_batch));
                current_batch = submit_batch{};
                batch_passes.clear();

                // Start a new batch
                current_batch.type = queue_assignments.find(pass)->second;
                current_batch.pass_indices.push_back(pass);
                batch_passes.push_back(pass);

                // Gather the resources acquired by this pass
                const auto& pass_entry = _passes[pass];
                for (const auto& access : pass_entry.resource_accesses)
                {
                    resource_queue_assignments[access.handle.handle] = queue_assignments.find(pass)->second;
                }
            }
            else
            {
                current_batch.pass_indices.push_back(pass);
                batch_passes.push_back(pass);

                // Gather the resources acquired by this pass
                const auto& pass_entry = _passes[pass];
                for (const auto& access : pass_entry.resource_accesses)
                {
                    if (resource_queue_assignments.find(access.handle.handle) == resource_queue_assignments.cend())
                    {
                        resource_queue_assignments[access.handle.handle] = queue_assignments.find(pass)->second;
                    }
                }
            }
        }

        if (!current_batch.pass_indices.empty())
        {
            batches.push_back(tempest::move(current_batch));
        }

        return batches;
    }

    graph_execution_plan graph_compiler::_build_execution_plan(span<const submit_batch> batches,
                                                               span<const size_t> resource_indices)
    {
        struct last_usage_info
        {
            work_type queue;
            uint64_t queue_index;
            enum_mask<rhi::pipeline_stage> stages;
            enum_mask<rhi::memory_access> access;
            rhi::image_layout layout;
            uint64_t timeline_value;
            uint64_t last_submit_index;
        };

        auto plan = graph_execution_plan{};

        for (auto resource_index : resource_indices)
        {
            const auto& resource = _resources[resource_index];

            auto sched_res = scheduled_resource{
                .handle = copy(resource.handle),
                .creation_info = [&]() -> variant<external_resource, rhi::buffer_desc, rhi::image_desc> {
                    if (holds_alternative<internal_resource>(resource.resource))
                    {
                        const auto& res = get<internal_resource>(resource.resource);
                        if (holds_alternative<rhi::buffer_desc>(res))
                        {
                            return get<rhi::buffer_desc>(res);
                        }
                        else if (holds_alternative<rhi::image_desc>(res))
                        {
                            return get<rhi::image_desc>(res);
                        }
                    }

                    return get<external_resource>(resource.resource);
                }(),
                .per_frame = resource.per_frame,
                .temporal = resource.temporal,
                .render_target = resource.render_target,
                .presentable = resource.presentable,
            };

            plan.resources.push_back(tempest::move(sched_res));
        }

        auto last_usage_map = flat_unordered_map<uint64_t, last_usage_info>{}; // handle -> last usage
        auto queue_timelines =
            flat_unordered_map<work_type,
                               flat_unordered_map<uint64_t, uint64_t>>{}; // queue -> (queue index -> timeline value)

        for (auto i = 0u; i < _cfg.graphics_queues; ++i)
        {
            queue_timelines[work_type::graphics][i] = 1;
        }

        for (auto i = 0u; i < _cfg.compute_queues; ++i)
        {
            queue_timelines[work_type::compute][i] = 1;
        }

        for (auto i = 0u; i < _cfg.transfer_queues; ++i)
        {
            queue_timelines[work_type::transfer][i] = 1;
        }

        struct future_usage
        {
            enum_mask<rhi::pipeline_stage> stages;
            enum_mask<rhi::memory_access> access_mask;
            rhi::image_layout layout;
        };

        auto future_usage_map = flat_unordered_map<uint64_t, flat_unordered_map<work_type, future_usage>>{};

        for (auto batch_idx = static_cast<ptrdiff_t>(batches.size() - 1); batch_idx >= 0; --batch_idx)
        {
            const auto& batch = batches[batch_idx];
            for (size_t pass_idx : batch.pass_indices)
            {
                const auto& pass = _passes[pass_idx];
                for (auto& access : pass.resource_accesses)
                {
                    auto& usage = future_usage_map[access.handle.handle][batch.type];
                    usage.stages |= access.stages;
                    usage.access_mask |= access.accesses;
                    usage.layout = access.layout;
                }
            }
        }

        for (size_t batch_idx = 0; batch_idx < batches.size(); ++batch_idx)
        {
            const auto& batch = batches[batch_idx];

            auto instructions = submit_instructions{};
            instructions.type = batch.type;
            instructions.queue_index = 0; // TODO: assign proper queue index

            auto& batch_timeline = queue_timelines[batch.type][instructions.queue_index];
            auto ownership_transferred_in_batch = vector<uint64_t>{};

            for (size_t pass_idx : batch.pass_indices)
            {
                const auto& pass = _passes[pass_idx];
                scheduled_pass sched_pass;
                sched_pass.name = pass.name;
                sched_pass.type = pass.type;

                for (auto& access : pass.resource_accesses)
                {
                    auto& last_usage = last_usage_map[access.handle.handle];
                    bool first_use = (last_usage.timeline_value == 0);

                    if (!first_use && last_usage.queue != batch.type &&
                        tempest::find(ownership_transferred_in_batch.cbegin(), ownership_transferred_in_batch.cend(),
                                      access.handle.handle) == ownership_transferred_in_batch.cend())
                    {

                        // Cross-queue ownership transfer
                        uint64_t signal_value = last_usage.timeline_value + 1;

                        // SOURCE queue: release and signal on its own timeline
                        submit_instructions& src_instructions = plan.submissions[last_usage.last_submit_index];
                        const auto& future_usage = future_usage_map[access.handle.handle][last_usage.queue];
                        src_instructions.released_resources.push_back({
                            .handle = copy(access.handle),
                            .src_queue = last_usage.queue,
                            .dst_queue = batch.type,
                            .src_stages = last_usage.stages,
                            .dst_stages = future_usage.stages,
                            .src_accesses = last_usage.access,
                            .dst_accesses = future_usage.access_mask,
                            .wait_value = 0,
                            .signal_value = signal_value,
                            .src_layout = last_usage.layout,
                            .dst_layout = future_usage.layout,
                        });

                        src_instructions.signals.push_back({
                            .type = last_usage.queue,
                            .queue_index = last_usage.queue_index,
                            .value = signal_value,
                            .stages = last_usage.stages,
                        });

                        // DESTINATION queue: acquire and wait on source queue timeline
                        instructions.acquired_resources.push_back({
                            .handle = copy(access.handle),
                            .src_queue = last_usage.queue,
                            .dst_queue = batch.type,
                            .src_stages = last_usage.stages,
                            .dst_stages = future_usage.stages,
                            .src_accesses = last_usage.access,
                            .dst_accesses = future_usage.access_mask,
                            .wait_value = signal_value,
                            .signal_value = 0, // destination timeline increment optional
                            .src_layout = last_usage.layout,
                            .dst_layout = future_usage.layout,
                        });
                        instructions.waits.push_back({
                            last_usage.queue,
                            last_usage.queue_index,
                            signal_value,
                            last_usage.stages,
                        });

                        ownership_transferred_in_batch.push_back(access.handle.handle);
                    }
                    else
                    {
                        // Same queue: merge stages/access masks
                        last_usage.stages |= access.stages;
                        last_usage.access |= access.accesses;
                    }

                    // Update last usage info
                    last_usage.queue = batch.type;
                    last_usage.queue_index = instructions.queue_index;
                    last_usage.stages |= access.stages;
                    last_usage.access |= access.accesses;
                    last_usage.timeline_value = batch_timeline;
                    last_usage.last_submit_index = batch_idx;
                    last_usage.layout = access.layout;

                    sched_pass.accesses.push_back({
                        .handle = copy(access.handle),
                        .stages = access.stages,
                        .accesses = access.accesses,
                        .layout = access.layout,
                    });
                }

                sched_pass.execution_context = pass.execution_context;
                instructions.passes.push_back(move(sched_pass));
            }

            plan.submissions.push_back(move(instructions));
        }

        plan.queue_cfg = _cfg;

        return plan;
    }

    graph_executor::graph_executor(rhi::device& device) : _device{&device}
    {
    }

    void graph_executor::execute()
    {
        // Get all queues that need to be waited on
        const auto frame_in_flight = _current_frame % _device->frames_in_flight();
        auto fences_to_wait = vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::fence>>{};
        for (auto& [type, fence_info] : _per_frame_fences[frame_in_flight].frame_complete_fence)
        {
            if (fence_info.queue_used)
            {
                fences_to_wait.push_back(fence_info.fence);
                fence_info.queue_used = false;
            }
        }

        // TODO: Restructure so only the graphics/present fence is waited on here, and all others are waited on
        // just before their first use in the frame. This can also allow deferral of work queue reset and fence reset
        // until just before first use in the frame (not first submission on the queue, but first command buffer usage)
        if (!fences_to_wait.empty())
        {
            _device->wait(fences_to_wait);
        }

        _device->release_resources();

        _device->get_primary_work_queue().reset(frame_in_flight);
        _device->get_dedicated_compute_queue().reset(frame_in_flight);
        _device->get_dedicated_transfer_queue().reset(frame_in_flight);

        const auto acquired_swapchains = _acquire_swapchain_images();
        if (!acquired_swapchains.empty())
        {
            if (!fences_to_wait.empty())
            {
                _device->reset(fences_to_wait);
            }

            _wait_for_swapchain_acquire(acquired_swapchains);
            _execute_plan(acquired_swapchains);
            _present_swapchain_images(acquired_swapchains);
        }

        _device->finish_frame();
    }

    void graph_executor::set_execution_plan(graph_execution_plan plan)
    {
        _destroy_owned_resources();
        _plan = tempest::move(plan);
        _construct_owned_resources();
    }

    rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> graph_executor::get_buffer(
        const base_graph_resource_handle& handle) const
    {
        if (get_resource_type(handle) != rhi::rhi_handle_type::buffer)
        {
            return rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>::null_handle;
        }

        const auto it = _all_buffers.find(handle.handle);
        if (it != _all_buffers.cend())
        {
            return it->second;
        }
        return rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>::null_handle;
    }

    rhi::typed_rhi_handle<rhi::rhi_handle_type::image> graph_executor::get_image(
        const base_graph_resource_handle& handle) const
    {
        if (get_resource_type(handle) == rhi::rhi_handle_type::image)
        {
            const auto it = _all_images.find(handle.handle);
            if (it != _all_images.cend())
            {
                return it->second;
            }
        }
        else if (get_resource_type(handle) == rhi::rhi_handle_type::render_surface)
        {
            const auto it = _current_swapchain_images.find(handle.handle);
            if (it != _current_swapchain_images.cend())
            {
                return it->second;
            }
        }

        return rhi::typed_rhi_handle<rhi::rhi_handle_type::image>::null_handle;
    }

    uint64_t graph_executor::get_current_frame_resource_offset(
        graph_resource_handle<rhi::rhi_handle_type::buffer> buffer) const
    {
        const auto it = tempest::find_if(_plan->resources.cbegin(), _plan->resources.cend(), [&](const auto& res) {
            return res.handle.handle == buffer.handle && res.handle.type == buffer.type;
        });

        if (it != _plan->resources.cend() && it->per_frame)
        {
            const auto size = visit(
                [&](auto&& res) -> size_t {
                    using type = remove_const_t<remove_reference_t<decltype(res)>>;

                    if constexpr (is_same_v<type, external_resource>)
                    {
                        if (auto buf_handle = get_if<rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>>(&res))
                        {
                            return _device->get_buffer_size(*buf_handle);
                        }
                        return 0;
                    }
                    else if constexpr (is_same_v<type, rhi::buffer_desc>)
                    {
                        return res.size;
                    }
                    else
                    {
                        return 0;
                    }
                },
                it->creation_info);

            return _current_frame % _device->frames_in_flight() * size;
        }

        return 0;
    }

    uint64_t graph_executor::get_resource_size(graph_resource_handle<rhi::rhi_handle_type::buffer> buffer) const
    {
        const auto it = tempest::find_if(_plan->resources.cbegin(), _plan->resources.cend(), [&](const auto& res) {
            return res.handle.handle == buffer.handle && res.handle.type == buffer.type;
        });

        if (it != _plan->resources.cend())
        {
            // Check if this is an external resource
            const auto size = visit(
                [&](auto&& res) -> size_t {
                    using type = remove_const_t<remove_reference_t<decltype(res)>>;

                    if constexpr (is_same_v<type, external_resource>)
                    {
                        if (auto buf_handle = get_if<rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>>(&res))
                        {
                            return _device->get_buffer_size(*buf_handle);
                        }
                        return 0;
                    }
                    else if constexpr (is_same_v<type, rhi::buffer_desc>)
                    {
                        return res.size;
                    }
                    else
                    {
                        return 0;
                    }
                },
                it->creation_info);

            if (it->per_frame && holds_alternative<external_resource>(it->creation_info))
            {
                return size / _device->frames_in_flight();
            }
            return size;
        }

        return 0;
    }

    rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> graph_executor::get_render_surface(
        const base_graph_resource_handle& handle) const
    {
        for (const auto& [res_handle, surface] : _external_surfaces)
        {
            if (res_handle == handle.handle)
            {
                return surface;
            }
        }
        return rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>::null_handle;
    }

    void graph_executor::_construct_owned_resources()
    {
        for (const auto& resource : _plan->resources)
        {
            if (holds_alternative<external_resource>(resource.creation_info))
            {
                const auto& ext_res = get<external_resource>(resource.creation_info);
                if (holds_alternative<rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>>(ext_res))
                {
                    const auto& buffer = get<rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>>(ext_res);
                    _all_buffers[resource.handle.handle] = buffer;
                }
                else if (holds_alternative<rhi::typed_rhi_handle<rhi::rhi_handle_type::image>>(ext_res))
                {
                    const auto& image = get<rhi::typed_rhi_handle<rhi::rhi_handle_type::image>>(ext_res);
                    _all_images[resource.handle.handle] = image;
                }
                else if (holds_alternative<rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>>(ext_res))
                {
                    const auto& surface = get<rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>>(ext_res);
                    _external_surfaces.push_back(make_pair(resource.handle.handle, surface));
                }
            }
            else if (holds_alternative<rhi::buffer_desc>(resource.creation_info))
            {
                // Intentional copy to modify size if per-frame
                rhi::buffer_desc desc = get<rhi::buffer_desc>(resource.creation_info);

                if (resource.per_frame)
                {
                    desc.size *= _device->frames_in_flight();
                }

                auto buffer = _device->create_buffer(desc);
                _owned_buffers[resource.handle.handle] = buffer;
                _all_buffers[resource.handle.handle] = buffer;
            }
            else if (holds_alternative<rhi::image_desc>(resource.creation_info))
            {
                const auto& desc = get<rhi::image_desc>(resource.creation_info);
                auto image = _device->create_image(desc);
                _owned_images[resource.handle.handle] = image;
                _all_images[resource.handle.handle] = image;
            }
        }

        // Construct the queue timelines
        for (size_t idx = 0; idx < _plan->queue_cfg.graphics_queues; ++idx)
        {
            _queue_timelines[work_type::graphics].push_back({
                .sem = _device->create_semaphore({
                    .type = rhi::semaphore_type::timeline,
                    .initial_value = 0,
                }),
                .value = 0,
            });
        }

        for (size_t idx = 0; idx < _plan->queue_cfg.compute_queues; ++idx)
        {
            _queue_timelines[work_type::compute].push_back({
                .sem = _device->create_semaphore({
                    .type = rhi::semaphore_type::timeline,
                    .initial_value = 0,
                }),
                .value = 0,
            });
        }

        for (size_t idx = 0; idx < _plan->queue_cfg.transfer_queues; ++idx)
        {
            _queue_timelines[work_type::transfer].push_back({
                .sem = _device->create_semaphore({
                    .type = rhi::semaphore_type::timeline,
                    .initial_value = 0,
                }),
                .value = 0,
            });
        }

        // Build fence for each frame
        _per_frame_fences.resize(_device->frames_in_flight());
        for (size_t idx = 0; idx < _device->frames_in_flight(); ++idx)
        {
            if (_plan->queue_cfg.graphics_queues > 0)
            {
                _per_frame_fences[idx].frame_complete_fence[work_type::graphics] = {
                    .fence = _device->create_fence({.signaled = false}),
                    .queue_used = false,
                };
            }

            if (_plan->queue_cfg.compute_queues > 0)
            {
                _per_frame_fences[idx].frame_complete_fence[work_type::compute] = {
                    .fence = _device->create_fence({.signaled = false}),
                    .queue_used = false,
                };
            }

            if (_plan->queue_cfg.transfer_queues > 0)
            {
                _per_frame_fences[idx].frame_complete_fence[work_type::transfer] = {
                    .fence = _device->create_fence({.signaled = false}),
                    .queue_used = false,
                };
            }
        }
    }

    void graph_executor::_destroy_owned_resources()
    {
        for (const auto& [handle, buffer] : _owned_buffers)
        {
            _device->destroy_buffer(buffer);
        }

        for (const auto& [handle, image] : _owned_images)
        {
            _device->destroy_image(image);
        }

        for (const auto& [type, timelines] : _queue_timelines)
        {
            for (const auto& timeline : timelines)
            {
                _device->destroy_semaphore(timeline.sem);
            }
        }

        for (auto& frame_fences : _per_frame_fences)
        {
            for (const auto& [type, exec_fence] : frame_fences.frame_complete_fence)
            {
                _device->destroy_fence(exec_fence.fence);
            }
        }

        _queue_timelines.clear();
        _owned_buffers.clear();
        _owned_images.clear();
        _all_buffers.clear();
        _all_images.clear();
        _external_surfaces.clear();
    }

    graph_executor::acquired_swapchains graph_executor::_acquire_swapchain_images()
    {
        auto results = vector<pair<rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>,
                                   rhi::swapchain_image_acquire_info_result>>{};

        for (auto it = _external_surfaces.cbegin(); it != _external_surfaces.cend();)
        {
            const auto& [handle, surface] = *it;
            const auto window = _device->get_window_surface(surface);

            if (window->should_close())
            {
                it = _external_surfaces.erase(it);
                continue;
            }

            if (window->framebuffer_width() == 0 || window->framebuffer_height() == 0 || window->minimized())
            {
                ++it;
                continue;
            }

            auto res = _device->acquire_next_image(surface);
            if (!res)
            {
                const auto err = res.error();
                if (err == rhi::swapchain_error_code::out_of_date)
                {
                    const auto recreate_info = rhi::render_surface_desc{
                        .window = window,
                        .min_image_count = 2,
                        .format =
                            {
                                .space = rhi::color_space::srgb_nonlinear,
                                .format = rhi::image_format::bgra8_srgb,
                            },
                        .present_mode = rhi::present_mode::immediate,
                        .width = window->framebuffer_width(),
                        .height = window->framebuffer_height(),
                        .layers = 1,
                    };

                    _device->recreate_render_surface(surface, recreate_info);
                    continue;
                }
                else if (err == rhi::swapchain_error_code::failure)
                {
                    it = _external_surfaces.erase(it);
                    continue;
                }
            }

            results.push_back(make_pair(surface, res.value()));
            ++it;
        }

        return results;
    }

    void graph_executor::_wait_for_swapchain_acquire(const acquired_swapchains& acquired)
    {
        auto wait_submit = rhi::work_queue::submit_info{};

        for (const auto& [surface, acquire_info] : acquired)
        {
            // Wait on the acquire semaphore
            wait_submit.wait_semaphores.push_back({
                .semaphore = acquire_info.acquire_sem,
                .value = 0, // binary semaphore, value doesn't matter
                .stages =
                    make_enum_mask(rhi::pipeline_stage::color_attachment_output, rhi::pipeline_stage::all_transfer),
            });
        }

        // Signal the timelines for each queue
        for (auto& [type, timelines] : _queue_timelines)
        {
            for (auto& timeline : timelines)
            {
                wait_submit.signal_semaphores.push_back({
                    .semaphore = timeline.sem,
                    .value = timeline.value + 1,
                    .stages = make_enum_mask(rhi::pipeline_stage::all),
                });

                timeline.value += 1; // Increment the timeline value
            }
        }

        auto& queue = _device->get_primary_work_queue();
        const array submits = {wait_submit};

        queue.submit(submits);
    }

    void graph_executor::_execute_plan(const acquired_swapchains& acquired)
    {
        _current_swapchain_images.clear();
        for (const auto& [surface, acquire_info] : acquired)
        {
            const auto res_it = tempest::find_if(_external_surfaces.cbegin(), _external_surfaces.cend(),
                                                 [&](const auto& pair) { return pair.second == surface; });
            if (res_it != _external_surfaces.cend())
            {
                _current_swapchain_images[res_it->first] = acquire_info.image;
            }
        }

        size_t submission_index = 0;
        for (const auto& submission : _plan->submissions)
        {
            auto get_queue = [dev = _device](work_type type) -> rhi::work_queue& {
                switch (type)
                {
                case work_type::graphics:
                    return dev->get_primary_work_queue();
                case work_type::compute:
                    return dev->get_dedicated_compute_queue();
                case work_type::transfer:
                    return dev->get_dedicated_transfer_queue();
                default:
                    return dev->get_primary_work_queue();
                }
            };

            auto& queue = get_queue(submission.type);

            auto command_list = queue.get_next_command_list();
            queue.begin_command_list(command_list, true);

            auto submit_info = rhi::work_queue::submit_info{};
            auto timeline_value = _queue_timelines[submission.type][submission.queue_index].value;

            struct sem_value
            {
                rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore> sem;
                uint64_t offset; // Offset from the timeline value at frame start
                uint64_t queue_value;
                enum_mask<rhi::pipeline_stage> stages;
            };

            // Handle waits on cross-queue ownership transfers with timeline semaphores
            auto wait_map = flat_unordered_map<uint64_t, sem_value>{}; // semaphore handle -> max wait value

            for (const auto& [_, sems] : _queue_timelines)
            {
                for (const auto& sem : sems)
                {
                    wait_map[sem.sem.id] = {
                        .sem = sem.sem,
                        .offset = 0,
                        .queue_value = sem.value,
                        .stages = make_enum_mask(rhi::pipeline_stage::none),
                    };
                }
            }

            for (const auto& wait : submission.waits)
            {
                const auto& timeline = _queue_timelines[wait.type][wait.queue_index];
                const auto& current_value = wait_map[timeline.sem.id];
                if (current_value.offset > wait.value)
                {
                    wait_map[timeline.sem.id].offset = current_value.offset;
                    wait_map[timeline.sem.id].stages |= wait.stages;
                }
            }

            // Handle signals on cross-queue ownership transfers with timeline semaphores
            auto signal_map = flat_unordered_map<uint64_t, sem_value>{}; // semaphore handle -> max signal value

            for (const auto& pass : submission.passes)
            {
                auto image_barriers = vector<rhi::work_queue::image_barrier>{};
                auto buffer_barriers = vector<rhi::work_queue::buffer_barrier>{};

                for (const auto& resource : pass.accesses)
                {
                    auto prior_usage_it = _current_resource_states.find(resource.handle.handle);
                    if (prior_usage_it != _current_resource_states.cend())
                    {
                        auto& prior_usage = prior_usage_it->second;
                        const auto cross_queue = prior_usage.queue != submission.type;

                        if (cross_queue)
                        {
                            auto sem_to_wait = _queue_timelines[prior_usage.queue][prior_usage.queue_index].sem;
                            auto wait_value = prior_usage.timeline_value;

                            const auto& current_value = wait_map[sem_to_wait.id];
                            if (current_value.offset > wait_value)
                            {
                                wait_map[sem_to_wait.id].offset = current_value.offset;
                                wait_map[sem_to_wait.id].stages |= prior_usage.stages;
                            }
                        }

                        rhi::work_queue* src_queue = nullptr;
                        rhi::work_queue* dst_queue = nullptr;

                        if (cross_queue)
                        {
                            src_queue = &get_queue(prior_usage.queue);
                            dst_queue = &queue;
                        }

                        const auto res_type = get_resource_type(resource.handle);
                        if (res_type == rhi::rhi_handle_type::image)
                        {
                            const auto image_it = _all_images.find(resource.handle.handle);
                            const auto& img_usage = tempest::get<image_usage>(prior_usage.usage);

                            auto existing_barrier_it = tempest::find_if(
                                image_barriers.begin(), image_barriers.end(),
                                [&](const auto& barrier) { return barrier.image.id == image_it->second.id; });
                            if (existing_barrier_it != image_barriers.end())
                            {
                                // Update existing barrier
                                TEMPEST_ASSERT(existing_barrier_it->new_layout == resource.layout);
                                existing_barrier_it->dst_stages |= resource.stages;
                                existing_barrier_it->dst_access |= resource.accesses;
                            }
                            else
                            {
                                // If src is a host operation and there is no ownership transfer or layout transition,
                                // we can skip the barrier entirely
                                if ((prior_usage.stages & make_enum_mask(rhi::pipeline_stage::host)) ==
                                        make_enum_mask(rhi::pipeline_stage::host) &&
                                    !cross_queue && img_usage.layout == resource.layout)
                                {
                                    continue;
                                }

                                // Create new barrier
                                const auto barrier = rhi::work_queue::image_barrier{
                                    .image = image_it->second,
                                    .old_layout = img_usage.layout,
                                    .new_layout = resource.layout,
                                    .src_stages = prior_usage.stages,
                                    .src_access = prior_usage.accesses,
                                    .dst_stages = resource.stages,
                                    .dst_access = resource.accesses,
                                    .src_queue = src_queue,
                                    .dst_queue = dst_queue,
                                };

                                image_barriers.push_back(barrier);
                            }
                        }
                        else if (res_type == rhi::rhi_handle_type::render_surface)
                        {
                            const auto surface_it = tempest::find_if(
                                _external_surfaces.begin(), _external_surfaces.end(),
                                [&](const auto& pair) { return pair.first == resource.handle.handle; });
                            const auto render_surface_info_it =
                                tempest::find_if(acquired.cbegin(), acquired.cend(),
                                                 [&](const auto& info) { return info.first == surface_it->second; });

                            if (render_surface_info_it != acquired.cend())
                            {
                                const auto& img_usage = tempest::get<image_usage>(prior_usage.usage);

                                auto existing_barrier_it = tempest::find_if(
                                    image_barriers.begin(), image_barriers.end(), [&](const auto& barrier) {
                                        return barrier.image.id == render_surface_info_it->second.image.id;
                                    });

                                if (existing_barrier_it != image_barriers.end())
                                {
                                    // Update existing barrier
                                    TEMPEST_ASSERT(existing_barrier_it->new_layout == resource.layout);
                                    existing_barrier_it->dst_stages |= resource.stages;
                                    existing_barrier_it->dst_access |= resource.accesses;
                                }
                                else
                                {
                                    // Add image layout transition from undefined to first usage
                                    const auto barrier = rhi::work_queue::image_barrier{
                                        .image = render_surface_info_it->second.image,
                                        .old_layout = img_usage.layout,
                                        .new_layout = resource.layout,
                                        .src_stages = prior_usage.stages,
                                        .src_access = prior_usage.accesses,
                                        .dst_stages = resource.stages,
                                        .dst_access = resource.accesses,
                                        .src_queue = src_queue,
                                        .dst_queue = dst_queue,
                                    };

                                    image_barriers.push_back(barrier);
                                }
                            }
                        }
                        else if (res_type == rhi::rhi_handle_type::buffer)
                        {
                            const auto buffer_it = _all_buffers.find(resource.handle.handle);
                            const auto& buf_usage = tempest::get<buffer_usage>(prior_usage.usage);
                            auto offset = buf_usage.offset;
                            auto range = buf_usage.range;

                            const auto create_info = _find_resource(resource.handle);

                            if (create_info && holds_alternative<rhi::buffer_desc>(create_info->creation_info))
                            {
                                const auto& buf_desc = get<rhi::buffer_desc>(create_info->creation_info);

                                // Get size of per-frame buffer
                                const auto per_frame_size = buf_desc.size;
                                const auto frame_offset = create_info->per_frame ? per_frame_size * _current_frame %
                                                                                       _device->frames_in_flight()
                                                                                 : 0;

                                offset = frame_offset + buf_usage.offset;
                                range = per_frame_size;
                            }

                            if (cross_queue)
                            {
                                offset = 0;
                                range = numeric_limits<size_t>::max();
                            }

                            // Search for prior write accesses that have not been waited on yet
                            const auto existing_write_barrier = _write_barriers.find(resource.handle.handle);

                            auto existing_write_stages = enum_mask<rhi::pipeline_stage>();
                            auto existing_write_accesses = enum_mask<rhi::memory_access>();

                            if (existing_write_barrier != _write_barriers.cend())
                            {
                                // Check if this pass's read usage overlaps with the existing write usage's reads
                                auto& write_usage = existing_write_barrier->second;
                                if ((write_usage.read_accesses_seen & resource.accesses) != resource.accesses ||
                                    (write_usage.read_stages_seen & resource.stages) != resource.stages)
                                {
                                    existing_write_stages |= write_usage.write_stages;
                                    existing_write_accesses |= write_usage.write_accesses;
                                }

                                // Reset the seen read accesses
                                if (is_write_access(resource.accesses))
                                {
                                    write_usage.read_accesses_seen = enum_mask<rhi::memory_access>();
                                    write_usage.read_stages_seen = enum_mask<rhi::pipeline_stage>();
                                    write_usage.write_accesses |= resource.accesses;
                                    write_usage.write_stages |= resource.stages;
                                }

                                if (is_read_access(resource.accesses))
                                {
                                    write_usage.read_accesses_seen |= resource.accesses;
                                    write_usage.read_stages_seen |= resource.stages;
                                }
                            }
                            else
                            {
                                auto write_usage = write_barrier_details{
                                    .write_stages = is_write_access(resource.accesses)
                                                        ? resource.stages
                                                        : enum_mask<rhi::pipeline_stage>(),
                                    .write_accesses = is_write_access(resource.accesses)
                                                          ? resource.accesses
                                                          : enum_mask<rhi::memory_access>(),
                                    .read_stages_seen = is_read_access(resource.accesses)
                                                            ? resource.stages
                                                            : enum_mask<rhi::pipeline_stage>(),
                                    .read_accesses_seen = is_read_access(resource.accesses)
                                                              ? resource.accesses
                                                              : enum_mask<rhi::memory_access>(),
                                };
                                _write_barriers[resource.handle.handle] = write_usage;
                            }

                            const auto existing_barrier_it = tempest::find_if(
                                buffer_barriers.begin(), buffer_barriers.end(),
                                [&](const auto& barrier) { return barrier.buffer.id == buffer_it->second.id; });
                            if (existing_barrier_it != buffer_barriers.end())
                            {
                                // Update existing barrier
                                existing_barrier_it->dst_stages |= resource.stages;
                                existing_barrier_it->dst_access |= resource.accesses;
                            }
                            else
                            {
                                // If src is a host operation and there is no ownership transfer,
                                // we can skip the barrier entirely
                                if ((prior_usage.stages & make_enum_mask(rhi::pipeline_stage::host)) ==
                                        make_enum_mask(rhi::pipeline_stage::host) &&
                                    !cross_queue)
                                {
                                    continue;
                                }

                                const auto barrier = rhi::work_queue::buffer_barrier{
                                    .buffer = buffer_it->second,
                                    .src_stages = existing_write_stages | prior_usage.stages,
                                    .src_access = existing_write_accesses | prior_usage.accesses,
                                    .dst_stages = resource.stages,
                                    .dst_access = resource.accesses,
                                    .src_queue = src_queue,
                                    .dst_queue = dst_queue,
                                    .offset = offset,
                                    .size = range,
                                };

                                buffer_barriers.push_back(barrier);
                            }
                        }
                    }
                    else
                    {
                        // If this is an image resource, we need to transition it from undefined to the first usage
                        // If this is a swapchain image, we need to transition it from the present layout to the first
                        // usage
                        const auto res_type = get_resource_type(resource.handle);
                        if (res_type == rhi::rhi_handle_type::image)
                        {
                            const auto image_it = _all_images.find(resource.handle.handle);

                            const auto barrier = rhi::work_queue::image_barrier{
                                .image = image_it->second,
                                .old_layout = rhi::image_layout::undefined,
                                .new_layout = resource.layout,
                                .src_stages = make_enum_mask(rhi::pipeline_stage::all_transfer,
                                                             rhi::pipeline_stage::color_attachment_output),
                                .src_access = make_enum_mask(rhi::memory_access::transfer_write,
                                                             rhi::memory_access::color_attachment_write),
                                .dst_stages = resource.stages,
                                .dst_access = resource.accesses,
                                .src_queue = nullptr,
                                .dst_queue = nullptr,
                            };

                            image_barriers.push_back(barrier);
                        }
                        else if (res_type == rhi::rhi_handle_type::render_surface)
                        {
                            const auto surface_it = tempest::find_if(
                                _external_surfaces.begin(), _external_surfaces.end(),
                                [&](const auto& pair) { return pair.first == resource.handle.handle; });
                            const auto render_surface_info_it =
                                tempest::find_if(acquired.cbegin(), acquired.cend(),
                                                 [&](const auto& info) { return info.first == surface_it->second; });

                            if (render_surface_info_it != acquired.cend())
                            {
                                // Add image layout transition from undefined to first usage
                                const auto barrier = rhi::work_queue::image_barrier{
                                    .image = render_surface_info_it->second.image,
                                    .old_layout = rhi::image_layout::undefined,
                                    .new_layout = resource.layout,
                                    .src_stages = make_enum_mask(rhi::pipeline_stage::all_transfer,
                                                                 rhi::pipeline_stage::color_attachment_output),
                                    .src_access = make_enum_mask(rhi::memory_access::transfer_write,
                                                                 rhi::memory_access::color_attachment_write),
                                    .dst_stages = resource.stages,
                                    .dst_access = resource.accesses,
                                    .src_queue = nullptr,
                                    .dst_queue = nullptr,
                                };

                                image_barriers.push_back(barrier);
                            }
                        }
                    }
                }

                queue.pipeline_barriers(command_list, image_barriers, buffer_barriers);

                // Execute the pass
                switch (pass.type)
                {
                case work_type::graphics: {
                    queue.begin_debug_region(command_list, pass.name.c_str());
                    auto executor = graphics_task_execution_context(this, command_list, &queue);
                    pass.execution_context(executor);
                    queue.end_debug_region(command_list);
                    break;
                }
                case work_type::compute: {
                    queue.begin_debug_region(command_list, pass.name.c_str());
                    auto executor = compute_task_execution_context(this, command_list, &queue);
                    pass.execution_context(executor);
                    queue.end_debug_region(command_list);
                    break;
                }
                case work_type::transfer: {
                    queue.begin_debug_region(command_list, pass.name.c_str());
                    auto executor = transfer_task_execution_context(this, command_list, &queue);
                    pass.execution_context(executor);
                    queue.end_debug_region(command_list);
                    break;
                }
                default:
                    // Should never reach here
                    break;
                }

                // Update the last used state for each resource
                for (const auto& resource : pass.accesses)
                {
                    auto res_type = get_resource_type(resource.handle);
                    if (res_type == rhi::rhi_handle_type::buffer)
                    {
                        auto buf_usage = buffer_usage{
                            .offset = 0,
                            .range = numeric_limits<size_t>::max(),
                        };

                        _current_resource_states[resource.handle.handle] = resource_usage{
                            .queue = submission.type,
                            .queue_index = submission.queue_index,
                            .stages = resource.stages,
                            .accesses = resource.accesses,
                            .usage = buf_usage,
                            .timeline_value = timeline_value,
                        };
                    }
                    else if (res_type == rhi::rhi_handle_type::image)
                    {
                        _current_resource_states[resource.handle.handle] = resource_usage{
                            .queue = submission.type,
                            .queue_index = submission.queue_index,
                            .stages = resource.stages,
                            .accesses = resource.accesses,
                            .usage =
                                image_usage{
                                    .base_mip = 0,
                                    .mip_levels = 1,
                                    .base_array_layer = 0,
                                    .array_layers = 1,
                                    .layout = resource.layout,
                                },
                            .timeline_value = timeline_value,
                        };
                    }
                    else if (res_type == rhi::rhi_handle_type::render_surface)
                    {
                        _current_resource_states[resource.handle.handle] = resource_usage{
                            .queue = submission.type,
                            .queue_index = submission.queue_index,
                            .stages = resource.stages,
                            .accesses = resource.accesses,
                            .usage =
                                image_usage{
                                    .base_mip = 0,
                                    .mip_levels = 1,
                                    .base_array_layer = 0,
                                    .array_layers = 1,
                                    .layout = resource.layout,
                                },
                            .timeline_value = timeline_value,
                        };
                    }
                }
            }

            for (const auto& signal : submission.signals)
            {
                const auto& timeline = _queue_timelines[signal.type][signal.queue_index];
                const auto& current_value = signal_map[timeline.sem.id];
                if (current_value.offset > signal.value)
                {
                    signal_map[timeline.sem.id].offset = current_value.offset;
                    signal_map[timeline.sem.id].stages |= signal.stages;
                }
            }

            // Set up barriers to transition any resources that were released in this submission to another queue
            auto release_buffer_ownership = vector<rhi::work_queue::buffer_barrier>{};
            auto release_image_ownership = vector<rhi::work_queue::image_barrier>{};

            for (const auto& rel_res : submission.released_resources)
            {
                const auto type = get_resource_type(rel_res.handle);
                switch (type)
                {
                case rhi::rhi_handle_type::buffer: {
                    auto barrier = rhi::work_queue::buffer_barrier{
                        .buffer = get_buffer(rel_res.handle),
                        .src_stages = rel_res.src_stages,
                        .src_access = rel_res.src_accesses,
                        .dst_stages = rel_res.dst_stages,
                        .dst_access = rel_res.dst_accesses,
                        .src_queue = &queue,
                        .dst_queue = &get_queue(rel_res.dst_queue),
                        .offset = 0,
                        .size = numeric_limits<size_t>::max(),
                    };
                    release_buffer_ownership.push_back(barrier);
                    break;
                }
                case rhi::rhi_handle_type::image:
                    [[fallthrough]];
                case rhi::rhi_handle_type::render_surface: {
                    auto barrier = rhi::work_queue::image_barrier{
                        .image = get_image(rel_res.handle),
                        .old_layout = rel_res.src_layout,
                        .new_layout = rel_res.dst_layout,
                        .src_stages = rel_res.src_stages,
                        .src_access = rel_res.src_accesses,
                        .dst_stages = rel_res.dst_stages,
                        .dst_access = rel_res.dst_accesses,
                        .src_queue = &queue,
                        .dst_queue = &get_queue(rel_res.dst_queue),
                    };
                    release_image_ownership.push_back(barrier);
                    break;
                }
                default:
                    break;
                }
            }

            // Fill out the wait and signal semaphores for the submit info
            for (const auto& [sem_id, value] : wait_map)
            {
                submit_info.wait_semaphores.push_back({
                    .semaphore = value.sem,
                    .value = value.offset + value.queue_value,
                    .stages = value.stages,
                });
            }

            for (const auto& [sem_id, value] : signal_map)
            {
                submit_info.signal_semaphores.push_back({
                    .semaphore = value.sem,
                    .value = value.offset + value.queue_value,
                    .stages = value.stages,
                });
            }

            // If this is the last submission in the frame for this queue family, signal the frame complete fence
            const auto frame_idx = _current_frame % _device->frames_in_flight();
            auto fence_handle = _per_frame_fences[frame_idx].frame_complete_fence[submission.type].fence;
            _per_frame_fences[frame_idx].frame_complete_fence[submission.type].queue_used = true;

            // Check the rest of the submissions for a queue match
            for (auto idx = submission_index + 1; idx < _plan->submissions.size(); ++idx)
            {
                if (_plan->submissions[idx].type == submission.type)
                {
                    fence_handle = rhi::typed_rhi_handle<rhi::rhi_handle_type::fence>::null_handle;
                    break;
                }
            }

            // If this is the last submission in the frame, transition any swapchain images back to present
            if (submission_index == _plan->submissions.size() - 1)
            {
                for (const auto& img : acquired)
                {
                    auto swapchain_resource_handle_it =
                        tempest::find_if(_external_surfaces.begin(), _external_surfaces.end(),
                                         [&](const auto& pair) { return pair.second == img.first; });
                    if (swapchain_resource_handle_it != _external_surfaces.cend())
                    {
                        auto last_usage_it = _current_resource_states.find(swapchain_resource_handle_it->first);

                        auto barrier = rhi::work_queue::image_barrier{
                            .image = img.second.image,
                            .old_layout = last_usage_it == _current_resource_states.end()
                                              ? rhi::image_layout::undefined
                                              : get<image_usage>(last_usage_it->second.usage).layout,
                            .new_layout = rhi::image_layout::present,
                            .src_stages = last_usage_it == _current_resource_states.end()
                                              ? make_enum_mask(rhi::pipeline_stage::bottom)
                                              : last_usage_it->second.stages,
                            .src_access = last_usage_it == _current_resource_states.end()
                                              ? make_enum_mask(rhi::memory_access::none)
                                              : last_usage_it->second.accesses,
                            .dst_stages = make_enum_mask(rhi::pipeline_stage::top),
                            .dst_access = make_enum_mask(rhi::memory_access::none),
                            .src_queue = nullptr,
                            .dst_queue = nullptr,
                        };

                        queue.transition_image(command_list, {&barrier, 1});

                        // Remove the swapchain image from the current resource states
                        _current_resource_states.erase(swapchain_resource_handle_it->first);
                    }
                }
            }

            queue.end_command_list(command_list);
            submit_info.command_lists.push_back(command_list);

            const array submits = {submit_info};
            queue.submit(submits, fence_handle);

            ++submission_index;
        }

        ++_current_frame;
    }

    void graph_executor::_present_swapchain_images(const acquired_swapchains& acquired)
    {
        auto& present_queue = _device->get_primary_work_queue();

        auto submit_info = rhi::work_queue::submit_info{};

        // Wait on all the queue timelines
        for (const auto& [type, timelines] : _queue_timelines)
        {
            for (const auto& timeline : timelines)
            {
                submit_info.wait_semaphores.push_back({
                    .semaphore = timeline.sem,
                    .value = timeline.value,
                    .stages =
                        make_enum_mask(rhi::pipeline_stage::all_transfer, rhi::pipeline_stage::color_attachment_output),
                });
            }
        }

        // Signal the render complete semaphores for each acquired swapchain image
        for (const auto& [surface, acquire_info] : acquired)
        {
            submit_info.signal_semaphores.push_back({
                .semaphore = acquire_info.render_complete_sem,
                .value = 0, // binary semaphore, value doesn't matter
                .stages = make_enum_mask(rhi::pipeline_stage::all),
            });
        }

        // Submit the gather timeline -> binary submit
        const auto submits = array{submit_info};
        present_queue.submit(submits);

        // Present the images
        auto present_info = rhi::work_queue::present_info{};
        for (const auto& [surface, acquire_info] : acquired)
        {
            present_info.swapchain_images.push_back({
                .render_surface = surface,
                .image_index = acquire_info.image_index,
            });

            present_info.wait_semaphores.push_back(acquire_info.render_complete_sem);
        }

        auto present_results = present_queue.present(present_info);

        auto idx = size_t(0);

        // Handle any out-of-date or suboptimal swapchains
        for (auto present_result : present_results)
        {
            if (present_result == rhi::work_queue::present_result::out_of_date ||
                present_result == rhi::work_queue::present_result::suboptimal)
            {
                auto surface_handle = acquired[idx].first;
                auto window = _device->get_window_surface(surface_handle);
                const auto recreate_info = rhi::render_surface_desc{
                    .window = window,
                    .min_image_count = 2,
                    .format =
                        {
                            .space = rhi::color_space::srgb_nonlinear,
                            .format = rhi::image_format::bgra8_srgb,
                        },
                    .present_mode = rhi::present_mode::immediate,
                    .width = window->framebuffer_width(),
                    .height = window->framebuffer_height(),
                    .layers = 1,
                };

                _device->recreate_render_surface(surface_handle, recreate_info);
            }
            else if (present_result == rhi::work_queue::present_result::error)
            {
                erase_if(_external_surfaces, [&](const auto& pair) { return pair.second == acquired[idx].first; });
            }
        }
    }

    optional<const scheduled_resource&> graph_executor::_find_resource(const base_graph_resource_handle& handle) const
    {
        const auto it = tempest::find_if(_plan->resources.cbegin(), _plan->resources.cend(),
                                         [&](const auto& res) { return res.handle.handle == handle.handle; });
        if (it != _plan->resources.cend())
        {
            return *it;
        }

        return nullopt;
    }

    void graphics_task_execution_context::begin_render_pass(const rhi::work_queue::render_pass_info& info)
    {
        _queue->begin_rendering(_cmd_list, info);
    }

    void graphics_task_execution_context::end_render_pass()
    {
        _queue->end_rendering(_cmd_list);
    }

    void graphics_task_execution_context::set_viewport(float x, float y, float width, float height, float min_depth,
                                                       float max_depth, bool flipped)
    {
        _queue->set_viewport(_cmd_list, x, y, width, height, min_depth, max_depth, 0, flipped);
    }

    void graphics_task_execution_context::set_scissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        _queue->set_scissor_region(_cmd_list, x, y, width, height, 0);
    }

    void graphics_task_execution_context::set_cull_mode(enum_mask<rhi::cull_mode> mode)
    {
        _queue->set_cull_mode(_cmd_list, mode);
    }

    void graphics_task_execution_context::bind_pipeline(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::graphics_pipeline> pipeline)
    {
        _queue->bind(_cmd_list, pipeline);
    }

    void graphics_task_execution_context::bind_index_buffer(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> index_buffer, rhi::index_format type, uint64_t offset)
    {
        _queue->bind_index_buffer(_cmd_list, index_buffer, static_cast<uint32_t>(offset), type);
    }

    void graphics_task_execution_context::bind_vertex_buffers(
        uint32_t first_binding, span<const rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer>> buffers,
        span<const size_t> offsets)
    {
        _queue->bind_vertex_buffers(_cmd_list, first_binding, buffers, offsets);
    }

    void graphics_task_execution_context::draw_indirect(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> indirect_buffer, uint32_t offset, uint32_t draw_count,
        uint32_t stride)
    {
        _queue->draw(_cmd_list, indirect_buffer, offset, draw_count, stride);
    }

    void graphics_task_execution_context::draw_indirect(
        graph_resource_handle<rhi::rhi_handle_type::buffer> indirect_buffer, uint32_t offset, uint32_t draw_count,
        uint32_t stride)
    {
        const auto buffer = _executor->get_buffer(indirect_buffer);
        _queue->draw(_cmd_list, buffer, offset, draw_count, stride);
    }

    void graphics_task_execution_context::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex,
                                               uint32_t first_instance)
    {
        _queue->draw(_cmd_list, vertex_count, instance_count, first_vertex, first_instance);
    }

    void graphics_task_execution_context::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index,
                                                      int32_t vertex_offset, uint32_t first_instance)
    {
        _queue->draw(_cmd_list, index_count, instance_count, first_index, vertex_offset, first_instance);
    }

    void compute_task_execution_context::bind_pipeline(
        rhi::typed_rhi_handle<rhi::rhi_handle_type::compute_pipeline> pipeline)
    {
        _queue->bind(_cmd_list, pipeline);
    }

    void compute_task_execution_context::dispatch(uint32_t group_count_x, uint32_t group_count_y,
                                                  uint32_t group_count_z)
    {
        _queue->dispatch(_cmd_list, group_count_x, group_count_y, group_count_z);
    }

    void transfer_task_execution_context::clear_color(const graph_resource_handle<rhi::rhi_handle_type::image>& image,
                                                      float r, float g, float b, float a)
    {
        const auto img = _executor->get_image(image);
        if (!img)
        {
            return;
        }

        _queue->clear_color_image(_cmd_list, img, rhi::image_layout::transfer_dst, r, g, b, a);
    }

    void transfer_task_execution_context::clear_color(
        const graph_resource_handle<rhi::rhi_handle_type::render_surface>& surface, float r, float g, float b, float a)
    {
        const auto img = _executor->get_image(surface);
        if (!img)
        {
            return;
        }

        _queue->clear_color_image(_cmd_list, img, rhi::image_layout::transfer_dst, r, g, b, a);
    }

    void transfer_task_execution_context::copy_buffer_to_buffer(
        const graph_resource_handle<rhi::rhi_handle_type::buffer>& src,
        const graph_resource_handle<rhi::rhi_handle_type::buffer>& dst, uint64_t src_offset, uint64_t dst_offset,
        uint64_t size)
    {
        const auto src_buf = _executor->get_buffer(src);
        const auto dst_buf = _executor->get_buffer(dst);
        if (!src_buf || !dst_buf)
        {
            return;
        }

        // TODO: Handle per-frame offsets
        _queue->copy(_cmd_list, src_buf, dst_buf, src_offset, dst_offset, size);
    }

    void transfer_task_execution_context::fill_buffer(const graph_resource_handle<rhi::rhi_handle_type::buffer>& dst,
                                                      uint64_t offset, uint64_t size, uint32_t data)
    {
        const auto dst_buf = _executor->get_buffer(dst);
        if (!dst_buf)
        {
            return;
        }

        _queue->fill(_cmd_list, dst_buf, offset, size, data);
    }

    void transfer_task_execution_context::blit(const graph_resource_handle<rhi::rhi_handle_type::image>& src,
                                               const graph_resource_handle<rhi::rhi_handle_type::image>& dst)
    {
        const auto src_img = _executor->get_image(src);
        const auto dst_img = _executor->get_image(dst);
        if (!src_img || !dst_img)
        {
            return;
        }
        _queue->blit(_cmd_list, src_img, rhi::image_layout::transfer_src, 0, dst_img, rhi::image_layout::transfer_dst,
                     0);
    }

    void transfer_task_execution_context::blit(const graph_resource_handle<rhi::rhi_handle_type::image>& src,
                                               const graph_resource_handle<rhi::rhi_handle_type::render_surface>& dst)
    {
        const auto src_img = _executor->get_image(src);
        const auto dst_img = _executor->get_image(dst);
        if (!src_img || !dst_img)
        {
            return;
        }
        _queue->blit(_cmd_list, src_img, rhi::image_layout::transfer_src, 0, dst_img, rhi::image_layout::transfer_dst,
                     0);
    }
} // namespace tempest::graphics