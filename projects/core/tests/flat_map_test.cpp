#include <gtest/gtest.h>

#include <tempest/flat_map.hpp>

TEST(flat_map, default_constructor) {
    tempest::flat_map<int, int> map;
    
    EXPECT_EQ(map.size(), 0);
    EXPECT_TRUE(map.empty());
}

TEST(flat_map, initializer_list_constructor) {
    tempest::flat_map<int, int> map = {{1, 2}, {3, 4}, {5, 6}};
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
    EXPECT_EQ(map[3], 4);
    EXPECT_EQ(map[5], 6);
}

TEST(flat_map, initializer_list_constructor_with_dup_keys) {
    tempest::flat_map<int, int> map = {{1, 2}, {1, 4}, {1, 6}};
    
    EXPECT_EQ(map.size(), 1);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
}

TEST(flat_map, initializer_list_assignment) {
    tempest::flat_map<int, int> map;
    map = {{1, 2}, {3, 4}, {5, 6}};
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
    EXPECT_EQ(map[3], 4);
    EXPECT_EQ(map[5], 6);
}

TEST(flat_map, initializer_list_assignment_with_dup_keys) {
    tempest::flat_map<int, int> map;
    map = {{1, 2}, {1, 4}, {1, 6}};
    
    EXPECT_EQ(map.size(), 1);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
}

TEST(flat_map, insert)
{
    tempest::flat_map<int, int> map;
    
    // validate bool returns
    EXPECT_TRUE(map.insert({1, 2}).second);
    EXPECT_TRUE(map.insert({3, 4}).second);
    EXPECT_TRUE(map.insert({5, 6}).second);
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
    EXPECT_EQ(map[3], 4);
    EXPECT_EQ(map[5], 6);
}

TEST(flat_map, insert_with_duplicate)
{
    tempest::flat_map<int, int> map;
    
    // validate bool returns
    EXPECT_TRUE(map.insert({1, 2}).second);
    EXPECT_TRUE(map.insert({3, 4}).second);
    EXPECT_TRUE(map.insert({5, 6}).second);
    EXPECT_FALSE(map.insert({1, 8}).second);
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
}

TEST(flat_map, insert_initializer_list)
{
    tempest::flat_map<int, int> map;
    map.insert({{1, 2}, {3, 4}, {5, 6}});
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
    EXPECT_EQ(map[3], 4);
    EXPECT_EQ(map[5], 6);
}

TEST(flat_map, insert_initializer_list_with_dup_keys)
{
    tempest::flat_map<int, int> map;
    map.insert({{1, 2}, {1, 4}, {1, 6}});
    
    EXPECT_EQ(map.size(), 1);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
}

TEST(flat_map, insert_range)
{
    tempest::flat_map<int, int> map;
    map.insert({{1, 2}, {3, 4}, {5, 6}});
    
    tempest::flat_map<int, int> map2;
    map2.insert(map.begin(), map.end());
    
    EXPECT_EQ(map2.size(), 3);
    EXPECT_FALSE(map2.empty());
    EXPECT_EQ(map2[1], 2);
    EXPECT_EQ(map2[3], 4);
    EXPECT_EQ(map2[5], 6);
}

TEST(flat_map, insert_range_from_vector)
{
    tempest::flat_map<int, int> map;
    
    std::vector<std::pair<int, int>> vec = {{1, 2}, {3, 4}, {5, 6}};
    map.insert(vec.begin(), vec.end());
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
    EXPECT_EQ(map[3], 4);
    EXPECT_EQ(map[5], 6);
}

TEST(flat_map, insert_range_from_vector_with_existing_contents)
{
    tempest::flat_map<int, int> map;
    map.insert({{1, 2}, {3, 4}, {5, 6}});
    
    std::vector<std::pair<int, int>> vec = {{7, 8}, {9, 10}, {11, 12}};
    map.insert(vec.begin(), vec.end());
    
    EXPECT_EQ(map.size(), 6);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
    EXPECT_EQ(map[3], 4);
    EXPECT_EQ(map[5], 6);
    EXPECT_EQ(map[7], 8);
    EXPECT_EQ(map[9], 10);
    EXPECT_EQ(map[11], 12);
}

TEST(flat_map, insert_range_with_dup_keys)
{
    tempest::flat_map<int, int> map;
    map.insert({{1, 2}, {1, 4}, {1, 6}});
    
    tempest::flat_map<int, int> map2;
    map2.insert(map.begin(), map.end());
    
    EXPECT_EQ(map2.size(), 1);
    EXPECT_FALSE(map2.empty());
    EXPECT_EQ(map2[1], 2);
}

