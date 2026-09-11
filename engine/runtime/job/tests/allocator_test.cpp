#include <gtest/gtest.h>

#include <tempest/array.hpp>
#include <tempest/job/allocator.hpp>
#include <tempest/job/context.hpp>
#include <tempest/job/task.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

namespace tempest::job::tests
{
    // =========================================================================
    // SECTION: Size-Class Routing Tests
    // =========================================================================

    /// @brief Verifies that allocations across all power-of-two bands route to their
    ///        exact slab classes and that frames > 2048B fall back to heap allocation.
    TEST(allocator_test, size_class_routing)
    {
        // 1. Setup
        auto alloc = job_allocator{};

        // 2. Act
        auto* p0 = alloc.allocate(48);   // Class 0: <= 64B
        auto* p1 = alloc.allocate(96);   // Class 1: <= 128B
        auto* p2 = alloc.allocate(192);  // Class 2: <= 256B
        auto* p3 = alloc.allocate(384);  // Class 3: <= 512B
        auto* p4 = alloc.allocate(768);  // Class 4: <= 1024B
        auto* p5 = alloc.allocate(1536); // Class 5: <= 2048B
        auto* p_heap = alloc.allocate(3000); // Overflow: > 2048B

        const auto telem = alloc.get_telemetry();

        job_allocator::deallocate(p0, 48);
        job_allocator::deallocate(p1, 96);
        job_allocator::deallocate(p2, 192);
        job_allocator::deallocate(p3, 384);
        job_allocator::deallocate(p4, 768);
        job_allocator::deallocate(p5, 1536);
        job_allocator::deallocate(p_heap, 3000);

        // 3. Assert
        for (size_t i = 0; i < slab_class_count; ++i)
        {
            EXPECT_EQ(telem.allocations_per_class[i], 1u) << "Class " << i << " did not record exactly 1 allocation";
        }
        EXPECT_EQ(telem.heap_fallback_count, 1u);
        EXPECT_EQ(telem.active_live_frames, 7);

        const auto telem_after = alloc.get_telemetry();
        EXPECT_EQ(telem_after.active_live_frames, 0);
    }

    // =========================================================================
    // SECTION: Zero-Growth Recycling Tests
    // =========================================================================

    /// @brief Verifies that completing 1,000 tasks and re-running another 1,000 tasks
    ///        achieves 100% slot recycling without committing additional slab chunks.
    TEST(allocator_test, zero_growth_recycling)
    {
        // 1. Setup
        auto alloc = job_allocator{};
        constexpr size_t batch_count = 1000;
        constexpr size_t slot_size = 128; // Class 1

        // Phase 1: Allocate initial batch
        auto pointers = vector<void*>{};
        pointers.reserve(batch_count);
        for (size_t i = 0; i < batch_count; ++i)
        {
            pointers.push_back(alloc.allocate(slot_size));
        }

        // Free initial batch
        for (auto* ptr : pointers)
        {
            job_allocator::deallocate(ptr, slot_size);
        }
        pointers.clear();

        // 2. Act: Snapshot committed chunks and allocate second batch
        const auto initial_chunks = alloc.get_telemetry().committed_slab_chunks;

        for (size_t i = 0; i < batch_count; ++i)
        {
            pointers.push_back(alloc.allocate(slot_size));
        }

        const auto second_batch_chunks = alloc.get_telemetry().committed_slab_chunks;

        for (auto* ptr : pointers)
        {
            job_allocator::deallocate(ptr, slot_size);
        }

        // 3. Assert
        EXPECT_GT(initial_chunks, 0u);
        EXPECT_EQ(second_batch_chunks, initial_chunks) << "Slab chunks increased despite 100% recycling";
        EXPECT_EQ(alloc.get_telemetry().active_live_frames, 0);
    }

    // =========================================================================
    // SECTION: Cross-Thread Remote-Free Tests
    // =========================================================================

    /// @brief Verifies that cross-thread frees push pointers onto the owner allocator's
    ///        MPSC queue and drain cleanly back into local free lists.
    TEST(allocator_test, cross_thread_remote_free_steady_state)
    {
        // 1. Setup
        auto alloc_a = job_allocator{};
        constexpr size_t count = 500;
        constexpr size_t slot_size = 256;

        auto pointers = vector<void*>{};
        pointers.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            pointers.push_back(alloc_a.allocate(slot_size));
        }

        // 2. Act: Free pointers on Thread B
        auto thread_b = tempest::thread([&pointers]() {
            for (auto* ptr : pointers)
            {
                job_allocator::deallocate(ptr, slot_size);
            }
        });
        thread_b.join();

