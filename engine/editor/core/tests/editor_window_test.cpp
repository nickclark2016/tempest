#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/editor.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/memory.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/ui.hpp>
#include <tempest/vk/context.hpp>
#include <tempest/window_manager.hpp>
#include <tempest/windows/engine_component_view_providers.hpp>
#include <tempest/windows/entity_view_window.hpp>
#include <tempest/windows/scene_hierarchy_window.hpp>
#include <tempest/windows/viewport_window.hpp>

#include <imgui.h>

namespace tempest::editor::tests
{
    namespace
    {
        struct test_env
        {
            unique_ptr<rhi::context> context;
            unique_ptr<rhi::device> dev;
            window_manager win_mgr;
            window_handle win{null_window_handle};
        };

        auto create_test_env() -> test_env
        {
            auto ctx_desc = rhi::context_desc{};
            ctx_desc.application_name = "Tempest Editor Window Test";
            ctx_desc.api = rhi::graphics_api::vulkan;

            auto result = rhi::vk::create_context(ctx_desc);
            if (!result.has_value())
            {
                return {};
            }

            auto context = tempest::move(result).value();
            auto devices = context->enumerate_devices();
            if (devices.empty())
            {
                return {};
            }

            auto dev = context->create_device(devices[0].device_uuid);
            if (!dev)
            {
                return {};
            }

            auto env = test_env{
                .context = tempest::move(context),
                .dev = tempest::move(dev),
                .win_mgr = window_manager{},
            };

            env.win = env.win_mgr.create_window({
                .width = 1280,
                .height = 720,
                .title = "Editor Window Test Window",
                .fullscreen = false,
                .resizable = true,
            });

            return env;
        }
    } // namespace

    TEST(editor_window_test, component_view_providers_names_and_default_creation)
    {
        auto events = event::event_registry{};
        auto reg = ecs::archetype_registry{events};

        auto transform_provider = transform_component_view_provider{};
        EXPECT_EQ(transform_provider.name(), "Transform Component");

        auto camera_provider = camera_component_view_provider{};
        EXPECT_EQ(camera_provider.name(), "Camera Component");

        auto active_cam_provider = active_camera_component_view_provider{};
        EXPECT_EQ(active_cam_provider.name(), "Active Camera Component");

        auto dir_light_provider = directional_light_component_view_provider{};
        EXPECT_EQ(dir_light_provider.name(), "Directional Light Component");

        auto pt_light_provider = point_light_component_view_provider{};
        EXPECT_EQ(pt_light_provider.name(), "Point Light Component");

        auto shadow_provider = shadow_caster_component_view_provider{};
        EXPECT_EQ(shadow_provider.name(), "Shadow Caster Component");

        // Test create_default
        auto ent = reg.create();

        transform_provider.create_default(&reg, ent);
        ASSERT_TRUE(reg.has<ecs::transform_component>(ent));
        const auto& tr = reg.get<ecs::transform_component>(ent);
        EXPECT_EQ(tr.position(), math::float3(0.0F, 0.0F, 0.0F));

        camera_provider.create_default(&reg, ent);
        ASSERT_TRUE(reg.has<render_system::camera_component>(ent));
        const auto& cam = reg.get<render_system::camera_component>(ent);
        EXPECT_NEAR(cam.aspect_ratio, 16.0F / 9.0F, 0.001F);
        EXPECT_NEAR(cam.vertical_fov, math::as_radians(60.0F), 0.001F);

        active_cam_provider.create_default(&reg, ent);
        EXPECT_TRUE(reg.has<render_system::active_camera_component>(ent));

        auto ent2 = reg.create();
        dir_light_provider.create_default(&reg, ent2);
        ASSERT_TRUE(reg.has<render_system::directional_light_component>(ent2));
        const auto& dl = reg.get<render_system::directional_light_component>(ent2);
        EXPECT_EQ(dl.intensity, 1.0F);

        auto ent3 = reg.create();
        pt_light_provider.create_default(&reg, ent3);
        ASSERT_TRUE(reg.has<render_system::point_light_component>(ent3));
        const auto& pl = reg.get<render_system::point_light_component>(ent3);
        EXPECT_EQ(pl.intensity, 10.0F);
        EXPECT_EQ(pl.range, 10.0F);

        auto ent4 = reg.create();
        shadow_provider.create_default(&reg, ent4);
        ASSERT_TRUE(reg.has<render_system::shadow_caster_component>(ent4));
        const auto& sc = reg.get<render_system::shadow_caster_component>(ent4);
        EXPECT_EQ(sc.resolution, 2048u);
        EXPECT_EQ(sc.num_cascades, 4u);
    }

