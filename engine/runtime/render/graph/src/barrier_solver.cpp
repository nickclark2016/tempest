#include <tempest/render_graph/barrier_solver.hpp>

namespace tempest::render_graph
{
    namespace
    {
        auto filter_stages_for_queue(enum_mask<rhi::pipeline_stage> stages, queue_type queue)
            -> enum_mask<rhi::pipeline_stage>
        {
            switch (queue)
            {
            case queue_type::graphics:
                return stages;
            case queue_type::async_compute:
                {
                    const auto valid = stages & (rhi::pipeline_stage::compute | rhi::pipeline_stage::all_transfer |
                                                 rhi::pipeline_stage::top_of_pipe | rhi::pipeline_stage::bottom_of_pipe);
                    return static_cast<bool>(valid) ? valid
                                                    : enum_mask<rhi::pipeline_stage>{rhi::pipeline_stage::top_of_pipe};
                }
            case queue_type::async_transfer:
                {
                    const auto valid = stages & (rhi::pipeline_stage::all_transfer |
                                                 rhi::pipeline_stage::top_of_pipe | rhi::pipeline_stage::bottom_of_pipe);
                    return static_cast<bool>(valid) ? valid
                                                    : enum_mask<rhi::pipeline_stage>{rhi::pipeline_stage::top_of_pipe};
                }
            }
            return stages;
        }
    } // namespace

