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
                auto mat = _material_reg.get_material(material_id);
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

                auto material = _material_reg.get_material(material_id);

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
} // namespace tempest