    TEST(editor_window_test, component_view_providers_draw_execution)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            auto events = event::event_registry{};
            auto reg = ecs::archetype_registry{events};

            auto ent = reg.create();
            reg.assign(ent, ecs::transform_component::identity());
            reg.assign(ent, render_system::camera_component{
                                .aspect_ratio = 16.0F / 9.0F,
                                .vertical_fov = math::as_radians(60.0F),
                                .near_plane = 0.1F,
                            });
            reg.assign(ent, render_system::active_camera_component{});
            reg.assign(ent, render_system::directional_light_component{
                                .color = math::float3(1.0F, 1.0F, 1.0F),
                                .intensity = 2.0F,
                            });
            reg.assign(ent, render_system::point_light_component{
                                .color = math::float3(1.0F, 0.5F, 0.0F),
                                .intensity = 5.0F,
                                .range = 20.0F,
                            });
            reg.assign(ent, render_system::shadow_caster_component{
                                .resolution = 1024,
                                .num_cascades = 3,
                                .split_lambda = 0.7F,
                                .max_shadow_distance = 300.0F,
                                .normal_bias = 0.01F,
                                .depth_bias = 0.002F,
                                .priority = 0,
                                .debug_mode = render_system::shadow_debug_mode::cascades,
                            });

            auto transform_provider = transform_component_view_provider{};
            auto camera_provider = camera_component_view_provider{};
            auto active_cam_provider = active_camera_component_view_provider{};
            auto dir_light_provider = directional_light_component_view_provider{};
            auto pt_light_provider = point_light_component_view_provider{};
            auto shadow_provider = shadow_caster_component_view_provider{};

