#include <tempest/tempest.hpp>

#include <tempest/input.hpp>
#include <tempest/logger.hpp>
#include <tempest/mesh_asset.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/transform_component.hpp>

#include <chrono>
#include <iostream>

namespace tempest
{
    namespace
    {
        auto log = logger::logger_factory::create({.prefix{"tempest::engine"}});

        graphics::material_type convert_material_type(assets::material_type type)
        {
            switch (type)
            {
            case assets::material_type::OPAQUE:
                return graphics::material_type::OPAQUE;
            case assets::material_type::BLEND:
                return graphics::material_type::TRANSPARENT;
            case assets::material_type::MASK:
                return graphics::material_type::MASK;
            }

            std::exit(EXIT_FAILURE);
        }
    } // namespace

    engine::engine() : _render_system{_entity_registry}, _delta_time{}
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

        return std::make_tuple(_windows.back().window.get(),
                               core::input_group{_windows.back().keyboard.get(), _windows.back().mouse.get()});
    }

    void engine::update(float dt)
    {
        auto current_time = std::chrono::steady_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::duration<float>>(current_time - _last_frame_time);
        _delta_time = delta;
        _last_frame_time = current_time;

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

        if (_windows.empty())
        {
            _should_close = true;
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

    ecs::entity engine::load_asset(std::string_view path)
    {
        auto model_result = assets::load_scene(std::string(path));
        if (!model_result)
        {
            log->error("Failed to load asset: {}", path);
            return ecs::tombstone;
        }

        auto& model = *model_result;

        std::unordered_map<std::uint32_t, ecs::entity> node_to_entity;

        auto root = _entity_registry.acquire_entity();

        std::uint32_t node_id = 0;
        std::uint32_t renderable_count = 0;
        for (auto& node : model.nodes)
        {
            auto ent = _entity_registry.acquire_entity();
            node_to_entity[node_id] = ent;

            ecs::transform_component transform;
            transform.position(node.position);
            transform.rotation(node.rotation);
            transform.scale(node.scale);

            _entity_registry.assign(ent, transform);

            ecs::relationship_component<ecs::entity> relationship{
                .parent = node.parent == std::numeric_limits<std::uint32_t>::max() ? root : node_to_entity[node.parent],
                .next_sibling = ecs::tombstone,
                .first_child = ecs::tombstone,
            };

            if (relationship.parent != root)
            {
                auto& parent_relationship =
                    _entity_registry.get<ecs::relationship_component<ecs::entity>>(relationship.parent);

                relationship.next_sibling = parent_relationship.first_child;

                if (parent_relationship.first_child == ecs::tombstone)
                {
                    parent_relationship.first_child = ent;
                }
            }

            _entity_registry.assign(ent, relationship);

            if (node.mesh_id != std::numeric_limits<std::uint32_t>::max()) [[likely]]
            {
                graphics::renderable_component renderable;
                renderable.mesh_id = node.mesh_id + _render_system.mesh_count();
                renderable.material_id = model.meshes[node.mesh_id].material_id + _render_system.material_count();
                renderable.object_id = renderable_count + _render_system.object_count();
                ++renderable_count;

                _entity_registry.assign(ent, renderable);
            }

            ++node_id;
        }

        // TODO: Load assets to renderer

        std::vector<core::mesh> meshes;
        meshes.reserve(model.meshes.size());
        for (auto& mesh : model.meshes)
        {
            meshes.push_back(mesh.mesh);
        }

        _render_system.load_mesh(meshes);
        _render_system.allocate_entities(node_id);

        for (auto material : model.materials)
        {
            graphics::material_payload payload{
                .type = convert_material_type(material.type),
                .albedo_map_id = material.base_color_texture,
                .normal_map_id = material.normal_map_texture,
                .metallic_map_id = material.metallic_roughness_texture,
                .ao_map_id = material.occlusion_map_texture,
                .alpha_cutoff = material.alpha_cutoff,
                .reflectance = 0.0f,
                .base_color_factor = material.base_color_factor,
            };

            _render_system.load_material(payload);
        }

        std::vector<graphics::texture_data_descriptor> texture_descriptors;
        for (auto& tex_asset : model.textures)
        {
            graphics::texture_data_descriptor desc{
                .fmt =
                    tex_asset.linear ? graphics::resource_format::RGBA8_UNORM : graphics::resource_format::RGBA8_SRGB,
                .mips{
                    {
                        {
                            .width = tex_asset.width,
                            .height = tex_asset.height,
                            .bytes = tex_asset.data,
                        },
                    },
                },
            };

            texture_descriptors.push_back(desc);
        }

        _render_system.load_textures(texture_descriptors, true);

        return root;
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

        auto last_tick_time = std::chrono::high_resolution_clock::now();
        auto last_frame_time = last_tick_time;
        std::uint32_t fps_counter = 0;
        std::uint32_t last_fps = 0;

        while (true)
        {
            auto current_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> time_since_tick = current_time - last_tick_time;
            std::chrono::duration<double> frame_time = current_time - last_frame_time;
            last_frame_time = current_time;

            ++fps_counter;

            if (time_since_tick.count() >= 1.0)
            {
                last_fps = fps_counter;
                fps_counter = 0;
                last_tick_time = current_time;
                std::cout << "FPS: " << last_fps << std::endl;
            }

            update(static_cast<float>(frame_time.count()));
            if (_should_close)
            {
                break;
            }

            render();
        }

        log->info("Engine exiting main loop");

        shutdown();

        log->info("Engine has stopped");

        std::exit(0);
    }
} // namespace tempest