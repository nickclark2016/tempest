#include <gtest/gtest.h>

#include <filesystem>
#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/default_importers.hpp>
#include <tempest/logger.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/passes/depth_prepass.hpp>
#include <tempest/render_system/passes/frame_upload_pass.hpp>
#include <tempest/render_system/passes/light_clustering_pass.hpp>
#include <tempest/render_system/passes/light_culling_pass.hpp>
#include <tempest/render_system/passes/pbr_opaque_pass.hpp>
#include <tempest/render_system/passes/shadow_map_pass.hpp>
#include <tempest/render_system/passes/skybox_pass.hpp>
#include <tempest/render_system/passes/ssao_blur_pass.hpp>
#include <tempest/render_system/passes/ssao_pass.hpp>
#include <tempest/render_system/passes/tonemapping_pass.hpp>
#include <tempest/render_system/passes/transparency_blend_pass.hpp>
#include <tempest/render_system/passes/transparency_gather_pass.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/render_system/renderer.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>
#include <tempest/rhi.hpp>
#include <tempest/transform_component.hpp>

namespace tempest::render_system::tests
{
    namespace
    {
        struct test_fixture
        {
            unique_ptr<rhi::context> ctx;
            unique_ptr<rhi::device> dev;
        };

        auto create_test_device() -> test_fixture
        {
            auto ctx_desc = rhi::context_desc{
                .application_name = "RenderSystemTest",
                .version_major = 1,
                .version_minor = 0,
                .version_patch = 0,
                .enable_api_validation = true,
                .api = rhi::graphics_api::vulkan,
            };

            auto ctx_res = rhi::create_context(ctx_desc);
            if (!ctx_res.has_value())
            {
                return {};
            }

            auto ctx = tempest::move(ctx_res).value();
            auto devices = ctx->enumerate_devices();
            if (devices.empty())
            {
                return {};
            }

            auto dev = ctx->create_device(devices[0].device_uuid);
            return test_fixture{
                .ctx = tempest::move(ctx),
                .dev = tempest::move(dev),
            };
        }

        auto create_test_mesh() -> core::mesh
        {
            auto m = core::mesh{};
            m.vertices.push_back(core::vertex{.position = {-1.0F, -1.0F, 0.0F}, .uv = {0.0F, 0.0F}, .normal = {0.0F, 0.0F, 1.0F}, .tangent = {1.0F, 0.0F, 0.0F, 1.0F}, .color = {1.0F, 1.0F, 1.0F, 1.0F}});
            m.vertices.push_back(core::vertex{.position = { 1.0F, -1.0F, 0.0F}, .uv = {1.0F, 0.0F}, .normal = {0.0F, 0.0F, 1.0F}, .tangent = {1.0F, 0.0F, 0.0F, 1.0F}, .color = {1.0F, 1.0F, 1.0F, 1.0F}});
            m.vertices.push_back(core::vertex{.position = { 1.0F,  1.0F, 0.0F}, .uv = {1.0F, 1.0F}, .normal = {0.0F, 0.0F, 1.0F}, .tangent = {1.0F, 0.0F, 0.0F, 1.0F}, .color = {1.0F, 1.0F, 1.0F, 1.0F}});
            m.vertices.push_back(core::vertex{.position = {-1.0F,  1.0F, 0.0F}, .uv = {0.0F, 1.0F}, .normal = {0.0F, 0.0F, 1.0F}, .tangent = {1.0F, 0.0F, 0.0F, 1.0F}, .color = {1.0F, 1.0F, 1.0F, 1.0F}});
            m.indices.push_back(0);
            m.indices.push_back(1);
            m.indices.push_back(2);
            m.indices.push_back(2);
            m.indices.push_back(3);
            m.indices.push_back(0);
            return m;
        }
    } // namespace

    TEST(render_system_tests, bda_buffer_allocation_and_pointer_arithmetic)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        {
            auto pool = resource_pool{*dev};
            EXPECT_NE(pool.get_vertex_buffer_address(), 0ULL);
            EXPECT_NE(pool.get_mesh_table_address(), 0ULL);
            EXPECT_NE(pool.get_material_table_address(), 0ULL);

            auto meshes = core::mesh_registry{};
            auto mesh_id = meshes.register_mesh(create_test_mesh());

            auto graph = render_graph::render_graph{1920, 1080};
            pool.load_meshes(span<const guid>{&mesh_id, 1}, meshes, graph);

            auto mesh_addr = pool.get_mesh_address(mesh_id);
            EXPECT_GE(mesh_addr, pool.get_mesh_table_address());

            auto layout_opt = pool.get_mesh_layout(mesh_id);
            ASSERT_TRUE(layout_opt.has_value());
            EXPECT_EQ(layout_opt->index_count, 6U);
            EXPECT_EQ(layout_opt->vertex_buffer_address, pool.get_vertex_buffer_address());
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, camera_system_active_selection)
    {
        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto cam_sys = camera_system{registry, events};

        auto cam1 = registry.create();
        registry.assign(cam1, camera_component{.aspect_ratio = 16.0F / 9.0F, .vertical_fov = 1.0F, .near_plane = 0.1F});
        registry.assign(cam1, ecs::transform_component::identity());

        auto active_entity = cam_sys.get_active_camera_entity();
        ASSERT_TRUE(active_entity.has_value());
        EXPECT_EQ(*active_entity, cam1);

        auto active_cam = cam_sys.get_active_camera();
        ASSERT_TRUE(active_cam.has_value());
        EXPECT_NE(active_cam->proj[0][0], 0.0F);

        // Assign explicit active_camera_component
        auto cam2 = registry.create();
        registry.assign(cam2, camera_component{.aspect_ratio = 4.0F / 3.0F, .vertical_fov = 0.8F, .near_plane = 0.5F});
        registry.assign(cam2, ecs::transform_component::identity());
        registry.assign(cam2, active_camera_component{});

        active_entity = cam_sys.get_active_camera_entity();
        ASSERT_TRUE(active_entity.has_value());
        EXPECT_EQ(*active_entity, cam2);

        auto render_cam = cam_sys.get_active_camera();
        ASSERT_TRUE(render_cam.has_value());
        EXPECT_NE(render_cam->proj[0][0], 0.0F);
    }

