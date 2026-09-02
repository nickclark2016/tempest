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
#include <tempest/render_system/passes/shadow_pass.hpp>
#include <tempest/render_system/passes/skybox_pass.hpp>
#include <tempest/render_system/passes/ssao_blur_pass.hpp>
#include <tempest/render_system/passes/ssao_pass.hpp>
#include <tempest/render_system/passes/tonemapping_pass.hpp>
#include <tempest/render_system/passes/transparency_blend_pass.hpp>
#include <tempest/render_system/passes/transparency_clear_pass.hpp>
#include <tempest/render_system/passes/transparency_gather_pass.hpp>
#include <tempest/render_system/passes/transparency_resolve_pass.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/render_system/renderer.hpp>
#include <tempest/render_system/resource_pool.hpp>
#include <tempest/render_system/shader_manager.hpp>
#include <tempest/render_system/shelf_allocator.hpp>
#include <tempest/rhi.hpp>
#include <tempest/transform_component.hpp>

namespace tempest::render_system::tests
{
    namespace
    {
        struct test_fixture
        {
            unique_ptr<rhi::context> ctx{};
            unique_ptr<rhi::device> dev{};
            assets::asset_database asset_db{nullptr};
        };

        auto create_test_asset_database() -> assets::asset_database
        {
            auto db = assets::asset_database{};
            assets::mount_default_shader_roots(db);
            db.scan_and_index();
            return db;
        }

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
                return test_fixture{};
            }

            auto ctx = tempest::move(ctx_res).value();
            auto devices = ctx->enumerate_devices();
            if (devices.empty())
            {
                return test_fixture{};
            }

