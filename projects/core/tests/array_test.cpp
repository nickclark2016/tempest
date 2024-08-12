#include <tempest/array.hpp>

#include <gtest/gtest.h>

TEST(array, default_constructor_non_zero_sized)
{
    tempest::array<int, 10> arr;

    EXPECT_EQ(arr.size(), 10);
    EXPECT_EQ(arr.max_size(), 10);
    EXPECT_FALSE(arr.empty());
}

TEST(array, default_constructor_zero_sized)
{
    tempest::array<int, 0> arr;

    EXPECT_EQ(arr.size(), 0);
    EXPECT_EQ(arr.max_size(), 0);
    EXPECT_TRUE(arr.empty());
}

TEST(array, construct_with_zeroes)
{
    tempest::array<int, 10> arr = {0};

    for (const auto& i : arr)
    {
        EXPECT_EQ(i, 0);
    }
}

TEST(array, construct_with_values)
{
    tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr[i], i + 1);
    }
}

TEST(array, copy_constructor)
{
    tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    tempest::array<int, 10> arr2(arr);

    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr2[i], i + 1);
    }
}

TEST(array, copy_constructor_non_trivial_copy)
{
    struct non_trivial
    {
        int i;
        non_trivial() : i(0)
        {
        }

        non_trivial(int i) : i(i)
        {
        }
        non_trivial(const non_trivial& other) : i(other.i)
        {
        }
    };

    tempest::array<non_trivial, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    tempest::array<non_trivial, 10> arr2(arr);

    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr2[i].i, i + 1);
    }
}

TEST(array, move_constructor)
{
    tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    tempest::array<int, 10> arr2(std::move(arr));

    for (size_t i = 0; i < arr2.size(); ++i)
    {
        EXPECT_EQ(arr2[i], i + 1);
    }
}

TEST(array, move_constructor_non_trivial_copy)
{
    struct non_trivial
    {
        int i;
        non_trivial() : i(0)
        {
        }

        non_trivial(int i) : i(i)
        {
        }
        non_trivial(const non_trivial& other) : i(other.i)
        {
        }
        non_trivial(non_trivial&& other) : i(other.i)
        {
            other.i = 0;
        }
    };

    tempest::array<non_trivial, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    tempest::array<non_trivial, 10> arr2(std::move(arr));

    for (size_t i = 0; i < arr2.size(); ++i)
    {
        EXPECT_EQ(arr2[i].i, i + 1);
    }

    // Check that the moved-from array is zeroed out
    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr[i].i, 0);
    }
}

TEST(array, copy_assignment)
{
    tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    tempest::array<int, 10> arr2;
    arr2 = arr;

    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr2[i], i + 1);
    }
}

TEST(array, copy_assignment_non_trivial_copy)
{
    struct non_trivial
    {
        int i;
        non_trivial() : i(0)
        {
        }

        non_trivial(int i) : i(i)
        {
        }
        non_trivial(const non_trivial& other) : i(other.i)
        {
        }
    };

    tempest::array<non_trivial, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    tempest::array<non_trivial, 10> arr2;
    arr2 = arr;

    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr2[i].i, i + 1);
    }
}

TEST(array, move_assignment)
{
    tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    tempest::array<int, 10> arr2;
    arr2 = std::move(arr);

    for (size_t i = 0; i < arr2.size(); ++i)
    {
        EXPECT_EQ(arr2[i], i + 1);
    }
}

TEST(array, move_assignment_non_trivial_move)
{
    struct non_trivial
    {
        int i;
        non_trivial() : i(0)
        {
        }

        non_trivial(int i) : i(i)
        {
        }
        non_trivial(const non_trivial& other) : i(other.i)
        {
        }
        non_trivial(non_trivial&& other) : i(other.i)
        {
            other.i = 0;
        }

        non_trivial operator=(non_trivial&& other)
        {
            i = other.i;
            other.i = 0;
            return *this;
        }
    };

    tempest::array<non_trivial, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    tempest::array<non_trivial, 10> arr2;
    arr2 = std::move(arr);

    for (size_t i = 0; i < arr2.size(); ++i)
    {
        EXPECT_EQ(arr2[i].i, i + 1);
    }

    // Check that the moved-from array is zeroed out
    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr[i].i, 0);
    }
}

TEST(array, fill)
{
    tempest::array<int, 10> arr;
    arr.fill(42);

    for (const auto& i : arr)
    {
        EXPECT_EQ(i, 42);
    }
}

TEST(array, swap)
{
    tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    tempest::array<int, 10> arr2 = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};

    arr.swap(arr2);

    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr[i], 10 - i);
        EXPECT_EQ(arr2[i], i + 1);
    }
}

TEST(array, front)
{
    tempest::array<int, 10> arr = {42};

    EXPECT_EQ(arr.front(), 42);
}

TEST(array, front_const)
{
    const tempest::array<int, 10> arr = {42};

    EXPECT_EQ(arr.front(), 42);
}

TEST(array, back)
{
    tempest::array<int, 10> arr = {42, 42, 42, 42, 42, 42, 42, 42, 42, 42};

    EXPECT_EQ(arr.back(), 42);
}

TEST(array, back_const)
{
    const tempest::array<int, 10> arr = {42, 42, 42, 42, 42, 42, 42, 42, 42, 42};

    EXPECT_EQ(arr.back(), 42);
}

TEST(array, at)
{
    tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr.at(i), i + 1);
    }
}

TEST(array, at_const)
{
    const tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr.at(i), i + 1);
    }
}

TEST(array, begin)
{
    tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (auto it = arr.begin(); it != arr.end(); ++it)
    {
        EXPECT_EQ(*it, it - arr.begin() + 1);
    }
}

TEST(array, begin_const)
{
    const tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (auto it = arr.begin(); it != arr.end(); ++it)
    {
        EXPECT_EQ(*it, it - arr.begin() + 1);
    }
}

TEST(array, cbegin)
{
    const tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (auto it = arr.cbegin(); it != arr.cend(); ++it)
    {
        EXPECT_EQ(*it, it - arr.cbegin() + 1);
    }
}

TEST(array, end)
{
    tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (auto it = arr.begin(); it != arr.end(); ++it)
    {
        EXPECT_EQ(*it, it - arr.begin() + 1);
    }
}

TEST(array, end_const)
{
    const tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (auto it = arr.begin(); it != arr.end(); ++it)
    {
        EXPECT_EQ(*it, it - arr.begin() + 1);
    }
}

TEST(array, cend)
{
    const tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (auto it = arr.cbegin(); it != arr.cend(); ++it)
    {
        EXPECT_EQ(*it, it - arr.cbegin() + 1);
    }
}

TEST(array, data)
{
    tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    int* data = arr.data();
    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(data[i], i + 1);
    }
}

TEST(array, data_const)
{
    const tempest::array<int, 10> arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    const int* data = arr.data();
    for (size_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(data[i], i + 1);
    }
}
