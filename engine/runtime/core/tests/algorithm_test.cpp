#include <tempest/algorithm.hpp>
#include <tempest/int.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

//=============================================================================
// Section: tempest::sort Basic Cases
//=============================================================================

/// @brief Verifies that sorting empty or single-element ranges is a no-op and does not crash.
TEST(algorithm_test, sort_empty_and_single_element)
{
    // 1. Setup
    auto empty_vec = tempest::vector<int>{};
    auto single_vec = tempest::vector<int>(tempest::init_list, 42);

    // 2. Act
    tempest::sort(empty_vec.begin(), empty_vec.end());
    tempest::sort(single_vec.begin(), single_vec.end());

    // 3. Assert
    EXPECT_TRUE(empty_vec.empty());
    ASSERT_EQ(single_vec.size(), 1);
    EXPECT_EQ(single_vec[0], 42);
}

/// @brief Verifies that already sorted ranges remain unchanged.
TEST(algorithm_test, sort_already_sorted)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    for (auto i = 0; i < 10; ++i)
    {
        EXPECT_EQ(vec[i], i + 1);
    }
}

/// @brief Verifies that reverse-sorted ranges are correctly ordered in ascending order.
TEST(algorithm_test, sort_reverse_sorted)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    for (auto i = 0; i < 10; ++i)
    {
        EXPECT_EQ(vec[i], i + 1);
    }
}

/// @brief Verifies that ranges with identical elements are handled without degradation or corruption.
TEST(algorithm_test, sort_all_identical_elements)
{
    // 1. Setup
    auto vec = tempest::vector<int>(20, 7);

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    ASSERT_EQ(vec.size(), 20);
    for (auto i = 0; i < 20; ++i)
    {
        EXPECT_EQ(vec[i], 7);
    }
}

//=============================================================================
// Section: tempest::sort Partitioning & Introsort Behavior
//=============================================================================

/// @brief Verifies sorting small partition ranges that exercise the insertion sort path.
TEST(algorithm_test, sort_small_array_insertion_sort)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 9, 3, 5, 2, 8, 1, 4, 7, 6);

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    for (auto i = 0; i < 9; ++i)
    {
        EXPECT_EQ(vec[i], i + 1);
    }
}

/// @brief Verifies sorting arbitrary mixed numbers across range size > 16.
TEST(algorithm_test, sort_random_elements)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 34, 12, 89, 5, 23, 77, 1, 99, 45, 62, 18, 54, 3, 81, 29, 90, 11,
                                    42, 68, 7, 15, 84, 2, 95, 33, 71, 20, 50, 6, 88);

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_LE(vec[i - 1], vec[i]);
    }
}

/// @brief Verifies sorting using a custom binary comparator (e.g. descending order).
TEST(algorithm_test, sort_custom_comparator)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 15, 3, 99, 42, 8, 23, 1, 60);

    // 2. Act
    tempest::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) -> auto {
        return a > b; // Descending
    });

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_GE(vec[i - 1], vec[i]);
    }
    EXPECT_EQ(vec[0], 99);
    EXPECT_EQ(vec[vec.size() - 1], 1);
}

/// @brief Verifies sorting on a larger dataset to trigger introsort pivot partitions and depth limits.
TEST(algorithm_test, sort_large_dataset)
{
    // 1. Setup - generate pseudo-random sequence
    constexpr auto count = 1000;
    auto vec = tempest::vector<int>{};
    vec.reserve(count);
    auto seed = 123456789U;
    for (auto i = 0; i < count; ++i)
    {
        seed = ((seed * 1103515245U) + 12345U) & 0x7fffffffU;
        vec.push_back(static_cast<int>(seed % 10000));
    }

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_LE(vec[i - 1], vec[i]);
    }
}

