#include <tempest/vector.hpp>

#include <gtest/gtest.h>

template <typename T>
using vector = tempest::vector<T>;

TEST(vector, default_constructor)
{
    vector<int> v;
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 0);
}

TEST(vector, constructor_with_size)
{
    vector<int> v(10);
    EXPECT_EQ(v.size(), 10);
    EXPECT_EQ(v.capacity(), 10);
}

TEST(vector, constructor_with_size_and_value)
{
    vector<int> v(10, 42);
    EXPECT_EQ(v.size(), 10);
    EXPECT_EQ(v.capacity(), 10);
    for (const auto& i : v)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(vector, copy_constructor)
{
    vector<int> v1(10, 42);
    vector<int> v2(v1);
    EXPECT_EQ(v2.size(), 10);
    EXPECT_GE(v2.capacity(), 10);
    for (const auto& i : v2)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(vector, copy_constructor_non_trivial_copy)
{
    struct non_trivial
    {
        int i;
        non_trivial(int i) : i(i) {}
        non_trivial(const non_trivial& other) : i(other.i) {}
    };

    vector<non_trivial> v1(10, 42);
    vector<non_trivial> v2(v1);
    EXPECT_EQ(v2.size(), 10);
    EXPECT_GE(v2.capacity(), 10);
    for (const auto& i : v2)
    {
        EXPECT_EQ(i.i, 42);
    }
}

TEST(vector, move_constructor)
{
    vector<int> v1(10, 42);
    vector<int> v2(std::move(v1));
    EXPECT_EQ(v2.size(), 10);
    EXPECT_EQ(v2.capacity(), 10);
    for (const auto& i : v2)
    {
        EXPECT_EQ(i, 42);
    }

    EXPECT_EQ(v1.size(), 0);
    EXPECT_EQ(v1.capacity(), 0);
}

TEST(vector, copy_assignment)
{
    vector<int> v1(10, 42);
    vector<int> v2;
    v2 = v1;
    EXPECT_EQ(v2.size(), 10);
    EXPECT_GE(v2.capacity(), 10);
    for (const auto& i : v2)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(vector, move_assignment)
{
    vector<int> v1(10, 42);
    vector<int> v2;
    v2 = std::move(v1);
    EXPECT_EQ(v2.size(), 10);
    EXPECT_EQ(v2.capacity(), 10);
    for (const auto& i : v2)
    {
        EXPECT_EQ(i, 42);
    }

    EXPECT_EQ(v1.size(), 0);
    EXPECT_EQ(v1.capacity(), 0);
}

TEST(vector, push_back)
{
    vector<int> v;
    for (int i = 0; i < 10; ++i)
    {
        v.push_back(i);
    }
    EXPECT_EQ(v.size(), 10);
    EXPECT_GE(v.capacity(), 10);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(v[i], i);
    }
}

TEST(vector, pop_back)
{
    vector<int> v(10, 42);
    for (int i = 0; i < 5; ++i)
    {
        v.pop_back();
    }
    EXPECT_EQ(v.size(), 5);
    EXPECT_EQ(v.capacity(), 10);
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(vector, clear)
{
    vector<int> v(10, 42);
    v.clear();
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 10);
}

TEST(vector, resize)
{
    vector<int> v(10, 42);
    v.resize(5);
    EXPECT_EQ(v.size(), 5);
    EXPECT_EQ(v.capacity(), 10);
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(vector, reserve)
{
    vector<int> v;
    v.reserve(10);
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 10);
}

TEST(vector, shrink_to_fit)
{
    vector<int> v(10, 42);
    v.resize(5);
    v.shrink_to_fit();
    EXPECT_EQ(v.size(), 5);
    EXPECT_EQ(v.capacity(), 5);
}

TEST(vector, at)
{
    vector<int> v(10, 42);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(v.at(i), 42);
    }
}

TEST(vector, operator_brackets)
{
    vector<int> v(10, 42);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(vector, front)
{
    vector<int> v(10, 42);
    EXPECT_EQ(v.front(), 42);
}

TEST(vector, back)
{
    vector<int> v(10, 42);
    EXPECT_EQ(v.back(), 42);
}

TEST(vector, data)
{
    vector<int> v(10, 42);
    int* data = v.data();
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(data[i], 42);
    }
}

TEST(vector, begin_end)
{
    vector<int> v(10, 42);
    int i = 0;
    for (auto it = v.begin(); it != v.end(); ++it)
    {
        EXPECT_EQ(*it, 42);
        ++i;
    }
    EXPECT_EQ(i, 10);
}

TEST(vector, cbegin_cend)
{
    vector<int> v(10, 42);
    int i = 0;
    for (auto it = v.cbegin(); it != v.cend(); ++it)
    {
        EXPECT_EQ(*it, 42);
        ++i;
    }
    EXPECT_EQ(i, 10);
}

TEST(vector, rbegin_rend)
{
    vector<int> v(10, 42);
    int i = 0;
    for (auto it = v.rbegin(); it != v.rend(); ++it)
    {
        EXPECT_EQ(*it, 42);
        ++i;
    }
    EXPECT_EQ(i, 10);
}

TEST(vector, crbegin_crend)
{
    vector<int> v(10, 42);
    int i = 0;
    for (auto it = v.crbegin(); it != v.crend(); ++it)
    {
        EXPECT_EQ(*it, 42);
        ++i;
    }
    EXPECT_EQ(i, 10);
}

TEST(vector, insert)
{
    vector<int> v(10, 42);
    auto it = v.insert(v.begin() + 5, 24);
    EXPECT_EQ(v.size(), 11);
    EXPECT_GE(v.capacity(), v.size());
    EXPECT_EQ(*it, 24);
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
    EXPECT_EQ(v[5], 24);
    for (int i = 6; i < 11; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(vector, erase)
{
    vector<int> v(10, 42);
    auto it = v.erase(v.begin() + 5);
    EXPECT_EQ(v.size(), 9);
    EXPECT_GE(v.capacity(), v.size());
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
    for (int i = 5; i < 9; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
}

TEST(vector, erase_range)
{
    vector<int> v(10, 42);
    auto it = v.erase(v.begin() + 5, v.begin() + 7);
    EXPECT_EQ(v.size(), 8);
    EXPECT_GE(v.capacity(), v.size());
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }
    for (int i = 5; i < 8; ++i)
    {
        EXPECT_EQ(v[i], 42);
    }

}

TEST(vector, swap)
{
    vector<int> v1(10, 42);
    vector<int> v2(5, 24);
    v1.swap(v2);
    EXPECT_EQ(v1.size(), 5);
    EXPECT_GE(v1.capacity(), v1.size());
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v1[i], 24);
    }
    EXPECT_EQ(v2.size(), 10);
    EXPECT_GE(v2.capacity(), 10);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(v2[i], 42);
    }
}

