#include <tempest/play_mode_controller.hpp>

#include <tempest/ecs_events.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/utility.hpp>

namespace tempest::editor
{
    play_mode_controller::play_mode_controller(editor_engine_context& context) : _context{context}
    {
    }

    play_mode_controller::~play_mode_controller()
    {
        stop();
    }

    auto play_mode_controller::play(const play_settings& settings) -> bool
    {
        if (_context->get_scene_manager().has_in_flight_loads())
        {
            return false;
        }

        const auto current_state = _context->get_simulation_state();
        if (current_state == simulation_state::playing)
        {
            return true;
        }

        if (current_state == simulation_state::edit)
        {
            auto effective_settings = settings;
            if (settings.start_embedded_server)
            {
                const auto server_cfg = server::server_config{
                    .port = settings.port,
                    .max_port_attempts = 10,
                };
                _server = make_unique<server::server_context>(server_cfg);
                if (!_server->initialize())
                {
                    _server.reset();
                    return false;
                }
                _server_port = _server->bound_port();
                effective_settings.port = _server_port;
                auto* const server_ptr = _server.get();
                _server_thread = make_unique<thread>([server_ptr]() { server_ptr->run(); });
            }

            _session_diff.attach(_context->get_events());
            _saved_registry_snapshot = _context->get_entities().snapshot();
            _saved_scene_snapshot = _context->get_scene_manager().snapshot();
            _saved_editor_camera = _context->get_editor_camera();
            _context->set_simulation_state(simulation_state::playing);
            if (_on_play_begin)
            {
                _on_play_begin(effective_settings);
            }
            _context->get_events().dispatcher<ecs::simulation_started_event>().publish(ecs::simulation_started_event{});
            return true;
        }

        if (current_state == simulation_state::paused)
        {
            if (_server)
            {
                _server->set_paused(false);
            }
            _context->set_simulation_state(simulation_state::playing);
            _context->get_events().dispatcher<ecs::simulation_started_event>().publish(ecs::simulation_started_event{});
            return true;
        }

        return false;
    }

    auto play_mode_controller::pause() -> void
    {
        const auto current_state = _context->get_simulation_state();
        if (current_state != simulation_state::playing)
        {
            return;
        }

        if (_server)
        {
            _server->set_paused(true);
        }

        _context->set_simulation_state(simulation_state::paused);
        _context->get_events().dispatcher<ecs::simulation_paused_event>().publish(ecs::simulation_paused_event{});
    }

    auto play_mode_controller::step() -> bool
    {
        const auto current_state = _context->get_simulation_state();
        if (current_state != simulation_state::paused)
        {
            return false;
        }

        _context->request_single_step();
        if (!_context->is_running())
        {
            _context->step_simulation_once();
        }

        return true;
    }

    auto play_mode_controller::stop() -> void
    {
        _session_diff.detach();
        _session_diff.clear();

        if (_server)
        {
            _server->request_close();
            if (_server_thread && _server_thread->joinable())
            {
                _server_thread->join();
            }
            _server_thread.reset();
            _server.reset();
            _server_port = 0;
        }

        const auto current_state = _context->get_simulation_state();
        if (current_state == simulation_state::edit)
        {
            return;
        }

        if (_on_play_end)
        {
            _on_play_end();
        }

        _context->set_simulation_state(simulation_state::edit);

        if (_saved_scene_snapshot.has_value())
        {
            _context->get_scene_manager().restore(tempest::move(*_saved_scene_snapshot));
            _saved_scene_snapshot = tempest::nullopt;
        }

        if (_saved_registry_snapshot.has_value())
        {
            _context->get_entities().restore(tempest::move(*_saved_registry_snapshot));
            _saved_registry_snapshot = tempest::nullopt;
        }

        if (_saved_editor_camera.has_value())
        {
            _context->get_editor_camera() = *_saved_editor_camera;
            _saved_editor_camera = tempest::nullopt;
        }

        _context->get_events().dispatcher<ecs::simulation_stopped_event>().publish(ecs::simulation_stopped_event{});
    }

    auto play_mode_controller::set_on_play_begin(function<void(const play_settings&)> callback) -> void
    {
        _on_play_begin = tempest::move(callback);
    }

    auto play_mode_controller::set_on_play_end(function<void()> callback) -> void
    {
        _on_play_end = tempest::move(callback);
    }

    auto play_mode_controller::get_state() const noexcept -> simulation_state
    {
        return _context->get_simulation_state();
    }

    auto play_mode_controller::is_playing() const noexcept -> bool
    {
        return _context->get_simulation_state() == simulation_state::playing;
    }

    auto play_mode_controller::is_paused() const noexcept -> bool
    {
        return _context->get_simulation_state() == simulation_state::paused;
    }

    auto play_mode_controller::is_edit() const noexcept -> bool
    {
        return _context->get_simulation_state() == simulation_state::edit;
    }

    auto play_mode_controller::is_server_running() const noexcept -> bool
    {
        return _server != nullptr && _server->is_running();
    }

    auto play_mode_controller::is_server_paused() const noexcept -> bool
    {
        return _server != nullptr && _server->is_paused();
    }

    auto play_mode_controller::get_server_port() const noexcept -> uint16_t
    {
        return _server_port;
    }

    auto play_mode_controller::get_server_client_count() const noexcept -> size_t
    {
        return _server != nullptr ? _server->client_count() : 0;
    }

    auto play_mode_controller::get_server() const noexcept -> const server::server_context*
    {
        return _server.get();
    }
} // namespace tempest::editor