        // Thread A drains remote free queue
        alloc_a.drain_remote_frees();

        // 3. Assert
        const auto telem = alloc_a.get_telemetry();
        EXPECT_EQ(telem.active_live_frames, 0);

        // Reallocating the same count should not commit new chunks
        const auto chunks_before = telem.committed_slab_chunks;
        for (size_t i = 0; i < count; ++i)
        {
            pointers[i] = alloc_a.allocate(slot_size);
        }
        const auto chunks_after = alloc_a.get_telemetry().committed_slab_chunks;
        EXPECT_EQ(chunks_after, chunks_before);

        for (auto* ptr : pointers)
        {
            job_allocator::deallocate(ptr, slot_size);
        }
    }

    // =========================================================================
    // SECTION: Argument-Forwarded Coroutine Allocation Tests
    // =========================================================================

    namespace
    {
        auto coro_with_allocator(job_allocator&, int val) -> task<int>
        {
            co_return val * 2;
        }

        auto coro_heap_fallback(int val) -> task<int>
        {
            co_return val + 1;
        }
    } // namespace

    /// @brief Verifies that coroutines accepting job_allocator& route their frame
    ///        allocation to slabs and track frame_bytes / is_heap profiler metrics,
    ///        while coroutines without an allocator gracefully fall back to heap.
    TEST(allocator_test, argument_forwarded_coroutine_allocation)
    {
        // 1. Setup
        auto alloc = job_allocator{};

        // 2. Act & Assert: Coroutine with job_allocator& parameter
        {
            const auto telem_before = alloc.get_telemetry();
            auto t = coro_with_allocator(alloc, 21);
            const auto telem_after_alloc = alloc.get_telemetry();

            EXPECT_EQ(telem_after_alloc.active_live_frames, telem_before.active_live_frames + 1);
            bool allocated_in_slab = false;
            for (size_t i = 0; i < slab_class_count; ++i)
            {
                if (telem_after_alloc.allocations_per_class[i] > telem_before.allocations_per_class[i])
                {
                    allocated_in_slab = true;
                    break;
                }
            }
            EXPECT_TRUE(allocated_in_slab);

            t.handle().resume();
            EXPECT_TRUE(t.handle().done());
            EXPECT_EQ(t.handle().promise().result.value(), 42);

            t = {};
            alloc.drain_remote_frees();
            const auto telem_after_free = alloc.get_telemetry();
            EXPECT_EQ(telem_after_free.active_live_frames, telem_before.active_live_frames);
        }

        // 3. Act & Assert: Coroutine without allocator parameters (pure heap fallback)
        {
            const auto telem_before = alloc.get_telemetry();
            auto t = coro_heap_fallback(100);
            const auto telem_after_alloc = alloc.get_telemetry();

            // Slabs should NOT be incremented
            EXPECT_EQ(telem_after_alloc.active_live_frames, telem_before.active_live_frames);

            t.handle().resume();
            EXPECT_TRUE(t.handle().done());
            EXPECT_EQ(t.handle().promise().result.value(), 101);

            t = {};
        }

        // 4. Act & Assert: Profiler metrics tracking (frame_bytes and is_heap)
        {
            auto prof = profiler::profiler_session{true};
            auto& thread_ctx = prof.get_or_register_thread();
            profiler::thread_profiler_context::set_current_thread_context(&thread_ctx);

            {
                auto t = coro_with_allocator(alloc, 5);
                t.handle().resume();
                EXPECT_TRUE(t.handle().done());
                EXPECT_EQ(t.handle().promise().result.value(), 10);
            }

            profiler::thread_profiler_context::set_current_thread_context(nullptr);

            auto chunks = prof.drain_completed_chunks();
            ASSERT_FALSE(chunks.empty());
            const auto zones = chunks[0]->zones();
            ASSERT_FALSE(zones.empty());

            const auto& zone = zones[0];
            bool found_frame_bytes = false;
            bool found_is_heap = false;
            for (const auto& met : zone.metrics)
            {
                if (met.name == "frame_bytes")
                {
                    found_frame_bytes = true;
                    EXPECT_GT(met.value, 0.0);
                }
                else if (met.name == "is_heap")
                {
                    found_is_heap = true;
                    EXPECT_EQ(met.value, 0.0); // Slab allocation
                }
            }
            EXPECT_TRUE(found_frame_bytes);
            EXPECT_TRUE(found_is_heap);

            alloc.drain_remote_frees();
        }
    }
} // namespace tempest::job::tests
