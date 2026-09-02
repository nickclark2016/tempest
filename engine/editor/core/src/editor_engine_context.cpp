#include <tempest/editor_engine_context.hpp>

#include <tempest/array.hpp>
#include <tempest/memory.hpp>
#include <tempest/move.hpp>
#include <tempest/optional.hpp>
#include <tempest/profiler/serialization.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/ui.hpp>
#include <tempest/vector.hpp>

#include <chrono>
#include <format>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shellapi.h>
#include <windows.h>

namespace tempest::editor
{
    namespace
    {
        using PFN_ShellExecuteW = HINSTANCE(WINAPI*)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);

        auto open_url_in_browser(string_view url) -> void
        {
            if (url.empty())
            {
                return;
            }
            const auto len = MultiByteToWideChar(CP_UTF8, 0, url.data(), static_cast<int>(url.size()), nullptr, 0);
            if (len <= 0)
            {
                return;
            }
            auto wide_url = vector<wchar_t>(static_cast<size_t>(len + 1), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, url.data(), static_cast<int>(url.size()), wide_url.data(), len);
            wide_url[len] = L'\0';

            auto shell32 = LoadLibraryA("shell32.dll");
            if (shell32)
            {
                auto pfn = reinterpret_cast<PFN_ShellExecuteW>(GetProcAddress(shell32, "ShellExecuteW"));
                if (pfn)
                {
                    pfn(nullptr, L"open", wide_url.data(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
        }

        auto open_folder_in_explorer(string_view folder_path) -> void
        {
            if (folder_path.empty())
            {
                return;
            }
            const auto len =
                MultiByteToWideChar(CP_UTF8, 0, folder_path.data(), static_cast<int>(folder_path.size()), nullptr, 0);
            if (len <= 0)
            {
                return;
            }
            auto wide_path = vector<wchar_t>(static_cast<size_t>(len + 1), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, folder_path.data(), static_cast<int>(folder_path.size()), wide_path.data(),
                                len);
            wide_path[len] = L'\0';

            auto shell32 = LoadLibraryA("shell32.dll");
            if (shell32)
            {
                auto pfn = reinterpret_cast<PFN_ShellExecuteW>(GetProcAddress(shell32, "ShellExecuteW"));
                if (pfn)
                {
                    pfn(nullptr, L"open", wide_path.data(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
        }
    } // namespace
} // namespace tempest::editor
#else
#include <cstdlib>

namespace tempest::editor
{
    namespace
    {
        auto open_url_in_browser(string_view url) -> void
        {
            if (url.empty())
            {
                return;
            }
            auto cmd = std::format("xdg-open '{}' &", std::string_view{url.data(), url.size()});
            [[maybe_unused]] const auto res = std::system(cmd.c_str());
        }

        auto open_folder_in_explorer(string_view folder_path) -> void
        {
            if (folder_path.empty())
            {
                return;
            }
            auto cmd = std::format("xdg-open '{}' &", std::string_view{folder_path.data(), folder_path.size()});
            [[maybe_unused]] const auto res = std::system(cmd.c_str());
        }
    } // namespace
} // namespace tempest::editor
#endif

namespace tempest::editor
{
    editor_engine_context::editor_engine_context()
    {
        _profiler_session.set_thread_name("Main Thread");
        auto config = profiler::web_server_config{
            .host = "127.0.0.1",
            .port = 8080,
            .max_port_attempts = 10,
            .enable_live_stream = true,
        };
        _web_server = make_unique<profiler::web_server>(_profiler_session, tempest::move(config));
        _web_server->start();
    }

    editor_engine_context::~editor_engine_context()
    {
        if (_web_server)
        {
            _web_server->stop();
        }
    }

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

    auto editor_engine_context::set_recording(bool recording) -> void
    {
        if (_is_recording == recording)
        {
            return;
        }
        _is_recording = recording;
        if (_is_recording)
        {
            _logger.info("Profiler capture recording started");
            _profiler_session.set_enabled(true);
        }
        else
        {
            _logger.info("Profiler capture recording stopped");
            _profiler_session.get_or_register_thread().flush_active_chunk();
            const auto capture = profiler::create_capture_from_session(_profiler_session);

            const auto now = std::chrono::system_clock::now();
            const auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            auto fname = std::format("captures/capture_{}.tprof", ts);

#if defined(_WIN32)
            CreateDirectoryA("captures", nullptr);
#else
            std::system("mkdir -p captures");
#endif
            auto save_result = profiler::save_binary_capture(capture, string_view{fname.data(), fname.size()});
            if (save_result)
            {
                const auto log_msg = std::format("Saved profiler capture to {}", fname);
                _logger.info(string_view{log_msg.data(), log_msg.size()});
            }
            else
            {
                const auto log_msg = std::format("Failed to save profiler capture to {}", fname);
                _logger.error(string_view{log_msg.data(), log_msg.size()});
            }
        }
    }

    auto editor_engine_context::toggle_recording() -> bool
    {
        set_recording(!_is_recording);
        return _is_recording;
    }

    auto editor_engine_context::insert_bookmark_marker(string_view name) -> void
    {
        auto marker_name = string{};
        if (name.empty())
        {
            auto formatted = std::format("ViewportBookmark_Frame{}", _frame_index);
            marker_name = string{formatted.data(), formatted.size()};
        }
        else
        {
            marker_name = string{name.data(), name.size()};
        }

        const auto log_msg = std::format("Inserted profiler bookmark marker: {}",
                                         std::string_view{marker_name.data(), marker_name.size()});
        _logger.info(string_view{log_msg.data(), log_msg.size()});
        _profiler_session.get_or_register_thread().add_marker(marker_name);
    }

    auto editor_engine_context::open_profiler_in_browser() -> void
    {
        if (_web_server && _web_server->is_running())
        {
            const auto url = _web_server->get_server_url();
            const auto log_msg = std::format("Opening profiler Web UI at {}", std::string_view{url.data(), url.size()});
            _logger.info(string_view{log_msg.data(), log_msg.size()});
            open_url_in_browser(string_view{url.data(), url.size()});
        }
    }

    auto editor_engine_context::open_captures_folder() -> void
    {
#if defined(_WIN32)
        CreateDirectoryA("captures", nullptr);
#else
        std::system("mkdir -p captures");
#endif
        open_folder_in_explorer(string_view{"captures"});
    }

    auto editor_engine_context::collect_and_broadcast_telemetry() -> void
    {
        _profiler_session.get_or_register_thread().flush_active_chunk();

        auto capture = profiler::create_capture_from_session(_profiler_session);
        auto telemetry = profiler::create_telemetry_frame_from_capture(++_frame_index, capture);

        auto gpu_time_ns = uint64_t{0};
        for (const auto& gtrack : telemetry.gpu_tracks)
        {
            for (const auto& z : gtrack.zones)
            {
                if (z.end_ns >= z.start_ns)
                {
                    gpu_time_ns += (z.end_ns - z.start_ns);
                }
            }
        }
        _last_gpu_time_ms = static_cast<float>(gpu_time_ns) / 1000000.0f;

        _last_telemetry_frame = tempest::move(telemetry);

        if (_web_server && _live_stream_enabled && _web_server->connected_client_count() > 0)
        {
            _web_server->broadcast_telemetry(_last_telemetry_frame);
        }
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
        const auto frame_start = std::chrono::steady_clock::now();
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

            [[maybe_unused]] auto result = _renderer->render_frame(
                win.handle, camera_override, [this](rhi::command_list& cmd, uint32_t w, uint32_t h) {
                    if (_ui_ctx)
                    {
                        _ui_ctx->render_ui_commands(cmd, w, h);
                    }
                });

            const auto frame_end = std::chrono::steady_clock::now();
            const auto frame_dur =
                std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(frame_end - frame_start);
            _last_cpu_time_ms = frame_dur.count();
        }

        collect_and_broadcast_telemetry();
    }
} // namespace tempest::editor