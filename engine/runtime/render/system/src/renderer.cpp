#include <tempest/render_system/renderer.hpp>

#include <bit>
#include <cmath>
#include <format>
#include <tempest/algorithm.hpp>
#include <tempest/limits.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/render_system/passes/depth_prepass.hpp>
#include <tempest/render_system/passes/frame_upload_pass.hpp>
#include <tempest/render_system/passes/light_clustering_pass.hpp>
#include <tempest/render_system/passes/light_culling_pass.hpp>
#include <tempest/render_system/passes/pbr_opaque_pass.hpp>
#include <tempest/render_system/passes/shadow_pass.hpp>
#include <tempest/render_system/passes/skybox_pass.hpp>
#include <tempest/render_system/passes/ssao_blur_pass.hpp>
#include <tempest/render_system/passes/ssao_pass.hpp>
#include <tempest/render_system/passes/tonemapping_pass.hpp>
#include <tempest/render_system/passes/transparency_blend_pass.hpp>
#include <tempest/render_system/passes/transparency_clear_pass.hpp>
#include <tempest/render_system/passes/transparency_gather_pass.hpp>
#include <tempest/render_system/passes/transparency_resolve_pass.hpp>
#include <tempest/render_system/shadow_atlas_math.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transformations.hpp>

namespace tempest::render_system
{
    namespace
    {
        auto compute_world_matrix(const ecs::archetype_registry& reg, ecs::entity ent) -> math::mat4<float>
        {
            auto world = math::mat4<float>{1.0F};
            auto curr = ent;
            while (curr != ecs::tombstone)
            {
                if (const auto* tx = reg.try_get<ecs::transform_component>(curr))
                {
                    world = tx->matrix() * world;
                }
                if (const auto* rel = reg.try_get<ecs::relationship_component<ecs::entity>>(curr))
                {
                    curr = rel->parent;
                }
                else
                {
                    break;
                }
            }
            return world;
        }

        auto calculate_directional_shadow_atlas_dimensions(const ecs::archetype_registry* registry,
                                                           uint32_t max_atlas_dim, logger* log) -> shadow_atlas_plan
        {
            constexpr auto shadow_padding = 4U;

            if (registry != nullptr)
            {
                auto best_priority = numeric_limits<uint32_t>::max();
                auto has_caster = false;
                auto cascade_res = 2048U;
                auto cascade_count = 4U;

                for (const auto [self, dl, tx] :
                     registry->with<ecs::self_component, directional_light_component, ecs::transform_component>())
                {
                    const auto* sc = registry->try_get<shadow_caster_component>(self.entity);
                    const auto priority = sc ? sc->priority : 0U;

                    if (!has_caster || priority < best_priority)
                    {
                        has_caster = true;
                        best_priority = priority;
                        if (sc != nullptr)
                        {
                            cascade_res = sc->resolution;
                            cascade_count = math::clamp(sc->num_cascades, 1U, 4U);
                        }
                    }
                }

                if (has_caster && cascade_res > 0 && cascade_count > 0)
                {
                    const auto plan = calculate_directional_shadow_atlas_plan(cascade_res, cascade_count, max_atlas_dim,
                                                                              shadow_padding);
                    if (log && plan.was_clamped)
                    {
                        const auto msg = std::format(
                            "Directional shadow cascade resolution clamped from {} to {} to satisfy device limit {}.",
                            cascade_res, plan.effective_cascade_resolution, max_atlas_dim);
                        log->warn(string_view{msg.data(), msg.size()});
                    }
                    return plan;
                }
            }

            return calculate_directional_shadow_atlas_plan(2048U, 0U, max_atlas_dim, shadow_padding);
        }
    } // namespace
    auto renderer::builder::build(rhi::device& dev, logger& log) -> unique_ptr<renderer>
    {
        auto owned_camera_sys = unique_ptr<camera_system>{};
        if (_inputs.camera_sys == nullptr && _inputs.entity_registry != nullptr)
        {
            owned_camera_sys = make_unique<camera_system>(*_inputs.entity_registry);
            _inputs.camera_sys = owned_camera_sys.get();
        }

        return make_unique<renderer>(dev, log, _cfg, _inputs, tempest::move(owned_camera_sys));
    }

    renderer::renderer(rhi::device& dev, logger& log, renderer_config cfg, renderer_inputs inputs,
                       unique_ptr<camera_system> camera_sys)
        : _device{&dev}, _log{&log}, _cfg{cfg}, _inputs{inputs}, _owned_camera_system{tempest::move(camera_sys)},
          _camera_system{_inputs.camera_sys ? _inputs.camera_sys : _owned_camera_system.get()},
          _pool{dev, cfg.pool_config}, _shaders{dev}, _graph{cfg.render_width, cfg.render_height},
          _shadow_debug_mode{cfg.shadow_debug}
    {
        _graph.get_allocator().set_frames_in_flight(_cfg.pool_config.frames_in_flight);
        if (_inputs.entity_registry != nullptr)
        {
            _events = &_inputs.entity_registry->event_registry();
            _subscribe_events();
            _init_renderables_from_registry();
            _init_lights_from_registry();
        }
    }

    renderer::~renderer()
    {
        _unsubscribe_events();
        if (_device)
        {
            _graph.get_allocator().release_all(*_device);
        }
    }

