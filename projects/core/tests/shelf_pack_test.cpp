#include <tempest/shelf_pack.hpp>

#include <gtest/gtest.h>

TEST(shelf_pack_allocator, empty)
{
    tempest::math::vec2<uint32_t> extent{128, 128};
    tempest::shelf_pack_allocator allocator(extent, {});

    EXPECT_TRUE(allocator.empty());
    EXPECT_EQ(0, allocator.used_memory());
    EXPECT_EQ(extent.x * extent.y, allocator.free_memory());
}

TEST(shelf_pack_allocator, simple)
{
    tempest::math::vec2<uint32_t> extent{2048, 2048};
    tempest::shelf_pack_allocator allocator(extent, {
                                                        .alignment = {16, 16},
                                                        .column_count = 2,
                                                    });

    EXPECT_TRUE(allocator.empty());
    EXPECT_EQ(0, allocator.used_memory());

    const auto a1 = allocator.allocate({128, 128});
    const auto a2 = allocator.allocate({128, 128});
    const auto a3 = allocator.allocate({128, 128});

    EXPECT_TRUE(a1.has_value());
    EXPECT_TRUE(a2.has_value());
    EXPECT_TRUE(a3.has_value());
    EXPECT_FALSE(allocator.empty());

    EXPECT_GE(allocator.used_memory(), 128 * 128 * 3);

    // Check that the allocations are at least the requested size.
    EXPECT_GE(a1->extent.x, 128);
    EXPECT_GE(a1->extent.y, 128);
    EXPECT_GE(a2->extent.x, 128);
    EXPECT_GE(a2->extent.y, 128);
    EXPECT_GE(a3->extent.x, 128);
    EXPECT_GE(a3->extent.y, 128);

    // Check that the allocations are at most the shelf size.
    EXPECT_LE(a1->extent.x, 1024);
    EXPECT_LE(a1->extent.y, 1024);
    EXPECT_LE(a2->extent.x, 1024);
    EXPECT_LE(a2->extent.y, 1024);
    EXPECT_LE(a3->extent.x, 1024);
    EXPECT_LE(a3->extent.y, 1024);

    // Check that the allocations are aligned.
    EXPECT_EQ(0, a1->position.x % 16);
    EXPECT_EQ(0, a1->position.y % 16);
    EXPECT_EQ(0, a2->position.x % 16);
    EXPECT_EQ(0, a2->position.y % 16);
    EXPECT_EQ(0, a3->position.x % 16);
    EXPECT_EQ(0, a3->position.y % 16);

    // Check that the allocations do not overlap.
    EXPECT_EQ(a1->position.x + a1->extent.x, a2->position.x);
    EXPECT_EQ(a2->position.x + a2->extent.x, a3->position.x);

    // Release the allocations.
    allocator.deallocate(a1->id);
    allocator.deallocate(a2->id);
    allocator.deallocate(a3->id);

    EXPECT_EQ(0, allocator.used_memory());
}

TEST(shelf_pack_allocator, shadow_map_test)
{
    // Build a large shelf pack allocator.
    tempest::math::vec2<uint32_t> extent{8192, 8192};
    tempest::shelf_pack_allocator allocator(extent, {
                                                        .alignment = {16, 16},
                                                        .column_count = 2,
                                                    });
    
    // Allocate 3 cascades, largest at 2048x2048, smallest at 512x512.
    const auto a1 = allocator.allocate({2048, 2048});
    const auto a2 = allocator.allocate({1024, 1024});
    const auto a3 = allocator.allocate({512, 512});

    // Check that the allocations are valid.
    EXPECT_TRUE(a1.has_value());
    EXPECT_TRUE(a2.has_value());
    EXPECT_TRUE(a3.has_value());

    // Check that the allocations are at least the requested size.
    EXPECT_GE(a1->extent.x, 2048);
    EXPECT_GE(a1->extent.y, 2048);
    EXPECT_GE(a2->extent.x, 1024);
    EXPECT_GE(a2->extent.y, 1024);
    EXPECT_GE(a3->extent.x, 512);
    EXPECT_GE(a3->extent.y, 512);

    // Check that the allocations are at most the shelf size.
    EXPECT_LE(a1->extent.x, 4096);
    EXPECT_LE(a1->extent.y, 4096);
    EXPECT_LE(a2->extent.x, 4096);
    EXPECT_LE(a2->extent.y, 4096);
    EXPECT_LE(a3->extent.x, 4096);
    EXPECT_LE(a3->extent.y, 4096);

    // Check that the allocations are aligned.
    EXPECT_EQ(0, a1->position.x % 16);
    EXPECT_EQ(0, a1->position.y % 16);
    EXPECT_EQ(0, a2->position.x % 16);
    EXPECT_EQ(0, a2->position.y % 16);
    EXPECT_EQ(0, a3->position.x % 16);
    EXPECT_EQ(0, a3->position.y % 16);

    // Check that the allocations do not overlap via AABB test
    EXPECT_FALSE((a1->position.x < a2->position.x + a2->extent.x &&
        a1->position.x + a1->extent.x > a2->position.x &&
        a1->position.y < a2->position.y + a2->extent.y &&
        a1->position.y + a1->extent.y > a2->position.y)); // a1 and a2 overlap
    EXPECT_FALSE((a1->position.x < a3->position.x + a3->extent.x &&
        a1->position.x + a1->extent.x > a3->position.x &&
        a1->position.y < a3->position.y + a3->extent.y &&
        a1->position.y + a1->extent.y > a3->position.y)); // a1 and a3 overlap
    EXPECT_FALSE((a2->position.x < a3->position.x + a3->extent.x &&
        a2->position.x + a2->extent.x > a3->position.x &&
        a2->position.y < a3->position.y + a3->extent.y &&
        a2->position.y + a2->extent.y > a3->position.y)); // a2 and a3 overlap

    // Release the allocations.
    allocator.deallocate(a1->id);
    allocator.deallocate(a2->id);
    allocator.deallocate(a3->id);

    // Check that the allocator is empty.
    EXPECT_TRUE(allocator.empty());
}

TEST(shelf_pack_allocator, clear)
{
    tempest::math::vec2<uint32_t> extent{2048, 2048};
    tempest::shelf_pack_allocator allocator(extent, {
                                                        .alignment = {16, 16},
                                                        .column_count = 2,
                                                    });

    const auto a1 = allocator.allocate({128, 128});
    const auto a2 = allocator.allocate({128, 128});
    const auto a3 = allocator.allocate({128, 128});

    EXPECT_TRUE(a1.has_value());
    EXPECT_TRUE(a2.has_value());
    EXPECT_TRUE(a3.has_value());
    EXPECT_FALSE(allocator.empty());

    allocator.clear();

    EXPECT_TRUE(allocator.empty());
    EXPECT_EQ(0, allocator.used_memory());
    EXPECT_EQ(extent.x * extent.y, allocator.free_memory());
}
