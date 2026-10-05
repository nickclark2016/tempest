#include <tempest/vector.hpp>

#include <gtest/gtest.h>

#include <tempest/iterator.hpp>
#include <tempest/utility.hpp>

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

        non_trivial(int i) : i(i)
        {
        }
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
    vector<int> v2(tempest::move(v1));
    EXPECT_EQ(v2.size(), 10);
    EXPECT_EQ(v2.capacity(), 10);
    for (const auto& i : v2)
    {
        EXPECT_EQ(i, 42);
    }

    EXPECT_EQ(v1.size(), 0);
    EXPECT_EQ(v1.capacity(), 0);
}

TEST(vector, variadic_constructor)
{
    vector<int> v(tempest::init_list, 1, 2, 3, 4, 5);
    EXPECT_EQ(v.size(), 5);
    EXPECT_GE(v.capacity(), 5);
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(v[i], i + 1);
    }
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
    v2 = tempest::move(v1);
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
    for (int & it : v)
    {
        EXPECT_EQ(it, 42);
        ++i;
    }
    EXPECT_EQ(i, 10);
}

TEST(vector, cbegin_cend)
{
    vector<int> v(10, 42);
    int i = 0;
    for (int it : v)
    {
        EXPECT_EQ(it, 42);
        ++i;
    }
    EXPECT_EQ(i, 10);
}

// NOLINTBEGIN(modernize-loop-convert)
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
// NOLINTEND(modernize-loop-convert)

