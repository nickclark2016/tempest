#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/array.hpp>
#include <tempest/commands/entity_commands.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/editor_selection.hpp>
#include <tempest/engine_types.hpp>
#include <tempest/event.hpp>
#include <tempest/format.hpp>
#include <tempest/guid.hpp>
#include <tempest/logger.hpp>
#include <tempest/material.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/memory.hpp>
#include <tempest/property_grid.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/scene/prefab_cache.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/prefab_instantiator.hpp>
#include <tempest/scene/prefab_operations.hpp>
#include <tempest/scene/prefab_template.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/span.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>
#include <tempest/vertex.hpp>
#include <tempest/windows/inspector_window.hpp>

namespace tempest::editor::tests
{
    namespace
    {
        template <typename T>
        auto to_byte_vector(const T& val) -> vector<byte>
        {
            auto result = vector<byte>{};
            result.resize(sizeof(T));
            tempest::memcpy(result.data(), &val, sizeof(T));
            return result;
        }
    } // namespace

    //==============================================================================
    // Inspector Window Lifecycle & Properties Tests
    //==============================================================================

    /// @brief Verifies inspector_window dock defaults, window name, and initial state.
    TEST(inspector_window_test, window_docking_and_naming_properties)
    {
        // Setup registry, scene manager, selection, and history
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto selection = editor_selection{};
        auto history = command_history{};
        auto inspector = inspector_window{reg, &scene_mgr, &selection, &history};

        // Assert window defaults
        EXPECT_EQ(inspector.desired_initial_dock(), editor_window::dock_location::right);
        EXPECT_EQ(inspector.window_name(), "Inspector");
        EXPECT_TRUE(inspector.open);
        EXPECT_TRUE(inspector.target == ecs::null);
    }

    //==============================================================================
    // Property Grid & Command Dispatch Tests
    //==============================================================================

    /// @brief Verifies that set_field_command updates component data, dispatches through
    /// command_history, and supports lossless undo and redo operations.
    TEST(inspector_window_test, property_grid_field_mutation_and_undo_redo)
    {
        // Setup entity with camera component
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        tempest::register_engine_types(types);
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto history = command_history{};

        const auto ent = reg.create();
        reg.assign(ent, render_system::camera_component{
                            .aspect_ratio = 1.0F,
                            .vertical_fov = 1.0F,
                            .near_plane = 0.1F,
                            .far_plane = 1000.0F,
                        });

        const auto addr = compute_entity_address(reg, ent);
        auto cmd_ctx = command_context{
            .registry = reg,
            .scene_manager = scene_mgr,
        };

        // Act: Apply mutation to vertical_fov
        const auto new_fov = 1.5F;
        auto set_cmd = make_unique<set_field_command>(addr, "tempest::render_system::camera_component", "vertical_fov",
                                                      to_byte_vector(new_fov));
        history.execute(tempest::move(set_cmd), cmd_ctx);

        // Assert mutated state
        const auto* cam_after = reg.try_get<render_system::camera_component>(ent);
        ASSERT_NE(cam_after, nullptr);
        EXPECT_FLOAT_EQ(cam_after->vertical_fov, 1.5F);

        // Act & Assert Undo
        EXPECT_TRUE(history.can_undo());
        history.undo(cmd_ctx);
        const auto* cam_undone = reg.try_get<render_system::camera_component>(ent);
        ASSERT_NE(cam_undone, nullptr);
        EXPECT_FLOAT_EQ(cam_undone->vertical_fov, 1.0F);

        // Act & Assert Redo
        EXPECT_TRUE(history.can_redo());
        history.redo(cmd_ctx);
        const auto* cam_redone = reg.try_get<render_system::camera_component>(ent);
        ASSERT_NE(cam_redone, nullptr);
        EXPECT_FLOAT_EQ(cam_redone->vertical_fov, 1.5F);
    }