TEST(flat_map, insert_range_from_vector_with_dup_keys)
{
    tempest::flat_map<int, int> map;
    
    std::vector<std::pair<int, int>> vec = {{1, 2}, {1, 4}, {1, 6}};
    map.insert(vec.begin(), vec.end());
    
    EXPECT_EQ(map.size(), 1);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
}

TEST(flat_map, insert_or_assign)
{
    tempest::flat_map<int, int> map;

    // validate bool returns
    EXPECT_TRUE(map.insert_or_assign(1, 2).second);
    EXPECT_TRUE(map.insert_or_assign(3, 4).second);
    EXPECT_TRUE(map.insert_or_assign(5, 6).second);
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
    EXPECT_EQ(map[3], 4);
    EXPECT_EQ(map[5], 6);
}

TEST(flat_map, insert_or_assign_with_existing_values)
{
    tempest::flat_map<int, int> map;

    // validate bool returns
    EXPECT_TRUE(map.insert_or_assign(1, 2).second);
    EXPECT_TRUE(map.insert_or_assign(3, 4).second);
    EXPECT_TRUE(map.insert_or_assign(5, 6).second);
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
}

TEST(flat_map, find_in_empty_map)
{
    tempest::flat_map<int, int> map;
    
    auto it = map.find(1);
    
    EXPECT_EQ(it, map.end());
}

TEST(flat_map, find_non_existant_key)
{
    tempest::flat_map<int, int> map = {{1, 2}, {3, 4}, {5, 6}};
    
    auto it = map.find(7);
    
    EXPECT_EQ(it, map.end());
}

TEST(flat_map, find_key_in_map)
{
    tempest::flat_map<int, int> map = {{1, 2}, {3, 4}, {5, 6}};
    
    auto it = map.find(3);
    
    EXPECT_NE(it, map.end());
    EXPECT_EQ(it->first, 3);
    EXPECT_EQ(it->second, 4);
}

TEST(flat_map, erase)
{
    tempest::flat_map<int, int> map = {{1, 2}, {3, 4}, {5, 6}};

    auto it = map.find(3);
    map.erase(it);
    
    EXPECT_EQ(map.size(), 2);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
    EXPECT_EQ(map[5], 6);
}

TEST(flat_map, erase_range)
{
    tempest::flat_map<int, int> map = {{1, 2}, {3, 4}, {5, 6}};

    auto it = map.find(3);
    map.erase(it, map.end());
    
    EXPECT_EQ(map.size(), 1);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map[1], 2);
}

TEST(flat_map, iteration) {
    tempest::flat_map<int, int> map = {{1, 2}, {3, 4}, {5, 6}};
    
    int count = 0;
    for (auto pair : map) {
        switch (count) {
            case 0:
                EXPECT_EQ(pair.first, 1);
                EXPECT_EQ(pair.second, 2);
                break;
            case 1:
                EXPECT_EQ(pair.first, 3);
                EXPECT_EQ(pair.second, 4);
                break;
            case 2:
                EXPECT_EQ(pair.first, 5);
                EXPECT_EQ(pair.second, 6);
                break;
        }
        count++;
    }
    
    EXPECT_EQ(count, 3);
}

TEST(flat_map, iteration_const) {
    tempest::flat_map<int, int> map = {{1, 2}, {3, 4}, {5, 6}};
    
    int count = 0;
    for (const auto& pair : map) {
        switch (count) {
            case 0:
                EXPECT_EQ(pair.first, 1);
                EXPECT_EQ(pair.second, 2);
                break;
            case 1:
                EXPECT_EQ(pair.first, 3);
                EXPECT_EQ(pair.second, 4);
                break;
            case 2:
                EXPECT_EQ(pair.first, 5);
                EXPECT_EQ(pair.second, 6);
                break;
        }
        count++;
    }
    
    EXPECT_EQ(count, 3);
}

TEST(flat_map, iteration_with_out_of_order_insertion)
{
    tempest::flat_map<int, int> map;
    map.insert({{5, 6}, {1, 2}, {3, 4}});
    
    int count = 0;
    for (auto pair : map) {
        switch (count) {
            case 0:
                EXPECT_EQ(pair.first, 1);
                EXPECT_EQ(pair.second, 2);
                break;
            case 1:
                EXPECT_EQ(pair.first, 3);
                EXPECT_EQ(pair.second, 4);
                break;
            case 2:
                EXPECT_EQ(pair.first, 5);
                EXPECT_EQ(pair.second, 6);
                break;
        }
        count++;
    }
    
    EXPECT_EQ(count, 3);
}