#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/component_type_registry.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/format.hpp>
#include <tempest/reflection.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/scene/scene_entity_component.hpp>
#include <tempest/scene/scene_load_handle.hpp>
#include <tempest/scene/scene_manager.hpp>
#include <tempest/scene/scene_tags.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

#if defined(_MSC_VER)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

namespace tempest_test
{
    struct test_async_component
    {
        int32_t value = 0;
        float scale = 1.0F;
    };

    static_assert(tempest::ecs::component<test_async_component>);
} // namespace tempest_test

namespace tempest::ecs
{
    template <>
    struct reflect<tempest_test::test_async_component>
    {
        static void describe(type_builder<tempest_test::test_async_component>& builder)
        {
            builder.version(1)
                .field<&tempest_test::test_async_component::value>("value")
                .field<&tempest_test::test_async_component::scale>("scale");
        }
    };
} // namespace tempest::ecs

namespace
{
    struct async_test_context
    {
        tempest::event::event_registry event_reg;
        tempest::ecs::component_type_registry type_reg;
        tempest::ecs::basic_archetype_registry registry{event_reg, type_reg};
        tempest::scene::scene_manager manager{registry};

        async_test_context()
        {
            const auto comp_idx = type_reg.ensure<tempest_test::test_async_component>();
            type_reg.add_alias("test_async_component", type_reg.type_info(comp_idx).name);

            type_reg.ensure<tempest::scene::scene_inactive_tag>();
            type_reg.ensure<tempest::scene::scene_entity_component>();
            type_reg.ensure<tempest::ecs::self_component>();
        }
    };