TEST(vector, operator_equal)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_TRUE(v1 == v2);
    v2[5] = 24;
    EXPECT_FALSE(v1 == v2);
}

TEST(vector, operator_not_equal)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_FALSE(v1 != v2);
    v2[5] = 24;
    EXPECT_TRUE(v1 != v2);
}

TEST(vector, operator_less)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_FALSE(v1 < v2);
    v1[5] = 24;
    EXPECT_TRUE(v1 < v2);
}

TEST(vector, operator_less_or_equal)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_TRUE(v1 <= v2);
    v1[5] = 24;
    EXPECT_TRUE(v1 <= v2);
    v2[5] = 42;
    EXPECT_TRUE(v1 <= v2);
}

TEST(vector, operator_greater)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_FALSE(v1 > v2);
    v1[5] = 24;
    EXPECT_FALSE(v1 > v2);
    v2[5] = 42;
    EXPECT_FALSE(v1 > v2);
}

TEST(vector, operator_greater_or_equal)
{
    vector<int> v1(10, 42);
    vector<int> v2(10, 42);
    EXPECT_TRUE(v1 >= v2);
    v1[5] = 24;
    EXPECT_FALSE(v1 >= v2);
    v2[5] = 42;
    EXPECT_FALSE(v1 >= v2);
}

TEST(vector, swap_non_member)
{
    vector<int> v1(10, 42);
    vector<int> v2(5, 24);
    swap(v1, v2);
    EXPECT_EQ(v1.size(), 5);
    EXPECT_GE(v1.capacity(), v1.size());
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v1[i], 24);
    }
    EXPECT_EQ(v2.size(), 10);
    EXPECT_GE(v2.capacity(), v2.size());
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(v2[i], 42);
    }
}

