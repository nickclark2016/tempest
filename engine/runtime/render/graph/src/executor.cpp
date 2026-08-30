#include <tempest/render_graph/executor.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_graph/temporal_texture.hpp>

#include <format>

namespace tempest::render_graph
{
    auto render_graph_executor::execute(rhi::device& dev, render_graph& graph, const frame_sync_options& frame_sync)
        -> expected<void, execution_error>
    {
        const auto compile_res = graph.compile();
        if (!compile_res.has_value())
        {
            return unexpected(execution_error::compile_failed);
        }

        const auto& dag = compile_res.value();
        if (dag.sorted_pass_indices.empty())
        {
            return {};
        }

        auto& allocator = graph.get_allocator();
        const auto reg_textures = graph.get_compiler().get_registered_textures();
        const auto reg_buffers = graph.get_compiler().get_registered_buffers();

        allocator.allocate(dev, dag, reg_textures, reg_buffers, graph.get_surface_width(), graph.get_surface_height());

        const auto all_passes = graph.get_compiler().get_passes();
        const auto sync = _barrier_solver.solve(dag, all_passes, allocator, reg_textures);

        auto plan_map = flat_unordered_map<uint32_t, const pass_sync_plan*>{};
        for (const auto& plan : sync.pass_plans)
        {
            plan_map[plan.pass_index] = &plan;
        }

        auto ctx = pass_execution_context{&graph};

        if (_timeline_semaphore.handle == 0)
        {
            _timeline_semaphore = dev.create_timeline_semaphore();
        }

        auto wait_semaphore_consumed = false;
        auto last_batch_signal_val = uint64_t{0};

        for (size_t batch_idx = 0; batch_idx < sync.queue_batches.size(); ++batch_idx)
        {
            const auto& batch = sync.queue_batches[batch_idx];
            const auto is_last_batch = (batch_idx + 1 == sync.queue_batches.size());
            auto& port = get_execution_port(dev, batch.queue);
            auto& cmd = port.acquire_command_list(0, rhi::command_list_lifetime::transient);

#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
            auto batch_name = string{};
            const auto* queue_name = batch.queue == queue_type::graphics        ? "Graphics"
                                     : batch.queue == queue_type::async_compute ? "Async Compute"
                                                                                : "Async Transfer";
            std::format_to(tempest::back_inserter(batch_name), "Queue Batch {} ({})", batch_idx, queue_name);
            port.begin_debug_region(rhi::debug_label{.name = batch_name.c_str()});
#endif

            cmd.begin();

            for (const auto pass_idx : batch.pass_indices)
            {
                if (pass_idx >= all_passes.size())
                {
                    continue;
                }

                const auto& pass = all_passes[pass_idx];

#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
                for (const auto& access : pass.texture_accesses)
                {
                    if (access.texture.id < reg_textures.size())
                    {
                        const auto& reg_tex = reg_textures[access.texture.id];
                        if (!reg_tex.desc.name.empty())
                        {
                            const auto* alloc = allocator.get_texture(access.texture.id);
                            if (alloc != nullptr)
                            {
                                dev.set_debug_name(alloc->handle, reg_tex.desc.name);
                            }
                        }
                    }
                }
                for (const auto& access : pass.buffer_accesses)
                {
                    if (access.buffer.id < reg_buffers.size())
                    {
                        const auto& reg_buf = reg_buffers[access.buffer.id];
                        if (!reg_buf.desc.name.empty())
                        {
                            const auto* alloc = allocator.get_buffer(access.buffer.id);
                            if (alloc != nullptr)
                            {
                                dev.set_debug_name(alloc->handle, reg_buf.desc.name);
                            }
                        }
                    }
                }

                cmd.begin_debug_region(rhi::debug_label{.name = pass.name.c_str()});
#endif

                // 1. Issue pre-pass pipeline barriers
                const auto plan_it = plan_map.find(pass_idx);
                if (plan_it != plan_map.end() && plan_it->second != nullptr)
                {
                    const auto* plan = plan_it->second;
                    if (!plan->texture_barriers.empty() || !plan->buffer_barriers.empty())
                    {
                        cmd.pipeline_barrier(plan->texture_barriers, plan->buffer_barriers);
                    }
                }

                // 2. Detect color and depth-stencil attachments for dynamic rendering
                auto color_attachments = vector<rhi::color_attachment>{};
                auto depth_attachment = optional<rhi::depth_stencil_attachment>{nullopt};
                uint32_t render_width = 0;
                uint32_t render_height = 0;

                for (const auto& access : pass.texture_accesses)
                {
                    if (access.attachment == attachment_type::color)
                    {
                        const auto* alloc = allocator.get_texture(access.texture.id);
                        if (alloc != nullptr)
                        {
                            color_attachments.push_back(rhi::color_attachment{
                                .view = alloc->default_view,
                                .load_op = access.load_op,
                                .store_op = access.store_op,
                                .clear_value = access.clear_color,
                            });
                            render_width = alloc->size.width;
                            render_height = alloc->size.height;
                        }
                    }
                    else if (access.attachment == attachment_type::depth_stencil)
                    {
                        const auto* alloc = allocator.get_texture(access.texture.id);
                        if (alloc != nullptr)
                        {
                            depth_attachment = rhi::depth_stencil_attachment{
                                .view = alloc->default_view,
                                .depth_load_op = access.load_op,
                                .depth_store_op = access.store_op,
                                .stencil_load_op = rhi::load_op::dont_care,
                                .stencil_store_op = rhi::store_op::dont_care,
                                .clear_value = access.clear_depth_stencil,
                            };
                            if (render_width == 0)
                            {
                                render_width = alloc->size.width;
                                render_height = alloc->size.height;
                            }
                        }
                    }
                }

                const auto is_render_pass = !color_attachments.empty() || depth_attachment.has_value();

                if (is_render_pass)
                {
                    cmd.begin_render_pass(color_attachments, depth_attachment, render_width, render_height);
                    cmd.set_viewport(0.0F, 0.0F, static_cast<float>(render_width), static_cast<float>(render_height),
                                     0.0F, 1.0F);
                    cmd.set_scissor(0, 0, render_width, render_height);

                    if (pass.execute_fn)
                    {
                        pass.execute_fn(ctx, cmd);
                    }
                    cmd.end_render_pass();
                }
                else
                {
                    if (pass.execute_fn)
                    {
                        pass.execute_fn(ctx, cmd);
                    }
                }

#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
                cmd.end_debug_region();
#endif
            }

            // 3. If this is the final presenting batch and an explicitly presented texture is provided, transition it
            // to present layout
            if (is_last_batch && frame_sync.signal_semaphore.has_value() && frame_sync.presented_texture.has_value())
            {
                const auto tex_handle = *frame_sync.presented_texture;
                const auto present_barrier = rhi::texture_barrier{
                    .texture = tex_handle,
                    .src =
                        {
                            .stages = rhi::pipeline_stage::attachment_output,
                            .access = rhi::resource_access::write,
                            .layout = rhi::image_layout::general,
                        },
                    .dst =
                        {
                            .stages = rhi::pipeline_stage::bottom_of_pipe,
                            .access = rhi::resource_access::none,
                            .layout = rhi::image_layout::present,
                        },
                };
                cmd.pipeline_barrier(span<const rhi::texture_barrier>{&present_barrier, 1}, {});
                _barrier_solver.set_texture_state(tex_handle.handle, rhi::pipeline_stage::bottom_of_pipe,
                                                  rhi::resource_access::none, rhi::image_layout::present,
                                                  queue_type::graphics);
            }

            cmd.end();

            auto wait_sync = vector<rhi::device_sync_point>{};
            auto signal_sync = vector<rhi::device_sync_point>{};

            // Cross-queue timeline wait from previous batch
            if (batch_idx > 0 && last_batch_signal_val > 0)
            {
                auto wait_stages = rhi::pipeline_stage::top_of_pipe;
                if (batch.queue == queue_type::async_compute)
                {
                    wait_stages = rhi::pipeline_stage::compute;
                }
                else if (batch.queue == queue_type::async_transfer)
                {
                    wait_stages = rhi::pipeline_stage::all_transfer;
                }

                wait_sync.push_back(rhi::device_sync_point{
                    .semaphore = _timeline_semaphore,
                    .value = last_batch_signal_val,
                    .stages = wait_stages,
                });
            }

            // Frame acquire binary semaphore wait (on the first graphics batch or first batch)
            if (!wait_semaphore_consumed && frame_sync.wait_semaphore.has_value())
            {
                if (batch.queue == queue_type::graphics || is_last_batch)
                {
                    wait_sync.push_back(rhi::device_sync_point{
                        .semaphore = *frame_sync.wait_semaphore,
                        .value = 0,
                        .stages = frame_sync.wait_stages,
                    });
                    wait_semaphore_consumed = true;
                }
            }

            // Cross-queue timeline signal for next batch
            if (!is_last_batch)
            {
                last_batch_signal_val = ++_current_timeline_value;
                signal_sync.push_back(rhi::device_sync_point{
                    .semaphore = _timeline_semaphore,
                    .value = last_batch_signal_val,
                    .stages = rhi::pipeline_stage::bottom_of_pipe,
                });
            }

            // Frame render binary semaphore signal (on the final presenting batch)
            if (is_last_batch && frame_sync.signal_semaphore.has_value())
            {
                signal_sync.push_back(rhi::device_sync_point{
                    .semaphore = *frame_sync.signal_semaphore,
                    .value = 0,
                    .stages = frame_sync.signal_stages,
                });
            }

            // Frame timeline semaphore signal (for host in-flight slot synchronization)
            if (is_last_batch && frame_sync.timeline_semaphore.has_value() && frame_sync.timeline_value > 0)
            {
                signal_sync.push_back(rhi::device_sync_point{
                    .semaphore = *frame_sync.timeline_semaphore,
                    .value = frame_sync.timeline_value,
                    .stages = rhi::pipeline_stage::bottom_of_pipe,
                });
            }

            const rhi::command_list* cmds[] = {&cmd};
            const auto submit_res = port.submit(span<const rhi::command_list*>{cmds}, wait_sync, signal_sync);

#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
            port.end_debug_region();
#endif

            if (!submit_res.has_value())
            {
                return unexpected(execution_error::queue_submit_failed);
            }
        }

        for (auto* temporal_res : graph.get_tracked_temporal_resources())
        {
            if (temporal_res != nullptr)
            {
                temporal_res->swap();
            }
        }

        return {};
    }

    void render_graph_executor::release(rhi::device& dev)
    {
        if (_timeline_semaphore.handle != 0)
        {
            dev.destroy_semaphore(_timeline_semaphore);
            _timeline_semaphore = {};
        }
        _current_timeline_value = 0;
        _barrier_solver.clear_persistent_states();
    }

    auto render_graph_executor::get_execution_port(rhi::device& dev, queue_type queue) -> rhi::execution_port&
    {
        switch (queue)
        {
        case queue_type::graphics:
            return dev.get_graphics_execution_port();
        case queue_type::async_compute:
            return dev.get_async_compute_execution_port();
        case queue_type::async_transfer:
            return dev.get_async_transfer_execution_port();
        default:
            return dev.get_graphics_execution_port();
        }
    }
} // namespace tempest::render_graph
