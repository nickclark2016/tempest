#include <tempest/editor_engine_context.hpp>

#include <tempest/default_importers.hpp>
#include <tempest/input.hpp>

namespace tempest::editor
{
    namespace
    {
        auto make_default_log_sinks()
        {
            auto sinks = vector<unique_ptr<log_sink>>{};
            sinks.push_back(make_unique<mt_stdout_log_sink>(log_level::trace, log_level::fatal));
            return sinks;
        }

        auto make_default_logger(span<unique_ptr<log_sink>> sinks)
        {
            auto logger = tempest::logger();
            for (auto& sink : sinks)
            {
                logger = tempest::logger(*sink);
            }
            return logger;
        }
    } // namespace

    editor_engine_context::editor_engine_context()
        : _log_sinks(make_default_log_sinks()), _logger(make_default_logger(_log_sinks)),
          _entity_registry(_event_registry), _asset_database(&_asset_type_reg),
          _render(graphics::renderer::builder()
                      .set_pbr_frame_graph_config({
                          .render_target_width = 1920,
                          .render_target_height = 1080,
                          .hdr_color_format = tempest::rhi::image_format::rgba16_float,
                          .depth_format = tempest::rhi::image_format::d32_float,
                          .tonemapped_color_format = tempest::rhi::image_format::rgba8_srgb,
                          .vertex_data_buffer_size = 16 * 1024 * 1024,
                          .max_mesh_count = 16 * 1024 * 1024,
                          .max_material_count = 4 * 1024 * 1024,
                          .staging_buffer_size_per_frame = 16 * 1024 * 1024,
                          .max_object_count = 256 * 1024,
                          .max_lights = 256,
                          .max_bindless_textures = 1024,
                          .max_anisotropy = 16.0f,
                          .light_clustering =
                              {
                                  .cluster_count_x = 16,
                                  .cluster_count_y = 9,
                                  .cluster_count_z = 24,
                                  .max_lights_per_cluster = 128,
                              },
                          .shadows =
                              {
                                  .directional_shadow_map_width = 4096,
                                  .directional_shadow_map_height = 4096,
                                  .max_shadow_casting_lights = 16,
                              },
                      })
                      .set_pbr_frame_graph_inputs({
                          .entity_registry = &_entity_registry,
                      })
                      .build(_logger))
    {
        if (::setlocale(LC_ALL, "en_US.UTF-8") == nullptr)
        {
            _logger.error("Failed to set locale to UTF-8. Logging may not work correctly.");
        }

        try
        {
            std::locale utf8_locale("en_US.UTF-8");
            std::locale::global(utf8_locale);
            std::cin.imbue(utf8_locale);
            std::cout.imbue(utf8_locale);
            std::cerr.imbue(utf8_locale);
        }
        catch (const std::runtime_error&)
        {
            _logger.error("Failed to set global locale to UTF-8. Logging may not work correctly.");
        }

#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif

        assets::register_default_importers(_asset_database, &_mesh_reg, &_texture_reg, &_material_reg);
    }

    engine_context::window_registration_info editor_engine_context::register_window(rhi::window_surface_desc desc,
                                                                                    bool install_swapchain_blit)
    {
        auto [window, surface] = _render.create_window(desc, install_swapchain_blit);
        auto keyboard = make_unique<core::keyboard>();
        auto mouse = make_unique<core::mouse>();

        window->register_keyboard_callback([kb = keyboard.get()](const core::key_state& s) { kb->set(s); });
        window->register_mouse_callback([mouse = mouse.get()](const core::mouse_button_state& s) { mouse->set(s); });
        window->register_cursor_callback([mouse = mouse.get()](float x, float y) { mouse->set_position(x, y); });
        window->register_scroll_callback([mouse = mouse.get()](float x, float y) { mouse->set_scroll(x, y); });

        _windows.push_back({
            .surface = tempest::move(window),
            .render_surface = surface,
            .keyboard = tempest::move(keyboard),
            .mouse = tempest::move(mouse),
        });

        return window_registration_info{
            .surface = _windows.back().surface.get(),
            .render_surface = _windows.back().render_surface,
            .inputs =
                core::input_group{
                    .kb = _windows.back().keyboard.get(),
                    .ms = _windows.back().mouse.get(),
                },
        };
    }

    void editor_engine_context::register_on_initialize_callback(function<void(engine_context&)> callback)
    {
        _engine_callbacks.on_initialize.push_back(tempest::move(callback));
    }

    void editor_engine_context::register_on_close_callback(function<void(engine_context&)> callback)
    {
        _engine_callbacks.on_close.push_back(tempest::move(callback));
    }

