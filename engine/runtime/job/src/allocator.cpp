#include <tempest/job/allocator.hpp>
#include <tempest/memory.hpp>

namespace tempest::job
{
    namespace
    {
        thread_local job_allocator* tl_current_allocator = nullptr;

        auto get_size_class(size_t size) noexcept -> int32_t
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
    } // namespace

    job_allocator::job_allocator() = default;

    job_allocator::~job_allocator()
    {
        drain_remote_frees();

        if (tl_current_allocator == this)
        {
            tl_current_allocator = nullptr;
        }

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
        drain_remote_frees();

        const auto cls = get_size_class(size);
        if (cls < 0)
        {
            auto* ptr = aligned_alloc(size, 16);
            _heap_fallback_count.fetch_add(1, memory_order::relaxed);
            _active_live_frames.fetch_add(1, memory_order::relaxed);
            return ptr;
        }

        const auto class_idx = static_cast<size_t>(cls);
        _allocations_per_class[class_idx].fetch_add(1, memory_order::relaxed);
        _active_live_frames.fetch_add(1, memory_order::relaxed);

        if (_local_free_list[class_idx] == nullptr)
        {
            _allocate_slab_chunk(class_idx);
        }

        auto* node = _local_free_list[class_idx];
        _local_free_list[class_idx] = node->next;
        return node;
    }

    auto job_allocator::_allocate_slab_chunk(size_t cls) -> void
    {
        const auto slot_sz = slab_class_sizes[cls];
        auto* raw = static_cast<byte*>(aligned_alloc(slab_chunk_size, slab_chunk_size));

        auto* chunk = reinterpret_cast<slab_chunk*>(raw);
        chunk->owner = this;
        chunk->size_class = static_cast<uint32_t>(cls);
        chunk->slot_size = static_cast<uint32_t>(slot_sz);
        chunk->next_chunk = _chunks[cls];
        _chunks[cls] = chunk;

        _committed_slab_chunks.fetch_add(1, memory_order::relaxed);

        const auto header_size = sizeof(slab_chunk);
        const auto first_slot_offset = (header_size + slot_sz - 1) & ~(slot_sz - 1);
        const auto total_slots = (slab_chunk_size - first_slot_offset) / slot_sz;
        chunk->total_slots = static_cast<uint32_t>(total_slots);

        free_slot_node* head = nullptr;
        for (size_t i = 0; i < total_slots; ++i)
        {
            auto* slot = reinterpret_cast<free_slot_node*>(raw + first_slot_offset + i * slot_sz);
            slot->next = head;
            head = slot;
        }

        _local_free_list[cls] = head;
    }

    auto job_allocator::push_remote_free(void* ptr) noexcept -> void
    {
        auto* node = reinterpret_cast<remote_free_node*>(ptr);
        auto* old_head = _remote_free_head.load(memory_order::relaxed);
        do
        {
            node->next = old_head;
        } while (!_remote_free_head.compare_exchange_weak(old_head, node, memory_order::release));
    }

    auto job_allocator::drain_remote_frees() noexcept -> void
    {
        auto* head = _remote_free_head.exchange(nullptr, memory_order::acquire);
        while (head != nullptr)
        {
            auto* next = head->next;
            auto* chunk = slab_chunk::from_pointer(head);
            const auto cls = chunk->size_class;

            auto* slot = reinterpret_cast<free_slot_node*>(head);
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

        if (size > 2048)
        {
            aligned_free(ptr);
            auto* curr = get_current();
            if (curr != nullptr)
            {
                curr->_active_live_frames.fetch_sub(1, memory_order::relaxed);
            }
            return;
        }

        auto* chunk = slab_chunk::from_pointer(ptr);
        auto* owner = chunk->owner;
        owner->_active_live_frames.fetch_sub(1, memory_order::relaxed);

        auto* current = get_current();
        if (current == owner)
        {
            const auto cls = chunk->size_class;
            auto* slot = reinterpret_cast<free_slot_node*>(ptr);
            slot->next = owner->_local_free_list[cls];
            owner->_local_free_list[cls] = slot;
        }
        else
        {
            owner->push_remote_free(ptr);
        }
    }

    auto job_allocator::get_telemetry() const noexcept -> allocator_telemetry
    {
        auto t = allocator_telemetry{};
        for (size_t i = 0; i < slab_class_count; ++i)
        {
            t.allocations_per_class[i] = _allocations_per_class[i].load(memory_order::relaxed);
        }
        t.heap_fallback_count = _heap_fallback_count.load(memory_order::relaxed);
        t.active_live_frames = _active_live_frames.load(memory_order::relaxed);
        t.committed_slab_chunks = _committed_slab_chunks.load(memory_order::relaxed);
        return t;
    }

    auto job_allocator::get_current() noexcept -> job_allocator*
    {
        return tl_current_allocator;
    }

    auto job_allocator::set_current(job_allocator* alloc) noexcept -> job_allocator*
    {
        auto* prev = tl_current_allocator;
        tl_current_allocator = alloc;
        return prev;
    }

    job_allocator_scope::job_allocator_scope(job_allocator& alloc) noexcept
        : _prev{job_allocator::set_current(&alloc)}
    {
    }

    job_allocator_scope::~job_allocator_scope()
    {
        job_allocator::set_current(_prev);
    }
} // namespace tempest::job
