#include <gtest/gtest.h>

#include <tempest/int.hpp>
#include <tempest/memory.hpp>

//==============================================================================
// system_allocator Tests
//==============================================================================

/// @brief Verify system_allocator successfully allocates and deallocates memory with default and custom alignments.
TEST(memory_tests, system_allocator_basic_allocation)
{
    // Setup: Instantiate system_allocator
    auto alloc = tempest::system_allocator{};

    // Act: Allocate memory with standard alignments (8, 16, 64 bytes)
    auto* ptr8 = alloc.allocate(128, 8);
    auto* ptr16 = alloc.allocate(256, 16);
    auto* ptr64 = alloc.allocate(512, 64);

    // Assert: Memory is non-null and aligned to requested boundaries
    ASSERT_NE(ptr8, nullptr);
    ASSERT_NE(ptr16, nullptr);
    ASSERT_NE(ptr64, nullptr);

    EXPECT_EQ(reinterpret_cast<tempest::uintptr_t>(ptr8) % 8, 0U);
    EXPECT_EQ(reinterpret_cast<tempest::uintptr_t>(ptr16) % 16, 0U);
    EXPECT_EQ(reinterpret_cast<tempest::uintptr_t>(ptr64) % 64, 0U);

    // Cleanup
    alloc.deallocate(ptr8);
    alloc.deallocate(ptr16);
    alloc.deallocate(ptr64);
}

/// @brief Verify system_allocator handles various allocation sizes from 1 byte to multi-megabytes.
TEST(memory_tests, system_allocator_varying_sizes)
{
    // Setup: Instantiate system_allocator
    auto alloc = tempest::system_allocator{};

    // Act & Assert: Test 0-byte allocation returns nullptr
    auto* ptr_zero = alloc.allocate(0, 8);
    EXPECT_EQ(ptr_zero, nullptr);

    // Act & Assert: Test 1-byte allocation
    auto* ptr1 = alloc.allocate(1, 1);
    ASSERT_NE(ptr1, nullptr);
    *static_cast<char*>(ptr1) = 'A';
    EXPECT_EQ(*static_cast<char*>(ptr1), 'A');
    alloc.deallocate(ptr1);

    // Act & Assert: Test 4 MB allocation
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
    // Setup: Create initial heap_allocator with 64 KB pool
    constexpr auto pool_size = 64 * 1024;
    auto heap1 = tempest::heap_allocator{pool_size};

    // Act: Allocate and verify writeability
    auto* p1 = heap1.allocate(1024, 16);
    ASSERT_NE(p1, nullptr);
    *static_cast<int*>(p1) = 12345;
    EXPECT_EQ(*static_cast<int*>(p1), 12345);

    // Act: Move construct heap2 from heap1
    auto heap2 = tempest::move(heap1);

    // Assert: heap2 can allocate and deallocate previously allocated memory
    auto* p2 = heap2.allocate(512, 16);
    ASSERT_NE(p2, nullptr);
    heap2.deallocate(p1);
    heap2.deallocate(p2);

    // Act: Move assign heap3 from heap2
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
    // Setup: Create stack allocator with 16 KB buffer
    constexpr auto stack_size = 16 * 1024;
    auto stack1 = tempest::stack_allocator{stack_size};

    // Act: Allocate memory and inspect markers
    auto* p1 = stack1.allocate(100, 8);
    ASSERT_NE(p1, nullptr);
    auto marker = stack1.get_marker();
    EXPECT_GE(marker, 100U);

    auto* p2 = stack1.allocate(200, 16);
    ASSERT_NE(p2, nullptr);
    EXPECT_GT(stack1.get_marker(), marker);

    // Act: Roll back to marker
    stack1.free_marker(marker);
    EXPECT_EQ(stack1.get_marker(), marker);

    // Act: Move construct stack2 from stack1
    auto stack2 = tempest::move(stack1);
    EXPECT_EQ(stack2.get_marker(), marker);

    // Allocate from moved-to stack
    auto* p3 = stack2.allocate(50, 8);
    ASSERT_NE(p3, nullptr);

    // Act: Move assign stack3 from stack2
    auto stack3 = tempest::stack_allocator{stack_size};
    stack3 = tempest::move(stack2);

    // Assert: stack3 is functional and reset clears marker
    stack3.reset();
    EXPECT_EQ(stack3.get_marker(), 0U);
}

/// @brief Verify stack_allocator safely deallocates the first allocation (offset 0) and nullptr without asserting.
TEST(memory_tests, stack_allocator_first_allocation_dealloc)
{
    // Setup: Create stack allocator
    constexpr auto stack_size = 4096;
    auto stack = tempest::stack_allocator{stack_size};

    // Act: Allocate with alignment 1 to ensure offset 0 at buffer start
    auto* const ptr = stack.allocate(64, 1);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(stack.get_marker(), 64U);

    // Deallocate first allocation at offset 0
    stack.deallocate(ptr);
    EXPECT_EQ(stack.get_marker(), 0U);

    // Deallocate nullptr is a safe no-op
    stack.deallocate(nullptr);
    EXPECT_EQ(stack.get_marker(), 0U);
}