    TEST(render_system_tests, renderer_forward_pbr_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 1. Setup Camera Entity
            auto cam_ent = registry.create();
            registry.assign(cam_ent, camera_component{
                .aspect_ratio = 1280.0F / 720.0F,
                .vertical_fov = 1.5707963F,
                .near_plane = 0.01F,
            });
            auto cam_tx = ecs::transform_component::identity();
            cam_tx.position({0.0F, 0.0F, -5.0F});
            registry.assign(cam_ent, cam_tx);
            registry.assign(cam_ent, active_camera_component{});

            // 2. Setup Sun Light Entity
            auto sun_ent = registry.create();
            registry.assign(sun_ent, directional_light_component{
                .color = {1.0F, 1.0F, 1.0F},
                .intensity = 2.0F,
            });
            registry.assign(sun_ent, ecs::transform_component::identity());

            // 3. Setup Renderable Geometry Entity
            auto meshes = core::mesh_registry{};
            auto materials = core::material_registry{};
            auto textures = core::texture_registry{};

            auto mesh_id = meshes.register_mesh(create_test_mesh());
            auto mat = core::material{};
            mat.set_vec4(core::material::base_color_factor_name, {0.8F, 0.2F, 0.2F, 1.0F});
            mat.set_scalar(core::material::metallic_factor_name, 0.0F);
            mat.set_scalar(core::material::roughness_factor_name, 0.5F);
            auto mat_id = materials.register_material(tempest::move(mat));

            auto geom_ent = registry.create();
            registry.assign(geom_ent, renderable_component{
                .mesh_id = mesh_id,
                .material_id = mat_id,
                .double_sided = false,
            });
            registry.assign(geom_ent, ecs::transform_component::identity());

            // 4. Upload Objects and Prepare Frame
            auto entities = array<ecs::entity, 1>{geom_ent};
            rend->upload_objects_sync(span<const ecs::entity>{entities.data(), entities.size()},
                                      meshes, textures, materials);

            rend->prepare_frame(1280, 720);

            // 5. Render execution
            auto render_res = rend->render();
            EXPECT_TRUE(render_res.has_value());

            dev->wait_idle();

            // 6. Readback the rendered frame
            auto readback_buf = dev->create_buffer(rhi::buffer_desc{
                .size = 1280 * 720 * 4,
                .memory_usage = rhi::memory_usage::readback,
                .usage = rhi::buffer_usage::transfer_dst,
                .name = "FrameReadbackBuffer",
            });

            const auto* alloc = rend->get_render_graph().get_physical_texture(rend->get_tonemapped_color_texture().id);
            ASSERT_NE(alloc, nullptr);
            if (alloc)
            {
                auto& port = dev->get_graphics_execution_port();
                auto& cmd = port.acquire_command_list();
                cmd.begin();

                const auto region = rhi::buffer_texture_copy_region{
                    .buffer_offset = 0,
                    .buffer_row_length = 0,
                    .buffer_image_height = 0,
                    .mip_level = 0,
                    .base_array_layer = 0,
                    .array_layer_count = 1,
                    .image_offset_x = 0,
                    .image_offset_y = 0,
                    .image_offset_z = 0,
                    .image_extent_width = 1280,
                    .image_extent_height = 720,
                    .image_extent_depth = 1,
                };
                cmd.copy_texture_to_buffer(alloc->handle, readback_buf,
                                           span<const rhi::buffer_texture_copy_region>{&region, 1});
                cmd.end();

                auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
                [[maybe_unused]] auto submit_res = port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
                dev->wait_idle();

                const auto* pixels = static_cast<const uint8_t*>(readback_buf.cpu_address);
                if (pixels)
                {
                    // Sample center pixel (640, 360) (Rendered red PBR geometry)
                    const auto center_idx = (360 * 1280 + 640) * 4;
                    const auto r = pixels[center_idx + 0];
                    const auto a = pixels[center_idx + 3];
                    EXPECT_GT(r, 50);
                    EXPECT_EQ(a, 255);

                    // Sample corner pixel (100, 100) (Skybox gradient)
                    const auto sky_idx = (100 * 1280 + 100) * 4;
                    const auto sky_b = pixels[sky_idx + 2];
                    const auto sky_a = pixels[sky_idx + 3];
                    EXPECT_GT(sky_b, 50);
                    EXPECT_EQ(sky_a, 255);
                }
            }

            dev->destroy_buffer(readback_buf);
        }
    }

    TEST(render_system_tests, sponza_asset_loading_and_render_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // Setup Camera
            auto cam = registry.create();
            registry.assign(cam, camera_component{
                .aspect_ratio = 16.0F / 9.0F,
                .vertical_fov = 1.2F,
                .near_plane = 0.01F,
            });
            auto cam_tx = ecs::transform_component::identity();
            cam_tx.position({0.0F, 5.0F, -2.0F});
            registry.assign(cam, cam_tx);
            registry.assign(cam, active_camera_component{});

            // Setup Sun Light
            auto sun = registry.create();
            registry.assign(sun, directional_light_component{
                .color = {1.0F, 0.98F, 0.92F},
                .intensity = 7.0F,
            });
            registry.assign(sun, shadow_map_component{
                .shadow_distance = 60.0F,
                .split_lambda = 0.85F,
                .blend_fraction = 0.1F,
                .cascade_count = 4,
            });
            auto sun_tx = ecs::transform_component::identity();
            sun_tx.rotation({math::as_radians(84.0F), math::as_radians(8.0F), 0.0F});
            registry.assign(sun, sun_tx);

            auto renderable_entities = vector<ecs::entity>{};
            auto meshes = core::mesh_registry{};
            auto materials = core::material_registry{};
            auto textures = core::texture_registry{};

            auto asset_type_reg = assets::asset_type_registry{};
            auto asset_db = assets::asset_database{&asset_type_reg};
            assets::register_default_importers(asset_db, &meshes, &textures, &materials);

            const auto sponza_path = "vendor/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf";
            if (std::filesystem::exists(sponza_path))
            {
                auto prefab_root = asset_db.load(sponza_path, registry);
                if (prefab_root != ecs::tombstone)
                {
                    if (registry.try_get<core::mesh_component>(prefab_root) != nullptr)
                    {
                        renderable_entities.push_back(prefab_root);
                    }
                    for (auto ent : ecs::archetype_entity_hierarchy_view(registry, prefab_root))
                    {
                        if (registry.try_get<core::mesh_component>(ent) != nullptr)
                        {
                            renderable_entities.push_back(ent);
                        }
                    }
                }
            }

            // If no Sponza entities loaded, fall back to procedural quad entities
            if (renderable_entities.empty())
            {
                auto mesh_id = meshes.register_mesh(create_test_mesh());
                auto mat = core::material{};
                mat.set_vec4(core::material::base_color_factor_name, {0.7F, 0.7F, 0.7F, 1.0F});
                auto mat_id = materials.register_material(tempest::move(mat));

                auto ent = registry.create();
                registry.assign(ent, renderable_component{.mesh_id = mesh_id, .material_id = mat_id, .double_sided = false});
                registry.assign(ent, ecs::transform_component::identity());
                renderable_entities.push_back(ent);
            }

            rend->upload_objects_sync(span<const ecs::entity>{renderable_entities.data(), renderable_entities.size()},
                                      meshes, textures, materials);

            rend->prepare_frame(1280, 720);

            EXPECT_TRUE(rend->get_shadow_atlas_texture().has_value());

            // Verify ShadowMapPass is present in the compiled DAG
            auto compile_res = rend->get_render_graph().compile();
            ASSERT_TRUE(compile_res.has_value());
            const auto& all_passes = rend->get_render_graph().get_compiler().get_passes();
            bool shadow_pass_executed = false;
            for (auto pass_idx : compile_res->sorted_pass_indices)
            {
                if (all_passes[pass_idx].name == "ShadowMapPass")
                {
                    shadow_pass_executed = true;
                    break;
                }
            }
            EXPECT_TRUE(shadow_pass_executed);

            auto res = rend->render();
            EXPECT_TRUE(res.has_value());

            dev->wait_idle();
        }
    }

    TEST(render_system_tests, cascaded_shadow_map_generation_and_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev};
        auto graph = render_graph::render_graph{2048, 2048};

        auto cam = render_camera{
            .proj = math::perspective(16.0F / 9.0F, 1.0F, 0.1F, 100.0F),
            .inv_proj = math::inverse(math::perspective(16.0F / 9.0F, 1.0F, 0.1F, 100.0F)),
            .view = math::look_at(math::vec3<float>{0.0F, 2.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F}, math::vec3<float>{0.0F, 1.0F, 0.0F}),
            .inv_view = math::inverse(math::look_at(math::vec3<float>{0.0F, 2.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F}, math::vec3<float>{0.0F, 1.0F, 0.0F})),
            .eye_position = {0.0F, 2.0F, -5.0F, 1.0F},
        };

        const auto shadow_cfg = shadow_map_component{
            .shadow_distance = 80.0F,
            .split_lambda = 0.85F,
            .blend_fraction = 0.1F,
            .cascade_count = 4,
        };

        const auto light_dir = math::vec3<float>{0.5F, -1.0F, 0.3F};
        const auto cascades = compute_csm_cascades(light_dir, cam, shadow_cfg, {2048, 2048});

        EXPECT_GT(cascades[0].split_depth, 0.1F);
        EXPECT_GT(cascades[1].split_depth, cascades[0].split_depth);
        EXPECT_GT(cascades[2].split_depth, cascades[1].split_depth);
        EXPECT_GT(cascades[3].split_depth, cascades[2].split_depth);

        auto shadow_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(2048, 2048),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "ShadowMapAtlas",
        });

        add_shadow_map_pass(graph, pool, shaders, shadow_tex, light_dir, cam, shadow_cfg, {2048, 2048}, 0);

        auto res = graph.execute(*dev);
        EXPECT_TRUE(res.has_value());

        dev->wait_idle();
    }

    TEST(render_system_tests, clustered_lighting_and_culling_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev};
        auto graph = render_graph::render_graph{1280, 720};

        auto cam = render_camera{
            .proj = math::perspective(16.0F / 9.0F, 1.0F, 0.1F, 100.0F),
            .inv_proj = math::inverse(math::perspective(16.0F / 9.0F, 1.0F, 0.1F, 100.0F)),
            .view = math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F}, math::vec3<float>{0.0F, 1.0F, 0.0F}),
            .inv_view = math::inverse(math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F}, math::vec3<float>{0.0F, 1.0F, 0.0F})),
            .eye_position = {0.0F, 0.0F, -5.0F, 1.0F},
        };

        const auto cluster_count = 16U * 9U * 24U;
        auto cluster_bounds_buf = graph.create_buffer(render_graph::rg_buffer_desc{
            .size = cluster_count * sizeof(cluster_bounds),
            .usage = rhi::buffer_usage::storage_buffer,
            .name = "ClusterBoundsBuffer",
        });

        auto lights_buf = graph.create_buffer(render_graph::rg_buffer_desc{
            .size = 256 * sizeof(light_payload),
            .usage = rhi::buffer_usage::storage_buffer,
            .name = "LightsBuffer",
        });

        auto light_grid_buf = graph.create_buffer(render_graph::rg_buffer_desc{
            .size = cluster_count * sizeof(light_grid_range),
            .usage = rhi::buffer_usage::storage_buffer,
            .name = "LightGridBuffer",
        });

        auto light_indices_buf = graph.create_buffer(render_graph::rg_buffer_desc{
            .size = cluster_count * 128 * sizeof(uint32_t),
            .usage = rhi::buffer_usage::storage_buffer,
            .name = "LightIndicesBuffer",
        });

        auto global_count_buf = graph.create_buffer(render_graph::rg_buffer_desc{
            .size = sizeof(uint32_t) * 4,
            .usage = rhi::buffer_usage::storage_buffer,
            .name = "GlobalIndexCountBuffer",
        });

        const auto& cluster_data = add_light_clustering_pass(graph, pool, shaders, cluster_bounds_buf, cam, 1280, 720);
        add_light_culling_pass(graph, pool, shaders, cluster_data.cluster_bounds_buffer,
                              lights_buf, light_grid_buf, light_indices_buf, global_count_buf,
                              cluster_data.create_info, 0);

        auto res = graph.execute(*dev);
        EXPECT_TRUE(res.has_value());

        dev->wait_idle();
    }

    TEST(render_system_tests, transparency_gather_and_blend_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev};
        auto graph = render_graph::render_graph{1280, 720};

        auto accum_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(1280, 720),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "TransparencyAccum",
        });

        auto depth_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(1280, 720),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "DepthTarget",
        });

        auto hdr_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(1280, 720),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "HDRColorTarget",
        });

        add_frame_upload_pass(graph, pool);
        const auto& depth_data = add_depth_prepass(graph, pool, shaders, depth_tex, 0);
        const auto& gather_data = add_transparency_gather_pass(graph, pool, shaders, accum_tex,
                                                               depth_data.depth_texture, nullopt, 0);
        add_transparency_blend_pass(graph, pool, shaders, gather_data.accum_texture, hdr_tex);

        auto res = graph.execute(*dev);
        EXPECT_TRUE(res.has_value());

        dev->wait_idle();
    }

    TEST(render_system_tests, shader_manager_handle_registration_and_slot_lookup)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto shaders = shader_manager{*dev};

        auto vs = shaders.register_shader_module("pbr.vert.spv", rhi::shader_stage::vertex, "VSMain");
        auto fs = shaders.register_shader_module("pbr.frag.spv", rhi::shader_stage::fragment, "FSMain");
        EXPECT_GT(vs.id, 0U);
        EXPECT_GT(fs.id, 0U);
        EXPECT_NE(vs.id, fs.id);

        // Idempotent re-registration of same module returns identical handle
        auto vs_dup = shaders.register_shader_module("pbr.vert.spv", rhi::shader_stage::vertex, "VSMain");
        EXPECT_EQ(vs.id, vs_dup.id);

        auto stages = array{vs, fs};
        auto color_formats = array{rhi::data_format::rgba16_float};
        auto tmpl = graphics_pipeline_template{
            .shader_modules = span<const shader_module_handle>{stages.data(), stages.size()},
            .color_attachment_formats = span<const rhi::data_format>{color_formats.data(), color_formats.size()},
            .depth_stencil_attachment_format = rhi::data_format::depth32_float,
            .primitive_topology = rhi::primitive_topology::triangle_list,
        };

        auto pipe_h = shaders.register_graphics_pipeline("test_pbr_pipeline", tmpl);
        EXPECT_GT(pipe_h.id, 0U);

        // Idempotent re-registration returns identical handle
        auto pipe_dup = shaders.register_graphics_pipeline("test_pbr_pipeline", tmpl);
        EXPECT_EQ(pipe_h.id, pipe_dup.id);

        // Find by name returns the registered handle
        auto found_pipe = shaders.find_graphics_pipeline("test_pbr_pipeline");
        ASSERT_TRUE(found_pipe.has_value());
        EXPECT_EQ(found_pipe->id, pipe_h.id);

        // O(1) slot lookup yields a valid RHI pipeline handle
        auto rhi_pipe = shaders.get_rhi_pipeline(pipe_h);
        EXPECT_NE(rhi_pipe.handle, 0ULL);

        // Compute pipeline registration and lookup
        auto cs = shaders.register_shader_module("build_cluster_grid.comp.spv", rhi::shader_stage::compute, "CSMain");
        EXPECT_GT(cs.id, 0U);

        auto comp_tmpl = compute_pipeline_template{
            .shader_module = cs,
        };
        auto comp_pipe_h = shaders.register_compute_pipeline("test_comp_pipeline", comp_tmpl);
        EXPECT_GT(comp_pipe_h.id, 0U);

        auto found_comp = shaders.find_compute_pipeline("test_comp_pipeline");
        ASSERT_TRUE(found_comp.has_value());
        EXPECT_EQ(found_comp->id, comp_pipe_h.id);

        auto rhi_comp_pipe = shaders.get_rhi_pipeline(comp_pipe_h);
        EXPECT_NE(rhi_comp_pipe.handle, 0ULL);

        dev->wait_idle();
    }

    TEST(render_system_tests, shader_manager_memory_blob_ingestion)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto shaders = shader_manager{*dev};

        // Load bytecode into memory first
        auto vs_bytes = shaders.load_shader_bytecode("skybox.vert.spv");
        auto fs_bytes = shaders.load_shader_bytecode("skybox.frag.spv");
        ASSERT_FALSE(vs_bytes.empty());
        ASSERT_FALSE(fs_bytes.empty());

        // Ingest memory blob with no disk_location
        auto vs_info = shader_module_create_info{
            .stage = rhi::shader_stage::vertex,
            .entry_point = "VSMain",
            .disk_location = nullopt,
            .initial_bytes = span<const byte>{vs_bytes.data(), vs_bytes.size()},
        };
        auto fs_info = shader_module_create_info{
            .stage = rhi::shader_stage::fragment,
            .entry_point = "FSMain",
            .disk_location = nullopt,
            .initial_bytes = span<const byte>{fs_bytes.data(), fs_bytes.size()},
        };

        auto vs_h = shaders.register_shader_module(vs_info);
        auto fs_h = shaders.register_shader_module(fs_info);
        EXPECT_GT(vs_h.id, 0U);
        EXPECT_GT(fs_h.id, 0U);

        auto stages = array{vs_h, fs_h};
        auto color_formats = array{rhi::data_format::rgba16_float};
        auto tmpl = graphics_pipeline_template{
            .shader_modules = span<const shader_module_handle>{stages.data(), stages.size()},
            .color_attachment_formats = span<const rhi::data_format>{color_formats.data(), color_formats.size()},
            .primitive_topology = rhi::primitive_topology::triangle_list,
        };

        auto pipe_h = shaders.register_graphics_pipeline("blob_skybox_pipeline", tmpl);
        EXPECT_GT(pipe_h.id, 0U);

        auto rhi_pipe = shaders.get_rhi_pipeline(pipe_h);
        EXPECT_NE(rhi_pipe.handle, 0ULL);

        dev->wait_idle();
    }

    TEST(render_system_tests, shader_manager_hot_reloading_and_inverted_dependency_update)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto shaders = shader_manager{*dev};

        auto vs = shaders.register_shader_module("pbr.vert.spv", rhi::shader_stage::vertex, "VSMain");
        auto fs = shaders.register_shader_module("pbr.frag.spv", rhi::shader_stage::fragment, "FSMain");

        auto stages = array{vs, fs};
        auto color_formats = array{rhi::data_format::rgba16_float};

        auto tmpl1 = graphics_pipeline_template{
            .shader_modules = span<const shader_module_handle>{stages.data(), stages.size()},
            .color_attachment_formats = span<const rhi::data_format>{color_formats.data(), color_formats.size()},
            .primitive_topology = rhi::primitive_topology::triangle_list,
        };
        auto pipe1 = shaders.register_graphics_pipeline("pipe1", tmpl1);

        auto tmpl2 = graphics_pipeline_template{
            .shader_modules = span<const shader_module_handle>{stages.data(), stages.size()},
            .color_attachment_formats = span<const rhi::data_format>{color_formats.data(), color_formats.size()},
            .primitive_topology = rhi::primitive_topology::triangle_list,
        };
        auto pipe2 = shaders.register_graphics_pipeline("pipe2", tmpl2);

        auto initial_rhi1 = shaders.get_rhi_pipeline(pipe1);
        auto initial_rhi2 = shaders.get_rhi_pipeline(pipe2);
        EXPECT_NE(initial_rhi1.handle, 0ULL);
        EXPECT_NE(initial_rhi2.handle, 0ULL);

        // Reload module from disk via reload_shader_module
        auto reload_ok = shaders.reload_shader_module(fs);
        EXPECT_TRUE(reload_ok);

        auto reloaded_rhi1 = shaders.get_rhi_pipeline(pipe1);
        auto reloaded_rhi2 = shaders.get_rhi_pipeline(pipe2);
        EXPECT_NE(reloaded_rhi1.handle, 0ULL);
        EXPECT_NE(reloaded_rhi2.handle, 0ULL);

        // Notify file changed triggers surgical reload
        auto notify_ok = shaders.notify_file_changed(std::filesystem::path("pbr.frag.spv"));
        EXPECT_TRUE(notify_ok);

        // Drain retired pipelines
        shaders.process_deferred_retirements();

        dev->wait_idle();
    }

    TEST(render_system_tests, shader_manager_fail_safe_recompilation_retains_last_known_good)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto shaders = shader_manager{*dev};

        auto vs = shaders.register_shader_module("pbr.vert.spv", rhi::shader_stage::vertex, "VSMain");
        auto fs = shaders.register_shader_module("pbr.frag.spv", rhi::shader_stage::fragment, "FSMain");

        auto stages = array{vs, fs};
        auto color_formats = array{rhi::data_format::rgba16_float};
        auto tmpl = graphics_pipeline_template{
            .shader_modules = span<const shader_module_handle>{stages.data(), stages.size()},
            .color_attachment_formats = span<const rhi::data_format>{color_formats.data(), color_formats.size()},
            .primitive_topology = rhi::primitive_topology::triangle_list,
        };
        auto pipe = shaders.register_graphics_pipeline("fail_safe_pipe", tmpl);

        auto initial_rhi = shaders.get_rhi_pipeline(pipe);
        EXPECT_NE(initial_rhi.handle, 0ULL);

        // Corrupt / invalid bytecode update
        auto invalid_bytes = array<byte, 8>{byte{0x01}, byte{0x02}, byte{0x03}, byte{0x04},
                                            byte{0x05}, byte{0x06}, byte{0x07}, byte{0x08}};
        auto update_result = shaders.update_shader_module_bytes(fs, span<const byte>{invalid_bytes.data(), invalid_bytes.size()});
        EXPECT_FALSE(update_result);

        // The pipeline MUST retain its last known good valid handle!
        auto retained_rhi = shaders.get_rhi_pipeline(pipe);
        EXPECT_EQ(retained_rhi.handle, initial_rhi.handle);

        dev->wait_idle();
    }

    TEST(render_system_tests, renderer_draw_command_partitioning_by_material_type)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            auto meshes = core::mesh_registry{};
            auto materials = core::material_registry{};
            auto textures = core::texture_registry{};

            auto mesh_id = meshes.register_mesh(create_test_mesh());

            // Create 5 materials:
            // 0: Opaque
            auto mat_opaque_1 = core::material{};
            mat_opaque_1.set_string(core::material::alpha_mode_name, "OPAQUE");
            auto mat_opaque_1_id = materials.register_material(tempest::move(mat_opaque_1));

            // 1: Mask
            auto mat_mask = core::material{};
            mat_mask.set_string(core::material::alpha_mode_name, "MASK");
            auto mat_mask_id = materials.register_material(tempest::move(mat_mask));

            // 2: Blend (Transparent)
            auto mat_blend = core::material{};
            mat_blend.set_string(core::material::alpha_mode_name, "BLEND");
            auto mat_blend_id = materials.register_material(tempest::move(mat_blend));

            // 3: Opaque
            auto mat_opaque_2 = core::material{};
            mat_opaque_2.set_string(core::material::alpha_mode_name, "OPAQUE");
            auto mat_opaque_2_id = materials.register_material(tempest::move(mat_opaque_2));

            // 4: Transmissive
            auto mat_trans = core::material{};
            mat_trans.set_string(core::material::alpha_mode_name, "TRANSMISSIVE");
            auto mat_trans_id = materials.register_material(tempest::move(mat_trans));

            // Create 5 entities in interleaved order:
            // ent0: OPAQUE (shadow contributor)
            auto ent0 = registry.create();
            registry.assign(ent0, renderable_component{.mesh_id = mesh_id, .material_id = mat_opaque_1_id, .double_sided = false});
            registry.assign(ent0, ecs::transform_component::identity());

            // ent1: BLEND (transparent)
            auto ent1 = registry.create();
            registry.assign(ent1, renderable_component{.mesh_id = mesh_id, .material_id = mat_blend_id, .double_sided = false});
            registry.assign(ent1, ecs::transform_component::identity());

            // ent2: MASK (shadow contributor)
            auto ent2 = registry.create();
            registry.assign(ent2, renderable_component{.mesh_id = mesh_id, .material_id = mat_mask_id, .double_sided = false});
            registry.assign(ent2, ecs::transform_component::identity());

            // ent3: TRANSMISSIVE (transparent)
            auto ent3 = registry.create();
            registry.assign(ent3, renderable_component{.mesh_id = mesh_id, .material_id = mat_trans_id, .double_sided = false});
            registry.assign(ent3, ecs::transform_component::identity());

            // ent4: OPAQUE (shadow contributor)
            auto ent4 = registry.create();
            registry.assign(ent4, renderable_component{.mesh_id = mesh_id, .material_id = mat_opaque_2_id, .double_sided = false});
            registry.assign(ent4, ecs::transform_component::identity());

            auto entities = array<ecs::entity, 5>{ent0, ent1, ent2, ent3, ent4};
            rend->upload_objects_sync(span<const ecs::entity>{entities.data(), entities.size()},
                                      meshes, textures, materials);

            // Assert draw counts:
            // 3 shadow contributors (ent0, ent2, ent4)
            // 5 total active draws
            EXPECT_EQ(rend->get_shadow_draw_count(), 3U);
            EXPECT_EQ(rend->get_active_draw_count(), 5U);

            // Validate the mapped GPU buffers
            auto& pool = rend->get_resource_pool();
            auto* cmds = static_cast<const indexed_indirect_command*>(pool.get_draw_commands_buffer().cpu_address);
            auto* instances = static_cast<const uint32_t*>(pool.get_instance_buffer().cpu_address);
            auto* objects = static_cast<const object_payload*>(pool.get_object_buffer().cpu_address);

            ASSERT_NE(cmds, nullptr);
            ASSERT_NE(instances, nullptr);
            ASSERT_NE(objects, nullptr);

            for (uint32_t i = 0; i < 5; ++i)
            {
                EXPECT_EQ(cmds[i].first_instance, i);
                EXPECT_EQ(cmds[i].instance_count, 1U);
                EXPECT_EQ(instances[i], i);
            }

            // The first 3 objects must be ent0, ent2, ent4 (OPAQUE / MASK)
            auto shadow_contributor_entities = array<uint32_t, 3>{
                static_cast<uint32_t>(ent0),
                static_cast<uint32_t>(ent2),
                static_cast<uint32_t>(ent4),
            };
            for (uint32_t i = 0; i < 3; ++i)
            {
                auto entity_id = objects[instances[i]].self_id;
                auto is_contributor = std::find(shadow_contributor_entities.begin(), shadow_contributor_entities.end(), entity_id) != shadow_contributor_entities.end();
                EXPECT_TRUE(is_contributor);
            }

            // The last 2 objects must be ent1, ent3 (BLEND / TRANSMISSIVE)
            auto transparent_entities = array<uint32_t, 2>{
                static_cast<uint32_t>(ent1),
                static_cast<uint32_t>(ent3),
            };
            for (uint32_t i = 3; i < 5; ++i)
            {
                auto entity_id = objects[instances[i]].self_id;
                auto is_transparent = std::find(transparent_entities.begin(), transparent_entities.end(), entity_id) != transparent_entities.end();
                EXPECT_TRUE(is_transparent);
            }
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, renderer_shadowed_directional_light_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 256,
            .render_height = 256,
            .shadow_atlas_width = 512,
            .shadow_atlas_height = 512,
            .enable_shadows = true,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 1. Camera Entity
            auto cam_ent = registry.create();
            registry.assign(cam_ent, camera_component{
                .aspect_ratio = 1.0F,
                .vertical_fov = 1.5707963F,
                .near_plane = 0.01F,
            });
            auto cam_tx = ecs::transform_component::identity();
            cam_tx.position({0.0F, 0.0F, -5.0F});
            registry.assign(cam_ent, cam_tx);
            registry.assign(cam_ent, active_camera_component{});

            // 2. Directional Sun Light with Shadows
            auto sun_ent = registry.create();
            registry.assign(sun_ent, directional_light_component{
                .color = {1.0F, 1.0F, 1.0F},
                .intensity = 3.0F,
            });
            registry.assign(sun_ent, shadow_map_component{
                .shadow_distance = 50.0F,
                .split_lambda = 0.85F,
                .blend_fraction = 0.1F,
                .cascade_count = 2,
            });
            registry.assign(sun_ent, ecs::transform_component::identity());

            // 3. Geometry (Opaque Plane)
            auto meshes = core::mesh_registry{};
            auto materials = core::material_registry{};
            auto textures = core::texture_registry{};

            auto mesh_id = meshes.register_mesh(create_test_mesh());
            auto mat = core::material{};
            mat.set_vec4(core::material::base_color_factor_name, {0.9F, 0.9F, 0.9F, 1.0F});
            mat.set_scalar(core::material::metallic_factor_name, 0.0F);
            mat.set_scalar(core::material::roughness_factor_name, 0.5F);
            auto mat_id = materials.register_material(tempest::move(mat));

            auto geom_ent = registry.create();
            registry.assign(geom_ent, renderable_component{
                .mesh_id = mesh_id,
                .material_id = mat_id,
                .double_sided = true,
            });
            registry.assign(geom_ent, ecs::transform_component::identity());

            // 4. Upload & Prepare Frame
            auto entities = array<ecs::entity, 1>{geom_ent};
            rend->upload_objects_sync(span<const ecs::entity>{entities.data(), entities.size()},
                                      meshes, textures, materials);

            rend->prepare_frame(256, 256);

            // Verify shadow atlas texture is allocated
            EXPECT_TRUE(rend->get_shadow_atlas_texture().has_value());

            // Verify ShadowMapPass is active in the compiled DAG (not culled)
            auto compile_res = rend->get_render_graph().compile();
            ASSERT_TRUE(compile_res.has_value());
            const auto& all_passes = rend->get_render_graph().get_compiler().get_passes();
            bool shadow_pass_executed = false;
            for (auto pass_idx : compile_res->sorted_pass_indices)
            {
                if (all_passes[pass_idx].name == "ShadowMapPass")
                {
                    shadow_pass_executed = true;
                    break;
                }
            }
            EXPECT_TRUE(shadow_pass_executed);

            // 5. Render execution
            auto render_res = rend->render();
            EXPECT_TRUE(render_res.has_value());

            dev->wait_idle();

            // 6. Readback tonemapped color pixels
            auto readback_buf = dev->create_buffer(rhi::buffer_desc{
                .size = 256 * 256 * 4,
                .memory_usage = rhi::memory_usage::readback,
                .usage = rhi::buffer_usage::transfer_dst,
                .name = "FrameReadbackBuffer",
            });

            const auto* alloc = rend->get_render_graph().get_physical_texture(rend->get_tonemapped_color_texture().id);
            ASSERT_NE(alloc, nullptr);
            if (alloc)
            {
                auto& port = dev->get_graphics_execution_port();
                auto& cmd = port.acquire_command_list();
                cmd.begin();

                const auto region = rhi::buffer_texture_copy_region{
                    .buffer_offset = 0,
                    .buffer_row_length = 0,
                    .buffer_image_height = 0,
                    .mip_level = 0,
                    .base_array_layer = 0,
                    .array_layer_count = 1,
                    .image_offset_x = 0,
                    .image_offset_y = 0,
                    .image_offset_z = 0,
                    .image_extent_width = 256,
                    .image_extent_height = 256,
                    .image_extent_depth = 1,
                };

                cmd.copy_texture_to_buffer(alloc->handle, readback_buf, span<const rhi::buffer_texture_copy_region>{&region, 1});
                cmd.end();
                auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
                [[maybe_unused]] auto submit_res = port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
                dev->wait_idle();

                auto* pixels = static_cast<const uint8_t*>(readback_buf.cpu_address);
                ASSERT_NE(pixels, nullptr);

                auto non_zero_pixels = 0U;
                for (size_t i = 0; i < 256 * 256 * 4; i += 4)
                {
                    if (pixels[i] > 0 || pixels[i + 1] > 0 || pixels[i + 2] > 0)
                    {
                        non_zero_pixels++;
                    }
                }
                EXPECT_GT(non_zero_pixels, 0U);
            }

            dev->destroy_buffer(readback_buf);

            // 7. Readback shadow map atlas depth pixels to verify geometry was rasterized into the shadow map
            auto shadow_readback_buf = dev->create_buffer(rhi::buffer_desc{
                .size = 256 * 256 * sizeof(float),
                .memory_usage = rhi::memory_usage::readback,
                .usage = rhi::buffer_usage::transfer_dst,
                .name = "ShadowAtlasReadbackBuffer",
            });

            const auto* shadow_alloc = rend->get_render_graph().get_physical_texture(rend->get_shadow_atlas_texture()->id);
            ASSERT_NE(shadow_alloc, nullptr);
            if (shadow_alloc)
            {
                auto& port = dev->get_graphics_execution_port();
                auto& cmd = port.acquire_command_list();
                cmd.begin();

                const auto region = rhi::buffer_texture_copy_region{
                    .buffer_offset = 0,
                    .buffer_row_length = 0,
                    .buffer_image_height = 0,
                    .mip_level = 0,
                    .base_array_layer = 0,
                    .array_layer_count = 1,
                    .image_offset_x = 0,
                    .image_offset_y = 0,
                    .image_offset_z = 0,
                    .image_extent_width = 256,
                    .image_extent_height = 256,
                    .image_extent_depth = 1,
                };

                cmd.copy_texture_to_buffer(shadow_alloc->handle, shadow_readback_buf, span<const rhi::buffer_texture_copy_region>{&region, 1});
                cmd.end();
                auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
                [[maybe_unused]] auto submit_res = port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
                dev->wait_idle();

                auto* shadow_depths = static_cast<const float*>(shadow_readback_buf.cpu_address);
                ASSERT_NE(shadow_depths, nullptr);

                auto non_zero_depth_pixels = 0U;
                for (size_t i = 0; i < 256 * 256; ++i)
                {
                    if (shadow_depths[i] > 0.0F)
                    {
                        non_zero_depth_pixels++;
                    }
                }
                EXPECT_GT(non_zero_depth_pixels, 0U);
            }

            dev->destroy_buffer(shadow_readback_buf);
        }

        dev->wait_idle();
    }
} // namespace tempest::render_system::tests
