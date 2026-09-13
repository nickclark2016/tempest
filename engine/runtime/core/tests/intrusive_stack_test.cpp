#include <tempest/intrusive_stack.hpp>

#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/checked.hpp>
#include <tempest/int.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

namespace
{
    struct base_hook_node : tempest::treiber_node<base_hook_node>
    {
        int32_t value{0};
    };

    struct member_hook_node
    {
        int32_t value{0};
        tempest::treiber_node<member_hook_node> hook;
    };

    struct stress_node : tempest::treiber_node<stress_node>
    {
        uint32_t thread_index{0};
        uint32_t sequence_id{0};
    };
} // namespace

//=============================================================================
// 1. Base-Class Hook Tests
//=============================================================================

/// @brief Verifies that a base-hook node stack supports push, drain, and reverse in LIFO/FIFO order.
TEST(intrusive_stack_test, base_hook_push_drain_reverse)
{
    // 1. Setup
    auto stack = tempest::intrusive_mpsc_stack<base_hook_node>{};
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.drain(), nullptr);

    auto node_one = base_hook_node{};
    node_one.value = 10;
    auto node_two = base_hook_node{};
    node_two.value = 20;
    auto node_three = base_hook_node{};
    node_three.value = 30;

    // 2. Act: Push nodes one by one
    stack.push(tempest::non_null<base_hook_node>{node_one});
    EXPECT_FALSE(stack.empty());
    stack.push(tempest::non_null<base_hook_node>{node_two});
    stack.push(tempest::non_null<base_hook_node>{node_three});

    // 3. Act: Drain the stack
    auto* drained_head = stack.drain();
    EXPECT_TRUE(stack.empty());

    // 4. Assert: Treiber stack order is LIFO (30 -> 20 -> 10 -> nullptr)
    ASSERT_NE(drained_head, nullptr);
    EXPECT_EQ(drained_head->value, 30);

    auto* current_node = tempest::intrusive_mpsc_stack<base_hook_node>::next(drained_head);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 20);

    current_node = tempest::intrusive_mpsc_stack<base_hook_node>::next(current_node);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 10);

    current_node = tempest::intrusive_mpsc_stack<base_hook_node>::next(current_node);
    EXPECT_EQ(current_node, nullptr);

    // 5. Act: Reverse the drained chain to obtain FIFO order (10 -> 20 -> 30 -> nullptr)
    auto* reversed_head = tempest::intrusive_mpsc_stack<base_hook_node>::reverse(drained_head);

    // 6. Assert: Chain is reversed
    ASSERT_NE(reversed_head, nullptr);
    EXPECT_EQ(reversed_head->value, 10);

    current_node = tempest::intrusive_mpsc_stack<base_hook_node>::next(reversed_head);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 20);

    current_node = tempest::intrusive_mpsc_stack<base_hook_node>::next(current_node);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 30);

    current_node = tempest::intrusive_mpsc_stack<base_hook_node>::next(current_node);
    EXPECT_EQ(current_node, nullptr);
}

/// @brief Verifies edge case behaviors for empty and single-element stacks and helper functions.
TEST(intrusive_stack_test, edge_cases_empty_and_single_element)
{
    // 1. Setup
    auto stack = tempest::intrusive_mpsc_stack<base_hook_node>{};

    // 2. Act & Assert: Empty stack drain and next on nullptr
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.drain(), nullptr);
    EXPECT_EQ(tempest::intrusive_mpsc_stack<base_hook_node>::next(nullptr), nullptr);
    EXPECT_EQ(tempest::intrusive_mpsc_stack<base_hook_node>::reverse(nullptr), nullptr);

    // 3. Act: Push a single element
    auto single_node = base_hook_node{};
    single_node.value = 42;
    stack.push(tempest::non_null<base_hook_node>{single_node});
    EXPECT_FALSE(stack.empty());

    // 4. Assert: Drain single element
    auto* drained = stack.drain();
    EXPECT_TRUE(stack.empty());
    ASSERT_NE(drained, nullptr);
    EXPECT_EQ(drained->value, 42);
    EXPECT_EQ(tempest::intrusive_mpsc_stack<base_hook_node>::next(drained), nullptr);

    // 5. Assert: Reverse single-element chain returns the same node
    auto* reversed = tempest::intrusive_mpsc_stack<base_hook_node>::reverse(drained);
    ASSERT_NE(reversed, nullptr);
    EXPECT_EQ(reversed->value, 42);
    EXPECT_EQ(tempest::intrusive_mpsc_stack<base_hook_node>::next(reversed), nullptr);
}

//=============================================================================
// 2. Member-Pointer Hook Tests
//=============================================================================

