#include <gtest/gtest.h>

#include <tempest/int.hpp>
#include <tempest/memory.hpp>

//==============================================================================
// system_allocator Tests
//==============================================================================

/// @brief Verify system_allocator successfully allocates and deallocates memory with default and custom alignments.
TEST(memory_tests, system_allocator_basic_allocation)
{
    // 1. Setup: Instantiate system_allocator
    auto alloc = tempest::system_allocator{};

    // 2. Act: Allocate memory with standard alignments (8, 16, 64 bytes)
    auto* ptr8 = alloc.allocate(128, 8);
    auto* ptr16 = alloc.allocate(256, 16);
    auto* ptr64 = alloc.allocate(512, 64);

    // 3. Assert: Memory is non-null and aligned to requested boundaries
    ASSERT_NE(ptr8, nullptr);
    ASSERT_NE(ptr16, nullptr);
    ASSERT_NE(ptr64, nullptr);

    EXPECT_EQ(reinterpret_cast<tempest::uintptr_t>(ptr8) % 8, 0U);
    EXPECT_EQ(reinterpret_cast<tempest::uintptr_t>(ptr16) % 16, 0U);
    EXPECT_EQ(reinterpret_cast<tempest::uintptr_t>(ptr64) % 64, 0U);

    // 4. Cleanup
    alloc.deallocate(ptr8);
    alloc.deallocate(ptr16);
    alloc.deallocate(ptr64);
}

/// @brief Verify system_allocator handles various allocation sizes from 1 byte to multi-megabytes.
TEST(memory_tests, system_allocator_varying_sizes)
{
    // 1. Setup: Instantiate system_allocator
    auto alloc = tempest::system_allocator{};

    // 2. Act & Assert: Test 0-byte allocation returns nullptr
    auto* ptr_zero = alloc.allocate(0, 8);
    EXPECT_EQ(ptr_zero, nullptr);

    // 3. Act & Assert: Test 1-byte allocation
    auto* ptr1 = alloc.allocate(1, 1);
    ASSERT_NE(ptr1, nullptr);
    *static_cast<char*>(ptr1) = 'A';
    EXPECT_EQ(*static_cast<char*>(ptr1), 'A');
    alloc.deallocate(ptr1);

    // 4. Act & Assert: Test 4 MB allocation
    constexpr auto large_size = 4 * 1024 * 1024;
    auto* ptr_large = alloc.allocate(large_size, 16);
    ASSERT_NE(ptr_large, nullptr);

    // Verify memory writeability
    auto* bytes = static_cast<unsigned char*>(ptr_large);
    bytes[0] = 0xAA;
    bytes[large_size - 1] = 0x55;
    EXPECT_EQ(bytes[0], 0xAA);
    EXPECT_EQ(bytes[large_size - 1], 0x55);

    alloc.deallocate(ptr_large);
}

//==============================================================================
// heap_allocator Tests
//==============================================================================

/// @brief Verify heap_allocator basic allocation, deallocation, move construction, and move assignment.
TEST(memory_tests, heap_allocator_allocation_and_move)
{
    // 1. Setup: Create initial heap_allocator with 64 KB pool
    constexpr auto pool_size = 64 * 1024;
    auto heap1 = tempest::heap_allocator{pool_size};

    // 2. Act: Allocate and verify writeability
    auto* p1 = heap1.allocate(1024, 16);
    ASSERT_NE(p1, nullptr);
    *static_cast<int*>(p1) = 12345;
    EXPECT_EQ(*static_cast<int*>(p1), 12345);

    // 3. Act: Move construct heap2 from heap1
    auto heap2 = tempest::move(heap1);

    // Assert: heap2 can allocate and deallocate previously allocated memory
    auto* p2 = heap2.allocate(512, 16);
    ASSERT_NE(p2, nullptr);
    heap2.deallocate(p1);
    heap2.deallocate(p2);

    // 4. Act: Move assign heap3 from heap2
    auto heap3 = tempest::heap_allocator{pool_size};
    heap3 = tempest::move(heap2);

    // Assert: heap3 is functional
    auto* p3 = heap3.allocate(256, 16);
    ASSERT_NE(p3, nullptr);
    heap3.deallocate(p3);
}

//==============================================================================
// stack_allocator Tests
//==============================================================================

/// @brief Verify stack_allocator bump allocation, markers, reset, and move semantics.
TEST(memory_tests, stack_allocator_allocation_and_move)
{
    // 1. Setup: Create stack allocator with 16 KB buffer
    constexpr auto stack_size = 16 * 1024;
    auto stack1 = tempest::stack_allocator{stack_size};

    // 2. Act: Allocate memory and inspect markers
    auto* p1 = stack1.allocate(100, 8);
    ASSERT_NE(p1, nullptr);
    auto marker = stack1.get_marker();
    EXPECT_GE(marker, 100U);

    auto* p2 = stack1.allocate(200, 16);
    ASSERT_NE(p2, nullptr);
    EXPECT_GT(stack1.get_marker(), marker);

    // 3. Act: Roll back to marker
    stack1.free_marker(marker);
    EXPECT_EQ(stack1.get_marker(), marker);

    // 4. Act: Move construct stack2 from stack1
    auto stack2 = tempest::move(stack1);
    EXPECT_EQ(stack2.get_marker(), marker);

    // Allocate from moved-to stack
    auto* p3 = stack2.allocate(50, 8);
    ASSERT_NE(p3, nullptr);

    // 5. Act: Move assign stack3 from stack2
    auto stack3 = tempest::stack_allocator{stack_size};
    stack3 = tempest::move(stack2);

    // Assert: stack3 is functional and reset clears marker
    stack3.reset();
    EXPECT_EQ(stack3.get_marker(), 0U);
}
