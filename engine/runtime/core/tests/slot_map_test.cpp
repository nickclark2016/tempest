#include <tempest/slot_map.hpp>

#include <gtest/gtest.h>

TEST(slot_map, default_constructor)
{
    tempest::slot_map<int> map;

    EXPECT_EQ(map.size(), 0);
    EXPECT_GE(map.capacity(), 0);
    EXPECT_TRUE(map.empty());
}

TEST(slot_map, insert)
{
    tempest::slot_map<int> map;

    auto k = map.insert(42);

    EXPECT_EQ(map.size(), 1);
    EXPECT_GE(map.capacity(), 1);
    EXPECT_FALSE(map.empty());

    auto it = map.find(k);
    EXPECT_NE(it, map.end());
    EXPECT_EQ(*it, 42);
}

TEST(slot_map, insert_many)
{
    tempest::slot_map<int> map;
    tempest::vector<tempest::slot_map<int>::key_type> keys;

    static constexpr int count = 1000;

    for (int i = 0; i < count; ++i)
    {
        auto k = map.insert(i);
        keys.push_back(k);
    }

    EXPECT_EQ(map.size(), count);
    EXPECT_GE(map.capacity(), count);
    EXPECT_FALSE(map.empty());

    for (int i = 0; i < count; ++i)
    {
        auto it = map.find(keys[i]);
        EXPECT_NE(it, map.end());
        EXPECT_EQ(*it, i);
    }
}

TEST(slot_map, insert_and_erase)
{
    tempest::slot_map<int> map;

    auto k = map.insert(42);

    EXPECT_EQ(map.size(), 1);
    EXPECT_GE(map.capacity(), 1);
    EXPECT_FALSE(map.empty());

    auto it = map.find(k);
    EXPECT_NE(it, map.end());
    EXPECT_EQ(*it, 42);

    map.erase(k);

    EXPECT_EQ(map.size(), 0);
    EXPECT_GE(map.capacity(), 1);
    EXPECT_TRUE(map.empty());

    it = map.find(k);
    EXPECT_EQ(it, map.end());
}

TEST(slot_map, insert_erase_insert)
{
    tempest::slot_map<int> map;

    auto k = map.insert(42);

    EXPECT_EQ(map.size(), 1);
    EXPECT_GE(map.capacity(), 1);
    EXPECT_FALSE(map.empty());

    auto it = map.find(k);
    EXPECT_NE(it, map.end());
    EXPECT_EQ(*it, 42);

    map.erase(k);

    EXPECT_EQ(map.size(), 0);
    EXPECT_GE(map.capacity(), 1);
    EXPECT_TRUE(map.empty());

    it = map.find(k);
    EXPECT_EQ(it, map.end());

    auto k2 = map.insert(43);

    EXPECT_EQ(map.size(), 1);
    EXPECT_GE(map.capacity(), 1);
    EXPECT_FALSE(map.empty());

    it = map.find(k2);
    EXPECT_NE(it, map.end());
    EXPECT_EQ(*it, 43);

    ASSERT_NE(k, k2);

    auto actual_key2_id = tempest::get_slot_map_key_id(k2);
    auto actual_key2_generation = tempest::get_slot_map_key_generation(k2);

    auto expected_key2_id = 127;
    auto expected_key2_generation = 1;

    EXPECT_EQ(actual_key2_id, expected_key2_id);
    EXPECT_EQ(actual_key2_generation, expected_key2_generation);
}

TEST(slot_map, iteration)
{
    tempest::slot_map<int> map;

    static constexpr int count = 100;

    for (int i = 0; i < count; ++i)
    {
        map.insert(i);
    }

    int i = count - 1;
    for (auto v : map)
    {
        EXPECT_EQ(v, i--);
    }
}

TEST(slot_map, iteration_after_erase)
{
    tempest::slot_map<int> map;

    static constexpr int count = 100;

    tempest::vector<tempest::slot_map<int>::key_type> keys;

    for (int i = 0; i < count; ++i)
    {
        auto it = map.insert(i);
        if (i % 2 == 0)
        {
            keys.push_back(it);
        }
    }

    for (auto k : keys)
    {
        map.erase(k);
    }

    int i = count - 1;
    for (auto v : map)
    {
        if (i % 2 == 0)
        {
            --i;
        }
        EXPECT_EQ(v, i);
        --i;
    }
}