/// @brief Verifies that a member-pointer hook stack correctly accesses hook members, drains, and reverses.
TEST(intrusive_stack_test, member_hook_push_drain_reverse)
{
    // 1. Setup
    auto stack = tempest::intrusive_mpsc_stack<member_hook_node, &member_hook_node::hook>{};
    EXPECT_TRUE(stack.empty());

    auto node_one = member_hook_node{};
    node_one.value = 100;
    auto node_two = member_hook_node{};
    node_two.value = 200;
    auto node_three = member_hook_node{};
    node_three.value = 300;

    // 2. Act: Push nodes
    stack.push(tempest::non_null<member_hook_node>{node_one});
    stack.push(tempest::non_null<member_hook_node>{node_two});
    stack.push(tempest::non_null<member_hook_node>{node_three});
    EXPECT_FALSE(stack.empty());

    // 3. Act: Drain
    auto* drained_head = stack.drain();
    EXPECT_TRUE(stack.empty());

    // 4. Assert: LIFO order (300 -> 200 -> 100 -> nullptr)
    ASSERT_NE(drained_head, nullptr);
    EXPECT_EQ(drained_head->value, 300);

    auto* current_node =
        tempest::intrusive_mpsc_stack<member_hook_node, &member_hook_node::hook>::next(drained_head);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 200);

    current_node =
        tempest::intrusive_mpsc_stack<member_hook_node, &member_hook_node::hook>::next(current_node);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 100);

    current_node =
        tempest::intrusive_mpsc_stack<member_hook_node, &member_hook_node::hook>::next(current_node);
    EXPECT_EQ(current_node, nullptr);

    // 5. Act: Reverse to FIFO order (100 -> 200 -> 300 -> nullptr)
    auto* reversed_head =
        tempest::intrusive_mpsc_stack<member_hook_node, &member_hook_node::hook>::reverse(drained_head);

    // 6. Assert: FIFO order
    ASSERT_NE(reversed_head, nullptr);
    EXPECT_EQ(reversed_head->value, 100);

    current_node =
        tempest::intrusive_mpsc_stack<member_hook_node, &member_hook_node::hook>::next(reversed_head);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 200);

    current_node =
        tempest::intrusive_mpsc_stack<member_hook_node, &member_hook_node::hook>::next(current_node);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 300);

    current_node =
        tempest::intrusive_mpsc_stack<member_hook_node, &member_hook_node::hook>::next(current_node);
    EXPECT_EQ(current_node, nullptr);
}

//=============================================================================
// 3. Range Operations
//=============================================================================

/// @brief Verifies push_range attaches an entire pre-linked chain atomically to the stack.
TEST(intrusive_stack_test, push_range_prelinked_chain)
{
    // 1. Setup: Create nodes
    auto stack = tempest::intrusive_mpsc_stack<base_hook_node>{};

    auto chain_a1 = base_hook_node{};
    chain_a1.value = 1;
    auto chain_a2 = base_hook_node{};
    chain_a2.value = 2;
    auto chain_a3 = base_hook_node{};
    chain_a3.value = 3;

    // Pre-link chain A: a1 -> a2 -> a3
    chain_a1.next = &chain_a2;
    chain_a2.next = &chain_a3;
    chain_a3.next = nullptr;

    auto chain_b1 = base_hook_node{};
    chain_b1.value = 4;
    auto chain_b2 = base_hook_node{};
    chain_b2.value = 5;

    // Pre-link chain B: b1 -> b2
    chain_b1.next = &chain_b2;
    chain_b2.next = nullptr;

    // 2. Act: Push chain A, then push chain B
    stack.push_range(tempest::non_null<base_hook_node>{chain_a1}, tempest::non_null<base_hook_node>{chain_a3});
    stack.push_range(tempest::non_null<base_hook_node>{chain_b1}, tempest::non_null<base_hook_node>{chain_b2});

    // 3. Act: Drain the stack
    auto* drained = stack.drain();

    // 4. Assert: Chain B is on top, followed by Chain A: b1 -> b2 -> a1 -> a2 -> a3 -> nullptr
    ASSERT_NE(drained, nullptr);
    EXPECT_EQ(drained->value, 4);

    auto* current_node = tempest::intrusive_mpsc_stack<base_hook_node>::next(drained);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 5);

    current_node = tempest::intrusive_mpsc_stack<base_hook_node>::next(current_node);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 1);

    current_node = tempest::intrusive_mpsc_stack<base_hook_node>::next(current_node);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 2);

    current_node = tempest::intrusive_mpsc_stack<base_hook_node>::next(current_node);
    ASSERT_NE(current_node, nullptr);
    EXPECT_EQ(current_node->value, 3);

    current_node = tempest::intrusive_mpsc_stack<base_hook_node>::next(current_node);
    EXPECT_EQ(current_node, nullptr);
}

//=============================================================================
// 4. High-Concurrency MPSC Stress Tests
//=============================================================================