    renderer::renderer(renderer&& other) noexcept
        : _device{other._device}, _log{other._log}, _cfg{other._cfg}, _inputs{other._inputs},
          _owned_camera_system{tempest::move(other._owned_camera_system)}, _camera_system{other._camera_system},
          _pool{tempest::move(other._pool)}, _shaders{tempest::move(other._shaders)},
          _graph{tempest::move(other._graph)}, _directional_shadow_atlas_target{other._directional_shadow_atlas_target},
          _punctual_shadow_atlas_target{other._punctual_shadow_atlas_target},
          _hdr_color_target{other._hdr_color_target}, _depth_target{other._depth_target},
          _ssao_target{other._ssao_target}, _ssao_blurred_target{other._ssao_blurred_target},
          _moments_target{other._moments_target}, _zeroth_moment_target{other._zeroth_moment_target},
          _transparency_accum_target{other._transparency_accum_target},
          _tonemapped_color_target{other._tonemapped_color_target},
          _cluster_bounds_target{other._cluster_bounds_target}, _light_bitmask_target{other._light_bitmask_target},
          _directional_shadow_allocator{tempest::move(other._directional_shadow_allocator)},
          _punctual_shadow_allocator{tempest::move(other._punctual_shadow_allocator)},
          _tracked_entities{tempest::move(other._tracked_entities)}, _active_draw_count{other._active_draw_count},
          _opaque_draw_count{other._opaque_draw_count}, _opaque_draw_offset{other._opaque_draw_offset},
          _transparent_draw_count{other._transparent_draw_count},
          _transparent_draw_offset{other._transparent_draw_offset}, _shadow_debug_mode{other._shadow_debug_mode},
          _renderable_indices{tempest::move(other._renderable_indices)},
          _renderables_dirty_count{other._renderables_dirty_count}, _events{other._events},
          _point_light_indices{tempest::move(other._point_light_indices)},
          _point_light_entities{tempest::move(other._point_light_entities)},
          _cached_lights{tempest::move(other._cached_lights)}, _lights_dirty_count{other._lights_dirty_count}
    {
        other._unsubscribe_events();
        other._device = nullptr;
        other._camera_system = nullptr;
        other._events = nullptr;

        _subscribe_events();
    }

    renderer& renderer::operator=(renderer&& other) noexcept
    {
        if (this != &other)
        {
            _unsubscribe_events();
            if (_device)
            {
                _graph.get_allocator().release_all(*_device);
            }

            other._unsubscribe_events();

            _device = other._device;
            _log = other._log;
            _cfg = other._cfg;
            _inputs = other._inputs;
            _owned_camera_system = tempest::move(other._owned_camera_system);
            _camera_system = other._camera_system;
            _pool = tempest::move(other._pool);
            _shaders = tempest::move(other._shaders);
            _graph = tempest::move(other._graph);
            _directional_shadow_atlas_target = other._directional_shadow_atlas_target;
            _punctual_shadow_atlas_target = other._punctual_shadow_atlas_target;
            _hdr_color_target = other._hdr_color_target;
            _depth_target = other._depth_target;
            _ssao_target = other._ssao_target;
            _ssao_blurred_target = other._ssao_blurred_target;
            _moments_target = other._moments_target;
            _zeroth_moment_target = other._zeroth_moment_target;
            _transparency_accum_target = other._transparency_accum_target;
            _tonemapped_color_target = other._tonemapped_color_target;
            _cluster_bounds_target = other._cluster_bounds_target;
            _light_bitmask_target = other._light_bitmask_target;
            _directional_shadow_allocator = tempest::move(other._directional_shadow_allocator);
            _punctual_shadow_allocator = tempest::move(other._punctual_shadow_allocator);
            _tracked_entities = tempest::move(other._tracked_entities);
            _active_draw_count = other._active_draw_count;
            _opaque_draw_count = other._opaque_draw_count;
            _opaque_draw_offset = other._opaque_draw_offset;
            _transparent_draw_count = other._transparent_draw_count;
            _transparent_draw_offset = other._transparent_draw_offset;
            _shadow_debug_mode = other._shadow_debug_mode;
            _renderable_indices = tempest::move(other._renderable_indices);
            _renderables_dirty_count = other._renderables_dirty_count;
            _point_light_indices = tempest::move(other._point_light_indices);
            _point_light_entities = tempest::move(other._point_light_entities);
            _cached_lights = tempest::move(other._cached_lights);
            _lights_dirty_count = other._lights_dirty_count;
            _events = other._events;

            other._device = nullptr;
            other._camera_system = nullptr;
            other._events = nullptr;

            _subscribe_events();
        }
        return *this;
    }

