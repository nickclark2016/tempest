#include <tempest/play_mode_controller.hpp>

#include <tempest/ecs_events.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/utility.hpp>

namespace tempest::editor
{
    play_mode_controller::play_mode_controller(editor_engine_context& context) : _context{context}
    {
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
            _saved_registry_snapshot = _context->get_entities().snapshot();
            _saved_scene_snapshot = _context->get_scene_manager().snapshot();
            _saved_editor_camera = _context->get_editor_camera();
            _context->set_simulation_state(simulation_state::playing);
            if (_on_play_begin)
            {
                _on_play_begin(settings);
            }
            _context->get_events().dispatcher<ecs::simulation_started_event>().publish(ecs::simulation_started_event{});
            return true;
        }

        if (current_state == simulation_state::paused)
        {
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
} // namespace tempest::editor
