#include <tempest/algorithm.hpp>
#include <tempest/comparators.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

// NOLINTBEGIN(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers,readability-identifier-length,readability-function-cognitive-complexity)

//=============================================================================
// Section: Trivial & Boundary Range Cases
//=============================================================================

/// @brief Verifies that sorting empty or single-element ranges is a no-op and does not crash.
TEST(sort_test, sort_empty_and_single_element)
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

/// @brief Verifies sorting two elements in already-sorted and reverse order.
TEST(sort_test, sort_two_elements)
{
    // 1. Setup
    auto sorted_pair = tempest::vector<int>(tempest::init_list, 1, 2);
    auto unsorted_pair = tempest::vector<int>(tempest::init_list, 2, 1);

    // 2. Act
    tempest::sort(sorted_pair.begin(), sorted_pair.end());
    tempest::sort(unsorted_pair.begin(), unsorted_pair.end());

    // 3. Assert
    EXPECT_EQ(sorted_pair[0], 1);
    EXPECT_EQ(sorted_pair[1], 2);
    EXPECT_EQ(unsorted_pair[0], 1);
    EXPECT_EQ(unsorted_pair[1], 2);
}

//=============================================================================
// Section: Ordering Invariants & Edge Distributions
//=============================================================================

/// @brief Verifies that already sorted ranges remain in ascending order.
TEST(sort_test, sort_already_sorted)
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

/// @brief Verifies that strictly reverse-sorted ranges are correctly ordered in ascending order.
TEST(sort_test, sort_reverse_sorted)
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
TEST(sort_test, sort_all_identical_elements)
{
    // 1. Setup
    auto vec = tempest::vector<int>(25, 7);

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    ASSERT_EQ(vec.size(), 25);
    for (int i : vec)
    {
        EXPECT_EQ(i, 7);
    }
}

/// @brief Verifies sorting small partition ranges that exercise the insertion sort path (<= 16 elements).
TEST(sort_test, sort_small_array_insertion_sort)
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

/// @brief Verifies sorting a large dataset composed of only 3 distinct duplicate values (Dutch National Flag stress).
TEST(sort_test, sort_many_duplicates_large)
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

/// @brief Verifies sorting a large pre-sorted descending dataset stressing pivot selection and partition balance.
TEST(sort_test, sort_large_reverse_sorted)
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

//=============================================================================
// Section: Random Permutations at Scale (10^2, 10^4, 10^5)
//=============================================================================

/// @brief Verifies sorting a random permutation of 10^2 (100) elements.
TEST(sort_test, sort_permutation_10_2)
{
    // 1. Setup
    constexpr auto count = 100;
    auto vec = tempest::vector<int>{};
    vec.reserve(count);
    auto seed = 123456789U;
    for (auto i = 0; i < count; ++i)
    {
        seed = ((seed * 1103515245U) + 12345U) & 0x7fffffffU;
        vec.push_back(static_cast<int>(seed % 1000));
    }

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_LE(vec[i - 1], vec[i]);
    }
}

/// @brief Verifies sorting a random permutation of 10^4 (10,000) elements.
TEST(sort_test, sort_permutation_10_4)
{
    // 1. Setup
    constexpr auto count = 10000;
    auto vec = tempest::vector<int>{};
    vec.reserve(count);
    auto seed = 987654321U;
    for (auto i = 0; i < count; ++i)
    {
        seed = ((seed * 1103515245U) + 12345U) & 0x7fffffffU;
        vec.push_back(static_cast<int>(seed % 50000));
    }

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_LE(vec[i - 1], vec[i]);
    }
}

/// @brief Verifies sorting a random permutation of 10^5 (100,000) elements (stressing introsort depth limit and
/// heapsort fallback).
TEST(sort_test, sort_permutation_10_5)
{
    // 1. Setup
    constexpr auto count = 100000;
    auto vec = tempest::vector<int>{};
    vec.reserve(count);
    auto seed = 424242424U;
    for (auto i = 0; i < count; ++i)
    {
        seed = ((seed * 1103515245U) + 12345U) & 0x7fffffffU;
        vec.push_back(static_cast<int>(seed % 1000000));
    }

    // 2. Act
    tempest::sort(vec.begin(), vec.end());

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_LE(vec[i - 1], vec[i]);
    }
}

//=============================================================================
// Section: Move-Only Types & Non-Trivially Copyable Types
//=============================================================================

