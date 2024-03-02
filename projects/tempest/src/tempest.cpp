#include <tempest/tempest.hpp>

#include <tempest/input.hpp>
#include <tempest/logger.hpp>

namespace tempest
{
    namespace
    {
        auto log = logger::logger_factory::create({.prefix{"tempest::engine"}});
    }

    engine::engine() : _render_system{_entity_registry}
    {
    }

    engine engine::initialize()
    {
        log->info("Initializing engine");
        return engine();
    }

    void engine::render()
    {
        _render_system.render();
    }

    void engine::add_window(std::unique_ptr<graphics::iwindow> window)
    {
        log->info("Adding window to engine");
        _windows.push_back(std::move(window));

        _render_system.register_window(*_windows.back());
    }

    void engine::update()
    {
        auto current_time = std::chrono::steady_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::duration<float>>(current_time - _last_frame_time);
        _delta_time = delta;
        _last_frame_time = current_time;

        core::input::poll();

        _windows.erase(
            std::remove_if(_windows.begin(), _windows.end(), [](const auto& window) { return window->should_close(); }),
            _windows.end());

        if (_windows.empty())
        {
            _should_close = true;
        }
    }

    void engine::shutdown()
    {
        log->info("Shutting down engine");
        _render_system.on_close();
    }

    void engine::run()
    {
        log->info("Initializing engine");

        _render_system.on_initialize();

        log->info("Initialization complete");
        log->info("Engine starting main loop");

        while (!_should_close)
        {
            update();
            render();
        }

        log->info("Engine exiting main loop");

        shutdown();

        log->info("Engine has stopped");

        std::exit(0);
    }
} // namespace tempest