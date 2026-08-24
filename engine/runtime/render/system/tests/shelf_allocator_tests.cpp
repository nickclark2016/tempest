#include <gtest/gtest.h>

#include <tempest/limits.hpp>
#include <tempest/render_system/render_components.hpp>
#include <tempest/render_system/shelf_allocator.hpp>

namespace tempest::render_system::tests
{
    TEST(ShelfAllocatorTest, BasicSingleAllocation)
    {
        auto allocator = shelf_allocator{2048, 2048, 4};
        EXPECT_EQ(allocator.get_atlas_width(), 2048U);
        EXPECT_EQ(allocator.get_atlas_height(), 2048U);
        EXPECT_EQ(allocator.get_padding(), 4U);

        auto res = allocator.allocate(512, 512);
        ASSERT_TRUE(res.has_value());
        EXPECT_EQ(res->x, 4U);
        EXPECT_EQ(res->y, 4U);
        EXPECT_EQ(res->width, 512U);
        EXPECT_EQ(res->height, 512U);
    }

    TEST(ShelfAllocatorTest, MultipleAllocationsSameShelf)
    {
        auto allocator = shelf_allocator{1024, 1024, 4};

        // Padded width = 256 + 8 = 264
        auto res1 = allocator.allocate(256, 256);
        ASSERT_TRUE(res1.has_value());
        EXPECT_EQ(res1->x, 4U);
        EXPECT_EQ(res1->y, 4U);
        EXPECT_EQ(res1->width, 256U);
        EXPECT_EQ(res1->height, 256U);

        auto res2 = allocator.allocate(256, 256);
        ASSERT_TRUE(res2.has_value());
        EXPECT_EQ(res2->x, 268U); // 264 + 4
        EXPECT_EQ(res2->y, 4U);
        EXPECT_EQ(res2->width, 256U);
        EXPECT_EQ(res2->height, 256U);

        auto res3 = allocator.allocate(256, 256);
        ASSERT_TRUE(res3.has_value());
        EXPECT_EQ(res3->x, 532U); // 264 * 2 + 4
        EXPECT_EQ(res3->y, 4U);
        EXPECT_EQ(res3->width, 256U);
        EXPECT_EQ(res3->height, 256U);
    }

    TEST(ShelfAllocatorTest, ShelfWrapping)
    {
        auto allocator = shelf_allocator{1000, 1000, 4};

        // Padded = 600 + 8 = 608, 200 + 8 = 208
        auto res1 = allocator.allocate(600, 200);
        ASSERT_TRUE(res1.has_value());
        EXPECT_EQ(res1->x, 4U);
        EXPECT_EQ(res1->y, 4U);

        // Second 600 wide allocation cannot fit on the first shelf (608 + 608 > 1000)
        auto res2 = allocator.allocate(600, 200);
        ASSERT_TRUE(res2.has_value());
        EXPECT_EQ(res2->x, 4U);
        EXPECT_EQ(res2->y, 212U); // New shelf at y = 208, plus padding 4
    }

    TEST(ShelfAllocatorTest, PaddingSeparation)
    {
        const auto padding = 8U;
        auto allocator = shelf_allocator{512, 512, padding};

        auto res1 = allocator.allocate(100, 100);
        auto res2 = allocator.allocate(100, 100);
        ASSERT_TRUE(res1.has_value());
        ASSERT_TRUE(res2.has_value());

        // Right edge of res1 content: res1->x + res1->width = 8 + 100 = 108
        // Left edge of res2 content: res2->x = (100 + 16) + 8 = 124
        // Gap = 124 - 108 = 16 = 2 * padding
        EXPECT_EQ(res2->x - (res1->x + res1->width), 2 * padding);
    }

    TEST(ShelfAllocatorTest, ZeroSizedAllocation)
    {
        auto allocator = shelf_allocator{512, 512, 4};

        auto res1 = allocator.allocate(0, 100);
        ASSERT_FALSE(res1.has_value());
        EXPECT_EQ(res1.error(), allocation_error::zero_size);

        auto res2 = allocator.allocate(100, 0);
        ASSERT_FALSE(res2.has_value());
        EXPECT_EQ(res2.error(), allocation_error::zero_size);

        auto res3 = allocator.allocate(0, 0);
        ASSERT_FALSE(res3.has_value());
        EXPECT_EQ(res3.error(), allocation_error::zero_size);
    }

