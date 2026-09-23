#include <tempest/algorithm.hpp>
#include <tempest/bit.hpp>
#include <tempest/checked.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/render_graph/executor.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_graph/temporal_texture.hpp>

#include <tempest/format.hpp>

namespace tempest::render_graph
{
    namespace
    {
        auto determine_batch_wait_stages(const queue_sync_batch& batch) -> enum_mask<rhi::pipeline_stage>
        {
            switch (batch.queue)
            {
            case queue_type::graphics:
                return rhi::pipeline_stage::all_graphics;
            case queue_type::async_compute:
                return rhi::pipeline_stage::compute;
            case queue_type::async_transfer:
                return rhi::pipeline_stage::all_transfer;
            }
            return rhi::pipeline_stage::all_graphics;
        }

        auto determine_batch_signal_stages(queue_type queue) -> enum_mask<rhi::pipeline_stage>
        {
            switch (queue)
            {
            case queue_type::graphics:
                return rhi::pipeline_stage::all_graphics;
            case queue_type::async_compute:
                return rhi::pipeline_stage::compute;
            case queue_type::async_transfer:
                return rhi::pipeline_stage::all_transfer;
            }
            return rhi::pipeline_stage::all_graphics;
        }

        struct presentation_sync_info
        {
            enum_mask<rhi::pipeline_stage> write_stages{rhi::pipeline_stage::attachment_output};
            enum_mask<rhi::pipeline_stage> signal_stages{rhi::pipeline_stage::attachment_output};
        };

        auto determine_presentation_sync_info(const queue_sync_batch& batch, const frame_sync_options& frame_sync,
                                              span<const pass_node> all_passes, const transient_allocator& allocator)
            -> presentation_sync_info
        {
            auto detected_write_stages = optional<enum_mask<rhi::pipeline_stage>>{nullopt};

            if (frame_sync.presented_texture.has_value())
            {
                const auto presented_handle = *frame_sync.presented_texture;
                for (const auto pass_idx : batch.pass_indices)
                {
                    if (pass_idx < all_passes.size())
                    {
                        for (const auto& access : all_passes[pass_idx].texture_accesses)
                        {
                            const auto* alloc = allocator.get_texture(access.texture.id);
                            if (alloc != nullptr && alloc->handle.handle == presented_handle.handle)
                            {
                                if (static_cast<bool>(access.access & rhi::resource_access::write))
                                {
                                    detected_write_stages = access.stages;
                                }
                            }
                        }
                    }
                }
            }

            const auto fallback_write_stage = (batch.queue == queue_type::async_compute)
                                                  ? rhi::pipeline_stage::compute
                                                  : (batch.queue == queue_type::async_transfer)
                                                        ? rhi::pipeline_stage::all_transfer
                                                        : rhi::pipeline_stage::attachment_output;

            const auto effective_write_stages = detected_write_stages.value_or(fallback_write_stage);

            const auto effective_signal_stages = frame_sync.signal_stages.has_value()
                                                     ? enum_mask<rhi::pipeline_stage>{*frame_sync.signal_stages}
                                                     : effective_write_stages;

            return presentation_sync_info{
                .write_stages = effective_write_stages,
                .signal_stages = effective_signal_stages,
            };
        }

        struct pass_query_allocation
        {
            uint32_t start_ts{0};
            uint32_t end_ts{0};
            optional<uint32_t> stat_idx{nullopt};
        };

        struct pass_recorder
        {
            non_null<rhi::execution_port> port;
            non_null<const pass_node> pass;
            const pass_sync_plan* plan{nullptr};
            pass_query_allocation alloc{};
            rhi::query_pool_handle ts_pool{};
            rhi::query_pool_handle stat_pool{};
            non_null<pass_execution_context> ctx;
            non_null<const transient_allocator> trans_alloc;
            non_null<const rhi::command_list*> out_cmd;
            uint32_t thread_id{0};

