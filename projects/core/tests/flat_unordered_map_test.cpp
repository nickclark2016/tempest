#include <gtest/gtest.h>

#include <tempest/flat_unordered_map.hpp>

#include <algorithm>
#include <utility>
#include <vector>

TEST(metadata_group, any_empty_none_empty)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = static_cast<tempest::core::detail::metadata_entry>(i);
    }

    EXPECT_FALSE(group.any_empty());
}

TEST(metadata_group, any_empty_one_empty)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = static_cast<tempest::core::detail::metadata_entry>(i);
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
        group.entries[i] = static_cast<tempest::core::detail::metadata_entry>(i);
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
        group.entries[i] = static_cast<tempest::core::detail::metadata_entry>(i);
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
        group.entries[i] = static_cast<tempest::core::detail::metadata_entry>(i);
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
        group.entries[i] = static_cast<tempest::core::detail::metadata_entry>(i);
    }

    EXPECT_FALSE(group.any_empty_or_deleted());
}

TEST(metadata_group, match_byte_none)
{
    tempest::core::detail::metadata_group group;

    for (std::size_t i = 0; i < tempest::core::detail::metadata_group::group_size; ++i)
    {
        group.entries[i] = static_cast<tempest::core::detail::metadata_entry>(i);
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

TEST(flat_unordered_map, insert_more_than_page_size)
{
    tempest::core::flat_unordered_map<int, int> map;

    for (int i = 0; i < 20; ++i)
    {
        map.insert({i, i});
    }

    EXPECT_EQ(map.size(), 20);
    EXPECT_FALSE(map.empty());

    for (int i = 0; i < 20; ++i)
    {
        EXPECT_EQ(map[i], i);
    }
}

TEST(flat_unordered_map, erase_value_that_exists_by_iterator)
{
    tempest::core::flat_unordered_map<int, int> map;

    map.insert({1, 1});
    map.insert({2, 2});

    ASSERT_EQ(map.size(), 2);

    auto it = map.find(1);

    ASSERT_NE(it, map.end());

    map.erase(it);

    ASSERT_EQ(map.find(1), map.end());
    ASSERT_EQ(map.size(), 1);
}

TEST(flat_unordered_map, erase_value_that_exists_by_value)
{
    tempest::core::flat_unordered_map<int, int> map;

    map.insert({1, 1});
    map.insert({2, 2});

    ASSERT_EQ(map.size(), 2);

    map.erase(1);

    ASSERT_EQ(map.find(1), map.end());
    ASSERT_EQ(map.size(), 1);
}

TEST(flat_unordered_map, erase_value_that_does_not_exist)
{
    tempest::core::flat_unordered_map<int, int> map;

    map.insert({1, 1});
    map.insert({2, 2});

    ASSERT_EQ(map.size(), 2);

    map.erase(3);

    ASSERT_EQ(map.size(), 2);
}

TEST(flat_unordered_map, iterate)
{
    tempest::core::flat_unordered_map<int, int> map;

    for (int i = 0; i < 10; ++i)
    {
        map.insert({i, 9 - i});
    }

    std::vector<std::pair<int, int>> found_values;

    for (auto& [k, v] : map)
    {
        found_values.push_back({k, v});
    }

    std::sort(found_values.begin(), found_values.end());

    for (int i = 0; i < 10; ++i)
    {
        ASSERT_EQ(found_values[i], std::make_pair(i, 9 - i));
    }
}

TEST(flat_unordered_map, const_iterator)
{
    tempest::core::flat_unordered_map<int, int> map;

    for (int i = 0; i < 20; ++i)
    {
        map.insert({i, 9 - i});
    }

    std::vector<std::pair<int, int>> found_values;

    std::for_each(std::cbegin(map), std::cend(map), [&](auto it) {
        found_values.push_back({it.first, it.second});
    });

    std::sort(found_values.begin(), found_values.end());

    for (int i = 0; i < 20; ++i)
    {
        ASSERT_EQ(found_values[i], std::make_pair(i, 9 - i));
    }
}