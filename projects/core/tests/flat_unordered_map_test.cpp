#include <gtest/gtest.h>

#include <tempest/flat_unordered_map.hpp>

TEST(metadata_group, any_empty_none_empty)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = i;
    }

    EXPECT_FALSE(group.any_empty());
}

TEST(metadata_group, any_empty_one_empty)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = i;
    }

    group.entries[0] = tempest::core::detail::empty_entry;

    EXPECT_TRUE(group.any_empty());
}

TEST(metadata_group, any_empty_all_empty)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = tempest::core::detail::empty_entry;
    }

    EXPECT_TRUE(group.any_empty());
}

TEST(metadata_group, any_empty_one_deleted)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = i;
    }

    group.entries[0] = tempest::core::detail::deleted_entry;

    EXPECT_FALSE(group.any_empty());
}

TEST(metadata_group, any_empty_all_deleted)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = tempest::core::detail::deleted_entry;
    }

    EXPECT_FALSE(group.any_empty());
}

TEST(metadata_group, any_empty_or_deleted_one_empty)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = i;
    }

    group.entries[0] = tempest::core::detail::empty_entry;

    EXPECT_TRUE(group.any_empty_or_deleted());
}

TEST(metadata_group, any_empty_or_deleted_all_empty)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = tempest::core::detail::empty_entry;
    }

    EXPECT_TRUE(group.any_empty_or_deleted());
}

TEST(metadata_group, any_empty_or_deleted_one_deleted)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = i;
    }

    group.entries[0] = tempest::core::detail::deleted_entry;

    EXPECT_TRUE(group.any_empty_or_deleted());
}

TEST(metadata_group, any_empty_or_deleted_all_deleted)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = tempest::core::detail::deleted_entry;
    }

    EXPECT_TRUE(group.any_empty_or_deleted());
}

TEST(metadata_group, any_empty_or_deleted_none)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = i;
    }

    EXPECT_FALSE(group.any_empty_or_deleted());
}

TEST(metadata_group, match_byte_none)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = i;
    }

    EXPECT_EQ(group.match_byte(tempest::core::detail::empty_entry), 0);
}

TEST(metadata_group, match_byte_one)
{
    tempest::core::detail::metadata_group group;

    group.entries[10] = 1;

    // set 10th bit
    std::uint16_t expected = 1 << 10;

    EXPECT_EQ(group.match_byte(1), expected);
}

TEST(metadata_group, match_byte_alternates)
{
    tempest::core::detail::metadata_group group;
    group.entries[0] = 1;
    group.entries[2] = 1;
    group.entries[4] = 1;
    group.entries[6] = 1;
    group.entries[8] = 1;
    group.entries[10] = 1;
    group.entries[12] = 1;
    group.entries[14] = 1;

    std::uint16_t entries = 0b0101010101010101;

    EXPECT_EQ(group.match_byte(1), entries);
}

TEST(metadata_group, match_byte_all)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = 1;
    }

    std::uint16_t expected = 0xFFFF;

    EXPECT_EQ(group.match_byte(1), expected);
}

TEST(flat_unordered_map, default_constructor)
{
    tempest::core::flat_unordered_map<int, int> map;

    EXPECT_EQ(map.size(), 0);
    EXPECT_TRUE(map.empty());
}

TEST(flat_unordered_map, insert_less_than_page_size)
{
    tempest::core::flat_unordered_map<int, int> map;

    for (int i = 0; i < 10; ++i)
    {
        map.insert({i, i});
    }

    EXPECT_EQ(map.size(), 10);
    EXPECT_FALSE(map.empty());

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(map[i], i);
    }
}