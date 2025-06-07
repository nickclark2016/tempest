#include <tempest/tempest.hpp>

#include <tempest/input.hpp>
#include <tempest/logger.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/transform_component.hpp>

#include <chrono>
#include <iostream>

namespace tempest
{
    namespace
    {
        auto log = logger::logger_factory::create({.prefix{"tempest::engine"}});
    } // namespace

    engine::engine()
        : _asset_database{&_mesh_reg, &_texture_reg, &_material_reg}, _render_system{_archetype_entity_registry}
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

    std::tuple<graphics::iwindow*, core::input_group> engine::add_window(std::unique_ptr<graphics::iwindow> window)
    {
        log->info("Adding window to engine");

        window_payload payload{
            .window = std::move(window),
            .keyboard = std::make_unique<core::keyboard>(),
            .mouse = std::make_unique<core::mouse>(),
        };

        _render_system.register_window(*payload.window);
        _windows.push_back(std::move(payload));

        _windows.back().window->register_keyboard_callback(
            [&](const auto& key_state) { _windows.back().keyboard->set(key_state); });
        _windows.back().window->register_mouse_callback(
            [&](const auto& mouse_state) { _windows.back().mouse->set(mouse_state); });
        _windows.back().window->register_cursor_callback(
            [&](float x, float y) { _windows.back().mouse->set_position(x, y); });
        _windows.back().window->register_scroll_callback(
            [&](float x, float y) { _windows.back().mouse->set_scroll(x, y); });

        return std::make_tuple(_windows.back().window.get(),
                               core::input_group{_windows.back().keyboard.get(), _windows.back().mouse.get()});
    }

    void engine::update(float dt)
    {
        // Reset mouse deltas
        for (auto& window : _windows)
        {
            window.mouse->reset_mouse_deltas();
            window.mouse->set_disabled(window.window->is_cursor_disabled());
        }

        core::input::poll();

        _windows.erase(std::remove_if(_windows.begin(), _windows.end(),
                                      [](const auto& window_payload) { return window_payload.window->should_close(); }),
                       _windows.end());

        if (_windows.empty()) [[unlikely]]
        {
            _should_close = true;
            return;
        }

        for (auto& cb : _update_callbacks)
        {
            cb(*this, dt);
        }
    }

    void engine::shutdown()
    {
        log->info("Shutting down engine");

        for (auto& close_cb : _close_callbacks)
        {
            close_cb(*this);
        }

        _render_system.on_close();
    }

