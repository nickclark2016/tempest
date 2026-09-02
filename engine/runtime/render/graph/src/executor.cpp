#include <tempest/algorithm.hpp>
#include <tempest/bit.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/render_graph/executor.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_graph/temporal_texture.hpp>

#include <algorithm>
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

        const auto flight_slot = frame_sync.flight_slot_index;
        const auto fif = tempest::max(frame_sync.frames_in_flight, 1U);
        if (_flight_query_rings.size() < fif)
        {
            _flight_query_rings.resize(fif);
        }
        auto& flight_state = _flight_query_rings[flight_slot];

        // 1. Query readback from previous frame on this flight slot
        if (!flight_state.recorded_passes.empty() && flight_state.recorded_timestamp_count > 0)
        {
            auto ts_results = vector<uint64_t>(flight_state.recorded_timestamp_count, 0);
            auto read_ts_ok = false;
            if (flight_state.timestamp_pool.handle != 0)
            {
                read_ts_ok =
                    dev.get_query_pool_results(flight_state.timestamp_pool, 0, flight_state.recorded_timestamp_count,
                                               span<uint64_t>{ts_results.data(), ts_results.size()}, true);
            }

            auto ps_results_by_queue = flat_unordered_map<queue_type, vector<uint64_t>>{};
            auto read_ps_ok_by_queue = flat_unordered_map<queue_type, bool>{};
            auto num_ps_entries_by_queue = flat_unordered_map<queue_type, size_t>{};

            for (size_t q_idx = 0; q_idx < flight_state.queue_stats.size(); ++q_idx)
            {
                const auto q_type = static_cast<queue_type>(q_idx);
                auto& q_stats = flight_state.queue_stats[q_idx];
                if (q_stats.pool.handle != 0 && q_stats.recorded_count > 0)
                {
                    const auto num_entries =
                        static_cast<size_t>(tempest::popcount(static_cast<uint32_t>(q_stats.mask.value())));
                    num_ps_entries_by_queue[q_type] = num_entries;
                    auto& q_results = ps_results_by_queue[q_type];
                    q_results.resize(q_stats.recorded_count * num_entries, 0);
                    read_ps_ok_by_queue[q_type] =
                        dev.get_query_pool_results(q_stats.pool, 0, q_stats.recorded_count,
                                                   span<uint64_t>{q_results.data(), q_results.size()}, true);
                }
            }

            if (read_ts_ok && frame_sync.profiler != nullptr && frame_sync.profiler->is_enabled())
            {
                auto zones_by_track = flat_unordered_map<uint64_t, vector<profiler::zone_record>>{};

                for (const auto& pass_rec : flight_state.recorded_passes)
                {
                    const auto start_ticks = ts_results[pass_rec.start_timestamp_idx];
                    const auto end_ticks = ts_results[pass_rec.end_timestamp_idx];
                    const auto start_ns = dev.convert_gpu_timestamp_to_cpu_ns(start_ticks);
                    auto end_ns = dev.convert_gpu_timestamp_to_cpu_ns(end_ticks);
                    if (end_ns < start_ns)
                    {
                        end_ns = start_ns;
                    }

                    auto z = profiler::zone_record{
                        .start_ns = start_ns,
                        .end_ns = end_ns,
                        .depth = pass_rec.depth,
                        .name = pass_rec.pass_name,
                        .location = {},
                        .task_id = flight_state.recorded_frame_index,
                        .metrics = {},
                    };

                    if (pass_rec.pipeline_stats_idx.has_value() && read_ps_ok_by_queue[pass_rec.queue] &&
                        num_ps_entries_by_queue[pass_rec.queue] > 0)
                    {
                        const auto& ps_results = ps_results_by_queue[pass_rec.queue];
                        const auto num_ps_entries_per_query = num_ps_entries_by_queue[pass_rec.queue];
                        const auto query_offset = (*pass_rec.pipeline_stats_idx) * num_ps_entries_per_query;
                        auto entry_idx = size_t{0};
                        const auto pool_flags = flight_state.queue_stats[static_cast<size_t>(pass_rec.queue)].mask;

                        auto check_flag = [&](rhi::pipeline_statistic_flags flag, string_view metric_name) {
                            if (static_cast<bool>(pool_flags & flag))
                            {
                                if (query_offset + entry_idx < ps_results.size())
                                {
                                    if (static_cast<bool>(pass_rec.pipeline_stats_flags & flag))
                                    {
                                        const auto val = static_cast<double>(ps_results[query_offset + entry_idx]);
                                        z.metrics.push_back(profiler::metric_record{
                                            .name = metric_name,
                                            .value = val,
                                            .unit = profiler::metric_unit::count,
                                        });
                                    }
                                }
                                ++entry_idx;
                            }
                        };

                        check_flag(rhi::pipeline_statistic_flags::input_assembly_vertices, "Input Assembly Vertices");
                        check_flag(rhi::pipeline_statistic_flags::input_assembly_primitives,
                                   "Input Assembly Primitives");
                        check_flag(rhi::pipeline_statistic_flags::vertex_shader_invocations,
                                   "Vertex Shader Invocations");
                        check_flag(rhi::pipeline_statistic_flags::geometry_shader_invocations,
                                   "Geometry Shader Invocations");
                        check_flag(rhi::pipeline_statistic_flags::geometry_shader_primitives,
                                   "Geometry Shader Primitives");
                        check_flag(rhi::pipeline_statistic_flags::clipping_input_primitives,
                                   "Clipping Input Primitives");
                        check_flag(rhi::pipeline_statistic_flags::clipping_output_primitives,
                                   "Clipping Output Primitives");
                        check_flag(rhi::pipeline_statistic_flags::fragment_shader_invocations,
                                   "Fragment Shader Invocations");
                        check_flag(rhi::pipeline_statistic_flags::tessellation_control_shader_patches,
                                   "Tessellation Control Shader Patches");
                        check_flag(rhi::pipeline_statistic_flags::tessellation_evaluation_shader_invocations,
                                   "Tessellation Evaluation Shader Invocations");
                        check_flag(rhi::pipeline_statistic_flags::compute_shader_invocations,
                                   "Compute Shader Invocations");
                    }

                    const auto track_id = get_queue_track_id(pass_rec.queue);
                    zones_by_track[track_id].push_back(tempest::move(z));
                }

                for (auto& [track_id, zones] : zones_by_track)
                {
                    const auto q_type = static_cast<queue_type>((track_id & 0x7FFF'FFFFULL) - 1);
                    frame_sync.profiler->register_track(track_id, get_queue_track_name(q_type));

                    // Sort zones so submit zones (depth 0) come before their nested child passes
                    std::sort(zones.begin(), zones.end(),
                              [](const profiler::zone_record& a, const profiler::zone_record& b) {
                                  if (a.start_ns != b.start_ns)
                                  {
                                      return a.start_ns < b.start_ns;
                                  }
                                  return a.depth < b.depth;
                              });

                    auto chunk = frame_sync.profiler->acquire_chunk();
                    chunk->set_thread_id(track_id);
                    for (const auto& z : zones)
                    {
                        if (!chunk->add_zone(z))
                        {
                            frame_sync.profiler->push_completed_chunk(tempest::move(chunk));
                            chunk = frame_sync.profiler->acquire_chunk();
                            chunk->set_thread_id(track_id);
                            chunk->add_zone(z);
                        }
                    }
                    frame_sync.profiler->push_completed_chunk(tempest::move(chunk));
                }
            }

            flight_state.recorded_passes.clear();
            flight_state.recorded_timestamp_count = 0;
            for (auto& q_stats : flight_state.queue_stats)
            {
                q_stats.recorded_count = 0;
            }
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

        // 2. Count passes and calculate required query pool capacities for current frame
        auto active_pass_count = size_t{0};
        auto needed_stats_per_queue = flat_unordered_map<queue_type, uint32_t>{};
        auto union_stats_per_queue = flat_unordered_map<queue_type, enum_mask<rhi::pipeline_statistic_flags>>{};

        for (const auto& batch : sync.queue_batches)
        {
            for (const auto pass_idx : batch.pass_indices)
            {
                if (pass_idx < all_passes.size())
                {
                    ++active_pass_count;
                    const auto& pass = all_passes[pass_idx];
                    if (pass.pipeline_statistics != rhi::pipeline_statistic_flags::none)
                    {
                        needed_stats_per_queue[batch.queue]++;
                        auto pass_stats = pass.pipeline_statistics;
                        if (batch.queue == queue_type::async_compute)
                        {
                            pass_stats &= rhi::pipeline_statistic_flags::compute_shader_invocations;
                        }
                        union_stats_per_queue[batch.queue] |= pass_stats;
                    }
                }
            }
        }

        const auto needed_timestamp_count =
            static_cast<uint32_t>((sync.queue_batches.size() * 2) + (active_pass_count * 2));
        if (needed_timestamp_count > 0)
        {
            if (flight_state.timestamp_pool.handle == 0 || flight_state.timestamp_count < needed_timestamp_count)
            {
                if (flight_state.timestamp_pool.handle != 0)
                {
                    dev.destroy_query_pool(flight_state.timestamp_pool);
                    flight_state.timestamp_pool = {};
                }
                const auto ts_cap = tempest::max(needed_timestamp_count, 64U);
                flight_state.timestamp_pool = dev.create_query_pool(rhi::query_pool_desc{
                    .type = rhi::query_type::timestamp,
                    .query_count = ts_cap,
                });
                flight_state.timestamp_count = ts_cap;
            }
        }

        for (const auto& [q_type, needed_count] : needed_stats_per_queue)
        {
            auto q_mask = union_stats_per_queue[q_type];
            if (needed_count > 0 && q_mask != rhi::pipeline_statistic_flags::none)
            {
                auto& q_state = flight_state.queue_stats[static_cast<size_t>(q_type)];
                if (q_state.pool.handle == 0 || q_state.capacity < needed_count || q_state.mask != q_mask)
                {
                    if (q_state.pool.handle != 0)
                    {
                        dev.destroy_query_pool(q_state.pool);
                        q_state.pool = {};
                    }
                    const auto ps_cap = tempest::max(needed_count, 16U);
                    q_state.pool = dev.create_query_pool(rhi::query_pool_desc{
                        .type = rhi::query_type::pipeline_statistics,
                        .query_count = ps_cap,
                        .pipeline_statistics = q_mask,
                    });
                    q_state.capacity = ps_cap;
                    q_state.mask = q_mask;
                }
                q_state.recorded_count = 0;
            }
        }

        auto ctx = pass_execution_context{&graph};

        if (_timeline_semaphore.handle == 0)
        {
            _timeline_semaphore = dev.create_timeline_semaphore();
        }

        auto wait_semaphore_consumed = false;
        auto last_batch_signal_val = uint64_t{0};
        auto current_timestamp_idx = uint32_t{0};
        auto current_stats_idx_per_queue = flat_unordered_map<queue_type, uint32_t>{};

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

            // Reset query pool slice used by this batch (including submit start/end and pass queries)
            const auto batch_ts_start = current_timestamp_idx;
            const auto batch_ps_start = current_stats_idx_per_queue[batch.queue];
            auto batch_ts_count = uint32_t{2}; // 2 timestamps for submit start and end
            auto batch_ps_count = uint32_t{0};

            for (const auto pass_idx : batch.pass_indices)
            {
                if (pass_idx < all_passes.size())
                {
                    batch_ts_count += 2;
                    if (all_passes[pass_idx].pipeline_statistics != rhi::pipeline_statistic_flags::none)
                    {
                        ++batch_ps_count;
                    }
                }
            }

            if (flight_state.timestamp_pool.handle != 0 && batch_ts_count > 0)
            {
                cmd.reset_query_pool(flight_state.timestamp_pool, batch_ts_start, batch_ts_count);
            }
            auto& q_state = flight_state.queue_stats[static_cast<size_t>(batch.queue)];
            if (q_state.pool.handle != 0 && batch_ps_count > 0)
            {
                cmd.reset_query_pool(q_state.pool, batch_ps_start, batch_ps_count);
            }

            // Top-level submit zone start timestamp
            const auto submit_start_ts = current_timestamp_idx++;
            if (flight_state.timestamp_pool.handle != 0)
            {
                cmd.write_timestamp(flight_state.timestamp_pool, submit_start_ts, rhi::pipeline_stage::bottom_of_pipe);
            }

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

                // Pass query setup
                const auto start_ts = current_timestamp_idx++;
                const auto end_ts = current_timestamp_idx++;
                auto pass_stat_idx = optional<uint32_t>{nullopt};
                const auto has_stats =
                    (pass.pipeline_statistics != rhi::pipeline_statistic_flags::none) && (q_state.pool.handle != 0);
                if (has_stats)
                {
                    pass_stat_idx = current_stats_idx_per_queue[batch.queue]++;
                }

                // 1. Issue pre-pass pipeline barriers FIRST before the start timestamp query
                const auto plan_it = plan_map.find(pass_idx);
                if (plan_it != plan_map.end() && plan_it->second != nullptr)
                {
                    const auto* plan = plan_it->second;
                    if (!plan->texture_barriers.empty() || !plan->buffer_barriers.empty())
                    {
                        cmd.pipeline_barrier(plan->texture_barriers, plan->buffer_barriers);
                    }
                }

                if (flight_state.timestamp_pool.handle != 0)
                {
                    cmd.write_timestamp(flight_state.timestamp_pool, start_ts, rhi::pipeline_stage::bottom_of_pipe);
                }
                if (has_stats)
                {
                    cmd.begin_query(q_state.pool, *pass_stat_idx);
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

                if (has_stats)
                {
                    cmd.end_query(q_state.pool, *pass_stat_idx);
                    q_state.recorded_count = tempest::max(q_state.recorded_count, *pass_stat_idx + 1);
                }
                if (flight_state.timestamp_pool.handle != 0)
                {
                    cmd.write_timestamp(flight_state.timestamp_pool, end_ts, rhi::pipeline_stage::bottom_of_pipe);
                }

                flight_state.recorded_passes.push_back(flight_query_state::pass_query_binding{
                    .pass_name = pass.name,
                    .queue = batch.queue,
                    .start_timestamp_idx = start_ts,
                    .end_timestamp_idx = end_ts,
                    .depth = 1,
                    .pipeline_stats_idx = pass_stat_idx,
                    .pipeline_stats_flags = pass.pipeline_statistics,
                });

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

            // Top-level submit zone end timestamp
            const auto submit_end_ts = current_timestamp_idx++;
            if (flight_state.timestamp_pool.handle != 0)
            {
                cmd.write_timestamp(flight_state.timestamp_pool, submit_end_ts, rhi::pipeline_stage::bottom_of_pipe);
            }

            auto submit_name = string{};
            switch (batch.queue)
            {
            case queue_type::graphics:
                submit_name = "Graphics Submit";
                break;
            case queue_type::async_compute:
                submit_name = "Async Compute Submit";
                break;
            case queue_type::async_transfer:
                submit_name = "Async Transfer Submit";
                break;
            default:
                submit_name = "Queue Submit";
                break;
            }

            flight_state.recorded_passes.push_back(flight_query_state::pass_query_binding{
                .pass_name = tempest::move(submit_name),
                .queue = batch.queue,
                .start_timestamp_idx = submit_start_ts,
                .end_timestamp_idx = submit_end_ts,
                .depth = 0,
                .pipeline_stats_idx = nullopt,
                .pipeline_stats_flags = rhi::pipeline_statistic_flags::none,
            });

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

        flight_state.recorded_timestamp_count = current_timestamp_idx;
        flight_state.recorded_frame_index = frame_sync.frame_index;

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

        for (auto& slot : _flight_query_rings)
        {
            if (slot.timestamp_pool.handle != 0)
            {
                dev.destroy_query_pool(slot.timestamp_pool);
                slot.timestamp_pool = {};
            }
            for (auto& q_state : slot.queue_stats)
            {
                if (q_state.pool.handle != 0)
                {
                    dev.destroy_query_pool(q_state.pool);
                    q_state.pool = {};
                }
            }
            slot.recorded_passes.clear();
        }
        _flight_query_rings.clear();

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