    auto generate_scene_json(size_t entity_count) -> tempest::string
    {
        auto result = tempest::string{R"({"version": 1, "entities": [)"};
        for (auto i = 0U; i < entity_count; ++i)
        {
            if (i > 0U)
            {
                result.push_back(',');
            }
            result += tempest::format(
                R"({{"id": "{:#018x}", "name": "Entity_{}", "components": {{"test_async_component": {{"value": {}, "scale": 1.0}}}}}})",
                i + 1U, i, i);
        }
        result += R"(]})";
        return result;
    }
} // namespace

//==============================================================================
// Asynchronous Time-Sliced Staging & Commit Pipeline Tests
//==============================================================================

/// @brief Verifies that a 1,000-entity scene commits in increments of 100 entities across exactly 10 consecutive ticks,
///        transitioning to ready on the 10th tick.
TEST(scene_manager_async_test, async_time_sliced_commit)
{
    // Setup: Test context and 1,000-entity scene document with 100 entities/frame budget
    auto ctx = async_test_context{};
    const auto scene_json = generate_scene_json(1000);
    const auto options = tempest::scene::scene_async_load_options{
        .entities_per_frame = 100,
        .mode = tempest::scene::scene_load_mode::single,
        .clear_first = false,
        .strict_asset_resolution = false,
    };

    // Act: Initiate asynchronous load
    auto handle = ctx.manager.load_from_string_async(scene_json, "TimeSlicedScene", options);

    // Assert: Initial handle state before ticking
    EXPECT_TRUE(handle.is_valid());
    EXPECT_FALSE(handle.is_ready());
    EXPECT_FALSE(handle.is_done());
    EXPECT_EQ(ctx.registry.size(), 0U);

    // Act & Assert: Execute exactly 10 consecutive ticks and verify progressive increments of 100 entities
    for (auto tick_index = 1U; tick_index <= 10U; ++tick_index)
    {
        ctx.manager.tick();

        const auto expected_count = tick_index * 100U;
        EXPECT_EQ(ctx.registry.size(), expected_count);

        if (tick_index < 10U)
        {
            EXPECT_EQ(handle.state(), tempest::scene::scene_load_state::committing);
            EXPECT_FALSE(handle.is_ready());
            EXPECT_FALSE(handle.is_done());
        }
        else
        {
            EXPECT_EQ(handle.state(), tempest::scene::scene_load_state::ready);
            EXPECT_TRUE(handle.is_ready());
            EXPECT_TRUE(handle.is_done());
            EXPECT_EQ(handle.progress(), 1.0F);
            EXPECT_TRUE(handle.scene().is_valid());
            EXPECT_TRUE(ctx.manager.is_valid(handle.scene()));
        }
    }
}

/// @brief Verifies that partially committed entities tagged with scene_inactive_tag remain invisible to queries
///        filtering out scene_inactive_tag until tick 10, when all 1,000 entities become visible simultaneously.
TEST(scene_manager_async_test, inactive_tag_suppression)
{
    // Setup: Test context and 1,000-entity scene document with 100 entities/frame budget
    auto ctx = async_test_context{};
    const auto scene_json = generate_scene_json(1000);
    const auto options = tempest::scene::scene_async_load_options{
        .entities_per_frame = 100,
    };

    auto count_visible_active_entities = [&]() -> size_t {
        auto count = size_t{0};
        for ([[maybe_unused]] const auto [self, comp] :
             ctx.registry.with<tempest::ecs::self_component, tempest::scene::scene_entity_component>()
                 .without<tempest::scene::scene_inactive_tag>())
        {
            ++count;
        }
        return count;
    };

    // Act: Initiate asynchronous load
    auto handle = ctx.manager.load_from_string_async(scene_json, "SuppressionScene", options);
    EXPECT_TRUE(handle.is_valid());

    // Act & Assert: For ticks 1 through 9, registry has entities but 0 are visible without scene_inactive_tag
    for (auto tick_index = 1U; tick_index <= 9U; ++tick_index)
    {
        ctx.manager.tick();

        EXPECT_EQ(ctx.registry.size(), tick_index * 100U);
        EXPECT_EQ(count_visible_active_entities(), 0U);
        EXPECT_FALSE(handle.is_ready());
    }

    // Act & Assert: On tick 10, all 1,000 entities have scene_inactive_tag stripped and become visible
    ctx.manager.tick();

    EXPECT_EQ(ctx.registry.size(), 1000U);
    EXPECT_EQ(count_visible_active_entities(), 1000U);
    EXPECT_TRUE(handle.is_ready());
}

/// @brief Verifies that progress strictly monotonically increases across all stages from 0.0 to 1.0.
TEST(scene_manager_async_test, monotonic_progress)
{
    // Setup: Test context and 1,000-entity scene document
    auto ctx = async_test_context{};
    const auto scene_json = generate_scene_json(1000);
    const auto options = tempest::scene::scene_async_load_options{
        .entities_per_frame = 100,
    };

    // Act: Initiate asynchronous load
    auto handle = ctx.manager.load_from_string_async(scene_json, "MonotonicProgressScene", options);
    EXPECT_TRUE(handle.is_valid());

    auto previous_progress = handle.progress();
    EXPECT_GE(previous_progress, 0.0F);
    EXPECT_LE(previous_progress, 1.0F);

    // Act & Assert: Verify that every consecutive tick strictly increases progress until 1.0
    for (auto tick_index = 1U; tick_index <= 10U; ++tick_index)
    {
        ctx.manager.tick();

        const auto current_progress = handle.progress();
        EXPECT_GT(current_progress, previous_progress);
        EXPECT_LE(current_progress, 1.0F);

        previous_progress = current_progress;
    }

    EXPECT_EQ(handle.progress(), 1.0F);
    EXPECT_TRUE(handle.is_ready());
}

/// @brief Verifies that cancelling an in-flight async load immediately destroys all partially committed entities,
///        restores the registry to its initial entity count with 0 remnants, and causes subsequent ticks to be no-ops.
TEST(scene_manager_async_test, immediate_cancellation)
{
    // Setup: Test context and 1,000-entity scene document with 100 entities/frame budget
    auto ctx = async_test_context{};
    const auto scene_json = generate_scene_json(1000);
    const auto initial_entity_count = ctx.registry.size();
    EXPECT_EQ(initial_entity_count, 0U);

    const auto options = tempest::scene::scene_async_load_options{
        .entities_per_frame = 100,
    };

    // Act: Initiate load and tick 3 times (partial commit of 300 entities)
    auto handle = ctx.manager.load_from_string_async(scene_json, "CancellationScene", options);
    EXPECT_TRUE(handle.is_valid());

    for (auto tick_index = 1U; tick_index <= 3U; ++tick_index)
    {
        ctx.manager.tick();
    }

    EXPECT_EQ(ctx.registry.size(), 300U);
    EXPECT_EQ(handle.state(), tempest::scene::scene_load_state::committing);

    // Act: Cancel load
    handle.cancel();

    // Assert: Handle reflects cancellation and all 300 entities are immediately destroyed
    EXPECT_TRUE(handle.is_cancelled());
    EXPECT_TRUE(handle.is_done());
    EXPECT_FALSE(handle.is_ready());
    EXPECT_FALSE(handle.is_failed());
    EXPECT_EQ(ctx.registry.size(), initial_entity_count);

    // Act & Assert: Subsequent ticks do nothing and entity count remains clean
    for (auto extra_tick = 0U; extra_tick < 5U; ++extra_tick)
    {
        ctx.manager.tick();
        EXPECT_EQ(ctx.registry.size(), initial_entity_count);
        EXPECT_TRUE(handle.is_cancelled());
    }
}

/// @brief Verifies single-mode atomic swap: existing scene entities remain active during time-sliced loading
///        of a replacement scene and are swapped out in a single frame when the new scene completes.
TEST(scene_manager_async_test, single_mode_atomic_swap)
{
    // Setup: Load initial active scene A with 50 entities
    auto ctx = async_test_context{};
    const auto scene_a_json = generate_scene_json(50);
    auto scene_a_res = ctx.manager.load_from_string_sync(scene_a_json, "SceneA");
    ASSERT_TRUE(scene_a_res.has_value());
    EXPECT_EQ(ctx.registry.size(), 50U);
    EXPECT_EQ(ctx.manager.get_active_scene(), *scene_a_res);

    // Act: Load replacement scene B with 100 entities in 2 batches of 50
    const auto scene_b_json = generate_scene_json(100);
    const auto options = tempest::scene::scene_async_load_options{
        .entities_per_frame = 50,
        .mode = tempest::scene::scene_load_mode::single,
        .clear_first = false,
    };

    auto handle_b = ctx.manager.load_from_string_async(scene_b_json, "SceneB", options);
    EXPECT_TRUE(handle_b.is_valid());

    // Act: Tick 1 - 50 entities of Scene B committed with inactive tag, Scene A remains active
    ctx.manager.tick();
    EXPECT_EQ(ctx.registry.size(), 100U); // 50 from Scene A + 50 from Scene B
    EXPECT_TRUE(ctx.manager.is_valid(*scene_a_res));

    // Act: Tick 2 - Scene B completes commit, atomic swap unloads Scene A and activates Scene B
    ctx.manager.tick();
    EXPECT_TRUE(handle_b.is_ready());
    EXPECT_FALSE(ctx.manager.is_valid(*scene_a_res)); // Scene A was unloaded
    EXPECT_TRUE(ctx.manager.is_valid(handle_b.scene())); // Scene B is valid
    EXPECT_EQ(ctx.registry.size(), 100U); // Only 100 entities of Scene B remain
    EXPECT_EQ(ctx.manager.get_active_scene(), handle_b.scene());
}
