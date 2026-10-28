#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/default_importers.hpp>
#include <tempest/editor.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/engine_types.hpp>
#include <tempest/logger.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/memory.hpp>
#include <tempest/command_history.hpp>
#include <tempest/commands/entity_commands.hpp>
#include <tempest/material.hpp>
#include <tempest/payloads/asset_payload.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/physics/box_collider_component.hpp>
#include <tempest/physics/physics_scene_system.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/physics/rigid_body_component.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/shared_library.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/ui.hpp>
#include <tempest/vk/context.hpp>
#include <tempest/window_manager.hpp>
#include <tempest/windows/hierarchy_window.hpp>
#include <tempest/windows/inspector_window.hpp>
#include <tempest/windows/viewport_window.hpp>

#include <imgui.h>
#include <imgui_internal.h>

namespace tempest::editor::tests
{
    namespace
    {
        struct test_env
        {
            unique_ptr<rhi::context> context;
            unique_ptr<rhi::device> dev;
            assets::asset_database asset_db{};
            window_manager win_mgr;
            window_handle win{null_window_handle};
        };

        auto create_test_env() -> test_env
        {
            static auto test_sink = stdout_log_sink{};
            static auto test_log = logger{test_sink};

            auto ctx_desc = rhi::context_desc{};
            ctx_desc.application_name = "Tempest Editor Window Test";
            ctx_desc.api = rhi::graphics_api::vulkan;

            auto result = rhi::vk::create_context(ctx_desc, test_log);
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

            auto asset_db = assets::asset_database{};
            assets::mount_default_shader_roots(asset_db);
            asset_db.scan_and_index();

            auto env = test_env{
                .context = tempest::move(context),
                .dev = tempest::move(dev),
                .asset_db = tempest::move(asset_db),
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

    /// @brief Verifies inspector_window dock defaults, window name, and drawing execution
    /// with all reflected engine components present on the target entity.
    TEST(editor_window_test, inspector_window_properties_and_draw)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            auto events = event::event_registry{};
            auto types = ecs::component_type_registry{};
            auto reg = ecs::archetype_registry{events, types};
            tempest::register_engine_types(types);

            auto ent = reg.create();
            reg.name(ent, "Test Reflected Entity");
            reg.assign(ent, ecs::transform_component{});
            reg.assign(ent, render_system::camera_component{
                                .aspect_ratio = 16.0F / 9.0F,
                                .vertical_fov = math::as_radians(60.0F),
                                .near_plane = 0.1F,
                                .far_plane = 1000.0F,
                            });
            reg.assign(ent, render_system::directional_light_component{
                                .color = math::float3{1.0F, 1.0F, 1.0F},
                                .intensity = 2.0F,
                            });
            reg.assign(ent, render_system::point_light_component{
                                .color = math::float3{1.0F, 0.5F, 0.0F},
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

            auto scene_mgr = scene::scene_manager{reg};
            auto selection = editor_selection{};
            auto inspector = inspector_window{reg, &scene_mgr, &selection};

            EXPECT_EQ(inspector.desired_initial_dock(), editor_window::dock_location::right);
            EXPECT_EQ(inspector.window_name(), "Inspector");

            // Direct target
            inspector.target = ent;

            ui_ctx.begin_ui_commands();
            EXPECT_NO_THROW(inspector.draw());
            ui_ctx.finish_ui_commands();
        }

        env.win_mgr.destroy_window(env.win);
    }

    /// @brief Verifies that inspector_window safely draws placeholder text when no entity is selected.
    TEST(editor_window_test, inspector_window_no_entity_selected)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            auto events = event::event_registry{};
            auto types = ecs::component_type_registry{};
            auto reg = ecs::archetype_registry{events, types};
            auto scene_mgr = scene::scene_manager{reg};
            auto selection = editor_selection{};

            auto inspector = inspector_window{reg, &scene_mgr, &selection};
            inspector.target = ecs::null;

            ui_ctx.begin_ui_commands();
            EXPECT_NO_THROW(inspector.draw());
            ui_ctx.finish_ui_commands();
        }

        env.win_mgr.destroy_window(env.win);
    }

    /// @brief Verifies hierarchy_window docking defaults, name, and clean draw execution
    /// with scenes, dynamic entities, and editor_selection attached.
    TEST(editor_window_test, hierarchy_window_properties_and_draw)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            auto events = event::event_registry{};
            auto types = ecs::component_type_registry{};
            auto reg = ecs::archetype_registry{events, types};
            auto scene_mgr = scene::scene_manager{reg};
            auto selection = editor_selection{};
            selection.attach_event_registry(events);

            const auto scene_opt = scene_mgr.load_from_string_sync(
                R"({"schema_version":1,"generator":"test","entities":[]})", "Test Scene");
            ASSERT_TRUE(scene_opt.has_value());
            const auto scene = *scene_opt;

            const auto root1 = scene_mgr.create_entity(scene, "Root Entity 1");
            const auto child1 = scene_mgr.create_entity(scene, "Child Entity 1");
            reg.reparent(child1, root1);

            const auto dyn1 = reg.create();
            reg.name(dyn1, "Dynamic Entity 1");

            auto log = logger{};
            auto hierarchy = hierarchy_window{reg, scene_mgr, selection, log};
            EXPECT_EQ(hierarchy.desired_initial_dock(), editor_window::dock_location::left);
            EXPECT_EQ(hierarchy.window_name(), "Hierarchy");
            EXPECT_TRUE(hierarchy.get_selected_entity() == ecs::null);