    /// @brief Verifies that sequential field adjustments during an active gesture coalesce
    /// into a single history transaction.
    TEST(inspector_window_test, property_grid_drag_coalescing)
    {
        // Setup entity and command history
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        tempest::register_engine_types(types);
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto history = command_history{};

        const auto ent = reg.create();
        reg.assign(ent, render_system::camera_component{
                            .aspect_ratio = 1.0F,
                            .vertical_fov = 1.0F,
                            .near_plane = 0.1F,
                            .far_plane = 1000.0F,
                        });

        const auto addr = compute_entity_address(reg, ent);
        auto cmd_ctx = command_context{
            .registry = reg,
            .scene_manager = scene_mgr,
        };

        // Act: Begin drag gesture, apply multiple increments, and end gesture
        history.begin_gesture(100);
        for (auto i = 1; i <= 5; ++i)
        {
            const auto val = 1.0F + static_cast<float>(i) * 0.1F;
            auto step_cmd = make_unique<set_field_command>(addr, "tempest::render_system::camera_component",
                                                           "vertical_fov", to_byte_vector(val));
            history.execute(tempest::move(step_cmd), cmd_ctx);
        }
        history.end_gesture();

        // Assert that the command history merged all intermediate updates into 1 undo step
        EXPECT_TRUE(history.can_undo());
        const auto* cam_current = reg.try_get<render_system::camera_component>(ent);
        ASSERT_NE(cam_current, nullptr);
        EXPECT_FLOAT_EQ(cam_current->vertical_fov, 1.5F);

        // Act & Assert that undo resets directly to original pre-drag state in 1 step
        history.undo(cmd_ctx);
        const auto* cam_reverted = reg.try_get<render_system::camera_component>(ent);
        ASSERT_NE(cam_reverted, nullptr);
        EXPECT_FLOAT_EQ(cam_reverted->vertical_fov, 1.0F);
        EXPECT_FALSE(history.can_undo());
    }

    //==============================================================================
    // Component Management Tests
    //==============================================================================

    /// @brief Verifies that add_component_command and remove_component_command correctly mutate
    /// entity archetype composition with full transaction reversibility.
    TEST(inspector_window_test, add_and_remove_component_commands)
    {
        // Setup empty entity
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        tempest::register_engine_types(types);
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto history = command_history{};

        const auto ent = reg.create();
        const auto addr = compute_entity_address(reg, ent);
        auto cmd_ctx = command_context{
            .registry = reg,
            .scene_manager = scene_mgr,
        };

        const auto point_light_idx = types.type_index("tempest::render_system::point_light_component");
        ASSERT_TRUE(point_light_idx.has_value());

        // Act: Add point light component
        auto add_cmd = make_unique<add_component_command>(addr, *point_light_idx);
        history.execute(tempest::move(add_cmd), cmd_ctx);

        // Assert component addition
        EXPECT_TRUE(reg.has<render_system::point_light_component>(ent));

        // Act: Remove point light component
        auto rem_cmd = make_unique<remove_component_command>(addr, *point_light_idx);
        history.execute(tempest::move(rem_cmd), cmd_ctx);

        // Assert component removal
        EXPECT_FALSE(reg.has<render_system::point_light_component>(ent));

        // Act & Assert Undo removal restores component
        history.undo(cmd_ctx);
        EXPECT_TRUE(reg.has<render_system::point_light_component>(ent));

        // Act & Assert Undo addition removes component
        history.undo(cmd_ctx);
        EXPECT_FALSE(reg.has<render_system::point_light_component>(ent));
    }