/// @brief Stress-tests concurrent pushes from multiple producer threads and a single consumer draining concurrently.
TEST(intrusive_stack_test, concurrent_mpsc_stress_push_and_drain)
{
    // 1. Setup: Constants and pre-allocated node buffers
    constexpr auto producer_thread_count = 4U;
    constexpr auto items_per_thread = 10000U;
    constexpr auto total_node_count = producer_thread_count * items_per_thread;

    auto all_nodes = tempest::vector<stress_node>(total_node_count);
    for (auto producer_index = 0U; producer_index < producer_thread_count; ++producer_index)
    {
        for (auto sequence_index = 0U; sequence_index < items_per_thread; ++sequence_index)
        {
            const auto flat_index = producer_index * items_per_thread + sequence_index;
            all_nodes[flat_index].thread_index = producer_index;
            all_nodes[flat_index].sequence_id = sequence_index;
            all_nodes[flat_index].next = nullptr;
        }
    }

    auto stack = tempest::intrusive_mpsc_stack<stress_node>{};
    auto start_barrier = tempest::atomic<bool>{false};
    auto producers_finished_count = tempest::atomic<uint32_t>{0};

    // 2. Act: Spawn producer threads
    auto producer_threads = tempest::vector<tempest::thread>{};
    for (auto thread_idx = 0U; thread_idx < producer_thread_count; ++thread_idx)
    {
        producer_threads.push_back(tempest::thread([thread_idx, &all_nodes, &stack, &start_barrier,
                                                    &producers_finished_count]() -> void {
            // Wait for start signal
            while (!start_barrier.load(tempest::memory_order::acquire))
            {
                tempest::this_thread::yield();
            }

            // Push nodes using a mix of single push and push_range
            const auto base_offset = thread_idx * items_per_thread;
            auto current_item_index = 0U;

            while (current_item_index < items_per_thread)
            {
                // Push in small batches of 4 if possible, otherwise single push
                if (current_item_index + 4U <= items_per_thread)
                {
                    auto* first_item = &all_nodes[base_offset + current_item_index];
                    auto* second_item = &all_nodes[base_offset + current_item_index + 1U];
                    auto* third_item = &all_nodes[base_offset + current_item_index + 2U];
                    auto* fourth_item = &all_nodes[base_offset + current_item_index + 3U];

                    first_item->next = second_item;
                    second_item->next = third_item;
                    third_item->next = fourth_item;
                    fourth_item->next = nullptr;

                    stack.push_range(tempest::non_null<stress_node>{*first_item},
                                     tempest::non_null<stress_node>{*fourth_item});
                    current_item_index += 4U;
                }
                else
                {
                    auto* single_item = &all_nodes[base_offset + current_item_index];
                    single_item->next = nullptr;
                    stack.push(tempest::non_null<stress_node>{*single_item});
                    current_item_index += 1U;
                }
            }

            producers_finished_count.fetch_add(1U, tempest::memory_order::release);
        }));
    }

    // 3. Act: Start concurrent execution and consumer draining
    start_barrier.store(true, tempest::memory_order::release);

    auto received_counts = tempest::vector<uint32_t>(total_node_count, 0U);
    auto total_received = 0U;

    // Concurrently drain until all producers are done and stack is empty
    while (producers_finished_count.load(tempest::memory_order::acquire) < producer_thread_count || !stack.empty())
    {
        auto* drained_batch = stack.drain();
        if (drained_batch == nullptr)
        {
            tempest::this_thread::yield();
            continue;
        }

        auto* current_node = drained_batch;
        while (current_node != nullptr)
        {
            const auto flat_index = current_node->thread_index * items_per_thread + current_node->sequence_id;
            ASSERT_LT(flat_index, total_node_count);
            received_counts[flat_index]++;
            total_received++;
            current_node = tempest::intrusive_mpsc_stack<stress_node>::next(current_node);
        }
    }

    // Final drain pass to ensure anything pushed right before the completion counter was incremented is drained
    auto* final_batch = stack.drain();
    auto* current_node = final_batch;
    while (current_node != nullptr)
    {
        const auto flat_index = current_node->thread_index * items_per_thread + current_node->sequence_id;
        ASSERT_LT(flat_index, total_node_count);
        received_counts[flat_index]++;
        total_received++;
        current_node = tempest::intrusive_mpsc_stack<stress_node>::next(current_node);
    }

    // Join all producer threads
    for (auto& producer_thread : producer_threads)
    {
        producer_thread.join();
    }

    // 4. Assert: All nodes were received exactly once with no duplicates or omissions
    EXPECT_EQ(total_received, total_node_count);
    for (auto verification_index = 0U; verification_index < total_node_count; ++verification_index)
    {
        EXPECT_EQ(received_counts[verification_index], 1U);
    }
}