/// @brief Verify stack_allocator marker rewind and rejection of invalid forward markers without unsigned underflow.
TEST(memory_tests, stack_allocator_marker_rewind_and_underflow_safety)
{
    // Setup: Create stack allocator and allocate successive blocks
    constexpr auto stack_size = 4096;
    auto stack = tempest::stack_allocator{stack_size};

    auto* const p1 = stack.allocate(128, 8);
    ASSERT_NE(p1, nullptr);
    const auto marker1 = stack.get_marker();

    auto* const p2 = stack.allocate(256, 8);
    ASSERT_NE(p2, nullptr);
    const auto marker2 = stack.get_marker();
    EXPECT_GT(marker2, marker1);

    // Act: Roll back to marker1
    stack.free_marker(marker1);
    EXPECT_EQ(stack.get_marker(), marker1);

    // Act: Attempt invalid forward rewind to marker2 (should be rejected)
    stack.free_marker(marker2);
    EXPECT_EQ(stack.get_marker(), marker1);

    // Act: Roll back to 0
    stack.free_marker(0);
    EXPECT_EQ(stack.get_marker(), 0U);
}

/// @brief Adversarially stress test stack_allocator deallocation boundaries, offset 0, intermediate blocks, and nullptr across states.
TEST(memory_tests, stack_allocator_adversarial_boundary_and_null_dealloc)
{
    // Setup: Create stack allocator
    constexpr auto stack_size = 4096;
    auto stack = tempest::stack_allocator{stack_size};

    // Assert: nullptr deallocation on fresh stack is a safe no-op
    stack.deallocate(nullptr);
    EXPECT_EQ(stack.get_marker(), 0U);

    // Act: Allocate first block at offset 0 with 64-byte alignment
    auto* const first_ptr = stack.allocate(128, 64);
    ASSERT_NE(first_ptr, nullptr);
    const auto marker_after_first = stack.get_marker();
    EXPECT_GE(marker_after_first, 128U);

    // Assert: nullptr deallocation with active allocations is a safe no-op
    stack.deallocate(nullptr);
    EXPECT_EQ(stack.get_marker(), marker_after_first);

    // Act: Allocate second block
    auto* const second_ptr = stack.allocate(256, 16);
    ASSERT_NE(second_ptr, nullptr);
    const auto marker_after_second = stack.get_marker();
    EXPECT_GT(marker_after_second, marker_after_first);

    // Act: Deallocate intermediate block (rewind to second_ptr)
    stack.deallocate(second_ptr);
    EXPECT_EQ(stack.get_marker(), marker_after_first);

    // Act: Re-allocate into freed space
    auto* const replacement_ptr = stack.allocate(256, 16);
    ASSERT_NE(replacement_ptr, nullptr);
    EXPECT_EQ(replacement_ptr, second_ptr);

    // Act: Deallocate initial block at offset 0
    stack.deallocate(first_ptr);
    EXPECT_EQ(stack.get_marker(), 0U);

    // Act: Allocate again from empty stack, confirming complete reuse
    auto* const reused_ptr = stack.allocate(64, 1);
    ASSERT_NE(reused_ptr, nullptr);
    EXPECT_EQ(reused_ptr, first_ptr);
    EXPECT_EQ(stack.get_marker(), 64U);

    // Act: Reset and verify nullptr deallocation on reset stack
    stack.reset();
    EXPECT_EQ(stack.get_marker(), 0U);
    stack.deallocate(nullptr);
    EXPECT_EQ(stack.get_marker(), 0U);

    // Act: Release and verify nullptr deallocation on released stack
    stack.release();
    EXPECT_EQ(stack.get_marker(), 0U);
    stack.deallocate(nullptr);
    EXPECT_EQ(stack.get_marker(), 0U);
}

/// @brief Adversarially stress test stack_allocator marker rewind boundary conditions, including zero, capacity, and forward overflow.
TEST(memory_tests, stack_allocator_adversarial_marker_rewind_stress)
{
    // Setup: Create stack allocator
    constexpr auto stack_size = 1024;
    auto stack = tempest::stack_allocator{stack_size};

    // Assert: Rewind on empty stack to 0 is a safe no-op
    stack.free_marker(0);
    EXPECT_EQ(stack.get_marker(), 0U);

    // Assert: Invalid forward rewind to capacity on empty stack is rejected
    stack.free_marker(stack_size);
    EXPECT_EQ(stack.get_marker(), 0U);

    // Assert: Invalid forward rewind to maximum size_t value is rejected without underflow
    stack.free_marker(~size_t(0));
    EXPECT_EQ(stack.get_marker(), 0U);

    // Act: Fill allocator to exact capacity
    auto* const full_block = stack.allocate(stack_size, 1);
    ASSERT_NE(full_block, nullptr);
    EXPECT_EQ(stack.get_marker(), stack_size);

    // Assert: Rewind to exact capacity when fully allocated is valid
    stack.free_marker(stack_size);
    EXPECT_EQ(stack.get_marker(), stack_size);

    // Assert: Invalid forward rewind past capacity is rejected
    stack.free_marker(stack_size + 1);
    EXPECT_EQ(stack.get_marker(), stack_size);
    stack.free_marker(stack_size + 500);
    EXPECT_EQ(stack.get_marker(), stack_size);
    stack.free_marker(~size_t(0));
    EXPECT_EQ(stack.get_marker(), stack_size);

    // Act: Rewind to half capacity
    constexpr auto half_capacity = stack_size / 2;
    stack.free_marker(half_capacity);
    EXPECT_EQ(stack.get_marker(), half_capacity);

    // Assert: Forward rewind back to full capacity is rejected
    stack.free_marker(stack_size);
    EXPECT_EQ(stack.get_marker(), half_capacity);

    // Act: Rewind to zero
    stack.free_marker(0);
    EXPECT_EQ(stack.get_marker(), 0U);

    // Assert: Allocator can be refilled to full capacity after rewind to zero
    auto* const refilled_block = stack.allocate(stack_size, 1);
    ASSERT_NE(refilled_block, nullptr);
    EXPECT_EQ(refilled_block, full_block);
    EXPECT_EQ(stack.get_marker(), stack_size);
}