    /// @brief Verifies that reset_component_command restores fields to default reflection values.
    TEST(inspector_window_test, reset_component_command_restores_defaults)
    {
        // Setup entity with modified camera values
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        tempest::register_engine_types(types);
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto history = command_history{};

        const auto ent = reg.create();
        reg.assign(ent, render_system::camera_component{
                            .aspect_ratio = 4.0F / 3.0F,
                            .vertical_fov = 2.0F,
                            .near_plane = 25.0F,
                            .far_plane = 5000.0F,
                        });

        const auto addr = compute_entity_address(reg, ent);
        auto cmd_ctx = command_context{
            .registry = reg,
            .scene_manager = scene_mgr,
        };

        const auto cam_idx = types.type_index("tempest::render_system::camera_component");
        ASSERT_TRUE(cam_idx.has_value());

        // Act: Reset component to default
        auto reset_cmd = make_unique<reset_component_command>(addr, *cam_idx);
        history.execute(tempest::move(reset_cmd), cmd_ctx);

        // Assert that near_plane and far_plane match engine defaults
        const auto* cam = reg.try_get<render_system::camera_component>(ent);
        ASSERT_NE(cam, nullptr);
        EXPECT_FLOAT_EQ(cam->near_plane, 0.1F);
        EXPECT_FLOAT_EQ(cam->far_plane, 1000.0F);

        // Act & Assert Undo restores modified values
        history.undo(cmd_ctx);
        const auto* cam_restored = reg.try_get<render_system::camera_component>(ent);
        ASSERT_NE(cam_restored, nullptr);
        EXPECT_FLOAT_EQ(cam_restored->near_plane, 25.0F);
        EXPECT_FLOAT_EQ(cam_restored->far_plane, 5000.0F);
    }

    //==============================================================================
    // Prefab Overrides & Unknown Component Tests
    //==============================================================================

    /// @brief Verifies that revert_override_command clears recorded field overrides on prefab instances.
    TEST(inspector_window_test, prefab_override_revert_command)
    {
        // Setup template, cache, and instantiated prefab in scene
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        tempest::register_engine_types(types);
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto prefab_cache = scene::prefab_cache{};
        auto history = command_history{};

        const auto scene_res = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "Test Scene");
        ASSERT_TRUE(scene_res.has_value());
        const auto scene = *scene_res;

        auto t_reg = ecs::archetype_registry{events, types};
        const auto tmpl_root = t_reg.create();
        t_reg.assign(tmpl_root, render_system::camera_component{
                                    .aspect_ratio = 16.0F / 9.0F,
                                    .vertical_fov = 1.0F,
                                    .near_plane = 0.1F,
                                    .far_plane = 1000.0F,
                                });
        auto tmpl = scene::prefab_template::from_entity(t_reg, tmpl_root);
        const auto test_guid = guid{array<byte, 16>{byte{1}, byte{2}, byte{3}, byte{4}}};
        prefab_cache.register_template(test_guid, tempest::move(tmpl));

        const auto inst_res = scene::prefab_instantiator::instantiate(test_guid, prefab_cache, reg);
        ASSERT_TRUE(inst_res.has_value());
        const auto inst_root = inst_res->root_entity;
        const auto root_id = scene::entity_local_id{1001};
        reg.assign(inst_root, scene::scene_entity_component{.scene = scene, .local_id = root_id});

        const auto root_addr = compute_entity_address(reg, inst_root);
        auto cmd_ctx = command_context{
            .registry = reg,
            .scene_manager = scene_mgr,
            .prefab_cache = &prefab_cache,
        };

        // Act: Apply field override to vertical_fov on the instance
        const auto new_fov = 1.8F;
        auto set_cmd = make_unique<set_field_command>(root_addr, "tempest::render_system::camera_component",
                                                      "vertical_fov", to_byte_vector(new_fov));
        history.execute(tempest::move(set_cmd), cmd_ctx);

        // Assert override was recorded on prefab instance
        const auto* inst_comp = reg.try_get<scene::prefab_instance_component>(inst_root);
        ASSERT_NE(inst_comp, nullptr);
        ASSERT_FALSE(inst_comp->overrides.field_overrides.empty());
        EXPECT_EQ(inst_comp->overrides.field_overrides[0].field_name, "vertical_fov");