    TEST(ShelfAllocatorTest, AllocationTooLarge)
    {
        auto allocator = shelf_allocator{512, 512, 4};

        // Padded width 512 + 8 = 520 > 512
        auto res1 = allocator.allocate(512, 512);
        ASSERT_FALSE(res1.has_value());
        EXPECT_EQ(res1.error(), allocation_error::allocation_too_large);

        auto res2 = allocator.allocate(600, 100);
        ASSERT_FALSE(res2.has_value());
        EXPECT_EQ(res2.error(), allocation_error::allocation_too_large);

        auto res3 = allocator.allocate(100, 600);
        ASSERT_FALSE(res3.has_value());
        EXPECT_EQ(res3.error(), allocation_error::allocation_too_large);
    }

    TEST(ShelfAllocatorTest, OutOfSpace)
    {
        auto allocator = shelf_allocator{100, 100, 0};

        auto res1 = allocator.allocate(60, 60);
        ASSERT_TRUE(res1.has_value());

        // 60x60 fits in empty 100x100, but now shelf 0 has 40 width left and shelf 1 would start at y=60 with only 40 height left
        auto res2 = allocator.allocate(60, 60);
        ASSERT_FALSE(res2.has_value());
        EXPECT_EQ(res2.error(), allocation_error::out_of_space);
    }

    TEST(ShelfAllocatorTest, ResetPreservesDimensions)
    {
        auto allocator = shelf_allocator{100, 100, 0};

        auto res1 = allocator.allocate(60, 60);
        ASSERT_TRUE(res1.has_value());

        auto res2 = allocator.allocate(60, 60);
        ASSERT_FALSE(res2.has_value());

        allocator.reset();
        EXPECT_EQ(allocator.get_atlas_width(), 100U);
        EXPECT_EQ(allocator.get_atlas_height(), 100U);
        EXPECT_EQ(allocator.get_padding(), 0U);

        auto res3 = allocator.allocate(60, 60);
        ASSERT_TRUE(res3.has_value());
        EXPECT_EQ(res3->x, 0U);
        EXPECT_EQ(res3->y, 0U);
    }

    TEST(ShelfAllocatorTest, ResetWithNewDimensions)
    {
        auto allocator = shelf_allocator{512, 512, 4};

        allocator.reset(1024, 1024, 8);
        EXPECT_EQ(allocator.get_atlas_width(), 1024U);
        EXPECT_EQ(allocator.get_atlas_height(), 1024U);
        EXPECT_EQ(allocator.get_padding(), 8U);

        auto res = allocator.allocate(512, 512);
        ASSERT_TRUE(res.has_value());
        EXPECT_EQ(res->x, 8U);
        EXPECT_EQ(res->y, 8U);
    }

    TEST(ShelfAllocatorTest, ShadowCasterComponentDefaults)
    {
        auto comp = shadow_caster_component{};
        EXPECT_EQ(comp.resolution, 2048U);
        EXPECT_EQ(comp.num_cascades, 4U);
        EXPECT_FLOAT_EQ(comp.split_lambda, 0.5F);
        EXPECT_FLOAT_EQ(comp.max_shadow_distance, 200.0F);
        EXPECT_FLOAT_EQ(comp.normal_bias, 0.02F);
        EXPECT_FLOAT_EQ(comp.depth_bias, 0.005F);
        EXPECT_EQ(comp.priority, 0U);
    }

    TEST(ShelfAllocatorTest, IntegerOverflowGuards)
    {
        auto allocator = shelf_allocator{1024, 1024, 4};

        // uint32 max values
        const auto u32_max = numeric_limits<uint32_t>::max();
        auto res1 = allocator.allocate(u32_max, 512);
        ASSERT_FALSE(res1.has_value());
        EXPECT_EQ(res1.error(), allocation_error::allocation_too_large);

        auto res2 = allocator.allocate(512, u32_max);
        ASSERT_FALSE(res2.has_value());
        EXPECT_EQ(res2.error(), allocation_error::allocation_too_large);

        auto res3 = allocator.allocate(u32_max - 2, u32_max - 2);
        ASSERT_FALSE(res3.has_value());
        EXPECT_EQ(res3.error(), allocation_error::allocation_too_large);

        // Atlas smaller than border padding
        auto small_allocator = shelf_allocator{4, 4, 8}; // padding * 2 = 16 > 4
        auto res4 = small_allocator.allocate(2, 2);
        ASSERT_FALSE(res4.has_value());
        EXPECT_EQ(res4.error(), allocation_error::allocation_too_large);
    }
} // namespace tempest::render_system::tests