/// @brief Adversarially test stack_allocator self-move assignment, moved-from state operations, and lifecycle transfers.
TEST(memory_tests, stack_allocator_adversarial_self_move_and_moved_from_state)
{
    // Setup: Create stack allocator and allocate initial block
    constexpr auto stack_size = 2048;
    auto stack = tempest::stack_allocator{stack_size};

    auto* const ptr1 = stack.allocate(512, 16);
    ASSERT_NE(ptr1, nullptr);
    const auto marker_before_self_move = stack.get_marker();

    // Act: Self-move assignment
    stack = tempest::move(stack);

    // Assert: Self-move preserves state intact
    EXPECT_EQ(stack.get_marker(), marker_before_self_move);
    auto* const ptr2 = stack.allocate(256, 16);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_GT(stack.get_marker(), marker_before_self_move);

    // Act: Move construct into another instance
    auto moved_target = tempest::move(stack);

    // Assert: Moved-from instance has zero marker and safely ignores nullptr dealloc
    EXPECT_EQ(stack.get_marker(), 0U);
    stack.deallocate(nullptr);
    stack.free_marker(0);
    stack.free_marker(100);
    stack.reset();
    stack.release();
    EXPECT_EQ(stack.get_marker(), 0U);

    // Assert: Target instance preserved all previous state
    EXPECT_EQ(moved_target.get_marker(), 768U);

    // Act: Move assign into a third instance
    auto third_stack = tempest::stack_allocator{1024};
    third_stack = tempest::move(moved_target);

    EXPECT_EQ(moved_target.get_marker(), 0U);
    EXPECT_EQ(third_stack.get_marker(), 768U);
}

/// @brief Adversarially test rapid interleaved allocation, rewind, and re-allocation patterns.
TEST(memory_tests, stack_allocator_adversarial_interleaved_churn)
{
    // Setup: Create stack allocator
    constexpr auto stack_size = 8192;
    auto stack = tempest::stack_allocator{stack_size};

    for (size_t iteration = 0; iteration < 100; ++iteration)
    {
        const auto base_marker = stack.get_marker();

        auto* const block_a = stack.allocate(64, 8);
        ASSERT_NE(block_a, nullptr);

        auto* const block_b = stack.allocate(128, 16);
        ASSERT_NE(block_b, nullptr);
        const auto marker_b = stack.get_marker();

        auto* const block_c = stack.allocate(256, 16);
        ASSERT_NE(block_c, nullptr);

        // Act: Rewind to marker_b and re-allocate
        stack.free_marker(marker_b);
        EXPECT_EQ(stack.get_marker(), marker_b);

        auto* const replacement_c = stack.allocate(128, 8);
        ASSERT_NE(replacement_c, nullptr);

        // Act: Rewind all the way to base_marker
        stack.free_marker(base_marker);
        EXPECT_EQ(stack.get_marker(), base_marker);
    }

    EXPECT_EQ(stack.get_marker(), 0U);
}

//==============================================================================
// max_align_t Tests
//==============================================================================

/// @brief Verify tempest::max_align_t alignment satisfies scalar and pointer alignment requirements.
TEST(memory_tests, max_align_t_alignment_invariants)
{
    // Assert: max_align_t alignment is at least as strict as all fundamental scalar types
    EXPECT_GE(alignof(tempest::max_align_t), alignof(char));
    EXPECT_GE(alignof(tempest::max_align_t), alignof(short));
    EXPECT_GE(alignof(tempest::max_align_t), alignof(int));
    EXPECT_GE(alignof(tempest::max_align_t), alignof(long));
    EXPECT_GE(alignof(tempest::max_align_t), alignof(long long));
    EXPECT_GE(alignof(tempest::max_align_t), alignof(float));
    EXPECT_GE(alignof(tempest::max_align_t), alignof(double));
    EXPECT_GE(alignof(tempest::max_align_t), alignof(long double));
    EXPECT_GE(alignof(tempest::max_align_t), alignof(void*));
    EXPECT_GE(alignof(tempest::max_align_t), alignof(tempest::max_align_t*));
}