        // Act: Revert the override via history using recorded override's target
        const auto target_path = inst_comp->overrides.field_overrides[0].target;
        auto rev_cmd = make_unique<revert_override_command>(
            root_addr, target_path, "tempest::render_system::camera_component", "vertical_fov");
        history.execute(tempest::move(rev_cmd), cmd_ctx);

        // Assert that the override was cleared and value restored to template default
        const auto* inst_comp_after = reg.try_get<scene::prefab_instance_component>(inst_root);
        ASSERT_NE(inst_comp_after, nullptr);
        EXPECT_TRUE(inst_comp_after->overrides.field_overrides.empty());
        const auto* cam = reg.try_get<render_system::camera_component>(inst_root);
        ASSERT_NE(cam, nullptr);
        EXPECT_FLOAT_EQ(cam->vertical_fov, 1.0F);
    }

    /// @brief Verifies that unrecognized component JSON payloads can be mapped into concrete
    /// registered components via map_unknown_component_command.
    TEST(inspector_window_test, unknown_component_mapping_undo_redo)
    {
        // Setup scene with unrecognized component data
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        tempest::register_engine_types(types);
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto history = command_history{};

        const auto scene_res = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "Test Scene");
        ASSERT_TRUE(scene_res.has_value());
        const auto scene = *scene_res;

        auto cmd_ctx = command_context{
            .registry = reg,
            .scene_manager = scene_mgr,
        };

        auto create_cmd = create_entity_command{scene, "MappedEntity"};
        create_cmd.apply(cmd_ctx);
        const auto addr = create_cmd.created_address();
        const auto ent = resolve_entity_address(reg, scene_mgr, addr);
        const auto local_id = addr.id_path[0].value;

        auto* unrec_data = scene_mgr.get_unrecognized_data(scene);
        ASSERT_NE(unrec_data, nullptr);

        auto unknown_comp = scene::unknown_component{
            .name = "legacy::point_light",
            .json_payload = R"({"color":[1.0, 1.0, 1.0],"intensity":8.0,"range":25.0})",
        };
        auto ent_data = scene::entity_unrecognized_data{};
        ent_data.unknown_components.push_back(tempest::move(unknown_comp));
        unrec_data->entities[local_id] = tempest::move(ent_data);

        const auto target_type_idx = types.type_index("tempest::render_system::point_light_component");
        ASSERT_TRUE(target_type_idx.has_value());

        // Act: Map unknown component to point_light_component
        auto map_cmd = make_unique<map_unknown_component_command>(addr, "legacy::point_light", *target_type_idx);
        history.execute(tempest::move(map_cmd), cmd_ctx);

        // Assert: Unknown payload removed and component assigned
        EXPECT_TRUE(reg.has<render_system::point_light_component>(ent));
        EXPECT_TRUE(unrec_data->entities.empty() || unrec_data->entities.find(local_id) == unrec_data->entities.end());

        // Act & Assert Undo restores unknown component and removes concrete component
        history.undo(cmd_ctx);
        EXPECT_FALSE(reg.has<render_system::point_light_component>(ent));
        ASSERT_FALSE(unrec_data->entities.empty());
        ASSERT_NE(unrec_data->entities.find(local_id), unrec_data->entities.end());
        EXPECT_EQ(unrec_data->entities[local_id].unknown_components[0].name, "legacy::point_light");

        // Act & Assert Redo reapplies mapping
        history.redo(cmd_ctx);
        EXPECT_TRUE(reg.has<render_system::point_light_component>(ent));
        EXPECT_TRUE(unrec_data->entities.empty() || unrec_data->entities.find(local_id) == unrec_data->entities.end());
    }

    //==============================================================================
    // Gesture & Direct Execution Fallback Tests
    //==============================================================================

    /// @brief Verifies that active gestures in command_history are automatically terminated
    /// when the inspector changes its target entity.
    TEST(inspector_window_test, lingering_gesture_terminated_on_selection_change)
    {
        // Setup registry, selection, history, and inspector window
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        tempest::register_engine_types(types);
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto selection = editor_selection{};
        auto history = command_history{};
        auto inspector = inspector_window{reg, &scene_mgr, &selection, &history};
        inspector.open = false;

        const auto ent_a = reg.create();
        const auto ent_b = reg.create();
        selection.set_selection(ent_a, reg, &scene_mgr);

        // Act: Begin a drag gesture and verify active state
        history.begin_gesture(42);
        EXPECT_TRUE(history.is_in_gesture());

        // Act: Change selection to ent_b and trigger draw
        selection.set_selection(ent_b, reg, &scene_mgr);
        inspector.draw();

        // Assert: Inspector automatically ended the gesture
        EXPECT_FALSE(history.is_in_gesture());
    }

    /// @brief Verifies that inspector_window destructor automatically ends any active gesture.
    TEST(inspector_window_test, lingering_gesture_terminated_on_destruction)
    {
        // Setup registry, scene manager, and history
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        tempest::register_engine_types(types);
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto history = command_history{};

        // Act: Begin a drag gesture
        history.begin_gesture(99);
        EXPECT_TRUE(history.is_in_gesture());

        // Act: Scope-destruct inspector window
        {
            auto inspector = inspector_window{reg, &scene_mgr, nullptr, &history};
        }

        // Assert: Gesture automatically ended on destruction
        EXPECT_FALSE(history.is_in_gesture());
    }

    /// @brief Verifies that multiple field overrides on a single component can be reverted
    /// without iterator invalidation when mutating prefab overrides.
    TEST(inspector_window_test, multiple_field_overrides_revert_without_iterator_invalidation)
    {
        // Setup template with multiple fields and instantiate in scene
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        tempest::register_engine_types(types);
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto prefab_cache = scene::prefab_cache{};
        auto history = command_history{};

        const auto scene_res = scene_mgr.load_from_string_sync(
            R"({"schema_version":1,"generator":"test","entities":[]})", "Test Scene");
        ASSERT_TRUE(scene_res.has_value());
        const auto scene = *scene_res;

        auto t_reg = ecs::archetype_registry{events, types};
        const auto tmpl_root = t_reg.create();
        t_reg.assign(tmpl_root, render_system::camera_component{
                                    .aspect_ratio = 16.0F / 9.0F,
                                    .vertical_fov = 1.0F,
                                    .near_plane = 0.1F,
                                    .far_plane = 1000.0F,
                                });
        auto tmpl = scene::prefab_template::from_entity(t_reg, tmpl_root);
        const auto test_guid = guid{array<byte, 16>{byte{5}, byte{6}, byte{7}, byte{8}}};
        prefab_cache.register_template(test_guid, tempest::move(tmpl));

        const auto inst_res = scene::prefab_instantiator::instantiate(test_guid, prefab_cache, reg);
        ASSERT_TRUE(inst_res.has_value());
        const auto inst_root = inst_res->root_entity;
        reg.assign(inst_root, scene::scene_entity_component{.scene = scene, .local_id = scene::entity_local_id{2002}});

        const auto root_addr = compute_entity_address(reg, inst_root);
        auto cmd_ctx = command_context{
            .registry = reg,
            .scene_manager = scene_mgr,
            .prefab_cache = &prefab_cache,
        };

        // Act: Apply two separate field overrides on the instance
        const auto new_aspect = 2.0F;
        auto set_aspect_cmd = make_unique<set_field_command>(root_addr, "tempest::render_system::camera_component",
                                                             "aspect_ratio", to_byte_vector(new_aspect));
        history.execute(tempest::move(set_aspect_cmd), cmd_ctx);

        const auto new_fov = 1.25F;
        auto set_fov_cmd = make_unique<set_field_command>(root_addr, "tempest::render_system::camera_component",
                                                          "vertical_fov", to_byte_vector(new_fov));
        history.execute(tempest::move(set_fov_cmd), cmd_ctx);

        // Assert two overrides recorded
        const auto* inst_comp = reg.try_get<scene::prefab_instance_component>(inst_root);
        ASSERT_NE(inst_comp, nullptr);
        EXPECT_EQ(inst_comp->overrides.field_overrides.size(), 2U);

        // Act: Revert both overrides safely using collected field names
        auto target_path = scene::id_path{};
        target_path.push_back(scene::entity_local_id{1});
        auto matching_fields = vector<string>{};
        for (const auto& ov : inst_comp->overrides.field_overrides)
        {
            if (ov.component_name == "tempest::render_system::camera_component")
            {
                matching_fields.push_back(ov.field_name);
            }
        }
        for (const auto& field_name : matching_fields)
        {
            auto rev_cmd = make_unique<revert_override_command>(
                root_addr, target_path, "tempest::render_system::camera_component", field_name);
            history.execute(tempest::move(rev_cmd), cmd_ctx);
        }

        // Assert all overrides reverted and values restored
        const auto* inst_comp_reverted = reg.try_get<scene::prefab_instance_component>(inst_root);
        ASSERT_NE(inst_comp_reverted, nullptr);
        EXPECT_TRUE(inst_comp_reverted->overrides.field_overrides.empty());
        const auto* cam = reg.try_get<render_system::camera_component>(inst_root);
        ASSERT_NE(cam, nullptr);
        EXPECT_FLOAT_EQ(cam->aspect_ratio, 16.0F / 9.0F);
        EXPECT_FLOAT_EQ(cam->vertical_fov, 1.0F);
    }

    /// @brief Verifies that reset_component_command, revert_override_command, and
    /// property_grid_context execute directly via apply() when history is null.
    TEST(inspector_window_test, direct_command_execution_without_history)
    {
        // Setup entity with camera component and property grid context without history
        auto events = event::event_registry{};
        auto types = ecs::component_type_registry{};
        tempest::register_engine_types(types);
        auto reg = ecs::archetype_registry{events, types};
        auto scene_mgr = scene::scene_manager{reg};
        auto prefab_cache = scene::prefab_cache{};

        const auto ent = reg.create();
        reg.assign(ent, render_system::camera_component{
                            .aspect_ratio = 4.0F / 3.0F,
                            .vertical_fov = 2.0F,
                            .near_plane = 25.0F,
                            .far_plane = 5000.0F,
                        });

        const auto addr = compute_entity_address(reg, ent);
        auto cmd_ctx = command_context{
            .registry = reg,
            .scene_manager = scene_mgr,
            .prefab_cache = &prefab_cache,
        };

        const auto cam_idx = types.type_index("tempest::render_system::camera_component");
        ASSERT_TRUE(cam_idx.has_value());

        // Act: Directly execute reset_component_command without history
        auto reset_cmd = reset_component_command{addr, *cam_idx};
        reset_cmd.apply(cmd_ctx);

        // Assert reset took effect directly
        const auto* cam = reg.try_get<render_system::camera_component>(ent);
        ASSERT_NE(cam, nullptr);
        EXPECT_FLOAT_EQ(cam->near_plane, 0.1F);
        EXPECT_FLOAT_EQ(cam->far_plane, 1000.0F);

        // Act & Assert: Property grid context has valid non_null registry and default null history
        auto grid_ctx = property_grid_context{
            .registry = reg,
            .entity = ent,
        };
        EXPECT_EQ(grid_ctx.history, nullptr);
        EXPECT_EQ(grid_ctx.cmd_ctx, nullptr);
        EXPECT_EQ(grid_ctx.prefab_instance, nullptr);
        EXPECT_TRUE(grid_ctx.registry->is_valid(ent));
    }
} // namespace tempest::editor::tests
