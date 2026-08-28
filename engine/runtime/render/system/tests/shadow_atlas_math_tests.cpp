#include <gtest/gtest.h>
#include <tempest/render_system/shadow_atlas_math.hpp>

namespace tempest::render_system::tests
{
    TEST(shadow_atlas_math_tests, compute_max_cascades_for_resolution)
    {
        // 4096 cascade with 4px padding (padded = 4104) on 16384 atlas -> floor(16384/4104) = 3 -> 3*3 = 9
        EXPECT_EQ(compute_max_cascades_for_resolution(4096, 16384, 4), 9U);

        // 4096 cascade on 8192 atlas -> floor(8192/4104) = 1 -> 1*1 = 1
        EXPECT_EQ(compute_max_cascades_for_resolution(4096, 8192, 4), 1U);

        // 2048 cascade on 8192 atlas -> floor(8192/2056) = 3 -> 3*3 = 9
        EXPECT_EQ(compute_max_cascades_for_resolution(2048, 8192, 4), 9U);

        // 1024 cascade on 4096 atlas -> floor(4096/1032) = 3 -> 3*3 = 9
        EXPECT_EQ(compute_max_cascades_for_resolution(1024, 4096, 4), 9U);

        // Edge cases
        EXPECT_EQ(compute_max_cascades_for_resolution(0, 8192, 4), 0U);
        EXPECT_EQ(compute_max_cascades_for_resolution(2048, 0, 4), 0U);
        EXPECT_EQ(compute_max_cascades_for_resolution(16384, 8192, 4), 0U);
    }

    TEST(shadow_atlas_math_tests, compute_max_cascade_resolution)
    {
        // 4 cascades (2x2 grid) on 16384 atlas -> tile=8192, avail=8184 -> floor_pow2 = 4096
        EXPECT_EQ(compute_max_cascade_resolution(4, 16384, 4), 4096U);

        // 4 cascades (2x2 grid) on 8192 atlas -> tile=4096, avail=4088 -> floor_pow2 = 2048
        EXPECT_EQ(compute_max_cascade_resolution(4, 8192, 4), 2048U);

        // 4 cascades (2x2 grid) on 4096 atlas -> tile=2048, avail=2040 -> floor_pow2 = 1024
        EXPECT_EQ(compute_max_cascade_resolution(4, 4096, 4), 1024U);

        // 1 cascade (1x1 grid) on 8192 atlas -> tile=8192, avail=8184 -> floor_pow2 = 4096
        EXPECT_EQ(compute_max_cascade_resolution(1, 8192, 4), 4096U);

        // 1 cascade on 4096 atlas -> tile=4096, avail=4088 -> floor_pow2 = 2048
        EXPECT_EQ(compute_max_cascade_resolution(1, 4096, 4), 2048U);

        // Edge cases
        EXPECT_EQ(compute_max_cascade_resolution(0, 8192, 4), 0U);
        EXPECT_EQ(compute_max_cascade_resolution(4, 0, 4), 0U);
    }

    TEST(shadow_atlas_math_tests, calculate_directional_shadow_atlas_plan_fits_without_clamping)
    {
        // 4 cascades of 4096 on 16384 limit -> 2x2 of 4104 = 8208 -> ceil_pow2 = 16384
        const auto plan_4k = calculate_directional_shadow_atlas_plan(4096, 4, 16384, 4);
        EXPECT_EQ(plan_4k.atlas_size.x, 16384U);
        EXPECT_EQ(plan_4k.atlas_size.y, 16384U);
        EXPECT_EQ(plan_4k.effective_cascade_resolution, 4096U);
        EXPECT_FALSE(plan_4k.was_clamped);

        // 4 cascades of 2048 on 8192 limit -> 2x2 of 2056 = 4112 -> ceil_pow2 = 8192
        const auto plan_2k = calculate_directional_shadow_atlas_plan(2048, 4, 8192, 4);
        EXPECT_EQ(plan_2k.atlas_size.x, 8192U);
        EXPECT_EQ(plan_2k.atlas_size.y, 8192U);
        EXPECT_EQ(plan_2k.effective_cascade_resolution, 2048U);
        EXPECT_FALSE(plan_2k.was_clamped);

        // 4 cascades of 1024 on 8192 limit -> 2x2 of 1032 = 2064 -> ceil_pow2 = 4096
        const auto plan_1k = calculate_directional_shadow_atlas_plan(1024, 4, 8192, 4);
        EXPECT_EQ(plan_1k.atlas_size.x, 4096U);
        EXPECT_EQ(plan_1k.atlas_size.y, 4096U);
        EXPECT_EQ(plan_1k.effective_cascade_resolution, 1024U);
        EXPECT_FALSE(plan_1k.was_clamped);
    }

    TEST(shadow_atlas_math_tests, calculate_directional_shadow_atlas_plan_clamps_on_device_limit)
    {
        // 4 cascades of 4096 on 8192 limit:
        // Exceeds 8192 (req_w = 8208 > 8192).
        // Clamps to 2048 (max power of two that fits 4 cascades in 8192).
        // 2x2 of 2056 = 4112 -> ceil_pow2 = 8192.
        const auto plan = calculate_directional_shadow_atlas_plan(4096, 4, 8192, 4);
        EXPECT_EQ(plan.atlas_size.x, 8192U);
        EXPECT_EQ(plan.atlas_size.y, 8192U);
        EXPECT_EQ(plan.effective_cascade_resolution, 2048U);
        EXPECT_TRUE(plan.was_clamped);
    }

    TEST(shadow_atlas_math_tests, calculate_directional_shadow_atlas_plan_edge_cases)
    {
        // Zero cascades
        const auto plan_zero = calculate_directional_shadow_atlas_plan(2048, 0, 8192, 4);
        EXPECT_EQ(plan_zero.atlas_size.x, 512U);
        EXPECT_EQ(plan_zero.atlas_size.y, 512U);
        EXPECT_EQ(plan_zero.effective_cascade_resolution, 0U);
        EXPECT_FALSE(plan_zero.was_clamped);

        // Zero resolution
        const auto plan_zero_res = calculate_directional_shadow_atlas_plan(0, 4, 8192, 4);
        EXPECT_EQ(plan_zero_res.atlas_size.x, 512U);
        EXPECT_EQ(plan_zero_res.atlas_size.y, 512U);
        EXPECT_EQ(plan_zero_res.effective_cascade_resolution, 0U);
        EXPECT_FALSE(plan_zero_res.was_clamped);
    }
} // namespace tempest::render_system::tests
