#include <tempest/editor_engine_context.hpp>

#include <tempest/array.hpp>
#include <tempest/move.hpp>
#include <tempest/optional.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/ui.hpp>

#include <chrono>

namespace tempest::editor
{
    void editor_engine_context::register_on_editor_paint_callback(function<void(engine_context&)> callback)
    {
        _editor_callbacks.on_paint.push_back(tempest::move(callback));
    }

    void editor_engine_context::register_on_editor_update_callback(function<void(engine_context&)> callback)
    {
        _editor_callbacks.on_update.push_back(tempest::move(callback));
    }

    void editor_engine_context::clear_editor_callbacks()
    {
        _editor_callbacks.on_paint.clear();
        _editor_callbacks.on_update.clear();
    }

    auto editor_engine_context::run() -> void
    {
        _logger.trace("Starting editor engine");

        _logger.trace("Running initialization callbacks");
        for (auto&& init_cb : _on_initialize_callbacks)
        {
            init_cb(*this);
        }
        _logger.trace("Finished initialization callbacks");

        auto simulated_time = std::chrono::duration<double>(0.0);
        auto delta_time = std::chrono::duration<double>(1.0 / 60.0);

        auto current_time = std::chrono::steady_clock::now();
        auto accumulator = std::chrono::duration<double>(0.0);
        _last_frame_time = current_time;

        _logger.trace("Starting editor main loop");
        while (!_should_close)
        {
            auto frame_start_time = std::chrono::steady_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::duration<float>>(frame_start_time - _last_frame_time);
            _delta_frame_time = delta;
            _last_frame_time = frame_start_time;

            auto new_time = std::chrono::steady_clock::now();
            auto frame_time = new_time - current_time;
            current_time = new_time;

            accumulator += frame_time;

            for (auto& win : _windows)
            {
                auto& mouse = _window_manager.get_mouse(win.handle);
                mouse.reset_mouse_deltas();
                mouse.set_disabled(_window_manager.is_cursor_disabled(win.handle));
            }

            _window_manager.poll_events();

            for (auto it = _windows.begin(); it != _windows.end();)
            {
                if (_window_manager.should_close(it->handle))
                {
                    if (_device)
                    {
                        if (it->acquire_sem.handle != 0)
                        {
                            _device->destroy_semaphore(it->acquire_sem);
                        }
                        if (it->timeline_sem.handle != 0)
                        {
                            _device->destroy_semaphore(it->timeline_sem);
                        }
                        for (auto sem : it->render_semaphores)
                        {
                            _device->destroy_semaphore(sem);
                        }
                        if (it->render_surface)
                        {
                            _device->destroy_render_surface(tempest::move(it->render_surface));
                        }
                        if (it->raw_surface.handle != 0)
                        {
                            _device->destroy_raw_surface(it->raw_surface);
                        }
                    }
                    _window_manager.destroy_window(it->handle);
                    it = _windows.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            if (_windows.empty())
            {
                _should_close = true;
                break;
            }

            if (_sim_state == simulation_state::play)
            {
                while (accumulator >= delta_time)
                {
                    for (auto&& callback : _on_fixed_update_callbacks)
                    {
                        callback(*this, std::chrono::duration_cast<std::chrono::duration<float>>(delta_time));
                    }
                    if (_should_close)
                    {
                        goto exit_editor_main_loop;
                    }

                    simulated_time += delta_time;
                    accumulator -= delta_time;
                }

                for (auto&& callback : _on_variable_update_callbacks)
                {
                    callback(*this, _delta_frame_time);
                }
            }
            else
            {
                accumulator = std::chrono::duration<double>(0.0);
            }

            for (auto&& editor_update : _editor_callbacks.on_update)
            {
                editor_update(*this);
            }

            _render_editor_frame();
        }

    exit_editor_main_loop:
        _logger.trace("Exiting editor main loop");

        if (_device)
        {
            _device->wait_idle();
        }

        _logger.trace("Running close callbacks");
        for (auto&& close_cb : _on_close_callbacks)
        {
            close_cb(*this);
        }
        _logger.trace("Finished close callbacks");
    }

    auto editor_engine_context::_render_editor_frame() -> void
    {
        if (!_renderer || !_device || _windows.empty())
        {
            return;
        }

        auto& win = _windows.front();
        if (!win.render_surface)
        {
            return;
        }

        const auto cur_w = _window_manager.get_framebuffer_width(win.handle);
        const auto cur_h = _window_manager.get_framebuffer_height(win.handle);

        if (cur_w == 0 || cur_h == 0)
        {
            return;
        }

        if (cur_w != win.render_surface->get_width() || cur_h != win.render_surface->get_height() || win.need_recreate)
        {
            _device->wait_idle();

            auto caps = _device->get_surface_capabilities(win.raw_surface);
            auto new_surf_desc = rhi::render_surface_desc{
                .raw_surface = win.raw_surface,
                .present_mode = win.present_mode,
                .format = win.surface_format,
                .width = cur_w,
                .height = cur_h,
                .min_image_count = caps.min_image_count,
                .preferred_image_count = caps.min_image_count + 1,
                .old_surface = win.render_surface.get(),
            };

            auto new_surf = _device->create_render_surface(new_surf_desc);
            if (new_surf)
            {
                _device->destroy_render_surface(tempest::move(win.render_surface));
                win.render_surface = tempest::move(new_surf);
                win.need_recreate = false;
            }
            else
            {
                return;
            }
        }

        // 1. Run paint callbacks (evaluates ImGui layout, viewport size, mouse/keyboard navigation, camera updates)
        for (auto&& on_paint : _editor_callbacks.on_paint)
        {
            on_paint(*this);
        }

        // 2. Prepare and render 3D scene offscreen (with up-to-date camera and viewport dimensions)
        auto camera_override =
            (_sim_state == simulation_state::play)
                ? tempest::nullopt
                : tempest::optional<render_system::render_camera>(_editor_camera.get_render_camera());

        const auto cfg_w = _renderer->get_config().render_width;
        const auto cfg_h = _renderer->get_config().render_height;
        const auto target_w = (cfg_w > 0) ? cfg_w : cur_w;
        const auto target_h = (cfg_h > 0) ? cfg_h : cur_h;

        _renderer->prepare_frame(target_w, target_h, nullopt, nullopt, camera_override);
        static_cast<void>(_renderer->render());

        // 3. Acquire swapchain image, record UI render pass targeting swapchain, present
        if (win.timeline_value > 0)
        {
            _device->wait_for_sync(rhi::host_sync_point{
                .semaphore = win.timeline_sem,
                .value = win.timeline_value,
            });
        }

        auto acquire_res = win.render_surface->acquire_next_image(rhi::device_sync_point{
            .semaphore = win.acquire_sem,
            .value = 0,
            .stages = rhi::pipeline_stage::attachment_output,
        });

        if (!acquire_res.has_value())
        {
            if (acquire_res.error() == rhi::swapchain_error::out_of_date ||
                acquire_res.error() == rhi::swapchain_error::suboptimal)
            {
                win.need_recreate = true;
            }
            return;
        }

        auto sc_img = acquire_res.value();

        while (sc_img.swapchain_image_index >= win.render_semaphores.size())
        {
            win.render_semaphores.push_back(_device->create_binary_semaphore());
        }
        auto render_sem = win.render_semaphores[sc_img.swapchain_image_index];
        win.timeline_value++;

        auto& graphics_port = _device->get_graphics_execution_port();
        auto& cmd = graphics_port.acquire_command_list(0, rhi::command_list_lifetime::transient);

        cmd.begin();

        auto pre_barriers = array{
            rhi::texture_barrier{
                .texture = sc_img.texture,
                .src =
                    {
                        .stages = rhi::pipeline_stage::top_of_pipe,
                        .access = rhi::resource_access::none,
                        .layout = rhi::image_layout::undefined,
                    },
                .dst =
                    {
                        .stages = rhi::pipeline_stage::attachment_output,
                        .access = rhi::resource_access::write,
                        .layout = rhi::image_layout::general,
                    },
            },
            rhi::texture_barrier{
                .texture = rhi::texture_handle{},
                .src =
                    {
                        .stages = rhi::pipeline_stage::attachment_output,
                        .access = rhi::resource_access::write,
                        .layout = rhi::image_layout::general,
                    },
                .dst =
                    {
                        .stages = rhi::pipeline_stage::fragment,
                        .access = rhi::resource_access::read,
                        .layout = rhi::image_layout::general,
                    },
            },
        };

        auto num_barriers = 1u;
        const auto rg_tex_id = _renderer->get_tonemapped_color_texture();
        const auto* tonemap_alloc = _renderer->get_render_graph().get_allocator().get_texture(rg_tex_id.id);
        if (tonemap_alloc && tonemap_alloc->handle.handle != 0)
        {
            pre_barriers[1].texture = tonemap_alloc->handle;
            num_barriers = 2u;
        }

        cmd.pipeline_barrier(span<const rhi::texture_barrier>{pre_barriers.data(), num_barriers}, {});

        auto color_att = rhi::color_attachment{
            .view = sc_img.view,
            .load_op = rhi::load_op::clear,
            .store_op = rhi::store_op::store,
            .clear_value = rhi::clear_color_value{0.0F, 0.0F, 0.0F, 1.0F},
        };

        cmd.begin_render_pass(span<const rhi::color_attachment>{&color_att, 1}, nullopt, cur_w, cur_h);
        if (_ui_ctx)
        {
            _ui_ctx->render_ui_commands(cmd, cur_w, cur_h);
        }
        cmd.end_render_pass();

        auto post_ui_barrier = rhi::texture_barrier{
            .texture = sc_img.texture,
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
        cmd.pipeline_barrier(span<const rhi::texture_barrier>{&post_ui_barrier, 1}, {});

        cmd.end();

        auto wait_point = rhi::device_sync_point{
            .semaphore = win.acquire_sem,
            .value = 0,
            .stages = rhi::pipeline_stage::attachment_output,
        };

        auto signal_points = array{
            rhi::device_sync_point{
                .semaphore = render_sem,
                .value = 0,
                .stages = rhi::pipeline_stage::bottom_of_pipe,
            },
            rhi::device_sync_point{
                .semaphore = win.timeline_sem,
                .value = win.timeline_value,
                .stages = rhi::pipeline_stage::bottom_of_pipe,
            },
        };

        const auto* cmd_ptr = &cmd;
        auto submit_res = graphics_port.submit(
            span<const rhi::command_list*>{&cmd_ptr, 1}, span<const rhi::device_sync_point>{&wait_point, 1},
            span<const rhi::device_sync_point>{signal_points.data(), signal_points.size()});

        if (submit_res.has_value())
        {
            auto present_res = win.render_surface->present(graphics_port, rhi::device_sync_point{
                                                                              .semaphore = render_sem,
                                                                              .value = 0,
                                                                          });

            if (!present_res.has_value() && (present_res.error() == rhi::swapchain_error::out_of_date ||
                                             present_res.error() == rhi::swapchain_error::suboptimal))
            {
                win.need_recreate = true;
            }
        }
    }
} // namespace tempest::editor