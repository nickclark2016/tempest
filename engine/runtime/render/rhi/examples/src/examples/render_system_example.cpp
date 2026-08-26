#include "render_system_example.hpp"

#include <cmath>
#include <filesystem>
#include <tempest/asset_database.hpp>
#include <tempest/default_importers.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/transform_component.hpp>

namespace tempest::rhi::examples
{
    namespace
    {
        auto create_cube_mesh() -> core::mesh
        {
            auto m = core::mesh{};
            
            auto add_face = [&](math::vec3<float> normal, math::vec3<float> tangent,
                                math::vec3<float> v0, math::vec3<float> v1, math::vec3<float> v2, math::vec3<float> v3,
                                math::vec4<float> color) {
                const auto base_idx = static_cast<uint32_t>(m.vertices.size());
                m.vertices.push_back(core::vertex{.position = v0, .uv = {0.0F, 0.0F}, .normal = normal, .tangent = {tangent.x, tangent.y, tangent.z, 1.0F}, .color = color});
                m.vertices.push_back(core::vertex{.position = v1, .uv = {1.0F, 0.0F}, .normal = normal, .tangent = {tangent.x, tangent.y, tangent.z, 1.0F}, .color = color});
                m.vertices.push_back(core::vertex{.position = v2, .uv = {1.0F, 1.0F}, .normal = normal, .tangent = {tangent.x, tangent.y, tangent.z, 1.0F}, .color = color});
                m.vertices.push_back(core::vertex{.position = v3, .uv = {0.0F, 1.0F}, .normal = normal, .tangent = {tangent.x, tangent.y, tangent.z, 1.0F}, .color = color});

                m.indices.push_back(base_idx + 0);
                m.indices.push_back(base_idx + 1);
                m.indices.push_back(base_idx + 2);
                m.indices.push_back(base_idx + 2);
                m.indices.push_back(base_idx + 3);
                m.indices.push_back(base_idx + 0);
            };

            // Front
            add_face({0, 0, 1}, {1, 0, 0}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}, {1, 0.2F, 0.2F, 1});
            // Back
            add_face({0, 0, -1}, {-1, 0, 0}, {1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {0.2F, 1, 0.2F, 1});
            // Left
            add_face({-1, 0, 0}, {0, 0, 1}, {-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}, {0.2F, 0.2F, 1, 1});
            // Right
            add_face({1, 0, 0}, {0, 0, -1}, {1, -1, 1}, {1, -1, -1}, {1, 1, -1}, {1, 1, 1}, {1, 1, 0.2F, 1});
            // Top
            add_face({0, 1, 0}, {1, 0, 0}, {-1, 1, 1}, {1, 1, 1}, {1, 1, -1}, {-1, 1, -1}, {0.2F, 1, 1, 1});
            // Bottom
            add_face({0, -1, 0}, {1, 0, 0}, {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}, {1, 0.2F, 1, 1});

            return m;
        }

        auto create_plane_mesh() -> core::mesh
        {
            auto m = core::mesh{};
            const auto s = 20.0F;
            m.vertices.push_back(core::vertex{.position = {-s, 0.0F, -s}, .uv = {0.0F, 0.0F}, .normal = {0.0F, 1.0F, 0.0F}, .tangent = {1.0F, 0.0F, 0.0F, 1.0F}, .color = {0.8F, 0.8F, 0.8F, 1.0F}});
            m.vertices.push_back(core::vertex{.position = { s, 0.0F, -s}, .uv = {10.0F, 0.0F}, .normal = {0.0F, 1.0F, 0.0F}, .tangent = {1.0F, 0.0F, 0.0F, 1.0F}, .color = {0.8F, 0.8F, 0.8F, 1.0F}});
            m.vertices.push_back(core::vertex{.position = { s, 0.0F,  s}, .uv = {10.0F, 10.0F}, .normal = {0.0F, 1.0F, 0.0F}, .tangent = {1.0F, 0.0F, 0.0F, 1.0F}, .color = {0.8F, 0.8F, 0.8F, 1.0F}});
            m.vertices.push_back(core::vertex{.position = {-s, 0.0F,  s}, .uv = {0.0F, 10.0F}, .normal = {0.0F, 1.0F, 0.0F}, .tangent = {1.0F, 0.0F, 0.0F, 1.0F}, .color = {0.8F, 0.8F, 0.8F, 1.0F}});
            m.indices.push_back(0);
            m.indices.push_back(1);
            m.indices.push_back(2);
            m.indices.push_back(2);
            m.indices.push_back(3);
            m.indices.push_back(0);
            return m;
        }
    } // namespace

