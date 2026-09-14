#ifndef tempest_job_allocator_hpp
#define tempest_job_allocator_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/intrusive_stack.hpp>
#include <tempest/job/types.hpp>
#include <tempest/mutex.hpp>

namespace tempest::job
{
    inline constexpr size_t slab_class_count = 6;
    inline constexpr array<size_t, slab_class_count> slab_class_sizes = {64, 128, 256, 512, 1024, 2048};
    inline constexpr size_t slab_chunk_size =
        static_cast<size_t>(64U) * 1024U; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    inline constexpr size_t size_class_64b = 64;
    inline constexpr size_t size_class_128b = 128;
    inline constexpr size_t size_class_256b = 256;
    inline constexpr size_t size_class_512b = 512;
    inline constexpr size_t size_class_1kb = 1024;
    inline constexpr size_t size_class_2kb = 2048;

    inline constexpr int32_t size_class_index_64b = 0;
    inline constexpr int32_t size_class_index_128b = 1;
    inline constexpr int32_t size_class_index_256b = 2;
    inline constexpr int32_t size_class_index_512b = 3;
    inline constexpr int32_t size_class_index_1kb = 4;
    inline constexpr int32_t size_class_index_2kb = 5;

    constexpr auto get_size_class(size_t size) noexcept -> int32_t
    {
        if (size <= size_class_64b)
        {
            return size_class_index_64b;
        }
        if (size <= size_class_128b)
        {
            return size_class_index_128b;
        }
        if (size <= size_class_256b)
        {
            return size_class_index_256b;
        }
        if (size <= size_class_512b)
        {
            return size_class_index_512b;
        }
        if (size <= size_class_1kb)
        {
            return size_class_index_1kb;
        }
        if (size <= size_class_2kb)
        {
            return size_class_index_2kb;
        }
        return -1;
    }

    struct allocator_telemetry
    {
        array<uint64_t, slab_class_count> allocations_per_class{};
        uint64_t heap_fallback_count{0};
        int64_t active_live_frames{0};
        uint64_t committed_slab_chunks{0};
    };

    class job_allocator;

    struct slab_chunk
    {
        job_allocator* owner{nullptr};
        uint32_t size_class{0};
        uint32_t slot_size{0};
        uint32_t total_slots{0};
        slab_chunk* next_chunk{nullptr};

        [[nodiscard]] static auto from_pointer(void* ptr) noexcept -> slab_chunk*
        {
            // return reinterpret_cast<slab_chunk*>(reinterpret_cast<uintptr_t>(ptr) & ~(slab_chunk_size - 1));
            const auto offset = reinterpret_cast<uintptr_t>(ptr) & (slab_chunk_size - 1);
            auto* const base = reinterpret_cast<byte*>(ptr) - offset;
            return reinterpret_cast<slab_chunk*>(base);
        }
    };

    struct free_slot_node
    {
        free_slot_node* next{nullptr};
    };

    struct remote_free_node : treiber_node<remote_free_node>
    {
    };

    class TEMPEST_API job_allocator
    {
      public:
        job_allocator();
        ~job_allocator();

        job_allocator(const job_allocator&) = delete;
        auto operator=(const job_allocator&) -> job_allocator& = delete;
        job_allocator(job_allocator&&) noexcept = delete;
        auto operator=(job_allocator&&) noexcept -> job_allocator& = delete;

        [[nodiscard]] auto allocate(size_t size) -> void*;
        static auto deallocate(void* ptr, size_t size) noexcept -> void;

        auto drain_remote_frees() noexcept -> void;

        [[nodiscard]] auto get_telemetry() const noexcept -> allocator_telemetry;

        auto push_remote_free(void* ptr) noexcept -> void;
        auto decrement_live_frames() noexcept -> void;

      private:
        auto _allocate_slab_chunk(size_t size_class) -> void;
        auto _drain_remote_frees_locked() noexcept -> void;

        mutable mutex _alloc_mutex;
        array<free_slot_node*, slab_class_count> _local_free_list{};
        array<slab_chunk*, slab_class_count> _chunks{};
        intrusive_mpsc_stack<remote_free_node> _remote_free_head;

        array<atomic<uint64_t>, slab_class_count> _allocations_per_class{};
        atomic<uint64_t> _heap_fallback_count{0};
        atomic<int64_t> _active_live_frames{0};
        atomic<uint64_t> _committed_slab_chunks{0};
    };
} // namespace tempest::job

#endif // tempest_job_allocator_hpp
