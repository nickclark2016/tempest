#include <gtest/gtest.h>

#include <tempest/checked.hpp>
#include <tempest/command_history.hpp>
#include <tempest/dynamic_entity_id_component.hpp>
#include <tempest/editor_command.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/prefab_instance_component.hpp>
#include <tempest/scene/scene_manager.hpp>

namespace
{
    using namespace tempest;
    using namespace tempest::editor;

    constexpr auto empty_scene_json = string_view{R"({"version": 1, "entities": []})"};

    class mock_editor_command final : public editor_command
    {
      public:
        static constexpr int32_t default_value = 0;

        mock_editor_command(int32_t& target, int32_t old_val, int32_t new_val,
                            scene::scene_handle sc = {}, bool can_merge = false,
                            vector<int32_t>* log = nullptr, int32_t id = 0)
            : _target{&target},
              _old_value{old_val},
              _new_value{new_val},
              _scene{sc},
              _can_merge{can_merge},
              _log{log},
              _id{id}
        {
        }

        ~mock_editor_command() override = default;
        mock_editor_command(const mock_editor_command&) = default;
        mock_editor_command(mock_editor_command&&) noexcept = default;
        auto operator=(const mock_editor_command&) -> mock_editor_command& = default;
        auto operator=(mock_editor_command&&) noexcept -> mock_editor_command& = default;

        auto apply(command_context&) -> void override
        {
            *_target = _new_value;
            if (_log != nullptr)
            {
                _log->push_back(_id);
            }
        }

        auto revert(command_context&) -> void override
        {
            *_target = _old_value;
            if (_log != nullptr)
            {
                _log->push_back(-_id);
            }
        }

        [[nodiscard]] auto label() const noexcept -> string_view override
        {
            return "mock_editor_command";
        }

        [[nodiscard]] auto target_scene() const noexcept -> scene::scene_handle override
        {
            return _scene;
        }

        [[nodiscard]] auto merge_with(const editor_command& next) -> bool override
        {
            if (!_can_merge)
            {
                return false;
            }
            const auto* next_mock = dynamic_cast<const mock_editor_command*>(&next);
            if (next_mock == nullptr || next_mock->_target != _target)
            {
                return false;
            }
            _new_value = next_mock->_new_value;
            return true;
        }

      private:
        int32_t* _target;
        int32_t _old_value = default_value;
        int32_t _new_value = default_value;
        scene::scene_handle _scene = {};
        bool _can_merge = false;
        vector<int32_t>* _log = nullptr;
        int32_t _id = 0;
    };

    struct test_fixture
    {
        event::event_registry event_reg{};
        ecs::component_type_registry type_reg{};
        ecs::archetype_registry registry{event_reg, type_reg};
        scene::scene_manager scene_mgr{registry};
        command_context ctx{
            .registry = registry,
            .scene_manager = scene_mgr,
            .events = &event_reg,
        };
    };
} // namespace

// ============================================================================
// Command History Execution, Undo & Redo Tests
// ============================================================================