TEST(vector, insert)
{
    vector<int> v(10, 42);
    auto *it = v.insert(v.begin() + 5, 24);
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

/// @brief Verifies that self-referential vector::insert (inserting an existing element from the vector)
/// does not cause use-after-free, dangling references, or moved-from value corruption, both when
/// capacity is exceeded (triggering reallocation) and when capacity is pre-reserved.
TEST(vector, vector_insert_self_referential)
{
    // =========================================================================
    // Case 1: Trivial Type (int) with Reallocation Triggered (Capacity Exceeded)
    // =========================================================================
    {
        // 1. Setup: Vector initialized with exact capacity matching size
        auto vec = vector<int>{tempest::init_list, 10, 20, 30};
        vec.shrink_to_fit();
        EXPECT_EQ(vec.size(), 3);
        EXPECT_EQ(vec.capacity(), 3);

        // 2. Act: Insert vec[0] into vec.begin() + 1 when size == capacity
        const auto it_first = vec.insert(vec.begin() + 1, vec[0]);

        // 3. Assert: Value is intact, capacity grew, and elements are in expected order
        EXPECT_EQ(vec.size(), 4);
        EXPECT_GE(vec.capacity(), 4);
        EXPECT_EQ(*it_first, 10);
        EXPECT_EQ(vec[0], 10);
        EXPECT_EQ(vec[1], 10);
        EXPECT_EQ(vec[2], 20);
        EXPECT_EQ(vec[3], 30);

        // Act: Shrink to fit then insert vec.back() into vec.begin() + 1 triggering another reallocation
        vec.shrink_to_fit();
        EXPECT_EQ(vec.capacity(), 4);
        const auto it_back = vec.insert(vec.begin() + 1, vec.back());

        // Assert: Value (30) correctly inserted without dangling read
        EXPECT_EQ(vec.size(), 5);
        EXPECT_GE(vec.capacity(), 5);
        EXPECT_EQ(*it_back, 30);
        EXPECT_EQ(vec[0], 10);
        EXPECT_EQ(vec[1], 30);
        EXPECT_EQ(vec[2], 10);
        EXPECT_EQ(vec[3], 20);
        EXPECT_EQ(vec[4], 30);
    }

    // =========================================================================
    // Case 2: Trivial Type (int) with Pre-Reserved Capacity (No Reallocation)
    // =========================================================================
    {
        // 1. Setup: Vector with pre-reserved excess capacity
        auto vec = vector<int>{};
        vec.reserve(16);
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);
        EXPECT_EQ(vec.size(), 3);
        EXPECT_GE(vec.capacity(), 16);

        // 2. Act: Insert vec[0] into vec.begin() + 1 with sufficient capacity
        const auto it_first = vec.insert(vec.begin() + 1, vec[0]);

        // 3. Assert: All elements shifted and inserted correctly without moving-from source element
        EXPECT_EQ(vec.size(), 4);
        EXPECT_EQ(*it_first, 10);
        EXPECT_EQ(vec[0], 10);
        EXPECT_EQ(vec[1], 10);
        EXPECT_EQ(vec[2], 20);
        EXPECT_EQ(vec[3], 30);

        // Act: Insert vec.back() into vec.begin() + 1
        const auto it_back = vec.insert(vec.begin() + 1, vec.back());

        // Assert: All elements intact
        EXPECT_EQ(vec.size(), 5);
        EXPECT_EQ(*it_back, 30);
        EXPECT_EQ(vec[0], 10);
        EXPECT_EQ(vec[1], 30);
        EXPECT_EQ(vec[2], 10);
        EXPECT_EQ(vec[3], 20);
        EXPECT_EQ(vec[4], 30);
    }

    // =========================================================================
    // Case 3: Non-Trivial Copy/Move Type with Reallocation & Pre-Reserved
    // =========================================================================
    {
        struct non_trivial_item
        {
            int value{0};
            bool moved_from{false};

            non_trivial_item() = default;
            explicit non_trivial_item(int val) : value{val} {}

            non_trivial_item(const non_trivial_item& other)
                : value{other.value}, moved_from{other.moved_from}
            {
            }

            non_trivial_item(non_trivial_item&& other) noexcept
                : value{other.value}, moved_from{false}
            {
                other.moved_from = true;
                other.value = -1;
            }

            auto operator=(const non_trivial_item& other) -> non_trivial_item&
            {
                if (this != &other)
                {
                    value = other.value;
                    moved_from = other.moved_from;
                }
                return *this;
            }

            auto operator=(non_trivial_item&& other) noexcept -> non_trivial_item&
            {
                if (this != &other)
                {
                    value = other.value;
                    moved_from = false;
                    other.moved_from = true;
                    other.value = -1;
                }
                return *this;
            }
        };

        // Subcase A: Reallocation triggered
        {
            auto vec = vector<non_trivial_item>{};
            vec.push_back(non_trivial_item{100});
            vec.push_back(non_trivial_item{200});
            vec.push_back(non_trivial_item{300});
            vec.shrink_to_fit();
            EXPECT_EQ(vec.size(), 3);
            EXPECT_EQ(vec.capacity(), 3);

            // Insert vec[0] into vec.begin() + 1
            const auto it_first = vec.insert(vec.begin() + 1, vec[0]);
            EXPECT_EQ(vec.size(), 4);
            EXPECT_EQ(it_first->value, 100);
            EXPECT_FALSE(it_first->moved_from);
            EXPECT_EQ(vec[0].value, 100);
            EXPECT_FALSE(vec[0].moved_from);
            EXPECT_EQ(vec[1].value, 100);
            EXPECT_FALSE(vec[1].moved_from);
            EXPECT_EQ(vec[2].value, 200);
            EXPECT_FALSE(vec[2].moved_from);
            EXPECT_EQ(vec[3].value, 300);
            EXPECT_FALSE(vec[3].moved_from);

            // Insert vec.back() into vec.begin() + 1 with reallocation
            vec.shrink_to_fit();
            const auto it_back = vec.insert(vec.begin() + 1, vec.back());
            EXPECT_EQ(vec.size(), 5);
            EXPECT_EQ(it_back->value, 300);
            EXPECT_FALSE(it_back->moved_from);
            EXPECT_EQ(vec[0].value, 100);
            EXPECT_EQ(vec[1].value, 300);
            EXPECT_EQ(vec[2].value, 100);
            EXPECT_EQ(vec[3].value, 200);
            EXPECT_EQ(vec[4].value, 300);
            for (const auto& item : vec)
            {
                EXPECT_FALSE(item.moved_from);
            }
        }

        // Subcase B: Pre-reserved capacity (no reallocation)
        {
            auto vec = vector<non_trivial_item>{};
            vec.reserve(16);
            vec.push_back(non_trivial_item{100});
            vec.push_back(non_trivial_item{200});
            vec.push_back(non_trivial_item{300});

            // Insert vec[0] into vec.begin() + 1
            const auto it_first = vec.insert(vec.begin() + 1, vec[0]);
            EXPECT_EQ(vec.size(), 4);
            EXPECT_EQ(it_first->value, 100);
            EXPECT_FALSE(it_first->moved_from);
            EXPECT_EQ(vec[0].value, 100);
            EXPECT_EQ(vec[1].value, 100);
            EXPECT_EQ(vec[2].value, 200);
            EXPECT_EQ(vec[3].value, 300);

            // Insert vec.back() into vec.begin() + 1
            const auto it_back = vec.insert(vec.begin() + 1, vec.back());
            EXPECT_EQ(vec.size(), 5);
            EXPECT_EQ(it_back->value, 300);
            EXPECT_FALSE(it_back->moved_from);
            EXPECT_EQ(vec[0].value, 100);
            EXPECT_EQ(vec[1].value, 300);
            EXPECT_EQ(vec[2].value, 100);
            EXPECT_EQ(vec[3].value, 200);
            EXPECT_EQ(vec[4].value, 300);
            for (const auto& item : vec)
            {
                EXPECT_FALSE(item.moved_from);
            }
        }
    }
}

TEST(vector, erase)
{
    vector<int> v(10, 42);
    (void)v.erase(v.begin() + 5);
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
    (void)v.erase(v.begin() + 5, v.begin() + 7);
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
    EXPECT_GE(v2.capacity(), 10);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(v2[i], 42);
    }
}

TEST(vector, iterator_checks)
{
    using vec_it = tempest::vector<int>::iterator;
    EXPECT_TRUE(tempest::contiguous_iterator<vec_it>);
}