            ui_ctx.begin_ui_commands();
            EXPECT_NO_THROW(hierarchy.draw());
            ui_ctx.finish_ui_commands();
        }

        env.win_mgr.destroy_window(env.win);
    }

    /// @brief Verifies viewport window initial dock target, mode support across simulation
    /// states, and safe aspect ratio computation with zero dimensions.
    TEST(editor_window_test, viewport_window_initial_state_and_aspect_ratio)
    {
        auto engine_ctx = editor_engine_context{};
        auto viewport = viewport_window{engine_ctx};

        // Initial State & Docking
        EXPECT_EQ(viewport.desired_initial_dock(), editor_window::dock_location::center);
        EXPECT_EQ(viewport.window_name(), "Viewport");
        EXPECT_TRUE(viewport.is_mode_supported(simulation_state::edit));
        EXPECT_TRUE(viewport.is_mode_supported(simulation_state::paused));
        EXPECT_TRUE(viewport.is_mode_supported(simulation_state::playing));

        // Default Aspect Ratio (Zero-size safe fallback to 1.0)
        EXPECT_FLOAT_EQ(viewport.aspect_ratio(), 1.0F);

        // Simulation State Switching
        EXPECT_EQ(engine_ctx.get_simulation_state(), simulation_state::edit);
        engine_ctx.set_simulation_state(simulation_state::playing);
        EXPECT_EQ(engine_ctx.get_simulation_state(), simulation_state::playing);
        engine_ctx.set_simulation_state(simulation_state::paused);
        EXPECT_EQ(engine_ctx.get_simulation_state(), simulation_state::paused);
        engine_ctx.set_simulation_state(simulation_state::edit);
        EXPECT_EQ(engine_ctx.get_simulation_state(), simulation_state::edit);
    }

    /// @brief Verifies that editor context initialization does NOT insert any phantom
    /// "Editor Camera" entities into the ECS scene hierarchy registry.
    TEST(editor_window_test, scene_hierarchy_no_editor_camera)
    {
        // Setup Engine Context & Register Window
        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Scene Hierarchy Test",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        // Initialize Editor Context
        auto ed_ctx = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Assert Zero Camera Entities in ECS Registry
        auto& cam_sys = engine_ctx.get_renderer().get_camera_system();
        auto active_cam = cam_sys.get_active_camera_entity();
        EXPECT_FALSE(active_cam.has_value());

        // Assert Standalone Editor Camera is Initialized Directly on Engine Context
        auto& editor_cam = engine_ctx.get_editor_camera();
        EXPECT_EQ(editor_cam.get_position(), math::vec3<float>(0.0F, 2.0F, -5.0F));
    }

    /// @brief Verifies that user-created game cameras are automatically discovered and
    /// synchronized with the viewport without creating dummy editor cameras.
    TEST(editor_window_test, editor_context_lifecycle_and_camera_sync)
    {
        // Setup Engine & Editor Context
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
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        auto ed_ctx = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        // Add Game Camera Entity
        auto game_cam = engine_ctx.get_entities().create();
        engine_ctx.get_entities().name(game_cam, "Player Camera");
        engine_ctx.get_entities().assign(game_cam, ecs::transform_component{});
        engine_ctx.get_entities().assign(game_cam, render_system::camera_component{
                                                       .aspect_ratio = 1.0F,
                                                       .vertical_fov = math::as_radians(60.0F),
                                                       .near_plane = 0.1F,
                                                   });

        // Assert Camera System Resolves the Game Camera
        auto& cam_sys = engine_ctx.get_renderer().get_camera_system();
        auto active_cam = cam_sys.get_active_camera_entity();
        ASSERT_TRUE(active_cam.has_value());
        EXPECT_EQ(active_cam.value(), game_cam);

        // Draw Frame & Assert Clean Execution
        ui_ctx.begin_ui_commands();
        EXPECT_NO_THROW(ed_ctx.draw());
        ui_ctx.finish_ui_commands();
    }

    /// @brief Verifies that destroying an editor_context cleans up registered callbacks on
    /// the editor_engine_context so no dangling pointers are retained.
    TEST(editor_window_test, editor_context_destructor_cleans_up_callbacks)
    {
        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Callback Cleanup Test",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        // Create editor_context in scoped block
        {
            auto ed_ctx = editor_context(engine_ctx, reg_info.handle, ui_ctx);
            ui_ctx.begin_ui_commands();
            EXPECT_NO_THROW(ed_ctx.draw());
            ui_ctx.finish_ui_commands();
        }

        // editor_context destroyed; clearing callbacks ensures no dangling pointer access
        EXPECT_NO_THROW(engine_ctx.clear_editor_callbacks());
    }

    /// @brief Verifies that editor_context exposes editor_selection service and connects it
    /// to the engine context's event registry for automatic entity destruction eviction.
    TEST(editor_window_test, editor_context_selection_service_integration)
    {
        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Editor Selection Test",
        };

        const auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto ui_ctx = ui_context(engine_ctx.get_window_manager(), reg_info.handle, engine_ctx.get_device(),
                                 engine_ctx.get_assets(), rhi::data_format::rgba8_unorm, 2);

        auto ed_ctx = editor_context(engine_ctx, reg_info.handle, ui_ctx);

        auto& sel = ed_ctx.get_selection();
        const auto& const_ed_ctx = ed_ctx;
        const auto& const_sel = const_ed_ctx.get_selection();
        EXPECT_TRUE(sel.is_empty());
        EXPECT_TRUE(const_sel.is_empty());

        auto& entities = engine_ctx.get_entities();
        const auto ent = entities.create();
        const auto addr = compute_entity_address(entities, ent);

        sel.set_selection(addr, ent);
        EXPECT_FALSE(sel.is_empty());
        EXPECT_TRUE(sel.is_selected(ent));
        EXPECT_EQ(sel.get_primary_entity(), ent);

        // Destroy entity in ECS registry - selection should automatically evict via attached event registry
        entities.destroy(ent);
        EXPECT_FALSE(sel.is_selected(ent));
        EXPECT_EQ(sel.get_primary_entity(), static_cast<ecs::entity>(ecs::tombstone));
    }

    /// @brief Verifies that renderer.resize() properly sets configured dimensions
    /// used for viewport offscreen rendering.
    TEST(editor_window_test, renderer_viewport_resize_sync)
    {
        auto engine_ctx = editor_engine_context{};
        auto& rend = engine_ctx.get_renderer();

        rend.resize(800, 600);
        EXPECT_EQ(rend.get_config().render_width, 800U);
        EXPECT_EQ(rend.get_config().render_height, 600U);
    }

    /// @brief Verifies that in play mode, fixed update callbacks update transform_history_component
    /// and that transform_component is properly synchronized via step and interpolate history.
    TEST(editor_window_test, play_mode_steps_and_interpolates_transform_history)
    {
        // Setup engine context and entity with transform and transform_history
        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Play Mode Transform History Test",
            .fullscreen = false,
            .resizable = true,
        };

        auto reg_info = engine_ctx.register_window(desc);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto& reg = engine_ctx.get_entities();
        auto ent = reg.create();

        const auto initial_pos = math::vec3<float>{1.0F, 2.0F, 3.0F};
        auto hist = ecs::transform_history_component::create(initial_pos);
        auto tx = ecs::transform_component{};
        tx.position(initial_pos);

        reg.assign(ent, hist);
        reg.assign(ent, tx);

        engine_ctx.set_simulation_state(simulation_state::playing);

        // Act: register fixed update that moves the entity, and request close after ticks run
        auto fixed_updated = false;
        engine_ctx.register_on_fixed_update_callback([&](engine_context& ctx, chrono::duration<double> dt) {
            fixed_updated = true;
            auto& entities = ctx.get_entities();
            entities.each([delta = static_cast<float>(dt.count())](ecs::transform_history_component& h) {
                h.current_position.x += 10.0F * delta;
            });
        });

        auto frame_count = 0;
        engine_ctx.register_on_editor_update_callback([&](engine_context& ctx) {
            ++frame_count;
            if (frame_count >= 3 && fixed_updated)
            {
                ctx.request_close();
            }
        });

        engine_ctx.run();

        // Assert: transform_component was updated by interpolate_transform_history
        ASSERT_TRUE(fixed_updated);
        const auto* updated_tx = reg.try_get<ecs::transform_component>(ent);
        ASSERT_NE(updated_tx, nullptr);
        EXPECT_GT(updated_tx->position().x, initial_pos.x);
    }

    /// @brief Verifies that editor_engine_context defaults to normal cursor mode,
    /// captures cursor exclusively in play mode, and restores normal cursor on pause/stop.
    TEST(editor_window_test, cursor_mode_and_play_mode_transitions)
    {
        // Setup: Create editor_engine_context and register a window
        auto engine_ctx = editor_engine_context{};
        const auto desc = window_desc{
            .width = 1280,
            .height = 720,
            .title = "Cursor Transition Test Window",
            .fullscreen = false,
            .resizable = true,
        };

        const auto reg_info = engine_ctx.register_window(desc, false);
        ASSERT_TRUE(reg_info.handle.is_valid());

        auto& wm = engine_ctx.get_window_manager();

        // Assert initial state: edit mode, not playing, cursor normal
        EXPECT_EQ(engine_ctx.get_simulation_state(), simulation_state::edit);
        EXPECT_FALSE(engine_ctx.is_play_mode());
        EXPECT_EQ(wm.get_cursor_mode(reg_info.handle), cursor_mode::normal);
        EXPECT_FALSE(wm.is_cursor_disabled(reg_info.handle));

        // Act & Assert: Transition to playing mode captures cursor
        engine_ctx.set_simulation_state(simulation_state::playing);
        EXPECT_TRUE(engine_ctx.is_play_mode());
        EXPECT_EQ(wm.get_cursor_mode(reg_info.handle), cursor_mode::disabled);
        EXPECT_TRUE(wm.is_cursor_disabled(reg_info.handle));

        // Act & Assert: Transition to paused mode uncaptures cursor
        engine_ctx.set_simulation_state(simulation_state::paused);
        EXPECT_FALSE(engine_ctx.is_play_mode());
        EXPECT_EQ(wm.get_cursor_mode(reg_info.handle), cursor_mode::normal);
        EXPECT_FALSE(wm.is_cursor_disabled(reg_info.handle));

        // Act & Assert: Transition to edit mode retains normal cursor
        engine_ctx.set_simulation_state(simulation_state::edit);
        EXPECT_FALSE(engine_ctx.is_play_mode());
        EXPECT_EQ(wm.get_cursor_mode(reg_info.handle), cursor_mode::normal);
        EXPECT_FALSE(wm.is_cursor_disabled(reg_info.handle));
    }

    /// @brief Verifies that removing box_collider_component via remove_component_command from an entity
    /// with rigid_body_component and box_collider_component in the presence of physics_scene_system
    /// does not crash during command execution or subsequent inspector window draw calls.
    TEST(editor_window_test, inspector_remove_box_collider_with_physics_scene_no_crash)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
            ImGui::SetCurrentContext(ui_ctx.get_imgui_context());

            auto events = event::event_registry{};
            auto types = ecs::component_type_registry{};
            tempest::register_engine_types(types);
            auto reg = ecs::archetype_registry{events, types};
            auto scene_mgr = scene::scene_manager{reg};
            auto selection = editor_selection{};
            auto history = command_history{};
            auto inspector = inspector_window{reg, &scene_mgr, &selection, &history};
            inspector.open = true;

            auto physics = physics::physics_world{};
            auto physics_scene = physics::physics_scene_system{physics, reg, events};

            // Create Floor entity matching sandbox setup
            const auto floor = reg.create();
            reg.assign(floor, ecs::transform_component{});
            reg.assign(floor, core::mesh_component{});
            reg.assign(floor, core::material_component{});
            reg.assign(floor, physics::rigid_body_component{
                .motion = jolt::shim::motion_type::static_motion,
                .layer = jolt::shim::object_layer::non_moving,
            });
            reg.assign(floor, physics::box_collider_component{
                .half_extents = {20.0F, 0.5F, 20.0F},
                .offset = {0.0F, -0.5F, 0.0F},
            });
            reg.name(floor, "FloorPlane");

            // Create Step obstacle entity matching sandbox setup
            const auto step = reg.create();
            reg.assign(step, ecs::transform_component{});
            reg.assign(step, core::mesh_component{});
            reg.assign(step, core::material_component{});
            reg.assign(step, physics::rigid_body_component{
                .motion = jolt::shim::motion_type::static_motion,
                .layer = jolt::shim::object_layer::non_moving,
            });
            reg.assign(step, physics::box_collider_component{
                .half_extents = {1.0F, 0.1F, 1.0F},
                .offset = {0.0F, 0.0F, 0.0F},
            });
            reg.name(step, "StepObstacle");

            selection.set_selection(floor, reg, &scene_mgr);

            // Act: Draw initial frame
            ui_ctx.begin_ui_commands();
            inspector.draw();
            ui_ctx.finish_ui_commands();

            // Act: Execute remove_component_command for box_collider_component
            const auto addr = compute_entity_address(reg, floor);
            const auto box_type_idx = types.type_index<physics::box_collider_component>();
            ASSERT_TRUE(box_type_idx.has_value());

            auto cmd_ctx = command_context{
                .registry = reg,
                .scene_manager = scene_mgr,
                .events = &events,
            };
            history.execute(make_unique<remove_component_command>(addr, *box_type_idx), cmd_ctx);

            // Act: Draw second frame with box_collider removed
            ui_ctx.begin_ui_commands();
            inspector.draw();
            ui_ctx.finish_ui_commands();

            // Assert: Floor no longer has box_collider, still has rigid_body
            EXPECT_FALSE(reg.has<physics::box_collider_component>(floor));
            EXPECT_TRUE(reg.has<physics::rigid_body_component>(floor));
        }

        env.win_mgr.destroy_window(env.win);
    }

    /// @brief Verifies that removing box_collider_component from FloorPlane in a fully loaded game scene
    /// inside editor_engine_context and editor_context does not crash.
    TEST(editor_window_test, inspector_remove_box_collider_full_editor_scene_reproduce)
    {
#if defined(TEMPEST_PLATFORM_WINDOWS)
        constexpr auto game_library_name = L"game-runtime.dll";
#elif defined(TEMPEST_PLATFORM_LINUX)
        constexpr auto game_library_name = "libgame-runtime.so";
#endif
        auto game_lib_res = shared_library::load(game_library_name);
        ASSERT_TRUE(game_lib_res.has_value());
        const auto& game_lib = *game_lib_res;

        const auto game_on_load =
            game_lib.get_function_handle<void, engine_context*, span<string_view>>("on_load");
        ASSERT_TRUE(game_on_load.has_value());
        const auto game_on_unload = game_lib.get_function_handle<void>("on_unload");
        ASSERT_TRUE(game_on_unload.has_value());

        {
            auto engine = editor_engine_context{};
            const auto win_info = engine.register_window(
                {
                    .width = 1280,
                    .height = 720,
                    .title = "Test Window",
                    .fullscreen = false,
                    .resizable = true,
                },
                false);
            ASSERT_TRUE(win_info.handle.is_valid());

            auto* const surface = engine.get_render_surface(win_info.handle);
            const auto fmt = surface ? rhi::to_data_format(surface->get_format()) : rhi::data_format::rgba8_unorm;

            auto ui_ctx = ui_context(engine.get_window_manager(), win_info.handle, engine.get_device(),
                                     engine.get_assets(), fmt, 2);
            auto ui_editor = editor_context(engine, win_info.handle, ui_ctx);

            auto empty_args = vector<string_view>{};
            (*game_on_load)(&engine, empty_args);
            engine.load_startup_scene_if_needed();

            auto frame_count = 0;
            engine.register_on_editor_update_callback([&](engine_context& ctx) {
                ++frame_count;
                auto& reg = ctx.get_entities();
                const auto floor = reg.find_first_with_name("FloorPlane");
                if (frame_count == 1)
                {
                    ASSERT_TRUE(floor.has_value());
                    engine.set_simulation_state(simulation_state::playing);
                    ui_editor.get_selection().set_selection(*floor, reg, &engine.get_scene_manager());
                }
                else if (frame_count == 2)
                {
                    ASSERT_TRUE(floor.has_value());
                    const auto addr = compute_entity_address(reg, *floor);
                    const auto box_type_idx = reg.type_registry().type_index<physics::box_collider_component>();
                    ASSERT_TRUE(box_type_idx.has_value());

                    auto cmd_ctx = command_context{
                        .registry = reg,
                        .scene_manager = engine.get_scene_manager(),
                        .events = &ctx.get_events(),
                    };
                    ui_editor.get_command_history().execute(
                        make_unique<remove_component_command>(addr, *box_type_idx), cmd_ctx);
                }

                ui_ctx.begin_ui_commands();
                ui_editor.draw();
                ui_ctx.finish_ui_commands();

                if (frame_count >= 30)
                {
                    ctx.request_close();
                }
            });

            engine.run();
        }

        (*game_on_unload)();
    }


    /// @brief Verifies that hierarchy_window avoids ImGui ID conflicts between scene headers
    /// and entity nodes that share the same numerical index (e.g. scene 0 and entity 0).
    TEST(editor_window_test, hierarchy_window_no_id_conflict_between_scene_and_camera_entity)
    {
        auto env = create_test_env();
        ASSERT_NE(env.dev, nullptr);
        ASSERT_TRUE(env.win.is_valid());

        {
            auto ui_ctx = ui_context(env.win_mgr, env.win, *env.dev, env.asset_db, rhi::data_format::rgba8_unorm, 2);
            auto* const imgui_ctx = ui_ctx.get_imgui_context();
            ImGui::SetCurrentContext(imgui_ctx);
            imgui_ctx->IO.ConfigDebugHighlightIdConflicts = true;

            auto events = event::event_registry{};
            auto types = ecs::component_type_registry{};
            auto reg = ecs::archetype_registry{events, types};
            auto scene_mgr = scene::scene_manager{reg};
            auto selection = editor_selection{};
            selection.attach_event_registry(events);

            const auto scene_opt = scene_mgr.load_from_string_sync(
                R"({"schema_version":1,"generator":"test","entities":[]})", "main.tscene");
            ASSERT_TRUE(scene_opt.has_value());
            const auto scene = *scene_opt;
            EXPECT_EQ(scene.index, 0U);

            const auto camera_ent = scene_mgr.create_entity(scene, "Camera");
            EXPECT_EQ(static_cast<uint32_t>(camera_ent), 0U);

            auto log = logger{};
            auto hierarchy = hierarchy_window{reg, scene_mgr, selection, log};

            // First verify that the ImGui IDs computed for the scene header and the camera entity differ inside "Hierarchy"
            ui_ctx.begin_ui_commands();
            ImGui::Begin("Hierarchy");
            ImGui::PushID("scene");
            ImGui::PushID(static_cast<int>(scene.index));
            ImGui::PushID(static_cast<int>(scene.generation));
            const auto scene_node_id = ImGui::GetID("##node");
            ImGui::PopID();
            ImGui::PopID();
            ImGui::PopID();

            ImGui::PushID("ent");
            ImGui::PushID(static_cast<int>(static_cast<uint64_t>(camera_ent) & 0xFFFFFFFFU));
            ImGui::PushID(static_cast<int>(static_cast<uint64_t>(camera_ent) >> 32U));
            const auto camera_node_id = ImGui::GetID("##node");
            ImGui::PopID();
            ImGui::PopID();
            ImGui::PopID();
            ImGui::End();

            EXPECT_NE(scene_node_id, camera_node_id);

            // Simulate hovering over camera_node_id in previous frame
            imgui_ctx->HoveredIdPreviousFrame = camera_node_id;

            hierarchy.draw();
            ui_ctx.finish_ui_commands();

            // Exactly 1 item must match the hovered ID, proving zero duplicate ID conflicts
            EXPECT_EQ(imgui_ctx->HoveredIdPreviousFrameItemCount, 1);
            EXPECT_EQ(imgui_ctx->DebugDrawIdConflicts, 0U);
        }

        env.win_mgr.destroy_window(env.win);
    }

    //==============================================================================
    // Viewport Window Drop Target & Raycast Tests
    //==============================================================================

    /// @brief Verifies camera ray unprojection and normalization across viewport center,
    /// corners, and safe fallback for non-positive viewport sizes.
    TEST(editor_window_test, viewport_camera_ray_calculation)
    {
        // Setup: Engine context in headless mode with editor camera at known position and orientation
        auto engine_ctx = editor_engine_context{};
        auto& cam = engine_ctx.get_editor_camera();
        cam.set_position(math::vec3<float>{0.0F, 2.0F, -5.0F});
        cam.set_rotation(0.0F, 0.0F);
        cam.set_vertical_fov(math::as_radians(60.0F));
        cam.set_aspect_ratio(800.0F / 600.0F);

        const auto viewport = viewport_window{engine_ctx};
        const auto vp_size = math::vec2<float>{800.0F, 600.0F};

        // Act: Compute ray at viewport center
        const auto center_mouse = math::vec2<float>{400.0F, 300.0F};
        const auto ray_center = viewport.compute_camera_ray(center_mouse, vp_size);

        // Assert: Ray origin matches camera position and ray direction points straight forward
        constexpr auto float_epsilon = 1e-3F;
        EXPECT_NEAR(ray_center.origin.x, 0.0F, float_epsilon);
        EXPECT_NEAR(ray_center.origin.y, 2.0F, float_epsilon);
        EXPECT_NEAR(ray_center.origin.z, -5.0F, float_epsilon);
        EXPECT_NEAR(ray_center.direction.x, 0.0F, float_epsilon);
        EXPECT_NEAR(ray_center.direction.y, 0.0F, float_epsilon);
        EXPECT_NEAR(ray_center.direction.z, 1.0F, float_epsilon);
        EXPECT_NEAR(math::length(ray_center.direction), 1.0F, float_epsilon);

        // Act: Compute ray at top-left corner
        const auto top_left_mouse = math::vec2<float>{0.0F, 0.0F};
        const auto ray_top_left = viewport.compute_camera_ray(top_left_mouse, vp_size);

        // Assert: Top-left ray points left, up, and forward with unit length
        EXPECT_LT(ray_top_left.direction.x, -0.1F);
        EXPECT_GT(ray_top_left.direction.y, 0.1F);
        EXPECT_GT(ray_top_left.direction.z, 0.5F);
        EXPECT_NEAR(math::length(ray_top_left.direction), 1.0F, float_epsilon);

        // Act: Compute ray at bottom-right corner
        const auto bottom_right_mouse = math::vec2<float>{800.0F, 600.0F};
        const auto ray_bottom_right = viewport.compute_camera_ray(bottom_right_mouse, vp_size);

        // Assert: Bottom-right ray points right, down, and forward with unit length
        EXPECT_GT(ray_bottom_right.direction.x, 0.1F);
        EXPECT_LT(ray_bottom_right.direction.y, -0.1F);
        EXPECT_GT(ray_bottom_right.direction.z, 0.5F);
        EXPECT_NEAR(math::length(ray_bottom_right.direction), 1.0F, float_epsilon);

        // Act: Compute ray with zero-sized viewport dimensions
        const auto zero_size = math::vec2<float>{0.0F, 0.0F};
        const auto ray_zero = viewport.compute_camera_ray(center_mouse, zero_size);

        // Assert: Safely falls back to camera forward vector without division by zero
        EXPECT_NEAR(ray_zero.direction.x, 0.0F, float_epsilon);
        EXPECT_NEAR(ray_zero.direction.y, 0.0F, float_epsilon);
        EXPECT_NEAR(ray_zero.direction.z, 1.0F, float_epsilon);
    }

    /// @brief Verifies scene placement position calculation against the ground plane (Y = 0),
    /// an entity with box collider, and the 5.0m forward fallback when no surface is intersected.
    TEST(editor_window_test, viewport_placement_ground_entity_and_fallback)
    {
        // Setup: Engine context with camera at (0, 2, -5) looking slightly downward
        auto engine_ctx = editor_engine_context{};
        auto& cam = engine_ctx.get_editor_camera();
        cam.set_position(math::vec3<float>{0.0F, 2.0F, -5.0F});
        cam.set_rotation(0.0F, 0.0F);
        cam.set_vertical_fov(math::as_radians(60.0F));
        cam.set_aspect_ratio(800.0F / 600.0F);

        const auto viewport = viewport_window{engine_ctx};
        const auto vp_size = math::vec2<float>{800.0F, 600.0F};

        // Act: Cursor at bottom center of viewport where downward ray intersects ground plane Y = 0
        const auto bottom_mouse = math::vec2<float>{400.0F, 600.0F};
        const auto ground_hit = viewport.calculate_placement_position(bottom_mouse, vp_size);

        // Assert: Hits ground plane with Y = 0.0 and tombstone hit entity
        constexpr auto float_epsilon = 1e-3F;
        EXPECT_TRUE(ground_hit.has_hit);
        EXPECT_FLOAT_EQ(ground_hit.position.y, 0.0F);
        EXPECT_GT(ground_hit.position.z, -5.0F);
        EXPECT_EQ(ground_hit.hit_entity, static_cast<ecs::entity>(ecs::tombstone));

        // Setup: Create a target entity directly in front of the camera with transform and box collider
        auto& reg = engine_ctx.get_entities();
        const auto target_ent = reg.create();
        reg.name(target_ent, "TargetCube");
        auto transform = ecs::transform_component{};
        transform.position(math::vec3<float>{0.0F, 2.0F, 0.0F});
        reg.assign(target_ent, transform);
        reg.assign(target_ent, physics::box_collider_component{
            .half_extents = math::vec3<float>{0.5F, 0.5F, 0.5F},
            .offset = math::vec3<float>{0.0F, 0.0F, 0.0F},
        });

        // Act: Cursor at center of viewport aimed straight at the target entity at (0, 2, 0)
        const auto center_mouse = math::vec2<float>{400.0F, 300.0F};
        const auto entity_hit = viewport.calculate_placement_position(center_mouse, vp_size);

        // Assert: Ray intersects the target entity bounds at front face Z = -0.5
        EXPECT_TRUE(entity_hit.has_hit);
        EXPECT_EQ(entity_hit.hit_entity, target_ent);
        EXPECT_NEAR(entity_hit.position.x, 0.0F, float_epsilon);
        EXPECT_NEAR(entity_hit.position.y, 2.0F, float_epsilon);
        EXPECT_NEAR(entity_hit.position.z, -0.5F, float_epsilon);

        // Setup: Remove target entity and aim upward so neither entity nor ground is hit
        reg.destroy(target_ent);
        const auto top_mouse = math::vec2<float>{400.0F, 0.0F};

        // Act: Calculate placement position for ray aimed into the sky
        const auto sky_placement = viewport.calculate_placement_position(top_mouse, vp_size);

        // Assert: Falls back to 5.0m along ray direction with has_hit = false
        EXPECT_FALSE(sky_placement.has_hit);
        EXPECT_EQ(sky_placement.hit_entity, static_cast<ecs::entity>(ecs::tombstone));
        const auto sky_ray = viewport.compute_camera_ray(top_mouse, vp_size);
        const auto expected_fallback = sky_ray.origin + (sky_ray.direction * 5.0F);
        EXPECT_NEAR(sky_placement.position.x, expected_fallback.x, float_epsilon);
        EXPECT_NEAR(sky_placement.position.y, expected_fallback.y, float_epsilon);
        EXPECT_NEAR(sky_placement.position.z, expected_fallback.z, float_epsilon);
    }

    /// @brief Verifies that dropping a prefab asset payload into the viewport executes
    /// instantiate_prefab_command placing the instance at the hit position, with undo and redo.
    TEST(editor_window_test, viewport_drop_prefab_instantiation_undo_redo)
    {
        // Setup: Engine context, active scene, prefab cache, and command history
        auto engine_ctx = editor_engine_context{};
        auto& scene_mgr = engine_ctx.get_scene_manager();
        const auto scene_res = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "ViewportDropScene.tscene");
        ASSERT_TRUE(scene_res.has_value());
        const auto active_scene = *scene_res;
        scene_mgr.set_active_scene(active_scene);

        auto& cam = engine_ctx.get_editor_camera();
        cam.set_position(math::vec3<float>{0.0F, 2.0F, -5.0F});
        cam.set_rotation(0.0F, 0.0F);

        auto p_cache = scene::prefab_cache{};
        auto& reg = engine_ctx.get_entities();
        const auto template_root = reg.create();
        reg.name(template_root, "ViewportPrefabRoot");
        auto tmpl = scene::prefab_template::from_entity(reg, template_root);
        reg.destroy(template_root);

        const auto prefab_id = guid::generate_random_guid();
        ASSERT_TRUE(p_cache.register_template(prefab_id, tempest::move(tmpl)));

        auto history = command_history{};
        auto viewport = viewport_window{engine_ctx, &history, &p_cache};
        const auto vp_size = math::vec2<float>{800.0F, 600.0F};

        // Downward ray hitting ground plane Y = 0
        const auto mouse_pos = math::vec2<float>{400.0F, 600.0F};
        const auto expected_placement = viewport.calculate_placement_position(mouse_pos, vp_size);
        ASSERT_TRUE(expected_placement.has_hit);
        ASSERT_FLOAT_EQ(expected_placement.position.y, 0.0F);

        auto payload = asset_payload{};
        payload.asset_guid = prefab_id;
        payload.set_type_name("prefab");
        payload.set_path("assets/prefabs/prop.tprefab");

        // Act: Drop prefab payload into viewport
        const auto drop_success = viewport.handle_asset_drop(payload, mouse_pos, vp_size);

        // Assert: Instantiation command succeeded and entity exists at placement position
        EXPECT_TRUE(drop_success);
        EXPECT_TRUE(history.can_undo(active_scene));

        const auto roots_opt = scene_mgr.get_root_entities(active_scene);
        ASSERT_TRUE(roots_opt.has_value());
        ASSERT_EQ(roots_opt->size(), 1U);
        const auto spawned_ent = roots_opt->front();
        EXPECT_TRUE(reg.is_valid(spawned_ent));

        const auto* const spawned_transform = reg.try_get<ecs::transform_component>(spawned_ent);
        ASSERT_NE(spawned_transform, nullptr);
        constexpr auto float_epsilon = 1e-3F;
        EXPECT_NEAR(spawned_transform->position().x, expected_placement.position.x, float_epsilon);
        EXPECT_NEAR(spawned_transform->position().y, expected_placement.position.y, float_epsilon);
        EXPECT_NEAR(spawned_transform->position().z, expected_placement.position.z, float_epsilon);

        // Act: Undo prefab instantiation
        auto cmd_ctx = command_context{
            .registry = reg,
            .scene_manager = scene_mgr,
            .prefab_cache = &p_cache,
            .events = &engine_ctx.get_events(),
            .log = &engine_ctx.get_logger(),
        };
        EXPECT_TRUE(history.undo(cmd_ctx, active_scene));

        // Assert: Spawned entity hierarchy is removed
        EXPECT_FALSE(reg.is_valid(spawned_ent));
        EXPECT_TRUE(history.can_redo(active_scene));

        // Act: Redo prefab instantiation
        EXPECT_TRUE(history.redo(cmd_ctx, active_scene));

        // Assert: Spawned entity hierarchy is restored
        const auto redo_roots = scene_mgr.get_root_entities(active_scene);
        ASSERT_TRUE(redo_roots.has_value());
        EXPECT_EQ(redo_roots->size(), 1U);
    }

    /// @brief Verifies that dropping a material asset payload onto an entity visible under the
    /// viewport cursor assigns the material via assign_material_command, with undo and redo.
    TEST(editor_window_test, viewport_drop_material_assignment_undo_redo)
    {
        // Setup: Engine context, active scene, target entity with existing material, and command history
        auto engine_ctx = editor_engine_context{};
        auto& scene_mgr = engine_ctx.get_scene_manager();
        const auto scene_res = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "ViewportMatScene.tscene");
        ASSERT_TRUE(scene_res.has_value());
        const auto active_scene = *scene_res;
        scene_mgr.set_active_scene(active_scene);

        auto& reg = engine_ctx.get_entities();
        const auto target_ent = scene_mgr.create_entity(active_scene, "MaterialTarget");
        auto transform = ecs::transform_component{};
        transform.position(math::vec3<float>{0.0F, 1.0F, 0.0F});
        reg.assign(target_ent, transform);
        reg.assign(target_ent, physics::box_collider_component{
            .half_extents = math::vec3<float>{0.5F, 0.5F, 0.5F},
        });
        const auto initial_material = guid::generate_random_guid();
        reg.assign(target_ent, core::material_component{.material_id = initial_material});

        auto& cam = engine_ctx.get_editor_camera();
        cam.set_position(math::vec3<float>{0.0F, 1.0F, -5.0F});
        cam.set_rotation(0.0F, 0.0F);

        auto history = command_history{};
        auto viewport = viewport_window{engine_ctx, &history};
        const auto vp_size = math::vec2<float>{800.0F, 600.0F};
        const auto center_mouse = math::vec2<float>{400.0F, 300.0F};

        const auto hit_check = viewport.calculate_placement_position(center_mouse, vp_size);
        ASSERT_TRUE(hit_check.has_hit);
        ASSERT_EQ(hit_check.hit_entity, target_ent);

        const auto dropped_material = guid::generate_random_guid();
        auto payload = asset_payload{};
        payload.asset_guid = dropped_material;
        payload.set_type_name("material");
        payload.set_path("assets/materials/stone.tmaterial");

        // Act: Drop material payload onto the entity in the viewport
        const auto drop_success = viewport.handle_asset_drop(payload, center_mouse, vp_size);

        // Assert: Material assignment command executed and target has new material GUID
        EXPECT_TRUE(drop_success);
        ASSERT_TRUE(reg.has<core::material_component>(target_ent));
        EXPECT_EQ(reg.get<core::material_component>(target_ent).material_id, dropped_material);
        EXPECT_TRUE(history.can_undo(active_scene));

        // Act: Undo material assignment
        auto cmd_ctx = command_context{
            .registry = reg,
            .scene_manager = scene_mgr,
            .events = &engine_ctx.get_events(),
            .log = &engine_ctx.get_logger(),
        };
        EXPECT_TRUE(history.undo(cmd_ctx, active_scene));

        // Assert: Entity reverted to initial material
        EXPECT_EQ(reg.get<core::material_component>(target_ent).material_id, initial_material);
        EXPECT_TRUE(history.can_redo(active_scene));

        // Act: Redo material assignment
        EXPECT_TRUE(history.redo(cmd_ctx, active_scene));

        // Assert: Entity restored to dropped material
        EXPECT_EQ(reg.get<core::material_component>(target_ent).material_id, dropped_material);
    }

    /// @brief Verifies that dropping invalid or mismatched asset payloads (e.g. material dropped
    /// onto ground or empty sky, or unknown payload type) is cleanly rejected without side effects.
    TEST(editor_window_test, viewport_drop_invalid_or_unknown_rejected)
    {
        // Setup: Engine context with camera looking horizontally into empty air
        auto engine_ctx = editor_engine_context{};
        auto& cam = engine_ctx.get_editor_camera();
        cam.set_position(math::vec3<float>{0.0F, 2.0F, -5.0F});
        cam.set_rotation(0.0F, 0.0F);

        auto history = command_history{};
        auto viewport = viewport_window{engine_ctx, &history};
        const auto vp_size = math::vec2<float>{800.0F, 600.0F};

        // Act: Drop material payload where ray hits sky (no entity hit)
        const auto center_mouse = math::vec2<float>{400.0F, 300.0F};
        auto material_payload = asset_payload{};
        material_payload.asset_guid = guid::generate_random_guid();
        material_payload.set_type_name("material");
        const auto sky_material_drop = viewport.handle_asset_drop(material_payload, center_mouse, vp_size);

        // Assert: Rejected because material requires a hovered entity
        EXPECT_FALSE(sky_material_drop);

        // Act: Drop material payload where ray hits ground plane (not an entity)
        const auto ground_mouse = math::vec2<float>{400.0F, 600.0F};
        const auto ground_material_drop = viewport.handle_asset_drop(material_payload, ground_mouse, vp_size);

        // Assert: Rejected because ground plane is a surface, not an entity
        EXPECT_FALSE(ground_material_drop);

        // Act: Drop unknown / non-asset payload type
        auto unknown_payload = asset_payload{};
        unknown_payload.asset_guid = guid::generate_random_guid();
        unknown_payload.set_type_name("unsupported_asset");
        const auto unknown_drop = viewport.handle_asset_drop(unknown_payload, ground_mouse, vp_size);

        // Assert: Rejected due to unhandled asset kind
        EXPECT_FALSE(unknown_drop);
    }
} // namespace tempest::editor::tests