            void operator()() const
            {
                auto& pass_cmd = port->acquire_command_list(thread_id, rhi::command_list_lifetime::transient);
                pass_cmd.begin();

#ifdef TEMPEST_ENABLE_DEBUG_MARKERS
                pass_cmd.begin_debug_region(rhi::debug_label{.name = pass->name.c_str()});
#endif

                // 1. Issue pre-pass pipeline barriers FIRST before the start timestamp query
                if (plan != nullptr)
                {
                    if (!plan->texture_barriers.empty() || !plan->buffer_barriers.empty())
                    {
                        pass_cmd.pipeline_barrier(plan->texture_barriers, plan->buffer_barriers);
                    }
                }

                // 2. Pass start timestamp & begin stats query
                if (ts_pool.handle != 0)
                {
                    pass_cmd.write_timestamp(ts_pool, alloc.start_ts, rhi::pipeline_stage::bottom_of_pipe);
                }
                if (alloc.stat_idx.has_value() && stat_pool.handle != 0)
                {
                    pass_cmd.begin_query(stat_pool, *alloc.stat_idx);
                }

                // 3. Detect color and depth-stencil attachments for dynamic rendering
                auto color_attachments = vector<rhi::color_attachment>{};
                auto depth_attachment = optional<rhi::depth_stencil_attachment>{nullopt};
                auto render_width = uint32_t{0};
                auto render_height = uint32_t{0};

                for (const auto& access : pass->texture_accesses)
                {
                    if (access.attachment == attachment_type::color)
                    {
                        const auto* tex_alloc = trans_alloc->get_texture(access.texture.id);
                        if (tex_alloc != nullptr)
                        {
                            color_attachments.push_back(rhi::color_attachment{
                                .view = tex_alloc->default_view,
                                .load_op = access.load_op,
                                .store_op = access.store_op,
                                .clear_value = access.clear_color,
                            });
                            render_width = tex_alloc->size.width;
                            render_height = tex_alloc->size.height;
                        }
                    }
                    else if (access.attachment == attachment_type::depth_stencil)
                    {
                        const auto* tex_alloc = trans_alloc->get_texture(access.texture.id);
                        if (tex_alloc != nullptr)
                        {
                            depth_attachment = rhi::depth_stencil_attachment{
                                .view = tex_alloc->default_view,
                                .depth_load_op = access.load_op,
                                .depth_store_op = access.store_op,
                                .stencil_load_op = rhi::load_op::dont_care,
                                .stencil_store_op = rhi::store_op::dont_care,
                                .clear_value = access.clear_depth_stencil,
                            };
                            if (render_width == 0)
                            {
                                render_width = tex_alloc->size.width;
                                render_height = tex_alloc->size.height;
                            }
                        }
                    }
                }

                const auto is_render_pass = !color_attachments.empty() || depth_attachment.has_value();

                if (is_render_pass)
                {
                    pass_cmd.begin_render_pass(color_attachments, depth_attachment, render_width, render_height);
                    pass_cmd.set_viewport(0.0F, 0.0F, static_cast<float>(render_width),
                                          static_cast<float>(render_height), 0.0F, 1.0F);
                    pass_cmd.set_scissor(0, 0, render_width, render_height);

                    if (pass->execute_fn)
                    {
                        pass->execute_fn(*ctx.get(), pass_cmd);
                    }
                    pass_cmd.end_render_pass();
                }
                else
                {
                    if (pass->execute_fn)
                    {
                        pass->execute_fn(*ctx.get(), pass_cmd);
                    }
                }

                // 4. End stats query & write end timestamp
                if (alloc.stat_idx.has_value() && stat_pool.handle != 0)
                {
                    pass_cmd.end_query(stat_pool, *alloc.stat_idx);
                }
                if (ts_pool.handle != 0)
                {
                    pass_cmd.write_timestamp(ts_pool, alloc.end_ts, rhi::pipeline_stage::bottom_of_pipe);
                }

#ifdef TEMPEST_ENABLE_DEBUG_MARKERS
                pass_cmd.end_debug_region();
#endif

                pass_cmd.end();
                *out_cmd.get() = &pass_cmd;
            }
        };
    } // namespace

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
            return expected<void, execution_error>{};
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
                    end_ns = max(end_ns, start_ns);

