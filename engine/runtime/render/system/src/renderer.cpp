#include <tempest/render_system/renderer.hpp>

#include <tempest/render_system/passes/depth_prepass.hpp>
#include <tempest/render_system/passes/frame_upload_pass.hpp>
#include <tempest/render_system/passes/pbr_opaque_pass.hpp>
#include <tempest/render_system/passes/skybox_pass.hpp>
#include <tempest/render_system/passes/ssao_pass.hpp>
#include <tempest/render_system/passes/ssao_blur_pass.hpp>
#include <tempest/render_system/passes/tonemapping_pass.hpp>
#include <tempest/relationship_component.hpp>
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
    }
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
        : _device{&dev}, _log{&log}, _cfg{cfg}, _inputs{inputs},
          _owned_camera_system{tempest::move(camera_sys)},
          _camera_system{_inputs.camera_sys ? _inputs.camera_sys : _owned_camera_system.get()},
          _pool{dev, cfg.pool_config},
          _shaders{dev},
          _graph{cfg.render_width, cfg.render_height}
    {
    }

    renderer::~renderer()
    {
        if (_device)
        {
            _graph.get_allocator().release_all(*_device);
        }
    }

    renderer::renderer(renderer&& other) noexcept
        : _device{other._device}, _log{other._log}, _cfg{other._cfg}, _inputs{other._inputs},
          _owned_camera_system{tempest::move(other._owned_camera_system)},
          _camera_system{other._camera_system},
          _pool{tempest::move(other._pool)},
          _shaders{tempest::move(other._shaders)},
          _graph{tempest::move(other._graph)},
          _hdr_color_target{other._hdr_color_target},
          _depth_target{other._depth_target},
          _ssao_target{other._ssao_target},
          _ssao_blurred_target{other._ssao_blurred_target},
          _tonemapped_color_target{other._tonemapped_color_target},
          _active_draw_count{other._active_draw_count}
    {
        other._device = nullptr;
        other._camera_system = nullptr;
    }

    renderer& renderer::operator=(renderer&& other) noexcept
    {
        if (this != &other)
        {
            if (_device)
            {
                _graph.get_allocator().release_all(*_device);
            }

            _device = other._device;
            _log = other._log;
            _cfg = other._cfg;
            _inputs = other._inputs;
            _owned_camera_system = tempest::move(other._owned_camera_system);
            _camera_system = other._camera_system;
            _pool = tempest::move(other._pool);
            _shaders = tempest::move(other._shaders);
            _graph = tempest::move(other._graph);
            _hdr_color_target = other._hdr_color_target;
            _depth_target = other._depth_target;
            _ssao_target = other._ssao_target;
            _ssao_blurred_target = other._ssao_blurred_target;
            _tonemapped_color_target = other._tonemapped_color_target;
            _active_draw_count = other._active_draw_count;

            other._device = nullptr;
            other._camera_system = nullptr;
        }
        return *this;
    }

    void renderer::upload_objects_sync(span<const ecs::entity> entities, const core::mesh_registry& meshes,
                                       const core::texture_registry& textures, const core::material_registry& materials)
    {
        if (_inputs.entity_registry == nullptr)
        {
            return;
        }

        auto mesh_ids = vector<guid>{};
        auto mat_ids = vector<guid>{};

        for (const auto entity : entities)
        {
            if (const auto* r = _inputs.entity_registry->try_get<renderable_component>(entity))
            {
                mesh_ids.push_back(r->mesh_id);
                mat_ids.push_back(r->material_id);
            }
            else if (const auto* mc = _inputs.entity_registry->try_get<core::mesh_component>(entity))
            {
                mesh_ids.push_back(mc->mesh_id);
                if (const auto* matc = _inputs.entity_registry->try_get<core::material_component>(entity))
                {
                    mat_ids.push_back(matc->material_id);
                }
            }
        }

        auto tex_ids = vector<guid>{};
        for (const auto& mat_id : mat_ids)
        {
            if (auto mat_opt = materials.find(mat_id))
            {
                const auto& m = *mat_opt;
                if (auto t = m.get_texture(core::material::base_color_texture_name)) tex_ids.push_back(*t);
                if (auto t = m.get_texture(core::material::normal_texture_name)) tex_ids.push_back(*t);
                if (auto t = m.get_texture(core::material::metallic_roughness_texture_name)) tex_ids.push_back(*t);
                if (auto t = m.get_texture(core::material::emissive_texture_name)) tex_ids.push_back(*t);
                if (auto t = m.get_texture(core::material::occlusion_texture_name)) tex_ids.push_back(*t);
                if (auto t = m.get_texture(core::material::transmissive_texture_name)) tex_ids.push_back(*t);
                if (auto t = m.get_texture(core::material::volume_thickness_texture_name)) tex_ids.push_back(*t);
            }
        }

        // Upload static mesh, texture & material data via staging transfer pass
        _pool.load_textures(tex_ids, textures, _graph);
        _pool.load_materials(mat_ids, materials, _graph);
        _pool.load_meshes(mesh_ids, meshes, _graph);

        // Execute upload graph and wait for transfers to complete
        if (_device)
        {
            [[maybe_unused]] const auto sync_res = _graph.execute(*_device);
            _device->wait_idle();
            _pool.clear_staging_buffers();
            _graph.reset();
        }

        // Build dynamic ObjectData, Instance Indices, and Indirect Draw Commands
        // Partition into:
        auto objects = vector<object_payload>{};
        objects.reserve(entities.size());
        auto instances = vector<uint32_t>{};
        instances.reserve(entities.size());
        auto commands = vector<indexed_indirect_command>{};
        commands.reserve(entities.size());

        for (const auto entity : entities)
        {
            auto mesh_id = guid{};
            auto mat_id = guid{};

            if (const auto* r = _inputs.entity_registry->try_get<renderable_component>(entity))
            {
                mesh_id = r->mesh_id;
                mat_id = r->material_id;
            }
            else if (const auto* mc = _inputs.entity_registry->try_get<core::mesh_component>(entity))
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

            auto mesh_layout_opt = _pool.get_mesh_layout(mesh_id);
            if (!mesh_layout_opt.has_value())
            {
                continue;
            }

            const auto& ml = *mesh_layout_opt;
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

            const auto idx = static_cast<uint32_t>(objects.size());
            objects.push_back(payload);
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

        _active_draw_count = static_cast<uint32_t>(commands.size());

        _pool.write_objects(objects);
        _pool.write_instances(instances);
        _pool.write_draw_commands(commands);
    }

    void renderer::prepare_frame(uint32_t width, uint32_t height, optional<rhi::texture_handle> swapchain_tex,
                                 optional<rhi::texture_view_handle> swapchain_view)
    {
        _shaders.process_deferred_retirements();
        _pool.advance_frame();
        _graph.reset();
        _graph.set_surface_size(width, height);

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

        if (_camera_system)
        {
            auto cam_opt = _camera_system->get_active_camera();
            if (cam_opt.has_value())
            {
                const auto& cam = *cam_opt;
                scene.view = cam.view;
                scene.inv_view = cam.inv_view;
                scene.projection = cam.proj;
                scene.inv_projection = cam.inv_proj;
                scene.camera_position = cam.eye_position;
            }
        }

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
                .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
                .mip_levels = 1,
                .array_layers = 1,
                .name = "TonemappedColorTarget",
            });
        }

        // Build DAG Passes
        add_frame_upload_pass(_graph, _pool);

        const auto& depth_data = add_depth_prepass(_graph, _pool, _shaders, _depth_target, _active_draw_count);

        if (_cfg.enable_ssao)
        {
            const auto& ssao_data = add_ssao_pass(_graph, _pool, _shaders, depth_data.depth_texture, _ssao_target);
            add_ssao_blur_pass(_graph, _pool, _shaders, ssao_data.ssao_raw,
                               depth_data.depth_texture, _ssao_blurred_target,
                               width, height);
        }

        const auto& skybox_data = add_skybox_pass(_graph, _pool, _shaders, _hdr_color_target);
        const auto& pbr_data = add_pbr_opaque_pass(_graph, _pool, _shaders, skybox_data.hdr_color,
                                                   depth_data.depth_texture, _active_draw_count);
        add_tonemapping_pass(_graph, _pool, _shaders, pbr_data.hdr_color, _tonemapped_color_target,
                            _cfg.tonemapped_color_format);
    }

    auto renderer::render(const render_graph::frame_sync_options& sync)
        -> expected<void, render_graph::execution_error>
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
} // namespace tempest::render_system