            ui_ctx.begin_ui_commands();
            if (ImGui::Begin("Inspector Window"))
            {
                EXPECT_NO_THROW({
                    transform_provider.draw(&reg, ent);
                    camera_provider.draw(&reg, ent);
                    active_cam_provider.draw(&reg, ent);
                    dir_light_provider.draw(&reg, ent);
                    pt_light_provider.draw(&reg, ent);
                    shadow_provider.draw(&reg, ent);
                });
            }
            ImGui::End();
            ui_ctx.finish_ui_commands();
        }

        env.win_mgr.destroy_window(env.win);
    }

    TEST(editor_window_test, scene_hierarchy_window_properties_and_draw)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            auto events = event::event_registry{};
            auto reg = ecs::archetype_registry{events};

            auto root1 = reg.create();
            reg.name(root1, "Root Entity 1");

            auto root2 = reg.create();
            reg.name(root2, "Root Entity 2");

            auto child1 = reg.create();
            reg.name(child1, "Child Entity 1");

            auto child2 = reg.create();
            reg.name(child2, "Child Entity 2");

            // Link child1 and child2 to root1
            reg.assign(root1, ecs::relationship_component<ecs::entity>{
                                  .parent = ecs::tombstone,
                                  .next_sibling = ecs::tombstone,
                                  .first_child = child1,
                              });
            reg.assign(child1, ecs::relationship_component<ecs::entity>{
                                   .parent = root1,
                                   .next_sibling = child2,
                                   .first_child = ecs::tombstone,
                               });
            reg.assign(child2, ecs::relationship_component<ecs::entity>{
                                   .parent = root1,
                                   .next_sibling = ecs::tombstone,
                                   .first_child = ecs::tombstone,
                               });

            // Add a prefab entity which should be filtered out
            auto prefab_ent = reg.create();
            reg.name(prefab_ent, "Prefab Template");
            reg.assign(prefab_ent, assets::prefab_tag_t{});

            auto hierarchy = scene_hierarchy_window{reg};
            EXPECT_EQ(hierarchy.desired_initial_dock(), editor_window::dock_location::left);
            EXPECT_EQ(hierarchy.window_name(), "Scene Hierarchy");
            EXPECT_TRUE(hierarchy.selected_entity == ecs::null);

            ui_ctx.begin_ui_commands();
            EXPECT_NO_THROW(hierarchy.draw());
            ui_ctx.finish_ui_commands();
        }

        env.win_mgr.destroy_window(env.win);
    }

    TEST(editor_window_test, entity_view_window_draw)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            auto events = event::event_registry{};
            auto reg = ecs::archetype_registry{events};

            auto ent = reg.create();
            reg.name(ent, "Test Entity");
            reg.assign(ent, ecs::transform_component::identity());

            auto view_window = entity_view_window{reg};
            view_window.target = ent;
            view_window.providers.push_back(make_unique<transform_component_view_provider>());
            view_window.providers.push_back(make_unique<camera_component_view_provider>());

            EXPECT_EQ(view_window.desired_initial_dock(), editor_window::dock_location::right);
            EXPECT_EQ(view_window.window_name(), "Entity Editor");

            ui_ctx.begin_ui_commands();
            EXPECT_NO_THROW(view_window.draw());
            ui_ctx.finish_ui_commands();
        }

        env.win_mgr.destroy_window(env.win);
    }

    TEST(editor_window_test, viewport_window_initial_state_and_aspect_ratio)
    {
        auto engine_ctx = editor_engine_context{};
        auto viewport = viewport_window{engine_ctx};

        EXPECT_EQ(viewport.desired_initial_dock(), editor_window::dock_location::center);
        EXPECT_EQ(viewport.window_name(), "Viewport");
        EXPECT_TRUE(viewport.is_mode_supported(simulation_state::stopped));
        EXPECT_TRUE(viewport.is_mode_supported(simulation_state::pause));
        EXPECT_TRUE(viewport.is_mode_supported(simulation_state::play));

        // Default aspect ratio with zero size returns 1.0f
        EXPECT_FLOAT_EQ(viewport.aspect_ratio(), 1.0F);

        // Simulation state switching
        EXPECT_EQ(engine_ctx.get_simulation_state(), simulation_state::stopped);
        engine_ctx.set_simulation_state(simulation_state::play);
        EXPECT_EQ(engine_ctx.get_simulation_state(), simulation_state::play);
        engine_ctx.set_simulation_state(simulation_state::pause);
        EXPECT_EQ(engine_ctx.get_simulation_state(), simulation_state::pause);
        engine_ctx.set_simulation_state(simulation_state::stopped);
        EXPECT_EQ(engine_ctx.get_simulation_state(), simulation_state::stopped);
    }

    TEST(editor_window_test, editor_context_lifecycle_and_camera_sync)
    {
        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Editor Context Test",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 rhi::data_format::rgba8_unorm, 2);

        auto ed_ctx = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        auto& cam_sys = engine_ctx.get_renderer().get_camera_system();
        auto active_cam = cam_sys.get_active_camera_entity();
        ASSERT_TRUE(active_cam.has_value());

        const auto* cam = engine_ctx.get_entities().try_get<render_system::camera_component>(active_cam.value());
        ASSERT_NE(cam, nullptr);
        EXPECT_GT(cam->aspect_ratio, 0.0F);

        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(ed_ctx.draw());
        ui_ctx.finish_ui_commands();
    }
} // namespace tempest::editor::tests