/// @brief Verifies sorting move-only types (tempest::unique_ptr<int>) using move semantics without copy overhead.
TEST(sort_test, sort_move_only_unique_ptr)
{
    // 1. Setup
    auto vec = tempest::vector<tempest::unique_ptr<int>>{};
    vec.push_back(tempest::make_unique<int>(50));
    vec.push_back(tempest::make_unique<int>(10));
    vec.push_back(tempest::make_unique<int>(90));
    vec.push_back(tempest::make_unique<int>(20));
    vec.push_back(tempest::make_unique<int>(80));
    vec.push_back(tempest::make_unique<int>(30));
    vec.push_back(tempest::make_unique<int>(70));
    vec.push_back(tempest::make_unique<int>(40));
    vec.push_back(tempest::make_unique<int>(60));

    // 2. Act - sort by dereferenced value using a custom comparator lambda
    tempest::sort(vec.begin(), vec.end(), [](const auto& lhs, const auto& rhs) -> auto { return *lhs < *rhs; });

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        ASSERT_NE(vec[i - 1], nullptr);
        ASSERT_NE(vec[i], nullptr);
        EXPECT_LE(*vec[i - 1], *vec[i]);
    }
    EXPECT_EQ(*vec[0], 10);
    EXPECT_EQ(*vec[vec.size() - 1], 90);
}

/// @brief Verifies sorting heap-allocating, non-trivially copyable types (tempest::string).
TEST(sort_test, sort_non_trivial_strings)
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

//=============================================================================
// Section: Custom Types (operator<, tempest::less specialization, custom comparators)
//=============================================================================

namespace
{
    struct type_with_operator_less
    {
        int value;
        int payload;

        friend constexpr auto operator<(const type_with_operator_less& lhs, const type_with_operator_less& rhs) noexcept
            -> bool
        {
            return lhs.value < rhs.value;
        }
    };

    struct type_with_specialized_less
    {
        int priority;
        int id;
        // Intentionally NO operator< declared
    };

    struct move_counter
    {
        int value;
        static inline int move_assignments = 0;

        move_counter() = default;
        ~move_counter() = default;
        explicit move_counter(int v) : value(v)
        {
        }
        move_counter(const move_counter&) = default;
        auto operator=(const move_counter&) -> move_counter& = default;

        move_counter(move_counter&& other) noexcept : value(other.value)
        {
        }
        auto operator=(move_counter&& other) noexcept -> move_counter&
        {
            value = other.value;
            ++move_assignments;
            return *this;
        }

        friend auto operator<(const move_counter& lhs, const move_counter& rhs) noexcept -> bool
        {
            return lhs.value < rhs.value;
        }
    };
} // namespace

template <>
struct tempest::less<type_with_specialized_less>
{
    constexpr auto operator()(const type_with_specialized_less& lhs,
                              const type_with_specialized_less& rhs) const noexcept -> bool
    {
        return lhs.priority < rhs.priority;
    }
};

/// @brief Verifies that custom types defining operator< are sorted via tempest::sort(first, last) without comparator.
TEST(sort_test, sort_custom_type_via_operator_less)
{
    // 1. Setup
    auto items = tempest::vector<type_with_operator_less>{};
    items.push_back(type_with_operator_less{.value = 45, .payload = 1});
    items.push_back(type_with_operator_less{.value = 12, .payload = 2});
    items.push_back(type_with_operator_less{.value = 89, .payload = 3});
    items.push_back(type_with_operator_less{.value = 3, .payload = 4});
    items.push_back(type_with_operator_less{.value = 67, .payload = 5});

    // 2. Act - uses 2-argument sort with default tempest::less<>{}
    tempest::sort(items.begin(), items.end());

    // 3. Assert
    for (size_t i = 1; i < items.size(); ++i)
    {
        EXPECT_LE(items[i - 1].value, items[i].value);
    }
    EXPECT_EQ(items[0].value, 3);
    EXPECT_EQ(items[items.size() - 1].value, 89);
}

/// @brief Verifies that custom types specializing tempest::less<T> without operator< are sorted via
/// tempest::sort(first, last).
TEST(sort_test, sort_custom_type_via_specialized_tempest_less)
{
    // 1. Setup
    auto items = tempest::vector<type_with_specialized_less>{};
    items.push_back(type_with_specialized_less{.priority = 50, .id = 101});
    items.push_back(type_with_specialized_less{.priority = 10, .id = 102});
    items.push_back(type_with_specialized_less{.priority = 90, .id = 103});
    items.push_back(type_with_specialized_less{.priority = 30, .id = 104});
    items.push_back(type_with_specialized_less{.priority = 70, .id = 105});

    // 2. Act - delegates to tempest::less<type_with_specialized_less> specialization
    tempest::sort(items.begin(), items.end());

    // 3. Assert
    for (size_t i = 1; i < items.size(); ++i)
    {
        EXPECT_LE(items[i - 1].priority, items[i].priority);
    }
    EXPECT_EQ(items[0].id, 102);
    EXPECT_EQ(items[items.size() - 1].id, 103);
}

/// @brief Verifies sorting using a custom binary comparator lambda (e.g. descending order or specific field).
TEST(sort_test, sort_custom_comparator_lambda)
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

    // 2. Act - sort descending by priority
    tempest::sort(items.begin(), items.end(),
                  [](const item& a, const item& b) -> bool { return a.priority > b.priority; });

    // 3. Assert
    for (size_t i = 1; i < items.size(); ++i)
    {
        EXPECT_GE(items[i - 1].priority, items[i].priority);
    }
    EXPECT_EQ(items[0].id, 103);                // priority 9
    EXPECT_EQ(items[items.size() - 1].id, 102); // priority 1
}

