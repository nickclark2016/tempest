#include <gtest/gtest.h>
#include <tempest/render_system/shadow_atlas_math.hpp>

// ============================================================================
// Shadow Atlas Math Unit Tests
// ============================================================================

namespace tempest::render_system::tests
{
    // ------------------------------------------------------------------------
    // Cascade Capacity Calculations
    // ------------------------------------------------------------------------

    /// @brief Verifies that compute_max_cascades_for_resolution correctly calculates the maximum
    ///        number of padded tiles that fit on both axes of the atlas.
    TEST(shadow_atlas_math_tests, compute_max_cascades_for_resolution)
    {
        // 1. Act & Assert: Padded tiles fitting along axes
        // 4096 cascade with 4px padding (padded = 4104) on 16384 atlas -> floor(16384/4104) = 3 -> 3*3 = 9
        EXPECT_EQ(compute_max_cascades_for_resolution(4096, 16384, 4), 9U);

        // 4096 cascade on 8192 atlas -> floor(8192/4104) = 1 -> 1*1 = 1
        EXPECT_EQ(compute_max_cascades_for_resolution(4096, 8192, 4), 1U);

        // 2048 cascade on 8192 atlas -> floor(8192/2056) = 3 -> 3*3 = 9
        EXPECT_EQ(compute_max_cascades_for_resolution(2048, 8192, 4), 9U);

        // 1024 cascade on 4096 atlas -> floor(4096/1032) = 3 -> 3*3 = 9
        EXPECT_EQ(compute_max_cascades_for_resolution(1024, 4096, 4), 9U);

        // 2. Act & Assert: Edge cases (zero dimensions, tile exceeds atlas)
        EXPECT_EQ(compute_max_cascades_for_resolution(0, 8192, 4), 0U);
        EXPECT_EQ(compute_max_cascades_for_resolution(2048, 0, 4), 0U);
        EXPECT_EQ(compute_max_cascades_for_resolution(16384, 8192, 4), 0U);
    }

    // ------------------------------------------------------------------------
    // Cascade Resolution Clamping Calculations
    // ------------------------------------------------------------------------

    /// @brief Verifies that compute_max_cascade_resolution calculates the exact maximum available
    ///        tile resolution for a given cascade count and atlas limit without floor-rounding to powers of two.
    TEST(shadow_atlas_math_tests, compute_max_cascade_resolution)
    {
        // 1. Act & Assert: 4 cascades (2x2 grid)
        // 16384 limit -> tile=8192, avail=8192 - 8 = 8184
        EXPECT_EQ(compute_max_cascade_resolution(4, 16384, 4), 8184U);

        // 8192 limit -> tile=4096, avail=4096 - 8 = 4088 (exact available resolution, allows 4x4088 in 8K)
        EXPECT_EQ(compute_max_cascade_resolution(4, 8192, 4), 4088U);

        // 4096 limit -> tile=2048, avail=2048 - 8 = 2040
        EXPECT_EQ(compute_max_cascade_resolution(4, 4096, 4), 2040U);

        // 2. Act & Assert: 1 cascade (1x1 grid)
        // 8192 limit -> tile=8192, avail=8192 - 8 = 8184
        EXPECT_EQ(compute_max_cascade_resolution(1, 8192, 4), 8184U);

        // 4096 limit -> tile=4096, avail=4096 - 8 = 4088
        EXPECT_EQ(compute_max_cascade_resolution(1, 4096, 4), 4088U);

        // 3. Act & Assert: Edge cases
        EXPECT_EQ(compute_max_cascade_resolution(0, 8192, 4), 0U);
        EXPECT_EQ(compute_max_cascade_resolution(4, 0, 4), 0U);
    }

    // ------------------------------------------------------------------------
    // Atlas Planning
    // ------------------------------------------------------------------------

