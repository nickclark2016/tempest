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
                    if (_renderer)
                    {
                        _renderer->unregister_surface(it->handle);
                    }
                    if (_device && it->raw_surface.handle != 0)
                    {
                        _device->destroy_raw_surface(it->raw_surface);
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
                        break;
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

            for (auto&& on_update : _editor_callbacks.on_update)
            {
                on_update(*this);
            }

            _render_editor_frame();
        }

        _logger.trace("Exiting editor main loop");

        if (_device)
        {
            _device->wait_idle();
        }

        _logger.trace("Running editor close callbacks");
        for (auto&& close_cb : _on_close_callbacks)
        {
            close_cb(*this);
        }
        _logger.trace("Finished editor close callbacks");
    }

    auto editor_engine_context::_render_editor_frame() -> void
    {
        [[maybe_unused]] const auto zone = profiler::scoped_zone{_profiler_session, "editor::render_frame"};
        if (!_renderer || _windows.empty())
        {
            return;
        }

        auto& win = _windows.front();

        // 1. Run paint callbacks (evaluates ImGui layout, viewport size, mouse/keyboard navigation, camera updates)
        {
            [[maybe_unused]] const auto paint_zone = profiler::scoped_zone{_profiler_session, "editor::on_paint"};
            for (auto&& on_paint : _editor_callbacks.on_paint)
            {
                on_paint(*this);
            }
        }

        auto camera_override =
            (_sim_state == simulation_state::play)
                ? tempest::nullopt
                : tempest::optional<render_system::render_camera>(_editor_camera.get_render_camera());

        [[maybe_unused]] auto result = _renderer->render_frame(win.handle, camera_override,
                                                               [this](rhi::command_list& cmd, uint32_t w, uint32_t h) {
                                                                   if (_ui_ctx)
                                                                   {
                                                                       _ui_ctx->render_ui_commands(cmd, w, h);
                                                                   }
                                                               });
    }
} // namespace tempest::editor