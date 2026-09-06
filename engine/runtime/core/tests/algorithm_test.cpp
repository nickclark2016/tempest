#include <tempest/algorithm.hpp>
#include <tempest/int.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

// NOLINTBEGIN(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers,readability-identifier-length)

//=============================================================================
// Section: tempest::clamp Tests
//=============================================================================

/// @brief Verifies that clamp constrains values within the [lo, hi] range.
TEST(algorithm_test, clamp_basic)
{
    // 1. Setup & 2. Act & 3. Assert
    EXPECT_EQ(tempest::clamp(5, 1, 10), 5);
    EXPECT_EQ(tempest::clamp(0, 1, 10), 1);
    EXPECT_EQ(tempest::clamp(15, 1, 10), 10);
}

//=============================================================================
// Section: tempest::find & find_if Tests
//=============================================================================

/// @brief Verifies that find finds an existing element and returns end when not found.
TEST(algorithm_test, find_basic)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 10, 20, 30, 40, 50);

    // 2. Act
    auto* it1 = tempest::find(vec.begin(), vec.end(), 30);
    auto* it2 = tempest::find(vec.begin(), vec.end(), 99);

    // 3. Assert
    ASSERT_NE(it1, vec.end());
    EXPECT_EQ(*it1, 30);
    EXPECT_EQ(it2, vec.end());
}

/// @brief Verifies find_if with a predicate.
TEST(algorithm_test, find_if_basic)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 1, 3, 5, 8, 9);

    // 2. Act
    auto* it = tempest::find_if(vec.begin(), vec.end(), [](int v) -> bool { return v % 2 == 0; });

    // 3. Assert
    ASSERT_NE(it, vec.end());
    EXPECT_EQ(*it, 8);
}

//=============================================================================
// Section: tempest::min_element & max_element Tests
//=============================================================================

/// @brief Verifies min_element and max_element on non-empty ranges.
TEST(algorithm_test, min_max_element)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 30, 10, 50, 20, 40);

    // 2. Act
    auto* min_it = tempest::min_element(vec.begin(), vec.end());
    auto* max_it = tempest::max_element(vec.begin(), vec.end());

    // 3. Assert
    ASSERT_NE(min_it, vec.end());
    ASSERT_NE(max_it, vec.end());
    EXPECT_EQ(*min_it, 10);
    EXPECT_EQ(*max_it, 50);
}

//=============================================================================
// Section: tempest::lower_bound & upper_bound Tests
//=============================================================================

/// @brief Verifies binary search partitioning with lower_bound and upper_bound.
TEST(algorithm_test, lower_upper_bound)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 10, 20, 20, 20, 30, 40);

    // 2. Act
    auto* lb = tempest::lower_bound(vec.begin(), vec.end(), 20);
    auto* ub = tempest::upper_bound(vec.begin(), vec.end(), 20);

    // 3. Assert
    EXPECT_EQ(lb - vec.begin(), 1);
    EXPECT_EQ(ub - vec.begin(), 4);
}

// NOLINTEND(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers,readability-identifier-length)
