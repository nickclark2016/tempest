#include <tempest/array.hpp>
#include <tempest/inplace_vector.hpp>

#include <gtest/gtest.h>

TEST(inplace_vector, default_constructor)
{
    tempest::inplace_vector<int, 5> vec;

    EXPECT_EQ(vec.size(), 0);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.data(), nullptr);
}

TEST(inplace_vector, fill_constructor)
{
    tempest::inplace_vector<int, 5> vec(3);

    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);

    for (size_t idx = 0; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 0);
    }
}

TEST(inplace_vector, fill_constructor_value)
{
    tempest::inplace_vector<int, 5> vec(3, 42);

    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);

    for (size_t idx = 0; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 42);
    }
}

TEST(inplace_vector, range_constructor)
{
    tempest::array<int, 3> arr = {1, 2, 3};
    tempest::inplace_vector<int, 5> vec(arr.begin(), arr.end());

    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);

    for (size_t idx = 0; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], arr[idx]);
    }
}

TEST(inplace_vector, copy_constructor)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    tempest::inplace_vector<int, 5> copy(vec);

    EXPECT_EQ(copy.size(), 3);
    EXPECT_EQ(copy.capacity(), 5);
    EXPECT_FALSE(copy.empty());
    EXPECT_NE(copy.data(), nullptr);

    for (size_t idx = 0; idx < copy.size(); ++idx)
    {
        EXPECT_EQ(copy[idx], 42);
    }
}

TEST(inplace_vector, move_constructor)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    tempest::inplace_vector<int, 5> moved(tempest::move(vec));

    EXPECT_EQ(moved.size(), 3);
    EXPECT_EQ(moved.capacity(), 5);
    EXPECT_FALSE(moved.empty());
    EXPECT_NE(moved.data(), nullptr);

    for (size_t idx = 0; idx < moved.size(); ++idx)
    {
        EXPECT_EQ(moved[idx], 42);
    }

    EXPECT_EQ(vec.size(), 0);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.data(), nullptr);
}

TEST(inplace_vector, assign)
{
    tempest::inplace_vector<int, 5> vec;
    vec.assign(3, 42);

    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);

    for (size_t idx = 0; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 42);
    }
}

TEST(inplace_vector, assign_with_contents)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    vec.assign(2, 24);

    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);

    for (size_t idx = 0; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 24);
    }
}

TEST(inplace_vector, copy_assign)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    tempest::inplace_vector<int, 5> copy;
    copy = vec;

    EXPECT_EQ(copy.size(), 3);
    EXPECT_EQ(copy.capacity(), 5);
    EXPECT_FALSE(copy.empty());
    EXPECT_NE(copy.data(), nullptr);

    for (size_t idx = 0; idx < copy.size(); ++idx)
    {
        EXPECT_EQ(copy[idx], 42);
    }
}

TEST(inplace_vector, copy_assign_with_contents)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    tempest::inplace_vector<int, 5> copy(2, 24);
    copy = vec;

    EXPECT_EQ(copy.size(), 3);
    EXPECT_EQ(copy.capacity(), 5);
    EXPECT_FALSE(copy.empty());
    EXPECT_NE(copy.data(), nullptr);

    for (size_t idx = 0; idx < copy.size(); ++idx)
    {
        EXPECT_EQ(copy[idx], 42);
    }
}

TEST(inplace_vector, move_assign)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    tempest::inplace_vector<int, 5> moved;
    moved = tempest::move(vec);

    EXPECT_EQ(moved.size(), 3);
    EXPECT_EQ(moved.capacity(), 5);
    EXPECT_FALSE(moved.empty());
    EXPECT_NE(moved.data(), nullptr);

    for (size_t idx = 0; idx < moved.size(); ++idx)
    {
        EXPECT_EQ(moved[idx], 42);
    }

    EXPECT_EQ(vec.size(), 0);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.data(), nullptr);
}

TEST(inplace_vector, move_assign_with_contents)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    tempest::inplace_vector<int, 5> moved(2, 24);
    moved = tempest::move(vec);

    EXPECT_EQ(moved.size(), 3);
    EXPECT_EQ(moved.capacity(), 5);
    EXPECT_FALSE(moved.empty());
    EXPECT_NE(moved.data(), nullptr);

    for (size_t idx = 0; idx < moved.size(); ++idx)
    {
        EXPECT_EQ(moved[idx], 42);
    }

    EXPECT_EQ(vec.size(), 0);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.data(), nullptr);
}

