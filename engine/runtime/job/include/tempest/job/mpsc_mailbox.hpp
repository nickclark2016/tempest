#ifndef tempest_job_mpsc_mailbox_hpp
#define tempest_job_mpsc_mailbox_hpp

#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/thread.hpp>
#include <tempest/utility.hpp>

namespace tempest::job
{
    enum class push_result : uint8_t
    {
        success,
        full
    };

    inline constexpr size_t mspc_mailbox_default_capacity = 2048;

    template <typename T, size_t Capacity = mspc_mailbox_default_capacity>
    class mpsc_mailbox
    {
        static_assert((Capacity & (Capacity - 1)) == 0 && Capacity >= 2,
                      "Capacity must be a power of two and >= 2");
        static constexpr size_t Mask = Capacity - 1;

        struct cell_t
        {
            atomic<size_t> sequence{0};
            alignas(alignof(T)) array<byte, sizeof(T)> storage;
        };

      public:
        using value_type = T;

        mpsc_mailbox() noexcept
        {
            for (auto slot_index = size_t{0}; slot_index < Capacity; ++slot_index)
            {
                _cells[slot_index].sequence.store(slot_index, memory_order::relaxed);
            }
        }

        ~mpsc_mailbox()
        {
            drain([]([[maybe_unused]] T&& item) {});
        }

        mpsc_mailbox(const mpsc_mailbox&) = delete;
        mpsc_mailbox(mpsc_mailbox&&) noexcept = delete;
        auto operator=(const mpsc_mailbox&) -> mpsc_mailbox& = delete;
        auto operator=(mpsc_mailbox&&) noexcept -> mpsc_mailbox& = delete;

        template <typename U>
        auto try_push(U&& value) -> push_result
        {
            cell_t* cell = nullptr;
            auto pos = _enqueue_pos.load(memory_order::relaxed);
            for (;;)
            {
                cell = &_cells[pos & Mask];
                const auto seq = cell->sequence.load(memory_order::acquire);
                const auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
                if (diff == 0)
                {
                    if (_enqueue_pos.compare_exchange_weak(pos, pos + 1, memory_order::relaxed))
                    {
                        break;
                    }
                }
                else if (diff < 0)
                {
                    return push_result::full;
                }
                else
                {
                    pos = _enqueue_pos.load(memory_order::relaxed);
                }
            }

            [[maybe_unused]] auto* const constructed_ptr =
                tempest::construct_at(reinterpret_cast<T*>(cell->storage.data()), tempest::forward<U>(value));
            cell->sequence.store(pos + 1, memory_order::release);
            return push_result::success;
        }

        template <typename Callback>
        auto drain(Callback&& callback) -> size_t
        {
            auto drained_count = size_t{0};
            for (;;)
            {
                auto* const cell = &_cells[_dequeue_pos & Mask];
                const auto seq = cell->sequence.load(memory_order::acquire);
                const auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(_dequeue_pos + 1);
                if (diff == 0)
                {
                    auto* const item_ptr = reinterpret_cast<T*>(cell->storage.data());
                    auto item = tempest::move(*item_ptr);
                    tempest::destroy_at(item_ptr);
                    cell->sequence.store(_dequeue_pos + Capacity, memory_order::release);
                    ++_dequeue_pos;
                    callback(tempest::move(item));
                    ++drained_count;
                }
                else if (diff < 0)
                {
                    break;
                }
            }
            return drained_count;
        }

        /// @brief Queries whether the mailbox is currently empty.
        /// @note Thread-confined to the single owning consumer; must not be called concurrently with drain().
        [[nodiscard]] auto empty() const noexcept -> bool
        {
            const auto* const cell = &_cells[_dequeue_pos & Mask];
            const auto seq = cell->sequence.load(memory_order::acquire);
            const auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(_dequeue_pos + 1);
            return diff < 0;
        }

        [[nodiscard]] constexpr auto capacity() const noexcept -> size_t
        {
            return Capacity;
        }

      private:
        alignas(hardware_destructive_interference_size) atomic<size_t> _enqueue_pos{0};
        alignas(hardware_destructive_interference_size) size_t _dequeue_pos{0};
        alignas(hardware_destructive_interference_size) array<cell_t, Capacity> _cells{};
    };
} // namespace tempest::job

#endif // tempest_job_mpsc_mailbox_hpp