            auto dev = ctx->create_device(devices[0].device_uuid);
            auto asset_db = create_test_asset_database();
            return test_fixture{
                .ctx = tempest::move(ctx),
                .dev = tempest::move(dev),
                .asset_db = tempest::move(asset_db),
            };
        }

        auto create_test_mesh() -> core::mesh
        {
            auto m = core::mesh{};
            m.vertices.push_back(core::vertex{.position = {-1.0F, -1.0F, 0.0F},
                                              .uv = {0.0F, 0.0F},
                                              .normal = {0.0F, 0.0F, 1.0F},
                                              .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                              .color = {1.0F, 1.0F, 1.0F, 1.0F}});
            m.vertices.push_back(core::vertex{.position = {1.0F, -1.0F, 0.0F},
                                              .uv = {1.0F, 0.0F},
                                              .normal = {0.0F, 0.0F, 1.0F},
                                              .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                              .color = {1.0F, 1.0F, 1.0F, 1.0F}});
            m.vertices.push_back(core::vertex{.position = {1.0F, 1.0F, 0.0F},
                                              .uv = {1.0F, 1.0F},
                                              .normal = {0.0F, 0.0F, 1.0F},
                                              .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                              .color = {1.0F, 1.0F, 1.0F, 1.0F}});
            m.vertices.push_back(core::vertex{.position = {-1.0F, 1.0F, 0.0F},
                                              .uv = {0.0F, 1.0F},
                                              .normal = {0.0F, 0.0F, 1.0F},
                                              .tangent = {1.0F, 0.0F, 0.0F, 1.0F},
                                              .color = {1.0F, 1.0F, 1.0F, 1.0F}});
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

    TEST(render_system_tests, camera_system_single_camera_fallback)
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
    }

    TEST(render_system_tests, camera_system_explicit_possession_and_switching)
    {
        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto cam_sys = camera_system{registry, events};

        auto cam1 = registry.create();
        registry.assign(cam1, camera_component{.aspect_ratio = 16.0F / 9.0F, .vertical_fov = 1.0F, .near_plane = 0.1F});
        registry.assign(cam1, ecs::transform_component::identity());

        auto cam2 = registry.create();
        registry.assign(cam2, camera_component{.aspect_ratio = 4.0F / 3.0F, .vertical_fov = 0.8F, .near_plane = 0.5F});
        registry.assign(cam2, ecs::transform_component::identity());

        // Default fallback picks cam1
        auto active_entity = cam_sys.get_active_camera_entity();
        ASSERT_TRUE(active_entity.has_value());
        EXPECT_EQ(*active_entity, cam1);

        // Explicitly possess cam2
        cam_sys.set_active_camera(cam2);
        active_entity = cam_sys.get_active_camera_entity();
        ASSERT_TRUE(active_entity.has_value());
        EXPECT_EQ(*active_entity, cam2);

        auto render_cam = cam_sys.get_active_camera();
        ASSERT_TRUE(render_cam.has_value());
        EXPECT_NE(render_cam->proj[0][0], 0.0F);

        // Clearing possession reverts to cam1 fallback
        cam_sys.clear_active_camera();
        active_entity = cam_sys.get_active_camera_entity();
        ASSERT_TRUE(active_entity.has_value());
        EXPECT_EQ(*active_entity, cam1);
    }

    TEST(render_system_tests, camera_system_inactive_and_render_texture_cameras_ignored)
    {
        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto cam_sys = camera_system{registry, events};

        // Inactive camera
        auto cam_inactive = registry.create();
        registry.assign(cam_inactive, camera_component{
                                          .aspect_ratio = 16.0F / 9.0F,
                                          .vertical_fov = 1.0F,
                                          .near_plane = 0.1F,
                                          .is_active = false,
                                      });
        registry.assign(cam_inactive, ecs::transform_component::identity());

        // Offscreen render texture camera
        auto cam_tex = registry.create();
        registry.assign(cam_tex, camera_component{
                                     .aspect_ratio = 1.0F,
                                     .vertical_fov = 1.0F,
                                     .near_plane = 0.1F,
                                     .target = camera_target_type::render_texture,
                                     .is_active = true,
                                 });
        registry.assign(cam_tex, ecs::transform_component::identity());

        // Viewport camera
        auto cam_viewport = registry.create();
        registry.assign(cam_viewport, camera_component{
                                          .aspect_ratio = 16.0F / 9.0F,
                                          .vertical_fov = 1.0F,
                                          .near_plane = 0.1F,
                                          .target = camera_target_type::viewport,
                                          .is_active = true,
                                      });
        registry.assign(cam_viewport, ecs::transform_component::identity());

        auto active_entity = cam_sys.get_active_camera_entity();
        ASSERT_TRUE(active_entity.has_value());
        EXPECT_EQ(*active_entity, cam_viewport);
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
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
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

            // 2. Setup Sun Light Entity
            auto sun_ent = registry.create();
            registry.assign(sun_ent, directional_light_component{
                                         .color = {1.0F, 1.0F, 1.0F},
                                         .intensity = 2.0F,
                                     });
            registry.assign(sun_ent, ecs::transform_component::identity());

            // 3. Setup Renderable Geometry Entity
            auto mesh_id = meshes.register_mesh(create_test_mesh());
            auto mat = core::material{};
            mat.set_vec4(core::material::base_color_factor_name, {0.8F, 0.2F, 0.2F, 1.0F});
            mat.set_scalar(core::material::metallic_factor_name, 0.0F);
            mat.set_scalar(core::material::roughness_factor_name, 0.5F);
            auto mat_id = materials.register_material(tempest::move(mat));

            auto geom_ent = registry.create();
            registry.assign(geom_ent, core::mesh_component{.mesh_id = mesh_id});
            registry.assign(geom_ent, core::material_component{.material_id = mat_id});
            registry.assign(geom_ent, ecs::transform_component::identity());

            // 4. Prepare Frame
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
                [[maybe_unused]] auto submit_res =
                    port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
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
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
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

            // Setup Sun Light
            auto sun = registry.create();
            registry.assign(sun, directional_light_component{
                                     .color = {1.0F, 0.98F, 0.92F},
                                     .intensity = 7.0F,
                                 });
            auto sun_tx = ecs::transform_component::identity();
            sun_tx.rotation({math::as_radians(60.0F), math::as_radians(40.0F), 0.0F});
            registry.assign(sun, sun_tx);

            auto asset_type_reg = assets::asset_type_registry{};
            auto asset_db = assets::asset_database{&asset_type_reg};
            assets::register_default_importers(asset_db, &meshes, &textures, &materials);

            const auto sponza_path = "assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf";
            if (std::filesystem::exists(sponza_path))
            {
                [[maybe_unused]] auto prefab_root = asset_db.load(sponza_path, registry);
            }
            else
            {
                // If no Sponza entities loaded, fall back to procedural quad entities
                auto mesh_id = meshes.register_mesh(create_test_mesh());
                auto mat = core::material{};
                mat.set_vec4(core::material::base_color_factor_name, {0.7F, 0.7F, 0.7F, 1.0F});
                auto mat_id = materials.register_material(tempest::move(mat));

                auto ent = registry.create();
                registry.assign(ent, core::mesh_component{.mesh_id = mesh_id});
                registry.assign(ent, core::material_component{.material_id = mat_id});
                registry.assign(ent, ecs::transform_component::identity());
            }

            rend->prepare_frame(1280, 720);

            auto res = rend->render();
            EXPECT_TRUE(res.has_value());

            dev->wait_idle();
        }
    }

    TEST(render_system_tests, light_clustering_best_fit_aspect_ratios)
    {
        // 16:9 Standard widescreen (1920x1080, 1280x720)
        auto dims16_9 = compute_cluster_grid_dimensions(1920, 1080);
        EXPECT_EQ(dims16_9.x, 16U);
        EXPECT_EQ(dims16_9.y, 9U);
        EXPECT_EQ(dims16_9.z, 24U);
        EXPECT_EQ(dims16_9.w, 120U);

        auto dims720p = compute_cluster_grid_dimensions(1280, 720);
        EXPECT_EQ(dims720p.x, 16U);
        EXPECT_EQ(dims720p.y, 9U);
        EXPECT_EQ(dims720p.z, 24U);
        EXPECT_EQ(dims720p.w, 80U);

        // 21:9 Ultrawide (2560x1080, 3440x1440)
        auto dims21_9 = compute_cluster_grid_dimensions(2560, 1080);
        EXPECT_EQ(dims21_9.x, 21U);
        EXPECT_EQ(dims21_9.y, 9U);
        EXPECT_EQ(dims21_9.z, 24U);
        EXPECT_EQ(dims21_9.w, (2560U + 21U - 1U) / 21U);

        auto dimsUWQHD = compute_cluster_grid_dimensions(3440, 1440);
        EXPECT_EQ(dimsUWQHD.x, 21U);
        EXPECT_EQ(dimsUWQHD.y, 9U);
        EXPECT_EQ(dimsUWQHD.z, 24U);

        // 32:9 Super-ultrawide (5120x1440)
        auto dims32_9 = compute_cluster_grid_dimensions(5120, 1440);
        EXPECT_EQ(dims32_9.x, 32U);
        EXPECT_EQ(dims32_9.y, 9U);
        EXPECT_EQ(dims32_9.z, 24U);
        EXPECT_EQ(dims32_9.w, 160U);

        // 16:10 Widescreen (1920x1200)
        auto dims16_10 = compute_cluster_grid_dimensions(1920, 1200);
        EXPECT_EQ(dims16_10.x, 16U);
        EXPECT_EQ(dims16_10.y, 10U);
        EXPECT_EQ(dims16_10.z, 24U);
        EXPECT_EQ(dims16_10.w, 120U);

        // 4:3 Standard (1024x768)
        auto dims4_3 = compute_cluster_grid_dimensions(1024, 768);
        EXPECT_EQ(dims4_3.x, 16U);
        EXPECT_EQ(dims4_3.y, 12U);
        EXPECT_EQ(dims4_3.z, 24U);
        EXPECT_EQ(dims4_3.w, 64U);

        // Portrait 9:16 (1080x1920)
        auto dims9_16 = compute_cluster_grid_dimensions(1080, 1920);
        EXPECT_EQ(dims9_16.x, 5U);
        EXPECT_EQ(dims9_16.y, 9U);
        EXPECT_EQ(dims9_16.z, 24U);
        EXPECT_EQ(dims9_16.w, 216U);
    }

    TEST(render_system_tests, light_clustering_bitmask_culling_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        auto graph = render_graph::render_graph{1280, 720};

        auto cam = render_camera{
            .proj = math::perspective(16.0F / 9.0F, 1.0F, 0.1F, 100.0F),
            .inv_proj = math::inverse(math::perspective(16.0F / 9.0F, 1.0F, 0.1F, 100.0F)),
            .view = math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                  math::vec3<float>{0.0F, 1.0F, 0.0F}),
            .inv_view =
                math::inverse(math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                            math::vec3<float>{0.0F, 1.0F, 0.0F})),
            .eye_position = {0.0F, 0.0F, -5.0F, 1.0F},
        };

        // Write 3 test lights to pool:
        // Light 0: Center near light (world pos 0, 0, -4 -> view pos 0, 0, -1, range 1.5)
        // Light 1: Far corner light (world pos 5, 3, 25 -> view pos 5, 3, -30, range 10)
        // Light 2: Behind camera (world pos 0, 0, -10 -> view pos 0, 0, +5, range 2.0)
        auto test_lights = array<light_payload, 3>{
            light_payload{
                .color_intensity = {1.0F, 1.0F, 1.0F, 1.0F},
                .position_falloff = {0.0F, 0.0F, -4.0F, 1.5F},
                .direction_angle = {0.0F, -1.0F, 0.0F, 0.0F},
                .type = 1,
                .enabled = 1,
                .padding = {0, 0},
            },
            light_payload{
                .color_intensity = {1.0F, 0.0F, 0.0F, 1.0F},
                .position_falloff = {5.0F, 3.0F, 25.0F, 10.0F},
                .direction_angle = {0.0F, -1.0F, 0.0F, 0.0F},
                .type = 1,
                .enabled = 1,
                .padding = {0, 0},
            },
            light_payload{
                .color_intensity = {0.0F, 1.0F, 0.0F, 1.0F},
                .position_falloff = {0.0F, 0.0F, -10.0F, 2.0F},
                .direction_angle = {0.0F, -1.0F, 0.0F, 0.0F},
                .type = 1,
                .enabled = 1,
                .padding = {0, 0},
            },
        };

        pool.write_lights(span<const light_payload>{test_lights.data(), test_lights.size()});

        auto scene = scene_constants{
            .projection = cam.proj,
            .inv_projection = cam.inv_proj,
            .view = cam.view,
            .inv_view = cam.inv_view,
            .camera_position = cam.eye_position,
            .lights_address = pool.get_lights_buffer_address(),
            .light_count = static_cast<uint32_t>(test_lights.size()),
            .words_per_cluster = 1,
            .cluster_counts_tile_size = compute_cluster_grid_dimensions(1280, 720),
            .cluster_depth_params = {0.1F, 100.0F, 0.0F, 0.0F},
        };
        pool.write_scene_constants(scene);

        const auto total_clusters = 16U * 9U * 24U;
        auto cluster_bounds_buf = graph.create_buffer(render_graph::rg_buffer_desc{
            .size = total_clusters * sizeof(cluster_bounds),
            .usage =
                rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address | rhi::buffer_usage::transfer_src,
            .name = "ClusterBoundsBuffer",
        });

        auto lights_buf = graph.import_buffer(pool.get_lights_buffer());

        const auto& cluster_data = add_light_clustering_pass(graph, pool, shaders, cluster_bounds_buf, cam, 1280, 720);
        const auto& culling_data =
            add_light_culling_pass(graph, pool, shaders, cluster_data.cluster_bounds_buffer, lights_buf,
                                   cluster_data.create_info, static_cast<uint32_t>(test_lights.size()));

        struct sink_pass_data
        {
            render_graph::rg_buffer_id bitmask_buf;
        };

        graph.add_compute_pass<sink_pass_data>(
            "BitmaskSinkPass",
            [bitmask_id = culling_data.light_bitmask_buffer](render_graph::pass_builder& builder,
                                                             sink_pass_data& data) {
                data.bitmask_buf = builder.read(bitmask_id, rhi::pipeline_stage::compute, rhi::resource_access::read);
                builder.mark_sink();
            },
            []([[maybe_unused]] const sink_pass_data& data, [[maybe_unused]] render_graph::pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        auto res = graph.execute(*dev);
        EXPECT_TRUE(res.has_value());
        dev->wait_idle();

        // Readback cluster bounds buffer
        auto cluster_readback = dev->create_buffer(rhi::buffer_desc{
            .size = total_clusters * sizeof(cluster_bounds),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "ClusterReadbackBuffer",
        });

        const auto* cluster_alloc = graph.get_physical_buffer(cluster_bounds_buf.id);
        ASSERT_NE(cluster_alloc, nullptr);
        if (cluster_alloc)
        {
            auto& port = dev->get_graphics_execution_port();
            auto& cmd = port.acquire_command_list();
            cmd.begin();
            const auto copy_region = rhi::buffer_copy_region{
                .src_offset = 0,
                .dst_offset = 0,
                .size = total_clusters * sizeof(cluster_bounds),
            };
            cmd.copy_buffer(cluster_alloc->handle, cluster_readback,
                            span<const rhi::buffer_copy_region>{&copy_region, 1});
            cmd.end();
            auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
            [[maybe_unused]] auto submit_res =
                port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
            dev->wait_idle();

            const auto* cb = static_cast<const cluster_bounds*>(cluster_readback.cpu_address);
            if (cb)
            {
                // Verify cluster 0 depth range is near plane [tile_near, tile_far]
                EXPECT_LT(cb[0].min_corner.z, 0.0F);
                EXPECT_LT(cb[0].max_corner.z, 0.0F);
                // Verify cluster 0 is on the left (x < 0) and top (y > 0) of view frustum
                EXPECT_LT(cb[0].min_corner.x, 0.0F);
                EXPECT_GT(cb[0].max_corner.y, 0.0F);
            }
        }
        dev->destroy_buffer(cluster_readback);

        // Readback bitmask buffer
        auto readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = total_clusters * sizeof(uint32_t),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "BitmaskReadbackBuffer",
        });

        const auto* bitmask_alloc = graph.get_physical_buffer(culling_data.light_bitmask_buffer.id);
        ASSERT_NE(bitmask_alloc, nullptr);
        if (bitmask_alloc)
        {
            auto& port = dev->get_graphics_execution_port();
            auto& cmd = port.acquire_command_list();
            cmd.begin();

            const auto copy_region = rhi::buffer_copy_region{
                .src_offset = 0,
                .dst_offset = 0,
                .size = total_clusters * sizeof(uint32_t),
            };
            cmd.copy_buffer(bitmask_alloc->handle, readback_buf, span<const rhi::buffer_copy_region>{&copy_region, 1});
            cmd.end();

            auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
            [[maybe_unused]] auto submit_res =
                port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
            dev->wait_idle();

            const auto* bitmasks = static_cast<const uint32_t*>(readback_buf.cpu_address);
            ASSERT_NE(bitmasks, nullptr);

            uint32_t count_light0 = 0;
            uint32_t count_light1 = 0;
            uint32_t count_light2 = 0;

            for (uint32_t i = 0; i < total_clusters; ++i)
            {
                const auto mask = bitmasks[i];
                if ((mask & (1U << 0)) != 0)
                {
                    ++count_light0;
                }
                if ((mask & (1U << 1)) != 0)
                {
                    ++count_light1;
                }
                if ((mask & (1U << 2)) != 0)
                {
                    ++count_light2;
                }
            }

            // Light 0 (near center) should intersect some near clusters
            EXPECT_GT(count_light0, 0U);
            // Light 1 (far corner) should intersect some far clusters
            EXPECT_GT(count_light1, 0U);
            // Light 2 (behind camera) must not intersect any clusters
            EXPECT_EQ(count_light2, 0U);
        }

        dev->destroy_buffer(readback_buf);
    }

    TEST(render_system_tests, clustered_lighting_and_culling_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        auto graph = render_graph::render_graph{1280, 720};

        auto cam = render_camera{
            .proj = math::perspective(16.0F / 9.0F, 1.0F, 0.1F, 100.0F),
            .inv_proj = math::inverse(math::perspective(16.0F / 9.0F, 1.0F, 0.1F, 100.0F)),
            .view = math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                  math::vec3<float>{0.0F, 1.0F, 0.0F}),
            .inv_view =
                math::inverse(math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                            math::vec3<float>{0.0F, 1.0F, 0.0F})),
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

        const auto& cluster_data = add_light_clustering_pass(graph, pool, shaders, cluster_bounds_buf, cam, 1280, 720);
        add_light_culling_pass(graph, pool, shaders, cluster_data.cluster_bounds_buffer, lights_buf,
                               cluster_data.create_info, 0);

        auto res = graph.execute(*dev);
        EXPECT_TRUE(res.has_value());

        dev->wait_idle();
    }

    TEST(render_system_tests, shader_manager_handle_registration_and_slot_lookup)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto shaders = shader_manager{*dev, fixture.asset_db};

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

        auto shaders = shader_manager{*dev, fixture.asset_db};

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

        auto shaders = shader_manager{*dev, fixture.asset_db};

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

        auto shaders = shader_manager{*dev, fixture.asset_db};

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
        auto update_result =
            shaders.update_shader_module_bytes(fs, span<const byte>{invalid_bytes.data(), invalid_bytes.size()});
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
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

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
            registry.assign(ent0, core::mesh_component{.mesh_id = mesh_id});
            registry.assign(ent0, core::material_component{.material_id = mat_opaque_1_id});
            registry.assign(ent0, ecs::transform_component::identity());

            // ent1: BLEND (transparent)
            auto ent1 = registry.create();
            registry.assign(ent1, core::mesh_component{.mesh_id = mesh_id});
            registry.assign(ent1, core::material_component{.material_id = mat_blend_id});
            registry.assign(ent1, ecs::transform_component::identity());

            // ent2: MASK (shadow contributor)
            auto ent2 = registry.create();
            registry.assign(ent2, core::mesh_component{.mesh_id = mesh_id});
            registry.assign(ent2, core::material_component{.material_id = mat_mask_id});
            registry.assign(ent2, ecs::transform_component::identity());

            // ent3: TRANSMISSIVE (transparent)
            auto ent3 = registry.create();
            registry.assign(ent3, core::mesh_component{.mesh_id = mesh_id});
            registry.assign(ent3, core::material_component{.material_id = mat_trans_id});
            registry.assign(ent3, ecs::transform_component::identity());

            // ent4: OPAQUE (shadow contributor)
            auto ent4 = registry.create();
            registry.assign(ent4, core::mesh_component{.mesh_id = mesh_id});
            registry.assign(ent4, core::material_component{.material_id = mat_opaque_2_id});
            registry.assign(ent4, ecs::transform_component::identity());

            rend->prepare_frame(1280, 720);

            // Assert draw counts:
            // 5 total active draws: 2 opaque (ent0, ent4), 1 alpha-masked (ent2), 2 transparent (ent1, ent3)
            EXPECT_EQ(rend->get_active_draw_count(), 5U);
            EXPECT_EQ(rend->get_opaque_draw_count(), 2U);
            EXPECT_EQ(rend->get_opaque_draw_offset(), 0U);
            EXPECT_EQ(rend->get_alpha_masked_draw_count(), 1U);
            EXPECT_EQ(rend->get_alpha_masked_draw_offset(), 2U);
            EXPECT_EQ(rend->get_transparent_draw_count(), 2U);
            EXPECT_EQ(rend->get_transparent_draw_offset(), 3U);

            // Validate the mapped GPU buffers for the active frame slot
            auto& pool = rend->get_resource_pool();
            const auto slot = pool.get_frame_slot();
            auto* cmds = static_cast<const indexed_indirect_command*>(pool.get_draw_commands_buffer().cpu_address) +
                         slot * pool.get_config().max_draw_command_count;
            auto* instances = static_cast<const uint32_t*>(pool.get_instance_buffer().cpu_address) +
                              slot * pool.get_config().max_instance_count;
            auto* objects = static_cast<const object_payload*>(pool.get_object_buffer().cpu_address) +
                            slot * pool.get_config().max_object_count;

            ASSERT_NE(cmds, nullptr);
            ASSERT_NE(instances, nullptr);
            ASSERT_NE(objects, nullptr);

            for (uint32_t i = 0; i < 5; ++i)
            {
                EXPECT_EQ(cmds[i].first_instance, i);
                EXPECT_EQ(cmds[i].instance_count, 1U);
                EXPECT_EQ(instances[i], i);
            }

            // Opaque partition first (ent0, ent4)
            EXPECT_EQ(objects[0].self_id, static_cast<uint32_t>(ent0));
            EXPECT_EQ(objects[1].self_id, static_cast<uint32_t>(ent4));

            // Alpha-masked partition second (ent2)
            EXPECT_EQ(objects[2].self_id, static_cast<uint32_t>(ent2));

            // Transparent partition third (ent1, ent3)
            EXPECT_EQ(objects[3].self_id, static_cast<uint32_t>(ent1));
            EXPECT_EQ(objects[4].self_id, static_cast<uint32_t>(ent3));
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, resource_pool_multi_frame_in_flight_buffer_slicing)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        {
            auto cfg = resource_pool_config{
                .max_object_count = 10,
                .max_instance_count = 10,
                .max_draw_command_count = 10,
                .frames_in_flight = 2,
            };
            auto pool = resource_pool{*dev, cfg};

            // Slot 0
            auto obj_slot0 = object_payload{.self_id = 42};
            pool.write_objects(span<const object_payload>{&obj_slot0, 1});

            const auto obj_addr0 = pool.get_object_buffer_address();
            const auto inst_addr0 = pool.get_instance_buffer_address();
            const auto draw_offset0 = pool.get_draw_commands_buffer_offset();

            EXPECT_EQ(draw_offset0, 0U);

            // Advance to Slot 1
            pool.advance_frame();
            EXPECT_EQ(pool.get_frame_slot(), 1U);

            auto obj_slot1 = object_payload{.self_id = 99};
            pool.write_objects(span<const object_payload>{&obj_slot1, 1});

            const auto obj_addr1 = pool.get_object_buffer_address();
            const auto inst_addr1 = pool.get_instance_buffer_address();
            const auto draw_offset1 = pool.get_draw_commands_buffer_offset();

            // Slot 1 must have distinct GPU addresses/offsets from Slot 0
            EXPECT_NE(obj_addr0, obj_addr1);
            EXPECT_NE(inst_addr0, inst_addr1);
            EXPECT_EQ(draw_offset1, sizeof(indexed_indirect_command) * 10);
            EXPECT_EQ(obj_addr1 - obj_addr0, sizeof(object_payload) * 10);
            EXPECT_EQ(inst_addr1 - inst_addr0, sizeof(uint32_t) * 10);

            // Readback from mapped CPU pointers: Slot 0's data must remain intact and isolated from Slot 1
            const auto* cpu_objs = static_cast<const object_payload*>(pool.get_object_buffer().cpu_address);
            ASSERT_NE(cpu_objs, nullptr);
            EXPECT_EQ(cpu_objs[0].self_id, 42U);
            EXPECT_EQ(cpu_objs[10].self_id, 99U);
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, resource_pool_texture_loading_and_mipmap_generation)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        {
            auto pool = resource_pool{*dev};
            auto graph = render_graph::render_graph{512, 512};
            auto registry = core::texture_registry{};

            constexpr uint32_t tex_w = 64;
            constexpr uint32_t tex_h = 64;
            auto mip_data = core::texture_mip_data{
                .data = vector<byte>(tex_w * tex_h * 4),
                .width = tex_w,
                .height = tex_h,
            };
            for (size_t i = 0; i < tex_w * tex_h * 4; i += 4)
            {
                mip_data.data[i + 0] = static_cast<byte>(180);
                mip_data.data[i + 1] = static_cast<byte>(90);
                mip_data.data[i + 2] = static_cast<byte>(45);
                mip_data.data[i + 3] = static_cast<byte>(255);
            }

            auto tex1 = core::texture{
                .width = tex_w,
                .height = tex_h,
                .format = core::texture_format::rgba8_srgb,
                .name = "TestMipmapTexture_IfMissing",
            };
            tex1.mips.push_back(mip_data);

            auto tex2 = core::texture{
                .width = tex_w,
                .height = tex_h,
                .format = core::texture_format::rgba8_srgb,
                .name = "TestMipmapTexture_None",
            };
            tex2.mips.push_back(mip_data);

            auto tex3 = core::texture{
                .width = tex_w,
                .height = tex_h,
                .format = core::texture_format::rgba8_srgb,
                .name = "TestMipmapTexture_Force",
            };
            tex3.mips.push_back(mip_data);

            const auto tex_id1 = registry.register_texture(tempest::move(tex1));
            const auto tex_id2 = registry.register_texture(tempest::move(tex2));
            const auto tex_id3 = registry.register_texture(tempest::move(tex3));

            // Load textures under different modes
            pool.load_textures(span<const guid>{&tex_id1, 1}, registry, graph, mipmap_generation_mode::if_missing);
            pool.load_textures(span<const guid>{&tex_id2, 1}, registry, graph, mipmap_generation_mode::none);
            pool.load_textures(span<const guid>{&tex_id3, 1}, registry, graph, mipmap_generation_mode::force);

            EXPECT_GE(pool.get_texture_descriptor_index(tex_id1), 0);
            EXPECT_GE(pool.get_texture_descriptor_index(tex_id2), 0);
            EXPECT_GE(pool.get_texture_descriptor_index(tex_id3), 0);

            // Execute the texture upload and mip generation pass
            const auto exec_res = graph.execute(*dev);
            EXPECT_TRUE(exec_res.has_value());

            dev->wait_idle();
            pool.clear_staging_buffers();
            graph.reset();
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, resource_pool_directional_shadow_buffer_and_bda)
    {
        EXPECT_EQ(sizeof(shadow_cascade_data), 96ULL);
        EXPECT_EQ(sizeof(directional_shadow_data), 400ULL);

        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        {
            auto pool = resource_pool{*dev};
            auto buf = pool.get_directional_shadow_buffer();
            EXPECT_NE(buf.handle, 0ULL);
            EXPECT_NE(buf.cpu_address, nullptr);
            EXPECT_NE(buf.gpu_address, 0ULL);

            const auto addr_f0 = pool.get_directional_shadow_address();
            EXPECT_EQ(addr_f0, buf.gpu_address);

            auto shadow_data_f0 = directional_shadow_data{};
            shadow_data_f0.cascade_count = 3;
            shadow_data_f0.normal_bias = 0.05F;
            shadow_data_f0.depth_bias = 0.01F;
            shadow_data_f0.cascades[0].split_depth = 10.0F;
            shadow_data_f0.cascades[1].split_depth = 50.0F;
            shadow_data_f0.cascades[2].split_depth = 150.0F;

            pool.write_directional_shadow_data(shadow_data_f0);

            // Verify written data in slot 0
            const auto* read_f0 = static_cast<const directional_shadow_data*>(buf.cpu_address);
            EXPECT_EQ(read_f0[0].cascade_count, 3U);
            EXPECT_FLOAT_EQ(read_f0[0].normal_bias, 0.05F);
            EXPECT_FLOAT_EQ(read_f0[0].depth_bias, 0.01F);
            EXPECT_FLOAT_EQ(read_f0[0].cascades[0].split_depth, 10.0F);

            // Advance frame slot to slot 1
            pool.advance_frame();
            const auto addr_f1 = pool.get_directional_shadow_address();
            EXPECT_EQ(addr_f1, buf.gpu_address + sizeof(directional_shadow_data));

            auto shadow_data_f1 = directional_shadow_data{};
            shadow_data_f1.cascade_count = 4;
            shadow_data_f1.normal_bias = 0.02F;
            shadow_data_f1.depth_bias = 0.005F;
            shadow_data_f1.cascades[0].split_depth = 20.0F;

            pool.write_directional_shadow_data(shadow_data_f1);

            const auto* read_f1 = static_cast<const directional_shadow_data*>(buf.cpu_address) + 1;
            EXPECT_EQ(read_f1->cascade_count, 4U);
            EXPECT_FLOAT_EQ(read_f1->normal_bias, 0.02F);
            EXPECT_FLOAT_EQ(read_f1->depth_bias, 0.005F);
            EXPECT_FLOAT_EQ(read_f1->cascades[0].split_depth, 20.0F);

            // Frame 0 data remains intact
            EXPECT_EQ(read_f0[0].cascade_count, 3U);
            EXPECT_FLOAT_EQ(read_f0[0].normal_bias, 0.05F);
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, csm_practical_splits_and_bounding_sphere_math)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto cam_sys = camera_system{registry, events};
        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        auto graph = render_graph::render_graph{1920, 1080};
        auto allocator = shelf_allocator{8192, 8192, 4};

        auto shadow_atlas_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(8192, 8192),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "ShadowAtlasTarget",
        });

        // 1. Setup Camera
        auto cam_ent = registry.create();
        registry.assign(cam_ent, camera_component{
                                     .aspect_ratio = 16.0F / 9.0F,
                                     .vertical_fov = 1.0F,
                                     .near_plane = 0.1F,
                                 });
        auto cam_tx = ecs::transform_component::identity();
        cam_tx.position({0.0F, 2.0F, -10.0F});
        registry.assign(cam_ent, cam_tx);

        // 2. Setup Sun Light with shadow_caster_component
        auto sun_ent = registry.create();
        registry.assign(sun_ent, directional_light_component{
                                     .color = {1.0F, 1.0F, 1.0F},
                                     .intensity = 5.0F,
                                 });
        registry.assign(sun_ent, shadow_caster_component{
                                     .resolution = 2048,
                                     .num_cascades = 4,
                                     .split_lambda = 0.6F,
                                     .max_shadow_distance = 150.0F,
                                     .normal_bias = 0.03F,
                                     .depth_bias = 0.008F,
                                     .priority = 0,
                                 });
        auto sun_tx = ecs::transform_component::identity();
        sun_tx.rotation({math::as_radians(45.0F), math::as_radians(30.0F), 0.0F});
        registry.assign(sun_ent, sun_tx);

        const auto shadow_res = add_shadow_pass(shadow_pass_params{
            .graph = graph,
            .pool = pool,
            .shaders = shaders,
            .shadow_atlas = shadow_atlas_tex,
            .allocator = allocator,
            .registry = registry,
            .camera_sys = &cam_sys,
            .opaque_draw_count = 0,
            .opaque_draw_offset = 0,
            .alpha_masked_draw_count = 0,
            .alpha_masked_draw_offset = 0,
        });
        const auto& shadow_data = shadow_res.shadow_data;

        EXPECT_EQ(shadow_data.cascade_count, 4U);
        EXPECT_FLOAT_EQ(shadow_data.normal_bias, 0.03F);
        EXPECT_FLOAT_EQ(shadow_data.depth_bias, 0.008F);

        // Splits must be monotonically increasing
        EXPECT_GT(shadow_data.cascades[0].split_depth, 0.1F);
        EXPECT_GT(shadow_data.cascades[1].split_depth, shadow_data.cascades[0].split_depth);
        EXPECT_GT(shadow_data.cascades[2].split_depth, shadow_data.cascades[1].split_depth);
        EXPECT_FLOAT_EQ(shadow_data.cascades[3].split_depth, 150.0F);

        // Validate UV offsets and scales in the atlas
        for (uint32_t i = 0; i < 4; ++i)
        {
            const auto& uv = shadow_data.cascades[i].uv_offset_scale;
            EXPECT_GE(uv.x, 0.0F);
            EXPECT_GE(uv.y, 0.0F);
            EXPECT_GT(uv.z, 0.0F);
            EXPECT_GT(uv.w, 0.0F);
            EXPECT_LE(uv.x + uv.z, 1.0F);
            EXPECT_LE(uv.y + uv.w, 1.0F);
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, shadow_pass_render_graph_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto cam_sys = camera_system{registry, events};
        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        auto graph = render_graph::render_graph{1280, 720};
        auto allocator = shelf_allocator{8192, 8192, 4};

        // 1. Setup Camera
        auto cam_ent = registry.create();
        registry.assign(cam_ent, camera_component{
                                     .aspect_ratio = 16.0F / 9.0F,
                                     .vertical_fov = 1.0F,
                                     .near_plane = 0.1F,
                                 });
        auto cam_tx = ecs::transform_component::identity();
        cam_tx.position({0.0F, 0.0F, -5.0F});
        registry.assign(cam_ent, cam_tx);

        // 2. Setup Sun Light
        auto sun_ent = registry.create();
        registry.assign(sun_ent, directional_light_component{
                                     .color = {1.0F, 1.0F, 1.0F},
                                     .intensity = 2.0F,
                                 });
        registry.assign(sun_ent, shadow_caster_component{
                                     .resolution = 2048,
                                     .num_cascades = 4,
                                     .split_lambda = 0.5F,
                                     .max_shadow_distance = 100.0F,
                                     .normal_bias = 0.02F,
                                     .depth_bias = 0.005F,
                                     .priority = 0,
                                 });
        auto sun_tx = ecs::transform_component::identity();
        sun_tx.rotation({math::as_radians(45.0F), 0.0F, 0.0F});
        registry.assign(sun_ent, sun_tx);

        // 3. Setup Mesh
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto mesh_id = meshes.register_mesh(create_test_mesh());
        auto mat = core::material{};
        mat.set_vec4(core::material::base_color_factor_name, {1.0F, 1.0F, 1.0F, 1.0F});
        auto mat_id = materials.register_material(tempest::move(mat));

        auto geom_ent = registry.create();
        registry.assign(geom_ent, core::mesh_component{.mesh_id = mesh_id});
        registry.assign(geom_ent, core::material_component{.material_id = mat_id});
        registry.assign(geom_ent, ecs::transform_component::identity());

        pool.load_materials(span<const guid>{&mat_id, 1}, materials, graph);
        pool.load_meshes(span<const guid>{&mesh_id, 1}, meshes, graph);

        const auto sync_res = graph.execute(*dev);
        EXPECT_TRUE(sync_res.has_value());
        dev->wait_idle();
        pool.clear_staging_buffers();
        graph.reset();

        auto shadow_atlas_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(8192, 8192),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "ShadowAtlasTarget",
        });

        const auto ml_opt = pool.get_mesh_layout(mesh_id);
        ASSERT_TRUE(ml_opt.has_value());
        const auto& ml = *ml_opt;
        auto objects = array{object_payload{
            .model = math::mat4<float>{1.0F},
            .inv_transpose_model = math::mat4<float>{1.0F},
            .mesh_gpu_address = pool.get_mesh_address(mesh_id),
            .material_gpu_address = pool.get_material_address(mat_id),
            .parent_gpu_address = 0,
            .self_id = static_cast<uint32_t>(geom_ent),
            .padding = 0,
        }};
        auto instances = array{0U};
        auto commands = array{indexed_indirect_command{
            .index_count = ml.index_count,
            .instance_count = 1,
            .first_index = (ml.mesh_start_offset + ml.index_offset) / static_cast<uint32_t>(sizeof(uint32_t)),
            .vertex_offset = 0,
            .first_instance = 0,
        }};

        pool.write_objects(span<const object_payload>{objects.data(), objects.size()});
        pool.write_instances(span<const uint32_t>{instances.data(), instances.size()});
        pool.write_draw_commands(span<const indexed_indirect_command>{commands.data(), commands.size()});

        add_frame_upload_pass(graph, pool);
        const auto shadow_res = add_shadow_pass(shadow_pass_params{
            .graph = graph,
            .pool = pool,
            .shaders = shaders,
            .shadow_atlas = shadow_atlas_tex,
            .allocator = allocator,
            .registry = registry,
            .camera_sys = &cam_sys,
            .opaque_draw_count = 1,
            .opaque_draw_offset = 0,
            .alpha_masked_draw_count = 0,
            .alpha_masked_draw_offset = 0,
        });

        EXPECT_EQ(shadow_res.shadow_data.cascade_count, 4U);
        EXPECT_TRUE(shadow_res.shadow_atlas.is_valid());

        auto exec_res = graph.execute(*dev);
        EXPECT_TRUE(exec_res.has_value());

        dev->wait_idle();
    }

    /// @brief Verifies that the directional shadow pass registers both the opaque and alpha-masked
    /// graphics pipelines, binds them in sequence with their respective draw offsets/counts, and
    /// successfully executes on the GPU.
    TEST(render_system_tests, shadow_pass_dual_pipeline_opaque_and_masked_execution)
    {
        // 1. Setup Test Device, Asset Database, and Contexts
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto cam_sys = camera_system{registry, events};

        auto cam_ent = registry.create();
        registry.assign(cam_ent, camera_component{
                                     .aspect_ratio = 16.0F / 9.0F,
                                     .vertical_fov = 1.0F,
                                     .near_plane = 0.1F,
                                     .is_active = true,
                                 });
        registry.assign(cam_ent, ecs::transform_component::identity());

        auto sun_ent = registry.create();
        registry.assign(sun_ent, directional_light_component{
                                     .color = {1.0F, 1.0F, 1.0F},
                                     .intensity = 5.0F,
                                 });
        registry.assign(sun_ent, shadow_caster_component{
                                     .resolution = 1024,
                                     .num_cascades = 2,
                                     .split_lambda = 0.5F,
                                     .max_shadow_distance = 50.0F,
                                     .normal_bias = 0.02F,
                                     .depth_bias = 0.005F,
                                     .priority = 0,
                                 });
        auto sun_tx = ecs::transform_component::identity();
        sun_tx.rotation({math::as_radians(45.0F), math::as_radians(30.0F), 0.0F});
        registry.assign(sun_ent, sun_tx);

        auto meshes = core::mesh_registry{};
        auto mesh_id = meshes.register_mesh(create_test_mesh());

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        auto allocator = shelf_allocator{4096, 4096, 4};
        auto graph = render_graph::render_graph{1280, 720};

        pool.load_meshes(span<const guid>{&mesh_id, 1}, meshes, graph);

        auto shadow_atlas_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(4096, 4096),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .name = "DirectionalShadowAtlas",
        });

        const auto mesh_layout_opt = pool.get_mesh_layout(mesh_id);
        ASSERT_TRUE(mesh_layout_opt.has_value());
        const auto& ml = *mesh_layout_opt;

        // Create 2 objects: Object 0 (Opaque), Object 1 (Alpha-Masked)
        auto objects = array<object_payload, 2>{
            object_payload{
                .model = math::mat4<float>{1.0F},
                .inv_transpose_model = math::mat4<float>{1.0F},
                .mesh_gpu_address = pool.get_mesh_address(mesh_id),
                .material_gpu_address = 0,
                .parent_gpu_address = 0,
                .self_id = 100,
                .padding = 0,
            },
            object_payload{
                .model = math::mat4<float>{1.0F},
                .inv_transpose_model = math::mat4<float>{1.0F},
                .mesh_gpu_address = pool.get_mesh_address(mesh_id),
                .material_gpu_address = 0,
                .parent_gpu_address = 0,
                .self_id = 101,
                .padding = 0,
            },
        };

        auto instances = array<uint32_t, 2>{0, 1};
        auto commands = array<indexed_indirect_command, 2>{
            indexed_indirect_command{
                .index_count = ml.index_count,
                .instance_count = 1,
                .first_index = (ml.mesh_start_offset + ml.index_offset) / static_cast<uint32_t>(sizeof(uint32_t)),
                .vertex_offset = 0,
                .first_instance = 0,
            },
            indexed_indirect_command{
                .index_count = ml.index_count,
                .instance_count = 1,
                .first_index = (ml.mesh_start_offset + ml.index_offset) / static_cast<uint32_t>(sizeof(uint32_t)),
                .vertex_offset = 0,
                .first_instance = 1,
            },
        };

        pool.write_objects(span<const object_payload>{objects.data(), objects.size()});
        pool.write_instances(span<const uint32_t>{instances.data(), instances.size()});
        pool.write_draw_commands(span<const indexed_indirect_command>{commands.data(), commands.size()});

        // 2. Act: Record Frame Upload and Dual-Pipeline Shadow Pass
        add_frame_upload_pass(graph, pool);
        const auto shadow_res = add_shadow_pass(shadow_pass_params{
            .graph = graph,
            .pool = pool,
            .shaders = shaders,
            .shadow_atlas = shadow_atlas_tex,
            .allocator = allocator,
            .registry = registry,
            .camera_sys = &cam_sys,
            .opaque_draw_count = 1,
            .opaque_draw_offset = 0,
            .alpha_masked_draw_count = 1,
            .alpha_masked_draw_offset = 1,
        });

        // 3. Assert: Verify Pipeline Registration and Render Graph Execution
        EXPECT_EQ(shadow_res.shadow_data.cascade_count, 2U);
        EXPECT_TRUE(shadow_res.shadow_atlas.is_valid());

        // Verify both specialized pipelines are registered in shader_manager
        const auto opaque_pipe_opt = shaders.find_graphics_pipeline("shadow_depth_opaque_pipeline");
        ASSERT_TRUE(opaque_pipe_opt.has_value());
        EXPECT_NE(shaders.get_rhi_pipeline(*opaque_pipe_opt).handle, 0ULL);

        const auto masked_pipe_opt = shaders.find_graphics_pipeline("shadow_depth_masked_pipeline");
        ASSERT_TRUE(masked_pipe_opt.has_value());
        EXPECT_NE(shaders.get_rhi_pipeline(*masked_pipe_opt).handle, 0ULL);

        // Execute render graph and verify clean completion
        auto exec_res = graph.execute(*dev);
        EXPECT_TRUE(exec_res.has_value());

        dev->wait_idle();
    }

    TEST(render_system_tests, renderer_shadow_atlas_target_and_integration)
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
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            auto cam_ent = registry.create();
            registry.assign(cam_ent, camera_component{
                                         .aspect_ratio = 16.0F / 9.0F,
                                         .vertical_fov = 1.0F,
                                         .near_plane = 0.1F,
                                     });
            registry.assign(cam_ent, ecs::transform_component::identity());

            auto sun_ent = registry.create();
            registry.assign(sun_ent, directional_light_component{
                                         .color = {1.0F, 1.0F, 1.0F},
                                         .intensity = 2.0F,
                                     });
            registry.assign(sun_ent, shadow_caster_component{
                                         .resolution = 2048,
                                         .num_cascades = 4,
                                     });
            registry.assign(sun_ent, ecs::transform_component::identity());

            rend->prepare_frame(1280, 720);

            EXPECT_TRUE(rend->get_directional_shadow_atlas_texture().is_valid());
            EXPECT_TRUE(rend->get_punctual_shadow_atlas_texture().is_valid());

            auto res = rend->render();
            EXPECT_TRUE(res.has_value());

            dev->wait_idle();
        }
    }

    TEST(render_system_tests, renderer_shadow_atlas_4k_cascades_allocation_and_render)
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
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            auto cam_ent = registry.create();
            registry.assign(cam_ent, camera_component{
                                         .aspect_ratio = 16.0F / 9.0F,
                                         .vertical_fov = 1.0F,
                                         .near_plane = 0.1F,
                                     });
            registry.assign(cam_ent, ecs::transform_component::identity());

            auto sun_ent = registry.create();
            registry.assign(sun_ent, directional_light_component{
                                         .color = {1.0F, 1.0F, 1.0F},
                                         .intensity = 2.0F,
                                     });
            registry.assign(sun_ent, shadow_caster_component{
                                         .resolution = 4096,
                                         .num_cascades = 4,
                                     });
            registry.assign(sun_ent, ecs::transform_component::identity());

            rend->prepare_frame(1280, 720);

            EXPECT_TRUE(rend->get_directional_shadow_atlas_texture().is_valid());
            EXPECT_TRUE(rend->get_punctual_shadow_atlas_texture().is_valid());

            auto res = rend->render();
            EXPECT_TRUE(res.has_value());

            const auto* const alloc =
                rend->get_render_graph().get_allocator().get_texture(rend->get_directional_shadow_atlas_texture().id);
            ASSERT_NE(alloc, nullptr);
            EXPECT_EQ(alloc->size.width, 16384U);
            EXPECT_EQ(alloc->size.height, 16384U);

            const auto slot = rend->get_resource_pool().get_frame_slot();
            const auto* shadow_data = static_cast<const directional_shadow_data*>(
                                          rend->get_resource_pool().get_directional_shadow_buffer().cpu_address) +
                                      slot;
            ASSERT_NE(shadow_data, nullptr);
            EXPECT_EQ(shadow_data->cascade_count, 4U);

            for (auto i = 0U; i < 4U; ++i)
            {
                const auto& uv = shadow_data->cascades[i].uv_offset_scale;
                EXPECT_GE(uv.x, 0.0F);
                EXPECT_GE(uv.y, 0.0F);
                EXPECT_GT(uv.z, 0.0F);
                EXPECT_GT(uv.w, 0.0F);
                EXPECT_LE(uv.x + uv.z, 1.0F);
                EXPECT_LE(uv.y + uv.w, 1.0F);
            }

            dev->wait_idle();
        }
    }

    TEST(render_system_tests, pbr_opaque_push_constants_layout)
    {
        EXPECT_EQ(sizeof(pbr_opaque_push_constants), 48U);
        EXPECT_EQ(offsetof(pbr_opaque_push_constants, scene_constants_address), 0U);
        EXPECT_EQ(offsetof(pbr_opaque_push_constants, objects_address), 8U);
        EXPECT_EQ(offsetof(pbr_opaque_push_constants, instance_indices_address), 16U);
        EXPECT_EQ(offsetof(pbr_opaque_push_constants, directional_shadow_address), 24U);
        EXPECT_EQ(offsetof(pbr_opaque_push_constants, light_bitmask_address), 32U);
        EXPECT_EQ(offsetof(pbr_opaque_push_constants, linear_sampler_index), 40U);
        EXPECT_EQ(offsetof(pbr_opaque_push_constants, shadow_atlas_index), 44U);
    }

    TEST(render_system_tests, scene_constants_layout)
    {
        EXPECT_EQ(sizeof(scene_constants), 400U);
        EXPECT_EQ(offsetof(scene_constants, projection), 0U);
        EXPECT_EQ(offsetof(scene_constants, camera_position), 256U);
        EXPECT_EQ(offsetof(scene_constants, ambient_light), 272U);
        EXPECT_EQ(offsetof(scene_constants, sun_color_intensity), 288U);
        EXPECT_EQ(offsetof(scene_constants, sun_direction), 304U);
        EXPECT_EQ(offsetof(scene_constants, screen_size), 320U);
        EXPECT_EQ(offsetof(scene_constants, inv_screen_size), 328U);
        EXPECT_EQ(offsetof(scene_constants, lights_address), 336U);
        EXPECT_EQ(offsetof(scene_constants, light_bitmask_address), 344U);
        EXPECT_EQ(offsetof(scene_constants, light_count), 352U);
        EXPECT_EQ(offsetof(scene_constants, words_per_cluster), 356U);
        EXPECT_EQ(offsetof(scene_constants, padding), 360U);
        EXPECT_EQ(offsetof(scene_constants, cluster_counts_tile_size), 368U);
        EXPECT_EQ(offsetof(scene_constants, cluster_depth_params), 384U);
    }

    TEST(render_system_tests, directional_shadow_data_debug_mode_layout)
    {
        EXPECT_EQ(sizeof(directional_shadow_data), 400ULL);
        EXPECT_EQ(offsetof(directional_shadow_data, debug_mode), 396ULL);
    }

    TEST(render_system_tests, shadow_debug_mode_visualization_cascades_and_shadow_factor)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
            .shadow_debug = shadow_debug_mode::cascades,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);
            EXPECT_EQ(rend->get_shadow_debug_mode(), shadow_debug_mode::cascades);

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

            // 2. Setup Sun Light Entity
            auto sun_ent = registry.create();
            registry.assign(sun_ent, directional_light_component{
                                         .color = {1.0F, 1.0F, 1.0F},
                                         .intensity = 2.0F,
                                     });
            registry.assign(sun_ent, shadow_caster_component{
                                         .resolution = 2048,
                                         .num_cascades = 4,
                                         .split_lambda = 0.5F,
                                         .max_shadow_distance = 100.0F,
                                         .normal_bias = 0.02F,
                                         .depth_bias = 0.005F,
                                         .priority = 0,
                                         .debug_mode = shadow_debug_mode::cascades,
                                     });
            auto sun_tx = ecs::transform_component::identity();
            sun_tx.rotation({math::as_radians(45.0F), 0.0F, 0.0F});
            registry.assign(sun_ent, sun_tx);

            // 3. Setup Renderable Geometry Entity (Green surface material)
            auto mesh_id = meshes.register_mesh(create_test_mesh());
            auto mat = core::material{};
            mat.set_vec4(core::material::base_color_factor_name, {0.0F, 1.0F, 0.0F, 1.0F});
            mat.set_scalar(core::material::metallic_factor_name, 0.0F);
            mat.set_scalar(core::material::roughness_factor_name, 0.5F);
            auto mat_id = materials.register_material(tempest::move(mat));

            auto geom_ent = registry.create();
            registry.assign(geom_ent, core::mesh_component{.mesh_id = mesh_id});
            registry.assign(geom_ent, core::material_component{.material_id = mat_id});
            registry.assign(geom_ent, ecs::transform_component::identity());

            // 4. Prepare Frame with Cascades debug mode
            rend->prepare_frame(1280, 720);

            auto render_res = rend->render();
            EXPECT_TRUE(render_res.has_value());
            dev->wait_idle();

            // Readback and verify Cascade 0 color (Red dominant: float3(1.0, 0.25, 0.25))
            auto readback_buf = dev->create_buffer(rhi::buffer_desc{
                .size = 1280 * 720 * 4,
                .memory_usage = rhi::memory_usage::readback,
                .usage = rhi::buffer_usage::transfer_dst,
                .name = "CascadeDebugReadbackBuffer",
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
                [[maybe_unused]] auto submit_res =
                    port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
                dev->wait_idle();

                const auto* pixels = static_cast<const uint8_t*>(readback_buf.cpu_address);
                ASSERT_NE(pixels, nullptr);
                if (pixels)
                {
                    // Center pixel (640, 360) is in Cascade 0 -> Red channel should be significantly higher than Green
                    const auto center_idx = (360 * 1280 + 640) * 4;
                    const auto r = pixels[center_idx + 0];
                    const auto g = pixels[center_idx + 1];
                    const auto b = pixels[center_idx + 2];
                    EXPECT_GT(r, 180);
                    EXPECT_GT(r, g);
                    EXPECT_GT(r, b);
                }
            }

            // Test dynamic switch to Shadow Factor debug mode
            rend->set_shadow_debug_mode(shadow_debug_mode::shadow_factor);
            EXPECT_EQ(rend->get_shadow_debug_mode(), shadow_debug_mode::shadow_factor);

            rend->prepare_frame(1280, 720);
            auto render_res2 = rend->render();
            EXPECT_TRUE(render_res2.has_value());
            dev->wait_idle();

            const auto* alloc2 = rend->get_render_graph().get_physical_texture(rend->get_tonemapped_color_texture().id);
            ASSERT_NE(alloc2, nullptr);
            if (alloc2)
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
                cmd.copy_texture_to_buffer(alloc2->handle, readback_buf,
                                           span<const rhi::buffer_texture_copy_region>{&region, 1});
                cmd.end();

                auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
                [[maybe_unused]] auto submit_res =
                    port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
                dev->wait_idle();

                const auto* pixels = static_cast<const uint8_t*>(readback_buf.cpu_address);
                ASSERT_NE(pixels, nullptr);
                if (pixels)
                {
                    // Center pixel in shadow_factor mode must be grayscale (R == G == B within tonemapping
                    // quantization)
                    const auto center_idx = (360 * 1280 + 640) * 4;
                    const auto r = pixels[center_idx + 0];
                    const auto g = pixels[center_idx + 1];
                    const auto b = pixels[center_idx + 2];
                    EXPECT_NEAR(static_cast<int>(r), static_cast<int>(g), 2);
                    EXPECT_NEAR(static_cast<int>(g), static_cast<int>(b), 2);
                }
            }

            dev->destroy_buffer(readback_buf);
        }
    }

    TEST(render_system_tests, transparency_clear_pass_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto shaders = shader_manager{*dev, fixture.asset_db};
        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;
        auto graph = render_graph::render_graph{width, height};

        auto moments_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 2,
            .name = "MomentsArrayTarget",
        });

        auto zeroth_moment_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::r32_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "ZerothMomentTarget",
        });

        const auto& pass_data =
            add_transparency_clear_pass(graph, shaders, moments_tex, zeroth_moment_tex, width, height);
        EXPECT_TRUE(pass_data.moments_texture.is_valid());
        EXPECT_TRUE(pass_data.zeroth_moment_texture.is_valid());

        auto exec_res = graph.execute(*dev);
        EXPECT_TRUE(exec_res.has_value());
        dev->wait_idle();

        // Readback zeroth moment texture (64 * 64 * sizeof(float))
        auto zeroth_readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = width * height * sizeof(float),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "ZerothMomentReadbackBuffer",
        });

        // Readback moments array texture (2 layers * 64 * 64 * 4 * sizeof(uint16_t))
        auto moments_readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = 2 * width * height * 4 * sizeof(uint16_t),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "MomentsReadbackBuffer",
        });

        const auto* zeroth_alloc = graph.get_physical_texture(pass_data.zeroth_moment_texture.id);
        const auto* moments_alloc = graph.get_physical_texture(pass_data.moments_texture.id);
        ASSERT_NE(zeroth_alloc, nullptr);
        ASSERT_NE(moments_alloc, nullptr);

        auto& port = dev->get_graphics_execution_port();
        auto& cmd = port.acquire_command_list();
        cmd.begin();

        const auto zeroth_region = rhi::buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = width,
            .image_extent_height = height,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(zeroth_alloc->handle, zeroth_readback_buf,
                                   span<const rhi::buffer_texture_copy_region>{&zeroth_region, 1});

        const auto moments_regions = array{
            rhi::buffer_texture_copy_region{
                .buffer_offset = 0,
                .buffer_row_length = 0,
                .buffer_image_height = 0,
                .mip_level = 0,
                .base_array_layer = 0,
                .array_layer_count = 1,
                .image_offset_x = 0,
                .image_offset_y = 0,
                .image_offset_z = 0,
                .image_extent_width = width,
                .image_extent_height = height,
                .image_extent_depth = 1,
            },
            rhi::buffer_texture_copy_region{
                .buffer_offset = width * height * 4 * sizeof(uint16_t),
                .buffer_row_length = 0,
                .buffer_image_height = 0,
                .mip_level = 0,
                .base_array_layer = 1,
                .array_layer_count = 1,
                .image_offset_x = 0,
                .image_offset_y = 0,
                .image_offset_z = 0,
                .image_extent_width = width,
                .image_extent_height = height,
                .image_extent_depth = 1,
            },
        };
        cmd.copy_texture_to_buffer(
            moments_alloc->handle, moments_readback_buf,
            span<const rhi::buffer_texture_copy_region>{moments_regions.data(), moments_regions.size()});

        cmd.end();

        auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
        [[maybe_unused]] auto submit_res =
            port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
        dev->wait_idle();

        const auto* zeroth_pixels = static_cast<const float*>(zeroth_readback_buf.cpu_address);
        ASSERT_NE(zeroth_pixels, nullptr);
        for (size_t i = 0; i < width * height; ++i)
        {
            EXPECT_FLOAT_EQ(zeroth_pixels[i], 0.0F);
        }

        const auto* moments_pixels = static_cast<const uint16_t*>(moments_readback_buf.cpu_address);
        ASSERT_NE(moments_pixels, nullptr);
        for (size_t i = 0; i < 2 * width * height * 4; ++i)
        {
            EXPECT_EQ(moments_pixels[i], 0);
        }

        dev->destroy_buffer(zeroth_readback_buf);
        dev->destroy_buffer(moments_readback_buf);
    }

    TEST(render_system_tests, transparency_gather_push_constants_layout)
    {
        EXPECT_EQ(sizeof(transparency_gather_push_constants), 56U);
        EXPECT_EQ(offsetof(transparency_gather_push_constants, scene_constants_address), 0U);
        EXPECT_EQ(offsetof(transparency_gather_push_constants, objects_address), 8U);
        EXPECT_EQ(offsetof(transparency_gather_push_constants, instance_indices_address), 16U);
        EXPECT_EQ(offsetof(transparency_gather_push_constants, directional_shadow_address), 24U);
        EXPECT_EQ(offsetof(transparency_gather_push_constants, moments_storage_index), 32U);
        EXPECT_EQ(offsetof(transparency_gather_push_constants, zeroth_moment_storage_index), 36U);
        EXPECT_EQ(offsetof(transparency_gather_push_constants, linear_sampler_index), 40U);
        EXPECT_EQ(offsetof(transparency_gather_push_constants, point_sampler_index), 44U);
        EXPECT_EQ(offsetof(transparency_gather_push_constants, ssao_texture_index), 48U);
        EXPECT_EQ(offsetof(transparency_gather_push_constants, shadow_atlas_index), 52U);
    }

    TEST(render_system_tests, transparency_gather_pass_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;
        auto graph = render_graph::render_graph{width, height};

        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto mesh_id = meshes.register_mesh(create_test_mesh());

        // Transparent material (BLEND)
        auto mat_blend = core::material{};
        mat_blend.set_string(core::material::alpha_mode_name, "BLEND");
        mat_blend.set_vec4(core::material::base_color_factor_name, {0.8F, 0.2F, 0.2F, 0.5F});
        auto mat_blend_id = materials.register_material(tempest::move(mat_blend));

        pool.load_meshes(span<const guid>{&mesh_id, 1}, meshes, graph);
        pool.load_materials(span<const guid>{&mat_blend_id, 1}, materials, graph);

        auto scene = scene_constants{
            .projection = math::perspective(1.0F, 1.0F, 0.1F, 100.0F),
            .inv_projection = math::inverse(math::perspective(1.0F, 1.0F, 0.1F, 100.0F)),
            .view = math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                  math::vec3<float>{0.0F, 1.0F, 0.0F}),
            .inv_view =
                math::inverse(math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                            math::vec3<float>{0.0F, 1.0F, 0.0F})),
            .camera_position = {0.0F, 0.0F, -5.0F, 1.0F},
            .ambient_light = {0.28F, 0.30F, 0.36F, 1.0F},
            .sun_color_intensity = {1.0F, 1.0F, 1.0F, 2.0F},
            .sun_direction = {0.0F, -1.0F, 0.0F, 0.0F},
            .screen_size = {static_cast<float>(width), static_cast<float>(height)},
            .inv_screen_size = {1.0F / static_cast<float>(width), 1.0F / static_cast<float>(height)},
        };
        pool.write_scene_constants(scene);

        const auto mesh_gpu_addr = pool.get_mesh_address(mesh_id);
        const auto mat_gpu_addr = pool.get_material_address(mat_blend_id);
        auto mesh_layout_opt = pool.get_mesh_layout(mesh_id);
        ASSERT_TRUE(mesh_layout_opt.has_value());

        auto payload = object_payload{
            .model = math::mat4<float>{1.0F},
            .inv_transpose_model = math::mat4<float>{1.0F},
            .mesh_gpu_address = mesh_gpu_addr,
            .material_gpu_address = mat_gpu_addr,
            .parent_gpu_address = 0,
            .self_id = 1,
            .padding = 0,
        };
        auto objects = vector<object_payload>{};
        objects.push_back(payload);

        auto instances = vector<uint32_t>{};
        instances.push_back(0);

        auto commands = vector<indexed_indirect_command>{};
        commands.push_back(indexed_indirect_command{
            .index_count = mesh_layout_opt->index_count,
            .instance_count = 1,
            .first_index = (mesh_layout_opt->mesh_start_offset + mesh_layout_opt->index_offset) /
                           static_cast<uint32_t>(sizeof(uint32_t)),
            .vertex_offset = 0,
            .first_instance = 0,
        });

        pool.write_objects(objects);
        pool.write_instances(instances);
        pool.write_draw_commands(commands);

        auto moments_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 2,
            .name = "MomentsArrayTarget",
        });

        auto zeroth_moment_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::r32_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "ZerothMomentTarget",
        });

        auto depth_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "DepthTarget",
        });

        add_frame_upload_pass(graph, pool);
        const auto& depth_data = add_depth_prepass(graph, pool, shaders, depth_tex, 0);
        const auto& clear_data =
            add_transparency_clear_pass(graph, shaders, moments_tex, zeroth_moment_tex, width, height);
        const auto& gather_data =
            add_transparency_gather_pass(graph, pool, shaders, clear_data.moments_texture,
                                         clear_data.zeroth_moment_texture, depth_data.depth_texture, 1);

        EXPECT_TRUE(gather_data.moments_texture.is_valid());
        EXPECT_TRUE(gather_data.zeroth_moment_texture.is_valid());
        EXPECT_TRUE(gather_data.depth_texture.is_valid());

        struct gather_sink_data
        {
            render_graph::rg_texture_id moments;
            render_graph::rg_texture_id zeroth;
        };

        graph.add_graphics_pass<gather_sink_data>(
            "GatherSinkPass",
            [m = gather_data.moments_texture,
             z = gather_data.zeroth_moment_texture](render_graph::pass_builder& builder, gather_sink_data& data) {
                data.moments = builder.read(m, rhi::pipeline_stage::fragment, rhi::resource_access::read,
                                            rhi::image_layout::general);
                data.zeroth = builder.read(z, rhi::pipeline_stage::fragment, rhi::resource_access::read,
                                           rhi::image_layout::general);
                builder.mark_sink();
            },
            []([[maybe_unused]] const gather_sink_data&, [[maybe_unused]] render_graph::pass_execution_context&,
               [[maybe_unused]] rhi::command_list&) {});

        auto exec_res = graph.execute(*dev);
        EXPECT_TRUE(exec_res.has_value());
        dev->wait_idle();

        // Readback zeroth moment texture
        auto zeroth_readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = width * height * sizeof(float),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "ZerothMomentGatherReadbackBuffer",
        });

        // Readback moments array texture (2 layers * 64 * 64 * 4 * sizeof(uint16_t))
        auto moments_readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = 2 * width * height * 4 * sizeof(uint16_t),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "MomentsGatherReadbackBuffer",
        });

        const auto* zeroth_alloc = graph.get_physical_texture(gather_data.zeroth_moment_texture.id);
        const auto* moments_alloc = graph.get_physical_texture(gather_data.moments_texture.id);
        ASSERT_NE(zeroth_alloc, nullptr);
        ASSERT_NE(moments_alloc, nullptr);

        auto& port = dev->get_graphics_execution_port();
        auto& cmd = port.acquire_command_list();
        cmd.begin();

        const auto zeroth_region = rhi::buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = width,
            .image_extent_height = height,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(zeroth_alloc->handle, zeroth_readback_buf,
                                   span<const rhi::buffer_texture_copy_region>{&zeroth_region, 1});

        const auto moments_regions = array{
            rhi::buffer_texture_copy_region{
                .buffer_offset = 0,
                .buffer_row_length = 0,
                .buffer_image_height = 0,
                .mip_level = 0,
                .base_array_layer = 0,
                .array_layer_count = 1,
                .image_offset_x = 0,
                .image_offset_y = 0,
                .image_offset_z = 0,
                .image_extent_width = width,
                .image_extent_height = height,
                .image_extent_depth = 1,
            },
            rhi::buffer_texture_copy_region{
                .buffer_offset = width * height * 4 * sizeof(uint16_t),
                .buffer_row_length = 0,
                .buffer_image_height = 0,
                .mip_level = 0,
                .base_array_layer = 1,
                .array_layer_count = 1,
                .image_offset_x = 0,
                .image_offset_y = 0,
                .image_offset_z = 0,
                .image_extent_width = width,
                .image_extent_height = height,
                .image_extent_depth = 1,
            },
        };
        cmd.copy_texture_to_buffer(
            moments_alloc->handle, moments_readback_buf,
            span<const rhi::buffer_texture_copy_region>{moments_regions.data(), moments_regions.size()});

        cmd.end();

        auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
        [[maybe_unused]] auto submit_res =
            port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
        dev->wait_idle();

        const auto* zeroth_pixels = static_cast<const float*>(zeroth_readback_buf.cpu_address);
        ASSERT_NE(zeroth_pixels, nullptr);

        // Center pixel (32, 32) is covered by transparent quad
        const auto center_idx = 32 * width + 32;
        EXPECT_GT(zeroth_pixels[center_idx], 0.0F);

        // Corner pixel (0, 0) is outside the quad
        const auto corner_idx = 0;
        EXPECT_FLOAT_EQ(zeroth_pixels[corner_idx], 0.0F);

        const auto* moments_pixels = static_cast<const uint16_t*>(moments_readback_buf.cpu_address);
        ASSERT_NE(moments_pixels, nullptr);

        // Verify moments are non-zero at center pixel for both layers
        const auto even_center_offset = center_idx * 4;
        const auto odd_center_offset = (width * height + center_idx) * 4;
        EXPECT_NE(moments_pixels[even_center_offset + 0], 0);
        EXPECT_NE(moments_pixels[odd_center_offset + 0], 0);

        dev->destroy_buffer(zeroth_readback_buf);
        dev->destroy_buffer(moments_readback_buf);
    }

    TEST(render_system_tests, transparency_gather_pass_transmissive_material_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;
        auto graph = render_graph::render_graph{width, height};

        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};

        auto mesh_id = meshes.register_mesh(create_test_mesh());

        // Transmissive material
        auto mat_trans = core::material{};
        mat_trans.set_string(core::material::alpha_mode_name, "TRANSMISSIVE");
        mat_trans.set_scalar(core::material::transmissive_factor_name, 0.8F);
        mat_trans.set_vec4(core::material::base_color_factor_name, {1.0F, 1.0F, 1.0F, 1.0F});
        auto mat_trans_id = materials.register_material(tempest::move(mat_trans));

        pool.load_meshes(span<const guid>{&mesh_id, 1}, meshes, graph);
        pool.load_materials(span<const guid>{&mat_trans_id, 1}, materials, graph);

        auto scene = scene_constants{
            .projection = math::perspective(1.0F, 1.0F, 0.1F, 100.0F),
            .inv_projection = math::inverse(math::perspective(1.0F, 1.0F, 0.1F, 100.0F)),
            .view = math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                  math::vec3<float>{0.0F, 1.0F, 0.0F}),
            .inv_view =
                math::inverse(math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                            math::vec3<float>{0.0F, 1.0F, 0.0F})),
            .camera_position = {0.0F, 0.0F, -5.0F, 1.0F},
            .ambient_light = {0.28F, 0.30F, 0.36F, 1.0F},
            .sun_color_intensity = {1.0F, 1.0F, 1.0F, 2.0F},
            .sun_direction = {0.0F, -1.0F, 0.0F, 0.0F},
            .screen_size = {static_cast<float>(width), static_cast<float>(height)},
            .inv_screen_size = {1.0F / static_cast<float>(width), 1.0F / static_cast<float>(height)},
        };
        pool.write_scene_constants(scene);

        const auto mesh_gpu_addr = pool.get_mesh_address(mesh_id);
        const auto mat_gpu_addr = pool.get_material_address(mat_trans_id);
        auto mesh_layout_opt = pool.get_mesh_layout(mesh_id);
        ASSERT_TRUE(mesh_layout_opt.has_value());

        auto payload = object_payload{
            .model = math::mat4<float>{1.0F},
            .inv_transpose_model = math::mat4<float>{1.0F},
            .mesh_gpu_address = mesh_gpu_addr,
            .material_gpu_address = mat_gpu_addr,
            .parent_gpu_address = 0,
            .self_id = 1,
            .padding = 0,
        };
        auto objects = vector<object_payload>{};
        objects.push_back(payload);

        auto instances = vector<uint32_t>{};
        instances.push_back(0);

        auto commands = vector<indexed_indirect_command>{};
        commands.push_back(indexed_indirect_command{
            .index_count = mesh_layout_opt->index_count,
            .instance_count = 1,
            .first_index = (mesh_layout_opt->mesh_start_offset + mesh_layout_opt->index_offset) /
                           static_cast<uint32_t>(sizeof(uint32_t)),
            .vertex_offset = 0,
            .first_instance = 0,
        });

        pool.write_objects(objects);
        pool.write_instances(instances);
        pool.write_draw_commands(commands);

        auto moments_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 2,
            .name = "MomentsArrayTarget",
        });

        auto zeroth_moment_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::r32_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "ZerothMomentTarget",
        });

        auto depth_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "DepthTarget",
        });

        add_frame_upload_pass(graph, pool);
        const auto& depth_data = add_depth_prepass(graph, pool, shaders, depth_tex, 0);
        const auto& clear_data =
            add_transparency_clear_pass(graph, shaders, moments_tex, zeroth_moment_tex, width, height);
        const auto& gather_data =
            add_transparency_gather_pass(graph, pool, shaders, clear_data.moments_texture,
                                         clear_data.zeroth_moment_texture, depth_data.depth_texture, 1);

        struct gather_sink_data
        {
            render_graph::rg_texture_id moments;
            render_graph::rg_texture_id zeroth;
        };

        graph.add_graphics_pass<gather_sink_data>(
            "GatherSinkPass",
            [m = gather_data.moments_texture,
             z = gather_data.zeroth_moment_texture](render_graph::pass_builder& builder, gather_sink_data& data) {
                data.moments = builder.read(m, rhi::pipeline_stage::fragment, rhi::resource_access::read,
                                            rhi::image_layout::general);
                data.zeroth = builder.read(z, rhi::pipeline_stage::fragment, rhi::resource_access::read,
                                           rhi::image_layout::general);
                builder.mark_sink();
            },
            []([[maybe_unused]] const gather_sink_data&, [[maybe_unused]] render_graph::pass_execution_context&,
               [[maybe_unused]] rhi::command_list&) {});

        auto exec_res = graph.execute(*dev);
        EXPECT_TRUE(exec_res.has_value());
        dev->wait_idle();

        // Readback zeroth moment texture
        auto zeroth_readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = width * height * sizeof(float),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "ZerothMomentTransmissiveReadbackBuffer",
        });

        const auto* zeroth_alloc = graph.get_physical_texture(gather_data.zeroth_moment_texture.id);
        ASSERT_NE(zeroth_alloc, nullptr);

        auto& port = dev->get_graphics_execution_port();
        auto& cmd = port.acquire_command_list();
        cmd.begin();

        const auto zeroth_region = rhi::buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = width,
            .image_extent_height = height,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(zeroth_alloc->handle, zeroth_readback_buf,
                                   span<const rhi::buffer_texture_copy_region>{&zeroth_region, 1});
        cmd.end();

        auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
        [[maybe_unused]] auto submit_res =
            port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
        dev->wait_idle();

        const auto* zeroth_pixels = static_cast<const float*>(zeroth_readback_buf.cpu_address);
        ASSERT_NE(zeroth_pixels, nullptr);

        const auto center_idx = 32 * width + 32;
        EXPECT_GT(zeroth_pixels[center_idx], 0.0F);

        dev->destroy_buffer(zeroth_readback_buf);
    }

    TEST(render_system_tests, transparency_resolve_push_constants_layout)
    {
        EXPECT_EQ(sizeof(transparency_resolve_push_constants), 64U);
        EXPECT_EQ(offsetof(transparency_resolve_push_constants, scene_constants_address), 0U);
        EXPECT_EQ(offsetof(transparency_resolve_push_constants, objects_address), 8U);
        EXPECT_EQ(offsetof(transparency_resolve_push_constants, instance_indices_address), 16U);
        EXPECT_EQ(offsetof(transparency_resolve_push_constants, directional_shadow_address), 24U);
        EXPECT_EQ(offsetof(transparency_resolve_push_constants, light_bitmask_address), 32U);
        EXPECT_EQ(offsetof(transparency_resolve_push_constants, moments_storage_index), 40U);
        EXPECT_EQ(offsetof(transparency_resolve_push_constants, zeroth_moment_storage_index), 44U);
        EXPECT_EQ(offsetof(transparency_resolve_push_constants, linear_sampler_index), 48U);
        EXPECT_EQ(offsetof(transparency_resolve_push_constants, point_sampler_index), 52U);
        EXPECT_EQ(offsetof(transparency_resolve_push_constants, ssao_texture_index), 56U);
        EXPECT_EQ(offsetof(transparency_resolve_push_constants, shadow_atlas_index), 60U);
    }

    TEST(render_system_tests, transparency_resolve_pass_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;
        auto graph = render_graph::render_graph{width, height};

        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};

        auto mesh_id = meshes.register_mesh(create_test_mesh());

        // Transparent Blend Material (reddish)
        auto mat_blend = core::material{};
        mat_blend.set_string(core::material::alpha_mode_name, "BLEND");
        mat_blend.set_vec4(core::material::base_color_factor_name, {0.8F, 0.2F, 0.2F, 0.5F});
        mat_blend.set_scalar(core::material::metallic_factor_name, 0.0F);
        mat_blend.set_scalar(core::material::roughness_factor_name, 0.5F);
        auto mat_blend_id = materials.register_material(tempest::move(mat_blend));

        pool.load_meshes(span<const guid>{&mesh_id, 1}, meshes, graph);
        pool.load_materials(span<const guid>{&mat_blend_id, 1}, materials, graph);

        auto scene = scene_constants{
            .projection = math::perspective(1.0F, 1.0F, 0.1F, 100.0F),
            .inv_projection = math::inverse(math::perspective(1.0F, 1.0F, 0.1F, 100.0F)),
            .view = math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                  math::vec3<float>{0.0F, 1.0F, 0.0F}),
            .inv_view =
                math::inverse(math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                            math::vec3<float>{0.0F, 1.0F, 0.0F})),
            .camera_position = {0.0F, 0.0F, -5.0F, 1.0F},
            .ambient_light = {0.28F, 0.30F, 0.36F, 1.0F},
            .sun_color_intensity = {1.0F, 1.0F, 1.0F, 2.0F},
            .sun_direction = {0.0F, -1.0F, 0.0F, 0.0F},
            .screen_size = {static_cast<float>(width), static_cast<float>(height)},
            .inv_screen_size = {1.0F / static_cast<float>(width), 1.0F / static_cast<float>(height)},
        };
        pool.write_scene_constants(scene);

        const auto mesh_gpu_addr = pool.get_mesh_address(mesh_id);
        const auto mat_gpu_addr = pool.get_material_address(mat_blend_id);
        auto mesh_layout_opt = pool.get_mesh_layout(mesh_id);
        ASSERT_TRUE(mesh_layout_opt.has_value());

        auto payload = object_payload{
            .model = math::mat4<float>{1.0F},
            .inv_transpose_model = math::mat4<float>{1.0F},
            .mesh_gpu_address = mesh_gpu_addr,
            .material_gpu_address = mat_gpu_addr,
            .parent_gpu_address = 0,
            .self_id = 1,
            .padding = 0,
        };
        auto objects = vector<object_payload>{};
        objects.push_back(payload);

        auto instances = vector<uint32_t>{};
        instances.push_back(0);

        auto commands = vector<indexed_indirect_command>{};
        commands.push_back(indexed_indirect_command{
            .index_count = mesh_layout_opt->index_count,
            .instance_count = 1,
            .first_index = (mesh_layout_opt->mesh_start_offset + mesh_layout_opt->index_offset) /
                           static_cast<uint32_t>(sizeof(uint32_t)),
            .vertex_offset = 0,
            .first_instance = 0,
        });

        pool.write_objects(objects);
        pool.write_instances(instances);
        pool.write_draw_commands(commands);

        auto accum_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage =
                rhi::texture_usage::color_attachment | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "TransparencyAccumTarget",
        });

        auto moments_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 2,
            .name = "MomentsArrayTarget",
        });

        auto zeroth_moment_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::r32_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "ZerothMomentTarget",
        });

        auto depth_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "DepthTarget",
        });

        add_frame_upload_pass(graph, pool);
        const auto& depth_data = add_depth_prepass(graph, pool, shaders, depth_tex, 0);
        const auto& clear_data =
            add_transparency_clear_pass(graph, shaders, moments_tex, zeroth_moment_tex, width, height);
        const auto& gather_data =
            add_transparency_gather_pass(graph, pool, shaders, clear_data.moments_texture,
                                         clear_data.zeroth_moment_texture, depth_data.depth_texture, 1);
        const auto& resolve_data =
            add_transparency_resolve_pass(graph, pool, shaders, accum_tex, gather_data.moments_texture,
                                          gather_data.zeroth_moment_texture, depth_data.depth_texture, 1);

        EXPECT_TRUE(resolve_data.accum_texture.is_valid());
        EXPECT_TRUE(resolve_data.moments_texture.is_valid());
        EXPECT_TRUE(resolve_data.zeroth_moment_texture.is_valid());
        EXPECT_TRUE(resolve_data.depth_texture.is_valid());

        struct resolve_sink_data
        {
            render_graph::rg_texture_id accum;
        };

        graph.add_graphics_pass<resolve_sink_data>(
            "ResolveSinkPass",
            [acc = resolve_data.accum_texture](render_graph::pass_builder& builder, resolve_sink_data& data) {
                data.accum = builder.read(acc, rhi::pipeline_stage::fragment, rhi::resource_access::read,
                                          rhi::image_layout::general);
                builder.mark_sink();
            },
            []([[maybe_unused]] const resolve_sink_data&, [[maybe_unused]] render_graph::pass_execution_context&,
               [[maybe_unused]] rhi::command_list&) {});

        auto exec_res = graph.execute(*dev);
        EXPECT_TRUE(exec_res.has_value());
        dev->wait_idle();

        // Readback accumulator texture
        auto accum_readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = width * height * 4 * sizeof(uint16_t),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "TransparencyAccumReadbackBuffer",
        });

        const auto* accum_alloc = graph.get_physical_texture(resolve_data.accum_texture.id);
        ASSERT_NE(accum_alloc, nullptr);

        auto& port = dev->get_graphics_execution_port();
        auto& cmd = port.acquire_command_list();
        cmd.begin();

        const auto accum_region = rhi::buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = width,
            .image_extent_height = height,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(accum_alloc->handle, accum_readback_buf,
                                   span<const rhi::buffer_texture_copy_region>{&accum_region, 1});
        cmd.end();

        auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
        [[maybe_unused]] auto submit_res =
            port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
        dev->wait_idle();

        const auto* accum_pixels = static_cast<const uint16_t*>(accum_readback_buf.cpu_address);
        ASSERT_NE(accum_pixels, nullptr);

        // Center pixel (32, 32) is covered by transparent quad
        const auto center_idx = (32 * width + 32) * 4;
        EXPECT_NE(accum_pixels[center_idx + 0], 0); // Red channel > 0
        EXPECT_NE(accum_pixels[center_idx + 3], 0); // Alpha coverage > 0

        // Corner pixel (0, 0) is outside the quad (remains cleared 0)
        const auto corner_idx = 0;
        EXPECT_EQ(accum_pixels[corner_idx + 0], 0);
        EXPECT_EQ(accum_pixels[corner_idx + 1], 0);
        EXPECT_EQ(accum_pixels[corner_idx + 2], 0);
        EXPECT_EQ(accum_pixels[corner_idx + 3], 0);

        dev->destroy_buffer(accum_readback_buf);
    }

    TEST(render_system_tests, transparency_resolve_pass_transmissive_material_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;
        auto graph = render_graph::render_graph{width, height};

        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};

        auto mesh_id = meshes.register_mesh(create_test_mesh());

        // Transmissive Material (tinted blueish)
        auto mat_trans = core::material{};
        mat_trans.set_string(core::material::alpha_mode_name, "TRANSMISSIVE");
        mat_trans.set_scalar(core::material::transmissive_factor_name, 0.7F);
        mat_trans.set_vec4(core::material::base_color_factor_name, {0.2F, 0.4F, 0.9F, 1.0F});
        auto mat_trans_id = materials.register_material(tempest::move(mat_trans));

        pool.load_meshes(span<const guid>{&mesh_id, 1}, meshes, graph);
        pool.load_materials(span<const guid>{&mat_trans_id, 1}, materials, graph);

        auto scene = scene_constants{
            .projection = math::perspective(1.0F, 1.0F, 0.1F, 100.0F),
            .inv_projection = math::inverse(math::perspective(1.0F, 1.0F, 0.1F, 100.0F)),
            .view = math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                  math::vec3<float>{0.0F, 1.0F, 0.0F}),
            .inv_view =
                math::inverse(math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                            math::vec3<float>{0.0F, 1.0F, 0.0F})),
            .camera_position = {0.0F, 0.0F, -5.0F, 1.0F},
            .ambient_light = {0.28F, 0.30F, 0.36F, 1.0F},
            .sun_color_intensity = {1.0F, 1.0F, 1.0F, 2.0F},
            .sun_direction = {0.0F, -1.0F, 0.0F, 0.0F},
            .screen_size = {static_cast<float>(width), static_cast<float>(height)},
            .inv_screen_size = {1.0F / static_cast<float>(width), 1.0F / static_cast<float>(height)},
        };
        pool.write_scene_constants(scene);

        const auto mesh_gpu_addr = pool.get_mesh_address(mesh_id);
        const auto mat_gpu_addr = pool.get_material_address(mat_trans_id);
        auto mesh_layout_opt = pool.get_mesh_layout(mesh_id);
        ASSERT_TRUE(mesh_layout_opt.has_value());

        auto payload = object_payload{
            .model = math::mat4<float>{1.0F},
            .inv_transpose_model = math::mat4<float>{1.0F},
            .mesh_gpu_address = mesh_gpu_addr,
            .material_gpu_address = mat_gpu_addr,
            .parent_gpu_address = 0,
            .self_id = 1,
            .padding = 0,
        };
        auto objects = vector<object_payload>{};
        objects.push_back(payload);

        auto instances = vector<uint32_t>{};
        instances.push_back(0);

        auto commands = vector<indexed_indirect_command>{};
        commands.push_back(indexed_indirect_command{
            .index_count = mesh_layout_opt->index_count,
            .instance_count = 1,
            .first_index = (mesh_layout_opt->mesh_start_offset + mesh_layout_opt->index_offset) /
                           static_cast<uint32_t>(sizeof(uint32_t)),
            .vertex_offset = 0,
            .first_instance = 0,
        });

        pool.write_objects(objects);
        pool.write_instances(instances);
        pool.write_draw_commands(commands);

        auto accum_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage =
                rhi::texture_usage::color_attachment | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "TransparencyAccumTarget",
        });

        auto moments_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 2,
            .name = "MomentsArrayTarget",
        });

        auto zeroth_moment_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::r32_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "ZerothMomentTarget",
        });

        auto depth_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "DepthTarget",
        });

        add_frame_upload_pass(graph, pool);
        const auto& depth_data = add_depth_prepass(graph, pool, shaders, depth_tex, 0);
        const auto& clear_data =
            add_transparency_clear_pass(graph, shaders, moments_tex, zeroth_moment_tex, width, height);
        const auto& gather_data =
            add_transparency_gather_pass(graph, pool, shaders, clear_data.moments_texture,
                                         clear_data.zeroth_moment_texture, depth_data.depth_texture, 1);
        const auto& resolve_data =
            add_transparency_resolve_pass(graph, pool, shaders, accum_tex, gather_data.moments_texture,
                                          gather_data.zeroth_moment_texture, depth_data.depth_texture, 1);

        struct resolve_sink_data
        {
            render_graph::rg_texture_id accum;
        };

        graph.add_graphics_pass<resolve_sink_data>(
            "ResolveSinkPass",
            [acc = resolve_data.accum_texture](render_graph::pass_builder& builder, resolve_sink_data& data) {
                data.accum = builder.read(acc, rhi::pipeline_stage::fragment, rhi::resource_access::read,
                                          rhi::image_layout::general);
                builder.mark_sink();
            },
            []([[maybe_unused]] const resolve_sink_data&, [[maybe_unused]] render_graph::pass_execution_context&,
               [[maybe_unused]] rhi::command_list&) {});

        auto exec_res = graph.execute(*dev);
        EXPECT_TRUE(exec_res.has_value());
        dev->wait_idle();

        // Readback accumulator texture
        auto accum_readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = width * height * 4 * sizeof(uint16_t),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "TransparencyTransmissiveAccumReadbackBuffer",
        });

        const auto* accum_alloc = graph.get_physical_texture(resolve_data.accum_texture.id);
        ASSERT_NE(accum_alloc, nullptr);

        auto& port = dev->get_graphics_execution_port();
        auto& cmd = port.acquire_command_list();
        cmd.begin();

        const auto accum_region = rhi::buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = width,
            .image_extent_height = height,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(accum_alloc->handle, accum_readback_buf,
                                   span<const rhi::buffer_texture_copy_region>{&accum_region, 1});
        cmd.end();

        auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
        [[maybe_unused]] auto submit_res =
            port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
        dev->wait_idle();

        const auto* accum_pixels = static_cast<const uint16_t*>(accum_readback_buf.cpu_address);
        ASSERT_NE(accum_pixels, nullptr);

        // Center pixel (32, 32) is covered by transmissive quad
        const auto center_idx = (32 * width + 32) * 4;
        EXPECT_NE(accum_pixels[center_idx + 0], 0); // Color channel > 0
        EXPECT_NE(accum_pixels[center_idx + 3], 0); // Alpha coverage > 0

        dev->destroy_buffer(accum_readback_buf);
    }

    TEST(render_system_tests, transparency_resolve_pass_additive_accumulation_multiple_layers)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;
        auto graph = render_graph::render_graph{width, height};

        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};

        auto mesh_id = meshes.register_mesh(create_test_mesh());

        // Two transparent materials
        auto mat_red = core::material{};
        mat_red.set_string(core::material::alpha_mode_name, "BLEND");
        mat_red.set_vec4(core::material::base_color_factor_name, {1.0F, 0.0F, 0.0F, 0.5F});
        auto mat_red_id = materials.register_material(tempest::move(mat_red));

        auto mat_green = core::material{};
        mat_green.set_string(core::material::alpha_mode_name, "BLEND");
        mat_green.set_vec4(core::material::base_color_factor_name, {0.0F, 1.0F, 0.0F, 0.5F});
        auto mat_green_id = materials.register_material(tempest::move(mat_green));

        auto mat_ids = array{mat_red_id, mat_green_id};
        pool.load_meshes(span<const guid>{&mesh_id, 1}, meshes, graph);
        pool.load_materials(span<const guid>{mat_ids.data(), mat_ids.size()}, materials, graph);

        auto scene = scene_constants{
            .projection = math::perspective(1.0F, 1.0F, 0.1F, 100.0F),
            .inv_projection = math::inverse(math::perspective(1.0F, 1.0F, 0.1F, 100.0F)),
            .view = math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                  math::vec3<float>{0.0F, 1.0F, 0.0F}),
            .inv_view =
                math::inverse(math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                            math::vec3<float>{0.0F, 1.0F, 0.0F})),
            .camera_position = {0.0F, 0.0F, -5.0F, 1.0F},
            .ambient_light = {0.28F, 0.30F, 0.36F, 1.0F},
            .sun_color_intensity = {1.0F, 1.0F, 1.0F, 2.0F},
            .sun_direction = {0.0F, -1.0F, 0.0F, 0.0F},
            .screen_size = {static_cast<float>(width), static_cast<float>(height)},
            .inv_screen_size = {1.0F / static_cast<float>(width), 1.0F / static_cast<float>(height)},
        };
        pool.write_scene_constants(scene);

        const auto mesh_gpu_addr = pool.get_mesh_address(mesh_id);
        const auto mat_red_gpu_addr = pool.get_material_address(mat_red_id);
        const auto mat_green_gpu_addr = pool.get_material_address(mat_green_id);
        auto mesh_layout_opt = pool.get_mesh_layout(mesh_id);
        ASSERT_TRUE(mesh_layout_opt.has_value());

        auto payload0 = object_payload{
            .model = math::mat4<float>{1.0F},
            .inv_transpose_model = math::mat4<float>{1.0F},
            .mesh_gpu_address = mesh_gpu_addr,
            .material_gpu_address = mat_red_gpu_addr,
            .parent_gpu_address = 0,
            .self_id = 1,
            .padding = 0,
        };
        auto payload1 = object_payload{
            .model = math::translate(math::mat4<float>{1.0F}, math::vec3<float>{0.0F, 0.0F, 0.5F}),
            .inv_transpose_model = math::mat4<float>{1.0F},
            .mesh_gpu_address = mesh_gpu_addr,
            .material_gpu_address = mat_green_gpu_addr,
            .parent_gpu_address = 0,
            .self_id = 2,
            .padding = 0,
        };
        auto objects = vector<object_payload>{};
        objects.push_back(payload0);
        objects.push_back(payload1);

        auto instances = vector<uint32_t>{};
        instances.push_back(0);
        instances.push_back(1);

        auto commands = vector<indexed_indirect_command>{};
        commands.push_back(indexed_indirect_command{
            .index_count = mesh_layout_opt->index_count,
            .instance_count = 1,
            .first_index = (mesh_layout_opt->mesh_start_offset + mesh_layout_opt->index_offset) /
                           static_cast<uint32_t>(sizeof(uint32_t)),
            .vertex_offset = 0,
            .first_instance = 0,
        });
        commands.push_back(indexed_indirect_command{
            .index_count = mesh_layout_opt->index_count,
            .instance_count = 1,
            .first_index = (mesh_layout_opt->mesh_start_offset + mesh_layout_opt->index_offset) /
                           static_cast<uint32_t>(sizeof(uint32_t)),
            .vertex_offset = 0,
            .first_instance = 1,
        });

        pool.write_objects(objects);
        pool.write_instances(instances);
        pool.write_draw_commands(commands);

        auto accum_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage =
                rhi::texture_usage::color_attachment | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "TransparencyAccumTarget",
        });

        auto moments_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 2,
            .name = "MomentsArrayTarget",
        });

        auto zeroth_moment_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::r32_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "ZerothMomentTarget",
        });

        auto depth_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "DepthTarget",
        });

        add_frame_upload_pass(graph, pool);
        const auto& depth_data = add_depth_prepass(graph, pool, shaders, depth_tex, 0);
        const auto& clear_data =
            add_transparency_clear_pass(graph, shaders, moments_tex, zeroth_moment_tex, width, height);
        const auto& gather_data =
            add_transparency_gather_pass(graph, pool, shaders, clear_data.moments_texture,
                                         clear_data.zeroth_moment_texture, depth_data.depth_texture, 2);
        const auto& resolve_data =
            add_transparency_resolve_pass(graph, pool, shaders, accum_tex, gather_data.moments_texture,
                                          gather_data.zeroth_moment_texture, depth_data.depth_texture, 2);

        struct resolve_sink_data
        {
            render_graph::rg_texture_id accum;
        };

        graph.add_graphics_pass<resolve_sink_data>(
            "ResolveSinkPass",
            [acc = resolve_data.accum_texture](render_graph::pass_builder& builder, resolve_sink_data& data) {
                data.accum = builder.read(acc, rhi::pipeline_stage::fragment, rhi::resource_access::read,
                                          rhi::image_layout::general);
                builder.mark_sink();
            },
            []([[maybe_unused]] const resolve_sink_data&, [[maybe_unused]] render_graph::pass_execution_context&,
               [[maybe_unused]] rhi::command_list&) {});

        auto exec_res = graph.execute(*dev);
        EXPECT_TRUE(exec_res.has_value());
        dev->wait_idle();

        // Readback accumulator texture
        auto accum_readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = width * height * 4 * sizeof(uint16_t),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "TransparencyMultiLayerAccumReadbackBuffer",
        });

        const auto* accum_alloc = graph.get_physical_texture(resolve_data.accum_texture.id);
        ASSERT_NE(accum_alloc, nullptr);

        auto& port = dev->get_graphics_execution_port();
        auto& cmd = port.acquire_command_list();
        cmd.begin();

        const auto accum_region = rhi::buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = width,
            .image_extent_height = height,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(accum_alloc->handle, accum_readback_buf,
                                   span<const rhi::buffer_texture_copy_region>{&accum_region, 1});
        cmd.end();

        auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
        [[maybe_unused]] auto submit_res =
            port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
        dev->wait_idle();

        const auto* accum_pixels = static_cast<const uint16_t*>(accum_readback_buf.cpu_address);
        ASSERT_NE(accum_pixels, nullptr);

        // Center pixel has both red and green contributions
        const auto center_idx = (32 * width + 32) * 4;
        EXPECT_NE(accum_pixels[center_idx + 0], 0); // Red > 0
        EXPECT_NE(accum_pixels[center_idx + 1], 0); // Green > 0
        EXPECT_NE(accum_pixels[center_idx + 3], 0); // Alpha > 0

        dev->destroy_buffer(accum_readback_buf);
    }

    TEST(render_system_tests, transparency_blend_push_constants_layout)
    {
        EXPECT_EQ(sizeof(transparency_blend_push_constants), 16U);
        EXPECT_EQ(offsetof(transparency_blend_push_constants, accumulation_texture_index), 0U);
        EXPECT_EQ(offsetof(transparency_blend_push_constants, zeroth_moment_storage_index), 4U);
        EXPECT_EQ(offsetof(transparency_blend_push_constants, linear_sampler_index), 8U);
        EXPECT_EQ(offsetof(transparency_blend_push_constants, padding), 12U);
    }

    TEST(render_system_tests, transparency_blend_pass_discard_when_zeroth_moment_empty)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;
        auto graph = render_graph::render_graph{width, height};

        auto hdr_color_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage =
                rhi::texture_usage::color_attachment | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "HDRColorTarget",
        });

        auto accum_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "TransparencyAccumTarget",
        });

        auto moments_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 2,
            .name = "MomentsArrayTarget",
        });

        auto zeroth_moment_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::r32_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "ZerothMomentTarget",
        });

        add_frame_upload_pass(graph, pool);
        const auto& skybox_data = add_skybox_pass(graph, pool, shaders, hdr_color_tex);
        const auto& clear_data =
            add_transparency_clear_pass(graph, shaders, moments_tex, zeroth_moment_tex, width, height);
        // Blend directly after clear with no gather or resolve (b0 == 0 everywhere)
        const auto& blend_data = add_transparency_blend_pass(graph, pool, shaders, skybox_data.hdr_color, accum_tex,
                                                             clear_data.zeroth_moment_texture);

        auto exec_res = graph.execute(*dev);
        EXPECT_TRUE(exec_res.has_value());
        dev->wait_idle();

        // Readback HDR Color texture
        auto hdr_readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = width * height * 4 * sizeof(uint16_t),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "HDRColorReadbackBufferEmptyMoments",
        });

        const auto* hdr_alloc = graph.get_physical_texture(blend_data.hdr_color.id);
        ASSERT_NE(hdr_alloc, nullptr);

        auto& port = dev->get_graphics_execution_port();
        auto& cmd = port.acquire_command_list();
        cmd.begin();

        const auto hdr_region = rhi::buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = width,
            .image_extent_height = height,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(hdr_alloc->handle, hdr_readback_buf,
                                   span<const rhi::buffer_texture_copy_region>{&hdr_region, 1});
        cmd.end();

        auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
        [[maybe_unused]] auto submit_res =
            port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
        dev->wait_idle();

        const auto* hdr_pixels = static_cast<const uint16_t*>(hdr_readback_buf.cpu_address);
        ASSERT_NE(hdr_pixels, nullptr);

        // Center pixel should be pure skybox (no alpha blending altered it)
        const auto center_idx = (32 * width + 32) * 4;
        EXPECT_NE(hdr_pixels[center_idx + 2], 0); // Blue channel from skybox > 0

        dev->destroy_buffer(hdr_readback_buf);
    }

    TEST(render_system_tests, transparency_full_chain_clear_gather_resolve_blend_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto pool = resource_pool{*dev};
        auto shaders = shader_manager{*dev, fixture.asset_db};
        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;
        auto graph = render_graph::render_graph{width, height};

        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};

        auto mesh_id = meshes.register_mesh(create_test_mesh());

        // Transparent Blend Material (pure red)
        auto mat_blend = core::material{};
        mat_blend.set_string(core::material::alpha_mode_name, "BLEND");
        mat_blend.set_vec4(core::material::base_color_factor_name, {1.0F, 0.0F, 0.0F, 0.5F});
        mat_blend.set_scalar(core::material::metallic_factor_name, 0.0F);
        mat_blend.set_scalar(core::material::roughness_factor_name, 0.5F);
        auto mat_blend_id = materials.register_material(tempest::move(mat_blend));

        pool.load_meshes(span<const guid>{&mesh_id, 1}, meshes, graph);
        pool.load_materials(span<const guid>{&mat_blend_id, 1}, materials, graph);

        auto scene = scene_constants{
            .projection = math::perspective(1.0F, 1.0F, 0.1F, 100.0F),
            .inv_projection = math::inverse(math::perspective(1.0F, 1.0F, 0.1F, 100.0F)),
            .view = math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                  math::vec3<float>{0.0F, 1.0F, 0.0F}),
            .inv_view =
                math::inverse(math::look_at(math::vec3<float>{0.0F, 0.0F, -5.0F}, math::vec3<float>{0.0F, 0.0F, 0.0F},
                                            math::vec3<float>{0.0F, 1.0F, 0.0F})),
            .camera_position = {0.0F, 0.0F, -5.0F, 1.0F},
            .ambient_light = {0.28F, 0.30F, 0.36F, 1.0F},
            .sun_color_intensity = {1.0F, 1.0F, 1.0F, 2.0F},
            .sun_direction = {0.0F, -1.0F, 0.0F, 0.0F},
            .screen_size = {static_cast<float>(width), static_cast<float>(height)},
            .inv_screen_size = {1.0F / static_cast<float>(width), 1.0F / static_cast<float>(height)},
        };
        pool.write_scene_constants(scene);

        const auto mesh_gpu_addr = pool.get_mesh_address(mesh_id);
        const auto mat_gpu_addr = pool.get_material_address(mat_blend_id);
        auto mesh_layout_opt = pool.get_mesh_layout(mesh_id);
        ASSERT_TRUE(mesh_layout_opt.has_value());

        auto payload = object_payload{
            .model = math::mat4<float>{1.0F},
            .inv_transpose_model = math::mat4<float>{1.0F},
            .mesh_gpu_address = mesh_gpu_addr,
            .material_gpu_address = mat_gpu_addr,
            .parent_gpu_address = 0,
            .self_id = 1,
            .padding = 0,
        };
        auto objects = vector<object_payload>{};
        objects.push_back(payload);

        auto instances = vector<uint32_t>{};
        instances.push_back(0);

        auto commands = vector<indexed_indirect_command>{};
        commands.push_back(indexed_indirect_command{
            .index_count = mesh_layout_opt->index_count,
            .instance_count = 1,
            .first_index = (mesh_layout_opt->mesh_start_offset + mesh_layout_opt->index_offset) /
                           static_cast<uint32_t>(sizeof(uint32_t)),
            .vertex_offset = 0,
            .first_instance = 0,
        });

        pool.write_objects(objects);
        pool.write_instances(instances);
        pool.write_draw_commands(commands);

        auto hdr_color_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage =
                rhi::texture_usage::color_attachment | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "HDRColorTarget",
        });

        auto accum_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage =
                rhi::texture_usage::color_attachment | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "TransparencyAccumTarget",
        });

        auto moments_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::rgba16_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 2,
            .name = "MomentsArrayTarget",
        });

        auto zeroth_moment_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::r32_float,
            .usage = rhi::texture_usage::storage | rhi::texture_usage::sampled | rhi::texture_usage::transfer_src,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "ZerothMomentTarget",
        });

        auto depth_tex = graph.create_texture(render_graph::rg_texture_desc{
            .size = render_graph::rg_texture_size::absolute(width, height),
            .format = rhi::data_format::depth32_float,
            .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
            .mip_levels = 1,
            .array_layers = 1,
            .name = "DepthTarget",
        });

        add_frame_upload_pass(graph, pool);
        const auto& skybox_data = add_skybox_pass(graph, pool, shaders, hdr_color_tex);
        const auto& depth_data = add_depth_prepass(graph, pool, shaders, depth_tex, 0);
        const auto& clear_data =
            add_transparency_clear_pass(graph, shaders, moments_tex, zeroth_moment_tex, width, height);
        const auto& gather_data =
            add_transparency_gather_pass(graph, pool, shaders, clear_data.moments_texture,
                                         clear_data.zeroth_moment_texture, depth_data.depth_texture, 1);
        const auto& resolve_data =
            add_transparency_resolve_pass(graph, pool, shaders, accum_tex, gather_data.moments_texture,
                                          gather_data.zeroth_moment_texture, depth_data.depth_texture, 1);
        const auto& blend_data = add_transparency_blend_pass(
            graph, pool, shaders, skybox_data.hdr_color, resolve_data.accum_texture, gather_data.zeroth_moment_texture);

        struct blend_sink_data
        {
            render_graph::rg_texture_id hdr;
        };

        graph.add_graphics_pass<blend_sink_data>(
            "BlendSinkPass",
            [h = blend_data.hdr_color](render_graph::pass_builder& builder, blend_sink_data& data) {
                data.hdr = builder.read(h, rhi::pipeline_stage::fragment, rhi::resource_access::read,
                                        rhi::image_layout::general);
                builder.mark_sink();
            },
            []([[maybe_unused]] const blend_sink_data&, [[maybe_unused]] render_graph::pass_execution_context&,
               [[maybe_unused]] rhi::command_list&) {});

        auto exec_res = graph.execute(*dev);
        EXPECT_TRUE(exec_res.has_value());
        dev->wait_idle();

        // Readback HDR Color texture
        auto hdr_readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = width * height * 4 * sizeof(uint16_t),
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "HDRColorReadbackBuffer",
        });

        const auto* hdr_alloc = graph.get_physical_texture(blend_data.hdr_color.id);
        ASSERT_NE(hdr_alloc, nullptr);

        auto& port = dev->get_graphics_execution_port();
        auto& cmd = port.acquire_command_list();
        cmd.begin();

        const auto hdr_region = rhi::buffer_texture_copy_region{
            .buffer_offset = 0,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .mip_level = 0,
            .base_array_layer = 0,
            .array_layer_count = 1,
            .image_offset_x = 0,
            .image_offset_y = 0,
            .image_offset_z = 0,
            .image_extent_width = width,
            .image_extent_height = height,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(hdr_alloc->handle, hdr_readback_buf,
                                   span<const rhi::buffer_texture_copy_region>{&hdr_region, 1});
        cmd.end();

        auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
        [[maybe_unused]] auto submit_res =
            port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
        dev->wait_idle();

        const auto* hdr_pixels = static_cast<const uint16_t*>(hdr_readback_buf.cpu_address);
        ASSERT_NE(hdr_pixels, nullptr);

        // Center pixel (32, 32) should have transparent red blended over skybox background
        const auto center_idx = (32 * width + 32) * 4;
        EXPECT_NE(hdr_pixels[center_idx + 0], 0); // Red channel > 0

        // Corner pixel (2, 2) is outside geometry, retains skybox background
        const auto corner_idx = (2 * width + 2) * 4;
        EXPECT_NE(hdr_pixels[corner_idx + 2], 0); // Blue channel from skybox > 0

        dev->destroy_buffer(hdr_readback_buf);
    }

    TEST(render_system_tests, mboit_full_pipeline_multi_layer_transparency_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = width,
            .render_height = height,
            .tonemapped_color_format = rhi::data_format::rgba8_unorm,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        auto rend = builder.build(*dev, log);
        ASSERT_NE(rend, nullptr);

        // 1. Setup Camera Entity
        auto cam_ent = registry.create();
        registry.assign(cam_ent, camera_component{
                                     .aspect_ratio = 1.0F,
                                     .vertical_fov = 1.04719755F,
                                     .near_plane = 0.01F,
                                 });
        auto cam_tx = ecs::transform_component::identity();
        cam_tx.position({0.0F, 0.0F, -4.0F});
        registry.assign(cam_ent, cam_tx);

        // 2. Setup Sun Light Entity
        auto sun_ent = registry.create();
        registry.assign(sun_ent, directional_light_component{
                                     .color = {1.0F, 1.0F, 1.0F},
                                     .intensity = 3.0F,
                                 });
        registry.assign(sun_ent, shadow_caster_component{
                                     .resolution = 1024,
                                     .num_cascades = 3,
                                     .split_lambda = 0.5F,
                                     .max_shadow_distance = 20.0F,
                                 });
        auto sun_tx = ecs::transform_component::identity();
        sun_tx.rotation({math::as_radians(70.0F), math::as_radians(15.0F), 0.0F});
        registry.assign(sun_ent, sun_tx);

        // 3. Setup Registries and Materials for 3 Layers:
        //    Layer 1: Opaque background (Blue) at z = 1.0
        //    Layer 2: Transmissive middle quad (Green tint) at z = 0.5
        //    Layer 3: Alpha-blended foreground quad (Semi-transparent Red) at z = 0.0
        auto mesh_id = meshes.register_mesh(create_test_mesh());

        // Opaque background material (Blue)
        auto mat_opaque = core::material{};
        mat_opaque.set_string(core::material::alpha_mode_name, "OPAQUE");
        mat_opaque.set_vec4(core::material::base_color_factor_name, {0.0F, 0.0F, 1.0F, 1.0F});
        mat_opaque.set_scalar(core::material::metallic_factor_name, 0.0F);
        mat_opaque.set_scalar(core::material::roughness_factor_name, 0.5F);
        auto mat_opaque_id = materials.register_material(tempest::move(mat_opaque));

        // Transmissive material (Green tint)
        auto mat_trans = core::material{};
        mat_trans.set_string(core::material::alpha_mode_name, "TRANSMISSIVE");
        mat_trans.set_scalar(core::material::transmissive_factor_name, 0.8F);
        mat_trans.set_vec4(core::material::base_color_factor_name, {0.0F, 1.0F, 0.0F, 1.0F});
        mat_trans.set_scalar(core::material::metallic_factor_name, 0.0F);
        mat_trans.set_scalar(core::material::roughness_factor_name, 0.1F);
        auto mat_trans_id = materials.register_material(tempest::move(mat_trans));

        // Alpha-blended material (Semi-transparent Red)
        auto mat_blend = core::material{};
        mat_blend.set_string(core::material::alpha_mode_name, "BLEND");
        mat_blend.set_vec4(core::material::base_color_factor_name, {1.0F, 0.0F, 0.0F, 0.5F});
        mat_blend.set_scalar(core::material::metallic_factor_name, 0.0F);
        mat_blend.set_scalar(core::material::roughness_factor_name, 0.5F);
        auto mat_blend_id = materials.register_material(tempest::move(mat_blend));

        // Entity 1: Opaque Background
        auto ent_opaque = registry.create();
        registry.assign(ent_opaque, core::mesh_component{.mesh_id = mesh_id});
        registry.assign(ent_opaque, core::material_component{.material_id = mat_opaque_id});
        auto tx_opaque = ecs::transform_component::identity();
        tx_opaque.position({0.0F, 0.0F, 1.0F});
        registry.assign(ent_opaque, tx_opaque);

        // Entity 2: Transmissive Middle Quad
        auto ent_trans = registry.create();
        registry.assign(ent_trans, core::mesh_component{.mesh_id = mesh_id});
        registry.assign(ent_trans, core::material_component{.material_id = mat_trans_id});
        auto tx_trans = ecs::transform_component::identity();
        tx_trans.position({0.0F, 0.0F, 0.5F});
        registry.assign(ent_trans, tx_trans);

        // Entity 3: Alpha-blended Foreground Quad
        auto ent_blend = registry.create();
        registry.assign(ent_blend, core::mesh_component{.mesh_id = mesh_id});
        registry.assign(ent_blend, core::material_component{.material_id = mat_blend_id});
        auto tx_blend = ecs::transform_component::identity();
        tx_blend.position({0.0F, 0.0F, 0.0F});
        registry.assign(ent_blend, tx_blend);

        // 4. Prepare Frame
        rend->prepare_frame(width, height);

        EXPECT_TRUE(rend->get_moments_texture().is_valid());
        EXPECT_TRUE(rend->get_zeroth_moment_texture().is_valid());
        EXPECT_TRUE(rend->get_transparency_accum_texture().is_valid());

        // 5. Execute Full Renderer Pipeline
        auto render_res = rend->render();
        EXPECT_TRUE(render_res.has_value());
        dev->wait_idle();

        // 6. Readback Tonemapped Output Buffer
        auto readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = width * height * 4,
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "MultiLayerTransparencyReadbackBuffer",
        });

        const auto* alloc = rend->get_render_graph().get_physical_texture(rend->get_tonemapped_color_texture().id);
        ASSERT_NE(alloc, nullptr);

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
            .image_extent_width = width,
            .image_extent_height = height,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(alloc->handle, readback_buf,
                                   span<const rhi::buffer_texture_copy_region>{&region, 1});
        cmd.end();

        auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
        [[maybe_unused]] auto submit_res =
            port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
        dev->wait_idle();

        const auto* pixels = static_cast<const uint8_t*>(readback_buf.cpu_address);
        ASSERT_NE(pixels, nullptr);

        // Center pixel (32, 32) should contain composite of:
        // Red foreground blend + Green middle transmissive + Blue background opaque
        const auto center_idx = (32 * width + 32) * 4;
        EXPECT_GT(pixels[center_idx + 0], 0); // Red channel > 0
        EXPECT_GT(pixels[center_idx + 3], 0); // Alpha > 0

        // Corner pixel (0, 0) is outside geometry, retains skybox background
        const auto corner_idx = 0;
        EXPECT_GT(pixels[corner_idx + 2], 0); // Blue channel from skybox > 0

        dev->destroy_buffer(readback_buf);
    }

    TEST(render_system_tests, abeautifulgame_asset_loading_and_render_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        constexpr uint32_t width = 1280;
        constexpr uint32_t height = 720;

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = width,
            .render_height = height,
            .tonemapped_color_format = rhi::data_format::rgba8_unorm,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        auto rend = builder.build(*dev, log);
        ASSERT_NE(rend, nullptr);

        // 1. Setup Camera Entity pointing at Chessboard
        auto cam_ent = registry.create();
        registry.assign(cam_ent, camera_component{
                                     .aspect_ratio = static_cast<float>(width) / static_cast<float>(height),
                                     .vertical_fov = 1.04719755F,
                                     .near_plane = 0.01F,
                                 });
        auto cam_tx = ecs::transform_component::identity();
        cam_tx.position({0.0F, 0.35F, -0.55F});
        cam_tx.rotation({math::as_radians(28.0F), 0.0F, 0.0F});
        registry.assign(cam_ent, cam_tx);

        // 2. Setup Sun Light Entity
        auto sun_ent = registry.create();
        registry.assign(sun_ent, directional_light_component{
                                     .color = {1.0F, 0.98F, 0.92F},
                                     .intensity = 4.0F,
                                 });
        registry.assign(sun_ent, shadow_caster_component{
                                     .resolution = 2048,
                                     .num_cascades = 4,
                                     .split_lambda = 0.5F,
                                     .max_shadow_distance = 2.0F,
                                     .normal_bias = 0.005F,
                                     .depth_bias = 0.001F,
                                 });
        auto sun_tx = ecs::transform_component::identity();
        sun_tx.rotation({math::as_radians(65.0F), math::as_radians(25.0F), 0.0F});
        registry.assign(sun_ent, sun_tx);

        // 3. Load ABeautifulGame.gltf
        auto asset_type_reg = assets::asset_type_registry{};
        auto asset_db = assets::asset_database{&asset_type_reg};
        assets::register_default_importers(asset_db, &meshes, &textures, &materials);

        const auto chess_path = "assets/glTF-Sample-Assets/Models/ABeautifulGame/glTF/ABeautifulGame.gltf";

        if (std::filesystem::exists(chess_path))
        {
            auto prefab_root = asset_db.load(chess_path, registry);
            ASSERT_TRUE(prefab_root != ecs::tombstone);
        }

        // 4. Prepare Frame
        rend->prepare_frame(width, height);

        EXPECT_TRUE(rend->get_moments_texture().is_valid());
        EXPECT_TRUE(rend->get_zeroth_moment_texture().is_valid());
        EXPECT_TRUE(rend->get_transparency_accum_texture().is_valid());

        // 5. Execute Full Renderer Pipeline
        auto render_res = rend->render();
        EXPECT_TRUE(render_res.has_value());
        dev->wait_idle();

        // 6. Readback Tonemapped Output Buffer
        auto readback_buf = dev->create_buffer(rhi::buffer_desc{
            .size = width * height * 4,
            .memory_usage = rhi::memory_usage::readback,
            .usage = rhi::buffer_usage::transfer_dst,
            .name = "ABeautifulGameReadbackBuffer",
        });

        const auto* alloc = rend->get_render_graph().get_physical_texture(rend->get_tonemapped_color_texture().id);
        ASSERT_NE(alloc, nullptr);

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
            .image_extent_width = width,
            .image_extent_height = height,
            .image_extent_depth = 1,
        };
        cmd.copy_texture_to_buffer(alloc->handle, readback_buf,
                                   span<const rhi::buffer_texture_copy_region>{&region, 1});
        cmd.end();

        auto cmd_ptrs = array<const rhi::command_list*, 1>{&cmd};
        [[maybe_unused]] auto submit_res =
            port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
        dev->wait_idle();

        const auto* pixels = static_cast<const uint8_t*>(readback_buf.cpu_address);
        ASSERT_NE(pixels, nullptr);

        // Center pixel (640, 360) should see chessboard / chess pieces
        const auto center_idx = (360 * width + 640) * 4;
        EXPECT_GT(pixels[center_idx + 3], 0); // Valid alpha

        dev->destroy_buffer(readback_buf);
    }

    TEST(render_system_tests, resource_pool_lights_buffer_slicing_and_bda)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        {
            auto cfg = resource_pool_config{
                .max_lights = 16,
                .frames_in_flight = 2,
            };
            auto pool = resource_pool{*dev, cfg};

            EXPECT_NE(pool.get_lights_buffer().handle, 0ULL);
            EXPECT_NE(pool.get_lights_buffer_address(), 0ULL);

            // Slot 0 write
            auto light0 = light_payload{
                .color_intensity = {1.0F, 0.0F, 0.0F, 5.0F},
                .position_falloff = {10.0F, 20.0F, 30.0F, 15.0F},
                .direction_angle = {0.0F, -1.0F, 0.0F, 0.0F},
                .type = 1,
                .enabled = 1,
                .padding = {0, 0},
            };
            pool.write_lights(span<const light_payload>{&light0, 1});

            const auto lights_addr0 = pool.get_lights_buffer_address();

            // Advance to Slot 1
            pool.advance_frame();
            EXPECT_EQ(pool.get_frame_slot(), 1U);

            auto light1 = light_payload{
                .color_intensity = {0.0F, 1.0F, 0.0F, 10.0F},
                .position_falloff = {-5.0F, 0.0F, 5.0F, 25.0F},
                .direction_angle = {0.0F, 0.0F, 1.0F, 0.0F},
                .type = 1,
                .enabled = 1,
                .padding = {0, 0},
            };
            pool.write_lights(span<const light_payload>{&light1, 1});

            const auto lights_addr1 = pool.get_lights_buffer_address();

            // Verify BDA slicing separation
            EXPECT_NE(lights_addr0, lights_addr1);
            EXPECT_EQ(lights_addr1 - lights_addr0, sizeof(light_payload) * 16);

            // Readback from mapped CPU pointer to verify frame isolation
            const auto* cpu_lights = static_cast<const light_payload*>(pool.get_lights_buffer().cpu_address);
            ASSERT_NE(cpu_lights, nullptr);
            EXPECT_FLOAT_EQ(cpu_lights[0].color_intensity.x, 1.0F);
            EXPECT_FLOAT_EQ(cpu_lights[0].position_falloff.x, 10.0F);
            EXPECT_FLOAT_EQ(cpu_lights[16].color_intensity.y, 1.0F);
            EXPECT_FLOAT_EQ(cpu_lights[16].position_falloff.x, -5.0F);
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, renderer_event_driven_point_light_registration)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};

        // Create pre-existing light before renderer construction to test discovery
        auto pre_light = registry.create();
        registry.assign(pre_light, point_light_component{
                                       .color = {1.0F, 0.5F, 0.2F},
                                       .intensity = 4.0F,
                                       .range = 10.0F,
                                   });
        auto pre_tx = ecs::transform_component::identity();
        pre_tx.position({1.0F, 2.0F, 3.0F});
        registry.assign(pre_light, pre_tx);

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 1. Verify pre-existing light discovered
            EXPECT_EQ(rend->get_tracked_point_light_count(), 1U);

            rend->prepare_frame(1280, 720);
            auto cached = rend->get_cached_lights();
            ASSERT_EQ(cached.size(), 1U);
            EXPECT_FLOAT_EQ(cached[0].color_intensity.x, 1.0F);
            EXPECT_FLOAT_EQ(cached[0].color_intensity.y, 0.5F);
            EXPECT_FLOAT_EQ(cached[0].color_intensity.z, 0.2F);
            EXPECT_FLOAT_EQ(cached[0].color_intensity.w, 4.0F);
            EXPECT_FLOAT_EQ(cached[0].position_falloff.x, 1.0F);
            EXPECT_FLOAT_EQ(cached[0].position_falloff.y, 2.0F);
            EXPECT_FLOAT_EQ(cached[0].position_falloff.z, 3.0F);
            EXPECT_FLOAT_EQ(cached[0].position_falloff.w, 10.0F);
            EXPECT_EQ(cached[0].type, 1U);
            EXPECT_EQ(cached[0].enabled, 1U);

            // 2. Add second point light dynamically
            auto light2 = registry.create();
            registry.assign(light2, point_light_component{
                                        .color = {0.0F, 1.0F, 0.0F},
                                        .intensity = 8.0F,
                                        .range = 20.0F,
                                    });
            auto tx2 = ecs::transform_component::identity();
            tx2.position({-5.0F, 0.0F, 10.0F});
            registry.assign(light2, tx2);

            EXPECT_EQ(rend->get_tracked_point_light_count(), 2U);

            rend->prepare_frame(1280, 720);
            cached = rend->get_cached_lights();
            ASSERT_EQ(cached.size(), 2U);
            EXPECT_FLOAT_EQ(cached[1].color_intensity.y, 1.0F);
            EXPECT_FLOAT_EQ(cached[1].position_falloff.x, -5.0F);

            // 3. Mutate transform of light2
            tx2.position({15.0F, 25.0F, 35.0F});
            registry.replace(light2, tx2);

            rend->prepare_frame(1280, 720);
            cached = rend->get_cached_lights();
            ASSERT_EQ(cached.size(), 2U);
            EXPECT_FLOAT_EQ(cached[1].position_falloff.x, 15.0F);
            EXPECT_FLOAT_EQ(cached[1].position_falloff.y, 25.0F);
            EXPECT_FLOAT_EQ(cached[1].position_falloff.z, 35.0F);

            // 4. Mutate point light component
            registry.replace(light2, point_light_component{
                                         .color = {0.0F, 0.0F, 1.0F},
                                         .intensity = 12.0F,
                                         .range = 30.0F,
                                     });

            rend->prepare_frame(1280, 720);
            cached = rend->get_cached_lights();
            ASSERT_EQ(cached.size(), 2U);
            EXPECT_FLOAT_EQ(cached[1].color_intensity.z, 1.0F);
            EXPECT_FLOAT_EQ(cached[1].color_intensity.w, 12.0F);
            EXPECT_FLOAT_EQ(cached[1].position_falloff.w, 30.0F);

            // 5. Remove component or destroy entity
            registry.destroy(pre_light);
            EXPECT_EQ(rend->get_tracked_point_light_count(), 1U);

            rend->prepare_frame(1280, 720);
            cached = rend->get_cached_lights();
            ASSERT_EQ(cached.size(), 1U);
            EXPECT_FLOAT_EQ(cached[0].color_intensity.z, 1.0F); // Remaining light is light2
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, clustered_forward_opaque_point_lighting)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 1. Setup Camera Entity at (0, 0, -5) looking towards +Z
            auto cam_ent = registry.create();
            registry.assign(cam_ent, camera_component{
                                         .aspect_ratio = 1280.0F / 720.0F,
                                         .vertical_fov = 1.5707963F,
                                         .near_plane = 0.01F,
                                     });
            auto cam_tx = ecs::transform_component::identity();
            cam_tx.position({0.0F, 0.0F, -5.0F});
            registry.assign(cam_ent, cam_tx);

            // 2. Setup Point Light Entity (between camera and quad, at (0, 0, -2))
            auto light_ent = registry.create();
            registry.assign(light_ent, point_light_component{
                                           .color = {1.0F, 0.2F, 0.2F},
                                           .intensity = 20.0F,
                                           .range = 10.0F,
                                       });
            auto light_tx = ecs::transform_component::identity();
            light_tx.position({0.0F, 0.0F, -2.0F});
            registry.assign(light_ent, light_tx);

            // 3. Setup Renderable Geometry Entity (Quad at z=0)
            auto mesh_id = meshes.register_mesh(create_test_mesh());
            auto mat = core::material{};
            mat.set_vec4(core::material::base_color_factor_name, {0.9F, 0.9F, 0.9F, 1.0F});
            mat.set_scalar(core::material::metallic_factor_name, 0.0F);
            mat.set_scalar(core::material::roughness_factor_name, 0.5F);
            auto mat_id = materials.register_material(tempest::move(mat));

            auto geom_ent = registry.create();
            registry.assign(geom_ent, core::mesh_component{.mesh_id = mesh_id});
            registry.assign(geom_ent, core::material_component{.material_id = mat_id});
            registry.assign(geom_ent, ecs::transform_component::identity());

            // 4. Prepare Frame
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
                .name = "OpaquePointLightReadbackBuffer",
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
                [[maybe_unused]] auto submit_res =
                    port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
                dev->wait_idle();

                const auto* pixels = static_cast<const uint8_t*>(readback_buf.cpu_address);
                if (pixels)
                {
                    // Sample center pixel (640, 360) (Rendered point-lit PBR geometry)
                    const auto center_idx = (360 * 1280 + 640) * 4;
                    const auto r = pixels[center_idx + 0];
                    const auto a = pixels[center_idx + 3];
                    // Strong red illumination from the point light
                    EXPECT_GT(r, 100);
                    EXPECT_EQ(a, 255);
                }
            }

            dev->destroy_buffer(readback_buf);
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, clustered_forward_transparency_point_lighting)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 1. Setup Camera Entity at (0, 0, -5) looking towards +Z
            auto cam_ent = registry.create();
            registry.assign(cam_ent, camera_component{
                                         .aspect_ratio = 1280.0F / 720.0F,
                                         .vertical_fov = 1.5707963F,
                                         .near_plane = 0.01F,
                                     });
            auto cam_tx = ecs::transform_component::identity();
            cam_tx.position({0.0F, 0.0F, -5.0F});
            registry.assign(cam_ent, cam_tx);

            // 2. Setup Point Light Entity with green color
            auto light_ent = registry.create();
            registry.assign(light_ent, point_light_component{
                                           .color = {0.1F, 1.0F, 0.1F},
                                           .intensity = 25.0F,
                                           .range = 10.0F,
                                       });
            auto light_tx = ecs::transform_component::identity();
            light_tx.position({0.0F, 0.0F, -2.0F});
            registry.assign(light_ent, light_tx);

            // 3. Setup Transparent Renderable Geometry Entity (Blend quad at z=0)
            auto mesh_id = meshes.register_mesh(create_test_mesh());
            auto mat = core::material{};
            mat.set_vec4(core::material::base_color_factor_name, {0.1F, 0.9F, 0.1F, 0.8F});
            mat.set_string(core::material::alpha_mode_name, "BLEND");
            mat.set_scalar(core::material::metallic_factor_name, 0.0F);
            mat.set_scalar(core::material::roughness_factor_name, 0.5F);
            auto mat_id = materials.register_material(tempest::move(mat));

            auto geom_ent = registry.create();
            registry.assign(geom_ent, core::mesh_component{.mesh_id = mesh_id});
            registry.assign(geom_ent, core::material_component{.material_id = mat_id});
            registry.assign(geom_ent, ecs::transform_component::identity());

            // 4. Prepare Frame
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
                .name = "TranspPointLightReadbackBuffer",
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
                [[maybe_unused]] auto submit_res =
                    port.submit(span<const rhi::command_list*>{cmd_ptrs.data(), cmd_ptrs.size()}, {}, {});
                dev->wait_idle();

                const auto* pixels = static_cast<const uint8_t*>(readback_buf.cpu_address);
                if (pixels)
                {
                    // Sample center pixel (640, 360) (Rendered transparent point-lit PBR geometry)
                    const auto center_idx = (360 * 1280 + 640) * 4;
                    const auto g = pixels[center_idx + 1];
                    const auto a = pixels[center_idx + 3];
                    // Strong green accumulation from the point light through MBOIT resolve and blend
                    EXPECT_GT(g, 50);
                    EXPECT_EQ(a, 255);
                }
            }

            dev->destroy_buffer(readback_buf);
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, renderer_automatic_renderable_tracking_lifecycle)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto mesh_id = meshes.register_mesh(create_test_mesh());

        auto mat_opaque = core::material{};
        mat_opaque.set_string(core::material::alpha_mode_name, "OPAQUE");
        auto mat_opaque_id = materials.register_material(tempest::move(mat_opaque));

        auto mat_blend = core::material{};
        mat_blend.set_string(core::material::alpha_mode_name, "BLEND");
        auto mat_blend_id = materials.register_material(tempest::move(mat_blend));

        // 1. Create pre-existing entity before renderer initialization
        auto pre_ent = registry.create();
        registry.assign(pre_ent, core::mesh_component{.mesh_id = mesh_id});
        registry.assign(pre_ent, core::material_component{.material_id = mat_opaque_id});
        registry.assign(pre_ent, ecs::transform_component::identity());

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 1. Verify pre-existing entity is automatically discovered on startup
            EXPECT_EQ(rend->get_tracked_renderable_count(), 1U);

            rend->prepare_frame(1280, 720);
            EXPECT_EQ(rend->get_active_draw_count(), 1U);
            EXPECT_EQ(rend->get_opaque_draw_count(), 1U);
            EXPECT_EQ(rend->get_transparent_draw_count(), 0U);

            // 2. Add second entity dynamically
            auto ent2 = registry.create();
            registry.assign(ent2, core::mesh_component{.mesh_id = mesh_id});
            registry.assign(ent2, core::material_component{.material_id = mat_blend_id});
            registry.assign(ent2, ecs::transform_component::identity());

            EXPECT_EQ(rend->get_tracked_renderable_count(), 2U);

            rend->prepare_frame(1280, 720);
            EXPECT_EQ(rend->get_active_draw_count(), 2U);
            EXPECT_EQ(rend->get_opaque_draw_count(), 1U);
            EXPECT_EQ(rend->get_transparent_draw_count(), 1U);

            // 3. Mutate material on pre_ent from opaque to blend
            registry.replace(pre_ent, core::material_component{.material_id = mat_blend_id});

            rend->prepare_frame(1280, 720);
            EXPECT_EQ(rend->get_active_draw_count(), 2U);
            EXPECT_EQ(rend->get_opaque_draw_count(), 0U);
            EXPECT_EQ(rend->get_transparent_draw_count(), 2U);

            // 4. Remove mesh_component from pre_ent
            registry.remove<core::mesh_component>(pre_ent);
            EXPECT_EQ(rend->get_tracked_renderable_count(), 1U);

            rend->prepare_frame(1280, 720);
            EXPECT_EQ(rend->get_active_draw_count(), 1U);
            EXPECT_EQ(rend->get_transparent_draw_count(), 1U);

            // 5. Destroy ent2
            registry.destroy(ent2);
            EXPECT_EQ(rend->get_tracked_renderable_count(), 0U);

            rend->prepare_frame(1280, 720);
            EXPECT_EQ(rend->get_active_draw_count(), 0U);
        }

        dev->wait_idle();
    }

    TEST(render_system_tests, renderer_automatic_renderable_asset_loading)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto mesh_id = meshes.register_mesh(create_test_mesh());
        auto mat = core::material{};
        mat.set_vec4(core::material::base_color_factor_name, {0.7F, 0.7F, 0.7F, 1.0F});
        auto mat_id = materials.register_material(tempest::move(mat));

        auto ent = registry.create();
        registry.assign(ent, core::mesh_component{.mesh_id = mesh_id});
        registry.assign(ent, core::material_component{.material_id = mat_id});
        registry.assign(ent, ecs::transform_component::identity());

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // Verify assets are NOT loaded in pool before prepare_frame
            EXPECT_FALSE(rend->get_resource_pool().get_mesh_layout(mesh_id).has_value());
            EXPECT_FALSE(rend->get_resource_pool().get_material(mat_id).has_value());

            // Calling prepare_frame should automatically detect and load missing assets
            rend->prepare_frame(1280, 720);

            EXPECT_TRUE(rend->get_resource_pool().get_mesh_layout(mesh_id).has_value());
            EXPECT_TRUE(rend->get_resource_pool().get_material(mat_id).has_value());
            EXPECT_EQ(rend->get_active_draw_count(), 1U);

            auto render_res = rend->render();
            EXPECT_TRUE(render_res.has_value());
        }

        dev->wait_idle();
    }

    // =========================================================================
    // Camera Override & Decoupled Rendering Tests
    // =========================================================================

    /// @brief Verifies that prepare_frame and render execute successfully when an explicit
    /// render_camera override is passed, even with ZERO camera entities in the ECS registry.
    TEST(render_system_tests, renderer_direct_camera_override_execution)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 1. Setup Scene Geometry without ANY Camera Entity in Registry
            auto mesh_id = meshes.register_mesh(create_test_mesh());
            auto mat_id = materials.register_material(core::material{});

            auto geom_ent = registry.create();
            registry.assign(geom_ent, core::mesh_component{.mesh_id = mesh_id});
            registry.assign(geom_ent, core::material_component{.material_id = mat_id});
            registry.assign(geom_ent, ecs::transform_component::identity());

            // 2. Build Direct render_camera Override
            const auto proj = math::perspective(16.0F / 9.0F, math::as_radians(75.0F), 0.05F);
            const auto eye = math::vec3<float>{0.0F, 10.0F, -20.0F};
            const auto view =
                math::look_at(eye, math::vec3<float>{0.0F, 0.0F, 0.0F}, math::vec3<float>{0.0F, 1.0F, 0.0F});
            const auto override_camera = render_camera{
                .proj = proj,
                .inv_proj = math::inverse(proj),
                .view = view,
                .inv_view = math::inverse(view),
                .eye_position = {eye.x, eye.y, eye.z, 1.0F},
            };

            // 3. Prepare Frame with Camera Override
            rend->prepare_frame(1280, 720, nullopt, nullopt, override_camera);

            // 4. Assert GPU Scene Constants Reflect Override Camera Matrices
            const auto slot = rend->get_resource_pool().get_frame_slot();
            const auto* scene = static_cast<const scene_constants*>(
                                    rend->get_resource_pool().get_scene_constants_buffer().cpu_address) +
                                slot;
            ASSERT_NE(scene, nullptr);
            EXPECT_FLOAT_EQ(scene->camera_position.y, 10.0F);
            EXPECT_FLOAT_EQ(scene->camera_position.z, -20.0F);

            // 5. Execute Render Graph
            auto render_res = rend->render();
            EXPECT_TRUE(render_res.has_value());
        }

        dev->wait_idle();
    }

    /// @brief Verifies that directional shadow cascade frustums are correctly calculated
    /// from an explicit render_camera override without ECS camera entities.
    TEST(render_system_tests, renderer_direct_camera_override_shadows)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 1. Setup Sun Light with 4 Shadow Cascades
            auto sun_ent = registry.create();
            registry.assign(sun_ent, directional_light_component{
                                         .color = {1.0F, 1.0F, 1.0F},
                                         .intensity = 2.0F,
                                     });
            registry.assign(sun_ent, shadow_caster_component{
                                         .resolution = 2048,
                                         .num_cascades = 4,
                                         .split_lambda = 0.5F,
                                         .max_shadow_distance = 100.0F,
                                     });
            auto sun_tx = ecs::transform_component::identity();
            sun_tx.rotation({math::as_radians(45.0F), 0.0F, 0.0F});
            registry.assign(sun_ent, sun_tx);

            // 2. Standalone render_camera Override
            const auto proj = math::perspective(16.0F / 9.0F, math::as_radians(60.0F), 0.1F);
            const auto eye = math::vec3<float>{0.0F, 2.0F, -10.0F};
            const auto view =
                math::look_at(eye, math::vec3<float>{0.0F, 0.0F, 0.0F}, math::vec3<float>{0.0F, 1.0F, 0.0F});
            const auto override_camera = render_camera{
                .proj = proj,
                .inv_proj = math::inverse(proj),
                .view = view,
                .inv_view = math::inverse(view),
                .eye_position = {eye.x, eye.y, eye.z, 1.0F},
            };

            // 3. Prepare Frame
            rend->prepare_frame(1280, 720, nullopt, nullopt, override_camera);

            // 4. Assert Shadow Data Contains 4 Computed Cascades
            const auto slot = rend->get_resource_pool().get_frame_slot();
            const auto* shadow_data = static_cast<const directional_shadow_data*>(
                                          rend->get_resource_pool().get_directional_shadow_buffer().cpu_address) +
                                      slot;
            ASSERT_NE(shadow_data, nullptr);
            EXPECT_EQ(shadow_data->cascade_count, 4U);

            // 5. Execute Render
            auto render_res = rend->render();
            EXPECT_TRUE(render_res.has_value());
        }

        dev->wait_idle();
    }

    /// @brief Verifies that calling prepare_frame with NO camera entity in the registry
    /// and NO camera override gracefully renders with fallback identity matrices without errors.
    TEST(render_system_tests, renderer_prepare_frame_without_camera_graceful)
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
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 1. Calling prepare_frame with NO camera entity and NO camera override
            rend->prepare_frame(1280, 720);

            // 2. Assert Render Completes Cleanly
            auto render_res = rend->render();
            EXPECT_TRUE(render_res.has_value());
        }

        dev->wait_idle();
    }

    /// @brief Verifies that when BOTH an ECS camera entity exists AND an explicit camera_override
    /// is provided, the camera_override takes strict precedence on that frame, then reverts to the
    /// ECS camera on subsequent frames when override is nullopt.
    TEST(render_system_tests, renderer_camera_override_precedence_over_ecs)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 1280,
            .render_height = 720,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 1. Add ECS camera entity at Y = 100.0
            auto ecs_cam_ent = registry.create();
            registry.assign(ecs_cam_ent, camera_component{
                                             .aspect_ratio = 16.0F / 9.0F,
                                             .vertical_fov = 1.0F,
                                             .near_plane = 0.1F,
                                         });
            auto ecs_tx = ecs::transform_component::identity();
            ecs_tx.position({0.0F, 100.0F, 0.0F});
            registry.assign(ecs_cam_ent, ecs_tx);

            // 2. Prepare frame with direct camera override at Y = 5.0
            const auto proj = math::perspective(16.0F / 9.0F, 1.0F, 0.1F);
            const auto eye = math::vec3<float>{0.0F, 5.0F, -20.0F};
            const auto view =
                math::look_at(eye, math::vec3<float>{0.0F, 0.0F, 0.0F}, math::vec3<float>{0.0F, 1.0F, 0.0F});
            const auto override_camera = render_camera{
                .proj = proj,
                .inv_proj = math::inverse(proj),
                .view = view,
                .inv_view = math::inverse(view),
                .eye_position = {eye.x, eye.y, eye.z, 1.0F},
            };

            rend->prepare_frame(1280, 720, nullopt, nullopt, override_camera);

            auto slot = rend->get_resource_pool().get_frame_slot();
            auto* scene = static_cast<const scene_constants*>(
                              rend->get_resource_pool().get_scene_constants_buffer().cpu_address) +
                          slot;
            ASSERT_NE(scene, nullptr);
            // Override camera takes precedence!
            EXPECT_FLOAT_EQ(scene->camera_position.y, 5.0F);
            EXPECT_FLOAT_EQ(scene->camera_position.z, -20.0F);

            // 3. Prepare next frame with nullopt override -> falls back to ECS camera
            rend->prepare_frame(1280, 720, nullopt, nullopt, nullopt);

            slot = rend->get_resource_pool().get_frame_slot();
            scene = static_cast<const scene_constants*>(
                        rend->get_resource_pool().get_scene_constants_buffer().cpu_address) +
                    slot;
            ASSERT_NE(scene, nullptr);
            // ECS camera is now active!
            EXPECT_FLOAT_EQ(scene->camera_position.y, 100.0F);
            EXPECT_FLOAT_EQ(scene->camera_position.z, 0.0F);
        }

        dev->wait_idle();
    }

    /// @brief Verifies that a direct camera override with non-default FOV (90 deg), aspect ratio (4:3),
    /// and custom near plane (0.5) correctly extracts frustum geometry and computes valid 4-cascade shadow data.
    TEST(render_system_tests, renderer_direct_camera_override_custom_fov_aspect_near)
    {
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        ASSERT_NE(dev, nullptr);

        auto sink = stdout_log_sink{};
        auto log = logger{sink};

        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 800,
            .render_height = 600,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 1. Setup Sun Light with 4 Shadow Cascades
            auto sun_ent = registry.create();
            registry.assign(sun_ent, directional_light_component{
                                         .color = {1.0F, 1.0F, 1.0F},
                                         .intensity = 2.0F,
                                     });
            registry.assign(sun_ent, shadow_caster_component{
                                         .resolution = 2048,
                                         .num_cascades = 4,
                                         .split_lambda = 0.5F,
                                         .max_shadow_distance = 100.0F,
                                     });
            auto sun_tx = ecs::transform_component::identity();
            sun_tx.rotation({math::as_radians(45.0F), 0.0F, 0.0F});
            registry.assign(sun_ent, sun_tx);

            // 2. Build Camera Override with 4:3 Aspect, 90 deg FOV, 0.5 near plane
            const auto proj = math::perspective(4.0F / 3.0F, math::as_radians(90.0F), 0.5F);
            const auto eye = math::vec3<float>{0.0F, 10.0F, -30.0F};
            const auto view =
                math::look_at(eye, math::vec3<float>{0.0F, 0.0F, 0.0F}, math::vec3<float>{0.0F, 1.0F, 0.0F});
            const auto override_camera = render_camera{
                .proj = proj,
                .inv_proj = math::inverse(proj),
                .view = view,
                .inv_view = math::inverse(view),
                .eye_position = {eye.x, eye.y, eye.z, 1.0F},
            };

            // 3. Prepare Frame
            rend->prepare_frame(800, 600, nullopt, nullopt, override_camera);

            // 4. Assert Shadow Data Contains 4 Cascades Computed with Custom Frustum
            const auto slot = rend->get_resource_pool().get_frame_slot();
            const auto* shadow_data = static_cast<const directional_shadow_data*>(
                                          rend->get_resource_pool().get_directional_shadow_buffer().cpu_address) +
                                      slot;
            ASSERT_NE(shadow_data, nullptr);
            EXPECT_EQ(shadow_data->cascade_count, 4U);

            // 5. Execute Render Graph
            auto render_res = rend->render();
            EXPECT_TRUE(render_res.has_value());
        }

        dev->wait_idle();
    }

    /// @brief Verify renderer pipeline statistics configuration and dynamic runtime mutation.
    TEST(render_system_tests, renderer_pipeline_statistics_configuration_and_mutation)
    {
        // 1. Setup: Create test fixture and registry
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        if (!dev)
        {
            GTEST_SKIP() << "Vulkan device unavailable";
        }

        auto sink = stdout_log_sink{};
        auto log = logger{sink};
        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        const auto stats_mask = rhi::pipeline_statistic_flags::input_assembly_vertices |
                                rhi::pipeline_statistic_flags::fragment_shader_invocations |
                                rhi::pipeline_statistic_flags::compute_shader_invocations;

        // 2. Act & Assert: Verify renderer built with custom statistics mask
        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 800,
            .render_height = 600,
            .pipeline_statistics = stats_mask,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);
            EXPECT_EQ(rend->get_pipeline_statistics(), stats_mask);

            // 3. Act: Dynamically update pipeline statistics
            const auto updated_stats = rhi::pipeline_statistic_flags::vertex_shader_invocations |
                                       rhi::pipeline_statistic_flags::clipping_input_primitives;
            rend->set_pipeline_statistics(updated_stats);

            // 4. Assert: Active statistics updated
            EXPECT_EQ(rend->get_pipeline_statistics(), updated_stats);

            rend->set_pipeline_statistics(rhi::pipeline_statistic_flags::none);
            EXPECT_EQ(rend->get_pipeline_statistics(), rhi::pipeline_statistic_flags::none);
        }

        dev->wait_idle();
    }

    /// @brief Verify renderer executes frame rendering with pipeline statistics enabled and transitions back to
    /// disabled cleanly.
    TEST(render_system_tests, renderer_pipeline_statistics_frame_execution)
    {
        // 1. Setup: Create test fixture and renderer
        auto fixture = create_test_device();
        auto* dev = fixture.dev.get();
        if (!dev)
        {
            GTEST_SKIP() << "Vulkan device unavailable";
        }

        auto sink = stdout_log_sink{};
        auto log = logger{sink};
        auto events = event::event_registry{};
        auto registry = ecs::archetype_registry{events};
        auto meshes = core::mesh_registry{};
        auto materials = core::material_registry{};
        auto textures = core::texture_registry{};

        const auto stats_mask = rhi::pipeline_statistic_flags::input_assembly_vertices |
                                rhi::pipeline_statistic_flags::fragment_shader_invocations |
                                rhi::pipeline_statistic_flags::compute_shader_invocations;

        auto builder = renderer::builder{};
        builder.set_config(renderer_config{
            .render_width = 800,
            .render_height = 600,
            .pipeline_statistics = stats_mask,
        });
        builder.set_inputs(renderer_inputs{
            .entity_registry = &registry,
            .meshes = &meshes,
            .textures = &textures,
            .materials = &materials,
            .asset_db = &fixture.asset_db,
        });

        {
            auto rend = builder.build(*dev, log);
            ASSERT_NE(rend, nullptr);

            // 2. Act: Prepare and render frame with pipeline statistics active
            rend->prepare_frame(800, 600);
            auto render_res = rend->render();

            // 3. Assert: Frame execution succeeded
            EXPECT_TRUE(render_res.has_value());

            // 4. Act: Disable statistics and render subsequent frame
            rend->set_pipeline_statistics(rhi::pipeline_statistic_flags::none);
            rend->prepare_frame(800, 600);
            auto disabled_res = rend->render();

            // 5. Assert: Frame execution succeeded after disabling statistics
            EXPECT_TRUE(disabled_res.has_value());
        }

        dev->wait_idle();
    }
} // namespace tempest::render_system::tests