//=============================================================================
// Section: Concept & Iterator Range Compatibility (Pointers, Vector, Span)
//=============================================================================

/// @brief Verifies sorting raw C-arrays using contiguous raw pointers as random access iterators.
TEST(sort_test, sort_raw_pointer_range)
{
    // 1. Setup
    int arr[] = {55, 12, 88, 3, 44, 91, 7, 23}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    constexpr auto count = sizeof(arr) / sizeof(arr[0]);

    // 2. Act
    tempest::sort(arr, arr + count);

    // 3. Assert
    for (size_t i = 1; i < count; ++i)
    {
        EXPECT_LE(arr[i - 1], arr[i]);
    }
    EXPECT_EQ(arr[0], 3);
    EXPECT_EQ(arr[count - 1], 91);
}

/// @brief Verifies sorting elements via a tempest::span view.
TEST(sort_test, sort_span_range)
{
    // 1. Setup
    int arr[] = {100, 45, 12, 78, 23, 89, 5, 67}; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    constexpr auto count = sizeof(arr) / sizeof(arr[0]);
    auto view = tempest::span<int>{arr, count};

    // 2. Act
    tempest::sort(view.begin(), view.end());

    // 3. Assert
    for (size_t i = 1; i < count; ++i)
    {
        EXPECT_LE(view[i - 1], view[i]);
    }
    EXPECT_EQ(view[0], 5);
    EXPECT_EQ(view[count - 1], 100);
}

//=============================================================================
// Section: Heapsort Direct Verification
//=============================================================================

/// @brief Directly tests detail::heap_sort to verify correctness of the heapsort fallback implementation.
TEST(sort_test, heapsort_direct_fallback_test)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 45, 12, 89, 3, 67, 23, 99, 1, 34, 78, 56);

    // 2. Act
    tempest::detail::heap_sort(vec.begin(), vec.end(), tempest::less<>{});

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_LE(vec[i - 1], vec[i]);
    }
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[vec.size() - 1], 99);
}

/// @brief Verifies detail::heap_sort on small ranges with both odd and even counts (2, 3, 4, 5, 6, 7).
TEST(sort_test, heapsort_odd_and_even_lengths)
{
    // 1. Setup & 2. Act & 3. Assert
    for (int size = 2; size <= 7; ++size)
    {
        auto vec = tempest::vector<int>{};
        for (int i = size; i > 0; --i)
        {
            vec.push_back(i);
        }

        tempest::detail::heap_sort(vec.begin(), vec.end(), tempest::less<>{});

        for (size_t i = 1; i < vec.size(); ++i)
        {
            EXPECT_LE(vec[i - 1], vec[i]);
        }
        EXPECT_EQ(vec[0], 1);
        EXPECT_EQ(vec[vec.size() - 1], size);
    }
}

/// @brief Directly tests detail::insertion_sort on small unsorted ranges.
TEST(sort_test, insertion_sort_direct_test)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 15, 3, 8, 1, 12, 6, 14, 2);

    // 2. Act
    tempest::detail::insertion_sort(vec.begin(), vec.end(), tempest::less<>{});

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_LE(vec[i - 1], vec[i]);
    }
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[vec.size() - 1], 15);
}

/// @brief Verifies sorting in descending order using tempest::greater<>{}.
TEST(sort_test, sort_descending_via_tempest_greater)
{
    // 1. Setup
    auto vec = tempest::vector<int>(tempest::init_list, 4, 1, 7, 2, 9, 3, 8, 5, 6);

    // 2. Act
    tempest::sort(vec.begin(), vec.end(), tempest::greater<>{});

    // 3. Assert
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_GE(vec[i - 1], vec[i]);
    }
    EXPECT_EQ(vec[0], 9);
    EXPECT_EQ(vec[vec.size() - 1], 1);
}

/// @brief Verifies that insertion_sort on already-sorted elements performs zero move assignments.
TEST(sort_test, insertion_sort_move_efficiency_on_sorted_data)
{
    // 1. Setup - 10 already-sorted elements (handled entirely by insertion sort path)
    auto vec = tempest::vector<move_counter>{};
    for (int i = 1; i <= 10; ++i)
    {
        vec.push_back(move_counter{i});
    }

    move_counter::move_assignments = 0;

    // 2. Act
    tempest::detail::insertion_sort(vec.begin(), vec.end(), tempest::less<>{});

    // 3. Assert - already sorted, so zero move assignments should occur
    EXPECT_EQ(move_counter::move_assignments, 0);
    for (size_t i = 1; i < vec.size(); ++i)
    {
        EXPECT_LE(vec[i - 1].value, vec[i].value);
    }
}

// NOLINTEND(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers,readability-identifier-length,readability-function-cognitive-complexity)