    void renderer::_ensure_assets_loaded()
    {
        if (_inputs.entity_registry == nullptr)
        {
            return;
        }

        auto mesh_ids = vector<guid>{};
        auto mat_ids = vector<guid>{};

        for (const auto entity : _tracked_entities)
        {
            if (const auto* mc = _inputs.entity_registry->try_get<core::mesh_component>(entity))
            {
                if (!_pool.get_mesh_layout(mc->mesh_id).has_value())
                {
                    mesh_ids.push_back(mc->mesh_id);
                }
            }

            if (const auto* matc = _inputs.entity_registry->try_get<core::material_component>(entity))
            {
                if (!_pool.get_material(matc->material_id).has_value())
                {
                    mat_ids.push_back(matc->material_id);
                }
            }
        }

        auto tex_ids = vector<guid>{};
        if (_inputs.materials)
        {
            for (const auto& mat_id : mat_ids)
            {
                if (auto mat_opt = _inputs.materials->find(mat_id))
                {
                    const auto& m = *mat_opt;
                    auto check_tex = [&](const string& tex_name) {
                        if (auto t = m.get_texture(tex_name))
                        {
                            if (_pool.get_texture_descriptor_index(*t) == -1)
                            {
                                tex_ids.push_back(*t);
                            }
                        }
                    };
                    check_tex(core::material::base_color_texture_name);
                    check_tex(core::material::normal_texture_name);
                    check_tex(core::material::metallic_roughness_texture_name);
                    check_tex(core::material::emissive_texture_name);
                    check_tex(core::material::occlusion_texture_name);
                    check_tex(core::material::transmissive_texture_name);
                    check_tex(core::material::volume_thickness_texture_name);
                }
            }
        }

        if (!tex_ids.empty() && _inputs.textures)
        {
            _pool.load_textures(tex_ids, *_inputs.textures, _graph);
        }
        if (!mat_ids.empty() && _inputs.materials)
        {
            _pool.load_materials(mat_ids, *_inputs.materials, _graph);
        }
        if (!mesh_ids.empty() && _inputs.meshes)
        {
            _pool.load_meshes(mesh_ids, *_inputs.meshes, _graph);
        }
    }

    void renderer::_update_renderable_commands()
    {
        if (_inputs.entity_registry == nullptr)
        {
            return;
        }

        // Partition entities into Opaque and Transparent batches based on material type
        auto opaque_entities = vector<ecs::entity>{};
        auto transparent_entities = vector<ecs::entity>{};
        opaque_entities.reserve(_tracked_entities.size());
        transparent_entities.reserve(_tracked_entities.size());

        for (const auto entity : _tracked_entities)
        {
            auto mesh_id = guid{};
            auto mat_id = guid{};

            if (const auto* mc = _inputs.entity_registry->try_get<core::mesh_component>(entity))
            {
                mesh_id = mc->mesh_id;
                if (const auto* matc = _inputs.entity_registry->try_get<core::material_component>(entity))
                {
                    mat_id = matc->material_id;
                }
            }
            else
            {
                continue;
            }

            if (!_inputs.entity_registry->try_get<ecs::transform_component>(entity))
            {
                continue;
            }

            if (!_pool.get_mesh_layout(mesh_id).has_value())
            {
                continue;
            }

            const auto mat_type = _pool.get_material_type(mat_id).value_or(material_type::opaque);
            if (mat_type == material_type::blend || mat_type == material_type::transmissive)
            {
                transparent_entities.push_back(entity);
            }
            else
            {
                opaque_entities.push_back(entity);
            }
        }

        _tracked_entities.clear();
        _tracked_entities.reserve(opaque_entities.size() + transparent_entities.size());
        _tracked_entities.insert(_tracked_entities.end(), opaque_entities.begin(), opaque_entities.end());
        _tracked_entities.insert(_tracked_entities.end(), transparent_entities.begin(), transparent_entities.end());

        _renderable_indices.clear();
        for (size_t i = 0; i < _tracked_entities.size(); ++i)
        {
            _renderable_indices[_tracked_entities[i]] = i;
        }

        // Build dynamic Instance Indices and Indirect Draw Commands
        auto instances = vector<uint32_t>{};
        instances.reserve(_tracked_entities.size());
        auto commands = vector<indexed_indirect_command>{};
        commands.reserve(_tracked_entities.size());

        for (const auto entity : _tracked_entities)
        {
            auto mesh_id = guid{};
            if (const auto* mc = _inputs.entity_registry->try_get<core::mesh_component>(entity))
            {
                mesh_id = mc->mesh_id;
            }

            const auto ml_opt = _pool.get_mesh_layout(mesh_id);
            if (!ml_opt)
            {
                continue;
            }
            const auto& ml = *ml_opt;

            const auto idx = static_cast<uint32_t>(instances.size());
            instances.push_back(idx);

            auto cmd = indexed_indirect_command{
                .index_count = ml.index_count,
                .instance_count = 1,
                .first_index = (ml.mesh_start_offset + ml.index_offset) / static_cast<uint32_t>(sizeof(uint32_t)),
                .vertex_offset = 0,
                .first_instance = idx,
            };
            commands.push_back(cmd);
        }

        _opaque_draw_count = static_cast<uint32_t>(opaque_entities.size());
        _opaque_draw_offset = 0;
        _transparent_draw_count = static_cast<uint32_t>(transparent_entities.size());
        _transparent_draw_offset = _opaque_draw_count;
        _active_draw_count = static_cast<uint32_t>(commands.size());

        _pool.write_instances(instances);
        _pool.write_draw_commands(commands);
        --_renderables_dirty_count;
    }