TEST(inplace_vector, swap)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    tempest::inplace_vector<int, 5> other(2, 24);
    vec.swap(other);

    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);

    for (size_t idx = 0; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 24);
    }

    EXPECT_EQ(other.size(), 3);
    EXPECT_EQ(other.capacity(), 5);
    EXPECT_FALSE(other.empty());
    EXPECT_NE(other.data(), nullptr);

    for (size_t idx = 0; idx < other.size(); ++idx)
    {
        EXPECT_EQ(other[idx], 42);
    }
}

TEST(inplace_vector, swap_free)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    tempest::inplace_vector<int, 5> other(2, 24);
    swap(vec, other);

    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);
 
    for (size_t idx = 0; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 24);
    }

    EXPECT_EQ(other.size(), 3);
    EXPECT_EQ(other.capacity(), 5);
    EXPECT_FALSE(other.empty());
    EXPECT_NE(other.data(), nullptr);
 
    for (size_t idx = 0; idx < other.size(); ++idx)
    {
        EXPECT_EQ(other[idx], 42);
    }
}

TEST(inplace_vector, clear)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    vec.clear();

    EXPECT_EQ(vec.size(), 0);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.data(), nullptr);
}

TEST(inplace_vector, resize)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    vec.resize(5);

    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);

    for (size_t idx = 0; idx < 3; ++idx)
    {
        EXPECT_EQ(vec[idx], 42);
    }

    for (size_t idx = 3; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 0);
    }
}

TEST(inplace_vector, resize_value)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    vec.resize(5, 24);

    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);

    for (size_t idx = 0; idx < 3; ++idx)
    {
        EXPECT_EQ(vec[idx], 42);
    }

    for (size_t idx = 3; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 24);
    }
}

TEST(inplace_vector, push_back)
{
    tempest::inplace_vector<int, 5> vec;
    vec.push_back(42);

    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);
    EXPECT_EQ(vec[0], 42);
}

TEST(inplace_vector, push_back_full)
{
    tempest::inplace_vector<int, 5> vec(5, 42);
    vec.push_back(24);

    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);

    for (size_t idx = 0; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 42);
    }
}

TEST(inplace_vector, try_push_back)
{
    tempest::inplace_vector<int, 5> vec;
    EXPECT_TRUE(vec.try_push_back(42));

    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);
    EXPECT_EQ(vec[0], 42);
}

TEST(inplace_vector, try_push_back_full)
{
    tempest::inplace_vector<int, 5> vec(5, 42);
    EXPECT_FALSE(vec.try_push_back(24));

    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);
    for (size_t idx = 0; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 42);
    }
}

TEST(inplace_vector, insert)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    vec.insert(vec.begin() + 1, 24);

    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);
    EXPECT_EQ(vec[0], 42);
    EXPECT_EQ(vec[1], 24);
    EXPECT_EQ(vec[2], 42);
    EXPECT_EQ(vec[3], 42);
}

TEST(inplace_vector, insert_full)
{
    tempest::inplace_vector<int, 5> vec(5, 42);
    vec.insert(vec.begin() + 1, 24);

    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);

    for (size_t idx = 0; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 42);
    }
}

TEST(inplace_vector, insert_range)
{
    tempest::inplace_vector<int, 5> vec(3, 42);
    tempest::array<int, 2> arr = {1, 2};
    vec.insert(vec.begin() + 1, arr.begin(), arr.end());

    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);
    EXPECT_EQ(vec[0], 42);
    EXPECT_EQ(vec[1], 1);
    EXPECT_EQ(vec[2], 2);
    EXPECT_EQ(vec[3], 42);
    EXPECT_EQ(vec[4], 42);
}

TEST(inplace_vector, insert_range_full)
{
    tempest::inplace_vector<int, 5> vec(5, 42);
    tempest::array<int, 2> arr = {1, 2};
    
    vec.insert(vec.begin() + 1, arr.begin(), arr.end());
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);
 
    for (size_t idx = 0; idx < vec.size(); ++idx)
    {
        EXPECT_EQ(vec[idx], 42);
    }
}

TEST(inplace_vector, erase)
{
    tempest::inplace_vector<int, 5> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    vec.erase(vec.begin() + 1);

    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);

    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 3);
}

TEST(inplace_vector, erase_range)
{
    tempest::inplace_vector<int, 5> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);
    vec.push_back(5);

    vec.erase(vec.begin() + 1, vec.begin() + 4);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_FALSE(vec.empty());
    EXPECT_NE(vec.data(), nullptr);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 5);
}

TEST(inplace_vector, erase_all)
{
    tempest::inplace_vector<int, 5> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.erase(vec.begin(), vec.end());

    EXPECT_EQ(vec.size(), 0);
    EXPECT_EQ(vec.capacity(), 5);
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.data(), nullptr);
}