    auto barrier_solver::solve(const compiled_dag& dag, span<const pass_node> all_passes,
                               const transient_allocator& allocator,
                               span<const registered_texture> registered_textures) -> solved_synchronization
    {
        auto result = solved_synchronization{};
        auto last_known_textures = flat_unordered_map<uint64_t, texture_state_record>{};
        auto last_known_buffers = flat_unordered_map<uint64_t, buffer_state_record>{};

        for (const auto pass_idx : dag.sorted_pass_indices)
        {
            if (pass_idx >= all_passes.size())
            {
                continue;
            }

            const auto& pass = all_passes[pass_idx];
            auto plan = pass_sync_plan{
                .pass_index = pass_idx,
                .queue = pass.queue,
                .texture_barriers = {},
                .buffer_barriers = {},
            };

            // 1. Texture Barriers
            for (const auto& access : pass.texture_accesses)
            {
                const auto* alloc = allocator.get_texture(access.texture.id);
                if (alloc == nullptr || alloc->handle.handle == 0)
                {
                    continue;
                }

                const auto handle_val = alloc->handle.handle;
                const auto dst_stages = access.stages;
                const auto dst_access = access.access;
                const auto dst_layout = access.layout;
                const auto dst_queue = pass.queue;

                const auto it = last_known_textures.find(handle_val);
                if (it == last_known_textures.end())
                {
                    // First time texture is accessed in this DAG execution
                    auto src_layout = rhi::image_layout::undefined;
                    auto src_stages = enum_mask<rhi::pipeline_stage>{rhi::pipeline_stage::top_of_pipe};
                    auto src_access = rhi::resource_access::none;

                    const auto persistent_it = _persistent_texture_states.find(handle_val);
                    if (persistent_it != _persistent_texture_states.end())
                    {
                        src_layout = persistent_it->second.layout;
                        src_stages = persistent_it->second.stages;
                        src_access = persistent_it->second.access;
                    }
                    else if (access.texture.id < registered_textures.size() &&
                             registered_textures[access.texture.id].is_imported)
                    {
                        src_layout = registered_textures[access.texture.id].initial_layout;
                    }

                    if (static_cast<bool>(dst_stages & rhi::pipeline_stage::attachment_output))
                    {
                        src_stages |= rhi::pipeline_stage::attachment_output;
                    }

                    const auto layout_changed = (src_layout != dst_layout);
                    const auto was_written = static_cast<bool>(src_access & rhi::resource_access::write);
                    const auto is_written = static_cast<bool>(dst_access & rhi::resource_access::write);

                    if (layout_changed || was_written || is_written)
                    {
                        plan.texture_barriers.push_back(rhi::texture_barrier{
                            .texture = alloc->handle,
                            .src = {
                                .stages = filter_stages_for_queue(src_stages, dst_queue),
                                .access = src_access,
                                .layout = src_layout,
                            },
                            .dst = {
                                .stages = filter_stages_for_queue(dst_stages, dst_queue),
                                .access = dst_access,
                                .layout = dst_layout,
                            },
                            .base_mip_level = access.subresource.base_mip_level,
                            .mip_level_count = access.subresource.mip_level_count,
                            .base_array_layer = access.subresource.base_array_layer,
                            .array_layer_count = access.subresource.array_layer_count,
                            .src_queue = nullptr,
                            .dst_queue = nullptr,
                        });
                    }

                    last_known_textures[handle_val] = texture_state_record{
                        .stages = dst_stages,
                        .access = dst_access,
                        .layout = dst_layout,
                        .queue = dst_queue,
                        .pass_index = pass_idx,
                    };
                }
                else
                {
                    auto& prev = it->second;
                    const auto layout_changed = (prev.layout != dst_layout);
                    const auto was_written = static_cast<bool>(prev.access & rhi::resource_access::write);
                    const auto is_written = static_cast<bool>(dst_access & rhi::resource_access::write);
                    const auto queue_changed = (prev.queue != dst_queue);

                    const auto need_barrier = layout_changed || was_written || is_written || queue_changed;

                    if (need_barrier)
                    {
                        plan.texture_barriers.push_back(rhi::texture_barrier{
                            .texture = alloc->handle,
                            .src = {
                                .stages = filter_stages_for_queue(prev.stages, dst_queue),
                                .access = prev.access,
                                .layout = prev.layout,
                            },
                            .dst = {
                                .stages = filter_stages_for_queue(dst_stages, dst_queue),
                                .access = dst_access,
                                .layout = dst_layout,
                            },
                            .base_mip_level = access.subresource.base_mip_level,
                            .mip_level_count = access.subresource.mip_level_count,
                            .base_array_layer = access.subresource.base_array_layer,
                            .array_layer_count = access.subresource.array_layer_count,
                            .src_queue = nullptr,
                            .dst_queue = nullptr,
                        });

                        prev = texture_state_record{
                            .stages = dst_stages,
                            .access = dst_access,
                            .layout = dst_layout,
                            .queue = dst_queue,
                            .pass_index = pass_idx,
                        };
                    }
                    else
                    {
                        // Accumulate read stages
                        prev.stages |= dst_stages;
                    }
                }
            }

            // 2. Buffer Barriers
            for (const auto& access : pass.buffer_accesses)
            {
                const auto* alloc = allocator.get_buffer(access.buffer.id);
                if (alloc == nullptr || alloc->handle.handle == 0)
                {
                    continue;
                }

                const auto handle_val = alloc->handle.handle;
                const auto dst_stages = access.stages;
                const auto dst_access = access.access;
                const auto dst_queue = pass.queue;

                const auto it = last_known_buffers.find(handle_val);
                if (it == last_known_buffers.end())
                {
                    const auto is_written = static_cast<bool>(dst_access & rhi::resource_access::write);
                    if (is_written)
                    {
                        plan.buffer_barriers.push_back(rhi::buffer_barrier{
                            .buffer = alloc->handle,
                            .src = {
                                .stages = rhi::pipeline_stage::top_of_pipe,
                                .access = rhi::resource_access::none,
                            },
                            .dst = {
                                .stages = filter_stages_for_queue(dst_stages, dst_queue),
                                .access = dst_access,
                            },
                            .offset = access.offset,
                            .size = access.size,
                            .src_queue = nullptr,
                            .dst_queue = nullptr,
                        });
                    }

                    last_known_buffers[handle_val] = buffer_state_record{
                        .stages = dst_stages,
                        .access = dst_access,
                        .queue = dst_queue,
                        .pass_index = pass_idx,
                    };
                }
                else
                {
                    auto& prev = it->second;
                    const auto was_written = static_cast<bool>(prev.access & rhi::resource_access::write);
                    const auto is_written = static_cast<bool>(dst_access & rhi::resource_access::write);
                    const auto queue_changed = (prev.queue != dst_queue);

                    const auto need_barrier = was_written || is_written || queue_changed;

                    if (need_barrier)
                    {
                        plan.buffer_barriers.push_back(rhi::buffer_barrier{
                            .buffer = alloc->handle,
                            .src = {
                                .stages = filter_stages_for_queue(prev.stages, dst_queue),
                                .access = prev.access,
                            },
                            .dst = {
                                .stages = filter_stages_for_queue(dst_stages, dst_queue),
                                .access = dst_access,
                            },
                            .offset = access.offset,
                            .size = access.size,
                            .src_queue = nullptr,
                            .dst_queue = nullptr,
                        });

                        prev = buffer_state_record{
                            .stages = dst_stages,
                            .access = dst_access,
                            .queue = dst_queue,
                            .pass_index = pass_idx,
                        };
                    }
                    else
                    {
                        prev.stages |= dst_stages;
                    }
                }
            }

            result.pass_plans.push_back(tempest::move(plan));
        }

        // 3. Form contiguous queue execution batches
        for (const auto pass_idx : dag.sorted_pass_indices)
        {
            if (pass_idx >= all_passes.size())
            {
                continue;
            }

            const auto& pass = all_passes[pass_idx];
            if (result.queue_batches.empty() || result.queue_batches.back().queue != pass.queue)
            {
                result.queue_batches.push_back(queue_sync_batch{
                    .queue = pass.queue,
                    .pass_indices = vector<uint32_t>{init_list, pass_idx},
                });
            }
            else
            {
                result.queue_batches.back().pass_indices.push_back(pass_idx);
            }
        }

        for (const auto& [handle, record] : last_known_textures)
        {
            _persistent_texture_states[handle] = record;
        }

        return result;
    }
} // namespace tempest::render_graph
