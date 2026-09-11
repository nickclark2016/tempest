#ifndef tempest_job_allocator_hpp
#define tempest_job_allocator_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/job/types.hpp>

namespace tempest::job
{
    inline constexpr size_t slab_class_count = 6;
    inline constexpr size_t slab_class_sizes[slab_class_count] = {64, 128, 256, 512, 1024, 2048};
    inline constexpr size_t slab_chunk_size = 64 * 1024; // 64 KB

    constexpr auto get_size_class(size_t size) noexcept -> int32_t
    {
        if (size <= 64)
        {
            return 0;
        }
        if (size <= 128)
        {
            return 1;
        }
        if (size <= 256)
        {
            return 2;
        }
        if (size <= 512)
        {
            return 3;
        }
        if (size <= 1024)
        {
            return 4;
        }
        if (size <= 2048)
        {
            return 5;
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
            return reinterpret_cast<slab_chunk*>(reinterpret_cast<uintptr_t>(ptr) & ~(slab_chunk_size - 1));
        }
    };

    struct free_slot_node
    {
        free_slot_node* next{nullptr};
    };

    struct remote_free_node
    {
        remote_free_node* next{nullptr};
    };

    class TEMPEST_API job_allocator
    {
      public:
        job_allocator();
        ~job_allocator();

        job_allocator(const job_allocator&) = delete;
        job_allocator& operator=(const job_allocator&) = delete;
        job_allocator(job_allocator&&) noexcept = delete;
        job_allocator& operator=(job_allocator&&) noexcept = delete;

        [[nodiscard]] static auto get_current() noexcept -> job_allocator*;
        static auto set_current(job_allocator* alloc) noexcept -> void;

        [[nodiscard]] auto allocate(size_t size) -> void*;
        static auto deallocate(void* ptr, size_t size) noexcept -> void;

        auto drain_remote_frees() noexcept -> void;

        [[nodiscard]] auto get_telemetry() const noexcept -> allocator_telemetry;

        auto push_remote_free(void* ptr) noexcept -> void;
        auto decrement_live_frames() noexcept -> void;

      private:
        auto _allocate_slab_chunk(size_t size_class) -> void;

        array<free_slot_node*, slab_class_count> _local_free_list{};
        array<slab_chunk*, slab_class_count> _chunks{};
        atomic<remote_free_node*> _remote_free_head{nullptr};

        array<atomic<uint64_t>, slab_class_count> _allocations_per_class{};
        atomic<uint64_t> _heap_fallback_count{0};
        atomic<int64_t> _active_live_frames{0};
        atomic<uint64_t> _committed_slab_chunks{0};
    };
} // namespace tempest::job

#endif // tempest_job_allocator_hpp
