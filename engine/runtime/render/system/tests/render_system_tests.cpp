#include <gtest/gtest.h>

#include <filesystem>
#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/default_importers.hpp>
#include <tempest/logger.hpp>
#include <tempest/render_system/camera_system.hpp>
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
                .color = {1.0F, 0.95F, 0.85F},
                .intensity = 3.0F,
            });
            registry.assign(sun, ecs::transform_component::identity());

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

            auto res = rend->render();
            EXPECT_TRUE(res.has_value());

            dev->wait_idle();
        }
    }
} // namespace tempest::render_system::tests
