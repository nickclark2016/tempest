#include <tempest/queue.hpp>

#include <gtest/gtest.h>

#include <tempest/memory.hpp>
#include <tempest/string_view.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/vector.hpp>

//=============================================================================
// Construction and Basic State Tests
//=============================================================================

/// @brief Verifies that default construction creates an empty queue with size 0.
TEST(queue_test, default_constructor)
{
    // Setup & Act
    auto test_queue = tempest::queue<int>{};

    // Assert
    EXPECT_TRUE(test_queue.empty());
    EXPECT_EQ(test_queue.size(), 0U);
}

/// @brief Verifies construction from an existing container by copy and move.
TEST(queue_test, container_constructors)
{
    // Setup
    auto test_deque = tempest::deque<int>{};
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    test_deque.push_back(10);
    test_deque.push_back(20);
    test_deque.push_back(30);

    // Act - Copy construction from container
    auto queue_copied = tempest::queue<int>{test_deque};

    // Assert
    EXPECT_FALSE(queue_copied.empty());
    EXPECT_EQ(queue_copied.size(), 3U);
    EXPECT_EQ(queue_copied.front(), 10);
    EXPECT_EQ(queue_copied.back(), 30);
    EXPECT_EQ(test_deque.size(), 3U);

    // Act - Move construction from container
    auto queue_moved = tempest::queue<int>{tempest::move(test_deque)};

    // Assert
    EXPECT_FALSE(queue_moved.empty());
    EXPECT_EQ(queue_moved.size(), 3U);
    EXPECT_EQ(queue_moved.front(), 10);
    EXPECT_EQ(queue_moved.back(), 30);
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

/// @brief Verifies queue copy and move construction and assignment.
TEST(queue_test, copy_and_move_semantics)
{
    // Setup
    auto queue_one = tempest::queue<int>{};
    queue_one.push(1);
    queue_one.push(2);
    queue_one.push(3);

    // Act - Copy constructor
    auto queue_two = queue_one;

    // Assert
    EXPECT_EQ(queue_two.size(), 3U);
    EXPECT_EQ(queue_two.front(), 1);
    EXPECT_EQ(queue_two.back(), 3);
    EXPECT_EQ(queue_one.size(), 3U);

    // Act - Move constructor
    auto queue_three = tempest::move(queue_one);

    // Assert
    EXPECT_EQ(queue_three.size(), 3U);
    EXPECT_EQ(queue_three.front(), 1);
    EXPECT_EQ(queue_three.back(), 3);

    // Act - Copy assignment
    auto queue_four = tempest::queue<int>{};
    queue_four = queue_two;

    // Assert
    EXPECT_EQ(queue_four.size(), 3U);
    EXPECT_EQ(queue_four.front(), 1);
    EXPECT_EQ(queue_four.back(), 3);

    // Act - Move assignment
    auto queue_five = tempest::queue<int>{};
    queue_five = tempest::move(queue_three);

    // Assert
    EXPECT_EQ(queue_five.size(), 3U);
    EXPECT_EQ(queue_five.front(), 1);
    EXPECT_EQ(queue_five.back(), 3);
}

//=============================================================================
// FIFO Ordering and Push/Pop Operations
//=============================================================================

// NOLINTBEGIN(readability-function-cognitive-complexity)
/// @brief Verifies that elements are strictly popped in first-in, first-out order.
TEST(queue_test, fifo_ordering)
{
    // Setup
    auto test_queue = tempest::queue<int>{};

    // Act - Push large sequence of integers
    constexpr auto element_count = 1000;
    for (auto idx = 0; idx < element_count; ++idx)
    {
        test_queue.push(idx);
        EXPECT_EQ(test_queue.back(), idx);
        EXPECT_EQ(test_queue.front(), 0);
        EXPECT_EQ(test_queue.size(), static_cast<size_t>(idx + 1));
    }

    // Assert - Pop and verify sequential FIFO ordering
    for (auto idx = 0; idx < element_count; ++idx)
    {
        EXPECT_FALSE(test_queue.empty());
        EXPECT_EQ(test_queue.front(), idx);
        EXPECT_EQ(test_queue.back(), element_count - 1);
        test_queue.pop();
        EXPECT_EQ(test_queue.size(), static_cast<size_t>(element_count - 1 - idx));
    }

    EXPECT_TRUE(test_queue.empty());
    EXPECT_EQ(test_queue.size(), 0U);
}
// NOLINTEND(readability-function-cognitive-complexity)

/// @brief Verifies front() and back() mutability on non-const queues.
TEST(queue_test, front_back_mutability)
{
    // Setup
    auto test_queue = tempest::queue<int>{};
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    test_queue.push(10);
    test_queue.push(20);

    // Act - Mutate front and back via references
    test_queue.front() = 100;
    test_queue.back() = 200;

    // Assert
    EXPECT_EQ(test_queue.front(), 100);
    EXPECT_EQ(test_queue.back(), 200);

    // Verify const access
    const auto& const_queue_ref = test_queue;
    EXPECT_EQ(const_queue_ref.front(), 100);
    EXPECT_EQ(const_queue_ref.back(), 200);
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

//=============================================================================
// Emplace and In-Place Construction
//=============================================================================

namespace
{
    struct test_composite
    {
        int a;
        double b;
        tempest::string_view tag;

        test_composite(int in_a, double in_b, tempest::string_view in_tag) : a(in_a), b(in_b), tag(in_tag)
        {
        }
    };
} // namespace

/// @brief Verifies in-place emplace forward-constructs elements and returns reference.
TEST(queue_test, emplace_in_place)
{
    // Setup
    auto test_queue = tempest::queue<test_composite>{};

    // Act - Emplace with multiple constructor parameters
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    auto& ref1 = test_queue.emplace(42, 3.14, "first");

    // Assert
    EXPECT_EQ(test_queue.size(), 1U);
    EXPECT_EQ(ref1.a, 42);
    EXPECT_DOUBLE_EQ(ref1.b, 3.14);
    EXPECT_EQ(ref1.tag, "first");
    EXPECT_EQ(&ref1, &test_queue.front());
    EXPECT_EQ(&ref1, &test_queue.back());

    // Act - Emplace second element
    constexpr auto test_e_approx = 2.718281828459045;
    auto& ref2 = test_queue.emplace(99, test_e_approx, "second");

    // Assert
    EXPECT_EQ(test_queue.size(), 2U);
    EXPECT_EQ(ref2.a, 99);
    EXPECT_DOUBLE_EQ(ref2.b, test_e_approx);
    EXPECT_EQ(&ref2, &test_queue.back());
    EXPECT_EQ(test_queue.front().a, 42);
    EXPECT_EQ(test_queue.back().a, 99);
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

//=============================================================================
// Move-Only Types Support
//=============================================================================

/// @brief Verifies tempest::queue supports move-only elements (tempest::unique_ptr).
TEST(queue_test, move_only_elements)
{
    // Static assertion that copy constructor is disabled for move-only queue
    static_assert(!tempest::is_copy_constructible_v<tempest::queue<tempest::unique_ptr<int>>>);
    static_assert(tempest::is_move_constructible_v<tempest::queue<tempest::unique_ptr<int>>>);

    // Setup
    auto test_queue = tempest::queue<tempest::unique_ptr<int>>{};

    // Act - Push rvalues
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    test_queue.push(tempest::make_unique<int>(10));
    test_queue.push(tempest::make_unique<int>(20));

    // Act - Emplace
    auto& emplaced = test_queue.emplace(tempest::make_unique<int>(30));
    EXPECT_EQ(*emplaced, 30);

    // Assert
    EXPECT_EQ(test_queue.size(), 3U);
    EXPECT_EQ(*test_queue.front(), 10);
    EXPECT_EQ(*test_queue.back(), 30);

    // Act - Transfer ownership out from front() and pop
    auto extracted = tempest::move(test_queue.front());
    EXPECT_EQ(*extracted, 10);
    test_queue.pop();

    EXPECT_EQ(test_queue.size(), 2U);
    EXPECT_EQ(*test_queue.front(), 20);

    // Act - Move the entire queue
    auto moved_queue = tempest::move(test_queue);
    EXPECT_EQ(moved_queue.size(), 2U);
    EXPECT_EQ(*moved_queue.front(), 20);
    EXPECT_EQ(*moved_queue.back(), 30);
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

//=============================================================================
// Swap Semantics
//=============================================================================

/// @brief Verifies member and non-member swap between queues of different sizes.
TEST(queue_test, member_and_non_member_swap)
{
    // Setup
    auto queue_one = tempest::queue<int>{};
    queue_one.push(1);
    queue_one.push(2);

    auto queue_two = tempest::queue<int>{};
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    queue_two.push(10);
    queue_two.push(20);
    queue_two.push(30);

    // Act - Member swap
    queue_one.swap(queue_two);

    // Assert
    EXPECT_EQ(queue_one.size(), 3U);
    EXPECT_EQ(queue_one.front(), 10);
    EXPECT_EQ(queue_one.back(), 30);

    EXPECT_EQ(queue_two.size(), 2U);
    EXPECT_EQ(queue_two.front(), 1);
    EXPECT_EQ(queue_two.back(), 2);

    // Act - Non-member swap (tempest::swap / ADL)
    swap(queue_one, queue_two);

    // Assert
    EXPECT_EQ(queue_one.size(), 2U);
    EXPECT_EQ(queue_one.front(), 1);
    EXPECT_EQ(queue_one.back(), 2);

    EXPECT_EQ(queue_two.size(), 3U);
    EXPECT_EQ(queue_two.front(), 10);
    EXPECT_EQ(queue_two.back(), 30);
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

/// @brief Verifies swap with an empty queue.
TEST(queue_test, swap_with_empty)
{
    // Setup
    auto queue_filled = tempest::queue<int>{};
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    queue_filled.push(42);
    queue_filled.push(84);

    auto queue_empty = tempest::queue<int>{};

    // Act
    queue_filled.swap(queue_empty);

    // Assert
    EXPECT_TRUE(queue_filled.empty());
    EXPECT_EQ(queue_filled.size(), 0U);

    EXPECT_FALSE(queue_empty.empty());
    EXPECT_EQ(queue_empty.size(), 2U);
    EXPECT_EQ(queue_empty.front(), 42);
    EXPECT_EQ(queue_empty.back(), 84);
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

//=============================================================================
// Comparison Operators
//=============================================================================

/// @brief Verifies equality and relational comparison operators.
TEST(queue_test, comparison_operators)
{
    // Setup
    auto queue_one = tempest::queue<int>{};
    queue_one.push(1);
    queue_one.push(2);
    queue_one.push(3);

    auto queue_two = tempest::queue<int>{};
    queue_two.push(1);
    queue_two.push(2);
    queue_two.push(3);

    auto queue_three = tempest::queue<int>{};
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    queue_three.push(1);
    queue_three.push(2);
    queue_three.push(4);

    auto queue_four = tempest::queue<int>{};
    queue_four.push(1);
    queue_four.push(2);

    // Assert - Equality & Inequality
    EXPECT_TRUE(queue_one == queue_two);
    EXPECT_FALSE(queue_one != queue_two);
    EXPECT_FALSE(queue_one == queue_three);
    EXPECT_TRUE(queue_one != queue_three);
    EXPECT_FALSE(queue_one == queue_four);
    EXPECT_TRUE(queue_one != queue_four);

    // Assert - Ordering comparisons
    EXPECT_TRUE(queue_one < queue_three);
    EXPECT_TRUE(queue_one <= queue_three);
    EXPECT_FALSE(queue_one > queue_three);
    EXPECT_FALSE(queue_one >= queue_three);

    EXPECT_TRUE(queue_four < queue_one);
    EXPECT_TRUE(queue_four <= queue_one);
    EXPECT_TRUE(queue_one > queue_four);
    EXPECT_TRUE(queue_one >= queue_four);

    EXPECT_TRUE(queue_one <= queue_two);
    EXPECT_TRUE(queue_one >= queue_two);
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

//=============================================================================
// CTAD & Edge Cases
//=============================================================================

/// @brief Verifies Class Template Argument Deduction (CTAD) from underlying container.
TEST(queue_test, deduction_guide)
{
    // Setup
    auto test_deque = tempest::deque<int>{};
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    test_deque.push_back(100);
    test_deque.push_back(200);

    // Act - Deduce queue<int, deque<int>> from deque<int>
    auto deduced_queue = tempest::queue(test_deque);

    // Assert
    EXPECT_EQ(deduced_queue.size(), 2U);
    EXPECT_EQ(deduced_queue.front(), 100);
    EXPECT_EQ(deduced_queue.back(), 200);
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
/// @brief Verifies ring-buffer wrap-around and memory safety under sustained push/pop cycle.
TEST(queue_test, circular_buffer_wrap_around)
{
    // Setup
    auto test_queue = tempest::queue<int>{};

    // Pre-populate with a working window of elements
    constexpr auto window_size = 50;
    for (auto idx = 0; idx < window_size; ++idx)
    {
        test_queue.push(idx);
    }

    // Act - Sustain push and pop across multiple block boundaries
    constexpr auto iterations = 5000;
    for (auto idx = 0; idx < iterations; ++idx)
    {
        EXPECT_EQ(test_queue.front(), idx);
        test_queue.pop();
        test_queue.push(idx + window_size);
        EXPECT_EQ(test_queue.back(), idx + window_size);
        EXPECT_EQ(test_queue.size(), static_cast<size_t>(window_size));
    }

    // Assert - Drain the remaining window
    for (auto idx = 0; idx < window_size; ++idx)
    {
        EXPECT_EQ(test_queue.front(), iterations + idx);
        test_queue.pop();
    }

    EXPECT_TRUE(test_queue.empty());
    EXPECT_EQ(test_queue.size(), 0U);
}
// NOLINTEND(readability-function-cognitive-complexity)

/// @brief Verifies deque emplace_front and push_front with move semantics.
TEST(queue_test, deque_push_front_move_and_emplace)
{
    // Setup
    auto test_deque = tempest::deque<tempest::unique_ptr<int>>{};

    // Act - push_front(T&&) and emplace_front
    test_deque.push_front(tempest::make_unique<int>(1));
    test_deque.emplace_front(tempest::make_unique<int>(2));
    test_deque.emplace_front(tempest::make_unique<int>(3));

    // Assert
    EXPECT_EQ(test_deque.size(), 3U);
    EXPECT_EQ(*test_deque.front(), 3);
    EXPECT_EQ(*test_deque[1], 2);
    EXPECT_EQ(*test_deque.back(), 1);
}
