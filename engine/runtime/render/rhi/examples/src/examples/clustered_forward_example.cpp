#include "clustered_forward_example.hpp"

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
        auto create_box_mesh(math::vec3<float> half_extents, math::vec4<float> color) -> core::mesh
        {
            auto m = core::mesh{};
            const auto hx = half_extents.x;
            const auto hy = half_extents.y;
            const auto hz = half_extents.z;

            auto add_face = [&](math::vec3<float> normal, math::vec3<float> tangent, math::vec3<float> v0,
                                math::vec3<float> v1, math::vec3<float> v2, math::vec3<float> v3) {
                const auto base_idx = static_cast<uint32_t>(m.vertices.size());
                m.vertices.push_back(core::vertex{.position = v0,
                                                  .uv = {0.0F, 0.0F},
                                                  .normal = normal,
                                                  .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                  .color = color});
                m.vertices.push_back(core::vertex{.position = v1,
                                                  .uv = {1.0F, 0.0F},
                                                  .normal = normal,
                                                  .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                  .color = color});
                m.vertices.push_back(core::vertex{.position = v2,
                                                  .uv = {1.0F, 1.0F},
                                                  .normal = normal,
                                                  .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                  .color = color});
                m.vertices.push_back(core::vertex{.position = v3,
                                                  .uv = {0.0F, 1.0F},
                                                  .normal = normal,
                                                  .tangent = {tangent.x, tangent.y, tangent.z, 1.0F},
                                                  .color = color});

                m.indices.push_back(base_idx + 0);
                m.indices.push_back(base_idx + 1);
                m.indices.push_back(base_idx + 2);
                m.indices.push_back(base_idx + 2);
                m.indices.push_back(base_idx + 3);
                m.indices.push_back(base_idx + 0);
            };

            // Front (+Z)
            add_face({0, 0, 1}, {1, 0, 0}, {-hx, -hy, hz}, {hx, -hy, hz}, {hx, hy, hz}, {-hx, hy, hz});
            // Back (-Z)
            add_face({0, 0, -1}, {-1, 0, 0}, {hx, -hy, -hz}, {-hx, -hy, -hz}, {-hx, hy, -hz}, {hx, hy, -hz});
            // Left (-X)
            add_face({-1, 0, 0}, {0, 0, 1}, {-hx, -hy, -hz}, {-hx, -hy, hz}, {-hx, hy, hz}, {-hx, hy, -hz});
            // Right (+X)
            add_face({1, 0, 0}, {0, 0, -1}, {hx, -hy, hz}, {hx, -hy, -hz}, {hx, hy, -hz}, {hx, hy, hz});
            // Top (+Y)
            add_face({0, 1, 0}, {1, 0, 0}, {-hx, hy, hz}, {hx, hy, hz}, {hx, hy, -hz}, {-hx, hy, -hz});
            // Bottom (-Y)
            add_face({0, -1, 0}, {1, 0, 0}, {-hx, -hy, -hz}, {hx, -hy, -hz}, {hx, -hy, hz}, {-hx, -hy, hz});

            return m;
        }

        auto create_plane_mesh(float width, float depth, float uv_scale = 1.0F,
                               math::vec4<float> color = {1.0F, 1.0F, 1.0F, 1.0F}) -> core::mesh
        {
            auto m = core::mesh{};
            const auto hw = width * 0.5F;
            const auto hd = depth * 0.5F;

            m.vertices.push_back(core::vertex{.position = {-hw, 0.0F, -hd},
                                              .uv = {0.0F, 0.0F},
                                              .normal = {0.0F, 1.0F, 0.0F},
                                              .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                              .color = color});
            m.vertices.push_back(core::vertex{.position = {hw, 0.0F, -hd},
                                              .uv = {uv_scale, 0.0F},
                                              .normal = {0.0F, 1.0F, 0.0F},
                                              .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                              .color = color});
            m.vertices.push_back(core::vertex{.position = {hw, 0.0F, hd},
                                              .uv = {uv_scale, uv_scale},
                                              .normal = {0.0F, 1.0F, 0.0F},
                                              .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                              .color = color});
            m.vertices.push_back(core::vertex{.position = {-hw, 0.0F, hd},
                                              .uv = {0.0F, uv_scale},
                                              .normal = {0.0F, 1.0F, 0.0F},
                                              .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                              .color = color});

            m.indices.push_back(0);
            m.indices.push_back(1);
            m.indices.push_back(2);
            m.indices.push_back(2);
            m.indices.push_back(3);
            m.indices.push_back(0);

            return m;
        }
    } // namespace

    auto clustered_forward_example::init(rhi::device& dev, rhi::render_surface_format surface_format) -> bool
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

        assets::mount_default_shader_roots(_asset_db);
        _asset_db.scan_and_index();

        // 1. Configure renderer
        auto builder = render_system::renderer::builder{};
        builder.set_config(render_system::renderer_config{
            .render_width = 1280,
            .render_height = 720,
            .tonemapped_color_format = target_format,
        });
        builder.set_inputs(render_system::renderer_inputs{
            .entity_registry = &_registry,
            .meshes = &_meshes,
            .textures = &_textures,
            .materials = &_materials,
            .asset_db = &_asset_db,
        });

        _renderer = builder.build(dev, _logger);
        if (!_renderer)
        {
            return false;
        }

        // 2. Camera Setup
        _camera_entity = _registry.create();
        _registry.assign(_camera_entity, render_system::camera_component{
                                             .aspect_ratio = 16.0F / 9.0F,
                                             .vertical_fov = 1.04719755F, // 60 degrees
                                             .near_plane = 0.1F,
                                         });
        auto cam_tx = ecs::transform_component::identity();
        cam_tx.position({0.0F, 1.8F, -4.0F});
        cam_tx.rotation({0.0F, 0.0F, 0.0F});
        _registry.assign(_camera_entity, cam_tx);

        // 3. Directional Sun Light Setup (Twilight / Sunset ambience)
        auto sun = _registry.create();
        _registry.assign(sun, render_system::directional_light_component{
                                  .color = {1.0F, 0.92F, 0.82F},
                                  .intensity = 2.5F,
                              });
        _registry.assign(sun, render_system::shadow_caster_component{
                                  .resolution = 2048,
                                  .num_cascades = 4,
                                  .split_lambda = 0.5F,
                                  .max_shadow_distance = 100.0F,
                                  .normal_bias = 0.02F,
                                  .depth_bias = 0.005F,
                                  .debug_mode = render_system::shadow_debug_mode::none,
                              });
        auto sun_tx = ecs::transform_component::identity();
        sun_tx.rotation({math::as_radians(70.0F), math::as_radians(25.0F), 0.0F});
        _registry.assign(sun, sun_tx);

        // 4. Asset Loading (Sponza glTF or Multi-Tier Procedural Fallback)
        auto asset_type_reg = assets::asset_type_registry{};
        auto asset_db = assets::asset_database{&asset_type_reg};
        assets::register_default_importers(asset_db, &_meshes, &_textures, &_materials);

        const auto model_path = "assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf";
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
            }
        }
        else
        {
            // Procedural Multi-Tier Architectural Hall Fallback (X = length 24m, Z = width 10m, Y = height 10m)

            // Floor Material & Mesh
            auto ground_mesh_id =
                _meshes.register_mesh(create_plane_mesh(24.0F, 48.0F, 12.0F, {0.85F, 0.85F, 0.88F, 1.0F}));
            auto ground_mat = core::material{};
            ground_mat.set_vec4(core::material::base_color_factor_name, {0.35F, 0.35F, 0.38F, 1.0F});
            ground_mat.set_scalar(core::material::metallic_factor_name, 0.05F);
            ground_mat.set_scalar(core::material::roughness_factor_name, 0.35F);
            auto ground_mat_id = _materials.register_material(tempest::move(ground_mat));

            auto ground_ent = _registry.create();
            _registry.assign(ground_ent, core::mesh_component{.mesh_id = ground_mesh_id});
            _registry.assign(ground_ent, core::material_component{.material_id = ground_mat_id});
            _registry.assign(ground_ent, ecs::transform_component::identity());

            // Ceiling Plane
            auto ceiling_mat = core::material{};
            ceiling_mat.set_vec4(core::material::base_color_factor_name, {0.25F, 0.22F, 0.20F, 1.0F});
            ceiling_mat.set_scalar(core::material::metallic_factor_name, 0.0F);
            ceiling_mat.set_scalar(core::material::roughness_factor_name, 0.8F);
            auto ceiling_mat_id = _materials.register_material(tempest::move(ceiling_mat));

            auto ceiling_ent = _registry.create();
            _registry.assign(ceiling_ent, core::mesh_component{.mesh_id = ground_mesh_id});
            _registry.assign(ceiling_ent, core::material_component{.material_id = ceiling_mat_id});
            auto ceiling_tx = ecs::transform_component::identity();
            ceiling_tx.position({0.0F, 8.5F, 0.0F});
            ceiling_tx.rotation({math::as_radians(180.0F), 0.0F, 0.0F});
            _registry.assign(ceiling_ent, ceiling_tx);

            // Pillar Material & Mesh
            auto pillar_mesh_id =
                _meshes.register_mesh(create_box_mesh({0.25F, 2.1F, 0.25F}, {0.95F, 0.92F, 0.88F, 1.0F}));
            auto pillar_mat = core::material{};
            pillar_mat.set_vec4(core::material::base_color_factor_name, {0.85F, 0.82F, 0.78F, 1.0F});
            pillar_mat.set_scalar(core::material::metallic_factor_name, 0.0F);
            pillar_mat.set_scalar(core::material::roughness_factor_name, 0.65F);
            auto pillar_mat_id = _materials.register_material(tempest::move(pillar_mat));

            // Upper Pillar Mesh
            auto upper_pillar_mesh_id =
                _meshes.register_mesh(create_box_mesh({0.2F, 1.9F, 0.2F}, {0.90F, 0.88F, 0.85F, 1.0F}));

            // Mezzanine Walkway Material & Mesh (longitudinal along X)
            auto mez_mesh_id =
                _meshes.register_mesh(create_box_mesh({12.0F, 0.12F, 1.2F}, {0.75F, 0.70F, 0.65F, 1.0F}));
            auto mez_mat = core::material{};
            mez_mat.set_vec4(core::material::base_color_factor_name, {0.65F, 0.60F, 0.55F, 1.0F});
            mez_mat.set_scalar(core::material::metallic_factor_name, 0.0F);
            mez_mat.set_scalar(core::material::roughness_factor_name, 0.5F);
            auto mez_mat_id = _materials.register_material(tempest::move(mez_mat));

            // Front / Back Mezzanine Walkways (along Z = +/- 3.2m)
            auto left_mez = _registry.create();
            _registry.assign(left_mez, core::mesh_component{.mesh_id = mez_mesh_id});
            _registry.assign(left_mez, core::material_component{.material_id = mez_mat_id});
            auto left_mez_tx = ecs::transform_component::identity();
            left_mez_tx.position({0.0F, 4.2F, -3.2F});
            _registry.assign(left_mez, left_mez_tx);

            auto right_mez = _registry.create();
            _registry.assign(right_mez, core::mesh_component{.mesh_id = mez_mesh_id});
            _registry.assign(right_mez, core::material_component{.material_id = mez_mat_id});
            auto right_mez_tx = ecs::transform_component::identity();
            right_mez_tx.position({0.0F, 4.2F, 3.2F});
            _registry.assign(right_mez, right_mez_tx);

            // Arch / Lintel Mesh
            auto arch_mesh_id =
                _meshes.register_mesh(create_box_mesh({1.4F, 0.2F, 0.25F}, {0.80F, 0.75F, 0.70F, 1.0F}));

            // Instantiate Colonnade Columns along longitudinal sides
            for (float side_z : {-2.2F, 2.2F})
            {
                for (int x_idx = -6; x_idx <= 6; ++x_idx)
                {
                    const auto x_pos = static_cast<float>(x_idx) * 1.8F;

                    // Ground-tier pillar (y = 0 to 4.2)
                    auto p_ent = _registry.create();
                    _registry.assign(p_ent, core::mesh_component{.mesh_id = pillar_mesh_id});
                    _registry.assign(p_ent, core::material_component{.material_id = pillar_mat_id});
                    auto p_tx = ecs::transform_component::identity();
                    p_tx.position({x_pos, 2.1F, side_z});
                    _registry.assign(p_ent, p_tx);

                    // Upper-tier pillar (y = 4.2 to 8.0)
                    auto up_ent = _registry.create();
                    _registry.assign(up_ent, core::mesh_component{.mesh_id = upper_pillar_mesh_id});
                    _registry.assign(up_ent, core::material_component{.material_id = pillar_mat_id});
                    auto up_tx = ecs::transform_component::identity();
                    up_tx.position({x_pos, 6.1F, side_z});
                    _registry.assign(up_ent, up_tx);

                    // Longitudinal Arch Beam
                    if (x_idx < 6)
                    {
                        auto arch_ent = _registry.create();
                        _registry.assign(arch_ent, core::mesh_component{.mesh_id = arch_mesh_id});
                        _registry.assign(arch_ent, core::material_component{.material_id = mez_mat_id});
                        auto arch_tx = ecs::transform_component::identity();
                        arch_tx.position({x_pos + 0.9F, 4.2F, side_z});
                        _registry.assign(arch_ent, arch_tx);
                    }
                }
            }

            // End Walls (along X = +/- 12.0m)
            auto wall_mesh_id =
                _meshes.register_mesh(create_box_mesh({0.5F, 4.25F, 5.0F}, {0.70F, 0.68F, 0.65F, 1.0F}));
            for (float x_wall : {-12.0F, 12.0F})
            {
                auto wall_ent = _registry.create();
                _registry.assign(wall_ent, core::mesh_component{.mesh_id = wall_mesh_id});
                _registry.assign(wall_ent, core::material_component{.material_id = mez_mat_id});
                auto wall_tx = ecs::transform_component::identity();
                wall_tx.position({x_wall, 4.25F, 0.0F});
                _registry.assign(wall_ent, wall_tx);
            }
        }

        // 5. Procedural 128 Point Light Generation (Multi-Tiered Palette)
        _light_entities.clear();
        _light_entities.reserve(128);

        for (size_t i = 0; i < 128; ++i)
        {
            auto light_ent = _registry.create();

            math::vec3<float> color;
            float range = 5.0F;
            float intensity = 25.0F;

            if (i < 50)
            {
                // Ground Tier: Warm torches, golden candles, ruby lanterns, warm coral
                static constexpr math::vec3<float> ground_palette[] = {
                    {1.0F, 0.52F, 0.12F}, // Torch Amber
                    {1.0F, 0.82F, 0.25F}, // Golden Candle
                    {1.0F, 0.18F, 0.15F}, // Ruby Lantern
                    {1.0F, 0.38F, 0.22F}, // Warm Coral
                };
                color = ground_palette[i % 4];
                range = 4.0F + static_cast<float>(i % 5) * 0.6F;
                intensity = 20.0F + static_cast<float>(i % 3) * 5.0F;
            }
            else if (i < 96)
            {
                // Mezzanine Tier: Cool lanterns, sapphire cyan, emerald green, royal amethyst
                static constexpr math::vec3<float> mez_palette[] = {
                    {0.15F, 0.65F, 1.0F},  // Sapphire Cyan
                    {0.85F, 0.20F, 0.95F}, // Royal Amethyst
                    {0.15F, 0.95F, 0.40F}, // Emerald Green
                    {0.25F, 0.85F, 1.0F},  // Azure
                };
                color = mez_palette[(i - 50) % 4];
                range = 3.5F + static_cast<float>((i - 50) % 4) * 0.6F;
                intensity = 18.0F + static_cast<float>((i - 50) % 4) * 4.0F;
            }
            else
            {
                // High Arch Tier: High-luminance accent colors
                static constexpr math::vec3<float> arch_palette[] = {
                    {1.0F, 0.55F, 0.15F},  // Warm Amber
                    {0.15F, 0.75F, 1.0F},  // Brilliant Cyan
                    {0.90F, 0.15F, 0.80F}, // Magenta
                    {0.20F, 0.95F, 0.35F}, // Emerald
                };
                color = arch_palette[(i - 96) % 4];
                range = 4.5F + static_cast<float>((i - 96) % 4) * 0.7F;
                intensity = 24.0F + static_cast<float>((i - 96) % 3) * 5.0F;
            }

            _registry.assign(light_ent, render_system::point_light_component{
                                            .color = color,
                                            .intensity = intensity,
                                            .range = range,
                                        });

            auto tx = ecs::transform_component::identity();
            tx.position({0.0F, 1.8F, 0.0F});
            _registry.assign(light_ent, tx);

            _light_entities.push_back(light_ent);
        }

        return true;
    }

    auto clustered_forward_example::render(const frame_render_info& info) -> void
    {
        if (!_renderer)
        {
            return;
        }

        _time += 1.0F / 120.0F;

        // 1. Procedural 128 Point Light Dynamics & ECS Event Dirty Tracking
        for (size_t i = 0; i < _light_entities.size(); ++i)
        {
            auto tx = _registry.get<ecs::transform_component>(_light_entities[i]);
            const auto fi = static_cast<float>(i);
            const auto phase = fi * 0.25F;

            math::vec3<float> pos;
            if (i < 50)
            {
                // Ground tier: Lissajous orbit winding along length of atrium and side aisles
                const auto t = _time * 0.7F + phase;
                const auto is_side_aisle = (i % 2 == 1);
                const auto side_sign = ((i / 2) % 2 == 0) ? 1.0F : -1.0F;

                const auto z_val =
                    is_side_aisle ? side_sign * (3.0F + std::sin(t * 0.9F) * 0.8F) : std::sin(t * 0.8F) * 1.5F;

                pos = math::vec3<float>{
                    std::sin(t * 0.5F + fi * 0.12F) * 9.5F,
                    0.8F + std::abs(std::sin(t * 1.3F)) * 2.2F,
                    z_val,
                };
            }
            else if (i < 96)
            {
                // Mezzanine tier: Harmonic orbits traversing upper galleries
                const auto t = _time * 0.6F + phase;
                const auto side_sign = (i % 2 == 0) ? 1.0F : -1.0F;
                pos = math::vec3<float>{
                    std::cos(t * 0.45F + fi * 0.15F) * 9.0F,
                    4.8F + std::sin(t * 1.1F) * 1.2F,
                    side_sign * (2.8F + std::sin(t * 0.8F) * 0.8F),
                };
            }
            else
            {
                // High arch tier: Swinging across the upper atrium vault
                const auto t = _time * 0.85F + phase;
                pos = math::vec3<float>{
                    std::sin(t * 0.6F + fi * 0.2F) * 8.0F,
                    6.6F + std::abs(std::sin(t * 1.4F)) * 1.0F,
                    std::cos(t * 0.7F) * 1.6F,
                };
            }

            tx.position(pos);
            _registry.assign_or_replace(_light_entities[i], tx);
        }

        // 2. Orbiting & Touring Camera along Ground and 2nd Floor Mezzanine
        if (_camera_entity != ecs::tombstone)
        {
            const auto t = _time * 0.15F;
            const auto cam_x = std::sin(t) * 8.0F;
            // Oscillate altitude smoothly between ground eye-level (1.6m) and mezzanine (5.4m)
            const auto cam_y = 1.6F + 3.8F * (0.5F + 0.5F * std::sin(t * 0.5F));
            // Weave down central aisle, staying safely within open space (|Z| <= 1.2m)
            const auto cam_z = std::sin(t * 2.0F) * 1.2F;

            // Look target placed dynamically along camera trajectory
            const auto target_t = t + 0.35F;
            const auto target_x = std::sin(target_t) * 6.5F;
            const auto target_y = cam_y * 0.7F + 1.2F;
            const auto target_z = std::sin(target_t * 2.0F) * 0.6F;

            const auto dir_x = target_x - cam_x;
            const auto dir_y = target_y - cam_y;
            const auto dir_z = target_z - cam_z;
            const auto len_xz = std::sqrt(dir_x * dir_x + dir_z * dir_z);

            const auto yaw = std::atan2(-dir_x, -dir_z);
            const auto pitch = std::atan2(-dir_y, len_xz);

            auto tx = _registry.get<ecs::transform_component>(_camera_entity);
            tx.position({cam_x, cam_y, cam_z});
            tx.rotation({pitch, yaw, 0.0F});
            _registry.assign_or_replace(_camera_entity, tx);
        }

        // 3. Prepare frame graph
        _renderer->prepare_frame(info.width, info.height, info.swapchain_texture, info.swapchain_view);

        // 4. Submit frame with synchronization
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

    auto clustered_forward_example::on_resize([[maybe_unused]] rhi::device& dev,
                                              [[maybe_unused]] rhi::render_surface_format surface_format,
                                              uint32_t width, uint32_t height) -> void
    {
        if (_renderer)
        {
            _renderer->resize(width, height);
        }
    }

    auto clustered_forward_example::shutdown(rhi::device& dev) -> void
    {
        dev.wait_idle();
        _renderer.reset();
    }
} // namespace tempest::rhi::examples
