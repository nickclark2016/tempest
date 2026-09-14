#include "tempest/job/task.hpp"
#include <tempest/job/allocator.hpp>
#include <tempest/memory.hpp>

namespace tempest::job
{
job_allocator::job_allocator() = default;

    job_allocator::~job_allocator()
    {
        auto guard = lock_guard{_alloc_mutex};
        _drain_remote_frees_locked();

        for (size_t cls = 0; cls < slab_class_count; ++cls)
        {
            auto* chunk = _chunks[cls];
            while (chunk != nullptr)
            {
                auto* next = chunk->next_chunk;
                aligned_free(chunk);
                chunk = next;
            }
            _chunks[cls] = nullptr;
            _local_free_list[cls] = nullptr;
        }
    }

    auto job_allocator::allocate(size_t size) -> void*
    {
        auto guard = lock_guard{_alloc_mutex};
        _drain_remote_frees_locked();

        const auto cls = get_size_class(size);
        if (cls < 0)
        {
            auto* raw = static_cast<byte*>(aligned_alloc(size + detail::coroutine_frame_header_size, detail::coroutine_frame_header_alignment));
            *reinterpret_cast<job_allocator**>(raw) = this;
            _heap_fallback_count.fetch_add(1, memory_order::relaxed);
            _active_live_frames.fetch_add(1, memory_order::acq_rel);
            return raw + detail::coroutine_frame_header_size; // first HEADER_SIZE bytes are reserved for the header
        }

        const auto class_idx = static_cast<size_t>(cls);
        _allocations_per_class[class_idx].fetch_add(1, memory_order::relaxed);
        _active_live_frames.fetch_add(1, memory_order::acq_rel);

        if (_local_free_list[class_idx] == nullptr)
        {
            _allocate_slab_chunk(class_idx);
        }

        auto* node = _local_free_list[class_idx];
        _local_free_list[class_idx] = node->next;
        return node;
    }

    auto job_allocator::_allocate_slab_chunk(size_t size_class) -> void
    {
        const auto slot_sz = slab_class_sizes[size_class];
        auto* raw = static_cast<byte*>(aligned_alloc(slab_chunk_size, slab_chunk_size));

        auto* chunk = reinterpret_cast<slab_chunk*>(raw);
        chunk->owner = this;
        chunk->size_class = static_cast<uint32_t>(size_class);
        chunk->slot_size = static_cast<uint32_t>(slot_sz);
        chunk->next_chunk = _chunks[size_class];
        _chunks[size_class] = chunk;

        _committed_slab_chunks.fetch_add(1, memory_order::relaxed);

        const auto header_size = sizeof(slab_chunk);
        const auto first_slot_offset = (header_size + slot_sz - 1) & ~(slot_sz - 1);
        const auto total_slots = (slab_chunk_size - first_slot_offset) / slot_sz;
        chunk->total_slots = static_cast<uint32_t>(total_slots);

        free_slot_node* head = nullptr;
        for (size_t i = 0; i < total_slots; ++i)
        {
            const auto offset = (slot_sz * i) + first_slot_offset;
            auto* slot = reinterpret_cast<free_slot_node*>(raw + offset);
            slot->next = head;
            head = slot;
        }

        _local_free_list[size_class] = head;
    }

    auto job_allocator::push_remote_free(void* ptr) noexcept -> void
    {
        auto* const node = reinterpret_cast<remote_free_node*>(ptr);
        _remote_free_head.push(non_null{*node});
    }

    auto job_allocator::drain_remote_frees() noexcept -> void
    {
        auto guard = lock_guard{_alloc_mutex};
        _drain_remote_frees_locked();
    }

    auto job_allocator::_drain_remote_frees_locked() noexcept -> void
    {
        auto* head = _remote_free_head.drain();
        while (head != nullptr)
        {
            auto* const next = head->next;
            auto* const chunk = slab_chunk::from_pointer(head);
            const auto cls = chunk->size_class;

            auto* const slot = reinterpret_cast<free_slot_node*>(head);
            slot->next = _local_free_list[cls];
            _local_free_list[cls] = slot;

            head = next;
        }
    }

    auto job_allocator::deallocate(void* ptr, size_t size) noexcept -> void
    {
        if (ptr == nullptr)
        {
            return;
        }

        if (size > size_class_2kb)
        {
            auto* raw = static_cast<byte*>(ptr) - detail::coroutine_frame_header_size;
            auto* owner = *reinterpret_cast<job_allocator**>(raw);
            if (owner != nullptr)
            {
                owner->_active_live_frames.fetch_sub(1, memory_order::acq_rel);
            }
            aligned_free(raw);
            return;
        }

        auto* chunk = slab_chunk::from_pointer(ptr);
        auto* owner = chunk->owner;
        owner->_active_live_frames.fetch_sub(1, memory_order::acq_rel);
        owner->push_remote_free(ptr);
    }

    auto job_allocator::decrement_live_frames() noexcept -> void
    {
        _active_live_frames.fetch_sub(1, memory_order::acq_rel);
    }

    auto job_allocator::get_telemetry() const noexcept -> allocator_telemetry
    {
        auto telemetry = allocator_telemetry{};
        for (size_t i = 0; i < slab_class_count; ++i)
        {
            telemetry.allocations_per_class[i] = _allocations_per_class[i].load(memory_order::relaxed);
        }
        telemetry.heap_fallback_count = _heap_fallback_count.load(memory_order::relaxed);
        telemetry.active_live_frames = _active_live_frames.load(memory_order::relaxed);
        telemetry.committed_slab_chunks = _committed_slab_chunks.load(memory_order::relaxed);
        return telemetry;
    }
} // namespace tempest::job
