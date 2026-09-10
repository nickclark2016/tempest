#include <gtest/gtest.h>

#include <tempest/array.hpp>
#include <tempest/job/allocator.hpp>
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
        auto scope = job_allocator_scope{alloc};

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
} // namespace tempest::job::tests