    void renderer::prepare_frame(uint32_t width, uint32_t height, optional<rhi::texture_handle> swapchain_tex,
                                 optional<rhi::texture_view_handle> swapchain_view,
                                 optional<render_camera> camera_override)
    {
        _shaders.process_deferred_retirements();
        _pool.advance_frame();
        _graph.get_allocator().set_frames_in_flight(_cfg.pool_config.frames_in_flight);
        _graph.get_allocator().set_frame_slot(_pool.get_frame_slot());
        _graph.reset();
        _graph.set_surface_size(width, height);

        // 1. Automatically load any missing mesh/material/texture assets needed by tracked renderables
        _ensure_assets_loaded();

        // 2. Automatically update draw commands and instance buffer if renderables changed
        if (_renderables_dirty_count > 0 && _inputs.entity_registry != nullptr)
        {
            _update_renderable_commands();
        }

        // 3. Automatically update dynamic object transforms from entity registry
        if (_inputs.entity_registry && !_tracked_entities.empty())
        {
            auto objects = vector<object_payload>{};
            objects.reserve(_tracked_entities.size());

            for (const auto entity : _tracked_entities)
            {
                auto mesh_id = guid{};
                auto mat_id = guid{};

                if (const auto* mc = _inputs.entity_registry->try_get<core::mesh_component>(entity))
                {
                    mesh_id = mc->mesh_id;
                    if (const auto* matc = _inputs.entity_registry->try_get<core::material_component>(entity))
                    {
                        mat_id = matc->material_id;
                    }
                }
                else
                {
                    continue;
                }

                const auto* t = _inputs.entity_registry->try_get<ecs::transform_component>(entity);
                if (!t)
                {
                    continue;
                }

                const auto mesh_gpu_addr = _pool.get_mesh_address(mesh_id);
                const auto mat_gpu_addr = _pool.get_material_address(mat_id);

                const auto model_mat = compute_world_matrix(*_inputs.entity_registry, entity);
                const auto inv_model = math::inverse(model_mat);
                const auto inv_trans = math::transpose(inv_model);

                auto payload = object_payload{
                    .model = model_mat,
                    .inv_transpose_model = inv_trans,
                    .mesh_gpu_address = mesh_gpu_addr,
                    .material_gpu_address = mat_gpu_addr,
                    .parent_gpu_address = 0,
                    .self_id = static_cast<uint32_t>(entity),
                    .padding = 0,
                };

                objects.push_back(payload);
            }

            _pool.write_objects(objects);
        }

        // 2. Automatically update dynamic lights from entity registry
        if (_lights_dirty_count > 0 && _inputs.entity_registry != nullptr)
        {
            _cached_lights.clear();
            _cached_lights.reserve(_point_light_entities.size());

            for (const auto entity : _point_light_entities)
            {
                const auto* const pl = _inputs.entity_registry->try_get<point_light_component>(entity);
                if (!pl)
                {
                    continue;
                }

                auto world_pos = math::vec3<float>{0.0F, 0.0F, 0.0F};
                if (_inputs.entity_registry->try_get<ecs::transform_component>(entity))
                {
                    const auto world_mat = compute_world_matrix(*_inputs.entity_registry, entity);
                    world_pos = math::vec3<float>{world_mat[3][0], world_mat[3][1], world_mat[3][2]};
                }

                auto payload = light_payload{
                    .color_intensity = {pl->color.x, pl->color.y, pl->color.z, pl->intensity},
                    .position_falloff = {world_pos.x, world_pos.y, world_pos.z, pl->range},
                    .direction_angle = {0.0F, -1.0F, 0.0F, 0.0F},
                    .type = 1,
                    .enabled = 1,
                    .padding = {0, 0},
                };
                _cached_lights.push_back(payload);
            }

            _pool.write_lights(_cached_lights);
            --_lights_dirty_count;
        }

        // Update Scene Globals
        auto scene = scene_constants{
            .projection = math::mat4<float>{1.0F},
            .inv_projection = math::mat4<float>{1.0F},
            .view = math::mat4<float>{1.0F},
            .inv_view = math::mat4<float>{1.0F},
            .camera_position = {0.0F, 0.0F, 0.0F, 1.0F},
            .ambient_light = {0.28F, 0.30F, 0.36F, 1.0F},
            .sun_color_intensity = {1.0F, 1.0F, 1.0F, 2.0F},
            .sun_direction = {0.0F, -1.0F, 0.0F, 0.0F},
            .screen_size = {static_cast<float>(width), static_cast<float>(height)},
            .inv_screen_size = {1.0F / static_cast<float>(width), 1.0F / static_cast<float>(height)},
        };

        auto near_plane = 0.1F;
        auto active_cam_opt = camera_override.has_value()
                                  ? camera_override
                                  : (_camera_system ? _camera_system->get_active_camera() : tempest::nullopt);

        if (active_cam_opt.has_value())
        {
            const auto& cam = *active_cam_opt;
            scene.view = cam.view;
            scene.inv_view = cam.inv_view;
            scene.projection = cam.proj;
            scene.inv_projection = cam.inv_proj;
            scene.camera_position = cam.eye_position;
        }

        if (camera_override.has_value() && camera_override->proj[3][2] > 0.0F)
        {
            near_plane = camera_override->proj[3][2];
        }
        else if (!camera_override.has_value() && _camera_system && _inputs.entity_registry)
        {
            auto cam_ent_opt = _camera_system->get_active_camera_entity();
            if (cam_ent_opt.has_value())
            {
                if (const auto* c = _inputs.entity_registry->try_get<camera_component>(*cam_ent_opt))
                {
                    near_plane = c->near_plane;
                }
            }
        }

        const auto far_plane = _cfg.cluster_far_plane;
        const auto valid_near = near_plane > 0.0F ? near_plane : 0.1F;
        const auto log_far_near = std::log(far_plane / valid_near);

        const auto grid_dims = compute_cluster_grid_dimensions(width, height);
        const auto total_clusters = grid_dims.x * grid_dims.y * grid_dims.z;
        const auto words_per_cluster = (static_cast<uint32_t>(_cached_lights.size()) + 31U) / 32U;

        scene.lights_address = _pool.get_lights_buffer_address();
        scene.light_bitmask_address = 0;
        scene.light_count = static_cast<uint32_t>(_cached_lights.size());
        scene.words_per_cluster = words_per_cluster;
        scene.cluster_counts_tile_size = grid_dims;
        scene.cluster_depth_params = {valid_near, far_plane, log_far_near, 0.0F};

        // Query directional sun light
        if (_inputs.entity_registry)
        {
            _inputs.entity_registry->each([&scene]([[maybe_unused]] const ecs::self_component& self,
                                                   const directional_light_component& dl,
                                                   const ecs::transform_component& tx) {
                const auto rot = math::quat(tx.rotation());
                const auto forward = math::extract_forward(rot);
                scene.sun_direction = {forward.x, forward.y, forward.z, 0.0F};
                scene.sun_color_intensity = {dl.color.x, dl.color.y, dl.color.z, dl.intensity};
            });
        }

        _pool.write_scene_constants(scene);

        // Create Transient Render Targets
        const auto max_image_dim = _device ? _device->get_device_desc().limits.max_image_dimension_2d : 8192U;
        const auto dir_shadow_plan =
            calculate_directional_shadow_atlas_dimensions(_inputs.entity_registry, max_image_dim, _log);
        _directional_shadow_allocator.reset(dir_shadow_plan.atlas_size.x, dir_shadow_plan.atlas_size.y, 4);
        _directional_shadow_atlas_target = _graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(dir_shadow_plan.atlas_size.x, dir_shadow_plan.atlas_size.y),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "DirectionalShadowAtlasTarget",
        });

        const auto punctual_atlas_dim = max_image_dim > 0 ? tempest::min(max_image_dim, 4096U) : 4096U;
        _punctual_shadow_allocator.reset(punctual_atlas_dim, punctual_atlas_dim, 4);
        _punctual_shadow_atlas_target = _graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(punctual_atlas_dim, punctual_atlas_dim),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "PunctualShadowAtlasTarget",
        });

        _hdr_color_target = _graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = _cfg.hdr_color_format,
            .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "HDRColorTarget",
        });

        _depth_target = _graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = _cfg.depth_format,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "DepthTarget",
        });

        if (_cfg.enable_ssao)
        {
            _ssao_target = _graph.create_texture(render_graph::rg_texture_desc{
                .size = render_graph::rg_texture_size::absolute(width, height),
                .format = rhi::data_format::r8_unorm,
                .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
                .mip_levels = 1,
                .array_layers = 1,
                .name = "SSAORawTarget",
            });

            _ssao_blurred_target = _graph.create_texture(render_graph::rg_texture_desc{
                .size = render_graph::rg_texture_size::absolute(width, height),
                .format = rhi::data_format::r8_unorm,
                .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
                .mip_levels = 1,
                .array_layers = 1,
                .name = "SSAOBlurredTarget",
            });
        }

        _moments_target = _graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 2,
            .name = "MBOITMomentsTarget",
        });

        _zeroth_moment_target = _graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::r32_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "MBOITZerothMomentTarget",
        });

        _transparency_accum_target = _graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "TransparencyAccumTarget",
        });

        if (swapchain_tex.has_value() && swapchain_view.has_value())
        {
            _tonemapped_color_target =
                _graph.import_texture(*swapchain_tex, *swapchain_view, rhi::image_layout::undefined);
        }
        else
        {
            _tonemapped_color_target = _graph.create_texture(render_graph::rg_texture_desc{
                .size = render_graph::rg_texture_size::absolute(width, height),
                .format = _cfg.tonemapped_color_format,
                .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled |
                         rhi::texture_usage::transfer_src,
                .mip_levels = 1,
                .array_layers = 1,
                .name = "TonemappedColorTarget",
            });
        }

        _cluster_bounds_target = _graph.create_buffer(render_graph::rg_buffer_desc{
            .size = total_clusters * sizeof(cluster_bounds),
            .usage =
                rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address | rhi::buffer_usage::transfer_src,
            .name = "ClusterBoundsBuffer",
        });

        const auto bitmask_buffer_size = tempest::max(1U, total_clusters * words_per_cluster) * sizeof(uint32_t);
        _light_bitmask_target = _graph.create_buffer(render_graph::rg_buffer_desc{
            .size = bitmask_buffer_size,
            .usage =
                rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address | rhi::buffer_usage::transfer_src,
            .name = "LightBitmaskBuffer",
        });

        // Build DAG Passes
        add_frame_upload_pass(_graph, _pool);

        // Clustered Lighting Passes
        const auto active_cam = render_camera{
            .proj = scene.projection,
            .inv_proj = scene.inv_projection,
            .view = scene.view,
            .inv_view = scene.inv_view,
            .eye_position = scene.camera_position,
        };
        const auto& cluster_data =
            add_light_clustering_pass(_graph, _pool, _shaders, _cluster_bounds_target, active_cam, width, height,
                                      grid_dims.x, grid_dims.y, grid_dims.z);

        auto lights_buf = _graph.import_buffer(_pool.get_lights_buffer());
        const auto& culling_data =
            add_light_culling_pass(_graph, _pool, _shaders, cluster_data.cluster_bounds_buffer, lights_buf,
                                   cluster_data.create_info, scene.light_count, _light_bitmask_target);

        if (_inputs.entity_registry)
        {
            auto shadow_res = add_shadow_pass(shadow_pass_params{
                .graph = _graph,
                .pool = _pool,
                .shaders = _shaders,
                .shadow_atlas = _directional_shadow_atlas_target,
                .allocator = _directional_shadow_allocator,
                .registry = *_inputs.entity_registry,
                .camera_sys = _camera_system,
                .camera_override = camera_override,
                .draw_count = _opaque_draw_count,
                .draw_offset = _opaque_draw_offset,
            });

            if (_shadow_debug_mode != shadow_debug_mode::none)
            {
                shadow_res.shadow_data.debug_mode = static_cast<uint32_t>(_shadow_debug_mode);
            }

            _pool.write_directional_shadow_data(shadow_res.shadow_data);
            _directional_shadow_atlas_target = shadow_res.shadow_atlas;
        }

        const auto& depth_data =
            add_depth_prepass(_graph, _pool, _shaders, _depth_target, _opaque_draw_count, _opaque_draw_offset);

        if (_cfg.enable_ssao)
        {
            const auto& ssao_data = add_ssao_pass(_graph, _pool, _shaders, depth_data.depth_texture, _ssao_target);
            add_ssao_blur_pass(_graph, _pool, _shaders, ssao_data.ssao_raw, depth_data.depth_texture,
                               _ssao_blurred_target, width, height);
        }

        const auto& skybox_data = add_skybox_pass(_graph, _pool, _shaders, _hdr_color_target);
        const auto& pbr_data = add_pbr_opaque_pass(
            _graph, _pool, _shaders, skybox_data.hdr_color, depth_data.depth_texture, _directional_shadow_atlas_target,
            _opaque_draw_count, _opaque_draw_offset, culling_data.light_bitmask_buffer);

        const auto& clear_data =
            add_transparency_clear_pass(_graph, _shaders, _moments_target, _zeroth_moment_target, width, height);
        const auto& gather_data = add_transparency_gather_pass(
            _graph, _pool, _shaders, clear_data.moments_texture, clear_data.zeroth_moment_texture,
            depth_data.depth_texture, _transparent_draw_count, _transparent_draw_offset);
        const auto& resolve_data = add_transparency_resolve_pass(
            _graph, _pool, _shaders, _transparency_accum_target, gather_data.moments_texture,
            gather_data.zeroth_moment_texture, depth_data.depth_texture, _transparent_draw_count,
            _transparent_draw_offset, _directional_shadow_atlas_target, culling_data.light_bitmask_buffer);
        const auto& blend_data = add_transparency_blend_pass(
            _graph, _pool, _shaders, pbr_data.hdr_color, resolve_data.accum_texture, gather_data.zeroth_moment_texture);

        add_tonemapping_pass(_graph, _pool, _shaders, blend_data.hdr_color, _tonemapped_color_target,
                             _cfg.tonemapped_color_format);
    }

    auto renderer::render(const render_graph::frame_sync_options& sync) -> expected<void, render_graph::execution_error>
    {
        if (!_device)
        {
            return unexpected(render_graph::execution_error::compile_failed);
        }

        return _graph.execute(*_device, sync);
    }

    void renderer::resize(uint32_t width, uint32_t height)
    {
        _cfg.render_width = width;
        _cfg.render_height = height;
        _graph.set_surface_size(width, height);
        if (_device)
        {
            _graph.get_allocator().on_surface_resize(*_device);
        }
    }

    void renderer::_subscribe_events()
    {
        if (_events == nullptr)
        {
            return;
        }

        // Mesh Component Added
        _mesh_added_sub =
            _events->dispatcher<ecs::component_added_event<ecs::entity, core::mesh_component>>().subscribe(
                [this](const ecs::component_added_event<ecs::entity, core::mesh_component>& evt) {
                    if (!_renderable_indices.contains(evt.entity))
                    {
                        const auto idx = _tracked_entities.size();
                        _renderable_indices[evt.entity] = idx;
                        _tracked_entities.push_back(evt.entity);
                        _renderables_dirty_count = _cfg.pool_config.frames_in_flight;
                    }
                });

        // Mesh Component Replaced
        _mesh_replaced_sub =
            _events->dispatcher<ecs::component_replaced_event<ecs::entity, core::mesh_component>>().subscribe(
                [this]([[maybe_unused]] const ecs::component_replaced_event<ecs::entity, core::mesh_component>& evt) {
                    _renderables_dirty_count = _cfg.pool_config.frames_in_flight;
                });

        // Mesh Component Removed
        _mesh_removed_sub =
            _events->dispatcher<ecs::component_removed_event<ecs::entity, core::mesh_component>>().subscribe(
                [this](const ecs::component_removed_event<ecs::entity, core::mesh_component>& evt) {
                    auto it = _renderable_indices.find(evt.entity);
                    if (it != _renderable_indices.end())
                    {
                        const auto remove_idx = it->second;
                        const auto last_idx = _tracked_entities.size() - 1;
                        if (remove_idx != last_idx)
                        {
                            const auto last_entity = _tracked_entities[last_idx];
                            _tracked_entities[remove_idx] = last_entity;
                            _renderable_indices[last_entity] = remove_idx;
                        }
                        _tracked_entities.pop_back();
                        _renderable_indices.erase(it);
                        _renderables_dirty_count = _cfg.pool_config.frames_in_flight;
                    }
                });

        // Material Component Added / Replaced / Removed
        _material_added_sub =
            _events->dispatcher<ecs::component_added_event<ecs::entity, core::material_component>>().subscribe(
                [this]([[maybe_unused]] const ecs::component_added_event<ecs::entity, core::material_component>& evt) {
                    _renderables_dirty_count = _cfg.pool_config.frames_in_flight;
                });

        _material_replaced_sub =
            _events->dispatcher<ecs::component_replaced_event<ecs::entity, core::material_component>>().subscribe(
                [this](
                    [[maybe_unused]] const ecs::component_replaced_event<ecs::entity, core::material_component>& evt) {
                    _renderables_dirty_count = _cfg.pool_config.frames_in_flight;
                });

        _material_removed_sub =
            _events->dispatcher<ecs::component_removed_event<ecs::entity, core::material_component>>().subscribe(
                [this](
                    [[maybe_unused]] const ecs::component_removed_event<ecs::entity, core::material_component>& evt) {
                    _renderables_dirty_count = _cfg.pool_config.frames_in_flight;
                });

        // Point Light Added
        _point_light_added_sub =
            _events->dispatcher<ecs::component_added_event<ecs::entity, point_light_component>>().subscribe(
                [this](const ecs::component_added_event<ecs::entity, point_light_component>& evt) {
                    if (!_point_light_indices.contains(evt.entity))
                    {
                        const auto idx = _point_light_entities.size();
                        _point_light_indices[evt.entity] = idx;
                        _point_light_entities.push_back(evt.entity);
                        _lights_dirty_count = _cfg.pool_config.frames_in_flight;
                    }
                });

        // Point Light Replaced
        _point_light_replaced_sub =
            _events->dispatcher<ecs::component_replaced_event<ecs::entity, point_light_component>>().subscribe(
                [this](const ecs::component_replaced_event<ecs::entity, point_light_component>& evt) {
                    if (_point_light_indices.contains(evt.entity))
                    {
                        _lights_dirty_count = _cfg.pool_config.frames_in_flight;
                    }
                });

        // Point Light Removed
        _point_light_removed_sub =
            _events->dispatcher<ecs::component_removed_event<ecs::entity, point_light_component>>().subscribe(
                [this](const ecs::component_removed_event<ecs::entity, point_light_component>& evt) {
                    auto it = _point_light_indices.find(evt.entity);
                    if (it != _point_light_indices.end())
                    {
                        const auto remove_idx = it->second;
                        const auto last_idx = _point_light_entities.size() - 1;
                        if (remove_idx != last_idx)
                        {
                            const auto last_entity = _point_light_entities[last_idx];
                            _point_light_entities[remove_idx] = last_entity;
                            _point_light_indices[last_entity] = remove_idx;
                        }
                        _point_light_entities.pop_back();
                        _point_light_indices.erase(it);
                        _lights_dirty_count = _cfg.pool_config.frames_in_flight;
                    }
                });

        // Directional Light Added / Replaced / Removed
        _dir_light_added_sub =
            _events->dispatcher<ecs::component_added_event<ecs::entity, directional_light_component>>().subscribe(
                [this](
                    [[maybe_unused]] const ecs::component_added_event<ecs::entity, directional_light_component>& evt) {
                    _lights_dirty_count = _cfg.pool_config.frames_in_flight;
                });

        _dir_light_replaced_sub =
            _events->dispatcher<ecs::component_replaced_event<ecs::entity, directional_light_component>>().subscribe(
                [this]([[maybe_unused]] const ecs::component_replaced_event<ecs::entity, directional_light_component>&
                           evt) { _lights_dirty_count = _cfg.pool_config.frames_in_flight; });

        _dir_light_removed_sub =
            _events->dispatcher<ecs::component_removed_event<ecs::entity, directional_light_component>>().subscribe(
                [this]([[maybe_unused]] const ecs::component_removed_event<ecs::entity, directional_light_component>&
                           evt) { _lights_dirty_count = _cfg.pool_config.frames_in_flight; });

        // Transform Replaced (filter for tracked light entities or directional lights)
        _transform_replaced_sub =
            _events->dispatcher<ecs::component_replaced_event<ecs::entity, ecs::transform_component>>().subscribe(
                [this](const ecs::component_replaced_event<ecs::entity, ecs::transform_component>& evt) {
                    if (_point_light_indices.contains(evt.entity))
                    {
                        _lights_dirty_count = _cfg.pool_config.frames_in_flight;
                    }
                    else if (_inputs.entity_registry &&
                             _inputs.entity_registry->has<directional_light_component>(evt.entity))
                    {
                        _lights_dirty_count = _cfg.pool_config.frames_in_flight;
                    }
                });

        // Entity Destroyed
        _entity_destroyed_sub = _events->dispatcher<ecs::entity_destroyed_event<ecs::entity>>().subscribe(
            [this](const ecs::entity_destroyed_event<ecs::entity>& evt) {
                // Check renderables
                if (auto it = _renderable_indices.find(evt.entity); it != _renderable_indices.end())
                {
                    const auto remove_idx = it->second;
                    const auto last_idx = _tracked_entities.size() - 1;
                    if (remove_idx != last_idx)
                    {
                        const auto last_entity = _tracked_entities[last_idx];
                        _tracked_entities[remove_idx] = last_entity;
                        _renderable_indices[last_entity] = remove_idx;
                    }
                    _tracked_entities.pop_back();
                    _renderable_indices.erase(it);
                    _renderables_dirty_count = _cfg.pool_config.frames_in_flight;
                }

                // Check point lights
                if (auto it = _point_light_indices.find(evt.entity); it != _point_light_indices.end())
                {
                    const auto remove_idx = it->second;
                    const auto last_idx = _point_light_entities.size() - 1;
                    if (remove_idx != last_idx)
                    {
                        const auto last_entity = _point_light_entities[last_idx];
                        _point_light_entities[remove_idx] = last_entity;
                        _point_light_indices[last_entity] = remove_idx;
                    }
                    _point_light_entities.pop_back();
                    _point_light_indices.erase(it);
                    _lights_dirty_count = _cfg.pool_config.frames_in_flight;
                }
            });
    }

    void renderer::_unsubscribe_events()
    {
        if (_events == nullptr)
        {
            return;
        }

        if (_mesh_added_sub != event::null_subscription<ecs::component_added_event<ecs::entity, core::mesh_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_added_event<ecs::entity, core::mesh_component>>().unsubscribe(
                    _mesh_added_sub));
            _mesh_added_sub = {};
        }
        if (_mesh_replaced_sub !=
            event::null_subscription<ecs::component_replaced_event<ecs::entity, core::mesh_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_replaced_event<ecs::entity, core::mesh_component>>().unsubscribe(
                    _mesh_replaced_sub));
            _mesh_replaced_sub = {};
        }
        if (_mesh_removed_sub !=
            event::null_subscription<ecs::component_removed_event<ecs::entity, core::mesh_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_removed_event<ecs::entity, core::mesh_component>>().unsubscribe(
                    _mesh_removed_sub));
            _mesh_removed_sub = {};
        }
        if (_material_added_sub !=
            event::null_subscription<ecs::component_added_event<ecs::entity, core::material_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_added_event<ecs::entity, core::material_component>>().unsubscribe(
                    _material_added_sub));
            _material_added_sub = {};
        }
        if (_material_replaced_sub !=
            event::null_subscription<ecs::component_replaced_event<ecs::entity, core::material_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_replaced_event<ecs::entity, core::material_component>>().unsubscribe(
                    _material_replaced_sub));
            _material_replaced_sub = {};
        }
        if (_material_removed_sub !=
            event::null_subscription<ecs::component_removed_event<ecs::entity, core::material_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_removed_event<ecs::entity, core::material_component>>().unsubscribe(
                    _material_removed_sub));
            _material_removed_sub = {};
        }
        if (_point_light_added_sub !=
            event::null_subscription<ecs::component_added_event<ecs::entity, point_light_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_added_event<ecs::entity, point_light_component>>().unsubscribe(
                    _point_light_added_sub));
            _point_light_added_sub = {};
        }
        if (_point_light_replaced_sub !=
            event::null_subscription<ecs::component_replaced_event<ecs::entity, point_light_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_replaced_event<ecs::entity, point_light_component>>().unsubscribe(
                    _point_light_replaced_sub));
            _point_light_replaced_sub = {};
        }
        if (_point_light_removed_sub !=
            event::null_subscription<ecs::component_removed_event<ecs::entity, point_light_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_removed_event<ecs::entity, point_light_component>>().unsubscribe(
                    _point_light_removed_sub));
            _point_light_removed_sub = {};
        }
        if (_dir_light_added_sub !=
            event::null_subscription<ecs::component_added_event<ecs::entity, directional_light_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_added_event<ecs::entity, directional_light_component>>().unsubscribe(
                    _dir_light_added_sub));
            _dir_light_added_sub = {};
        }
        if (_dir_light_replaced_sub !=
            event::null_subscription<ecs::component_replaced_event<ecs::entity, directional_light_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_replaced_event<ecs::entity, directional_light_component>>()
                    .unsubscribe(_dir_light_replaced_sub));
            _dir_light_replaced_sub = {};
        }
        if (_dir_light_removed_sub !=
            event::null_subscription<ecs::component_removed_event<ecs::entity, directional_light_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_removed_event<ecs::entity, directional_light_component>>()
                    .unsubscribe(_dir_light_removed_sub));
            _dir_light_removed_sub = {};
        }
        if (_transform_replaced_sub !=
            event::null_subscription<ecs::component_replaced_event<ecs::entity, ecs::transform_component>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::component_replaced_event<ecs::entity, ecs::transform_component>>().unsubscribe(
                    _transform_replaced_sub));
            _transform_replaced_sub = {};
        }
        if (_entity_destroyed_sub != event::null_subscription<ecs::entity_destroyed_event<ecs::entity>>)
        {
            static_cast<void>(
                _events->dispatcher<ecs::entity_destroyed_event<ecs::entity>>().unsubscribe(_entity_destroyed_sub));
            _entity_destroyed_sub = {};
        }
    }

    void renderer::_init_renderables_from_registry()
    {
        _renderable_indices.clear();
        _tracked_entities.clear();

        if (_inputs.entity_registry != nullptr)
        {
            _inputs.entity_registry->each([this](const ecs::self_component& self, const core::mesh_component&) {
                const auto idx = _tracked_entities.size();
                _renderable_indices[self.entity] = idx;
                _tracked_entities.push_back(self.entity);
            });
            _renderables_dirty_count = _cfg.pool_config.frames_in_flight;
        }
    }

    void renderer::_init_lights_from_registry()
    {
        _point_light_indices.clear();
        _point_light_entities.clear();

        if (_inputs.entity_registry != nullptr)
        {
            _inputs.entity_registry->each([this](const ecs::self_component& self, const point_light_component&) {
                const auto idx = _point_light_entities.size();
                _point_light_indices[self.entity] = idx;
                _point_light_entities.push_back(self.entity);
            });
            _lights_dirty_count = _cfg.pool_config.frames_in_flight;
        }
    }
} // namespace tempest::render_system
