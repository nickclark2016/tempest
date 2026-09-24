#include <gtest/gtest.h>

#include <tempest/fixed_timestep_accumulator.hpp>
#include <tempest/tempest.hpp>

using tempest::engine_config;
using tempest::fixed_timestep_accumulator;
using tempest::standalone_engine_context;

// ============================================================================
// Section: Accumulator Invariants & Step Execution
// ============================================================================

/// @brief Tests that variable dt frames accumulate and execute exactly floor(sum(dt) / fixed_delta) ticks.
TEST(fixed_timestep_accumulator_test, variable_dt_exact_tick_count)
{
    // 1. Setup
    constexpr auto fixed_dt = 1.0F / 60.0F;
    auto accumulator = fixed_timestep_accumulator{fixed_dt, 0.1F};

    const auto frame_deltas = {0.008F, 0.016F, 0.033F, 0.016666F, 0.004F, 0.020F};
    auto total_time = 0.0F;
    auto executed_ticks = 0U;

    // 2. Act - Run 100 frames with cycling delta times
    for (auto frame_index = 0; frame_index < 100; ++frame_index)
    {
        for (const auto delta_time : frame_deltas)
        {
            total_time += delta_time;
            accumulator.step(delta_time, [&executed_ticks]([[maybe_unused]] float step_dt) {
                ++executed_ticks;
            });
        }
    }

    // 3. Assert - Ticks must equal floor(total_time / fixed_dt)
    const auto expected_ticks = static_cast<uint32_t>(total_time / fixed_dt);
    EXPECT_EQ(executed_ticks, expected_ticks);
    EXPECT_EQ(accumulator.total_ticks(), expected_ticks);
}

/// @brief Tests that hitches exceeding max_frame_delta are clamped to prevent spiral-of-death.
TEST(fixed_timestep_accumulator_test, hitch_clamping_prevents_spiral_of_death)
{
    // 1. Setup - 60 Hz simulation with 100ms max clamp
    constexpr auto fixed_dt = 1.0F / 60.0F;
    constexpr auto max_clamp = 0.1F;
    auto accumulator = fixed_timestep_accumulator{fixed_dt, max_clamp};

    // 2. Act - Simulate a single 500ms hitch
    auto ticks_during_hitch = 0U;
    accumulator.step(0.5F, [&ticks_during_hitch]([[maybe_unused]] float step_dt) {
        ++ticks_during_hitch;
    });

    // 3. Assert - 100ms clamp / (1/60s) = 6 ticks max, not 30 ticks
    const auto max_possible_ticks = static_cast<uint32_t>(max_clamp / fixed_dt);
    EXPECT_EQ(ticks_during_hitch, max_possible_ticks);
    EXPECT_EQ(ticks_during_hitch, 6U);
}

/// @brief Tests that residual alpha is strictly bounded in [0.0, 1.0] and progresses monotonically.
TEST(fixed_timestep_accumulator_test, alpha_bounds_and_monotonicity)
{
    // 1. Setup
    constexpr auto fixed_dt = 1.0F / 60.0F;
    auto accumulator = fixed_timestep_accumulator{fixed_dt, 0.1F};

    // 2. Act & Assert - Sub-tick increments must produce strictly increasing alpha
    auto previous_alpha = -1.0F;
    for (auto sub_step = 0; sub_step < 10; ++sub_step)
    {
        accumulator.accumulate(fixed_dt * 0.08F);
        const auto alpha = accumulator.alpha();

        EXPECT_GE(alpha, 0.0F);
        EXPECT_LT(alpha, 1.0F);
        EXPECT_GT(alpha, previous_alpha);

        previous_alpha = alpha;
    }
}

/// @brief Tests that time scaling correctly modulates tick execution rate and pause.
TEST(fixed_timestep_accumulator_test, time_scaling_and_pause)
{
    // 1. Setup
    constexpr auto fixed_dt = 1.0F / 60.0F;
    auto accumulator = fixed_timestep_accumulator{fixed_dt, 0.5F};

    // 2. Act - Paused (scale = 0)
    accumulator.set_time_scale(0.0F);
    auto paused_ticks = 0U;
    accumulator.step(0.1F, [&paused_ticks]([[maybe_unused]] float) { ++paused_ticks; });
    EXPECT_EQ(paused_ticks, 0U);
    EXPECT_FLOAT_EQ(accumulator.accumulated_time(), 0.0F);

    // 3. Act - Double speed (scale = 2.0)
    accumulator.set_time_scale(2.0F);
    auto fast_ticks = 0U;
    accumulator.step(fixed_dt, [&fast_ticks]([[maybe_unused]] float) { ++fast_ticks; });
    EXPECT_EQ(fast_ticks, 2U);
}

// ============================================================================
// Section: Standalone Engine Context Headless Execution
// ============================================================================

/// @brief Tests that standalone_engine_context in headless mode executes fixed ticks without windows or GPU.
TEST(fixed_timestep_accumulator_test, headless_engine_context_execution)
{
    // 1. Setup - Configure headless mode
    auto config = engine_config{
        .headless = true,
        .fixed_timestep = 1.0F / 60.0F,
        .max_frame_delta = 0.1F,
    };
    auto context = standalone_engine_context{config};

    auto fixed_ticks_counted = 0U;
    context.register_on_fixed_update_callback([&fixed_ticks_counted](auto& ctx, auto /*dt*/) {
        ++fixed_ticks_counted;
        if (fixed_ticks_counted >= 5)
        {
            ctx.request_close(true);
        }
    });

    // 2. Act - Run loop (should exit cleanly after 5 ticks)
    context.run();

    // 3. Assert
    EXPECT_GE(fixed_ticks_counted, 5U);
    EXPECT_TRUE(context.should_close());
}