    ecs::archetype_entity engine::load_entity(ecs::archetype_entity src)
    {
        auto dst = _archetype_entity_registry.duplicate(src);

        // TODO: Move this logic to the render system
        // Iterate over all of the entities and upload the mesh, material, and textures
        auto hierarchy = ecs::archetype_entity_hierarchy_view(_archetype_entity_registry, dst);

        vector<guid> mesh_guids;
        vector<guid> material_guids;
        vector<guid> texture_guids;

        for (auto e : hierarchy)
        {
            // Get the mesh, material, and transform components
            auto mesh_comp = _archetype_entity_registry.try_get<core::mesh_component>(e);
            auto material_comp = _archetype_entity_registry.try_get<core::material_component>(e);

            if (mesh_comp != nullptr && material_comp != nullptr)
            {
                auto mesh_id = mesh_comp->mesh_id;
                mesh_guids.push_back(mesh_id);

                auto material_id = material_comp->material_id;
                if (tempest::find(material_guids.begin(), material_guids.end(), material_id) != material_guids.end())
                {
                    continue;
                }

                material_guids.push_back(material_id);

                // Get the material and add the textures to the texture list
                auto mat = _material_reg.find(material_id);
                if (mat)
                {
                    if (auto base_color = mat->get_texture(core::material::base_color_texture_name);
                        base_color.has_value())
                    {
                        texture_guids.push_back(*base_color);
                    }

                    if (auto normal_map = mat->get_texture(core::material::normal_texture_name); normal_map.has_value())
                    {
                        texture_guids.push_back(*normal_map);
                    }

                    if (auto metallic_roughness = mat->get_texture(core::material::metallic_roughness_texture_name);
                        metallic_roughness.has_value())
                    {
                        texture_guids.push_back(*metallic_roughness);
                    }

                    if (auto occlusion_map = mat->get_texture(core::material::occlusion_texture_name);
                        occlusion_map.has_value())
                    {
                        texture_guids.push_back(*occlusion_map);
                    }

                    if (auto emissive_map = mat->get_texture(core::material::emissive_texture_name);
                        emissive_map.has_value())
                    {
                        texture_guids.push_back(*emissive_map);
                    }

                    if (auto transmission_map = mat->get_texture(core::material::transmissive_texture_name);
                        transmission_map.has_value())
                    {
                        texture_guids.push_back(*transmission_map);
                    }

                    if (auto thickness_map = mat->get_texture(core::material::volume_thickness_texture_name);
                        thickness_map.has_value())
                    {
                        texture_guids.push_back(*thickness_map);
                    }
                }
            }
        }

        _render_system.load_meshes(mesh_guids, _mesh_reg);
        _render_system.load_textures(texture_guids, _texture_reg, true);
        _render_system.load_materials(material_guids, _material_reg);

        // Assign the mesh layouts to render components
        for (auto e : hierarchy)
        {
            auto mesh_comp = _archetype_entity_registry.try_get<core::mesh_component>(e);
            auto material_comp = _archetype_entity_registry.try_get<core::material_component>(e);

            if (mesh_comp != nullptr && material_comp != nullptr)
            {
                auto mesh_id = mesh_comp->mesh_id;
                auto material_id = material_comp->material_id;

                log->info("Assigning mesh {} and material {} to entity {}:{}", to_string(mesh_id).c_str(),
                          to_string(material_id).c_str(), ecs::entity_traits<ecs::entity>::as_entity(e),
                          ecs::entity_traits<ecs::entity>::as_version(e));

                auto material = _material_reg.find(material_id);

                graphics::renderable_component renderable{
                    .mesh_id = static_cast<uint32_t>(_render_system.get_mesh_id(mesh_id).value()),
                    .material_id = static_cast<uint32_t>(_render_system.get_material_id(material_id).value()),
                    .object_id = static_cast<uint32_t>(_render_system.acquire_new_object()),
                    .double_sided = material->get_bool(core::material::double_sided_name).value_or(false),
                };

                _archetype_entity_registry.assign_or_replace(e, renderable);

                if (!_archetype_entity_registry.has<ecs::transform_component>(e))
                {
                    _archetype_entity_registry.assign(e, ecs::transform_component{});
                }
            }
        }

        return dst;
    }

    void engine::run()
    {
        log->info("Initializing engine");

        _render_system.on_initialize();

        for (auto& init_cb : _initialize_callbacks)
        {
            init_cb(*this);
        }

        log->info("Initialization complete");
        log->info("Engine starting main loop");

        _render_system.after_initialize();

        std::chrono::duration<double> simulated_time = std::chrono::duration<double>(0.0);
        std::chrono::duration<double> delta_time = std::chrono::duration<double>(1.0 / 60.0);

        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> accumulator = std::chrono::duration<double>(0.0);

        while (true)
        {
            _start_frame();

            auto new_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> frame_time = new_time - current_time;

            current_time = new_time;

            accumulator += frame_time;

            while (accumulator >= delta_time)
            {
                // Update the engine with the fixed timestep
                update(static_cast<float>(delta_time.count()));
                if (_should_close)
                {
                    goto exit_main_loop;
                }

                simulated_time += delta_time;
                accumulator -= delta_time;
            }

            render();
        }

    exit_main_loop:

        log->info("Engine exiting main loop");

        shutdown();

        log->info("Engine has stopped");

        std::exit(0);
    }

    void engine::_start_frame()
    {
        auto current_time = std::chrono::steady_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::duration<float>>(current_time - _last_frame_time);
        _delta_time = delta;
        _last_frame_time = current_time;
    }

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