/// @brief Verifies sorting raw C-arrays using raw pointer iterators.
TEST(algorithm_test, sort_raw_c_array)
{
    // 1. Setup
    int arr[] = {55, 12, 88, 3, 44, 91, 7, 23};
    constexpr auto count = sizeof(arr) / sizeof(arr[0]);

    // 2. Act
    tempest::sort(arr, arr + count);

    // 3. Assert
    for (size_t i = 1; i < count; ++i)
    {
        EXPECT_LE(arr[i - 1], arr[i]);
    }
}

/// @brief Verifies sorting non-trivial heap-allocating types (tempest::string) with move operations.
TEST(algorithm_test, sort_non_trivial_strings)
{
    // 1. Setup
    auto strings = tempest::vector<tempest::string>{};
    strings.push_back(tempest::string{"banana"});
    strings.push_back(tempest::string{"apple"});
    strings.push_back(tempest::string{"cherry"});
    strings.push_back(tempest::string{"date"});
    strings.push_back(tempest::string{"elderberry"});
    strings.push_back(tempest::string{"fig"});
    strings.push_back(tempest::string{"grape"});

    // 2. Act
    tempest::sort(strings.begin(), strings.end());

    // 3. Assert
    for (size_t i = 1; i < strings.size(); ++i)
    {
        EXPECT_LE(strings[i - 1], strings[i]);
    }
    EXPECT_EQ(strings[0], tempest::string{"apple"});
    EXPECT_EQ(strings[strings.size() - 1], tempest::string{"grape"});
}

/// @brief Verifies sorting custom struct objects by a specific field with a lambda comparator.
TEST(algorithm_test, sort_custom_struct_by_key)
{
    struct item
    {
        int priority;
        int id;
    };

    // 1. Setup
    auto items = tempest::vector<item>{};
    items.push_back(item{.priority = 5, .id = 101});
    items.push_back(item{.priority = 1, .id = 102});
    items.push_back(item{.priority = 9, .id = 103});
    items.push_back(item{.priority = 3, .id = 104});
    items.push_back(item{.priority = 7, .id = 105});

    // 2. Act
    tempest::sort(items.begin(), items.end(),
                  [](const item& a, const item& b) -> bool { return a.priority < b.priority; });

    // 3. Assert
    for (size_t i = 1; i < items.size(); ++i)
    {
        EXPECT_LE(items[i - 1].priority, items[i].priority);
    }
    EXPECT_EQ(items[0].id, 102);
    EXPECT_EQ(items[items.size() - 1].id, 103);
}

/// @brief Verifies sorting a large dataset composed of only 3 distinct duplicate values (Dutch National Flag stress).
TEST(algorithm_test, sort_many_duplicates_large)
{
    // 1. Setup - 600 elements with values in {0, 1, 2}
    constexpr auto count = 600;
    auto vec = tempest::vector<int>{};
    vec.reserve(count);
    for (auto i = 0; i < count; ++i)
    {
        vec.push_back(((i * 7) + 3) % 3);
    }

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_LE(vec[i - 1], vec[i]);
    }
}

/// @brief Verifies sorting a large pre-sorted descending dataset (stressing pivot selection and partition balance).
TEST(algorithm_test, sort_large_reverse_sorted)
{
    // 1. Setup - 500 elements in strictly descending order
    constexpr auto count = 500;
    auto vec = tempest::vector<int>{};
    vec.reserve(count);
    for (auto i = count; i > 0; --i)
    {
        vec.push_back(i);
    }

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_LE(vec[i - 1], vec[i]);
    }
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[vec.size() - 1], count);
}

/// @brief Directly tests detail::heap_sort to verify correctness of the heapsort fallback implementation.
TEST(algorithm_test, heapsort_direct_fallback_test)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 45, 12, 89, 3, 67, 23, 99, 1, 34, 78, 56);

    // 2. Act
    tempest::detail::heap_sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) -> auto { return a < b; });

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_LE(vec[i - 1], vec[i]);
    }
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[vec.size() - 1], 99);
}