    auto render_system_example::init(rhi::device& dev, rhi::render_surface_format surface_format) -> bool
    {
        auto target_format = rhi::data_format::rgba8_unorm;
        if (surface_format == rhi::render_surface_format::bgra8_srgb)
        {
            target_format = rhi::data_format::bgra8_srgb;
        }
        else if (surface_format == rhi::render_surface_format::bgra8_unorm)
        {
            target_format = rhi::data_format::bgra8_unorm;
        }
        else if (surface_format == rhi::render_surface_format::rgba8_srgb)
        {
            target_format = rhi::data_format::rgba8_srgb;
        }

        // 1. Configure renderer
        auto builder = render_system::renderer::builder{};
        builder.set_config(render_system::renderer_config{
            .render_width = 1280,
            .render_height = 720,
            .tonemapped_color_format = target_format,
        });
        builder.set_inputs(render_system::renderer_inputs{
            .entity_registry = &_registry,
        });

        _renderer = builder.build(dev, _logger);
        if (!_renderer)
        {
            return false;
        }

        // 2. Camera Setup
        _camera_entity = _registry.create();
        const auto near_plane = (_model == scene_model::chess) ? 0.01F : 0.1F;
        _registry.assign(_camera_entity, render_system::camera_component{
            .aspect_ratio = 16.0F / 9.0F,
            .vertical_fov = 1.04719755F, // 60 degrees
            .near_plane = near_plane,
        });
        auto cam_tx = ecs::transform_component::identity();
        if (_model == scene_model::chess)
        {
            cam_tx.position({0.0F, 0.35F, -0.55F});
            cam_tx.rotation({math::as_radians(28.0F), 0.0F, 0.0F});
        }
        else
        {
            cam_tx.position({0.0F, 1.8F, -4.0F});
            cam_tx.rotation({0.0F, 0.0F, 0.0F});
        }
        _registry.assign(_camera_entity, cam_tx);
        _registry.assign(_camera_entity, render_system::active_camera_component{});

        // 3. Sun Light Setup
        auto sun = _registry.create();
        _registry.assign(sun, render_system::directional_light_component{
            .color = {1.0F, 0.98F, 0.92F},
            .intensity = (_model == scene_model::chess) ? 4.0F : 5.0F,
        });
        const auto max_shadow_dist = (_model == scene_model::chess) ? 2.0F : 100.0F;
        const auto normal_bias = (_model == scene_model::chess) ? 0.005F : 0.02F;
        const auto depth_bias = (_model == scene_model::chess) ? 0.001F : 0.005F;
        _registry.assign(sun, render_system::shadow_caster_component{
            .resolution = 2048,
            .num_cascades = 4,
            .split_lambda = 0.5F,
            .max_shadow_distance = max_shadow_dist,
            .normal_bias = normal_bias,
            .depth_bias = depth_bias,
            .debug_mode = render_system::shadow_debug_mode::none,
        });
        auto sun_tx = ecs::transform_component::identity();
        if (_model == scene_model::chess)
        {
            sun_tx.rotation({math::as_radians(65.0F), math::as_radians(25.0F), 0.0F});
        }
        else
        {
            sun_tx.rotation({math::as_radians(85.0F), math::as_radians(10.0F), 0.0F});
        }
        _registry.assign(sun, sun_tx);

        // 4. Asset Loading (glTF Model or Procedural Scene)
        auto asset_type_reg = assets::asset_type_registry{};
        auto asset_db = assets::asset_database{&asset_type_reg};
        assets::register_default_importers(asset_db, &_meshes, &_textures, &_materials);

        auto loaded_entities = vector<ecs::entity>{};
        const auto model_path = (_model == scene_model::chess)
                                    ? "vendor/glTF-Sample-Assets/Models/ABeautifulGame/glTF/ABeautifulGame.gltf"
                                    : "vendor/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf";
        if (std::filesystem::exists(model_path))
        {
            auto prefab_root = asset_db.load(model_path, _registry);
            if (prefab_root != ecs::tombstone)
            {
                _root_entity = prefab_root;
                if (!_registry.has<ecs::transform_component>(_root_entity))
                {
                    _registry.assign(_root_entity, ecs::transform_component::identity());
                }

                if (_registry.try_get<core::mesh_component>(prefab_root) != nullptr)
                {
                    loaded_entities.push_back(prefab_root);
                }
                for (auto ent : ecs::archetype_entity_hierarchy_view(_registry, prefab_root))
                {
                    if (_registry.try_get<core::mesh_component>(ent) != nullptr)
                    {
                        loaded_entities.push_back(ent);
                    }
                }
            }
        }

        if (loaded_entities.empty())
        {
            // Create Ground Floor
            auto ground_mesh_id = _meshes.register_mesh(create_plane_mesh());
            auto ground_mat = core::material{};
            ground_mat.set_vec4(core::material::base_color_factor_name, {0.4F, 0.45F, 0.5F, 1.0F});
            ground_mat.set_scalar(core::material::metallic_factor_name, 0.1F);
            ground_mat.set_scalar(core::material::roughness_factor_name, 0.8F);
            auto ground_mat_id = _materials.register_material(tempest::move(ground_mat));

            auto ground_ent = _registry.create();
            _registry.assign(ground_ent, render_system::renderable_component{
                .mesh_id = ground_mesh_id,
                .material_id = ground_mat_id,
                .double_sided = false,
            });
            _registry.assign(ground_ent, ecs::transform_component::identity());
            loaded_entities.push_back(ground_ent);

            // Create Array of Multi-Material Cubes
            auto cube_mesh_id = _meshes.register_mesh(create_cube_mesh());

            for (int i = -3; i <= 3; ++i)
            {
                for (int j = -2; j <= 2; ++j)
                {
                    auto cube_mat = core::material{};
                    const auto metallic = static_cast<float>(i + 3) / 6.0F;
                    const auto roughness = math::clamp(static_cast<float>(j + 2) / 4.0F, 0.05F, 1.0F);

                    cube_mat.set_vec4(core::material::base_color_factor_name,
                                      {0.9F * (1.0F - metallic), 0.7F, 0.2F + 0.8F * metallic, 1.0F});
                    cube_mat.set_scalar(core::material::metallic_factor_name, metallic);
                    cube_mat.set_scalar(core::material::roughness_factor_name, roughness);
                    auto cube_mat_id = _materials.register_material(tempest::move(cube_mat));

                    auto cube_ent = _registry.create();
                    _registry.assign(cube_ent, render_system::renderable_component{
                        .mesh_id = cube_mesh_id,
                        .material_id = cube_mat_id,
                        .double_sided = false,
                    });

                    auto tx = ecs::transform_component::identity();
                    tx.position({static_cast<float>(i) * 2.5F, 1.0F, static_cast<float>(j) * 2.5F});
                    tx.scale({0.8F, 0.8F, 0.8F});
                    _registry.assign(cube_ent, tx);
                    loaded_entities.push_back(cube_ent);
                }
            }
        }

        // 5. Upload scene geometries & build indirect buffers
        _renderer->upload_objects_sync(span<const ecs::entity>{loaded_entities.data(), loaded_entities.size()},
                                      _meshes, _textures, _materials);

        return true;
    }