                    auto z = profiler::zone_record{
                        .start_ns = start_ns,
                        .end_ns = end_ns,
                        .depth = pass_rec.depth,
                        .name = pass_rec.pass_name,
                        .location = {},
                        .task_id = 0,
                        .coroutine_id = 0,
                        .slice_index = 0,
                        .reason = profiler::suspend_reason::none,
                        .spawned_by_thread_id = 0,
                        .spawned_by_coroutine_id = 0,
                        .awaited_by_thread_id = 0,
                        .awaited_by_coroutine_id = 0,
                        .metrics = {},
                        .frame_index = flight_state.recorded_frame_index,
                    };

                    if (pass_rec.pipeline_stats_idx.has_value() && read_ps_ok_by_queue[pass_rec.queue] &&
                        num_ps_entries_by_queue[pass_rec.queue] > 0)
                    {
                        const auto& ps_results = ps_results_by_queue[pass_rec.queue];
                        const auto num_ps_entries_per_query = num_ps_entries_by_queue[pass_rec.queue];
                        const auto query_offset = (*pass_rec.pipeline_stats_idx) * num_ps_entries_per_query;
                        auto entry_idx = size_t{0};
                        const auto pool_flags = flight_state.queue_stats[static_cast<size_t>(pass_rec.queue)].mask;

                        auto check_flag = [&](rhi::pipeline_statistic_flags flag, string_view metric_name) -> void {
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
                    tempest::sort(zones.begin(), zones.end(),
                              [](const profiler::zone_record& a, const profiler::zone_record& b) -> bool {
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

        struct batch_query_allocation
        {
            uint32_t ts_start{0};
            uint32_t ts_count{0};
            uint32_t ps_start{0};
            uint32_t ps_count{0};
            uint32_t submit_start_ts{0};
            uint32_t submit_end_ts{0};
        };

        auto batch_allocations = vector<batch_query_allocation>{};
        batch_allocations.resize(sync.queue_batches.size());
        auto pass_allocations = flat_unordered_map<uint32_t, pass_query_allocation>{};

        auto current_ts_idx = uint32_t{0};
        auto current_stats_idx_per_queue = flat_unordered_map<queue_type, uint32_t>{};

        for (size_t batch_idx = 0; batch_idx < sync.queue_batches.size(); ++batch_idx)
        {
            const auto& batch = sync.queue_batches[batch_idx];
            auto& b_alloc = batch_allocations[batch_idx];
            b_alloc.ts_start = current_ts_idx;
            b_alloc.ps_start = current_stats_idx_per_queue[batch.queue];
            b_alloc.submit_start_ts = current_ts_idx++;

            for (const auto pass_idx : batch.pass_indices)
            {
                if (pass_idx < all_passes.size())
                {
                    auto p_alloc = pass_query_allocation{};
                    p_alloc.start_ts = current_ts_idx++;
                    p_alloc.end_ts = current_ts_idx++;

                    const auto& pass = all_passes[pass_idx];
                    auto& q_state = flight_state.queue_stats[static_cast<size_t>(batch.queue)];
                    const auto has_stats = (pass.pipeline_statistics != rhi::pipeline_statistic_flags::none) &&
                                           (q_state.pool.handle != 0);
                    if (has_stats)
                    {
                        const auto stat_idx = current_stats_idx_per_queue[batch.queue]++;
                        p_alloc.stat_idx = stat_idx;
                        b_alloc.ps_count++;
                        q_state.recorded_count = tempest::max(q_state.recorded_count, stat_idx + 1);
                    }
                    pass_allocations[pass_idx] = p_alloc;
                }
            }

            b_alloc.submit_end_ts = current_ts_idx++;
            b_alloc.ts_count = current_ts_idx - b_alloc.ts_start;
        }

        auto wait_semaphore_consumed = false;
        auto batch_signal_values = vector<uint64_t>(sync.queue_batches.size(), 0);

        for (size_t batch_idx = 0; batch_idx < sync.queue_batches.size(); ++batch_idx)
        {
            const auto& batch = sync.queue_batches[batch_idx];
            const auto is_last_batch = (batch_idx + 1 == sync.queue_batches.size());
            auto& port = get_execution_port(dev, batch.queue);
            auto batch_commands = vector<const rhi::command_list*>{};
            const auto& b_alloc = batch_allocations[batch_idx];

#ifdef TEMPEST_ENABLE_DEBUG_MARKERS
            auto batch_name = string{};
            const auto* queue_name = batch.queue == queue_type::graphics        ? "Graphics"
                                     : batch.queue == queue_type::async_compute ? "Async Compute"
                                                                                : "Async Transfer";
            format_to(tempest::back_inserter(batch_name), "Queue Batch {} ({})", batch_idx, queue_name);
            port.begin_debug_region(rhi::debug_label{.name = batch_name.c_str()});
#endif

            // A. Batch Prologue: Query pool resets and submit start timestamp
            auto& cmd_prologue = port.acquire_command_list(0, rhi::command_list_lifetime::transient);
            cmd_prologue.begin();

            if (flight_state.timestamp_pool.handle != 0 && b_alloc.ts_count > 0)
            {
                cmd_prologue.reset_query_pool(flight_state.timestamp_pool, b_alloc.ts_start, b_alloc.ts_count);
            }
            auto& q_state = flight_state.queue_stats[static_cast<size_t>(batch.queue)];
            if (q_state.pool.handle != 0 && b_alloc.ps_count > 0)
            {
                cmd_prologue.reset_query_pool(q_state.pool, b_alloc.ps_start, b_alloc.ps_count);
            }

            if (flight_state.timestamp_pool.handle != 0)
            {
                cmd_prologue.write_timestamp(flight_state.timestamp_pool, b_alloc.submit_start_ts,
                                             rhi::pipeline_stage::bottom_of_pipe);
            }

            cmd_prologue.end();
            batch_commands.push_back(&cmd_prologue);

            // B. Pass Recording
            const auto pass_count = batch.pass_indices.size();
            auto pass_commands = vector<const rhi::command_list*>(pass_count, nullptr);
            auto recorders = vector<pass_recorder>{};
            recorders.reserve(pass_count);

            for (size_t i = 0; i < pass_count; ++i)
            {
                const auto pass_idx = batch.pass_indices[i];
                if (pass_idx >= all_passes.size())
                {
                    continue;
                }

                const auto& pass = all_passes[pass_idx];
                const auto& p_alloc = pass_allocations[pass_idx];
                const auto plan_it = plan_map.find(pass_idx);
                const auto* plan = (plan_it != plan_map.end()) ? plan_it->second : nullptr;

                recorders.push_back(pass_recorder{
                    .port = &port,
                    .pass = &pass,
                    .plan = plan,
                    .alloc = p_alloc,
                    .ts_pool = flight_state.timestamp_pool,
                    .stat_pool = q_state.pool,
                    .ctx = &ctx,
                    .trans_alloc = &allocator,
                    .out_cmd = &pass_commands[i],
                    .thread_id = static_cast<uint32_t>(i + 1),
                });
            }

            auto pass_items = vector<pass_record_item>{};
            pass_items.reserve(recorders.size());
            for (auto& recorder : recorders)
            {
                pass_items.push_back(pass_record_item{
                    .record_fn = tempest::function_ref<void()>{recorder},
                    .pass_name = recorder.pass->name,
                });
            }

            if (frame_sync.pass_dispatcher.has_value() && pass_items.size() > 1)
            {
                (*frame_sync.pass_dispatcher)(span<const pass_record_item>{pass_items.data(), pass_items.size()});
            }
            else
            {
                for (const auto& item : pass_items)
                {
                    item.record_fn();
                }
            }

            for (const auto* cmd_ptr : pass_commands)
            {
                if (cmd_ptr != nullptr)
                {
                    batch_commands.push_back(cmd_ptr);
                }
            }

            // C. Batch Epilogue: Present layout transition and submit end timestamp
            auto& cmd_epilogue = port.acquire_command_list(0, rhi::command_list_lifetime::transient);
            cmd_epilogue.begin();

            if (is_last_batch && frame_sync.signal_semaphore.has_value() && frame_sync.presented_texture.has_value())
            {
                const auto pres_sync = determine_presentation_sync_info(batch, frame_sync, all_passes, allocator);
                const auto tex_handle = *frame_sync.presented_texture;
                const auto present_barrier = rhi::texture_barrier{
                    .texture = tex_handle,
                    .src =
                        {
                            .stages = pres_sync.write_stages,
                            .access = rhi::resource_access::write,
                            .layout = rhi::image_layout::general,
                        },
                    .dst =
                        {
                            .stages = pres_sync.signal_stages,
                            .access = rhi::resource_access::none,
                            .layout = rhi::image_layout::present,
                        },
                };
                cmd_epilogue.pipeline_barrier(span<const rhi::texture_barrier>{&present_barrier, 1}, {});
                _barrier_solver.set_texture_state(tex_handle.handle, pres_sync.signal_stages,
                                                  rhi::resource_access::none, rhi::image_layout::present,
                                                  queue_type::graphics);
            }

            if (flight_state.timestamp_pool.handle != 0)
            {
                cmd_epilogue.write_timestamp(flight_state.timestamp_pool, b_alloc.submit_end_ts,
                                             rhi::pipeline_stage::bottom_of_pipe);
            }

            cmd_epilogue.end();
            batch_commands.push_back(&cmd_epilogue);

            // D. Synchronizations and unified submission
            auto wait_sync = vector<rhi::device_sync_point>{};
            auto signal_sync = vector<rhi::device_sync_point>{};

            // Cross-queue timeline waits for batches this batch depends on
            auto max_wait_per_queue = flat_unordered_map<queue_type, uint64_t>{};
            for (const auto dep_batch_idx : batch.wait_batch_indices)
            {
                if (dep_batch_idx < sync.queue_batches.size())
                {
                    const auto& dep_batch = sync.queue_batches[dep_batch_idx];
                    const auto dep_val = batch_signal_values[dep_batch_idx];
                    if (dep_val > 0)
                    {
                        max_wait_per_queue[dep_batch.queue] =
                            tempest::max(max_wait_per_queue[dep_batch.queue], dep_val);
                    }
                }
            }

            for (const auto& [dep_queue, wait_val] : max_wait_per_queue)
            {
                const auto wait_stages = determine_batch_wait_stages(batch);

                auto sem_it = _queue_timeline_semaphores.find(dep_queue);
                if (sem_it != _queue_timeline_semaphores.end() && sem_it->second.handle != 0)
                {
                    wait_sync.push_back(rhi::device_sync_point{
                        .semaphore = sem_it->second,
                        .value = wait_val,
                        .stages = wait_stages,
                    });
                }
            }

            // Frame acquire binary semaphore wait (on the batch accessing presented_texture, or final batch)
            if (!wait_semaphore_consumed && frame_sync.wait_semaphore.has_value())
            {
                auto should_wait = false;
                if (frame_sync.presented_texture.has_value())
                {
                    const auto presented_handle = *frame_sync.presented_texture;
                    for (const auto pass_idx : batch.pass_indices)
                    {
                        if (pass_idx < all_passes.size())
                        {
                            for (const auto& access : all_passes[pass_idx].texture_accesses)
                            {
                                const auto* alloc = allocator.get_texture(access.texture.id);
                                if (alloc != nullptr && alloc->handle.handle == presented_handle.handle)
                                {
                                    should_wait = true;
                                    break;
                                }
                            }
                        }
                        if (should_wait)
                        {
                            break;
                        }
                    }
                    if (is_last_batch)
                    {
                        should_wait = true;
                    }
                }
                else
                {
                    should_wait = (batch.queue == queue_type::graphics || is_last_batch);
                }

                if (should_wait)
                {
                    wait_sync.push_back(rhi::device_sync_point{
                        .semaphore = *frame_sync.wait_semaphore,
                        .value = 0,
                        .stages = frame_sync.wait_stages,
                    });
                    wait_semaphore_consumed = true;
                }
            }

            // Cross-queue timeline signal for this batch
            auto& queue_sem = _queue_timeline_semaphores[batch.queue];
            if (queue_sem.handle == 0)
            {
                queue_sem = dev.create_timeline_semaphore();
            }
            const auto batch_signal_val = ++_queue_timeline_values[batch.queue];
            batch_signal_values[batch_idx] = batch_signal_val;

            const auto batch_signal_stages = determine_batch_signal_stages(batch.queue);
            signal_sync.push_back(rhi::device_sync_point{
                .semaphore = queue_sem,
                .value = batch_signal_val,
                .stages = batch_signal_stages,
            });

            // Frame render binary semaphore signal (on the final presenting batch)
            if (is_last_batch && frame_sync.signal_semaphore.has_value())
            {
                const auto pres_sync = determine_presentation_sync_info(batch, frame_sync, all_passes, allocator);
                signal_sync.push_back(rhi::device_sync_point{
                    .semaphore = *frame_sync.signal_semaphore,
                    .value = 0,
                    .stages = pres_sync.signal_stages,
                });
            }

            // Frame timeline semaphore signal (for host in-flight slot synchronization)
            if (is_last_batch && frame_sync.timeline_semaphore.has_value() && frame_sync.timeline_value > 0)
            {
                const auto slot_signal_stages = determine_batch_signal_stages(batch.queue);
                signal_sync.push_back(rhi::device_sync_point{
                    .semaphore = *frame_sync.timeline_semaphore,
                    .value = frame_sync.timeline_value,
                    .stages = slot_signal_stages,
                });
            }

            const auto submit_res = port.submit(
                span<const rhi::command_list*>{batch_commands.data(), batch_commands.size()}, wait_sync, signal_sync);

#ifdef TEMPEST_ENABLE_DEBUG_MARKERS
            port.end_debug_region();
#endif

            if (!submit_res.has_value())
            {
                return unexpected(execution_error::queue_submit_failed);
            }

            // Record pass query bindings into flight state
            for (const auto pass_idx : batch.pass_indices)
            {
                if (pass_idx < all_passes.size())
                {
                    const auto& pass = all_passes[pass_idx];
                    const auto& p_alloc = pass_allocations[pass_idx];
                    flight_state.recorded_passes.push_back(flight_query_state::pass_query_binding{
                        .pass_name = pass.name,
                        .queue = batch.queue,
                        .start_timestamp_idx = p_alloc.start_ts,
                        .end_timestamp_idx = p_alloc.end_ts,
                        .depth = 1,
                        .pipeline_stats_idx = p_alloc.stat_idx,
                        .pipeline_stats_flags = pass.pipeline_statistics,
                    });
                }
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
                .start_timestamp_idx = b_alloc.submit_start_ts,
                .end_timestamp_idx = b_alloc.submit_end_ts,
                .depth = 0,
                .pipeline_stats_idx = nullopt,
                .pipeline_stats_flags = rhi::pipeline_statistic_flags::none,
            });
        }

        flight_state.recorded_timestamp_count = current_ts_idx;
        flight_state.recorded_frame_index = frame_sync.frame_index;

        for (auto* temporal_res : graph.get_tracked_temporal_resources())
        {
            if (temporal_res != nullptr)
            {
                temporal_res->swap();
            }
        }

        return expected<void, execution_error>{};
    }

    void render_graph_executor::release(rhi::device& dev)
    {
        for (auto& [q, sem] : _queue_timeline_semaphores)
        {
            if (sem.handle != 0)
            {
                dev.destroy_semaphore(sem);
                sem = {};
            }
        }
        _queue_timeline_semaphores.clear();
        _queue_timeline_values.clear();

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