    void editor_engine_context::register_on_fixed_update_callback(
        function<void(engine_context&, std::chrono::duration<float>)> callback)
    {
        _engine_callbacks.on_fixed_update.push_back(tempest::move(callback));
    }

    void editor_engine_context::register_on_variable_update_callback(
        function<void(engine_context&, std::chrono::duration<float>)> callback)
    {
        _engine_callbacks.on_variable_update.push_back(tempest::move(callback));
    }

    void editor_engine_context::register_on_editor_paint_callback(function<void(engine_context&)> callback)
    {
        _editor_callbacks.on_paint.push_back(tempest::move(callback));
    }

    void editor_engine_context::register_on_editor_update_callback(function<void(engine_context&)> callback)
    {
        _editor_callbacks.on_update.push_back(tempest::move(callback));
    }

    void editor_engine_context::request_close(bool close)
    {
        _should_close = close;
    }

    bool editor_engine_context::should_close() const
    {
        return _should_close;
    }

    ecs::entity editor_engine_context::load_entity(ecs::entity src)
    {
        auto ent = _entity_registry.duplicate(src);
        _entities_to_load.push_back(ent);
        return ent;
    }

    void editor_engine_context::run()
    {
        _logger.trace("Starting engine");

        _logger.trace("Running initialization callbacks");
        for (auto&& init_cb : _engine_callbacks.on_initialize)
        {
            init_cb(*this);
        }
        _logger.trace("Finished initialization callbacks");

        _render.finalize_graph();

        _render.upload_objects_sync(_entities_to_load, get_meshes(), get_textures(), get_materials());
        _entities_to_load.clear();

        std::chrono::duration<double> simulated_time = std::chrono::duration<double>(0.0);
        std::chrono::duration<double> delta_time = std::chrono::duration<double>(1.0 / 60.0);

        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> accumulator = std::chrono::duration<double>(0.0);

        _logger.trace("Starting main loop");
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
        _logger.trace("Exiting main loop");

        _logger.trace("Running close callbacks");
        for (auto&& close_cb : _engine_callbacks.on_close)
        {
            close_cb(*this);
        }
        _logger.trace("Finished close callbacks");

        std::exit(0);
    }

    auto editor_engine_context::get_entities() -> ecs::archetype_registry&
    {
        return _entity_registry;
    }

    auto editor_engine_context::get_entities() const -> const ecs::archetype_registry&
    {
        return _entity_registry;
    }

    auto editor_engine_context::get_events() -> event::event_registry&
    {
        return _event_registry;
    }

    auto editor_engine_context::get_events() const -> const event::event_registry&
    {
        return _event_registry;
    }

    auto editor_engine_context::get_materials() -> core::material_registry&
    {
        return _material_reg;
    }

    auto editor_engine_context::get_materials() const -> const core::material_registry&
    {
        return _material_reg;
    }

    auto editor_engine_context::get_meshes() -> core::mesh_registry&
    {
        return _mesh_reg;
    }

    auto editor_engine_context::get_meshes() const -> const core::mesh_registry&
    {
        return _mesh_reg;
    }

    auto editor_engine_context::get_textures() -> core::texture_registry&
    {
        return _texture_reg;
    }

    auto editor_engine_context::get_textures() const -> const core::texture_registry&
    {
        return _texture_reg;
    }

    auto editor_engine_context::get_assets() -> assets::asset_database&
    {
        return _asset_database;
    }

    auto editor_engine_context::get_assets() const -> const assets::asset_database&
    {
        return _asset_database;
    }

    auto editor_engine_context::get_renderer() -> graphics::renderer&
    {
        return _render;
    }

    auto editor_engine_context::get_renderer() const -> const graphics::renderer&
    {
        return _render;
    }

    void editor_engine_context::_update_fixed(std::chrono::duration<float> delta_time)
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

        if (_sim_state == simulation_state::play)
        {
            for (auto&& callback : _engine_callbacks.on_fixed_update)
            {
                callback(*this, delta_time);
            }
        }
    }

    void editor_engine_context::_update_variable(std::chrono::duration<float> delta_time)
    {
        for (auto&& callback : _editor_callbacks.on_update)
        {
            callback(*this);
        }

        if (_sim_state == simulation_state::play)
        {
            for (auto&& callback : _engine_callbacks.on_variable_update)
            {
                callback(*this, delta_time);
            }
        }
    }

    void editor_engine_context::_render_frame()
    {
        for (auto&& callback : _editor_callbacks.on_paint)
        {
            callback(*this);
        }

        _render.render();
    }
} // namespace tempest::editor