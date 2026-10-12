#include <gtest/gtest.h>

#include <tempest/archetype.hpp>
#include <tempest/int.hpp>
#include <tempest/vector.hpp>

namespace
{
    struct position_component
    {
        float x{0.0f};
        float y{0.0f};
    };

    struct velocity_component
    {
        float vx{0.0f};
        float vy{0.0f};
    };

    struct health_component
    {
        int32_t hp{0};
    };

    struct armor_component
    {
        int32_t value{0};
    };

    struct disabled_tag
    {
    };

    struct frozen_tag
    {
    };

    struct hidden_tag
    {
    };

    struct dead_tag
    {
    };

    struct unrelated_tag
    {
    };
} // namespace

// =========================================================================
// SECTION: ECS View Exclusion Filter Tests
// =========================================================================

/// @brief Verifies that entities possessing an excluded tag are omitted from view iteration.
TEST(archetype_without, without_single_tag)
{
    // 1. Setup: Register components and create entities with and without disabled_tag
    auto event_reg = tempest::event::event_registry{};
    auto type_reg = tempest::ecs::component_type_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{event_reg, type_reg};

    const auto e1 = reg.create<position_component>();
    reg.replace(e1, position_component{.x = 1.0f, .y = 2.0f});

    const auto e2 = reg.create<position_component, disabled_tag>();
    reg.replace(e2, position_component{.x = 3.0f, .y = 4.0f});

    const auto e3 = reg.create<position_component>();
    reg.replace(e3, position_component{.x = 5.0f, .y = 6.0f});

    // 2. Act: Query view with position_component and without disabled_tag
    auto matched_positions = tempest::vector<position_component>{};
    for (const auto& [pos] : reg.with<position_component>().without<disabled_tag>())
    {
        matched_positions.push_back(pos);
    }

    // Also count all entities without exclusion
    auto all_count = 0;
    for (const auto& [pos] : reg.with<position_component>())
    {
        EXPECT_GT(pos.x, 0.0f);
        ++all_count;
    }

    // 3. Assert: Excluded entity is omitted while non-tagged entities match
    EXPECT_EQ(all_count, 3);
    EXPECT_EQ(matched_positions.size(), 2);
    EXPECT_FLOAT_EQ(matched_positions[0].x, 1.0f);
    EXPECT_FLOAT_EQ(matched_positions[0].y, 2.0f);
    EXPECT_FLOAT_EQ(matched_positions[1].x, 5.0f);
    EXPECT_FLOAT_EQ(matched_positions[1].y, 6.0f);
}

/// @brief Verifies that entities possessing either tag in a multi-tag exclusion or chained without() are omitted.
TEST(archetype_without, without_multiple_tags)
{
    // 1. Setup: Create entities across multiple archetypes with combinations of frozen_tag and hidden_tag
    auto event_reg = tempest::event::event_registry{};
    auto type_reg = tempest::ecs::component_type_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{event_reg, type_reg};

    // e1: only velocity
    const auto e1 = reg.create<velocity_component>();
    reg.replace(e1, velocity_component{.vx = 1.0f, .vy = 0.0f});

    // e2: velocity + frozen_tag
    const auto e2 = reg.create<velocity_component, frozen_tag>();
    reg.replace(e2, velocity_component{.vx = 0.0f, .vy = 1.0f});

    // e3: velocity + hidden_tag
    const auto e3 = reg.create<velocity_component, hidden_tag>();
    reg.replace(e3, velocity_component{.vx = 1.0f, .vy = 1.0f});

    // e4: velocity + frozen_tag + hidden_tag
    const auto e4 = reg.create<velocity_component, frozen_tag, hidden_tag>();
    reg.replace(e4, velocity_component{.vx = 2.0f, .vy = 2.0f});

    // e5: only velocity
    const auto e5 = reg.create<velocity_component>();
    reg.replace(e5, velocity_component{.vx = 3.0f, .vy = 3.0f});

    // 2. Act: Iterate with variadic without<frozen_tag, hidden_tag>() and chained without().without()
    auto variadic_matches = tempest::vector<velocity_component>{};
    for (const auto& [vel] : reg.with<velocity_component>().without<frozen_tag, hidden_tag>())
    {
        variadic_matches.push_back(vel);
    }

    auto chained_matches = tempest::vector<velocity_component>{};
    for (const auto& [vel] : reg.with<velocity_component>().without<frozen_tag>().without<hidden_tag>())
    {
        chained_matches.push_back(vel);
    }

    // 3. Assert: Both variadic and chained forms produce identical results omitting e2, e3, and e4
    EXPECT_EQ(variadic_matches.size(), 2);
    EXPECT_EQ(chained_matches.size(), 2);

    EXPECT_FLOAT_EQ(variadic_matches[0].vx, 1.0f);
    EXPECT_FLOAT_EQ(variadic_matches[1].vx, 3.0f);

    EXPECT_FLOAT_EQ(chained_matches[0].vx, 1.0f);
    EXPECT_FLOAT_EQ(chained_matches[1].vx, 3.0f);
}

/// @brief Verifies that exclusion filters do not affect unrelated components and unregistered tags don't exclude.
TEST(archetype_without, without_does_not_affect_unrelated)
{
    // 1. Setup: Create entities with health, armor, dead_tag, and unrelated archetypes
    auto event_reg = tempest::event::event_registry{};
    auto type_reg = tempest::ecs::component_type_registry{};
    auto reg = tempest::ecs::basic_archetype_registry{event_reg, type_reg};

    const auto e1 = reg.create<health_component>();
    reg.replace(e1, health_component{.hp = 100});

    const auto e2 = reg.create<health_component, armor_component>();
    reg.replace(e2, health_component{.hp = 80});
    reg.replace(e2, armor_component{.value = 50});

    const auto e3 = reg.create<health_component, dead_tag>();
    reg.replace(e3, health_component{.hp = 0});

    const auto e4 = reg.create<health_component, armor_component>();
    reg.replace(e4, health_component{.hp = 50});
    reg.replace(e4, armor_component{.value = 20});

    // 2. Act: Query with single component exclusion, multi-component with exclusion, and unused tag exclusion
    auto health_alive_count = 0;
    for (const auto& [h] : reg.with<health_component>().without<dead_tag>())
    {
        EXPECT_GE(h.hp, 0);
        ++health_alive_count;
    }

    auto health_armor_alive_count = 0;
    for (const auto& [h, a] : reg.with<health_component, armor_component>().without<dead_tag>())
    {
        EXPECT_GE(h.hp, 0);
        EXPECT_GE(a.value, 0);
        ++health_armor_alive_count;
    }

    auto unused_tag_count = 0;
    for (const auto& [h] : reg.with<health_component>().without<unrelated_tag>())
    {
        EXPECT_GE(h.hp, 0);
        ++unused_tag_count;
    }

    // 3. Assert: Dead entity omitted from health queries, armored entities intact, unrelated tag does not exclude
    EXPECT_EQ(health_alive_count, 3);
    EXPECT_EQ(health_armor_alive_count, 2);
    EXPECT_EQ(unused_tag_count, 4);
}