    auto render_system_example::render(const frame_render_info& info) -> void
    {
        if (!_renderer)
        {
            return;
        }

        _time += 1.0F / 240.0F;

        if (_model == scene_model::chess)
        {
            // Rotate the chess scene while keeping the camera fixed
            if (_root_entity != ecs::tombstone)
            {
                auto root_tx = _registry.get<ecs::transform_component>(_root_entity);
                root_tx.rotation({0.0F, _time * 0.4F, 0.0F});
                _registry.assign_or_replace(_root_entity, root_tx);
            }
        }
        else
        {
            // Orbit Camera around Sponza interior
            if (_camera_entity != ecs::tombstone)
            {
                const auto cam_x = std::sin(_time * 0.3F) * 5.0F;
                const auto cam_z = -std::cos(_time * 0.3F) * 4.0F;
                const auto yaw = std::atan2(-cam_x, -cam_z);

                auto tx = _registry.get<ecs::transform_component>(_camera_entity);
                tx.position({cam_x, 1.8F, cam_z});
                tx.rotation({0.0F, yaw, 0.0F});
                _registry.assign_or_replace(_camera_entity, tx);
            }
        }

        // 1. Prepare frame graph
        _renderer->prepare_frame(info.width, info.height, info.swapchain_texture, info.swapchain_view);

        // 2. Submit frame with synchronization
        const auto sync = render_graph::frame_sync_options{
            .wait_semaphore = info.acquire_semaphore,
            .wait_stages = rhi::pipeline_stage::attachment_output,
            .signal_semaphore = info.render_semaphore,
            .signal_stages = rhi::pipeline_stage::bottom_of_pipe,
            .timeline_semaphore = info.timeline_semaphore,
            .timeline_value = info.timeline_value,
            .presented_texture = info.swapchain_texture,
        };

        [[maybe_unused]] const auto res = _renderer->render(sync);
    }

    auto render_system_example::on_resize([[maybe_unused]] rhi::device& dev,
                                          [[maybe_unused]] rhi::render_surface_format surface_format,
                                          uint32_t width, uint32_t height) -> void
    {
        if (_renderer)
        {
            _renderer->resize(width, height);
        }
    }

    auto render_system_example::shutdown(rhi::device& dev) -> void
    {
        dev.wait_idle();
        _renderer.reset();
    }
} // namespace tempest::rhi::examples