/// @brief Verifies that executing a command applies mutations and dirty state, and undoing reverts idempotently.
TEST(command_history_test, execute_and_undo_is_idempotent)
{
    // Setup: Initialize context, scene, and command history
    auto fix = test_fixture{};
    auto history = command_history{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneA");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto value = int32_t{10};
    EXPECT_FALSE(history.is_dirty(scene));
    EXPECT_FALSE(fix.scene_mgr.is_dirty(scene));

    // Act: Execute command mutating value from 10 to 42
    auto cmd = make_unique<mock_editor_command>(value, 10, 42, scene);
    history.execute(tempest::move(cmd), fix.ctx);

    // Assert: Mutation applied and dirty state toggled
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(history.can_undo(scene));
    EXPECT_FALSE(history.can_redo(scene));
    EXPECT_TRUE(history.is_dirty(scene));
    EXPECT_TRUE(fix.scene_mgr.is_dirty(scene));

    // Act: Undo command
    const auto undo_res = history.undo(fix.ctx, scene);

    // Assert: Value restored to initial state and dirty state cleared
    EXPECT_TRUE(undo_res);
    EXPECT_EQ(value, 10);
    EXPECT_FALSE(history.can_undo(scene));
    EXPECT_TRUE(history.can_redo(scene));
    EXPECT_FALSE(history.is_dirty(scene));
    EXPECT_FALSE(fix.scene_mgr.is_dirty(scene));
}

/// @brief Verifies that redo reapplies an undone command and restores dirty state.
TEST(command_history_test, redo_reapplies_command)
{
    // Setup: Initialize context, scene, and command history
    auto fix = test_fixture{};
    auto history = command_history{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneA");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto value = int32_t{5};
    history.execute(make_unique<mock_editor_command>(value, 5, 100, scene), fix.ctx);
    EXPECT_EQ(value, 100);

    // Act: Undo then redo
    EXPECT_TRUE(history.undo(fix.ctx, scene));
    EXPECT_EQ(value, 5);

    const auto redo_res = history.redo(fix.ctx, scene);

    // Assert: Redo restored modified value and dirty state
    EXPECT_TRUE(redo_res);
    EXPECT_EQ(value, 100);
    EXPECT_TRUE(history.can_undo(scene));
    EXPECT_FALSE(history.can_redo(scene));
    EXPECT_TRUE(history.is_dirty(scene));
    EXPECT_TRUE(fix.scene_mgr.is_dirty(scene));
}

/// @brief Verifies that executing a new command while in an undone state truncates the redo stack.
TEST(command_history_test, branching_mutation_clears_redo_stack)
{
    // Setup: Push 2 commands and undo 1
    auto fix = test_fixture{};
    auto history = command_history{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneA");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto val_a = int32_t{0};
    auto val_b = int32_t{0};

    history.execute(make_unique<mock_editor_command>(val_a, 0, 10, scene), fix.ctx);
    history.execute(make_unique<mock_editor_command>(val_b, 0, 20, scene), fix.ctx);
    EXPECT_TRUE(history.can_undo(scene));

    EXPECT_TRUE(history.undo(fix.ctx, scene));
    EXPECT_EQ(val_b, 0);
    EXPECT_TRUE(history.can_redo(scene));

    // Act: Execute a branching third command
    auto val_c = int32_t{0};
    history.execute(make_unique<mock_editor_command>(val_c, 0, 30, scene), fix.ctx);

    // Assert: Redo stack is discarded and can_redo is false
    EXPECT_EQ(val_c, 30);
    EXPECT_FALSE(history.can_redo(scene));
    EXPECT_TRUE(history.can_undo(scene));
}

// ============================================================================
// Drag Coalescing & Gesture Tests
// ============================================================================

/// @brief Verifies that sequential mergeable commands inside an active gesture coalesce into a single undo step.
TEST(command_history_test, drag_coalescing_merges_within_gesture)
{
    // Setup: Initialize context and gesture
    auto fix = test_fixture{};
    auto history = command_history{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneA");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto value = int32_t{0};

    // Act: Begin drag gesture and issue incremental mutations
    history.begin_gesture(1);
    EXPECT_TRUE(history.is_in_gesture());

    history.execute(make_unique<mock_editor_command>(value, 0, 10, scene, true), fix.ctx);
    EXPECT_EQ(value, 10);

    history.execute(make_unique<mock_editor_command>(value, 10, 20, scene, true), fix.ctx);
    EXPECT_EQ(value, 20);

    history.execute(make_unique<mock_editor_command>(value, 20, 50, scene, true), fix.ctx);
    EXPECT_EQ(value, 50);

    history.end_gesture();
    EXPECT_FALSE(history.is_in_gesture());

    // Assert: Single undo step reverts directly back to the initial value before the gesture
    EXPECT_TRUE(history.can_undo(scene));
    const auto undo_res = history.undo(fix.ctx, scene);
    EXPECT_TRUE(undo_res);
    EXPECT_EQ(value, 0);
    EXPECT_FALSE(history.can_undo(scene));
}

/// @brief Verifies that separate drag gestures on the same field produce distinct undo steps.
TEST(command_history_test, distinct_gestures_do_not_coalesce)
{
    // Setup: Initialize context and perform gesture 1
    auto fix = test_fixture{};
    auto history = command_history{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneA");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto value = int32_t{0};

    // Gesture 1: Drag from 0 to 10 to 20
    history.begin_gesture(1);
    history.execute(make_unique<mock_editor_command>(value, 0, 10, scene, true), fix.ctx);
    history.execute(make_unique<mock_editor_command>(value, 10, 20, scene, true), fix.ctx);
    history.end_gesture();
    EXPECT_EQ(value, 20);

    // Gesture 2: Drag from 20 to 30 to 40
    history.begin_gesture(2);
    history.execute(make_unique<mock_editor_command>(value, 20, 30, scene, true), fix.ctx);
    history.execute(make_unique<mock_editor_command>(value, 30, 40, scene, true), fix.ctx);
    history.end_gesture();
    EXPECT_EQ(value, 40);

    // Assert: Two distinct undo steps exist
    EXPECT_TRUE(history.can_undo(scene));
    EXPECT_TRUE(history.undo(fix.ctx, scene));
    EXPECT_EQ(value, 20);

    EXPECT_TRUE(history.can_undo(scene));
    EXPECT_TRUE(history.undo(fix.ctx, scene));
    EXPECT_EQ(value, 0);

    EXPECT_FALSE(history.can_undo(scene));
}

// ============================================================================
// Compound Command Tests
// ============================================================================

/// @brief Verifies that compound_command applies sub-commands in forward order and reverts in reverse order upon undo.
TEST(command_history_test, compound_command_atomic_undo)
{
    // Setup: Create 5 sub-commands with execution log
    auto fix = test_fixture{};
    auto history = command_history{};
    const auto scene_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneA");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    auto log = vector<int32_t>{};
    auto target_val = int32_t{0};

    auto compound = make_unique<compound_command>("MultiStepOperation");
    for (auto i = 1; i <= 5; ++i)
    {
        compound->add_command(make_unique<mock_editor_command>(target_val, i - 1, i, scene, false, &log, i));
    }

    // Act: Execute compound command
    history.execute(tempest::move(compound), fix.ctx);

    // Assert: Forward execution order 1, 2, 3, 4, 5
    ASSERT_EQ(log.size(), 5U);
    EXPECT_EQ(log[0], 1);
    EXPECT_EQ(log[1], 2);
    EXPECT_EQ(log[2], 3);
    EXPECT_EQ(log[3], 4);
    EXPECT_EQ(log[4], 5);
    EXPECT_EQ(target_val, 5);

    // Act: Single undo
    const auto undo_res = history.undo(fix.ctx, scene);

    // Assert: Reverse revert order -5, -4, -3, -2, -1
    EXPECT_TRUE(undo_res);
    ASSERT_EQ(log.size(), 10U);
    EXPECT_EQ(log[5], -5);
    EXPECT_EQ(log[6], -4);
    EXPECT_EQ(log[7], -3);
    EXPECT_EQ(log[8], -2);
    EXPECT_EQ(log[9], -1);
    EXPECT_EQ(target_val, 0);
}

// ============================================================================
// Multi-Scene Isolation & Dirty State Tests
// ============================================================================

/// @brief Verifies that mutations and undo operations in Scene A do not bleed into Scene B's stack or dirty state.
TEST(command_history_test, per_scene_stack_isolation)
{
    // Setup: Initialize two loaded scenes
    auto fix = test_fixture{};
    auto history = command_history{};
    const auto scene_a_res = fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneA");
    const auto scene_b_res =
        fix.scene_mgr.load_from_string_sync(empty_scene_json, "SceneB", {.mode = scene::scene_load_mode::additive});
    ASSERT_TRUE(scene_a_res.has_value());
    ASSERT_TRUE(scene_b_res.has_value());
    const auto scene_a = *scene_a_res;
    const auto scene_b = *scene_b_res;

    auto val_a = int32_t{0};
    auto val_b = int32_t{0};

    // Act: Mutate Scene A only
    history.execute(make_unique<mock_editor_command>(val_a, 0, 100, scene_a), fix.ctx);

    // Assert: Scene A dirty, Scene B clean
    EXPECT_TRUE(history.is_dirty(scene_a));
    EXPECT_TRUE(fix.scene_mgr.is_dirty(scene_a));
    EXPECT_TRUE(history.can_undo(scene_a));

    EXPECT_FALSE(history.is_dirty(scene_b));
    EXPECT_FALSE(fix.scene_mgr.is_dirty(scene_b));
    EXPECT_FALSE(history.can_undo(scene_b));

    // Act: Mutate Scene B and mark Scene A saved
    history.execute(make_unique<mock_editor_command>(val_b, 0, 200, scene_b), fix.ctx);
    history.mark_saved(scene_a);

    // Assert: Scene A clean, Scene B dirty
    EXPECT_FALSE(history.is_dirty(scene_a));
    EXPECT_FALSE(fix.scene_mgr.is_dirty(scene_a));
    EXPECT_TRUE(history.is_dirty(scene_b));
    EXPECT_TRUE(fix.scene_mgr.is_dirty(scene_b));

    // Act: Undo Scene B
    EXPECT_TRUE(history.undo(fix.ctx, scene_b));
    EXPECT_EQ(val_b, 0);
    EXPECT_FALSE(history.is_dirty(scene_b));
    EXPECT_EQ(val_a, 100);
}

// ============================================================================
// Entity Address Roundtrip Tests
// ============================================================================

/// @brief Verifies compute_entity_address and resolve_entity_address across root, child, nested prefab, and dynamic entities.
TEST(command_history_test, entity_address_resolution_roundtrip)
{
    // Setup: Initialize registry and scene manager with required components
    auto fix = test_fixture{};
    auto& reg = fix.registry;
    auto& scene_mgr = fix.scene_mgr;

    reg.type_registry().ensure<scene::scene_entity_component>();
    reg.type_registry().ensure<scene::prefab_instance_component>();
    reg.type_registry().ensure<scene::prefab_member_component>();
    reg.type_registry().ensure<dynamic_entity_id_component>();
    reg.type_registry().ensure<ecs::relationship_component<ecs::entity>>();

    const auto scene_res = scene_mgr.load_from_string_sync(empty_scene_json, "AuthoringScene");
    ASSERT_TRUE(scene_res.has_value());
    const auto scene = *scene_res;

    // Case 1: Scene root entity
    const auto scene_root = scene_mgr.create_entity(scene, "RootEntity");
    ASSERT_TRUE(reg.is_valid(scene_root));

    const auto root_addr = compute_entity_address(reg, scene_root);
    EXPECT_EQ(root_addr.scene, scene);
    EXPECT_EQ(root_addr.id_path.size(), 1U);

    const auto resolved_root = resolve_entity_address(reg, scene_mgr, root_addr);
    EXPECT_EQ(resolved_root, scene_root);

    // Case 2: Child entity parented to scene root
    const auto scene_child = scene_mgr.create_entity(scene, "ChildEntity");
    ASSERT_TRUE(reg.is_valid(scene_child));
    ecs::create_parent_child_relationship(reg, scene_root, scene_child);

    const auto child_addr = compute_entity_address(reg, scene_child);
    EXPECT_EQ(child_addr.scene, scene);
    EXPECT_EQ(child_addr.id_path.size(), 2U);
    EXPECT_EQ(child_addr.id_path[0], root_addr.id_path[0]);

    const auto resolved_child = resolve_entity_address(reg, scene_mgr, child_addr);
    EXPECT_EQ(resolved_child, scene_child);

    // Case 3: Nested prefab member entity
    const auto prefab_root = scene_mgr.create_entity(scene, "PrefabRoot");
    reg.assign(prefab_root, scene::prefab_instance_component{});
    reg.assign(prefab_root, scene::prefab_member_component{
                                .template_local_id = scene::entity_local_id{0x1000},
                            });

    const auto nested_parent = reg.create();
    reg.assign(nested_parent, scene::prefab_member_component{
                                  .template_local_id = scene::entity_local_id{0x2000},
                              });
    ecs::create_parent_child_relationship(reg, prefab_root, nested_parent);

    const auto nested_leaf = reg.create();
    reg.assign(nested_leaf, scene::prefab_member_component{
                                .template_local_id = scene::entity_local_id{0x3000},
                            });
    ecs::create_parent_child_relationship(reg, nested_parent, nested_leaf);

    const auto leaf_addr = compute_entity_address(reg, nested_leaf);
    EXPECT_EQ(leaf_addr.scene, scene);
    ASSERT_EQ(leaf_addr.id_path.size(), 3U);
    EXPECT_EQ(leaf_addr.id_path[1], scene::entity_local_id{0x2000});
    EXPECT_EQ(leaf_addr.id_path[2], scene::entity_local_id{0x3000});

    const auto resolved_leaf = resolve_entity_address(reg, scene_mgr, leaf_addr);
    EXPECT_EQ(resolved_leaf, nested_leaf);

    // Case 4: Dynamic (non-scene-bound) entity
    const auto dyn_ent = reg.create();
    ASSERT_TRUE(reg.is_valid(dyn_ent));
    EXPECT_FALSE(reg.has<dynamic_entity_id_component>(dyn_ent));

    const auto dyn_addr = compute_entity_address(reg, dyn_ent);
    EXPECT_FALSE(dyn_addr.scene.is_valid());
    ASSERT_EQ(dyn_addr.id_path.size(), 1U);
    EXPECT_TRUE(reg.has<dynamic_entity_id_component>(dyn_ent));

    const auto resolved_dyn = resolve_entity_address(reg, scene_mgr, dyn_addr);
    EXPECT_EQ(resolved_dyn, dyn_ent);
}
