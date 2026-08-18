#include <gtest/gtest.h>

#include <tempest/flat_unordered_map.hpp>
#include <tempest/render_graph/types.hpp>

namespace tempest::render_graph
{
    TEST(types_test, texture_handle_properties)
    {
        const auto default_handle = rg_texture_id{};
        EXPECT_FALSE(default_handle.is_valid());
        EXPECT_FALSE(static_cast<bool>(default_handle));

        const auto handle_v0 = rg_texture_id{
            .id = 42,
            .version = 0,
        };
        EXPECT_TRUE(handle_v0.is_valid());
        EXPECT_TRUE(static_cast<bool>(handle_v0));
        EXPECT_EQ(handle_v0.id, 42U);
        EXPECT_EQ(handle_v0.version, 0U);

        const auto handle_v1 = handle_v0.next_version();
        EXPECT_TRUE(handle_v1.is_valid());
        EXPECT_TRUE(static_cast<bool>(handle_v1));
        EXPECT_EQ(handle_v1.id, 42U);
        EXPECT_EQ(handle_v1.version, 1U);
        EXPECT_NE(handle_v0, handle_v1);

        const auto handle_v0_dup = rg_texture_id{
            .id = 42,
            .version = 0,
        };
        EXPECT_EQ(handle_v0, handle_v0_dup);
    }

    TEST(types_test, buffer_handle_properties)
    {
        const auto default_handle = rg_buffer_id{};
        EXPECT_FALSE(default_handle.is_valid());
        EXPECT_FALSE(static_cast<bool>(default_handle));

        const auto handle_v0 = rg_buffer_id{
            .id = 10,
            .version = 0,
        };
        EXPECT_TRUE(handle_v0.is_valid());
        EXPECT_TRUE(static_cast<bool>(handle_v0));

        const auto handle_v1 = handle_v0.next_version();
        EXPECT_EQ(handle_v1.id, 10U);
        EXPECT_EQ(handle_v1.version, 1U);
        EXPECT_NE(handle_v0, handle_v1);
    }

    TEST(types_test, handle_hashing_and_map)
    {
        auto map = flat_unordered_map<rg_texture_id, int>{};

        const auto t1 = rg_texture_id{.id = 1, .version = 0};
        const auto t2 = rg_texture_id{.id = 1, .version = 1};
        const auto t3 = rg_texture_id{.id = 2, .version = 0};

        map[t1] = 100;
        map[t2] = 200;
        map[t3] = 300;

        EXPECT_EQ(map.size(), 3U);
        EXPECT_EQ(map[t1], 100);
        EXPECT_EQ(map[t2], 200);
        EXPECT_EQ(map[t3], 300);
    }

    TEST(types_test, texture_size_resolution)
    {
        // Absolute size
        const auto abs_size = rg_texture_size::absolute(1920, 1080);
        const auto resolved_abs = abs_size.evaluate(1280, 720);
        EXPECT_EQ(resolved_abs.width, 1920U);
        EXPECT_EQ(resolved_abs.height, 1080U);
        EXPECT_EQ(resolved_abs.depth, 1U);

        // Surface relative (1.0x)
        const auto rel_full = rg_texture_size::surface_relative(1.0F, 1.0F);
        const auto resolved_full = rel_full.evaluate(1920, 1080);
        EXPECT_EQ(resolved_full.width, 1920U);
        EXPECT_EQ(resolved_full.height, 1080U);

        // Surface relative (0.5x half-res)
        const auto rel_half = rg_texture_size::surface_relative(0.5F, 0.5F);
        const auto resolved_half = rel_half.evaluate(1920, 1080);
        EXPECT_EQ(resolved_half.width, 960U);
        EXPECT_EQ(resolved_half.height, 540U);
    }
} // namespace tempest::render_graph
