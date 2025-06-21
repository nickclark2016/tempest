#include <tempest/tempest.hpp>

#include <tempest/input.hpp>
#include <tempest/logger.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/transform_component.hpp>

#include <chrono>
#include <cstdlib>

namespace tempest
{
    namespace
    {
        auto log = logger::logger_factory::create({.prefix{"tempest::engine"}});
    } // namespace

    engine_context::engine_context() : _asset_database(&_mesh_reg, &_texture_reg, &_material_reg)
    {
    }

    tuple<rhi::window_surface*, core::input_group> engine_context::register_window(rhi::window_surface_desc desc)
    {
        auto window = _render.create_window(desc);
        auto keyboard = make_unique<core::keyboard>();
        auto mouse = make_unique<core::mouse>();

        window->register_keyboard_callback([kb = keyboard.get()](const core::key_state& s) { kb->set(s); });
        window->register_mouse_callback([mouse = mouse.get()](const core::mouse_button_state& s) { mouse->set(s); });
        window->register_cursor_callback([mouse = mouse.get()](float x, float y) { mouse->set_position(x, y); });
        window->register_scroll_callback([mouse = mouse.get()](float x, float y) { mouse->set_scroll(x, y); });

        _windows.push_back({
            .surface = tempest::move(window),
            .keyboard = tempest::move(keyboard),
            .mouse = tempest::move(mouse),
        });

        return make_tuple(_windows.back().surface.get(), core::input_group{
                                                             .kb = _windows.back().keyboard.get(),
                                                             .ms = _windows.back().mouse.get(),
                                                         });
    }

    void engine_context::register_on_initialize_callback(function<void(engine_context&)> callback)
    {
        _on_initialize_callbacks.push_back(tempest::move(callback));
    }

    void engine_context::register_on_close_callback(function<void(engine_context&)> callback)
    {
        _on_close_callbacks.push_back(tempest::move(callback));
    }

    void engine_context::register_on_fixed_update_callback(
        function<void(engine_context&, std::chrono::duration<float>)> callback)
    {
        _on_fixed_update_callbacks.push_back(tempest::move(callback));
    }

    void engine_context::register_on_variable_update_callback(
        function<void(engine_context&, std::chrono::duration<float>)> callback)
    {
        _on_variable_update_callbacks.push_back(tempest::move(callback));
    }

    void engine_context::request_close(bool close) noexcept
    {
        _should_close = close;
    }

    bool engine_context::should_close() const noexcept
    {
        return _should_close;
    }

    ecs::archetype_entity engine_context::load_entity(ecs::archetype_entity src)
    {
        auto ent = _entity_registry.duplicate(src);
        _entities_to_load.push_back(ent);
        return ent;
    }

    void engine_context::run()
    {
        for (auto&& init_cb : _on_initialize_callbacks)
        {
            init_cb(*this);
        }

        _render.upload_objects_sync(_entities_to_load, get_mesh_registry(), get_texture_registry(),
                                    get_material_registry());
        _entities_to_load.clear();

        std::chrono::duration<double> simulated_time = std::chrono::duration<double>(0.0);
        std::chrono::duration<double> delta_time = std::chrono::duration<double>(1.0 / 60.0);

        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> accumulator = std::chrono::duration<double>(0.0);

        while (!_should_close)
        {
            auto frame_start_time = std::chrono::steady_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::duration<float>>(frame_start_time - _last_frame_time);
            _delta_frame_time = delta;
            _last_frame_time = frame_start_time;

            auto new_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> frame_time = new_time - current_time;

            current_time = new_time;

            accumulator += frame_time;

            while (accumulator >= delta_time)
            {
                // Update the engine with the fixed timestep
                _update_fixed(delta_time);
                if (_should_close)
                {
                    goto exit_main_loop;
                }

                simulated_time += delta_time;
                accumulator -= delta_time;
            }

            _update_variable(_delta_frame_time);
            _render_frame();
        }

    exit_main_loop:

        log->info("Engine exiting main loop");

        for (auto&& close_cb : _on_close_callbacks)
        {
            close_cb(*this);
        }

        log->info("Engine has stopped");

        std::exit(0);
    }

    ecs::archetype_registry& engine_context::get_registry() noexcept
    {
        return _entity_registry;
    }

    const ecs::archetype_registry& engine_context::get_registry() const noexcept
    {
        return _entity_registry;
    }

    core::material_registry& engine_context::get_material_registry() noexcept
    {
        return _material_reg;
    }

    const core::material_registry& engine_context::get_material_registry() const noexcept
    {
        return _material_reg;
    }

    core::mesh_registry& engine_context::get_mesh_registry() noexcept
    {
        return _mesh_reg;
    }

    const core::mesh_registry& engine_context::get_mesh_registry() const noexcept
    {
        return _mesh_reg;
    }

    core::texture_registry& engine_context::get_texture_registry() noexcept
    {
        return _texture_reg;
    }

    const core::texture_registry& engine_context::get_texture_registry() const noexcept
    {
        return _texture_reg;
    }

    assets::asset_database& engine_context::get_asset_database() noexcept
    {
        return _asset_database;
    }

    const assets::asset_database& engine_context::get_asset_database() const noexcept
    {
        return _asset_database;
    }

    graphics::renderer& engine_context::get_renderer() noexcept
    {
        return _render;
    }

    const graphics::renderer& engine_context::get_renderer() const noexcept
    {
        return _render;
    }

    void engine_context::_update_fixed(std::chrono::duration<float> fixed_step)
    {
        for (auto& window : _windows)
        {
            window.mouse->reset_mouse_deltas();
            window.mouse->set_disabled(window.surface->is_cursor_disabled());
        }

        core::input::poll();

        tempest::erase_if(_windows, [](const auto& window_payload) { return window_payload.surface->should_close(); });

        if (_windows.empty()) [[unlikely]]
        {
            _should_close = true;
            return;
        }

        for (auto&& cb : _on_fixed_update_callbacks)
        {
            cb(*this, fixed_step);
        }
    }

    void engine_context::_update_variable(std::chrono::duration<float> free_step)
    {
        for (auto&& cb : _on_variable_update_callbacks)
        {
            cb(*this, free_step);
        }
    }

    void engine_context::_render_frame()
    {
        _render.render();
    }
} // namespace tempest