    /// @brief Verifies that calculate_directional_shadow_atlas_plan generates atlas dimensions
    ///        without clamping when the requested cascade resolutions fit within the maximum atlas limit.
    TEST(shadow_atlas_math_tests, calculate_directional_shadow_atlas_plan_fits_without_clamping)
    {
        // 1. Setup & Act: 4 cascades of 4096 on 16384 limit
        // 2x2 of 4104 = 8208 -> ceil_pow2 = 16384
        const auto plan_4k = calculate_directional_shadow_atlas_plan(4096, 4, 16384, 4);
        EXPECT_EQ(plan_4k.atlas_size.x, 16384U);
        EXPECT_EQ(plan_4k.atlas_size.y, 16384U);
        EXPECT_EQ(plan_4k.effective_cascade_resolution, 4096U);
        EXPECT_FALSE(plan_4k.was_clamped);

        // 2. Setup & Act: 4 cascades of 2048 on 8192 limit
        // 2x2 of 2056 = 4112 -> ceil_pow2 = 8192
        const auto plan_2k = calculate_directional_shadow_atlas_plan(2048, 4, 8192, 4);
        EXPECT_EQ(plan_2k.atlas_size.x, 8192U);
        EXPECT_EQ(plan_2k.atlas_size.y, 8192U);
        EXPECT_EQ(plan_2k.effective_cascade_resolution, 2048U);
        EXPECT_FALSE(plan_2k.was_clamped);

        // 3. Setup & Act: 4 cascades of 1024 on 8192 limit
        // 2x2 of 1032 = 2064 -> ceil_pow2 = 4096
        const auto plan_1k = calculate_directional_shadow_atlas_plan(1024, 4, 8192, 4);
        EXPECT_EQ(plan_1k.atlas_size.x, 4096U);
        EXPECT_EQ(plan_1k.atlas_size.y, 4096U);
        EXPECT_EQ(plan_1k.effective_cascade_resolution, 1024U);
        EXPECT_FALSE(plan_1k.was_clamped);
    }

    /// @brief Verifies that requesting 4 cascades of 4096 on an 8192 limit clamps each cascade
    ///        to 4088 so all 4 cascades fit with padding in an 8192x8192 atlas, avoiding 16K allocation.
    TEST(shadow_atlas_math_tests, calculate_directional_shadow_atlas_plan_clamps_on_device_limit)
    {
        // 1. Setup: 4 cascades of 4096 with 4px padding targeting 8192 max limit
        // 2. Act: Calculate plan
        // 2x2 padded tiles: req_w = 2 * (4096 + 8) = 8208 > 8192.
        // Clamps cascade resolution to 4088. Padded tile = 4088 + 8 = 4096.
        // Final atlas size: 2 * 4096 = 8192x8192.
        const auto plan = calculate_directional_shadow_atlas_plan(4096, 4, 8192, 4);

        // 3. Assert: Atlas fits exactly into 8K with 4088 effective resolution
        EXPECT_EQ(plan.atlas_size.x, 8192U);
        EXPECT_EQ(plan.atlas_size.y, 8192U);
        EXPECT_EQ(plan.effective_cascade_resolution, 4088U);
        EXPECT_TRUE(plan.was_clamped);
    }

    /// @brief Verifies fallback behavior when cascade count or resolution is zero.
    TEST(shadow_atlas_math_tests, calculate_directional_shadow_atlas_plan_edge_cases)
    {
        // 1. Act: Zero cascades
        const auto plan_zero = calculate_directional_shadow_atlas_plan(2048, 0, 8192, 4);
        EXPECT_EQ(plan_zero.atlas_size.x, 512U);
        EXPECT_EQ(plan_zero.atlas_size.y, 512U);
        EXPECT_EQ(plan_zero.effective_cascade_resolution, 0U);
        EXPECT_FALSE(plan_zero.was_clamped);

        // 2. Act: Zero resolution
        const auto plan_zero_res = calculate_directional_shadow_atlas_plan(0, 4, 8192, 4);
        EXPECT_EQ(plan_zero_res.atlas_size.x, 512U);
        EXPECT_EQ(plan_zero_res.atlas_size.y, 512U);
        EXPECT_EQ(plan_zero_res.effective_cascade_resolution, 0U);
        EXPECT_FALSE(plan_zero_res.was_clamped);
    }
} // namespace tempest::render_system::tests
